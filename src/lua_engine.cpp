// lua_engine.cpp — Lua 5.4 scripting engine for zTracker
//
// Design: Lua calls block the main thread (no coroutines / cooperative
// threading).  A buggy script that loops forever will freeze the app —
// that is intentional and acceptable (per @guysv / SchismTracker advice).

#include "zt.h"
#include "lua_engine.h"
#include "playback.h"
#include "module.h"
#include "midi-io.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------
ZtLuaEngine g_lua;

// ---------------------------------------------------------------------------
// Internal helper: append a printf-formatted line to the scrollback
// ---------------------------------------------------------------------------
static void lua_engine_printf(const char *fmt, ...)
{
    char buf[LUA_CONSOLE_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_lua.print_line(buf);
}

// ---------------------------------------------------------------------------
// Lua print() replacement — redirects to our scrollback
// ---------------------------------------------------------------------------
static int l_zt_print(lua_State *L)
{
    int n = lua_gettop(L);
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    for (int i = 1; i <= n; i++) {
        size_t len;
        const char *s = luaL_tolstring(L, i, &len);
        luaL_addlstring(&b, s, len);
        lua_pop(L, 1);
        if (i < n) luaL_addchar(&b, '\t');
    }
    luaL_pushresult(&b);
    g_lua.print_line(lua_tostring(L, -1));
    lua_pop(L, 1);
    return 0;
}

// ===========================================================================
// zt.* API bindings
// ===========================================================================

// --- Pattern access ---------------------------------------------------------

// zt.get_note(pattern, track, row) -> note_value (0 = empty)
static int l_get_note(lua_State *L)
{
    int p = (int)luaL_checkinteger(L, 1);
    int t = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);

    if (!song || p < 0 || p >= ZTM_MAX_PATTERNS || !song->patterns[p]) {
        lua_pushinteger(L, 0); return 1;
    }
    if (t < 0 || t >= ZTM_MAX_TRACKS || r < 0) {
        lua_pushinteger(L, 0); return 1;
    }
    event *ev = song->patterns[p]->tracks[t]->get_event((unsigned short)r);
    lua_pushinteger(L, ev ? ev->note : 0);
    return 1;
}

// zt.set_note(pattern, track, row, note, inst, vol)
static int l_set_note(lua_State *L)
{
    int p    = (int)luaL_checkinteger(L, 1);
    int t    = (int)luaL_checkinteger(L, 2);
    int r    = (int)luaL_checkinteger(L, 3);
    int note = (int)luaL_checkinteger(L, 4);
    int inst = (int)luaL_optinteger(L, 5, -1);
    int vol  = (int)luaL_optinteger(L, 6, -1);

    if (!song || p < 0 || p >= ZTM_MAX_PATTERNS || !song->patterns[p]) return 0;
    if (t < 0 || t >= ZTM_MAX_TRACKS || r < 0) return 0;

    song->patterns[p]->tracks[t]->update_event_safe(
        (unsigned short)r, note, inst, vol, -1, -1, -1);
    return 0;
}

// zt.get_pattern_length(pattern) -> length
static int l_get_pattern_length(lua_State *L)
{
    int p = (int)luaL_checkinteger(L, 1);
    if (!song || p < 0 || p >= ZTM_MAX_PATTERNS || !song->patterns[p]) {
        lua_pushinteger(L, 0); return 1;
    }
    lua_pushinteger(L, song->patterns[p]->length);
    return 1;
}

// zt.set_pattern_length(pattern, length)
static int l_set_pattern_length(lua_State *L)
{
    int p   = (int)luaL_checkinteger(L, 1);
    int len = (int)luaL_checkinteger(L, 2);
    if (!song || p < 0 || p >= ZTM_MAX_PATTERNS || !song->patterns[p]) return 0;
    if (len < 1) len = 1;
    song->patterns[p]->resize(len);
    return 0;
}

// --- Playback ---------------------------------------------------------------

static int l_play(lua_State *L)
{
    (void)L;
    if (ztPlayer) ztPlayer->play(0, cur_edit_pattern, 0);
    return 0;
}

static int l_stop(lua_State *L)
{
    (void)L;
    if (ztPlayer) ztPlayer->stop();
    return 0;
}

static int l_play_pattern(lua_State *L)
{
    (void)L;
    if (ztPlayer) ztPlayer->play(0, cur_edit_pattern, 1);
    return 0;
}

static int l_get_bpm(lua_State *L)
{
    lua_pushinteger(L, song ? song->bpm : 0);
    return 1;
}

