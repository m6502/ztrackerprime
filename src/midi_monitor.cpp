#include "midi_monitor.h"
#include "winmm_compat.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cerrno>
#include <map>
#include <mutex>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
std::string clean_name(const char *name) {
    std::string result = name ? name : "";
    const auto end = std::find_if(result.begin(), result.end(), [](unsigned char c) { return c < 32; });
    result.erase(end, result.end());
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}
}

struct ZTMidiMonitor::Impl {
    std::atomic<bool> running{false}, idle{true}, requested{false}, notified{false};
    std::atomic<unsigned> request_id{0};
    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    ZTMidiPortSnapshot latest;
    bool pending = false;
#if defined(__linux__)
    snd_seq_t *seq = nullptr;
    std::vector<pollfd> descriptors;
    std::map<int, unsigned> generations;
    unsigned next_generation = 0;
#elif defined(__APPLE__)
    MIDIClientRef client = 0;
    std::mutex notification_mutex;
    std::map<MIDIObjectRef, unsigned> generations;
    unsigned next_generation = 0;
    static void notification(const MIDINotification *message, void *context) {
        auto &self = *static_cast<Impl *>(context);
        switch (message->messageID) {
        case kMIDIMsgObjectAdded:
        case kMIDIMsgObjectRemoved: {
            const auto *change = reinterpret_cast<const MIDIObjectAddRemoveNotification *>(message);
            if (change->childType != kMIDIObjectType_Source && change->childType != kMIDIObjectType_Destination)
                return;
            {
                std::lock_guard<std::mutex> guard(self.notification_mutex);
                self.generations[change->child] = ++self.next_generation;
            }
            break;
        }
        case kMIDIMsgSetupChanged:
        case kMIDIMsgPropertyChanged:
        case kMIDIMsgIOError:
            break;
        default: return;
        }
        self.notified = true;
        self.wake.notify_one();
    }
#endif

    void initialize_native() {
#if defined(__linux__)
        if (seq) return;
        if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
            seq = nullptr;
            return;
        }
        snd_seq_set_client_name(seq, "zTracker Device Monitor");
        const int port = snd_seq_create_simple_port(seq, "Device changes",
            SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE | SND_SEQ_PORT_CAP_NO_EXPORT,
            SND_SEQ_PORT_TYPE_APPLICATION);
        if (port < 0 || snd_seq_connect_from(seq, port, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE) < 0) {
            snd_seq_close(seq); seq = nullptr; return;
        }
        const int count = snd_seq_poll_descriptors_count(seq, POLLIN);
        if (count > 0) {
            descriptors.resize(count);
            snd_seq_poll_descriptors(seq, descriptors.data(), count, POLLIN);
        }
#elif defined(__APPLE__)
        if (!client && MIDIClientCreate(CFSTR("zTracker Device Monitor"), notification, this, &client) != noErr)
            client = 0;
#endif
    }

    ZTMidiPortSnapshot scan() {
        ZTMidiPortSnapshot snapshot;
        initialize_native();
#if defined(__linux__)
        if (!seq) return snapshot; // backend failure is not an empty device list
        snd_seq_client_info_t *ci;
        snd_seq_port_info_t *pi;
        snd_seq_client_info_alloca(&ci);
        snd_seq_port_info_alloca(&pi);
        snd_seq_client_info_set_client(ci, -1);
        int client_result;
        while ((client_result = snd_seq_query_next_client(seq, ci)) >= 0) {
            if (snd_seq_client_info_get_pid(ci) == getpid()) continue;
            const int owner = snd_seq_client_info_get_client(ci);
            snd_seq_port_info_set_client(pi, owner);
            snd_seq_port_info_set_port(pi, -1);
            int port_result;
            while ((port_result = snd_seq_query_next_port(seq, pi)) >= 0) {
                const unsigned caps = snd_seq_port_info_get_capability(pi);
                if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) continue;
                char name[128];
                std::snprintf(name, sizeof(name), "%s:%s", snd_seq_client_info_get_name(ci), snd_seq_port_info_get_name(pi));
                const int port = snd_seq_port_info_get_port(pi);
                const int address = (owner << 8) | port;
                const std::string id = std::to_string(owner) + ":" + std::to_string(port) + ":" + std::to_string(generations[address]);
                if ((caps & (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE)) ==
                    (SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE))
                    snapshot.outputs.push_back({clean_name(name), id, static_cast<unsigned>(snapshot.outputs.size())});
                if ((caps & (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ)) ==
                    (SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ))
                    snapshot.inputs.push_back({clean_name(name), id, static_cast<unsigned>(snapshot.inputs.size())});
            }
            if (port_result != -ENOENT) return snapshot;
        }
        if (client_result != -ENOENT) return snapshot;
        snapshot.outputs_valid = snapshot.inputs_valid = true;
