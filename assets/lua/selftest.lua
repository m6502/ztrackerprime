-- selftest.lua — exercises every zTracker Lua API surface and reports
-- PASS/FAIL per check. Run it two ways:
--   * headless / CI:   zt --headless --lua-test
--   * in the console:  dofile("lua/selftest.lua")   (Ctrl+Alt+L)
-- It mutates the in-memory song (notes, bpm, arps, ...) and saves a temp
-- file, so run it on a scratch song. Sets the global SELFTEST_FAILURES
-- (0 = all good); the --lua-test runner uses it for the process exit code.

local fails, total = 0, 0
local function check(name, cond)
  total = total + 1
  if cond then
    print("PASS  " .. name)
  else
    fails = fails + 1
    print("FAIL  " .. name)
  end
end
local function eq(name, got, want)
  check(name .. "  (got " .. tostring(got) .. ", want " .. tostring(want) .. ")", got == want)
end

-- ── introspection helpers exist ──────────────────────────────────────
check("rprint exists", type(rprint) == "function")
check("oprint exists", type(oprint) == "function")
check("help exists",   type(help)   == "function")
check("dump alias",    dump == rprint)

-- ── constants ────────────────────────────────────────────────────────
eq("MAX_PATTERNS",    zt.MAX_PATTERNS,    256)
eq("MAX_TRACKS",      zt.MAX_TRACKS,      64)
eq("MAX_INSTRUMENTS", zt.MAX_INSTRUMENTS, 100)
eq("MAX_ORDERS",      zt.MAX_ORDERS,      256)
eq("MAX_MIDIMACROS",  zt.MAX_MIDIMACROS,  256)
eq("MAX_ARPEGGIOS",   zt.MAX_ARPEGGIOS,   256)
eq("NOTE_EMPTY",      zt.NOTE_EMPTY,      0x80)
eq("NOTE_CUT",        zt.NOTE_CUT,        0x81)
eq("NOTE_OFF",        zt.NOTE_OFF,        0x82)
eq("MIDDLE_C",        zt.MIDDLE_C,        60)
eq("BREAK",           zt.BREAK,           0x100)
eq("SKIP",            zt.SKIP,            0x101)
eq("MACRO_END",       zt.MACRO_END,       0x100)
eq("MACRO_PARAM",     zt.MACRO_PARAM,     0x101)

-- ── note name <-> value ──────────────────────────────────────────────
eq("note_name(60)",      zt.note_name(60),      "C-5")
eq("note_value('C-5')",  zt.note_value("C-5"),  60)
eq("note_value('F#3')",  zt.note_value("F#3"),  42)
eq("note roundtrip",     zt.note_value(zt.note_name(72)), 72)
eq("note_value off",     zt.note_value("off"),  0x82)
eq("note_value cut",     zt.note_value("cut"),  0x81)

-- ── flat functions still work ────────────────────────────────────────
zt.set_bpm(140) ; eq("flat get/set_bpm", zt.get_bpm(), 140)
zt.set_note(0, 0, 0, 60) ; eq("flat get/set_note", zt.get_note(0, 0, 0), 60)

-- ── zt.song properties ───────────────────────────────────────────────
zt.song.bpm = 150 ;          eq("song.bpm",  zt.song.bpm,  150)
zt.song.tpb = 6 ;            eq("song.tpb",  zt.song.tpb,  6)
zt.song.name = "selftest" ;  eq("song.name", zt.song.name, "selftest")
zt.song.cur_pattern = 1 ;    eq("song.cur_pattern", zt.song.cur_pattern, 1)
zt.song.cur_pattern = 0
zt.song.cur_track = 2 ;      eq("song.cur_track", zt.song.cur_track, 2)
zt.song.cur_instrument = 3 ; eq("song.cur_instrument", zt.song.cur_instrument, 3)
zt.song.message = "hi from selftest" ; eq("song.message", zt.song.message, "hi from selftest")

-- ── zt.instrument(i) ─────────────────────────────────────────────────
local ins = zt.instrument(1)
ins.name = "Lead" ;        eq("instrument.name",   ins.name, "Lead")
ins.channel = 9 ;          eq("instrument.channel", ins.channel, 9)
ins.device = 1 ;           eq("instrument.device",  ins.device, 1)
ins.transpose = -12 ;      eq("instrument.transpose", ins.transpose, -12)
ins.bank = 5 ;             eq("instrument.bank", ins.bank, 5)
ins.volume = 100 ;         eq("instrument.volume", ins.volume, 100)
ins.global_volume = 110 ;  eq("instrument.global_volume", ins.global_volume, 110)
ins.default_length = 24 ;  eq("instrument.default_length", ins.default_length, 24)
ins.ccizer_bank = "mf.txt"; eq("instrument.ccizer_bank", ins.ccizer_bank, "mf.txt")
eq("instrument.index", ins.index, 1)

