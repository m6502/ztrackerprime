#include "pattern_display_layout.h"

#include <cstdio>
#include <initializer_list>

static int failures = 0;

#define CHECK_EQ(actual, expected) do { \
    const int value = (actual); \
    if (value != (expected)) { \
        std::fprintf(stderr, "FAIL %s:%d: got %d, expected %d\n", \
                     __FILE__, __LINE__, value, (expected)); \
        ++failures; \
    } \
} while (0)

int main()
{
    // Use the maximum number of complete channels, then center the whole grid.
    for (int columns : {80, 100, 104, 105, 106, 128, 180}) {
        for (int width : {7, 8, 14, 20}) {
            const auto layout = pattern_display_horizontal_layout(columns, width, 64);
            CHECK_EQ(layout.tracks, (columns - 5) / width);
            const int right = columns - layout.x - (4 + layout.tracks * width);
            CHECK_EQ(right >= 0, 1);
            CHECK_EQ(layout.x - right >= -1 && layout.x - right <= 1, 1);
        }
    }

    // 840px / 8px = 105 columns: the editor and playback both fit five Full channels.
    CHECK_EQ(pattern_display_horizontal_layout(105, 20, 64).tracks, 5);
    CHECK_EQ(pattern_display_horizontal_layout(104, 20, 64).tracks, 4);

    int orders[256];
    int lengths[256];
    for (int i = 0; i < 256; ++i) {
        orders[i] = 0x101;
        lengths[i] = 64;
    }
    orders[0] = 10;
    orders[1] = 11;
    orders[2] = 12;
    orders[3] = 0x100;

    // The reported transition: pattern 11 has just become current at row 00.
    // Even with a viewport extending across several complete patterns, offset
    // zero must remain pattern 11/row 00, never the following pattern 12.
    PatternDisplayPosition pos = pattern_display_position_at_offset(
        orders, lengths, 1, 1, 11, 0, 0);
    CHECK_EQ(pos.pattern, 11);
    CHECK_EQ(pos.row, 0);

    pos = pattern_display_position_at_offset(orders, lengths, 1, 1, 11, 0, -132);
    CHECK_EQ(pos.pattern, 11);
    CHECK_EQ(pos.row, 60);

    pos = pattern_display_position_at_offset(orders, lengths, 1, 1, 11, 0, 64);
    CHECK_EQ(pos.pattern, 12);
    CHECK_EQ(pos.row, 0);

    pos = pattern_display_position_at_offset(orders, lengths, 1, 1, 11, 0, 128);
    CHECK_EQ(pos.pattern, 10);
    CHECK_EQ(pos.row, 0);

    if (failures) {
        std::fprintf(stderr, "%d pattern display layout check(s) failed\n", failures);
        return 1;
    }
    std::puts("pattern display layout checks passed");
    return 0;
}
