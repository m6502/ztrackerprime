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
// Object layer — Renoise-style proxies with dot-property access.
//   zt.song            properties: bpm, tpb, name, cur_pattern, cur_track,
//                      cur_row, cur_instrument          (all read/write)
//   zt.instrument(i)   properties: name, channel, device, transpose, bank,
//                      volume, patch, index            (index read-only;
//                      i defaults to the current instrument)
// The flat zt.get_*/set_* functions still work; these are the nicer layer
// the user reached for ("zt.cur_track.name" / Renoise muscle memory).
// ===========================================================================

#define ZT_MT_SONG       "zt.song"
#define ZT_MT_INSTRUMENT "zt.instrument"

// ---- zt.song (singleton; all state lives in C globals) --------------------
static int song_index(lua_State *L)
{
    const char *k = luaL_checkstring(L, 2);
    if (!song) { lua_pushnil(L); return 1; }
    if (!strcmp(k, "bpm"))            { lua_pushinteger(L, song->bpm); return 1; }
    if (!strcmp(k, "tpb"))            { lua_pushinteger(L, song->tpb); return 1; }
    if (!strcmp(k, "name"))           { lua_pushstring(L, (const char *)song->title); return 1; }
    if (!strcmp(k, "cur_pattern"))    { lua_pushinteger(L, cur_edit_pattern); return 1; }
    if (!strcmp(k, "cur_track"))      { lua_pushinteger(L, cur_edit_track); return 1; }
    if (!strcmp(k, "cur_row"))        { lua_pushinteger(L, cur_edit_row); return 1; }
    if (!strcmp(k, "cur_instrument")) { lua_pushinteger(L, cur_inst); return 1; }
    return luaL_error(L, "zt.song has no field '%s' (try help('song'))", k);
}

static int song_newindex(lua_State *L)
{
    const char *k = luaL_checkstring(L, 2);
    if (!song) return 0;
    if (!strcmp(k, "bpm")) {
        song->bpm = (int)luaL_checkinteger(L, 3);
        if (ztPlayer) ztPlayer->set_speed();
        return 0;
    }
    if (!strcmp(k, "tpb")) {
        song->tpb = (int)luaL_checkinteger(L, 3);
        if (ztPlayer) ztPlayer->set_speed();
        return 0;
    }
    if (!strcmp(k, "name")) {
        const char *v = luaL_checkstring(L, 3);
        strncpy((char *)song->title, v, ZTM_SONGTITLE_MAXLEN - 1);
        song->title[ZTM_SONGTITLE_MAXLEN - 1] = '\0';
        return 0;
    }
    if (!strcmp(k, "cur_pattern")) {
        int p = (int)luaL_checkinteger(L, 3);
        if (p >= 0 && p < ZTM_MAX_PATTERNS) { cur_edit_pattern = p; need_refresh++; }
        return 0;
    }
    if (!strcmp(k, "cur_track")) {
        int t = (int)luaL_checkinteger(L, 3);
        if (t >= 0 && t < ZTM_MAX_TRACKS) { cur_edit_track = t; need_refresh++; }
        return 0;
    }
    if (!strcmp(k, "cur_row")) {
        int r = (int)luaL_checkinteger(L, 3);
        if (song->patterns[cur_edit_pattern] && r >= 0 &&
            r < song->patterns[cur_edit_pattern]->length) { cur_edit_row = r; need_refresh++; }
        return 0;
    }
    if (!strcmp(k, "cur_instrument")) {
        int i = (int)luaL_checkinteger(L, 3);
        if (i >= 0 && i < ZTM_MAX_INSTS) { cur_inst = i; need_refresh++; }
        return 0;
    }
    return luaL_error(L, "zt.song has no writable field '%s'", k);
}

static int song_tostring(lua_State *L)
{
    if (song) lua_pushfstring(L, "zt.song <%s> bpm=%d tpb=%d",
                              (const char *)song->title, song->bpm, song->tpb);
    else      lua_pushliteral(L, "zt.song <no song>");
    return 1;
}

// ---- zt.instrument(i) -----------------------------------------------------
static instrument *inst_at(int i)
{
    if (!song || i < 0 || i >= ZTM_MAX_INSTS) return nullptr;
    return song->instruments[i];
}

