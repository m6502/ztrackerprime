#ifndef ZT_MIDI_ROUTE_H
#define ZT_MIDI_ROUTE_H

#include <cstring>
#include <vector>

// Names survive device enumeration changes; aliases allow a user to give
// the same destination a portable name on different machines.
struct ZTMidiRoute {
    char name[256] = {};
    char alias[1024] = {};
    bool named() const { return name[0] || alias[0]; }
};

struct ZTMidiDestination {
    const char *name;
    const char *alias;
    bool opened;
    bool midi;
};

struct ZTMidiResolution {
    int device;
    bool fallback;
};

inline ZTMidiResolution zt_resolve_midi_route(
    unsigned char legacy, const ZTMidiRoute &route,
    const std::vector<ZTMidiDestination> &devices)
{
    if (route.named()) {
        // Never use a numeric slot or an arbitrary fallback for a named
        // destination. Duplicate names/aliases are ambiguous, too.
        for (int pass = 0; pass < 2; ++pass) {
            const char *wanted = pass == 0 ? route.name : route.alias;
            if (!wanted[0]) continue;
            int match = -1;
            for (size_t i = 0; i < devices.size(); ++i) {
                const char *candidate = pass == 0 ? devices[i].name : devices[i].alias;
                if (candidate && !std::strcmp(wanted, candidate)) {
                    if (match >= 0) { match = -2; break; }
                    match = static_cast<int>(i);
                }
            }
            if (match >= 0) return {match, false};
        }
        return {-1, false};
    }
    // Historical UI used 64 for "off"; constructors used 255.
    if (legacy == 64 || legacy == 255) return {-1, false};
    // Audio plugins are appended after MIDI ports. An old MIDI slot may
    // now land on NoiseMaker/TestTone after hardware was disconnected.
    if (legacy < devices.size() && devices[legacy].midi) return {legacy, false};

    // Old files cannot identify the original hardware. A single already
    // enabled MIDI output is a useful migration fallback, not proof of
    // the original destination. The caller reports this substitution.
    int only = -1;
    for (size_t i = 0; i < devices.size(); ++i) {
        if (!devices[i].opened || !devices[i].midi) continue;
        if (only >= 0) return {-1, false};
        only = static_cast<int>(i);
    }
    return {only, only >= 0};
}

struct ZTMidiRouteEntry {
    unsigned char instrument;
    ZTMidiRoute route;
};

// Return 1 if newly opened, 0 if already open, -1 on failure. A bank of
// instruments sharing one unavailable port must not retry it 128 times.
template<class Outputs>
int zt_open_midi_route_once(Outputs &outputs, int device, std::vector<bool> &attempted)
{
    if (device < 0 || static_cast<size_t>(device) >= attempted.size()) return -1;
    if (outputs.outputDevices[device]->opened) return 0;
    if (attempted[device]) return -1;
    attempted[device] = true;
    return outputs.AddDevice(device) == 0 && outputs.outputDevices[device]->opened ? 1 : -1;
}

// MIDR v1: u8 version, u16 count, then count records:
// u8 instrument, u16 name_length, u16 alias_length, name bytes, alias bytes.
// Explicit little endian, no terminators. Unknown versions are rejected.
template<class Buffer>
void zt_write_midi_routes(Buffer &buf, const std::vector<ZTMidiRouteEntry> &entries)
{
    auto u16 = [&buf](size_t n) { buf.pushuc(n & 255); buf.pushuc((n >> 8) & 255); };
    buf.pushuc(1);
    u16(entries.size());
    for (const auto &entry : entries) {
        const size_t n = std::strlen(entry.route.name), a = std::strlen(entry.route.alias);
        buf.pushuc(entry.instrument);
        u16(n); u16(a);
        buf.write(entry.route.name, static_cast<int>(n));
        buf.write(entry.route.alias, static_cast<int>(a));
    }
}

inline bool zt_read_midi_routes(const char *data, size_t size,
                               std::vector<ZTMidiRouteEntry> &entries)
{
    if (size < 3 || static_cast<unsigned char>(data[0]) != 1) return false;
    auto u16 = [data](size_t p) {
        return static_cast<unsigned char>(data[p]) |
               (static_cast<unsigned char>(data[p + 1]) << 8);
    };
    const int count = u16(1);
    if (count > 256) return false;
    std::vector<ZTMidiRouteEntry> parsed;
    bool seen[256] = {};
    size_t p = 3;
    for (int i = 0; i < count; ++i) {
        if (size - p < 5) return false;
        ZTMidiRouteEntry entry{};
        entry.instrument = static_cast<unsigned char>(data[p]);
        const size_t n = u16(p + 1), a = u16(p + 3);
        p += 5;
        if (seen[entry.instrument] || n >= sizeof(entry.route.name) ||
            a >= sizeof(entry.route.alias) || n + a > size - p || n + a == 0)
            return false;
        if (std::memchr(data + p, 0, n + a)) return false;
        seen[entry.instrument] = true;
        std::memcpy(entry.route.name, data + p, n); p += n;
        std::memcpy(entry.route.alias, data + p, a); p += a;
        parsed.push_back(entry);
    }
    if (p != size) return false;
    entries.swap(parsed); // malformed chunks never partially change routing
    return true;
}

#endif
