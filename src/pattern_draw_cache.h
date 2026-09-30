#ifndef ZT_PATTERN_DRAW_CACHE_H
#define ZT_PATTERN_DRAW_CACHE_H

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

struct PatternCellPaint {
    uint64_t contents = 0;
    uint32_t foreground = 0, background = 0, separator = 0;
    uint32_t caret_foreground = 0, caret_background = 0;
    int caret = -1;

    bool operator==(const PatternCellPaint &other) const {
        return contents == other.contents && foreground == other.foreground &&
               background == other.background && separator == other.separator &&
               caret_foreground == other.caret_foreground &&
               caret_background == other.caret_background && caret == other.caret;
    }
};

struct PatternDrawLayout {
    const void *pixels;
    int width, height, pitch, pattern, first_row, first_track;
    int tracks, rows, field_size, view, origin_x, origin_y, pattern_length;

    bool operator==(const PatternDrawLayout &other) const {
        return pixels == other.pixels && width == other.width && height == other.height &&
               pitch == other.pitch && pattern == other.pattern && first_row == other.first_row &&
               first_track == other.first_track && tracks == other.tracks && rows == other.rows &&
               field_size == other.field_size && view == other.view &&
               origin_x == other.origin_x && origin_y == other.origin_y &&
               pattern_length == other.pattern_length;
    }
};

// A record of what was painted, not an event index. The caller still reads
// current event values on each refresh, so edits, undo, and Lua writes cannot
// leave a stale cached cell. Storage grows only when the viewport changes.
class PatternDrawCache {
public:
    void invalidate() { valid_ = false; }

    void begin(const PatternDrawLayout &layout, const unsigned char *font,
               bool force_full) {
        repaint_all_ = force_full || !valid_ || !(layout_ == layout) ||
                       std::memcmp(font_.data(), font, font_.size()) != 0;
        layout_ = layout;
        cells_.resize(static_cast<size_t>(layout.rows) * layout.tracks);
        if (repaint_all_) std::memcpy(font_.data(), font, font_.size());
        valid_ = true;
        painted_cells = 0;
    }

    bool needs_paint(int row, int track, const PatternCellPaint &paint) {
        auto &old = cells_[static_cast<size_t>(row) * layout_.tracks + track];
        if (!repaint_all_ && old == paint) return false;
        old = paint;
        ++painted_cells;
        return true;
    }

    // Useful to verify that incremental drawing actually avoids rasterization.
    size_t painted_cells = 0;

private:
    bool valid_ = false, repaint_all_ = true;
    PatternDrawLayout layout_{};
    std::array<unsigned char, 256 * 8> font_{};
    std::vector<PatternCellPaint> cells_;
};

#endif
