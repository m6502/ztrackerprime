#!/usr/bin/env python3
"""Run production MIDI refresh/open paths against changing fake host ports."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from drawing_harness import function

source = r'''
#include "midi_route.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>
constexpr int MAX_MIDI_DEVS = 64, MAX_MIDI_INS = 64, MAX_MIDI_OUTS = 64;
constexpr int ZTM_MAX_INSTS = 128, OUTPUTDEVICE_TYPE_MIDI = 2;
constexpr int MMSYSERR_BADDEVICEID = 2, MMSYSERR_NOERROR = 0;
using UINT = unsigned int;
using HMIDIIN = void *;
struct UserInterfaceElement {};
struct List { int need_redraw = 0; void OnChange() {} } outlist, inlist;
List *midioutdevlist = &outlist, *midiindevlist = &inlist;
struct Editor { int reset = 0; } editor;
Editor *UIP_InstEditor = &editor;
const char *cur_dir = "/app", *statusmsg = "";
char szStatmsg[1024];
int status_change = 0, need_refresh = 0, lock_depth = 0;
int lock_mutex(int) { ++lock_depth; return 1; }
void unlock_mutex(int) { --lock_depth; }
void jazz_clear_all_states() {}
struct { bool auto_open_midi = true; } zt_config_globals;
struct instrument {
    unsigned char midi_device = 255, midi_legacy_device = 255;
    ZTMidiRoute midi_route;
};
struct Song {
    int hEditMutex = 0;
    instrument data[ZTM_MAX_INSTS];
    instrument *instruments[ZTM_MAX_INSTS];
    Song() { for (int i = 0; i < ZTM_MAX_INSTS; ++i) instruments[i] = &data[i]; }
} storage;
Song *song = &storage;
struct Player { int stops = 0, queued = 5; void stop() { ++stops; queued = 0; } } player;
Player *ztPlayer = &player;
struct Queue { int clears = 0; void clear() { ++clears; } } midiInQueue;
std::map<std::string, std::string> config_data;
struct conf {
    conf(const char *path) { assert(std::string(path) == "/app/devices.conf"); }
    char *get(const char *key) {
        auto i = config_data.find(key);
        return i == config_data.end() ? nullptr : i->second.data();
    }
};
struct Port { std::string name; bool fail = false; };
std::vector<Port> host_out, host_in;
struct intlist {
    int key; intlist *next;
    intlist(int k, intlist *n) : key(k), next(n) {}
};
struct OutputDevice {
    char szName[256]; char *alias = nullptr;
    int opened = 0, delay_ticks = 0, type = OUTPUTDEVICE_TYPE_MIDI;
    bool fail;
    OutputDevice(const Port &p) : fail(p.fail) { std::snprintf(szName, sizeof(szName), "%s", p.name.c_str()); }
    virtual ~OutputDevice() { free(alias); }
    int open() { if (fail) return 7; opened = 1; return 0; }
    int close() { opened = 0; return 0; }
};
struct MidiOutputDevice : OutputDevice {
    int reverse_bank_select = 0;
    using OutputDevice::OutputDevice;
};
struct midiOut {
    OutputDevice *outputDevices[MAX_MIDI_OUTS] = {};
    unsigned int numOuputDevices;
    intlist *devlist_head;
    midiOut(); ~midiOut();
    UINT AddDevice(int); int RemDevice(int); int QueryDevice(int);
    int get_delay_ticks(int); void set_delay_ticks(int, int);
    int get_bank_select(int); void set_bank_select(int, int);
    const char *get_alias(int); void set_alias(int, char *);
};
midiOut *MidiOut = nullptr;
void InitializePlugins(midiOut *out) {
    for (const auto &p : host_out) out->outputDevices[out->numOuputDevices++] = new MidiOutputDevice(p);
}
struct midiInDevice {
    std::string name; const char *szName; HMIDIIN handle = this;
    int opened = 0; bool fail;
    midiInDevice(int i) : name(host_in[i].name), szName(name.c_str()), fail(host_in[i].fail) {}
    int open() { if (fail) return 7; opened = 1; return 0; }
    int close();
};
unsigned midiInGetNumDevs() { return host_in.size(); }
struct midiIn {
    midiInDevice *midiInDev[MAX_MIDI_INS] = {};
    unsigned numMidiDevs;
    intlist *devlist_head;
    midiIn(); ~midiIn();
    UINT AddDevice(int); int RemDevice(int); int QueryDevice(int);
    midiInDevice *GetDevice(HMIDIIN);
};
midiIn *MidiIn = nullptr;
int midiInDevice::close() {
    // Simulate a final callback during close; the active linked list must
    // remain valid while every subsequent input is being drained.
    if (opened) assert(MidiIn->GetDevice(handle) == this);
    opened = 0; return 0;
}
'''
source += function('src/midi-io.h', 'struct ZTMidiConnectionState') + ';\n'
source += 'std::mutex zt_midi_output_mutex;\n'
for signature in [
    'std::map<std::string, ZTMidiConnectionState> &zt_midi_connections(',
    'std::vector<ZTMidiDestination> zt_midi_destinations(',
    'void zt_capture_midi_instruments(', 'void zt_reconnect_midi_instruments(',
    'midiOut::midiOut(', 'midiOut::~midiOut(',
    'int midiOut::get_delay_ticks(', 'void midiOut::set_delay_ticks(',
    'int midiOut::get_bank_select(', 'void midiOut::set_bank_select(',
    'const char *midiOut::get_alias(', 'void midiOut::set_alias(',
    'UINT midiOut::AddDevice(', 'int midiOut::RemDevice(', 'int midiOut::QueryDevice(',
    'midiIn::midiIn(', 'midiIn::~midiIn(', 'UINT midiIn::AddDevice(',
    'int midiIn::RemDevice(', 'int midiIn::QueryDevice(', 'midiInDevice *midiIn::GetDevice(']:
    source += function('src/midi-io.cpp', signature) + '\n'
for signature in [
    'static void encode_dev_key(',
    'static ZTMidiConnectionState read_midi_connection_settings(',
    'static void seed_midi_connections(', 'void BTNCLK_RefreshMidiOutDeviceList(',
    'void BTNCLK_RefreshMidiInDeviceList(']:
    source += function('src/CUI_Sysconfig.cpp', signature) + '\n'
source += r'''
const char *zt_decode_midi_error(int) { return "device busy"; }
void set_error_status(const char *message) {
    std::snprintf(szStatmsg, sizeof(szStatmsg), "%s", message);
    statusmsg = szStatmsg;
}
'''
source += function('src/UserInterface.cpp', 'void midi_out_sel(')
source += function('src/UserInterface.cpp', 'void midi_in_sel(')
# Exercise the real driver-open bodies separately, with deterministic host
# capabilities and input-start failure. The refresh fixture above replaces
# device I/O, while these probes verify stale indexes never reach the driver.
source += r'''
namespace OpenProbe {
constexpr int CALLBACK_NULL = 0, CALLBACK_FUNCTION = 1;
using DWORD_PTR = unsigned long;
using LPSTR = char *;
struct MIDIOUTCAPS { char szPname[256]; };
using MIDIINCAPS = MIDIOUTCAPS;
struct MIDIHDR { char *lpData; unsigned dwBufferLength, dwFlags; };
std::string current_name = "Synth A";
int driver_calls = 0, start_error = 0;
int midiOutGetDevCaps(int, MIDIOUTCAPS *caps, unsigned) {
    std::snprintf(caps->szPname, sizeof(caps->szPname), "%s", current_name.c_str());
    return 0;
}
int midiInGetDevCaps(int i, MIDIINCAPS *caps, unsigned size) { return midiOutGetDevCaps(i, caps, size); }
void midiInCallback() {}
void zt_sanitize_device_name(char *) {}
int midiOutOpen(void **h, int, int, int, int) { ++driver_calls; *h = (void *)1; return 0; }
int midiInOpen(void **h, int, DWORD_PTR, DWORD_PTR, int) { return midiOutOpen(h, 0, 0, 0, 0); }
int midiInPrepareHeader(void *, MIDIHDR *, unsigned) { return 0; }
int midiInAddBuffer(void *, MIDIHDR *, unsigned) { return 0; }
int midiInStart(void *) { return start_error; }
struct MidiOutputDevice {
    int devNum = 0, opened = 0, closes = 0;
    char szName[256] = "Synth A";
    void *handle = nullptr;
    int open(); int close() { opened = 0; handle = nullptr; ++closes; return 0; }
};
struct midiInDevice {
    int devNum = 0, opened = 0, closes = 0;
    const char *szName = "Synth A";
    void *handle = nullptr;
    MIDIHDR midiHdr{}; char SysXBuffer[8192];
    int open(); int close() { opened = 0; handle = nullptr; ++closes; return 0; }
};
'''
source += function('src/OutputDevices.cpp', 'int MidiOutputDevice::open(')
source += function('src/midi-io.cpp', 'int midiInDevice::open(')
source += r'''
void run() {
    MidiOutputDevice output;
    midiInDevice input;
    current_name = "Synth B"; // another device now occupies the old index
    assert(output.open() == MMSYSERR_BADDEVICEID);
    assert(input.open() == MMSYSERR_BADDEVICEID);
    assert(driver_calls == 0 && !output.opened && !input.opened);
    current_name = "Synth A";
    assert(output.open() == 0 && output.opened);
    start_error = 9;
    assert(input.open() == 9 && !input.opened && input.handle == nullptr);
    assert(input.closes == 1); // failed starts release the acquired handle
    start_error = 0;
    assert(input.open() == 0 && input.opened);
}
} // namespace OpenProbe
'''
source += r'''
void refresh_out() {
    BTNCLK_RefreshMidiOutDeviceList(nullptr);
    assert(lock_depth == 0 && player.queued == 0);
}
int main() {
    OpenProbe::run();
    // Missing at startup. Restore by identity after the host gains the port.
    config_data = {{"open_out_device_0", "Synth A"}, {"alias_Synth_A", "Lead"},
                   {"latency_Synth_A", "12"}, {"bank_Synth_A", "Yes"},
                   {"open_in_device_0", "Keyboard"}};
    MidiOut = new midiOut;
    MidiIn = new midiIn;
    std::strcpy(song->instruments[0]->midi_route.name, "Synth A");
    for (int i = 1; i < 32; ++i) *song->instruments[i] = *song->instruments[0];
    host_out = {{"Synth B"}, {"Synth A"}};
    refresh_out();
    assert(MidiOut->QueryDevice(1) && !MidiOut->QueryDevice(0));
    for (int i = 0; i < 32; ++i) assert(song->instruments[i]->midi_device == 1);
    assert(song->instruments[32]->midi_device == 255); // explicitly disabled
    assert(std::string(MidiOut->get_alias(1)) == "Lead");
    assert(MidiOut->get_delay_ticks(1) == 12 && MidiOut->get_bank_select(1) == 1);
    assert(MidiOut->AddDevice(1) == 0 && MidiOut->devlist_head->next == nullptr);
    assert(MidiOut->AddDevice(-1) == MMSYSERR_BADDEVICEID);
    assert(MidiOut->AddDevice(64) == MMSYSERR_BADDEVICEID);

    // Unnamed live assignments must be captured before numeric slots shift.
    song->instruments[32]->midi_device = 1;
    MidiOut->set_alias(1, (char *)"Edited Lead");
    MidiOut->set_delay_ticks(1, 23); MidiOut->set_bank_select(1, 0);
    host_out = {{"Synth A"}, {"Synth B"}};
    refresh_out();
    assert(song->instruments[32]->midi_device == 0);
    assert(std::string(song->instruments[32]->midi_route.name) == "Synth A");
    assert(std::string(MidiOut->get_alias(0)) == "Edited Lead");
    assert(MidiOut->get_delay_ticks(0) == 23 && MidiOut->get_bank_select(0) == 0);

    // Disconnect, refresh twice, reconnect. Never route to a replacement slot.
    host_out = {{"Synth B"}};
    refresh_out(); refresh_out();
    for (int i = 0; i <= 32; ++i) assert(song->instruments[i]->midi_device == 255);
    assert(!MidiOut->QueryDevice(0));
    host_out = {{"Synth A", true}, {"Synth B"}};
    refresh_out();
    assert(song->instruments[0]->midi_device == 0 && !MidiOut->QueryDevice(0));
    assert(std::string(statusmsg).find("1 failed") != std::string::npos);
    host_out[0].fail = false;
    refresh_out();
    assert(MidiOut->QueryDevice(0));
    assert(std::string(MidiOut->get_alias(0)) == "Edited Lead");
    assert(MidiOut->get_delay_ticks(0) == 23 && MidiOut->get_bank_select(0) == 0);

    // Alias matching needs restored settings before route reconciliation.
    std::strcpy(song->instruments[33]->midi_route.name, "Old host name");
    std::strcpy(song->instruments[33]->midi_route.alias, "Edited Lead");
    refresh_out();
    assert(song->instruments[33]->midi_device == 0);
    // Manual open path can restore a missing assignment without a song reload.
    song->instruments[0]->midi_device = 255;
    zt_reconnect_midi_instruments();
    assert(song->instruments[0]->midi_device == 0);

    // Actual F12 open handling reconnects instruments and preserves errors.
    midi_out_sel(0); // explicit close
    song->instruments[0]->midi_device = 255;
    midi_out_sel(0); // explicit reopen
    assert(song->instruments[0]->midi_device == 0 && MidiOut->QueryDevice(0));
    midi_out_sel(0);
    MidiOut->outputDevices[0]->fail = true;
    MidiOut->set_alias(0, (char *)"");
    midi_out_sel(0);
    assert(std::string(statusmsg).find("Could not open Synth A") != std::string::npos);
    assert(!MidiOut->QueryDevice(0));
    assert(lock_depth == 0);

    // Duplicate system identities remain unresolved; no arbitrary output opens.
    host_out = {{"Synth A"}, {"Synth A"}};
    refresh_out();
    assert(song->instruments[0]->midi_device == 255);
    assert(!MidiOut->QueryDevice(0) && !MidiOut->QueryDevice(1));

    // Inputs remember requested opens across absence and failures too.
    host_in = {{"Other keyboard"}, {"Keyboard"}};
    BTNCLK_RefreshMidiInDeviceList(nullptr);
    assert(MidiIn->QueryDevice(1) && !MidiIn->QueryDevice(0));
    assert(MidiIn->AddDevice(1) == 0 && MidiIn->devlist_head->next == nullptr);
    assert(MidiIn->AddDevice(-1) == MMSYSERR_BADDEVICEID);
    host_in.clear();
    BTNCLK_RefreshMidiInDeviceList(nullptr);
    BTNCLK_RefreshMidiInDeviceList(nullptr);
    host_in = {{"Keyboard", true}};
    BTNCLK_RefreshMidiInDeviceList(nullptr);
    assert(!MidiIn->QueryDevice(0));
    assert(std::string(statusmsg).find("1 failed") != std::string::npos);
    host_in = {{"Keyboard"}, {"Other keyboard"}};
    BTNCLK_RefreshMidiInDeviceList(nullptr);
    assert(MidiIn->QueryDevice(0));
    assert(MidiIn->AddDevice(1) == 0); // both open exercises callback-safe cleanup
    host_in.resize(70, {"Extra input"});
    BTNCLK_RefreshMidiInDeviceList(nullptr);
    assert(MidiIn->numMidiDevs == MAX_MIDI_INS);
    delete MidiIn; delete MidiOut;
}
'''
with tempfile.TemporaryDirectory(prefix='zt-midi-reconnect-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                   ['-std=c++17', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(root / 'src'), str(cpp), '-o', str(exe)], check=True, cwd=root)
    subprocess.run([str(exe)], check=True)
print('MIDI reconnect: startup absence, reorder, repeated unplug, aliases/settings, failed opens, ambiguity and input lifecycle passed.')
