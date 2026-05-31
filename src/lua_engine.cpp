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
#include "CDataBuf.h"   // song message text buffer (zt.song.message)

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

// --- notifiers: zt.on(event, fn) / zt.off(event) / zt.fire(event [,arg]) ----
// Built-in events fired from the main loop: "idle" (every frame), "play",
// "stop", "row" (arg = current row). Custom event names work too via fire().
static int l_on(lua_State *L)
{
    const char *ev = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_getfield(L, LUA_REGISTRYINDEX, "zt_notifiers");   // t
    lua_getfield(L, -1, ev);                              // t[ev]
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);                                  // new list
        lua_pushvalue(L, -1);                             // dup
        lua_setfield(L, -3, ev);                          // t[ev] = list
    }
    int n = (int)lua_rawlen(L, -1);
    lua_pushvalue(L, 2);                                  // the function
    lua_rawseti(L, -2, n + 1);                            // list[n+1] = fn
    lua_pop(L, 2);                                        // list, t
    return 0;
}
static int l_off(lua_State *L)   // clear all callbacks for an event
{
    const char *ev = luaL_checkstring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "zt_notifiers");
    lua_pushnil(L);
    lua_setfield(L, -2, ev);
    lua_pop(L, 1);
    return 0;
}
static int l_fire(lua_State *L)  // zt.fire(event [, arg])
{
    const char *ev = luaL_checkstring(L, 1);
    bool has_arg = !lua_isnoneornil(L, 2);
    long arg = has_arg ? (long)luaL_checkinteger(L, 2) : 0;
    g_lua.fire(ev, arg, has_arg);
    return 0;
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
    if (!strcmp(k, "message")) {
        if (song->songmessage && song->songmessage->songmessage &&
            song->songmessage->songmessage->getbuffer()) {
            CDataBuf *b = song->songmessage->songmessage;
            int n = b->getsize();
            if (n > 0 && b->getbuffer()[n - 1] == '\0') n--;   // drop the stored null
            lua_pushlstring(L, b->getbuffer(), (size_t)(n < 0 ? 0 : n));
        } else lua_pushliteral(L, "");
        return 1;
    }
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
    if (!strcmp(k, "message")) {
        size_t len; const char *v = luaL_checklstring(L, 3, &len);
        if (!song->songmessage) song->songmessage = new songmsg();
        CDataBuf *b = song->songmessage->songmessage;
        b->flush();
        if (len) b->write((char *)v, (int)len);
        b->pushc((char)0);   // null-terminate, matching the loader
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
    if (!strcmp(k, "volume"))        { lua_pushinteger(L, ins->default_volume); return 1; }
    if (!strcmp(k, "patch"))         { lua_pushinteger(L, ins->patch); return 1; }
    if (!strcmp(k, "global_volume")) { lua_pushinteger(L, ins->global_volume); return 1; }
    if (!strcmp(k, "default_length")){ lua_pushinteger(L, ins->default_length); return 1; }
    if (!strcmp(k, "flags"))         { lua_pushinteger(L, ins->flags); return 1; }
    if (!strcmp(k, "ccizer_bank"))   { lua_pushstring(L, ins->ccizer_bank); return 1; }
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
    if (!strcmp(k, "volume"))        { ins->default_volume = (unsigned char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "patch"))         { ins->patch          = (signed char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "global_volume")) { ins->global_volume  = (unsigned char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "default_length")){ ins->default_length = (unsigned short)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "flags"))         { ins->flags          = (unsigned char)luaL_checkinteger(L, 3); return 0; }
    if (!strcmp(k, "ccizer_bank")) {
        const char *v = luaL_checkstring(L, 3);
        strncpy(ins->ccizer_bank, v, sizeof(ins->ccizer_bank) - 1);
        ins->ccizer_bank[sizeof(ins->ccizer_bank) - 1] = '\0';
        return 0;
    }
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

// Cell constructors are defined further down (after the transport object);
// forward-declare them so the track/pattern __index handlers can hand out
// :cell(...) methods.
static int pat_cell(lua_State *L);
static int track_cell(lua_State *L);

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
    if (!strcmp(k, "cell"))     { lua_pushcfunction(L, track_cell); return 1; }
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
    if (!strcmp(k, "cell"))     { lua_pushcfunction(L, pat_cell); return 1; }
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
// Cell object — a single (pattern, track, row) location with FULL field
// access: note, instrument, volume, length, effect, effect_data (all r/w).
// Empty cells read back the engine's empty sentinels (note 0x80, inst 0xFF,
// vol 0x80, effect 0xFF). Writing one field leaves the others alone (the
// engine treats -1 as "unchanged"), so cell.volume = 100 is a real merge.
// ===========================================================================
#define ZT_MT_CELL   "zt.cell"
#define ZT_MT_ORDERS "zt.orders"

struct CellRef { int pat; int trk; int row; };

static track *cell_track(CellRef *c)
{
    if (!song || c->pat < 0 || c->pat >= ZTM_MAX_PATTERNS || !song->patterns[c->pat]) return nullptr;
    if (c->trk < 0 || c->trk >= ZTM_MAX_TRACKS) return nullptr;
    return song->patterns[c->pat]->tracks[c->trk];
}
static event *cell_event(CellRef *c)
{
    track *tk = cell_track(c);
    if (!tk || c->row < 0) return nullptr;
    return tk->get_event((unsigned short)c->row);
}
static void new_cellref(lua_State *L, int pat, int trk, int row)
{
    CellRef *c = (CellRef *)lua_newuserdatauv(L, sizeof(CellRef), 0);
    c->pat = pat; c->trk = trk; c->row = row;
    luaL_setmetatable(L, ZT_MT_CELL);
}
// write one field; -1 in the others = leave unchanged
static void cell_write(CellRef *c, int note, int inst, int vol, int len, int eff, int effd)
{
    track *tk = cell_track(c);
    if (tk && c->row >= 0)
        tk->update_event_safe((unsigned short)c->row, note, inst, vol, len, eff, effd);
}
static int cell_clear(lua_State *L)
{
    CellRef *c = (CellRef *)luaL_checkudata(L, 1, ZT_MT_CELL);
    track *tk = cell_track(c);
    if (tk && c->row >= 0) { tk->del_event((unsigned short)c->row); need_refresh++; }
    return 0;
}
static int cell_index(lua_State *L)
{
    CellRef *c = (CellRef *)luaL_checkudata(L, 1, ZT_MT_CELL);
    const char *k = luaL_checkstring(L, 2);
    event *e = cell_event(c);
    if (!strcmp(k, "note"))        { lua_pushinteger(L, e ? e->note : 0x80); return 1; }
    if (!strcmp(k, "instrument"))  { lua_pushinteger(L, e ? e->inst : 0xFF); return 1; }
    if (!strcmp(k, "volume"))      { lua_pushinteger(L, e ? e->vol : 0x80); return 1; }
    if (!strcmp(k, "length"))      { lua_pushinteger(L, e ? e->length : 0); return 1; }
    if (!strcmp(k, "effect"))      { lua_pushinteger(L, e ? e->effect : 0xFF); return 1; }
    if (!strcmp(k, "effect_data")) { lua_pushinteger(L, e ? e->effect_data : 0); return 1; }
    if (!strcmp(k, "empty"))       { lua_pushboolean(L, e == nullptr); return 1; }
    if (!strcmp(k, "pattern"))     { lua_pushinteger(L, c->pat); return 1; }
    if (!strcmp(k, "track"))       { lua_pushinteger(L, c->trk); return 1; }
    if (!strcmp(k, "row"))         { lua_pushinteger(L, c->row); return 1; }
    if (!strcmp(k, "name")) {       // note rendered as "C-4" / "===" / "..."
        char buf[4] = {0,0,0,0};
        hex2note(buf, (unsigned char)(e ? e->note : 0x80));
        lua_pushstring(L, buf);
        return 1;
    }
    if (!strcmp(k, "clear"))       { lua_pushcfunction(L, cell_clear); return 1; }
    return luaL_error(L, "zt.cell has no field '%s' (try help('cell'))", k);
}
static int cell_newindex(lua_State *L)
{
    CellRef *c = (CellRef *)luaL_checkudata(L, 1, ZT_MT_CELL);
    const char *k = luaL_checkstring(L, 2);
    int v = (int)luaL_checkinteger(L, 3);
    if (!strcmp(k, "note"))        { cell_write(c, v, -1, -1, -1, -1, -1); return 0; }
    if (!strcmp(k, "instrument"))  { cell_write(c, -1, v, -1, -1, -1, -1); return 0; }
    if (!strcmp(k, "volume"))      { cell_write(c, -1, -1, v, -1, -1, -1); return 0; }
    if (!strcmp(k, "length"))      { cell_write(c, -1, -1, -1, v, -1, -1); return 0; }
    if (!strcmp(k, "effect"))      { cell_write(c, -1, -1, -1, -1, v, -1); return 0; }
    if (!strcmp(k, "effect_data")) { cell_write(c, -1, -1, -1, -1, -1, v); return 0; }
    return luaL_error(L, "zt.cell has no writable field '%s'", k);
}
static int cell_tostring(lua_State *L)
{
    CellRef *c = (CellRef *)luaL_checkudata(L, 1, ZT_MT_CELL);
    event *e = cell_event(c);
    char buf[4] = {0,0,0,0};
    hex2note(buf, (unsigned char)(e ? e->note : 0x80));
    lua_pushfstring(L, "zt.cell(p%d t%d r%d) %s i%d v%d", c->pat, c->trk, c->row,
                    buf, e ? e->inst : 0xFF, e ? e->vol : 0x80);
    return 1;
}
static int l_cell(lua_State *L)   // zt.cell(track, row [, pattern])
{
    int t = (int)luaL_checkinteger(L, 1);
    int r = (int)luaL_checkinteger(L, 2);
    int p = (int)luaL_optinteger(L, 3, cur_edit_pattern);
    if (t < 0 || t >= ZTM_MAX_TRACKS) return luaL_error(L, "track %d out of range", t);
    if (p < 0 || p >= ZTM_MAX_PATTERNS) return luaL_error(L, "pattern %d out of range", p);
    new_cellref(L, p, t, r);
    return 1;
}
// pattern:cell(track, row)  and  track:cell(row)
static int pat_cell(lua_State *L)
{
    int p = *(int *)luaL_checkudata(L, 1, ZT_MT_PATTERN);
    int t = (int)luaL_checkinteger(L, 2);
    int r = (int)luaL_checkinteger(L, 3);
    new_cellref(L, p, t, r);
    return 1;
}
static int track_cell(lua_State *L)
{
    TrackRef *tr = (TrackRef *)luaL_checkudata(L, 1, ZT_MT_TRACK);
    int r = (int)luaL_checkinteger(L, 2);
    new_cellref(L, tr->pat, tr->trk, r);
    return 1;
}

// ===========================================================================
// zt.orders — the order list as an array proxy: zt.orders[i] reads/writes
// orderlist[i] (0..255 = pattern, zt.BREAK / zt.SKIP for the markers);
// zt.orders.count and #zt.orders give the number of playing entries.
// ===========================================================================
static int orders_count(void)
{
    if (!song) return 0;
    int i = 0;
    while (i < ZTM_ORDERLIST_LEN &&
           (song->orderlist[i] == 0x101 || song->orderlist[i] <= 0xFF)) i++;
    return i;
}
static int orders_index(lua_State *L)
{
    if (lua_type(L, 2) == LUA_TNUMBER) {
        int i = (int)lua_tointeger(L, 2);
        if (!song || i < 0 || i >= ZTM_ORDERLIST_LEN)
            return luaL_error(L, "order index %d out of range (0..%d)", i, ZTM_ORDERLIST_LEN - 1);
        lua_pushinteger(L, song->orderlist[i]);
        return 1;
    }
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "count")) { lua_pushinteger(L, orders_count()); return 1; }
    return luaL_error(L, "zt.orders has no field '%s' (use a number index, or .count)", k);
}
static int orders_newindex(lua_State *L)
{
    int i = (int)luaL_checkinteger(L, 2);
    int v = (int)luaL_checkinteger(L, 3);
    if (!song || i < 0 || i >= ZTM_ORDERLIST_LEN)
        return luaL_error(L, "order index %d out of range (0..%d)", i, ZTM_ORDERLIST_LEN - 1);
    song->orderlist[i] = v;
    if (ztPlayer) ztPlayer->num_orders();
    need_refresh++;
    return 0;
}
static int orders_len(lua_State *L)      { lua_pushinteger(L, orders_count()); return 1; }
static int orders_tostring(lua_State *L) { lua_pushfstring(L, "zt.orders [%d playing]", orders_count()); return 1; }

