#!/usr/bin/env python3
"""Run the native monitor worker against deterministic CoreMIDI/WinMM APIs.

The production implementation is unchanged except for redirecting its platform
header and platform-selection macros. CoreMIDI notifications are delivered only
when the creating thread pumps its run loop, matching Apple's API contract.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
stub = r'''
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cassert>
using MIDIClientRef = unsigned;
using MIDIObjectRef = unsigned;
using MIDIEndpointRef = unsigned;
using ItemCount = unsigned;
using SInt32 = int;
constexpr int noErr = 0;
constexpr int kMIDIMsgObjectAdded = 1, kMIDIMsgObjectRemoved = 2, kMIDIMsgSetupChanged = 3;
constexpr int kMIDIMsgPropertyChanged = 4, kMIDIMsgIOError = 5;
constexpr int kMIDIObjectType_Source = 1, kMIDIObjectType_Destination = 2;
constexpr int kMIDIPropertyOffline = 1, kMIDIPropertyUniqueID = 2, kCFRunLoopDefaultMode = 0;
#define CFSTR(s) s
struct MIDINotification { int messageID; };
struct MIDIObjectAddRemoveNotification : MIDINotification { int childType; MIDIObjectRef child; };
using Notify = void (*)(const MIDINotification *, void *);
struct FakePort { std::string name; unsigned endpoint = 100; bool offline = false; };
std::mutex api_mutex;
std::condition_variable api_wake;
std::vector<FakePort> fake_ports;
std::deque<MIDIObjectAddRemoveNotification> notifications;
Notify callback = nullptr;
void *callback_context = nullptr;
std::thread::id client_thread;
std::atomic<bool> fail_input{false};
int MIDIClientCreate(const char *, Notify cb, void *context, MIDIClientRef *client) {
    std::lock_guard<std::mutex> guard(api_mutex);
    callback = cb; callback_context = context; client_thread = std::this_thread::get_id();
    *client = 1; return noErr;
}
void MIDIClientDispose(MIDIClientRef) {
    std::lock_guard<std::mutex> guard(api_mutex);
    assert(std::this_thread::get_id() == client_thread);
    callback = nullptr; callback_context = nullptr; notifications.clear();
}
void CFRunLoopRunInMode(int, double seconds, bool return_after_source) {
    assert(std::this_thread::get_id() == client_thread && return_after_source);
    std::unique_lock<std::mutex> guard(api_mutex);
    api_wake.wait_for(guard, std::chrono::duration<double>(seconds), [] { return !notifications.empty(); });
    if (notifications.empty()) return;
    auto event = notifications.front(); notifications.pop_front();
    auto cb = callback; auto context = callback_context;
    guard.unlock();
    if (cb) cb(&event, context);
}
ItemCount MIDIGetNumberOfDestinations() {
    std::lock_guard<std::mutex> guard(api_mutex); return fake_ports.size();
}
ItemCount MIDIGetNumberOfSources() { return MIDIGetNumberOfDestinations(); }
MIDIEndpointRef MIDIGetDestination(ItemCount index) {
    std::lock_guard<std::mutex> guard(api_mutex);
    return index < fake_ports.size() ? fake_ports[index].endpoint : 0;
}
MIDIEndpointRef MIDIGetSource(ItemCount index) { return fail_input ? 0 : MIDIGetDestination(index); }
int MIDIObjectGetIntegerProperty(MIDIEndpointRef endpoint, int property, SInt32 *value) {
    std::lock_guard<std::mutex> guard(api_mutex);
    for (const auto &p : fake_ports) if (p.endpoint == endpoint) {
        *value = property == kMIDIPropertyOffline ? p.offline : 1234; return 0;
    }
    return -1;
}
void zt_coremidi_endpoint_name(MIDIEndpointRef endpoint, char *name, size_t size) {
    std::lock_guard<std::mutex> guard(api_mutex);
    for (const auto &p : fake_ports) if (p.endpoint == endpoint) std::snprintf(name, size, "%s", p.name.c_str());
}
using UINT = unsigned;
struct MIDIOUTCAPS { char szPname[256]; };
using MIDIINCAPS = MIDIOUTCAPS;
unsigned midiOutGetNumDevs() { return MIDIGetNumberOfDestinations(); }
unsigned midiInGetNumDevs() { return MIDIGetNumberOfSources(); }
int midiOutGetDevCaps(unsigned index, MIDIOUTCAPS *caps, size_t) {
    std::lock_guard<std::mutex> guard(api_mutex);
    if (index >= fake_ports.size()) return 1;
    std::snprintf(caps->szPname, sizeof(caps->szPname), "%s", fake_ports[index].name.c_str()); return 0;
}
int midiInGetDevCaps(unsigned index, MIDIINCAPS *caps, size_t size) {
    return fail_input ? 1 : midiOutGetDevCaps(index, caps, size);
}
void replace_ports(std::vector<FakePort> ports, int type = kMIDIMsgObjectAdded) {
    std::lock_guard<std::mutex> guard(api_mutex);
    const unsigned endpoint = ports.empty() ? 100 : ports[0].endpoint;
    fake_ports = std::move(ports);
    notifications.push_back({{type}, kMIDIObjectType_Destination, endpoint});
    api_wake.notify_all();
}
'''
main = r'''
using namespace std::chrono_literals;
bool wait_for(ZTMidiMonitor &monitor, ZTMidiPortSnapshot &snapshot,
              const std::function<bool(const ZTMidiPortSnapshot &)> &ready) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (monitor.take_snapshot(snapshot) && ready(snapshot)) return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}
int main() {
    ZTMidiMonitor monitor;
    ZTMidiPortSnapshot snapshot;
    monitor.start();
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs_valid && s.inputs_valid; }));
    // Native notifications (macOS) or the real 3-second interval (Windows).
    replace_ports({{"Synth A", 100}});
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs.size() == 1 && s.inputs.size() == 1; }));
    const auto original = snapshot.outputs[0].id;
    replace_ports({{"Synth B", 200}});
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs.size() == 1 && s.outputs[0].name == "Synth B"; }));
    assert(snapshot.outputs[0].id != original); // same count, different device
#if defined(ZT_TEST_MAC)
    replace_ports({{"Synth B", 200, true}}, kMIDIMsgPropertyChanged);
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs_valid && s.outputs.empty(); }));
    replace_ports({{"Synth B", 200, false}}, kMIDIMsgPropertyChanged);
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs.size() == 1; }));
#endif
    fail_input = true;
    monitor.request_scan();
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs_valid && !s.inputs_valid; }));
    fail_input = false;
    monitor.set_idle(false);
    monitor.take_snapshot(snapshot);
    replace_ports({});
    std::this_thread::sleep_for(500ms);
    assert(!monitor.take_snapshot(snapshot));
    monitor.set_idle(true);
    assert(wait_for(monitor, snapshot, [](const auto &s) { return s.outputs_valid && s.outputs.empty(); }));
    monitor.stop(); monitor.stop();
}
'''
implementation = (root / 'src/midi_monitor.cpp').read_text()
implementation = implementation.replace('#include "winmm_compat.h"', '#include "native_api.h"')
implementation = implementation.replace('defined(__linux__)', 'defined(ZT_TEST_LINUX)')
implementation = implementation.replace('defined(__APPLE__)', 'defined(ZT_TEST_MAC)')
with tempfile.TemporaryDirectory(prefix='zt-midi-platforms-') as tmp:
    tmp = Path(tmp)
    (tmp / 'native_api.h').write_text(stub)
    (tmp / 'test.cpp').write_text('#include <functional>\n' + implementation + main)
    for platform in ['MAC', 'WINDOWS']:
        exe = tmp / platform
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                       ['-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pthread',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', f'-DZT_TEST_{platform}',
                        '-I', str(root / 'src'), str(tmp / 'test.cpp'), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=20)
print('Native monitor contracts: CoreMIDI run-loop notifications/offline endpoints and WinMM polling passed.')
