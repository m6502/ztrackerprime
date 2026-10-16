#ifndef ZT_MIDI_MONITOR_H
#define ZT_MIDI_MONITOR_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Host indexes are temporary. The native ID also distinguishes a replacement
// endpoint from a still-open endpoint with the same display name.
struct ZTDetectedMidiPort {
    std::string name;
    std::string id;
    unsigned int index = 0;
    bool operator==(const ZTDetectedMidiPort &other) const {
        return name == other.name && id == other.id && index == other.index;
    }
};
struct ZTMidiPortSnapshot {
    bool outputs_valid = false;
    bool inputs_valid = false;
    std::vector<ZTDetectedMidiPort> outputs, inputs;
};

// No application objects or live MIDI handles cross this boundary. Discovery
// runs on the worker; the UI applies snapshots only when transport is idle.
class ZTMidiMonitor {
public:
    ZTMidiMonitor();
    ~ZTMidiMonitor();
    void start();
    void stop();
    void set_idle(bool idle);
    void request_scan();
    bool take_snapshot(ZTMidiPortSnapshot &snapshot);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Shared retry policy. Repeated failures eventually retry every 30 seconds;
// a changed endpoint or an explicit open/refresh can reset it immediately.
struct ZTMidiRetry {
    std::uint64_t next = 0;
    unsigned failures = 0;
    bool ready(std::uint64_t now) const { return now >= next; }
    void reset() { next = 0; failures = 0; }
    void failed(std::uint64_t now) {
        if (failures < 5) ++failures;
        const unsigned delay = failures >= 5 ? 30000 : (1000u << failures);
        next = now + delay;
    }
};

void zt_midi_hotplug_start();
void zt_midi_hotplug_stop();
void zt_midi_hotplug_pump();
void zt_midi_hotplug_rescan();

#endif