static int l_instrument(lua_State *L)
{
    int i = (int)luaL_optinteger(L, 1, cur_inst);
    if (i < 0 || i >= ZTM_MAX_INSTS)
        return luaL_error(L, "instrument index %d out of range (0..%d)",
                          i, ZTM_MAX_INSTS - 1);
    int *p = (int *)lua_newuserdatauv(L, sizeof(int), 0);
    *p = i;
    luaL_setmetatable(L, ZT_MT_INSTRUMENT);
    return 1;
}

static int inst_index(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_INSTRUMENT);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "index")) { lua_pushinteger(L, i); return 1; }
    instrument *ins = inst_at(i);
    if (!ins) return luaL_error(L, "instrument %d is empty", i);
    if (!strcmp(k, "name"))      { lua_pushstring(L, (const char *)ins->title); return 1; }
    if (!strcmp(k, "channel"))   { lua_pushinteger(L, ins->channel); return 1; }
    if (!strcmp(k, "device"))    { lua_pushinteger(L, ins->midi_device); return 1; }
    if (!strcmp(k, "transpose")) { lua_pushinteger(L, ins->transpose); return 1; }
    if (!strcmp(k, "bank"))      { lua_pushinteger(L, ins->bank); return 1; }
    if (!strcmp(k, "volume"))    { lua_pushinteger(L, ins->default_volume); return 1; }
    if (!strcmp(k, "patch"))     { lua_pushinteger(L, ins->patch); return 1; }
    return luaL_error(L, "instrument has no field '%s' (try help('instrument'))", k);
}

static int inst_newindex(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_INSTRUMENT);
    const char *k = luaL_checkstring(L, 2);
    instrument *ins = inst_at(i);
    if (!ins) return luaL_error(L, "instrument %d is empty", i);
    if (!strcmp(k, "name")) {
        const char *v = luaL_checkstring(L, 3);
        strncpy((char *)ins->title, v, ZTM_INSTTITLE_MAXLEN - 1);
        ins->title[ZTM_INSTTITLE_MAXLEN - 1] = '\0';
        return 0;
    }
    if (!strcmp(k, "channel"))   { ins->channel        = (unsigned char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "device"))    { ins->midi_device    = (unsigned char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "transpose")) { ins->transpose      = (signed char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "bank"))      { ins->bank           = (short)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "volume"))    { ins->default_volume = (unsigned char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "patch"))     { ins->patch          = (signed char)luaL_checkinteger(L, 3); return 0; }
    return luaL_error(L, "instrument has no writable field '%s'", k);
}

static int inst_tostring(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_INSTRUMENT);
    instrument *ins = inst_at(i);
    if (ins) lua_pushfstring(L, "zt.instrument(%d) <%s>", i, (const char *)ins->title);
    else     lua_pushfstring(L, "zt.instrument(%d) <empty>", i);
    return 1;
}

// ---- shared note get/set (bounds-checked) ---------------------------------
static int note_get(int p, int t, int r)
{
    if (!song || p < 0 || p >= ZTM_MAX_PATTERNS || !song->patterns[p]) return 0;
    if (t < 0 || t >= ZTM_MAX_TRACKS || r < 0) return 0;
    event *ev = song->patterns[p]->tracks[t]->get_event((unsigned short)r);
    return ev ? ev->note : 0;
}

static void note_set(int p, int t, int r, int note, int inst, int vol)
{
    if (!song || p < 0 || p >= ZTM_MAX_PATTERNS || !song->patterns[p]) return;
    if (t < 0 || t >= ZTM_MAX_TRACKS || r < 0) return;
    song->patterns[p]->tracks[t]->update_event_safe(
        (unsigned short)r, note, inst, vol, -1, -1, -1);
}

#define ZT_MT_PATTERN   "zt.pattern"
#define ZT_MT_TRACK     "zt.track"
#define ZT_MT_TRANSPORT "zt.transport"

// A track proxy carries BOTH a pattern and a track index (tracks are
// per-pattern); zt.track(t) / pattern:track(t) both produce one of these.
struct TrackRef { int pat; int trk; };

static void new_trackref(lua_State *L, int pat, int trk)
{
    TrackRef *tr = (TrackRef *)lua_newuserdatauv(L, sizeof(TrackRef), 0);
    tr->pat = pat; tr->trk = trk;
    luaL_setmetatable(L, ZT_MT_TRACK);
}

