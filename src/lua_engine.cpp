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
    print_line("[Lua] Tab completes; type 'zt.' and press Tab to list the API.");

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

// ---------------------------------------------------------------------------
// API listing + tab completion
// ---------------------------------------------------------------------------

// Lua identifier char? (incl. digits after first pos; we treat '.' specially)
static bool is_ident_char(char c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
            c == '_';
}

// Push the table whose dotted path is `path` (may be empty, meaning _G) onto
// the Lua stack. Returns true if the path resolved to a table; otherwise the
// stack is unchanged and false is returned.
static bool push_table_by_path(lua_State *L, const char *path)
{
    if (!path || !path[0]) {
        lua_pushglobaltable(L);
        return true;
    }
    lua_pushglobaltable(L);
    const char *p = path;
    while (*p) {
        const char *dot = strchr(p, '.');
        size_t n = dot ? (size_t)(dot - p) : strlen(p);
        lua_pushlstring(L, p, n);
        lua_gettable(L, -2);
        lua_remove(L, -2);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return false;
        }
        if (!dot) break;
        p = dot + 1;
    }
    return true;
}

// Collect string keys in the table on top of the stack that start with
// `prefix`. Table is left on the stack. `out` is a caller-owned buffer of
// capacity `cap`; returns number of candidates written (may be less than
// cap if truncated).
static int collect_keys(lua_State *L, const char *prefix,
                        char out[][64], int cap)
{
    int n = 0;
    size_t plen = strlen(prefix);
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char *k = lua_tostring(L, -2);
            if (strncmp(k, prefix, plen) == 0) {
                if (n < cap) {
                    strncpy(out[n], k, 63);
                    out[n][63] = '\0';
                    n++;
                }
            }
        }
        lua_pop(L, 1); // value
    }
    return n;
}

