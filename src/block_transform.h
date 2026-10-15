#ifndef ZT_BLOCK_TRANSFORM_H
#define ZT_BLOCK_TRANSFORM_H

#include "module.h"
#include <algorithm>
#include <array>
#include <memory>

enum class BlockTransform { Duplicate, Expand, Halve };
enum class BlockTransformResult { Changed, Unchanged, InvalidSelection, LockTimeout };

struct BlockRange {
    int first_track, last_track, first_row, last_row;
};

// Transform selected rows and tracks in place. Expanded/duplicated output
// overwrites its destination and is clipped at the existing pattern end.
// Halving clears the unused portion of the original block. Never resize.
inline BlockTransformResult transform_block(zt_module *song, int pattern_index,
                                           BlockRange &range, BlockTransform operation)
{
    if (!song || pattern_index < 0 || pattern_index >= ZTM_MAX_PATTERNS)
        return BlockTransformResult::InvalidSelection;
    pattern *pat = song->patterns[pattern_index];
    if (!pat || range.first_track < 0 || range.last_track >= MAX_TRACKS ||
        range.first_track > range.last_track || range.first_row < 0 ||
        range.last_row >= pat->length || range.first_row > range.last_row)
        return BlockTransformResult::InvalidSelection;

    const int old_rows = range.last_row - range.first_row + 1;
    const int new_rows = operation == BlockTransform::Halve ? (old_rows + 1) / 2 : old_rows * 2;
    if (new_rows == old_rows) return BlockTransformResult::Unchanged;

    struct Lock {
        zt_mutex_handle mutex;
        bool held;
        explicit Lock(zt_mutex_handle value) : mutex(value), held(zt_mutex_lock(value, 800) != 0) {}
        ~Lock() { if (held) zt_mutex_unlock(mutex); }
    } lock(song->hEditMutex);
    if (!lock.held) return BlockTransformResult::LockTimeout;

    const int output_end = std::min<int>(pat->length - 1, range.first_row + new_rows - 1);
    const int overwritten_end = std::max(range.last_row, output_end);

    std::array<std::unique_ptr<track>, MAX_TRACKS> replacements;
    for (int t = range.first_track; t <= range.last_track; ++t) {
        track *source = pat->tracks[t];
        if (!source) continue;
        replacements[t].reset(new track(pat->length));
        auto append = [&](event *original, int row) {
            if (row >= pat->length) return;
            event *copy = new event(original);
            copy->row = static_cast<unsigned short>(row);
            copy->next_event = replacements[t]->event_list;
            replacements[t]->event_list = copy;
        };
        for (event *e = source->event_list; e; e = e->next_event) {
            if (e->row >= pat->length) continue;
            if (e->row < range.first_row) {
                append(e, e->row);
            } else if (e->row > range.last_row) {
                if (e->row > overwritten_end) append(e, e->row);
            } else {
                const int offset = e->row - range.first_row;
                switch (operation) {
                case BlockTransform::Duplicate:
                    append(e, e->row);
                    append(e, e->row + old_rows);
                    break;
                case BlockTransform::Expand:
                    append(e, range.first_row + offset * 2);
                    break;
                case BlockTransform::Halve:
                    if (offset % 2 == 0) append(e, range.first_row + offset / 2);
                    break;
                }
            }
        }
    }
    // Commit together under the playback mutex, preserving track defaults.
    for (int t = range.first_track; t <= range.last_track; ++t)
        if (replacements[t])
            std::swap(pat->tracks[t]->event_list, replacements[t]->event_list);
    range.last_row = output_end;
    return BlockTransformResult::Changed;
}

#endif