// ---- zt.track(t [, pattern]) ----------------------------------------------
static int track_note(lua_State *L)   // tr:note(row) -> note
{
    TrackRef *tr = (TrackRef *)luaL_checkudata(L, 1, ZT_MT_TRACK);
    int r = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, note_get(tr->pat, tr->trk, r));
    return 1;
}
static int track_set_note(lua_State *L)  // tr:set_note(row, note [,inst [,vol]])
{
    TrackRef *tr = (TrackRef *)luaL_checkudata(L, 1, ZT_MT_TRACK);
    int r    = (int)luaL_checkinteger(L, 2);
    int note = (int)luaL_checkinteger(L, 3);
    int inst = (int)luaL_optinteger(L, 4, -1);
    int vol  = (int)luaL_optinteger(L, 5, -1);
    note_set(tr->pat, tr->trk, r, note, inst, vol);
    return 0;
}
static int track_index(lua_State *L)
{
    TrackRef *tr = (TrackRef *)luaL_checkudata(L, 1, ZT_MT_TRACK);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "index"))    { lua_pushinteger(L, tr->trk); return 1; }
    if (!strcmp(k, "pattern"))  { lua_pushinteger(L, tr->pat); return 1; }
    if (!strcmp(k, "muted")) {
        lua_pushboolean(L, song ? (song->track_mute[tr->trk] != 0) : 0);
        return 1;
    }
    if (!strcmp(k, "note"))     { lua_pushcfunction(L, track_note); return 1; }
    if (!strcmp(k, "set_note")) { lua_pushcfunction(L, track_set_note); return 1; }
    return luaL_error(L, "zt.track has no field '%s' (try help('track'))", k);
}
static int track_newindex(lua_State *L)
{
    TrackRef *tr = (TrackRef *)luaL_checkudata(L, 1, ZT_MT_TRACK);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "muted")) {
        bool v = lua_toboolean(L, 3);
        if (song && tr->trk >= 0 && tr->trk < ZTM_MAX_TRACKS) {
            song->track_mute[tr->trk] = v ? 1 : 0;
            if (MidiOut) { if (v) MidiOut->mute_track(tr->trk);
                           else   MidiOut->unmute_track(tr->trk); }
            need_refresh++;
        }
        return 0;
    }
    return luaL_error(L, "zt.track has no writable field '%s'", k);
}
static int track_tostring(lua_State *L)
{
    TrackRef *tr = (TrackRef *)luaL_checkudata(L, 1, ZT_MT_TRACK);
    lua_pushfstring(L, "zt.track(%d) [pattern %d]%s", tr->trk, tr->pat,
                    (song && song->track_mute[tr->trk]) ? " muted" : "");
    return 1;
}
static int l_track(lua_State *L)
{
    int t = (int)luaL_optinteger(L, 1, cur_edit_track);
    int p = (int)luaL_optinteger(L, 2, cur_edit_pattern);
    if (t < 0 || t >= ZTM_MAX_TRACKS)
        return luaL_error(L, "track index %d out of range (0..%d)", t, ZTM_MAX_TRACKS - 1);
    if (p < 0 || p >= ZTM_MAX_PATTERNS)
        return luaL_error(L, "pattern index %d out of range (0..%d)", p, ZTM_MAX_PATTERNS - 1);
    new_trackref(L, p, t);
    return 1;
}

