// Minimal stand-in for zt.h, used when src/ableton_link.cpp is compiled into
// the unit test (ZT_TEST_NO_SDL). Mirrors exactly the declarations the Link
// integration uses -- a recording fake player, a song with bpm + orderlist,
// the three ableton_link_* conf fields, and the page-refresh globals -- so
// the test can drive the full pump/defer logic without SDL or the real app.
// Same pattern as tests/sdl_stub.h (see CLAUDE.md "Test harness").
//
// Definitions (not just declarations) live here: the test is a single
// translation unit that includes ableton_link.cpp, which includes this.
#ifndef ZT_LINK_STUB_H
#define ZT_LINK_STUB_H

struct zt_module {
    int bpm = 120;
    int orderlist[256] = {0};   // 0x100 = Break (end of song)
};

static zt_module  g_test_song;
static zt_module *song = &g_test_song;

struct player {
    int playing    = 0;
    int stop_count = 0;

    // Phase-chase surface (mirrors src/playback.h). 120 BPM nominal:
    // 60e6/(96*120) = 5208 us -> ms part 5000, mms remainder 208.
    int bpm             = 120;
    int subtick_len_ms  = 5000;
    int subtick_len_mms = 208;
    int played_subticks = 0;
    int chase_skew_us   = 0;

    // Recording fakes: the test asserts on these.
    int play_immediately_calls = 0;
    int last_row = -1, last_pat = -1, last_pm = -1;
    int stop_calls      = 0;
    int set_speed_calls = 0;

    void play_immediately(int row, int pattern, int pm, int loopmode = 1) {
        (void)loopmode;
        play_immediately_calls++;
        last_row = row; last_pat = pattern; last_pm = pm;
        playing = 1;
        bpm = song->bpm;        // mirrors prepare_play() adopting the song tempo
        played_subticks = 0;    // mirrors prepare_play() resetting position
    }
    void stop(void) {
        stop_count++;
        stop_calls++;
        playing = 0;
    }
    // Mirrors the real set_speed(): adopt the song tempo as the engine's
    // nominal rate (the chase's rate-agreement guard reads it).
    void set_speed(void) { set_speed_calls++; bpm = song->bpm; }
};

struct zt_conf {
    int ableton_link_enable          = 0;
    int ableton_link_start_stop_sync = 0;
    int ableton_link_quantum         = 4;
    int ableton_link_offset_ms       = 0;
};

static player    g_test_player;
static player    *ztPlayer = &g_test_player;
static zt_conf    zt_config_globals;

static const char *statusmsg          = nullptr;
static int         status_change      = 0;
static int         need_refresh       = 0;
static int         cur_edit_pattern   = 0;
static int         zt_song_generation = 0;

#endif /* ZT_LINK_STUB_H */