// ---- note-name helpers ----------------------------------------------------
static int l_note_name(lua_State *L)   // zt.note_name(60) -> "C-4"
{
    int n = (int)luaL_checkinteger(L, 1);
    char buf[4] = {0,0,0,0};
    hex2note(buf, (unsigned char)(n & 0xFF));
    lua_pushstring(L, buf);
    return 1;
}
// case-insensitive compare of the first 3 chars of s against lowercase w
static bool ieq3(const char *s, const char *w)
{
    for (int i = 0; i < 3; i++) {
        char a = s[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (a != w[i]) return false;
    }
    return true;
}
static int l_note_value(lua_State *L)  // zt.note_value("C-4") -> 60 ; "off"->0x82, "cut"->0x81
{
    const char *s = luaL_checkstring(L, 1);
    // skip leading spaces
    while (*s == ' ') s++;
    // sentinels
    if (s[0] == '=' || ieq3(s, "off")) { lua_pushinteger(L, 0x82); return 1; }
    if (s[0] == '^' || ieq3(s, "cut")) { lua_pushinteger(L, 0x81); return 1; }
    if (s[0] == '.' || s[0] == '\0')   { lua_pushinteger(L, 0x80); return 1; }
    // "C-4" / "C#4": letter + accidental(- or #) + octave digit
    static const char *names = "C-C#D-D#E-F-F#G-G#A-A#B-";
    char a = s[0];
    if (a >= 'a' && a <= 'z') a = (char)(a - 32);   // uppercase the letter
    char b = s[1];
    if (b == 'b') b = '-';          // tolerate "Cb"-style flats as natural
    int semi = -1;
    for (int i = 0; i < 12; i++) {
        if (names[2 * i] == a && names[2 * i + 1] == b) { semi = i; break; }
    }
    if (semi < 0) return luaL_error(L, "bad note name '%s' (try \"C-4\", \"F#3\", \"off\", \"cut\")", s);
    char oc = s[2];
    if (oc < '0' || oc > '9') return luaL_error(L, "bad octave in note name '%s'", s);
    int val = (oc - '0') * 12 + semi;
    if (val < 0 || val > 127) return luaL_error(L, "note '%s' out of MIDI range", s);
    lua_pushinteger(L, val);
    return 1;
}

// ===========================================================================
// zt.midimacro(i) — F4 MIDI-macro slots. name (rw), data steps via
// :get(step)/:set(step,value), .empty/.syx/.index. data values are raw
// (a MIDI byte, or zt.MACRO_END / zt.MACRO_PARAM tokens).
// ===========================================================================
#define ZT_MT_MIDIMACRO "zt.midimacro"
#define ZT_MT_ARPEGGIO  "zt.arpeggio"

static midimacro *macro_at(int i)
{
    if (!song || i < 0 || i >= ZTM_MAX_MIDIMACROS) return nullptr;
    return song->midimacros[i];
}
static bool name_is_syx(const char *n)   // case-insensitive "*.syx", len > 4
{
    int L = (int)strlen(n);
    if (L <= 4) return false;
    const char *e = n + L - 4;
    return (e[0] == '.') &&
           (e[1] == 's' || e[1] == 'S') &&
           (e[2] == 'y' || e[2] == 'Y') &&
           (e[3] == 'x' || e[3] == 'X');
}
static int mm_get(lua_State *L)   // mm:get(step) -> data value
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_MIDIMACRO);
    int s = (int)luaL_checkinteger(L, 2);
    midimacro *m = macro_at(i);
    if (!m || s < 0 || s >= ZTM_MIDIMACRO_MAXLEN) { lua_pushinteger(L, ZTM_MIDIMAC_END); return 1; }
    lua_pushinteger(L, m->data[s]);
    return 1;
}
static int mm_set(lua_State *L)   // mm:set(step, value)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_MIDIMACRO);
    int s = (int)luaL_checkinteger(L, 2);
    int v = (int)luaL_checkinteger(L, 3);
    midimacro *m = macro_at(i);
    if (m && s >= 0 && s < ZTM_MIDIMACRO_MAXLEN) { m->data[s] = (unsigned short)v; need_refresh++; }
    return 0;
}
static int mm_index(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_MIDIMACRO);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "index")) { lua_pushinteger(L, i); return 1; }
    midimacro *m = macro_at(i);
    if (!m) return luaL_error(L, "midimacro %d is empty", i);
    if (!strcmp(k, "name"))  { lua_pushstring(L, m->name); return 1; }
    if (!strcmp(k, "empty")) { lua_pushboolean(L, m->isempty()); return 1; }
    if (!strcmp(k, "syx"))   { lua_pushboolean(L, name_is_syx(m->name)); return 1; }
    if (!strcmp(k, "get"))   { lua_pushcfunction(L, mm_get); return 1; }
    if (!strcmp(k, "set"))   { lua_pushcfunction(L, mm_set); return 1; }
    return luaL_error(L, "zt.midimacro has no field '%s' (try help('midimacro'))", k);
}
static int mm_newindex(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_MIDIMACRO);
    const char *k = luaL_checkstring(L, 2);
    midimacro *m = macro_at(i);
    if (!m) return luaL_error(L, "midimacro %d is empty", i);
    if (!strcmp(k, "name")) {
        const char *v = luaL_checkstring(L, 3);
        strncpy(m->name, v, ZTM_MIDIMACRONAME_MAXLEN - 1);
        m->name[ZTM_MIDIMACRONAME_MAXLEN - 1] = '\0';
        need_refresh++;
        return 0;
    }
    return luaL_error(L, "zt.midimacro has no writable field '%s'", k);
}
static int mm_tostring(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_MIDIMACRO);
    midimacro *m = macro_at(i);
    lua_pushfstring(L, "zt.midimacro(%d) <%s>", i, m ? m->name : "?");
    return 1;
}
static int l_midimacro(lua_State *L)
{
    int i = (int)luaL_checkinteger(L, 1);
    if (i < 0 || i >= ZTM_MAX_MIDIMACROS)
        return luaL_error(L, "midimacro index %d out of range (0..%d)", i, ZTM_MAX_MIDIMACROS - 1);
    // Slots start NULL; materialise on reference like the F4 editor does
    // (an empty macro stays out of saves/dispatch via isempty()).
    if (song && !song->midimacros[i]) song->midimacros[i] = new midimacro;
    int *p = (int *)lua_newuserdatauv(L, sizeof(int), 0);
    *p = i;
    luaL_setmetatable(L, ZT_MT_MIDIMACRO);
    return 1;
}