// ---- zt.pattern(p) --------------------------------------------------------
static int pat_note(lua_State *L)     // pat:note(track, row) -> note
{
    int p = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    int t = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    lua_pushinteger(L, note_get(p, t, r));
    return 1;
}
static int pat_set_note(lua_State *L) // pat:set_note(track, row, note [,inst [,vol]])
{
    int p    = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    int t    = (int)luaL_checkinteger(L, 2);
    int r    = (int)luaL_checkinteger(L, 3);
    int note = (int)luaL_checkinteger(L, 4);
    int inst = (int)luaL_optinteger(L, 5, -1);
    int vol  = (int)luaL_optinteger(L, 6, -1);
    note_set(p, t, r, note, inst, vol);
    return 0;
}
static int pat_track(lua_State *L)    // pat:track(t) -> zt.track bound to this pattern
{
    int p = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    int t = (int)luaL_checkinteger(L, 2);
    if (t < 0 || t >= ZTM_MAX_TRACKS)
        return luaL_error(L, "track index %d out of range (0..%d)", t, ZTM_MAX_TRACKS - 1);
    new_trackref(L, p, t);
    return 1;
}
static int pat_index(lua_State *L)
{
    int p = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "index"))  { lua_pushinteger(L, p); return 1; }
    if (!strcmp(k, "length")) {
        lua_pushinteger(L, (song && song->patterns[p]) ? song->patterns[p]->length : 0);
        return 1;
    }
    if (!strcmp(k, "empty")) {
        lua_pushboolean(L, (song && song->patterns[p]) ? song->patterns[p]->isempty() : 1);
        return 1;
    }
    if (!strcmp(k, "note"))     { lua_pushcfunction(L, pat_note); return 1; }
    if (!strcmp(k, "set_note")) { lua_pushcfunction(L, pat_set_note); return 1; }
    if (!strcmp(k, "track"))    { lua_pushcfunction(L, pat_track); return 1; }
    return luaL_error(L, "zt.pattern has no field '%s' (try help('pattern'))", k);
}
static int pat_newindex(lua_State *L)
{
    int p = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "length")) {
        int len = (int)luaL_checkinteger(L, 3);
        if (len < 1) len = 1;
        if (song && song->patterns[p]) { song->patterns[p]->resize(len); need_refresh++; }
        return 0;
    }
    return luaL_error(L, "zt.pattern has no writable field '%s'", k);
}
static int pat_tostring(lua_State *L)
{
    int p = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    int len = (song && song->patterns[p]) ? song->patterns[p]->length : 0;
    lua_pushfstring(L, "zt.pattern(%d) length=%d", p, len);
    return 1;
}
static int l_pattern(lua_State *L)
{
    int p = (int)luaL_optinteger(L, 1, cur_edit_pattern);
    if (p < 0 || p >= ZTM_MAX_PATTERNS)
        return luaL_error(L, "pattern index %d out of range (0..%d)", p, ZTM_MAX_PATTERNS - 1);
    int *up = (int *)lua_newuserdatauv(L, sizeof(int), 0);
    *up = p;
    luaL_setmetatable(L, ZT_MT_PATTERN);
    return 1;
}

// ---- zt.transport (singleton) ---------------------------------------------
static int trans_play(lua_State *L)         { (void)L; if (ztPlayer) ztPlayer->play(0, cur_edit_pattern, 0); return 0; }
static int trans_stop(lua_State *L)         { (void)L; if (ztPlayer) ztPlayer->stop(); return 0; }
static int trans_play_pattern(lua_State *L) { (void)L; if (ztPlayer) ztPlayer->play(0, cur_edit_pattern, 1); return 0; }
static int trans_panic(lua_State *L)        { (void)L; if (MidiOut) MidiOut->panic(); return 0; }
static int trans_index(lua_State *L)
{
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "playing"))      { lua_pushboolean(L, (ztPlayer && ztPlayer->playing) ? 1 : 0); return 1; }
    if (!strcmp(k, "play"))         { lua_pushcfunction(L, trans_play); return 1; }
    if (!strcmp(k, "stop"))         { lua_pushcfunction(L, trans_stop); return 1; }
    if (!strcmp(k, "play_pattern")) { lua_pushcfunction(L, trans_play_pattern); return 1; }
    if (!strcmp(k, "panic"))        { lua_pushcfunction(L, trans_panic); return 1; }
    return luaL_error(L, "zt.transport has no field '%s' (try help('transport'))", k);
}
static int trans_tostring(lua_State *L)
{
    lua_pushfstring(L, "zt.transport <%s>",
                    (ztPlayer && ztPlayer->playing) ? "playing" : "stopped");
    return 1;
}

// ===========================================================================
// ZtLuaEngine implementation
// ===========================================================================

