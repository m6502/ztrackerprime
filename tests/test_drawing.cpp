#include "zt.h"
#include "pattern_draw_cache.h"
#include "ccizer.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <random>
#include <vector>

// Real type layouts, minimal lifecycle fixtures: no threads/devices/file I/O.
ZTConf::ZTConf() {}
ZTConf::~ZTConf() {}
zt_module::zt_module(int, int) {}
zt_module::~zt_module() { for (auto *p : patterns) delete p; }
player::player(int, int, zt_module *s) : patt_memory(64) { song = s; }
player::~player() {}
Skin::Skin() {}
Skin::~Skin() {}
ZTConf zt_config_globals;
static zt_module module(4, 120);
zt_module *song = &module;
static player transport(1, 8, song);
player *ztPlayer = &transport;
static Skin skin;
Skin *CurrentSkin = &skin;
event blank_event;
unsigned char font[256 * 8];
int cur_edit_row, cur_edit_row_disp, cur_edit_pattern, cur_edit_track, cur_edit_col, cur_edit_track_disp;
int select_row_start, select_row_end, select_track_start, select_track_end, selected;
int g_posx_tracks, max_displayable_rows, PATTERN_EDIT_ROWS = 64;
int g_cc_drawmode[MAX_TRACKS];
edit_col edit_cols[41];
const char *col_desc[41];
CUI_Patterneditor *UIP_Patterneditor = nullptr;
ZtCcizerFile *zt_ccizer_current_file() { return nullptr; }
PatternDrawCache g_pattern_draw_cache;

char *printNote(char *, event *, int);
char *playback_note(char *, event *, int);
char *reference_printNote(char *, event *, int);
char *reference_playback_note(char *, event *, int);
void reference_printchar(int, int, unsigned char, TColor, Drawable *);

static void require(bool condition, const char *message) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

static void check_text(std::mt19937 &rng) {
    event e;
    char a[64], b[64];
    for (int n = 0; n < 50000; ++n) {
        if (n) {
            e.note = rng() % 256; e.inst = rng() % 256; e.vol = rng() % 256;
            e.effect = rng() % 256; e.effect_data = rng() % 65536;
            e.length = n < 1100 ? n - 1 : rng() % 65536;
        }
        for (int view = 0; view < 4; ++view) {
            printNote(a, &e, view); reference_printNote(b, &e, view);
            require(!strcmp(a, b), "editor formatter changed output");
            playback_note(a, &e, view); reference_playback_note(b, &e, view);
            require(!strcmp(a, b), "playback formatter changed output");
            playback_note(a, nullptr, view); reference_playback_note(b, nullptr, view);
            require(!strcmp(a, b), "null playback event changed output");
        }
    }
    puts("Text: 600,000 editor/playback/null comparisons match legacy output.");
}

static void check_glyphs() {
    Drawable a(48, 40), b(48, 40);
    const size_t bytes = a.surface->pitch * a.surface->h;
    for (int ch = 0; ch < 256; ++ch) {
        for (int edge = 0; edge < 5; ++edge) {
            memset(a.surface->pixels, 0x12, bytes);
            memset(b.surface->pixels, 0x12, bytes);
            int x = edge == 1 ? 44 : edge == 3 ? -3 : 5;
            int y = edge == 2 ? 37 : edge == 4 ? -3 : 5;
            printchar(x, y, ch, 0xffddaa33, &a);
            if (x >= 0 && y >= 0) reference_printchar(x, y, ch, 0xffddaa33, &b);
            else {
                // The legacy routine did not safely clip negative origins.
                for (int i = 0; i < 8; ++i) for (int j = 0; j < 8; ++j)
                    if (x+j >= 0 && y+i >= 0 && x+j < b.width && y+i < b.height &&
                        (font[ch * 8 + i] & (128 >> j)))
                        b.getLine(y+i)[x+j] = 0xffddaa33;
            }
            require(!memcmp(a.surface->pixels, b.surface->pixels, bytes), "glyph pixels differ");
        }
    }
    puts("Glyphs: all 256 glyphs match, including each clipping edge (1,280 cases).");
}

static int field_size = 19, tracks_shown = 8;
static void render(Drawable &s) { disp_pattern(tracks_shown, field_size, 14, &s); }

