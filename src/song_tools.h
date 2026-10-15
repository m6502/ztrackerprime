#ifndef ZT_SONG_TOOLS_H
#define ZT_SONG_TOOLS_H

#include "module.h"
#include <array>
#include <cstring>

enum class SongReorderKind { Patterns, Tracks };

// destination[old slot] = new slot. Track scope -1 means every pattern.
struct SongReorder {
    SongReorderKind kind = SongReorderKind::Patterns;
    int pattern_index = -1;
    std::array<int, ZTM_MAX_PATTERNS> destination{};
    SongReorder() { for (int i = 0; i < ZTM_MAX_PATTERNS; ++i) destination[i] = i; }
    int count() const { return kind == SongReorderKind::Patterns ? ZTM_MAX_PATTERNS : MAX_TRACKS; }
    bool identity() const {
        for (int i = 0; i < count(); ++i) if (destination[i] != i) return false;
        return true;
    }
    bool valid() const {
        std::array<bool, ZTM_MAX_PATTERNS> seen{};
        for (int i = 0; i < count(); ++i) {
            int target = destination[i];
            if (target < 0 || target >= count() || seen[target]) return false;
            seen[target] = true;
        }
        return pattern_index >= -1 && pattern_index < ZTM_MAX_PATTERNS;
    }
    SongReorder inverse() const {
        SongReorder result = *this;
        for (int i = 0; i < count(); ++i) result.destination[destination[i]] = i;
        return result;
    }
};

// Optional main-thread UI observer: keep cached playback/editor positions
// attached to their contents on both forward operations and undo/redo.
inline void (*g_song_reorder_observer)(const SongReorder &) = nullptr;

inline SongReorder make_song_reorder(SongReorderKind kind, int source, int target,
                                     bool swap, int pattern_index = -1)
{
    SongReorder result;
    result.kind = kind; result.pattern_index = pattern_index;
    if (source < 0 || target < 0 || source >= result.count() || target >= result.count()) return result;
    result.destination[source] = target;
    if (swap) result.destination[target] = source;
    else if (source < target)
        for (int i = source + 1; i <= target; ++i) result.destination[i] = i - 1;
    else
        for (int i = target; i < source; ++i) result.destination[i] = i + 1;
    return result;
}

inline std::array<bool, ZTM_MAX_PATTERNS> song_used_patterns(const zt_module &song)
{
    std::array<bool, ZTM_MAX_PATTERNS> used{};
    for (int entry : song.orderlist)
        if (entry >= 0 && entry < ZTM_MAX_PATTERNS) used[entry] = true;
    return used;
}

inline SongReorder song_defrag_order(const zt_module &song)
{
    SongReorder result;
    std::array<bool, ZTM_MAX_PATTERNS> seen{};
    int next = 0;
    for (int entry : song.orderlist) {
        if (entry >= 0 && entry < ZTM_MAX_PATTERNS && !seen[entry]) {
            result.destination[entry] = next++;
            seen[entry] = true;
        }
    }
    for (int i = 0; i < ZTM_MAX_PATTERNS; ++i)
        if (!seen[i]) result.destination[i] = next++;
    return result;
}

inline bool song_pattern_has_events(const pattern *pat)
{
    if (pat) for (const track *trk : pat->tracks) if (trk && trk->event_list) return true;
    return false;
}

inline int clear_song_unused_patterns(zt_module *song)
{
    if (!song || !zt_mutex_lock(song->hEditMutex, 800)) return -1;
    const auto used = song_used_patterns(*song);
    int cleared = 0;
    for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) {
        pattern *pat = song->patterns[p];
        if (used[p] || !song_pattern_has_events(pat)) continue;
        for (track *trk : pat->tracks) {
            if (!trk) continue;
            while (trk->event_list) {
                event *e = trk->event_list;
                trk->event_list = e->next_event;
                delete e;
            }
        }
        ++cleared;
    }
    zt_mutex_unlock(song->hEditMutex);
    return cleared;
}

inline bool apply_song_reorder(zt_module *song, const SongReorder &reorder, int *draw_modes = nullptr)
{
    if (!song || !reorder.valid() || reorder.identity()) return false;
    if (!zt_mutex_lock(song->hEditMutex, 800)) return false;
    if (reorder.kind == SongReorderKind::Patterns) {
        std::array<pattern *, ZTM_MAX_PATTERNS> original;
        for (int i = 0; i < ZTM_MAX_PATTERNS; ++i) original[i] = song->patterns[i];
        for (int i = 0; i < ZTM_MAX_PATTERNS; ++i) song->patterns[reorder.destination[i]] = original[i];
        for (int &entry : song->orderlist)
            if (entry >= 0 && entry < ZTM_MAX_PATTERNS) entry = reorder.destination[entry];
    } else {
        for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) {
            if (reorder.pattern_index >= 0 && p != reorder.pattern_index) continue;
            pattern *pat = song->patterns[p];
            if (!pat) continue;
            std::array<track *, MAX_TRACKS> original;
            for (int t = 0; t < MAX_TRACKS; ++t) original[t] = pat->tracks[t];
            for (int t = 0; t < MAX_TRACKS; ++t) pat->tracks[reorder.destination[t]] = original[t];
        }
        if (reorder.pattern_index < 0) {
            struct Properties {
                char name[ZTM_TRACKNAME_MAXLEN];
                unsigned long color;
                unsigned char mute;
                int draw_mode;
            } original[MAX_TRACKS];
            for (int t = 0; t < MAX_TRACKS; ++t) {
                std::memcpy(original[t].name, song->track_name[t], ZTM_TRACKNAME_MAXLEN);
                original[t].color = song->track_color[t]; original[t].mute = song->track_mute[t];
                original[t].draw_mode = draw_modes ? draw_modes[t] : 0;
            }
            for (int t = 0; t < MAX_TRACKS; ++t) {
                int target = reorder.destination[t];
                std::memcpy(song->track_name[target], original[t].name, ZTM_TRACKNAME_MAXLEN);
                song->track_color[target] = original[t].color; song->track_mute[target] = original[t].mute;
                if (draw_modes) draw_modes[target] = original[t].draw_mode;
            }
        }
    }
    zt_mutex_unlock(song->hEditMutex);
    if (g_song_reorder_observer) g_song_reorder_observer(reorder);
    return true;
}

// UI source selections survive closing the popup, but never a song change.
struct SongToolsSelection {
    zt_module *owner = nullptr;
    int generation = -1;
    pattern *source_pattern = nullptr;
    int source_track = -1;
    void sync(zt_module *song, int song_generation) {
        if (owner != song || generation != song_generation) {
            owner = song; generation = song_generation;
            source_pattern = nullptr; source_track = -1;
        }
    }
    int pattern_index(const zt_module *song) const {
        if (song && source_pattern)
            for (int i = 0; i < ZTM_MAX_PATTERNS; ++i)
                if (song->patterns[i] == source_pattern) return i;
        return -1;
    }
};

inline SongToolsSelection g_song_tools_selection;
#endif
