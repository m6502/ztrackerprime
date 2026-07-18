#include "pattern_display_layout.h"

// ----------------------------------------------------------------------------------------
//
//
int pattern_display_adjacent_order(const int *orders, int order, int direction)
{
    const int limit = 256;

    for (int attempts = 0; attempts < limit * 2; ++attempts) {
        order += direction;

        if (order < 0) {
            order = limit - 1;
            while (order > 0 && orders[order] != 0x100)
                --order;
            if (orders[order] == 0x100)
                --order;
        } else if (order >= limit || orders[order] == 0x100) {
            order = 0;
        }

        if (order >= 0 && order < limit && orders[order] <= 0xFF)
            return order;
    }

    return -1;
}



// ----------------------------------------------------------------------------------------
// Resolve a screen-row offset relative to the current play position.
// Traverses as many pattern boundaries as the view size requires. 
//
PatternDisplayPosition pattern_display_position_at_offset(
                                                         const int *orders, const int *lengths, int playmode,
                                                         int playing_order, int playing_pattern, int playing_row, int offset)
{
    PatternDisplayPosition pos = { playing_order, playing_pattern, playing_row + offset };

    if (!playmode) {
        const int length = lengths[pos.pattern];
        if (length > 0) {
            pos.row %= length;
            if (pos.row < 0)
                pos.row += length;
        }
        return pos;
    }

    while (pos.row < 0) {
        const int previous = pattern_display_adjacent_order(orders, pos.order, -1);
        if (previous < 0)
            break;
        pos.order = previous;
        pos.pattern = orders[previous];
        pos.row += lengths[pos.pattern];
    }

    while (lengths[pos.pattern] > 0 && pos.row >= lengths[pos.pattern]) {
        pos.row -= lengths[pos.pattern];
        const int next = pattern_display_adjacent_order(orders, pos.order, 1);
        if (next < 0)
            break;
        pos.order = next;
        pos.pattern = orders[next];
    }

    return pos;
}