ZtLuaEngine::ZtLuaEngine()
    : L(nullptr), line_count(0), scroll_off(0),
      history_count(0), history_pos(-1),
      tab_cycle_active(false), tab_cycle_start(0),
      tab_cycle_index(0), tab_cycle_n(0)
{
    memset(lines,   0, sizeof(lines));
    memset(history, 0, sizeof(history));
    tab_cycle_table[0] = '\0';
    memset(tab_cycle_keys, 0, sizeof(tab_cycle_keys));
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

    // Prelude: Renoise-style introspection helpers (rprint / oprint / dump)
    // plus a help() that documents the zt.* API. Kept here as a Lua string so
    // it lives alongside the bindings and needs no bundled .lua file.
    static const char *prelude =
        "do\n"
        "  local function sorted_keys(t)\n"
        "    local ks = {}\n"
        "    for k in pairs(t) do ks[#ks+1] = k end\n"
        "    table.sort(ks, function(a,b) return tostring(a) < tostring(b) end)\n"
        "    return ks\n"
        "  end\n"
        "  -- rprint(t): recursive dump of a table's keys and values (Renoise\n"
        "  -- style). Depth-limited and cycle-safe so rprint(_G) won't hang.\n"
        "  function rprint(t, indent, seen, depth)\n"
        "    if type(t) ~= 'table' then print(tostring(t)) return end\n"
        "    indent = indent or '' ; seen = seen or {} ; depth = depth or 0\n"
        "    if seen[t] then print(indent .. '<cycle>') return end\n"
        "    seen[t] = true\n"
        "    for _, k in ipairs(sorted_keys(t)) do\n"
        "      local v = t[k] ; local tv = type(v)\n"
        "      if tv == 'table' and depth < 5 then\n"
        "        print(indent .. tostring(k) .. ' = {')\n"
        "        rprint(v, indent .. '  ', seen, depth + 1)\n"
        "        print(indent .. '}')\n"
        "      else\n"
        "        print(indent .. tostring(k) .. ' = ' .. tostring(v) .. '  (' .. tv .. ')')\n"
        "      end\n"
        "    end\n"
        "    seen[t] = nil\n"
        "  end\n"
        "  -- oprint(t): one level, lists each key with its type (Renoise style).\n"
        "  function oprint(t)\n"
        "    if type(t) ~= 'table' then print(type(t) .. ': ' .. tostring(t)) return end\n"
        "    for _, k in ipairs(sorted_keys(t)) do\n"
        "      print('  ' .. tostring(k) .. '  (' .. type(t[k]) .. ')')\n"
        "    end\n"
        "  end\n"
        "  dump = rprint\n"
        "  local docs = {\n"
        "    {'get_note','get_note(pattern, track, row) -> note   read a note cell (0 = empty)'},\n"
        "    {'set_note','set_note(pattern, track, row, note [,inst [,vol]])   write a note cell'},\n"
        "    {'get_pattern_length','get_pattern_length(pattern) -> rows'},\n"
        "    {'set_pattern_length','set_pattern_length(pattern, rows)'},\n"
        "    {'play','play()   start playback from the current pattern'},\n"
        "    {'stop','stop()   stop playback'},\n"
        "    {'play_pattern','play_pattern()   loop the current pattern'},\n"
        "    {'get_bpm','get_bpm() -> bpm'},\n"
        "    {'set_bpm','set_bpm(bpm)'},\n"
        "    {'get_tpb','get_tpb() -> ticks-per-beat'},\n"
        "    {'set_tpb','set_tpb(tpb)'},\n"
        "    {'cur_pattern','cur_pattern() -> current pattern index'},\n"
        "    {'cur_track','cur_track() -> current track index'},\n"
        "    {'cur_row','cur_row() -> current row'},\n"
        "    {'cur_instrument','cur_instrument() -> current instrument index'},\n"
        "    {'set_cur_pattern','set_cur_pattern(p)'},\n"
        "    {'set_cur_row','set_cur_row(r)'},\n"
        "    {'midi_note_on','midi_note_on(device, note, channel [,velocity=127])'},\n"
        "    {'midi_note_off','midi_note_off(device, note, channel)'},\n"
        "    {'status','status(text)   show a status-bar message'},\n"
        "    {'print','print(...)   print to this console'},\n"
        "  }\n"
        "  local objdocs = {\n"
        "    song = 'zt.song  properties (read/write): bpm, tpb, name, cur_pattern, cur_track, cur_row, cur_instrument',\n"
        "    instrument = 'zt.instrument(i)  properties: name, channel, device, transpose, bank, volume, patch, index  (i defaults to current)',\n"
        "    pattern = 'zt.pattern(p)  props: length(rw), index, empty;  methods: :note(track,row), :set_note(track,row,note[,inst[,vol]]), :track(t)  (p defaults to current)',\n"
        "    track = 'zt.track(t[,p])  props: index, pattern, muted(rw);  methods: :note(row), :set_note(row,note[,inst[,vol]])  (defaults to current track/pattern)',\n"
        "    transport = 'zt.transport  prop: playing;  methods: :play(), :stop(), :play_pattern(), :panic()',\n"
        "  }\n"
        "  function help(what)\n"
        "    if what == nil then\n"
        "      print('zt.* are FUNCTIONS -- call them with ()   e.g.  zt.cur_instrument()')\n"
        "      for _, d in ipairs(docs) do print('  zt.' .. d[2]) end\n"
        "      print('Objects (dot access, read & write):')\n"
        "      print('  ' .. objdocs.song)\n"
        "      print('  ' .. objdocs.instrument)\n"
        "      print('  ' .. objdocs.pattern)\n"
        "      print('  ' .. objdocs.track)\n"
        "      print('  ' .. objdocs.transport)\n"
        "      print('  e.g.  zt.song.bpm = 140   zt.pattern(0):set_note(0,0,60)   zt.transport:play()')\n"
        "      print('Introspect: rprint(zt) dumps all, oprint(t) lists one level, help(\"name\") for one.')\n"
        "      return\n"
        "    end\n"
        "    local name = tostring(what):gsub('^zt%.', '')\n"
        "    if objdocs[name] then print('  ' .. objdocs[name]) return end\n"
        "    for _, d in ipairs(docs) do\n"
        "      if d[1] == name then print('  zt.' .. d[2]) return end\n"
        "    end\n"
        "    print(\"no help for '\" .. name .. \"' -- try help() for the full list\")\n"
        "  end\n"
        "end\n";
    if (luaL_dostring(L, prelude) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        char msg[LUA_CONSOLE_LINE_LEN];
        snprintf(msg, sizeof(msg), "[Lua] prelude error: %s", err ? err : "(nil)");
        print_line(msg);
        lua_pop(L, 1);
    }

    print_line("[Lua] Lua 5.4 ready. zt.* are functions -- CALL them: zt.cur_instrument()");
    print_line("[Lua] help() lists the API. rprint(zt)/oprint(zt) inspect. Tab cycles completions.");

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

    // Print any returned values. If a result is a function, append a hint —
    // this is the #1 stumble: `zt.cur_instrument` prints "function: 0x..",
    // because it's the function itself; you have to CALL it with ().
    int nret = lua_gettop(L) - nresults_before;
    for (int i = 1; i <= nret; i++) {
        int slot = nresults_before + i;
        bool is_fn = lua_isfunction(L, slot);
        size_t len;
        const char *s = luaL_tolstring(L, slot, &len);
        if (is_fn) {
            char msg[LUA_CONSOLE_LINE_LEN];
            snprintf(msg, sizeof(msg),
                     "%s   -- a function; add () to call it, or help()", s);
            print_line(msg);
        } else {
            print_line(s);
        }
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

    // --- Object layer (Renoise-style dot properties) -----------------------
    // Metatables live in the registry; creating them here leaves the `zt`
    // table on top of the stack untouched (each newmetatable push is popped).
    luaL_newmetatable(L, ZT_MT_SONG);
    lua_pushcfunction(L, song_index);     lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, song_newindex);  lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, song_tostring);  lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_INSTRUMENT);
    lua_pushcfunction(L, inst_index);     lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, inst_newindex);  lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, inst_tostring);  lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_PATTERN);
    lua_pushcfunction(L, pat_index);      lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, pat_newindex);   lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, pat_tostring);   lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_TRACK);
    lua_pushcfunction(L, track_index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, track_newindex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, track_tostring); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_TRANSPORT);
    lua_pushcfunction(L, trans_index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, trans_tostring); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    // Constructors: zt.instrument(i) / zt.pattern(p) / zt.track(t [,p]).
    lua_pushcfunction(L, l_instrument);   lua_setfield(L, -2, "instrument");
    lua_pushcfunction(L, l_pattern);      lua_setfield(L, -2, "pattern");
    lua_pushcfunction(L, l_track);        lua_setfield(L, -2, "track");

    // Singletons: zt.song and zt.transport.
    lua_newuserdatauv(L, 1, 0);
    luaL_setmetatable(L, ZT_MT_SONG);
    lua_setfield(L, -2, "song");
    lua_newuserdatauv(L, 1, 0);
    luaL_setmetatable(L, ZT_MT_TRANSPORT);
    lua_setfield(L, -2, "transport");

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