// ===========================================================================
// zt.arpeggio(i) — Shift-F4 arpeggio slots. name/speed/length/repeat_pos/
// num_cc (rw), .empty/.index; methods :pitch(step)/:set_pitch(step,offset)
// (offset is a signed semitone delta, nil = empty), :cc(lane)/:set_cc,
// :ccval(lane,step)/:set_ccval.
// ===========================================================================
static arpeggio *arp_at(int i)
{
    if (!song || i < 0 || i >= ZTM_MAX_ARPEGGIOS) return nullptr;
    return song->arpeggios[i];
}
static int arp_pitch(lua_State *L)    // :pitch(step) -> offset or nil
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    int s = (int)luaL_checkinteger(L, 2);
    arpeggio *a = arp_at(i);
    if (!a || s < 0 || s >= ZTM_ARPEGGIO_LEN || a->pitch[s] == ZTM_ARP_EMPTY_PITCH) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, (int)(short)a->pitch[s]);
    return 1;
}
static int arp_set_pitch(lua_State *L) // :set_pitch(step, offset|nil)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    int s = (int)luaL_checkinteger(L, 2);
    arpeggio *a = arp_at(i);
    if (a && s >= 0 && s < ZTM_ARPEGGIO_LEN) {
        if (lua_isnoneornil(L, 3)) a->pitch[s] = ZTM_ARP_EMPTY_PITCH;
        else a->pitch[s] = (unsigned short)(short)luaL_checkinteger(L, 3);
        need_refresh++;
    }
    return 0;
}
static int arp_cc(lua_State *L)        // :cc(lane) -> CC number
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    int lane = (int)luaL_checkinteger(L, 2);
    arpeggio *a = arp_at(i);
    if (!a || lane < 0 || lane >= ZTM_ARPEGGIO_NUM_CC) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, a->cc[lane]);
    return 1;
}
static int arp_set_cc(lua_State *L)    // :set_cc(lane, ccnum)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    int lane = (int)luaL_checkinteger(L, 2);
    int v = (int)luaL_checkinteger(L, 3);
    arpeggio *a = arp_at(i);
    if (a && lane >= 0 && lane < ZTM_ARPEGGIO_NUM_CC) { a->cc[lane] = (unsigned char)v; need_refresh++; }
    return 0;
}
static int arp_ccval(lua_State *L)     // :ccval(lane, step) -> value or nil
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    int lane = (int)luaL_checkinteger(L, 2);
    int s = (int)luaL_checkinteger(L, 3);
    arpeggio *a = arp_at(i);
    if (!a || lane < 0 || lane >= ZTM_ARPEGGIO_NUM_CC || s < 0 || s >= ZTM_ARPEGGIO_LEN ||
        a->ccval[lane][s] == ZTM_ARP_EMPTY_CCVAL) { lua_pushnil(L); return 1; }
    lua_pushinteger(L, a->ccval[lane][s]);
    return 1;
}
static int arp_set_ccval(lua_State *L) // :set_ccval(lane, step, value|nil)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    int lane = (int)luaL_checkinteger(L, 2);
    int s = (int)luaL_checkinteger(L, 3);
    arpeggio *a = arp_at(i);
    if (a && lane >= 0 && lane < ZTM_ARPEGGIO_NUM_CC && s >= 0 && s < ZTM_ARPEGGIO_LEN) {
        if (lua_isnoneornil(L, 4)) a->ccval[lane][s] = ZTM_ARP_EMPTY_CCVAL;
        else a->ccval[lane][s] = (unsigned char)luaL_checkinteger(L, 4);
        need_refresh++;
    }
    return 0;
}
static int arp_index(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    const char *k = luaL_checkstring(L, 2);
    if (!strcmp(k, "index")) { lua_pushinteger(L, i); return 1; }
    arpeggio *a = arp_at(i);
    if (!a) return luaL_error(L, "arpeggio %d is empty", i);
    if (!strcmp(k, "name"))       { lua_pushstring(L, a->name); return 1; }
    if (!strcmp(k, "speed"))      { lua_pushinteger(L, a->speed); return 1; }
    if (!strcmp(k, "length"))     { lua_pushinteger(L, a->length); return 1; }
    if (!strcmp(k, "repeat_pos")) { lua_pushinteger(L, a->repeat_pos); return 1; }
    if (!strcmp(k, "num_cc"))     { lua_pushinteger(L, a->num_cc); return 1; }
    if (!strcmp(k, "empty"))      { lua_pushboolean(L, a->isempty()); return 1; }
    if (!strcmp(k, "pitch"))      { lua_pushcfunction(L, arp_pitch); return 1; }
    if (!strcmp(k, "set_pitch"))  { lua_pushcfunction(L, arp_set_pitch); return 1; }
    if (!strcmp(k, "cc"))         { lua_pushcfunction(L, arp_cc); return 1; }
    if (!strcmp(k, "set_cc"))     { lua_pushcfunction(L, arp_set_cc); return 1; }
    if (!strcmp(k, "ccval"))      { lua_pushcfunction(L, arp_ccval); return 1; }
    if (!strcmp(k, "set_ccval"))  { lua_pushcfunction(L, arp_set_ccval); return 1; }
    return luaL_error(L, "zt.arpeggio has no field '%s' (try help('arpeggio'))", k);
}
static int arp_newindex(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    const char *k = luaL_checkstring(L, 2);
    arpeggio *a = arp_at(i);
    if (!a) return luaL_error(L, "arpeggio %d is empty", i);
    if (!strcmp(k, "name")) {
        const char *v = luaL_checkstring(L, 3);
        strncpy(a->name, v, ZTM_ARPEGGIONAME_MAXLEN - 1);
        a->name[ZTM_ARPEGGIONAME_MAXLEN - 1] = '\0';
        need_refresh++; return 0;
    }
    int v = (int)luaL_checkinteger(L, 3);
    if (!strcmp(k, "speed"))      { a->speed = v < 1 ? 1 : v; need_refresh++; return 0; }
    if (!strcmp(k, "length"))     { if (v < 0) v = 0; if (v > ZTM_ARPEGGIO_LEN) v = ZTM_ARPEGGIO_LEN; a->length = v; need_refresh++; return 0; }
    if (!strcmp(k, "repeat_pos")) { a->repeat_pos = v; need_refresh++; return 0; }
    if (!strcmp(k, "num_cc"))     { if (v < 0) v = 0; if (v > ZTM_ARPEGGIO_NUM_CC) v = ZTM_ARPEGGIO_NUM_CC; a->num_cc = v; need_refresh++; return 0; }
    return luaL_error(L, "zt.arpeggio has no writable field '%s'", k);
}
static int arp_tostring(lua_State *L)
{
    int i = *(int *)luaL_checkudata(L, 1, ZT_MT_ARPEGGIO);
    arpeggio *a = arp_at(i);
    lua_pushfstring(L, "zt.arpeggio(%d) <%s> len=%d", i, a ? a->name : "?", a ? a->length : 0);
    return 1;
}
static int l_arpeggio(lua_State *L)
{
    int i = (int)luaL_checkinteger(L, 1);
    if (i < 0 || i >= ZTM_MAX_ARPEGGIOS)
        return luaL_error(L, "arpeggio index %d out of range (0..%d)", i, ZTM_MAX_ARPEGGIOS - 1);
    // Slots start NULL; materialise on reference like the Shift-F4 editor
    // (an empty arpeggio stays out of saves/dispatch via isempty()).
    if (song && !song->arpeggios[i]) song->arpeggios[i] = new arpeggio;
    int *p = (int *)lua_newuserdatauv(L, sizeof(int), 0);
    *p = i;
    luaL_setmetatable(L, ZT_MT_ARPEGGIO);
    return 1;
}

