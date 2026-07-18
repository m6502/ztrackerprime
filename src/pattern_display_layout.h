#ifndef ZT_PATTERN_DISPLAY_LAYOUT_H
#define ZT_PATTERN_DISPLAY_LAYOUT_H

// ---------------------------------------------------------
// ---------------------------------------------------------

struct PatternDisplayPosition
{
    int order;
    int pattern;
    int row;
};

// ---------------------------------------------------------
// ---------------------------------------------------------

int pattern_display_adjacent_order(const int *orders, int order, int direction) ;
PatternDisplayPosition pattern_display_position_at_offset(const int *orders, const int *lengths, int playmode,
                                                          int playing_order, int playing_pattern, int playing_row, int offset) ;


#endif
