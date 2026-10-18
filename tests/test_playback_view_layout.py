#!/usr/bin/env python3
"""Check the production playback layout at different widths and track positions."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import function

fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
// Guard against Windows' function-like min/max macros too.
#define min(a, b) windows_min_macro_must_not_expand(a, b)
#define max(a, b) windows_max_macro_must_not_expand(a, b)
constexpr int MAX_TRACKS = 64, FONT_SIZE_X = 8;
constexpr int ZTM_TRACKNAME_MAXLEN = 16;
using TColor = unsigned int;
struct Drawable {};
struct {
    char track_name[MAX_TRACKS][ZTM_TRACKNAME_MAXLEN] = {};
    bool track_mute[MAX_TRACKS] = {};
    TColor track_color[MAX_TRACKS] = {};
} module, *song = &module;
struct { TColor Text = 1, Background = 2, EditText = 3, Lowlight = 4,
                Brighttext = 5, Black = 6; } COLORS;
int col(int v) { return v; }
int row(int v) { return v; }
TColor zt_track_shade(TColor c, int) { return c; }
TColor zt_track_text_for_bg(TColor) { return COLORS.Text; }
struct Header { int x, y; std::string text; TColor bg; };
std::vector<Header> headers;
void printBG(int x, int y, const char *text, TColor, TColor bg, Drawable *) {
    headers.push_back({x, y, text, bg});
}
int INTERNAL_RESOLUTION_X = 840;
struct Frame { int x, y, xsize, ysize; };
struct PatternDisplay {
    Frame storage, *frame = &storage;
    int x = 1, y = 12, ysize = 40;
    int cur_pat_view = 1, tracksize = 0, tracks = 0;
    int cur_track = 0, starttrack = 0;
    void update_frame();
    void disp_track_headers(Drawable *);
};
'''
fixture += function('src/PatternDisplay.cpp', 'void PatternDisplay::update_frame(')
fixture += function('src/PatternDisplay.cpp', 'void PatternDisplay::disp_track_headers(')
fixture += r'''
int main() {
    PatternDisplay view;
    view.cur_pat_view = 3;
    view.update_frame();
    assert(view.tracks == 4 && view.tracksize == 20);
    view.cur_pat_view = 2;
    view.update_frame();
    assert(view.tracks == 7 && view.tracksize == 14);
    for (int width : {840, 848, 1024, 1920, 4096, 16384}) {
        INTERNAL_RESOLUTION_X = width;
        for (int selected = -1; selected <= MAX_TRACKS; ++selected) {
            for (int start : {-1, 0, 30, 63, 64}) {
                view.cur_track = selected;
                view.starttrack = start;
                // Repeatedly shrink and expand around the same selection.
                for (int mode : {1, 2, 3, 1, 3, 2}) {
                    view.cur_pat_view = mode;
                    view.update_frame();
                    assert(view.tracks >= 1 && view.tracks <= MAX_TRACKS);
                    assert(view.starttrack >= 0);
                    assert(view.starttrack + view.tracks <= MAX_TRACKS);
                    assert(view.cur_track >= view.starttrack);
                    assert(view.cur_track < view.starttrack + view.tracks);
                    assert(view.frame->xsize == view.tracks * view.tracksize);
                    assert((view.frame->x + view.frame->xsize + 1) * FONT_SIZE_X <= width);
                }
                // Resizing while scrolled to the right also keeps selection.
                INTERNAL_RESOLUTION_X = 840;
                view.update_frame();
                assert(view.cur_track >= view.starttrack);
                assert(view.cur_track < view.starttrack + view.tracks);
                INTERNAL_RESOLUTION_X = width;
            }
        }
    }
    // Custom labels use the whole available header width without crossing
    // the separator; unnamed tracks retain their numeric fallback.
    std::strcpy(song->track_name[0], "ABCDEFGHIJKLMNO");
    std::strcpy(song->track_name[1], "Drums");
    INTERNAL_RESOLUTION_X = 840;
    view.cur_track = view.starttrack = 0;
    for (int mode : {1, 2, 3}) {
        view.cur_pat_view = mode;
        view.update_frame();
        headers.clear();
        view.disp_track_headers(nullptr);
        assert(headers.size() == (size_t)view.tracks * 2);
        const char *label = mode == 1 ? "ABCDEFG" :
                            mode == 2 ? "ABCDEFGHIJKLM" : "  ABCDEFGHIJKLMNO  ";
        assert(headers[0].text == label);
        assert(headers[2].text == (mode == 1 ? " Drums " :
                                  mode == 2 ? "    Drums    " : "       Drums       "));
        assert(headers[4].text == (mode == 1 ? "Trk 03 " :
                                  mode == 2 ? "  Track 03   " : "     Track 03      "));
        for (int t = 0; t < view.tracks; ++t) {
            assert(headers[t * 2].text.size() == (size_t)view.tracksize - 1);
            assert(headers[t * 2 + 1].text == " ");
            assert(headers[t * 2 + 1].bg == COLORS.Background);
            assert(headers[t * 2 + 1].x == headers[t * 2].x + view.tracksize - 1);
        }
    }
    std::puts("Playback layout: widths, navigation, resize, custom names and header gaps passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='zt-playback-layout-') as tmp:
    source = Path(tmp) / 'layout.cpp'
    binary = Path(tmp) / 'layout-test'
    source.write_text(fixture)
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                   ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