static int qsort_strcmp(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

void ZtLuaEngine::print_api_list()
{
    if (!L) return;

    // Print the zt.* table contents first.
    lua_getglobal(L, "zt");
    if (lua_istable(L, -1)) {
        static char keys[64][64];
        int n = collect_keys(L, "", keys, 64);
        qsort(keys, (size_t)n, 64, qsort_strcmp);

        print_line("[Lua] zt.* API:");
        char buf[LUA_CONSOLE_LINE_LEN];
        buf[0] = ' '; buf[1] = ' '; buf[2] = '\0';
        int col = 2;
        for (int i = 0; i < n; i++) {
            int klen = (int)strlen(keys[i]);
            if (col > 2 && col + klen + 2 > 76) {
                print_line(buf);
                buf[0] = ' '; buf[1] = ' '; buf[2] = '\0';
                col = 2;
            }
            strcat(buf, "zt.");
            strcat(buf, keys[i]);
            strcat(buf, "  ");
            col += klen + 5;
        }
        if (col > 2) print_line(buf);
    }
    lua_pop(L, 1);

    print_line("[Lua] Globals (selected): print, pairs, ipairs, math, string, table");
    print_line("[Lua] Tab to complete any identifier. Esc/F2 to close.");
}

bool ZtLuaEngine::tab_complete(char *input, int cap, int *pcursor)
{
    if (!L || !input || !pcursor) return false;

    int cursor = *pcursor;
    int len = (int)strlen(input);
    if (cursor < 0) cursor = 0;
    if (cursor > len) cursor = len;

    // Walk backward from cursor over identifier/dot characters.
    int start = cursor;
    while (start > 0 && (is_ident_char(input[start - 1]) || input[start - 1] == '.'))
        start--;

    // Portion to complete is input[start..cursor-1].
    int span = cursor - start;
    char word[128];
    if (span > (int)sizeof(word) - 1) span = (int)sizeof(word) - 1;
    memcpy(word, input + start, (size_t)span);
    word[span] = '\0';

    // Split on last dot.
    char table_path[128] = "";
    const char *key_prefix = word;
    char *last_dot = strrchr(word, '.');
    if (last_dot) {
        size_t tn = (size_t)(last_dot - word);
        if (tn >= sizeof(table_path)) tn = sizeof(table_path) - 1;
        memcpy(table_path, word, tn);
        table_path[tn] = '\0';
        key_prefix = last_dot + 1;
    }

    // If the user typed "zt." alone and hit Tab, show the full API.
    bool prefix_empty = (key_prefix[0] == '\0');

    if (!push_table_by_path(L, table_path)) {
        char msg[LUA_CONSOLE_LINE_LEN];
        snprintf(msg, sizeof(msg), "[tab] '%s' is not a table", table_path);
        print_line(msg);
        return false;
    }

    static char keys[128][64];
    int n = collect_keys(L, key_prefix, keys, 128);

    // If completion target is an empty prefix and it's the zt.* table, redirect
    // to full API listing (nicer output).
    if (prefix_empty && table_path[0] == 'z' && table_path[1] == 't' && table_path[2] == '\0') {
        lua_pop(L, 1);
        print_api_list();
        return false;
    }

    if (n == 0) {
        lua_pop(L, 1);
        char msg[LUA_CONSOLE_LINE_LEN];
        snprintf(msg, sizeof(msg), "[tab] no match for '%s'", word);
        print_line(msg);
        return false;
    }

    qsort(keys, (size_t)n, 64, qsort_strcmp);

    // Compute longest common prefix among matches.
    int lcp = (int)strlen(keys[0]);
    for (int i = 1; i < n; i++) {
        int j = 0;
        while (j < lcp && keys[0][j] && keys[i][j] && keys[0][j] == keys[i][j]) j++;
        lcp = j;
    }

    // Determine the full replacement — "table.prefix"
    char replacement[256];
    if (n == 1) {
        // Single match: check if it's a function → append "(".
        lua_getfield(L, -1, keys[0]);
        bool is_fn = lua_isfunction(L, -1);
        lua_pop(L, 1);

        if (table_path[0])
            snprintf(replacement, sizeof(replacement), "%s.%s%s",
                     table_path, keys[0], is_fn ? "(" : "");
        else
            snprintf(replacement, sizeof(replacement), "%s%s",
                     keys[0], is_fn ? "(" : "");
    } else {
        // Multiple — apply the longest common prefix only.
        char common[64];
        if (lcp > (int)sizeof(common) - 1) lcp = (int)sizeof(common) - 1;
        memcpy(common, keys[0], (size_t)lcp);
        common[lcp] = '\0';

        if (table_path[0])
            snprintf(replacement, sizeof(replacement), "%s.%s", table_path, common);
        else
            snprintf(replacement, sizeof(replacement), "%s", common);

        // Print the candidates so the user sees what's available.
        char line[LUA_CONSOLE_LINE_LEN];
        line[0] = ' '; line[1] = ' '; line[2] = '\0';
        int col = 2;
        for (int i = 0; i < n; i++) {
            int klen = (int)strlen(keys[i]);
            if (col > 2 && col + klen + 2 > 76) {
                print_line(line);
                line[0] = ' '; line[1] = ' '; line[2] = '\0';
                col = 2;
            }
            strcat(line, keys[i]);
            strcat(line, "  ");
            col += klen + 2;
        }
        if (col > 2) print_line(line);
    }
    lua_pop(L, 1); // the table we walked

    // Apply replacement in `input`: replace bytes [start..cursor-1] with
    // `replacement`, preserving everything after the original cursor.
    int repl_len = (int)strlen(replacement);
    int tail_len = len - cursor;
    int new_len  = start + repl_len + tail_len;
    if (new_len >= cap) return false;

    if (tail_len > 0)
        memmove(input + start + repl_len, input + cursor, (size_t)tail_len + 1);
    else
        input[start + repl_len] = '\0';
    memcpy(input + start, replacement, (size_t)repl_len);

    *pcursor = start + repl_len;
    return (repl_len != span);
}