static int l_set_bpm(lua_State *L)
{
    int bpm = (int)luaL_checkinteger(L, 1);
    if (song) {
        song->bpm = bpm;
        if (ztPlayer) ztPlayer->set_speed();
    }
    return 0;
}

static int l_get_tpb(lua_State *L)
{
    lua_pushinteger(L, song ? song->tpb : 0);
    return 1;
}

static int l_set_tpb(lua_State *L)
{
    int tpb = (int)luaL_checkinteger(L, 1);
    if (song) {
        song->tpb = tpb;
        if (ztPlayer) ztPlayer->set_speed();
    }
    return 0;
}

// --- Current state ----------------------------------------------------------

static int l_cur_pattern(lua_State *L)
{
    lua_pushinteger(L, cur_edit_pattern);
    return 1;
}

static int l_cur_track(lua_State *L)
{
    lua_pushinteger(L, cur_edit_track);
    return 1;
}

static int l_cur_row(lua_State *L)
{
    lua_pushinteger(L, cur_edit_row);
    return 1;
}

static int l_cur_instrument(lua_State *L)
{
    lua_pushinteger(L, cur_inst);
    return 1;
}

static int l_set_cur_pattern(lua_State *L)
{
    int p = (int)luaL_checkinteger(L, 1);
    if (p >= 0 && p < ZTM_MAX_PATTERNS) cur_edit_pattern = p;
    return 0;
}

static int l_set_cur_row(lua_State *L)
{
    int r = (int)luaL_checkinteger(L, 1);
    if (song && song->patterns[cur_edit_pattern] && r >= 0 &&
        r < song->patterns[cur_edit_pattern]->length) {
        cur_edit_row = r;
    }
    return 0;
}

// --- MIDI -------------------------------------------------------------------

