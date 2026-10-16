#include "zt.h"
#include "midi_monitor.h"
#include "ableton_link.h"

#include <algorithm>
#include <filesystem>
#include <set>

extern MidiOutDeviceOpener *midioutdevlist;
extern MidiInDeviceOpener *midiindevlist;
extern MidiOutDeviceSelector *mds;

namespace {
ZTMidiMonitor monitor;
ZTMidiPortSnapshot ports;
bool started = false;
std::uint64_t next_update = 0;

std::string config_key(const char *prefix, const std::string &name) {
    std::string result(prefix);
    for (unsigned char c : name.substr(0, 255))
        result += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '_';
    return result;
}
ZTMidiConnectionState saved_settings(conf &config, const std::string &name) {
    ZTMidiConnectionState state;
    const char *value = config.get(config_key("alias_", name).c_str());
    if (value) state.alias = value;
    value = config.get(config_key("latency_", name).c_str());
    if (value) state.delay_ticks = atoi(value);
    value = config.get(config_key("bank_", name).c_str());
    state.bank_select = value && (value[0] == 'y' || value[0] == 'Y');
    return state;
}
ZTMidiConnectionState &output_settings(const std::string &name) {
    auto &states = zt_midi_connections(true);
    const auto found = states.find(name);
    if (found != states.end()) return found->second;
    const auto path = (std::filesystem::path(cur_dir ? cur_dir : ".") / "devices.conf").string();
    conf config(path.c_str());
    return states.emplace(name, saved_settings(config, name)).first->second;
}
void load_preferences() {
    const auto path = (std::filesystem::path(cur_dir ? cur_dir : ".") / "devices.conf").string();
    conf config(path.c_str());
    for (bool output : {false, true}) {
        auto &states = zt_midi_connections(output);
        for (int i = 0; i < MAX_MIDI_DEVS; ++i) {
            // Read open requests first; known-but-closed devices stay closed.
            const std::string key = std::string("open_") + (output ? "out" : "in") + "_device_" + std::to_string(i);
            const char *name = config.get(key.c_str());
            if (!name || !name[0]) continue;
            auto state = output ? saved_settings(config, name) : ZTMidiConnectionState{};
            state.reopen = zt_config_globals.auto_open_midi != 0;
            states.emplace(name, state);
        }
        for (int i = 0; i < MAX_MIDI_DEVS; ++i) {
            const std::string key = std::string("known_") + (output ? "out" : "in") + "_device_" + std::to_string(i);
            const char *name = config.get(key.c_str());
            if (name && name[0]) states.emplace(name, output ? saved_settings(config, name) : ZTMidiConnectionState{});
        }
    }
}
void remember_live_settings() {
    for (unsigned i = 0; i < MidiOut->numOuputDevices; ++i) {
        auto *dev = MidiOut->outputDevices[i];
        auto &state = zt_midi_connections(true)[dev->szName];
        state.alias = MidiOut->get_alias(i);
        state.delay_ticks = MidiOut->get_delay_ticks(i);
        state.bank_select = MidiOut->get_bank_select(i);
        if (dev->type != OUTPUTDEVICE_TYPE_MIDI) state.reopen = MidiOut->QueryDevice(i) != 0;
        else if (MidiOut->QueryDevice(i)) state.reopen = true;
    }
    for (unsigned i = 0; i < MidiIn->numMidiDevs; ++i) {
        const auto *dev = MidiIn->midiInDev[i];
        if (dev->szName && MidiIn->QueryDevice(i)) zt_midi_connections(false)[dev->szName].reopen = true;
    }
}

// Prefer an exact native endpoint. Match returning endpoints by an unambiguous
// name, never by the new host index. Keep application slots stable throughout
// the session so audition voices and pending note-offs cannot reach a new port.
template<class Name, class Id>
std::vector<int> match_ports(const std::vector<ZTDetectedMidiPort> &detected,
                             unsigned count, Name name, Id id) {
    std::vector<int> slots(detected.size(), -1);
    std::set<int> used;
    for (size_t p = 0; p < detected.size(); ++p) {
        for (unsigned i = 0; i < count; ++i) {
            if (used.count(i)) continue;
            if (!id(i).empty() && id(i) == detected[p].id) {
                slots[p] = i; used.insert(i); break;
            }
        }
    }
    for (size_t p = 0; p < detected.size(); ++p) {
        if (slots[p] >= 0) continue;
        if (std::count_if(detected.begin(), detected.end(), [&](const ZTDetectedMidiPort &port) {
                return port.name == detected[p].name;
            }) != 1) continue;
        int match = -1;
        for (unsigned i = 0; i < count; ++i) {
            if (!used.count(i) && name(i) == detected[p].name) {
                if (match >= 0) { match = -1; break; }
                match = i;
            }
        }
        if (match >= 0) { slots[p] = match; used.insert(match); }
    }
    return slots;
}

void refresh_device_list(ListBox *list) {
    if (!list) return;
    const int index = list->getCurrentItemIndex();
    const auto *node = list->getNode(index);
    const std::string caption = node && node->caption ? node->caption : "";
    list->OnChange();
    if (list->num_elements > 0) {
        const int found = list->findItem((char *)caption.c_str());
        list->setCursor(found >= 0 ? found : (std::min)(index, list->num_elements - 1));
    }
    list->need_redraw++;
}

struct Changes { int connected = 0, disconnected = 0, failed = 0; bool topology = false; };
void reconcile_outputs(const std::vector<ZTDetectedMidiPort> &detected, Changes &changes, std::uint64_t now) {
    auto slots = match_ports(detected, MidiOut->numOuputDevices,
        [](unsigned i) { return std::string(MidiOut->outputDevices[i]->type == OUTPUTDEVICE_TYPE_MIDI ? MidiOut->outputDevices[i]->szName : ""); },
        [](unsigned i) { return MidiOut->outputDevices[i]->type == OUTPUTDEVICE_TYPE_MIDI ? static_cast<MidiOutputDevice *>(MidiOut->outputDevices[i])->native_id : std::string(); });
    for (unsigned i = 0; i < MidiOut->numOuputDevices; ++i) {
        auto *dev = MidiOut->outputDevices[i];
        if (dev->type != OUTPUTDEVICE_TYPE_MIDI || std::find(slots.begin(), slots.end(), i) != slots.end()) continue;
        if (dev->available) {
            const bool active = MidiOut->QueryDevice(i) != 0;
            if (active) MidiOut->RemDevice(i);
            dev->clear_notestates();
            dev->available = false;
            if (active || zt_midi_connections(true)[dev->szName].reopen) ++changes.disconnected;
            changes.topology = true;
        }
    }
    for (size_t p = 0; p < detected.size(); ++p) {
        const auto &port = detected[p];
        int slot = slots[p];
        if (slot < 0) {
            // Duplicate names are visible only through the manual refresh;
            // reconnecting a stored name to either duplicate would be a guess.
            if (std::count_if(detected.begin(), detected.end(), [&](const ZTDetectedMidiPort &other) { return other.name == port.name; }) != 1)
                continue;
            if (MidiOut->numOuputDevices >= MAX_MIDI_OUTS) continue;
            slot = MidiOut->numOuputDevices++;
            MidiOut->outputDevices[slot] = new MidiOutputDevice(port.index);
            auto &state = output_settings(port.name);
            MidiOut->set_alias(slot, (char *)state.alias.c_str());
            MidiOut->set_delay_ticks(slot, state.delay_ticks);
            MidiOut->set_bank_select(slot, state.bank_select);
            changes.topology = true;
        }
        auto *dev = static_cast<MidiOutputDevice *>(MidiOut->outputDevices[slot]);
        const bool replaced = !dev->available || (!dev->native_id.empty() && dev->native_id != port.id);
        if (replaced) {
            if (MidiOut->QueryDevice(slot)) MidiOut->RemDevice(slot);
            dev->clear_notestates();
            zt_midi_connections(true)[port.name].retry.reset();
            changes.topology = true;
        }
        // Native IDs also let us follow a renamed endpoint without touching
        // its healthy handle. Migrate bound instruments and session settings.
        if (port.name != dev->szName) {
            auto &states = zt_midi_connections(true);
            states.emplace(port.name, states[dev->szName]);
            for (int i = 0; i < ZTM_MAX_INSTS; ++i) {
                auto *inst = song->instruments[i];
                if (inst->midi_device == slot)
                    snprintf(inst->midi_route.name, sizeof(inst->midi_route.name), "%s", port.name.c_str());
            }
            snprintf(dev->szName, sizeof(dev->szName), "%s", port.name.c_str());
            changes.topology = true;
        }
        dev->devNum = port.index;
        dev->native_id = port.id;
        dev->available = true;
    }
    zt_reconnect_midi_instruments();
    std::set<int> required;
    for (int i = 0; i < ZTM_MAX_INSTS; ++i) {
        const auto *inst = song->instruments[i];
        if (inst->midi_route.named() && inst->midi_device < MidiOut->numOuputDevices)
            required.insert(inst->midi_device);
    }
    for (unsigned i = 0; i < MidiOut->numOuputDevices; ++i) {
        auto *dev = MidiOut->outputDevices[i];
        if (!dev->available || dev->type != OUTPUTDEVICE_TYPE_MIDI) continue;
        auto &state = zt_midi_connections(true)[dev->szName];
        if (state.explicitly_closed || (!state.reopen && !required.count(i)) || MidiOut->QueryDevice(i) || !state.retry.ready(now)) continue;
        state.reopen = true;
        if (MidiOut->AddDevice(i) == 0) { state.retry.reset(); ++changes.connected; }
        else { state.retry.failed(now); ++changes.failed; }
    }
}

void reconcile_inputs(const std::vector<ZTDetectedMidiPort> &detected, Changes &changes, std::uint64_t now) {
    auto slots = match_ports(detected, MidiIn->numMidiDevs,
        [](unsigned i) { const char *name = MidiIn->midiInDev[i]->szName; return std::string(name ? name : ""); },
        [](unsigned i) { return MidiIn->midiInDev[i]->native_id; });
    for (unsigned i = 0; i < MidiIn->numMidiDevs; ++i) {
        auto *dev = MidiIn->midiInDev[i];
        if (std::find(slots.begin(), slots.end(), i) != slots.end()) continue;
        if (dev->available) {
            const bool active = MidiIn->QueryDevice(i) != 0;
            if (active) MidiIn->RemDevice(i);
            dev->available = false;
            if (active || (dev->szName && zt_midi_connections(false)[dev->szName].reopen)) ++changes.disconnected;
            changes.topology = true;
        }
    }
    for (size_t p = 0; p < detected.size(); ++p) {
        const auto &port = detected[p];
        int slot = slots[p];
        if (slot < 0) {
            if (std::count_if(detected.begin(), detected.end(), [&](const ZTDetectedMidiPort &other) { return other.name == port.name; }) != 1)
                continue;
            if (MidiIn->numMidiDevs >= MAX_MIDI_INS) continue;
            slot = MidiIn->numMidiDevs++;
            MidiIn->midiInDev[slot] = new midiInDevice(port.index);
            changes.topology = true;
        }
        auto *dev = MidiIn->midiInDev[slot];
        if (!dev->available || (!dev->native_id.empty() && dev->native_id != port.id)) {
            if (MidiIn->QueryDevice(slot)) MidiIn->RemDevice(slot);
            zt_midi_connections(false)[port.name].retry.reset();
            changes.topology = true;
        }
        if (!dev->szName || port.name != dev->szName) {
            auto &states = zt_midi_connections(false);
            if (dev->szName) states.emplace(port.name, states[dev->szName]);
            snprintf(dev->caps.szPname, sizeof(dev->caps.szPname), "%s", port.name.c_str());
            dev->szName = dev->caps.szPname;
            changes.topology = true;
        }
        dev->devNum = port.index;
        dev->native_id = port.id;
        dev->available = true;
        auto &state = zt_midi_connections(false)[port.name];
        if (state.explicitly_closed || !state.reopen || MidiIn->QueryDevice(slot) || !state.retry.ready(now)) continue;
        if (MidiIn->AddDevice(slot) == 0) { state.retry.reset(); ++changes.connected; }
        else { state.retry.failed(now); ++changes.failed; }
    }
}
}