static size_t compare_frame(Drawable &surface, const char *name, int expected = -1) {
    const size_t bytes = surface.surface->pitch * surface.surface->h;
    std::vector<unsigned char> before(bytes), incremental(bytes);
    memcpy(before.data(), surface.surface->pixels, bytes);
    render(surface);
    const size_t painted = g_pattern_draw_cache.painted_cells;
    memcpy(incremental.data(), surface.surface->pixels, bytes);
    auto saved_cache = g_pattern_draw_cache;
    memcpy(surface.surface->pixels, before.data(), bytes);
    g_pattern_draw_cache.invalidate();
    render(surface);
    if (memcmp(surface.surface->pixels, incremental.data(), bytes)) {
        fprintf(stderr, "Incremental/full pixel mismatch: %s\n", name); exit(1);
    }
    if (expected >= 0 && painted != static_cast<size_t>(expected)) {
        fprintf(stderr, "%s: painted %zu cells, expected %d\n", name, painted, expected); exit(1);
    }
    g_pattern_draw_cache = std::move(saved_cache);
    return painted;
}

static void check_grid(Drawable &s, std::mt19937 &rng) {
    compare_frame(s, "initial", 512);
    compare_frame(s, "unchanged", 0);
    cur_edit_col = 1;
    compare_frame(s, "caret column", 1);
    cur_edit_track = 1;
    compare_frame(s, "caret track", 2);
    cur_edit_row = 1;
    compare_frame(s, "cursor row", 16);
    song->patterns[0]->tracks[2]->get_event(2)->vol ^= 1;
    compare_frame(s, "edit same event allocation", 1);
    event **link = &song->patterns[0]->tracks[2]->event_list;
    while ((*link)->row != 2) link = &(*link)->next_event;
    event saved = **link;
    event *old = *link;
    *link = new event;
    **link = saved;
    delete old;
    compare_frame(s, "replace event with identical values", 0);
    old = *link; *link = old->next_event; delete old;
    compare_frame(s, "delete event", 1);
    auto *restored = new event;
    *restored = saved; restored->next_event = *link; *link = restored;
    compare_frame(s, "restore event", 1);
    song->track_color[3] = 0xff2266cc;
    compare_frame(s, "track color", 64);
    selected = 1; select_row_start = 3; select_row_end = 6;
    select_track_start = 1; select_track_end = 2;
    compare_frame(s, "selection", 8);
    select_row_end = 7;
    compare_frame(s, "extend selection", 2);
    selected = 0;
    compare_frame(s, "clear selection", 10);
    ztPlayer->playing = 1; ztPlayer->playing_cur_row = 5;
    compare_frame(s, "playhead gutter only", 0);
    ztPlayer->playing_cur_row = 6;
    compare_frame(s, "move playhead without follow", 0);
    ztPlayer->playing = 0;
    compare_frame(s, "stop", 0);
    zt_config_globals.lowlight_increment = 3;
    compare_frame(s, "beat highlight settings");
    font['C' * 8] ^= 1;
    compare_frame(s, "font changed", 512);
    COLORS.Lowlight ^= 0x404040;
    compare_frame(s, "separator palette changed", 512);
    cur_edit_row_disp = 8;
    compare_frame(s, "vertical scroll", 512);
    cur_edit_track_disp = 8;
    compare_frame(s, "horizontal scroll", 512);
    song->patterns[0]->length = 32; cur_edit_row_disp = 0;
    compare_frame(s, "short pattern/trailing blank rows", 512);
    compare_frame(s, "unchanged trailing blanks", 0);
    COLORS.Background ^= 0x101010;
    compare_frame(s, "blank-row palette changed", 256);
    song->patterns[0]->length = 128;
    compare_frame(s, "grow pattern", 512);
    // Simulate the clear/popup/drawbar invalidation contract at the page boundary.
    s.fillRect(80, 160, 400, 300, 0xff55ff55);
    g_pattern_draw_cache.invalidate();
    compare_frame(s, "overwritten framebuffer then full invalidation", 512);
    cur_edit_pattern = 1;
    compare_frame(s, "pattern switch", 512);
    cur_edit_pattern = 0;
    compare_frame(s, "return to pattern", 512);
    for (int mode = 0; mode < 4; ++mode) {
        const int sizes[] = {6, 13, 12, 19};
        zt_config_globals.cur_edit_mode = mode; field_size = sizes[mode];
        cur_edit_col = 0;
        compare_frame(s, "view mode", 512);
        compare_frame(s, "unchanged view mode", 0);
    }
    // Random edits/selection/cursor movement exercise their interactions.
    cur_edit_track_disp = 0;
    for (int i = 0; i < 300; ++i) {
        cur_edit_row = rng() % 64; cur_edit_track = rng() % 8;
        cur_edit_col = rng() % 14;
        selected = rng() % 2; select_row_start = rng() % 32;
        select_row_end = select_row_start + rng() % 32;
        select_track_start = rng() % 4; select_track_end = 4 + rng() % 4;
        event *e = song->patterns[0]->tracks[rng() % 8]->get_event(rng() % 128);
        e->note = rng() % 128; e->vol = rng() % 256; e->effect_data = rng() % 65536;
        compare_frame(s, "randomized edit/selection/caret");
    }
    puts("Grid: incremental frames match full redraws; unchanged=0, caret=1/2, cursor row=16 of 512 cells.");
}