#elif defined(__APPLE__)
        if (!client) return snapshot;
        for (int direction = 0; direction < 2; ++direction) {
            auto &ports = direction ? snapshot.inputs : snapshot.outputs;
            const ItemCount count = direction ? MIDIGetNumberOfSources() : MIDIGetNumberOfDestinations();
            bool valid = true;
            for (ItemCount i = 0; i < count; ++i) {
                const MIDIEndpointRef endpoint = direction ? MIDIGetSource(i) : MIDIGetDestination(i);
                if (!endpoint) { valid = false; break; }
                SInt32 offline = 0, unique = 0;
                MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyOffline, &offline);
                if (offline) continue;
                MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &unique);
                char name[128];
                zt_coremidi_endpoint_name(endpoint, name, sizeof(name));
                unsigned generation;
                {
                    std::lock_guard<std::mutex> guard(notification_mutex);
                    generation = generations[endpoint];
                }
                const std::string id = std::to_string(unique) + ":" + std::to_string(endpoint) + ":" + std::to_string(generation);
                ports.push_back({clean_name(name), id, static_cast<unsigned>(i)});
            }
            if (direction) snapshot.inputs_valid = valid;
            else snapshot.outputs_valid = valid;
        }
#else
        // WinMM exposes indexed enumeration. Read every name, since replacing
        // one device with another can leave the device count unchanged.
        for (int direction = 0; direction < 2; ++direction) {
            auto &ports = direction ? snapshot.inputs : snapshot.outputs;
            const unsigned count = direction ? midiInGetNumDevs() : midiOutGetNumDevs();
            bool valid = true;
            for (unsigned i = 0; i < count; ++i) {
                char name[256] = {};
                if (direction) {
                    MIDIINCAPS caps{};
                    if (midiInGetDevCaps(i, &caps, sizeof(caps))) { valid = false; break; }
                    std::snprintf(name, sizeof(name), "%s", caps.szPname);
                } else {
                    MIDIOUTCAPS caps{};
                    if (midiOutGetDevCaps(i, &caps, sizeof(caps))) { valid = false; break; }
                    std::snprintf(name, sizeof(name), "%s", caps.szPname);
                }
                ports.push_back({clean_name(name), clean_name(name), i});
            }
            if (direction) snapshot.inputs_valid = valid;
            else snapshot.outputs_valid = valid;
        }
