// Real ALSA announce notifications with ports owned by a disposable child.
// No notes are sent, and no existing device connections are modified.
#include "midi_monitor.h"
#include <alsa/asoundlib.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <cstdio>
#include <functional>
#include <thread>

using namespace std::chrono_literals;
static bool wait_snapshot(ZTMidiMonitor &monitor, ZTMidiPortSnapshot &snapshot,
                          const std::function<bool(const ZTMidiPortSnapshot &)> &ready) {
    const auto until = std::chrono::steady_clock::now() + 4s;
    while (std::chrono::steady_clock::now() < until) {
        if (monitor.take_snapshot(snapshot) && ready(snapshot)) return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}
static const ZTDetectedMidiPort *find(const std::vector<ZTDetectedMidiPort> &ports, const std::string &name) {
    for (const auto &port : ports) if (port.name == name) return &port;
    return nullptr;
}
int main() {
    int commands[2], replies[2];
    assert(pipe(commands) == 0 && pipe(replies) == 0);
    const std::string client_name = "ZT Hotplug Test " + std::to_string(getpid());
    const std::string name = client_name + ":Virtual MIDI";
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(commands[1]); close(replies[0]);
        snd_seq_t *seq = nullptr;
        const bool available = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) >= 0;
        char reply = available ? 'y' : 'n';
        if (write(replies[1], &reply, 1) != 1) _exit(1);
        if (!available) _exit(77);
        snd_seq_set_client_name(seq, client_name.c_str());
        int port = -1;
        char command;
        while (read(commands[0], &command, 1) == 1) {
            if (command == 'o') {
                port = snd_seq_create_simple_port(seq, "Virtual MIDI",
                    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ |
                    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                    SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
                assert(port >= 0);
            } else if (command == 'x') {
                assert(snd_seq_delete_simple_port(seq, port) == 0);
                port = -1;
            } else if (command == 'r') {
                snd_seq_port_info_t *info;
                snd_seq_port_info_alloca(&info);
                assert(snd_seq_get_port_info(seq, port, info) == 0);
                snd_seq_port_info_set_name(info, "Renamed MIDI");
                assert(snd_seq_set_port_info(seq, port, info) == 0);
            } else if (command == 'q') break;
            reply = 'y';
            if (write(replies[1], &reply, 1) != 1) _exit(1);
        }
        snd_seq_close(seq);
        _exit(0);
    }
    close(commands[0]); close(replies[1]);
    char reply;
    assert(read(replies[0], &reply, 1) == 1);
    if (reply == 'n') {
        waitpid(child, nullptr, 0);
        std::puts("SKIP: ALSA sequencer unavailable");
        return 77;
    }
    auto command = [&](char c) {
        assert(write(commands[1], &c, 1) == 1);
        assert(read(replies[0], &reply, 1) == 1 && reply == 'y');
    };
    ZTMidiMonitor monitor;
    ZTMidiPortSnapshot snapshot;
    monitor.start(); monitor.start();
    assert(wait_snapshot(monitor, snapshot, [](const ZTMidiPortSnapshot &s) { return s.outputs_valid && s.inputs_valid; }));

    // Internal application ports must not appear as external MIDI devices.
    snd_seq_t *own = nullptr;
    assert(snd_seq_open(&own, "default", SND_SEQ_OPEN_DUPLEX, 0) == 0);
    snd_seq_set_client_name(own, "ZT Own Test");
    assert(snd_seq_create_simple_port(own, "Own MIDI",
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ |
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC) >= 0);
    monitor.request_scan();
    assert(wait_snapshot(monitor, snapshot, [](const ZTMidiPortSnapshot &s) { return s.outputs_valid && s.inputs_valid; }));
    assert(!find(snapshot.outputs, "ZT Own Test:Own MIDI") && !find(snapshot.inputs, "ZT Own Test:Own MIDI"));

    // Arrival/removal wakes the monitor; no 30-second fallback scan is needed.
    command('o');
    assert(wait_snapshot(monitor, snapshot, [&](const ZTMidiPortSnapshot &s) { return find(s.outputs, name) && find(s.inputs, name); }));
    const std::string original_id = find(snapshot.outputs, name)->id;
    command('x');
    assert(wait_snapshot(monitor, snapshot, [&](const ZTMidiPortSnapshot &s) { return !find(s.outputs, name) && !find(s.inputs, name); }));
    command('o');
    assert(wait_snapshot(monitor, snapshot, [&](const ZTMidiPortSnapshot &s) { return find(s.outputs, name) && find(s.inputs, name); }));
    assert(find(snapshot.outputs, name)->id != original_id); // same client/port number, new endpoint

    // Port renaming updates the name without declaring the handle replaced.
    const auto before_rename = find(snapshot.outputs, name)->id;
    const auto renamed = client_name + ":Renamed MIDI";
    command('r');
    assert(wait_snapshot(monitor, snapshot, [&](const ZTMidiPortSnapshot &s) { return find(s.outputs, renamed) != nullptr; }));
    assert(find(snapshot.outputs, renamed)->id == before_rename);

    // Playing: record notifications but defer discovery. Resume gets a fresh scan.
    monitor.set_idle(false);
    monitor.take_snapshot(snapshot);
    command('x');
    std::this_thread::sleep_for(600ms);
    assert(!monitor.take_snapshot(snapshot));
    monitor.set_idle(true);
    assert(wait_snapshot(monitor, snapshot, [&](const ZTMidiPortSnapshot &s) { return !find(s.outputs, renamed); }));

    monitor.stop(); monitor.stop();
    monitor.start();
    monitor.request_scan();
    assert(wait_snapshot(monitor, snapshot, [](const ZTMidiPortSnapshot &s) { return s.outputs_valid && s.inputs_valid; }));
    monitor.stop();
    snd_seq_close(own);
    const char quit = 'q';
    assert(write(commands[1], &quit, 1) == 1);
    close(commands[1]); close(replies[0]);
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    std::puts("ALSA monitor: arrival, removal, replacement, rename, own-port exclusion, idle deferral and shutdown passed");
}