-- ── zt.pattern(p) ────────────────────────────────────────────────────
local pat = zt.pattern(0)
pat.length = 64 ;          eq("pattern.length", pat.length, 64)
eq("pattern.index", pat.index, 0)
pat:set_note(1, 4, 62) ;   eq("pattern:note", pat:note(1, 4), 62)
check("pattern.empty false", pat.empty == false)
local pt = pat:track(1)
eq("pattern:track -> track", pt.index, 1)

-- ── zt.track(t) ──────────────────────────────────────────────────────
local trk = zt.track(3)
eq("track.index", trk.index, 3)
trk:set_note(2, 64) ;      eq("track:note", trk:note(2), 64)
trk.muted = true ;         eq("track.muted true", trk.muted, true)
trk.muted = false ;        eq("track.muted false", trk.muted, false)

-- ── zt.cell — every field, with per-field merge ──────────────────────
local c = zt.cell(0, 8)            -- track 0, row 8
c.note = zt.MIDDLE_C ;             eq("cell.note", c.note, 60)
c.instrument = 2 ;                 eq("cell.instrument", c.instrument, 2)
c.volume = 96 ;                    eq("cell.volume", c.volume, 96)
c.length = 4 ;                     eq("cell.length", c.length, 4)
c.effect = 0x53 ;                  eq("cell.effect", c.effect, 0x53)
c.effect_data = 0x1234 ;           eq("cell.effect_data", c.effect_data, 0x1234)
eq("cell.name", c.name, "C-5")
check("cell.empty false", c.empty == false)
-- setting one field must not clobber the others (merge)
c.volume = 64
eq("cell merge keeps note", c.note, 60)
eq("cell merge sets volume", c.volume, 64)
c:clear() ;                        check("cell:clear -> empty", c.empty == true)
eq("empty cell reads NOTE_EMPTY", c.note, zt.NOTE_EMPTY)

-- ── zt.orders ────────────────────────────────────────────────────────
zt.orders[0] = 2 ;                 eq("orders[0]", zt.orders[0], 2)
zt.orders[1] = 5 ;                 eq("orders[1]", zt.orders[1], 5)
zt.orders[2] = zt.BREAK
eq("orders.count", zt.orders.count, 2)
eq("#orders", #zt.orders, 2)

-- ── zt.transport ─────────────────────────────────────────────────────
zt.transport:stop()
check("transport.playing is bool", type(zt.transport.playing) == "boolean")
zt.transport:play_pattern()
check("transport playing after play", zt.transport.playing == true)
zt.transport:stop()
check("transport stopped", zt.transport.playing == false)

-- ── zt.midimacro(i) ──────────────────────────────────────────────────
local m = zt.midimacro(0)
m.name = "Bank" ;                  eq("midimacro.name", m.name, "Bank")
m:set(0, 0xB0) ; m:set(1, 0x20)
eq("midimacro:get", m:get(0), 0xB0)
check("midimacro not empty", m.empty == false)
m.name = "dump.syx" ;              check("midimacro.syx", m.syx == true)

-- ── zt.arpeggio(i) ───────────────────────────────────────────────────
local a = zt.arpeggio(0)
a.name = "up" ;                    eq("arpeggio.name", a.name, "up")
a.length = 3 ;                     eq("arpeggio.length", a.length, 3)
a.speed = 2 ;                      eq("arpeggio.speed", a.speed, 2)
a.repeat_pos = 0 ;                 eq("arpeggio.repeat_pos", a.repeat_pos, 0)
a.num_cc = 1 ;                     eq("arpeggio.num_cc", a.num_cc, 1)
a:set_pitch(0, 0) ; a:set_pitch(1, 4) ; a:set_pitch(2, 7)
eq("arpeggio:pitch", a:pitch(2), 7)
a:set_pitch(2, nil) ;              check("arpeggio:pitch nil empties", a:pitch(2) == nil)
a:set_cc(0, 74) ;                  eq("arpeggio:cc", a:cc(0), 74)
a:set_ccval(0, 0, 100) ;           eq("arpeggio:ccval", a:ccval(0, 0), 100)

-- ── file save / load roundtrip ───────────────────────────────────────
local tmp = (os.getenv("TMPDIR") or os.getenv("TEMP") or "/tmp") .. "/zt_selftest.zt"
zt.song.bpm = 171
check("save ok", zt.save(tmp) == true)
zt.song.bpm = 99
check("load ok", zt.load(tmp) == true)
eq("load restores bpm", zt.song.bpm, 171)
os.remove(tmp)

-- ── summary ──────────────────────────────────────────────────────────
print(string.format("=== %d/%d checks passed, %d failed ===", total - fails, total, fails))
SELFTEST_FAILURES = fails
return fails