void zt_midi_hotplug_start() {
    if (started) return;
    load_preferences();
    remember_live_settings();
    started = true;
    next_update = 0;
    ports = {};
    monitor.start();
}
void zt_midi_hotplug_stop() {
    monitor.stop();
    started = false;
}
void zt_midi_hotplug_rescan() {
    if (!started) return;
    ports = {}; // do not apply an old snapshot after a manual refresh
    for (bool output : {false, true})
        for (auto &entry : zt_midi_connections(output)) entry.second.retry.reset();
    monitor.request_scan();
    next_update = 0;
}
void zt_midi_hotplug_pump() {
    if (!started || !song || !ztPlayer || !MidiOut || !MidiIn) return;
    const bool idle = !ztPlayer->playing && !zt_ableton_link_play_pending();
    monitor.set_idle(idle);
    if (!idle) { ports = {}; return; }
    const auto now = static_cast<std::uint64_t>(SDL_GetTicks());
    const bool fresh = monitor.take_snapshot(ports);
    if (!fresh && now < next_update) return;
    next_update = now + 250;
    // Timeout zero: automatic maintenance must never wait for the playback
    // producer. All device mutations and route updates stay on the UI thread.
    if (!lock_mutex(song->hEditMutex, 0)) return;
    Changes changes;
    {
        std::unique_lock<std::mutex> audio_guard(zt_midi_output_mutex, std::try_to_lock);
        if (!audio_guard.owns_lock()) {
            unlock_mutex(song->hEditMutex);
            return;
        }
        zt_capture_midi_instruments();
        remember_live_settings();
        if (ports.outputs_valid) reconcile_outputs(ports.outputs, changes, now);
        if (ports.inputs_valid) reconcile_inputs(ports.inputs, changes, now);
    }
    unlock_mutex(song->hEditMutex);
    if (changes.topology || changes.connected) {
        refresh_device_list(midioutdevlist);
        refresh_device_list(midiindevlist);
        refresh_device_list(mds);
        if (UIP_InstEditor) UIP_InstEditor->reset = 1;
        need_refresh++;
    }
    if (changes.connected || changes.disconnected || changes.failed) {
        snprintf(szStatmsg, sizeof(szStatmsg), "MIDI auto: %d reconnected, %d disconnected, %d failed",
                 changes.connected, changes.disconnected, changes.failed);
        statusmsg = szStatmsg;
        status_change = 1;
        need_refresh++;
    }
}

