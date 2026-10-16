#!/usr/bin/env python3
"""Exercise production automatic connection management with a controllable host."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from test_midi_reconnect import fixture, root, function

source = fixture + r'''
#include <set>
bool link_pending = false;
bool zt_ableton_link_play_pending() { return link_pending; }
ZTDetectedMidiPort detected_port(const std::string &name, unsigned index, int generation = 1) {
    return {name, name + "#" + std::to_string(generation), index};
}
ZTMidiPortSnapshot next_snapshot;
bool snapshot_pending = false, worker_idle = true, worker_started = false;
unsigned scan_requests = 0;
struct ZTMidiMonitor::Impl {};
ZTMidiMonitor::ZTMidiMonitor() : impl(new Impl) {}
ZTMidiMonitor::~ZTMidiMonitor() { stop(); }
void ZTMidiMonitor::start() { worker_started = true; }
void ZTMidiMonitor::stop() { worker_started = false; }
void ZTMidiMonitor::set_idle(bool idle) {
    if (idle && !worker_idle) request_scan();
    worker_idle = idle;
}
void ZTMidiMonitor::request_scan() { ++scan_requests; snapshot_pending = false; }
bool ZTMidiMonitor::take_snapshot(ZTMidiPortSnapshot &snapshot) {
    if (!snapshot_pending) return false;
    snapshot = next_snapshot; snapshot_pending = false; return true;
}
void publish(std::vector<ZTDetectedMidiPort> outputs, std::vector<ZTDetectedMidiPort> inputs) {
    next_snapshot = {true, true, std::move(outputs), std::move(inputs)};
    snapshot_pending = true;
    ticks += 250;
}
'''
source += '\n'.join(line for line in (root / 'src/midi_hotplug.cpp').read_text().splitlines()
                    if not line.startswith('#include'))
# Model Windows headers defining min/max before the application code.
source = source.replace('namespace {', r'''
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
namespace {
''', 1)

source += r'''
int main() {
    config_data = {{"open_out_device_0", "Synth A"}, {"known_out_device_0", "Synth A"},
                   {"alias_Synth_A", "Lead"}, {"latency_Synth_A", "17"}, {"bank_Synth_A", "Yes"},
                   {"open_in_device_0", "Keyboard"}, {"known_in_device_0", "Keyboard"}};
    host_out = {{"Synth B"}};
    MidiOut = new midiOut;
    MidiIn = new midiIn;
    for (int i = 0; i < 32; ++i) std::strcpy(song->instruments[i]->midi_route.name, "Synth A");
    song->instruments[32]->midi_legacy_device = 14; // unknown old slots are never guessed
    zt_midi_hotplug_start();
    assert(worker_started);
    const auto b = detected_port("Synth B", 0);
    publish({b}, {});
    zt_midi_hotplug_pump();
    assert(!MidiOut->QueryDevice(0)); // discovery alone must not enable unknown ports
    assert(song->instruments[0]->midi_device == 255);
    assert(song->instruments[32]->midi_device == 255);
    assert(player.stops == 0 && player.queued == 5);

    // Unused ports can change without overwriting the user's status message.
    statusmsg = "Keep this status";
    publish({}, {}); zt_midi_hotplug_pump();
    assert(std::string(statusmsg) == "Keep this status");
    publish({b}, {}); zt_midi_hotplug_pump();
    assert(std::string(statusmsg) == "Keep this status");

    // Returning devices restore assignments and settings without F12/song reload.
    host_out = {{"Synth A"}, {"Synth B"}};
    host_in = {{"Keyboard"}};
    auto a = detected_port("Synth A", 0);
    auto moved_b = detected_port("Synth B", 1);
    auto keyboard = detected_port("Keyboard", 0);
    publish({a, moved_b}, {keyboard});
    zt_midi_hotplug_pump();
    assert(MidiOut->numOuputDevices == 2 && MidiOut->QueryDevice(1));
    for (int i = 0; i < 32; ++i) assert(song->instruments[i]->midi_device == 1);
    assert(MidiIn->QueryDevice(0));
    assert(std::string(MidiOut->get_alias(1)) == "Lead");
    assert(MidiOut->get_delay_ticks(1) == 17 && MidiOut->get_bank_select(1) == 1);
    auto *synth = static_cast<MidiOutputDevice *>(MidiOut->outputDevices[1]);
    auto *healthy = MidiOut->outputDevices[0];
    assert(synth->open_calls == 1 && synth->devNum == 0);
    assert(MidiIn->midiInDev[0]->open_calls == 1);
    assert(player.stops == 0 && player.queued == 5);

    // Unchanged scans preserve healthy handles, settings and active editor input.
    assert(MidiOut->AddDevice(0) == 0);
    editor.reset = 0;
    const auto redraws = selector.updates;
    publish({a, moved_b}, {keyboard});
    zt_midi_hotplug_pump();
    ticks += 1000;
    zt_midi_hotplug_pump();
    assert(healthy->open_calls == 1 && healthy->close_calls == 0);
    assert(synth->open_calls == 1 && synth->close_calls == 0);
    assert(MidiIn->midiInDev[0]->open_calls == 1);
    assert(editor.reset == 0 && selector.updates == redraws);

    // Device changes remain pending during playback and a quantized Link launch.
    player.playing = 1;
    publish({b}, {});
    zt_midi_hotplug_pump();
    assert(!worker_idle && MidiOut->QueryDevice(1) && MidiIn->QueryDevice(0));
    player.playing = 0; link_pending = true;
    zt_midi_hotplug_pump();
    assert(MidiOut->QueryDevice(1));
    link_pending = false;
    zt_midi_hotplug_pump(); // requests fresh discovery, discards playing-era result
    assert(scan_requests > 0 && MidiOut->QueryDevice(1));
    host_out = {{"Synth B"}}; host_in.clear();
    publish({b}, {});
    zt_midi_hotplug_pump();
    assert(!MidiOut->QueryDevice(1) && !synth->available);
    assert(!MidiIn->QueryDevice(0) && !MidiIn->midiInDev[0]->available);
    assert(song->instruments[0]->midi_device == 255);
    assert(healthy->open_calls == 1 && healthy->close_calls == 0);

    // Saved requests survive an exit while devices are missing.
    conf config("/app/devices.conf");
    zt_save_midi_connections(config);
    bool output_saved = false, input_saved = false;
    for (const auto &entry : config_data) {
        if (entry.first.find("open_out_device_") == 0 && entry.second == "Synth A") output_saved = true;
        if (entry.first.find("open_in_device_") == 0 && entry.second == "Keyboard") input_saved = true;
    }
    assert(output_saved && input_saved);
    assert(config_data["alias_Synth_A"] == "Lead");

    // Failed opens back off, without rebuilding or touching healthy devices.
    host_out = {{"Synth A", true}, {"Synth B"}};
    host_in = {{"Keyboard", true}};
    a.id = "Synth A#2"; keyboard.id = "Keyboard#2";
    publish({a, moved_b}, {keyboard});
    zt_midi_hotplug_pump();
    const auto attempts = synth->open_calls;
    const auto input_attempts = MidiIn->midiInDev[0]->open_calls;
    assert(!MidiOut->QueryDevice(1) && !MidiIn->QueryDevice(0));
    ticks += 1000;
    zt_midi_hotplug_pump();
    assert(synth->open_calls == attempts && MidiIn->midiInDev[0]->open_calls == input_attempts);
    host_out[0].fail = false; host_in[0].fail = false;
    ticks += 1500;
    zt_midi_hotplug_pump();
    assert(MidiOut->QueryDevice(1) && MidiIn->QueryDevice(0));
    assert(healthy->open_calls == 1 && healthy->close_calls == 0);

    // An empty/failed discovery result must never mean every port disappeared.
    next_snapshot = {}; snapshot_pending = true; ticks += 250;
    zt_midi_hotplug_pump();
    assert(MidiOut->QueryDevice(1) && MidiIn->QueryDevice(0));

    // Closing a song's output deliberately suppresses automatic reopening.
    MidiOut->RemDevice(1);
    auto &out_state = zt_midi_connections(true)["Synth A"];
    out_state.reopen = false; out_state.explicitly_closed = true;
    MidiIn->RemDevice(0);
    auto &in_state = zt_midi_connections(false)["Keyboard"];
    in_state.reopen = false; in_state.explicitly_closed = true;
    publish({b}, {}); zt_midi_hotplug_pump();
    a.id = "Synth A#3"; keyboard.id = "Keyboard#3";
    publish({a, moved_b}, {keyboard}); zt_midi_hotplug_pump();
    ticks += 60000; zt_midi_hotplug_pump();
    assert(!MidiOut->QueryDevice(1) && !MidiIn->QueryDevice(0));
    assert(song->instruments[0]->midi_device == 1); // assignment retained, closed
    zt_save_midi_connections(config);
    for (const auto &entry : config_data)
        if (entry.first.find("open_") == 0) assert(entry.second != "Synth A" && entry.second != "Keyboard");

    // Replacement endpoint, same name/count: only its handle is reopened.
    out_state.explicitly_closed = false; out_state.reopen = true;
    in_state.explicitly_closed = false; in_state.reopen = true;
    publish({a, moved_b}, {keyboard}); zt_midi_hotplug_pump();
    const auto previous_closes = synth->close_calls;
    a.id = "Synth A#4";
    publish({a, moved_b}, {keyboard}); zt_midi_hotplug_pump();
    assert(synth->close_calls == previous_closes + 1 && MidiOut->QueryDevice(1));
    assert(healthy->close_calls == 0);

    // Native identity follows a rename and keeps the live handle intact.
    host_out[0].name = "Synth Renamed";
    a.name = "Synth Renamed";
    const auto before_rename = synth->close_calls;
    publish({a, moved_b}, {keyboard}); zt_midi_hotplug_pump();
    assert(std::string(synth->szName) == "Synth Renamed" && synth->close_calls == before_rename);
    assert(std::string(song->instruments[0]->midi_route.name) == "Synth Renamed");
    assert(song->instruments[0]->midi_device == 1);

    // Busy audio callback: skip maintenance rather than blocking the UI.
    zt_midi_output_mutex.lock();
    publish({b}, {}); zt_midi_hotplug_pump();
    assert(MidiOut->QueryDevice(1) && lock_depth == 0);
    zt_midi_output_mutex.unlock();

    // Busy producer: no mutations; the pending snapshot is applied next tick.
    allow_lock = false;
    publish({b}, {}); zt_midi_hotplug_pump();
    assert(MidiOut->QueryDevice(1));
    allow_lock = true; ticks += 250; zt_midi_hotplug_pump();
    assert(!MidiOut->QueryDevice(1));
    assert(lock_depth == 0 && player.stops == 0 && player.queued == 5);
    out_state.forgotten = true;
    zt_save_midi_connections(config);
    for (const auto &entry : config_data)
        if (entry.first.find("known_out_device_") == 0) assert(entry.second != "Synth A");

    // Internal audio plugins are always present: closing one must not become
    // a saved reconnect request, unlike an absent external MIDI connection.
    const auto audio_slot = MidiOut->numOuputDevices++;
    MidiOut->outputDevices[audio_slot] = new OutputDevice({"Noise"});
    MidiOut->outputDevices[audio_slot]->type = 1;
    assert(MidiOut->AddDevice(audio_slot) == 0);
    remember_live_settings();
    assert(zt_midi_connections(true)["Noise"].reopen);
    MidiOut->RemDevice(audio_slot);
    remember_live_settings();
    assert(!zt_midi_connections(true)["Noise"].reopen);

    ZTMidiRetry retry;
    for (int i = 0; i < 12; ++i) {
        const auto now = retry.next;
        retry.failed(now);
        assert(retry.next > now && retry.next - now <= 30000);
    }
    retry.reset(); assert(retry.ready(0));
    zt_midi_hotplug_stop(); assert(!worker_started);
    delete MidiIn; delete MidiOut;
}
'''
with tempfile.TemporaryDirectory(prefix='zt-midi-hotplug-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                   ['-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(root / 'src'), str(cpp), '-o', str(exe)], check=True, cwd=root)
    subprocess.run([str(exe)], check=True)
print('Automatic MIDI: idle deferral, stable handles/slots, retries, settings, explicit closes, rename and offline persistence passed.')