// Replace input[start..cursor-1] with `replacement`, preserving the tail
// after the original cursor. Updates *pcursor. Returns false if it wouldn't
// fit in `cap` bytes.
static bool apply_completion(char *input, int cap, int *pcursor,
                            int start, int cursor, const char *replacement)
{
    int len      = (int)strlen(input);
    int repl_len = (int)strlen(replacement);
    int tail_len = len - cursor;
    if (start + repl_len + tail_len >= cap) return false;
    if (tail_len > 0)
        memmove(input + start + repl_len, input + cursor, (size_t)tail_len + 1);
    else
        input[start + repl_len] = '\0';
    memcpy(input + start, replacement, (size_t)repl_len);
    *pcursor = start + repl_len;
    return true;
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

    // --- Cycling: a repeated Tab with no edits since the last completion
    //     advances to the next candidate (real autocomplete feel). We detect
    //     "no edits" by the word at the cursor still being exactly the
    //     candidate we last inserted, at the same start position.
    if (tab_cycle_active && tab_cycle_n > 1 && start == tab_cycle_start &&
        strcmp(table_path, tab_cycle_table) == 0 &&
        strcmp(key_prefix, tab_cycle_keys[tab_cycle_index]) == 0) {
        tab_cycle_index = (tab_cycle_index + 1) % tab_cycle_n;
        char repl[256];
        if (table_path[0])
            snprintf(repl, sizeof(repl), "%s.%s", table_path,
                     tab_cycle_keys[tab_cycle_index]);
        else
            snprintf(repl, sizeof(repl), "%s", tab_cycle_keys[tab_cycle_index]);
        return apply_completion(input, cap, pcursor, start, cursor, repl);
    }
    // Any non-continuation Tab starts fresh.
    tab_cycle_active = false;

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

    char replacement[256];

    // --- Single match: complete fully, append "(" if it's a function. ----
    if (n == 1) {
        lua_getfield(L, -1, keys[0]);
        bool is_fn = lua_isfunction(L, -1);
        lua_pop(L, 1);
        lua_pop(L, 1); // the table we walked
        if (table_path[0])
            snprintf(replacement, sizeof(replacement), "%s.%s%s",
                     table_path, keys[0], is_fn ? "(" : "");
        else
            snprintf(replacement, sizeof(replacement), "%s%s",
                     keys[0], is_fn ? "(" : "");
        return apply_completion(input, cap, pcursor, start, cursor, replacement);
    }
    lua_pop(L, 1); // the table we walked

    // Helper: print the candidate list, two-column wrapped.
    // (Captures keys/n by reference; both are in scope here.)
    auto list_candidates = [&]() {
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
    };

    // --- Multiple matches: first advance by the longest common prefix. ---
    int prefix_len = (int)strlen(key_prefix);
    if (lcp > prefix_len) {
        char common[64];
        if (lcp > (int)sizeof(common) - 1) lcp = (int)sizeof(common) - 1;
        memcpy(common, keys[0], (size_t)lcp);
        common[lcp] = '\0';
        if (table_path[0])
            snprintf(replacement, sizeof(replacement), "%s.%s", table_path, common);
        else
            snprintf(replacement, sizeof(replacement), "%s", common);
        list_candidates();   // show where the prefix can still branch
        return apply_completion(input, cap, pcursor, start, cursor, replacement);
    }

    // --- Prefix can't advance: begin cycling. Store the candidate set, list
    //     it once, and insert the first candidate. The continuation check at
    //     the top of this function advances through the rest on later Tabs. ---
    tab_cycle_active = true;
    tab_cycle_start  = start;
    tab_cycle_n      = (n > 128) ? 128 : n;
    snprintf(tab_cycle_table, sizeof(tab_cycle_table), "%s", table_path);
    for (int i = 0; i < tab_cycle_n; i++) {
        strncpy(tab_cycle_keys[i], keys[i], 63);
        tab_cycle_keys[i][63] = '\0';
    }
    tab_cycle_index = 0;
    list_candidates();
    if (table_path[0])
        snprintf(replacement, sizeof(replacement), "%s.%s",
                 table_path, tab_cycle_keys[0]);
    else
        snprintf(replacement, sizeof(replacement), "%s", tab_cycle_keys[0]);
    return apply_completion(input, cap, pcursor, start, cursor, replacement);
}