#endif
        return snapshot;
    }

    void wait_for_changes() {
#if defined(__linux__)
        if (seq && !descriptors.empty()) {
            const int result = poll(descriptors.data(), descriptors.size(), 100);
            bool failed = result < 0 && errno != EINTR;
            for (const auto &descriptor : descriptors)
                failed |= (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
            if (failed) {
                snd_seq_close(seq); seq = nullptr;
                descriptors.clear();
                notified = true;
                return;
            }
            snd_seq_event_t *event = nullptr;
            while (snd_seq_event_input(seq, &event) >= 0 && event) {
                switch (event->type) {
                case SND_SEQ_EVENT_PORT_START:
                case SND_SEQ_EVENT_PORT_EXIT:
                case SND_SEQ_EVENT_PORT_CHANGE: {
                    // Application ports created by our own MIDI handles must
                    // not cause an endless discover/open/discover loop.
                    snd_seq_client_info_t *ci;
                    snd_seq_client_info_alloca(&ci);
                    const int owner = event->data.addr.client;
                    if (snd_seq_get_any_client_info(seq, owner, ci) >= 0 &&
                        snd_seq_client_info_get_pid(ci) == getpid()) break;
                    const int address = (owner << 8) | event->data.addr.port;
                    if (event->type != SND_SEQ_EVENT_PORT_CHANGE)
                        generations[address] = ++next_generation;
                    notified = true;
                    break;
                }
                default: break;
                }
            }
            return;
        }
#elif defined(__APPLE__)
        // CoreMIDI delivers notifications on the run loop that created the
        // client. Pump that worker's run loop even while playback is active.
        if (client) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
            return;
        }
#endif
        std::unique_lock<std::mutex> guard(mutex);
        wake.wait_for(guard, std::chrono::milliseconds(100), [this] {
            return !running || (idle && requested) || notified;
        });
    }

    void run() {
        auto next_scan = Clock::now();
        auto debounce = Clock::time_point::min();
        auto first_notification = Clock::time_point::min();
        while (running) {
            wait_for_changes();
            const auto now = Clock::now();
            if (notified.exchange(false)) {
                if (first_notification == Clock::time_point::min()) first_notification = now;
                debounce = std::min(now + std::chrono::milliseconds(250), first_notification + std::chrono::seconds(1));
            }
            const bool forced = requested.exchange(false);
            if (!idle) {
                if (forced) requested = true;
                continue;
            }
            const bool changed = debounce != Clock::time_point::min() && now >= debounce;
            if (!forced && !changed && now < next_scan) continue;
            const unsigned scanning_request = request_id.load();
            auto snapshot = scan();
            {
                std::lock_guard<std::mutex> guard(mutex);
                if (idle && scanning_request == request_id.load()) {
                    latest = std::move(snapshot);
                    pending = true;
                }
            }
#if defined(__linux__) || defined(__APPLE__)
            next_scan = now + std::chrono::seconds(30);
#else
            next_scan = now + std::chrono::seconds(3);
#endif
            debounce = Clock::time_point::min();
            first_notification = Clock::time_point::min();
        }
#if defined(__linux__)
        if (seq) { snd_seq_close(seq); seq = nullptr; }
        descriptors.clear();
#elif defined(__APPLE__)
        if (client) { MIDIClientDispose(client); client = 0; }
#endif
    }
};

ZTMidiMonitor::ZTMidiMonitor() : impl(new Impl) {}
ZTMidiMonitor::~ZTMidiMonitor() { stop(); }
void ZTMidiMonitor::start() {
    if (impl->running.exchange(true)) return;
    request_scan();
    impl->worker = std::thread([this] { impl->run(); });
}
void ZTMidiMonitor::stop() {
    impl->running = false;
    impl->wake.notify_one();
    if (impl->worker.joinable()) impl->worker.join();
}
void ZTMidiMonitor::set_idle(bool idle) {
    if (impl->idle.exchange(idle) == idle) return;
    if (idle) request_scan();
    else {
        std::lock_guard<std::mutex> guard(impl->mutex);
        impl->pending = false;
    }
}
void ZTMidiMonitor::request_scan() {
    {
        std::lock_guard<std::mutex> guard(impl->mutex);
        ++impl->request_id;
        impl->pending = false;
    }
    impl->requested = true;
    impl->wake.notify_one();
}
bool ZTMidiMonitor::take_snapshot(ZTMidiPortSnapshot &snapshot) {
    std::lock_guard<std::mutex> guard(impl->mutex);
    if (!impl->pending) return false;
    snapshot = std::move(impl->latest);
    impl->pending = false;
    return true;
}
