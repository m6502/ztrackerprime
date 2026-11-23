#ifndef ZT_JUMP_COMMAND_H
#define ZT_JUMP_COMMAND_H

// Jnnnn: jump to song order position nnnn.
//
// Tracker playback follows the jump only in song modes (F5 / F7); F6 plays a
// single pattern and ignores it, and MIDI export never follows it (export is
// one straight pass, so a backwards jump would never finish). Export instead
// writes text markers so a game engine can loop the region itself:
//   "[nnnn" at row 0 of every order position some J targets
//   "]nnnn" at the end of the J row, where the tracker jumps

#include <cstdio>

#define ZT_JUMP_MAX_ORDERS 256

inline bool zt_jump_target_valid(const int *orderlist, int orderlist_len, int target)
{
    return target >= 0 && target < orderlist_len && orderlist[target] >= 0 && orderlist[target] <= 0xFF;
}

// Order position the player must move to at the end of the row, or -1.
// playmode: 0 = single pattern (F6), anything else = song order list.
inline int zt_jump_resolve(const int *orderlist, int orderlist_len, int target,
                           int playmode, bool ignore_jumps)
{
    if (playmode == 0 || ignore_jumps) return -1;
    return zt_jump_target_valid(orderlist, orderlist_len, target) ? target : -1;
}

// Marks every order position targeted by a J in any pattern the order list plays.
// for_each_effect(pattern, fn) must call fn(effect, effect_data) for every
// event of that pattern.
template <class ForEachEffect>
void zt_jump_collect_targets(const int *orderlist, int orderlist_len,
                             bool targets[ZT_JUMP_MAX_ORDERS], ForEachEffect for_each_effect)
{
    bool scanned[ZT_JUMP_MAX_ORDERS] = {};
    for (int i = 0; i < ZT_JUMP_MAX_ORDERS; i++) targets[i] = false;

    for (int o = 0; o < orderlist_len && orderlist[o] != 0x100; o++) {
        int p = orderlist[o];
        if (p < 0 || p > 0xFF || scanned[p]) continue;
        scanned[p] = true;
        for_each_effect(p, [&](unsigned char effect, unsigned short data) {
            if (effect == 'J' && zt_jump_target_valid(orderlist, orderlist_len, data))
                targets[data] = true;
        });
    }
}

// Text of the marker meta-event: "[000A" / "]000A". Returns its length (5).
inline int zt_jump_marker_text(char out[8], bool loop_start, unsigned short value)
{
    return std::snprintf(out, 8, "%c%04X", loop_start ? '[' : ']', value);
}

#endif
