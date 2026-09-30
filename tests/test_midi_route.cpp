#include "midi_route.h"
#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "%d: %s\n", __LINE__, #x); std::exit(1); } } while (0)

struct Buffer {
    std::string data;
    void pushuc(unsigned char c) { data += static_cast<char>(c); }
    void write(const char *p, int size) { data.append(p, size); }
};

struct FakeOutputs {
    struct Device { int opened = 0; } a, b;
    Device *outputDevices[2] = {&a, &b};
    int calls[2] = {};
    int AddDevice(int i) {
        ++calls[i];
        if (i == 1) return 7; // driver failure
        outputDevices[i]->opened = 1;
        return 0;
    }
};

int main()
{
    ZTMidiRoute empty, synth;
    std::strcpy(synth.name, "Synth A");
    std::strcpy(synth.alias, "Lead synth");
    std::vector<ZTMidiDestination> devices = {
        {"Synth B", "Bass", true, true}, {"Synth A", "Lead synth", false, true},
        {"Noise", "", true, false}};
    CHECK(zt_resolve_midi_route(0, synth, devices).device == 1); // reordered, closed
    CHECK(zt_resolve_midi_route(255, synth, devices).device == 1); // return after offline save
    devices[1].name = "Renamed port";
    CHECK(zt_resolve_midi_route(0, synth, devices).device == 1); // alias portability
    devices[1].alias = "Other";
    CHECK(zt_resolve_midi_route(0, synth, devices).device == -1); // no wrong-slot fallback
    CHECK(zt_resolve_midi_route(14, empty, devices).device == 0);
    CHECK(zt_resolve_midi_route(14, empty, devices).fallback);
    CHECK(zt_resolve_midi_route(2, empty, devices).device == 0); // stale slot is now audio
    CHECK(zt_resolve_midi_route(1, empty, devices).device == 1); // retain closed slot
    CHECK(!zt_resolve_midi_route(1, empty, devices).fallback);
    CHECK(zt_resolve_midi_route(64, empty, devices).device == -1);
    CHECK(zt_resolve_midi_route(255, empty, devices).device == -1);
    devices[1].opened = true;
    CHECK(zt_resolve_midi_route(14, empty, devices).device == -1); // ambiguous
    CHECK(zt_resolve_midi_route(2, empty, devices).device == -1); // never route old MIDI to noise
    ZTMidiRoute audio;
    std::strcpy(audio.name, "Noise");
    CHECK(zt_resolve_midi_route(2, audio, devices).device == 2); // named audio remains supported
    devices[0].opened = devices[1].opened = false;
    CHECK(zt_resolve_midi_route(14, empty, devices).device == -1); // audio is not fallback
    CHECK(zt_resolve_midi_route(0, empty, {}).device == -1);
    devices[0].name = devices[1].name = "Synth A";
    devices[0].alias = devices[1].alias = "Lead synth";
    CHECK(zt_resolve_midi_route(0, synth, devices).device == -1); // ambiguous names
    devices[0].alias = "Bass";
    CHECK(zt_resolve_midi_route(0, synth, devices).device == 1); // alias disambiguates

    FakeOutputs outputs;
    std::vector<bool> attempted(2, false);
    CHECK(zt_open_midi_route_once(outputs, 0, attempted) == 1);
    CHECK(zt_open_midi_route_once(outputs, 0, attempted) == 0);
    CHECK(outputs.calls[0] == 1);
    CHECK(zt_open_midi_route_once(outputs, 1, attempted) == -1);
    CHECK(zt_open_midi_route_once(outputs, 1, attempted) == -1);
    CHECK(outputs.calls[1] == 1);
    CHECK(zt_open_midi_route_once(outputs, 255, attempted) == -1);

    Buffer buf;
    std::vector<ZTMidiRouteEntry> entries = {{0, synth}, {127, synth}};
    zt_write_midi_routes(buf, entries);
    CHECK(static_cast<unsigned char>(buf.data[0]) == 1);
    CHECK(static_cast<unsigned char>(buf.data[1]) == 2);
    std::vector<ZTMidiRouteEntry> loaded;
    CHECK(zt_read_midi_routes(buf.data.data(), buf.data.size(), loaded));
    CHECK(loaded.size() == 2 && loaded[1].instrument == 127);
    CHECK(!std::strcmp(loaded[0].route.name, synth.name));
    CHECK(!std::strcmp(loaded[0].route.alias, synth.alias));
    Buffer again;
    zt_write_midi_routes(again, loaded);
    CHECK(again.data == buf.data); // offline save retains identities byte for byte
    for (size_t size = 0; size < buf.data.size(); ++size) {
        CHECK(!zt_read_midi_routes(buf.data.data(), size, loaded));
        CHECK(loaded.size() == 2); // no partial updates
    }
    std::string bad = buf.data;
    bad[0] = 2;
    CHECK(!zt_read_midi_routes(bad.data(), bad.size(), loaded));
    bad = buf.data; bad[4] = '\xff'; bad[5] = '\x7f'; // oversized name
    CHECK(!zt_read_midi_routes(bad.data(), bad.size(), loaded));
    bad = buf.data; bad[8] = 0; // embedded NUL
    CHECK(!zt_read_midi_routes(bad.data(), bad.size(), loaded));
    bad = buf.data + "extra";
    CHECK(!zt_read_midi_routes(bad.data(), bad.size(), loaded));
    entries[1].instrument = 0;
    Buffer duplicates;
    zt_write_midi_routes(duplicates, entries);
    CHECK(!zt_read_midi_routes(duplicates.data.data(), duplicates.data.size(), loaded));
    std::puts("MIDI routing: resolution, activation, persistence, malformed data passed");
}