// Save requested connections, including temporarily absent devices. Rebuild
// indexed keys from names so a disconnected launch/exit cannot erase them.
void zt_save_midi_connections(conf &config) {
    load_preferences();
    remember_live_settings();
    for (bool output : {false, true}) {
        auto &states = zt_midi_connections(output);
        std::set<std::string> known;
        for (int i = 0; i < MAX_MIDI_DEVS; ++i) {
            const std::string suffix = std::string(output ? "out" : "in") + "_device_" + std::to_string(i);
            const char *name = config.get(("known_" + suffix).c_str());
            if (name && name[0] && (!states.count(name) || !states[name].forgotten)) known.insert(name);
            config.remove(("known_" + suffix).c_str());
            config.remove(("open_" + suffix).c_str());
        }
        int enabled = 0;
        for (const auto &entry : states) {
            const auto &state = entry.second;
            if (state.forgotten) continue;
            known.insert(entry.first);
            if (zt_config_globals.auto_open_midi && state.reopen && !state.explicitly_closed && enabled < MAX_MIDI_DEVS) {
                const auto key = std::string("open_") + (output ? "out" : "in") + "_device_" + std::to_string(enabled++);
                config.set(key.c_str(), entry.first.c_str(), 0);
            }
            if (output) {
                config.set(config_key("alias_", entry.first).c_str(), state.alias.c_str());
                config.set(config_key("latency_", entry.first).c_str(), std::to_string(state.delay_ticks).c_str());
                config.set(config_key("bank_", entry.first).c_str(), state.bank_select ? "Yes" : "No");
            }
        }
        int index = 0;
        for (const auto &name : known) {
            if (index >= MAX_MIDI_DEVS) break;
            const auto key = std::string("known_") + (output ? "out" : "in") + "_device_" + std::to_string(index++);
            config.set(key.c_str(), name.c_str(), 0);
        }
    }
}
