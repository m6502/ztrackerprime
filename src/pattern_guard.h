#ifndef ZT_PATTERN_GUARD_H
#define ZT_PATTERN_GUARD_H

// Pattern-index validity + order-list sentinel handling.
//
// patterns[] is sized ZTM_MAX_PATTERNS (256); valid indices are 0..255.
// Order-list entries reuse out-of-range values as sentinels: 0x100 = empty /
// end-of-song, 0x101 = skip. Those sentinels must NEVER be used to index
// patterns[] -- doing so reads past the array and dereferences a garbage
// track pointer.
//
// Bus-error history (crash report 2026-06-03): clicking an empty order slot
// in the F11 Order editor ran `cur_edit_pattern = orderlist[slot]` (= 0x100)
// with no guard; pressing F6 then called player::play(cur_edit_pattern) ->
// playback() -> patterns[256]->tracks[t]->get_event() -> SIGBUS
// (KERN_PROTECTION_FAILURE). The player play path now funnels through
// zt_pattern_index_playable() (see play_immediately() in playback.cpp); the
// F11 editor paths already guard cur_edit_pattern inline with `< 0x100`.
// zt_orderlist_select_pattern() is the canonical order-entry resolver,
// exercised by the unit test and available should those inline guards ever be
// unified onto it.
//
// Header-only and dependency-free on purpose: the unit test includes it
// directly without SDL or module.h. ZT_PATTERN_GUARD_COUNT mirrors
// ZTM_MAX_PATTERNS; a static_assert in playback.cpp keeps them in lock-step
// so this constant can't silently drift from the real patterns[] size.

#ifndef ZT_PATTERN_GUARD_COUNT
#define ZT_PATTERN_GUARD_COUNT 256
#endif

// True iff idx is a real, in-range pattern slot -- not a sentinel (>= count),
// not negative.
static inline int zt_pattern_index_playable(int idx) {
    return idx >= 0 && idx < ZT_PATTERN_GUARD_COUNT;
}

// Resolve an order-list entry to an editable pattern index. Real patterns pass
// through; sentinels (or any out-of-range value) return `fallback`, so a
// sentinel never leaks into cur_edit_pattern. Matches the behaviour of the
// already-guarded order paths (which leave cur_edit_pattern unchanged on a
// marker).
static inline int zt_orderlist_select_pattern(int orderlist_value, int fallback) {
    return zt_pattern_index_playable(orderlist_value) ? orderlist_value : fallback;
}

#endif // ZT_PATTERN_GUARD_H