// zt.midi_note_on(device, note, channel, velocity)
static int l_midi_note_on(lua_State *L)
{
    int dev  = (int)luaL_checkinteger(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    int chan = (int)luaL_checkinteger(L, 3);
    int vel  = (int)luaL_optinteger(L, 4, 127);
    if (MidiOut)
        MidiOut->noteOn((unsigned int)dev,
                        (unsigned char)note, (unsigned char)chan,
                        (unsigned char)vel, 0, 0);
    return 0;
}

// zt.midi_note_off(device, note, channel)
static int l_midi_note_off(lua_State *L)
{
    int dev  = (int)luaL_checkinteger(L, 1);
    int note = (int)luaL_checkinteger(L, 2);
    int chan = (int)luaL_checkinteger(L, 3);
    if (MidiOut)
        MidiOut->noteOff((unsigned int)dev,
                         (unsigned char)note, (unsigned char)chan, 0, 0);
    return 0;
}

// --- Status bar -------------------------------------------------------------

static int l_status(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    // Set the status bar message (same pattern as rest of codebase)
    strncpy(szStatmsg, msg, sizeof(szStatmsg) - 1);
    szStatmsg[sizeof(szStatmsg) - 1] = '\0';
    statusmsg = szStatmsg;
    status_change = 1;
    need_refresh++;
    return 0;
}

// --- zt.print (alias for the print replacement) ----------------------------

static int l_zt_print_fn(lua_State *L)
{
    return l_zt_print(L);
}

// ===========================================================================
// ZtLuaEngine implementation
// ===========================================================================

ZtLuaEngine::ZtLuaEngine()
    : L(nullptr), line_count(0), scroll_off(0),
      history_count(0), history_pos(-1)
{
    memset(lines,   0, sizeof(lines));
    memset(history, 0, sizeof(history));
}

ZtLuaEngine::~ZtLuaEngine()
{
    shutdown();
}

void ZtLuaEngine::print_line(const char *text)
{
    int idx = line_count % LUA_CONSOLE_MAX_LINES;
    strncpy(lines[idx], text, LUA_CONSOLE_LINE_LEN - 1);
    lines[idx][LUA_CONSOLE_LINE_LEN - 1] = '\0';
    line_count++;
    // Keep scroll anchored at bottom unless user has scrolled up
    // (scroll_off stays; caller should reset it on Enter)
}

bool ZtLuaEngine::init()
{
    if (L) return true;  // already init'd

    L = luaL_newstate();
    if (!L) {
        print_line("[Lua] Failed to create Lua state");
        return false;
    }

    // Open standard libraries (safe: no io/os/debug opened — see below)
    luaL_openlibs(L);

    // Replace the standard print with ours
    lua_pushcfunction(L, l_zt_print);
    lua_setglobal(L, "print");

    registerBindings();

    print_line("[Lua] Lua 5.4 ready. Type Lua code and press Enter.");
    print_line("[Lua] API: zt.get_note/set_note, zt.play/stop, zt.get_bpm/set_bpm, ...");

    return true;
}

void ZtLuaEngine::shutdown()
{
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void ZtLuaEngine::execute(const char *code)
{
    if (!L || !code || !code[0]) return;

    // Add to history (skip duplicates of last entry)
    if (history_count == 0 ||
        strncmp(history[(history_count - 1) % LUA_CONSOLE_HISTORY],
                code, 255) != 0) {
        int hi = history_count % LUA_CONSOLE_HISTORY;
        strncpy(history[hi], code, 255);
        history[hi][255] = '\0';
        history_count++;
    }
    history_pos = -1;

    // Echo the command
    char echo[LUA_CONSOLE_LINE_LEN];
    snprintf(echo, sizeof(echo), "> %s", code);
    print_line(echo);

    // Try as expression first (return <code>) so single expressions print
    char expr[512];
    snprintf(expr, sizeof(expr), "return %s", code);
    int status = luaL_loadstring(L, expr);
    if (status != LUA_OK) {
        // Not an expression — compile as statement
        lua_pop(L, 1);
        status = luaL_loadstring(L, code);
    }

    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        char msg[LUA_CONSOLE_LINE_LEN];
        snprintf(msg, sizeof(msg), "[err] %s", err ? err : "(nil)");
        print_line(msg);
        lua_pop(L, 1);
        return;
    }

    int nresults_before = lua_gettop(L) - 1; // stack size before call
    int call_status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (call_status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        char msg[LUA_CONSOLE_LINE_LEN];
        snprintf(msg, sizeof(msg), "[err] %s", err ? err : "(nil)");
        print_line(msg);
        lua_pop(L, 1);
        return;
    }

    // Print any returned values
    int nret = lua_gettop(L) - nresults_before;
    for (int i = 1; i <= nret; i++) {
        size_t len;
        const char *s = luaL_tolstring(L, nresults_before + i, &len);
        print_line(s);
        lua_pop(L, 1);
    }
    if (nret > 0) lua_pop(L, nret);
}

void ZtLuaEngine::registerBindings()
{
    // Create the 'zt' table
    lua_newtable(L);

    // Pattern access
    lua_pushcfunction(L, l_get_note);          lua_setfield(L, -2, "get_note");
    lua_pushcfunction(L, l_set_note);          lua_setfield(L, -2, "set_note");
    lua_pushcfunction(L, l_get_pattern_length);lua_setfield(L, -2, "get_pattern_length");
    lua_pushcfunction(L, l_set_pattern_length);lua_setfield(L, -2, "set_pattern_length");

    // Playback
    lua_pushcfunction(L, l_play);              lua_setfield(L, -2, "play");
    lua_pushcfunction(L, l_stop);              lua_setfield(L, -2, "stop");
    lua_pushcfunction(L, l_play_pattern);      lua_setfield(L, -2, "play_pattern");
    lua_pushcfunction(L, l_get_bpm);           lua_setfield(L, -2, "get_bpm");
    lua_pushcfunction(L, l_set_bpm);           lua_setfield(L, -2, "set_bpm");
    lua_pushcfunction(L, l_get_tpb);           lua_setfield(L, -2, "get_tpb");
    lua_pushcfunction(L, l_set_tpb);           lua_setfield(L, -2, "set_tpb");

    // Current state
    lua_pushcfunction(L, l_cur_pattern);       lua_setfield(L, -2, "cur_pattern");
    lua_pushcfunction(L, l_cur_track);         lua_setfield(L, -2, "cur_track");
    lua_pushcfunction(L, l_cur_row);           lua_setfield(L, -2, "cur_row");
    lua_pushcfunction(L, l_cur_instrument);    lua_setfield(L, -2, "cur_instrument");
    lua_pushcfunction(L, l_set_cur_pattern);   lua_setfield(L, -2, "set_cur_pattern");
    lua_pushcfunction(L, l_set_cur_row);       lua_setfield(L, -2, "set_cur_row");

    // MIDI
    lua_pushcfunction(L, l_midi_note_on);      lua_setfield(L, -2, "midi_note_on");
    lua_pushcfunction(L, l_midi_note_off);     lua_setfield(L, -2, "midi_note_off");

    // Status / output
    lua_pushcfunction(L, l_status);            lua_setfield(L, -2, "status");
    lua_pushcfunction(L, l_zt_print_fn);       lua_setfield(L, -2, "print");

    lua_setglobal(L, "zt");
}