// ---- file load / save -----------------------------------------------------
static int l_load(lua_State *L)   // zt.load("song.zt") -> ok
{
    const char *path = luaL_checkstring(L, 1);
    if (!song) { lua_pushboolean(L, 0); return 1; }
    int rc = song->load((char *)path);
    need_refresh++;
    lua_pushboolean(L, rc >= 0);
    return 1;
}
static int l_save(lua_State *L)   // zt.save("song.zt" [, compressed=true]) -> ok
{
    const char *path = luaL_checkstring(L, 1);
    int compressed = lua_isnoneornil(L, 2) ? 1 : (lua_toboolean(L, 2) ? 1 : 0);
    if (!song) { lua_pushboolean(L, 0); return 1; }
    // zt_module::save() writes to this->filename and ignores its fn arg, so
    // point the song's filename at the requested path first (this is also
    // the "save as" behaviour: the song adopts the new filename).
    strncpy((char *)song->filename, path, ZTM_FILENAME_MAXLEN - 1);
    song->filename[ZTM_FILENAME_MAXLEN - 1] = '\0';
    int rc = song->save(NULL, compressed);
    lua_pushboolean(L, rc >= 0);
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

void ZtLuaEngine::fire(const char *event, long arg, bool has_arg)
{
    if (!L || !event) return;
    lua_getfield(L, LUA_REGISTRYINDEX, "zt_notifiers");   // notifiers table
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, event);                           // notifiers[event]
    if (!lua_istable(L, -1)) { lua_pop(L, 2); return; }
    int n = (int)lua_rawlen(L, -1);
    for (int i = 1; i <= n; i++) {
        lua_rawgeti(L, -1, i);                            // fn
        if (lua_isfunction(L, -1)) {
            int nargs = 0;
            if (has_arg) { lua_pushinteger(L, (lua_Integer)arg); nargs = 1; }
            if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
                const char *err = lua_tostring(L, -1);
                char msg[LUA_CONSOLE_LINE_LEN];
                snprintf(msg, sizeof(msg), "[notifier:%s] %s", event, err ? err : "?");
                print_line(msg);
                lua_pop(L, 1);                            // pop error
            }
        } else {
            lua_pop(L, 1);                                // not a function
        }
    }
    lua_pop(L, 2);                                        // list, table
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

    // Notifier registry: a table  event-name -> { fn1, fn2, ... }  kept in
    // the Lua registry so zt.on()/zt.off()/fire() can find it.
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "zt_notifiers");

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
        "    song = 'zt.song  properties (rw): bpm, tpb, name, message, cur_pattern, cur_track, cur_row, cur_instrument',\n"
        "    instrument = 'zt.instrument(i)  props (rw): name, channel, device, transpose, bank, volume, global_volume, default_length, flags, ccizer_bank; ro: index  (i defaults to current)',\n"
        "    pattern = 'zt.pattern(p)  props: length(rw), index, empty;  methods: :note(track,row), :set_note(track,row,note[,inst[,vol]]), :track(t)  (p defaults to current)',\n"
        "    track = 'zt.track(t[,p])  props: index, pattern, muted(rw);  methods: :note(row), :set_note(row,note[,inst[,vol]])  (defaults to current track/pattern)',\n"
        "    transport = 'zt.transport  prop: playing;  methods: :play(), :stop(), :play_pattern(), :panic()',\n"
        "    cell = 'zt.cell(track,row[,pat]) / pattern:cell(t,r) / track:cell(r)  props (rw): note, instrument, volume, length, effect, effect_data;  ro: name, empty, pattern, track, row;  method :clear()',\n"
        "    orders = 'zt.orders  array: zt.orders[i] = pattern (rw), .count, #zt.orders;  pattern markers zt.BREAK / zt.SKIP',\n"
        "    midimacro = 'zt.midimacro(i)  props: name (rw), empty, syx, index;  methods :get(step), :set(step,value)  (tokens zt.MACRO_END / zt.MACRO_PARAM)',\n"
        "    arpeggio = 'zt.arpeggio(i)  props (rw): name, speed, length, repeat_pos, num_cc;  methods :pitch(s)/:set_pitch(s,off), :cc(l)/:set_cc(l,n), :ccval(l,s)/:set_ccval(l,s,v)',\n"
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
        "      print('  ' .. objdocs.cell)\n"
        "      print('  ' .. objdocs.transport)\n"
        "      print('  ' .. objdocs.orders)\n"
        "      print('  ' .. objdocs.midimacro)\n"
        "      print('  ' .. objdocs.arpeggio)\n"
        "      print('Helpers: zt.note_name(60)->\"C-5\", zt.note_value(\"C-5\")->60 ; zt.load(path), zt.save(path)')\n"
        "      print('Notifiers: zt.on(event,fn) / zt.off(event) / zt.fire(event[,arg]).  events: idle, play, stop, row')\n"
        "      print('Consts: zt.MAX_PATTERNS/TRACKS/INSTRUMENTS/ORDERS/MIDIMACROS/ARPEGGIOS, NOTE_*, MIDDLE_C, BREAK, SKIP, MACRO_END/PARAM')\n"
        "      print('  e.g.  zt.song.bpm=140   zt.cell(0,0).note = zt.note_value(\"C-5\")   zt.orders[0]=2   zt.transport:play()')\n"
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

    luaL_newmetatable(L, ZT_MT_CELL);
    lua_pushcfunction(L, cell_index);     lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, cell_newindex);  lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, cell_tostring);  lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_ORDERS);
    lua_pushcfunction(L, orders_index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, orders_newindex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, orders_len);      lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, orders_tostring); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_MIDIMACRO);
    lua_pushcfunction(L, mm_index);     lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, mm_newindex);  lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, mm_tostring);  lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    luaL_newmetatable(L, ZT_MT_ARPEGGIO);
    lua_pushcfunction(L, arp_index);    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, arp_newindex); lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, arp_tostring); lua_setfield(L, -2, "__tostring");
    lua_pop(L, 1);

    // Constructors: zt.instrument(i) / zt.pattern(p) / zt.track(t [,p]) /
    // zt.cell(track,row[,pattern]) + the note-name helpers.
    lua_pushcfunction(L, l_instrument);   lua_setfield(L, -2, "instrument");
    lua_pushcfunction(L, l_pattern);      lua_setfield(L, -2, "pattern");
    lua_pushcfunction(L, l_track);        lua_setfield(L, -2, "track");
    lua_pushcfunction(L, l_cell);         lua_setfield(L, -2, "cell");
    lua_pushcfunction(L, l_midimacro);    lua_setfield(L, -2, "midimacro");
    lua_pushcfunction(L, l_arpeggio);     lua_setfield(L, -2, "arpeggio");
    lua_pushcfunction(L, l_note_name);    lua_setfield(L, -2, "note_name");
    lua_pushcfunction(L, l_note_value);   lua_setfield(L, -2, "note_value");
    lua_pushcfunction(L, l_load);         lua_setfield(L, -2, "load");
    lua_pushcfunction(L, l_save);         lua_setfield(L, -2, "save");
    lua_pushcfunction(L, l_on);           lua_setfield(L, -2, "on");
    lua_pushcfunction(L, l_off);          lua_setfield(L, -2, "off");
    lua_pushcfunction(L, l_fire);         lua_setfield(L, -2, "fire");

    // Singletons: zt.song, zt.transport, zt.orders.
    lua_newuserdatauv(L, 1, 0);
    luaL_setmetatable(L, ZT_MT_SONG);
    lua_setfield(L, -2, "song");
    lua_newuserdatauv(L, 1, 0);
    luaL_setmetatable(L, ZT_MT_TRANSPORT);
    lua_setfield(L, -2, "transport");
    lua_newuserdatauv(L, 1, 0);
    luaL_setmetatable(L, ZT_MT_ORDERS);
    lua_setfield(L, -2, "orders");

    // Constants: sizes, note sentinels, order-list markers, middle C.
    lua_pushinteger(L, ZTM_MAX_PATTERNS);  lua_setfield(L, -2, "MAX_PATTERNS");
    lua_pushinteger(L, ZTM_MAX_TRACKS);    lua_setfield(L, -2, "MAX_TRACKS");
    lua_pushinteger(L, ZTM_MAX_INSTS);     lua_setfield(L, -2, "MAX_INSTRUMENTS");
    lua_pushinteger(L, ZTM_ORDERLIST_LEN); lua_setfield(L, -2, "MAX_ORDERS");
    lua_pushinteger(L, 0x80);              lua_setfield(L, -2, "NOTE_EMPTY");
    lua_pushinteger(L, 0x81);              lua_setfield(L, -2, "NOTE_CUT");
    lua_pushinteger(L, 0x82);              lua_setfield(L, -2, "NOTE_OFF");
    lua_pushinteger(L, 60);                lua_setfield(L, -2, "MIDDLE_C");
    lua_pushinteger(L, 0x100);             lua_setfield(L, -2, "BREAK");
    lua_pushinteger(L, 0x101);             lua_setfield(L, -2, "SKIP");
    lua_pushinteger(L, ZTM_MAX_MIDIMACROS);lua_setfield(L, -2, "MAX_MIDIMACROS");
    lua_pushinteger(L, ZTM_MAX_ARPEGGIOS); lua_setfield(L, -2, "MAX_ARPEGGIOS");
    lua_pushinteger(L, ZTM_MIDIMAC_END);   lua_setfield(L, -2, "MACRO_END");
    lua_pushinteger(L, ZTM_MIDIMAC_PARAM1);lua_setfield(L, -2, "MACRO_PARAM");

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