template<class F> static void benchmark(const char *name, F work) {
    using clock = std::chrono::steady_clock;
    std::vector<double> times;
    for (int sample = 0; sample < 7; ++sample) {
        auto start = clock::now();
        for (int i = 0; i < 500; ++i) work();
        times.push_back(std::chrono::duration<double, std::micro>(clock::now() - start).count() / 500);
    }
    std::sort(times.begin(), times.end());
    printf("%-32s %.2f [%.2f, %.2f] us/grid\n", name, times[3], times.front(), times.back());
}

int main(int argc, char **argv) {
    require(argc >= 2, "font path required");
    FILE *f = fopen(argv[1], "rb");
    require(f && fread(font, 1, sizeof(font), f) == sizeof(font), "font load");
    fclose(f);
    std::mt19937 rng(20260926);
    check_text(rng); check_glyphs();
    zt_config_globals.zoom = 1;
    zt_config_globals.screen_width = 1344; zt_config_globals.screen_height = 680;
    zt_config_globals.cur_edit_mode = VIEW_BIG;
    zt_config_globals.highlight_increment = 16; zt_config_globals.lowlight_increment = 4;
    init_edit_cols();
    for (int p = 0; p < 2; ++p) {
        song->patterns[p] = new pattern(128);
        for (int t = 0; t < MAX_TRACKS; ++t) for (int r = 0; r < 128; ++r) {
            auto *e = new event;
            e->row = r; e->note = 36 + r % 60; e->inst = t; e->vol = 100; e->length = 4;
            e->next_event = song->patterns[p]->tracks[t]->event_list;
            song->patterns[p]->tracks[t]->event_list = e;
        }
    }
    Drawable s(1344, 680);
    s.fillRect(0, 0, s.width-1, s.height-1, COLORS.Background);
    check_grid(s, rng);
    Drawable resized(1504, 720);
    zt_config_globals.screen_width = 1504; zt_config_globals.screen_height = 720;
    resized.fillRect(0, 0, resized.width-1, resized.height-1, COLORS.Background);
    compare_frame(resized, "resize/new surface", 512);
    compare_frame(resized, "unchanged resized surface", 0);
    PATTERN_EDIT_ROWS = 68;
    compare_frame(resized, "more visible rows", 544);
    PATTERN_EDIT_ROWS = 64;
    compare_frame(resized, "fewer visible rows", 512);
    if (argc > 2) {
        selected = 0; cur_edit_row = 20; cur_edit_track = 1; cur_edit_col = 0;
        render(resized);
        benchmark("Full grid (optimized text)", [&] { g_pattern_draw_cache.invalidate(); render(resized); });
        benchmark("Unchanged grid", [&] { render(resized); });
        benchmark("Caret moves between tracks", [&] { cur_edit_track ^= 1; render(resized); });
        benchmark("Cursor moves between rows", [&] { cur_edit_row ^= 1; render(resized); });
    }
}
