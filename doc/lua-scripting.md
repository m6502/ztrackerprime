# Lua Scripting Support in zTracker

## Overview

Lua 5.4.7 scripting has been integrated into zTracker (issue #1). The implementation follows the SchismTracker approach: Lua calls block the main thread. Buggy scripts that loop forever will freeze the app — this is intentional.

## Activation

Press **Ctrl+Alt+L** to open the Lua Console. Press **Esc** or **F2** to return to the Pattern Editor.

The key binding can be remapped via `zt.conf`:
```
key_switch_lua_console: ctrl+alt+l
```

## Lua Console

- Scrollback buffer (256 lines)
- Single-line text input with cursor
- **Enter** — execute current line
- **Up/Down** — browse input history (64 entries)
- **PgUp/PgDn** — scroll scrollback
- **Backspace** — delete character left of cursor
- **Delete** — clear entire input line
- **Tab** — completion. It advances by the longest common prefix; when the
  prefix can't advance and several candidates remain, it lists them once and
  **repeated Tab cycles** through them (the input line shows the current
  candidate). A unique match completes fully and appends `(` for functions.

## Everything in `zt.*` is a FUNCTION — call it with `()`

The single most common stumble: typing `zt.cur_instrument` (or
`print(zt.cur_instrument)`) prints `function: 0x...`. That's the function
*object*, not a value — you have to **call** it:

```lua
zt.cur_instrument()          --> 5       (the console prints return values)
print(zt.cur_instrument())   --> 5
zt.get_bpm()                 --> 138
```

The console flags this for you: a bare function result prints with a
`-- a function; add () to call it, or help()` hint.

## Introspection (Renoise-style helpers)

```lua
help()              -- list the whole zt.* API with signatures
help("cur_row")     -- one-line help for a single function
rprint(zt)          -- recursively dump a table (keys, values, nested) — cycle-safe
oprint(zt)          -- list one level: each key with its type
dump = rprint       -- alias
```

`rprint`/`oprint` mirror the Renoise/Paketti helpers of the same name, so
`rprint(zt)` lists the entire API and `rprint(_G)` walks the globals
(depth-limited so it won't hang).

## Notifiers (react to events)

Register Lua callbacks that fire from the main loop:

```lua
zt.on("row", function(r) zt.status("row " .. r) end)  -- playhead advanced
zt.on("play",  function() print("started") end)
zt.on("stop",  function() print("stopped") end)
zt.on("idle",  function() --[[ every frame; keep it cheap ]] end)
zt.off("row")                       -- remove all callbacks for an event
zt.fire("my_event", 42)             -- fire a custom (or built-in) event yourself
```

Built-in events: **`idle`** (every frame), **`play`** / **`stop`** (transport
transitions), **`row`** (arg = current playing row). Multiple callbacks per
event are allowed. A callback that errors prints to the console and the others
still run; a callback that loops forever stalls the app (same trade-off as any
Lua call here).

## Self-test (every feature, runnable)

`lua/selftest.lua` exercises the whole `zt.*` API with PASS/FAIL checks. Run it:

```sh
zt --headless --lua-test     # CI-friendly: prints PASS/FAIL, exits 0 (ok) / 1 (fail)
```
or in the console (Ctrl+Alt+L): `dofile("lua/selftest.lua")`. It also runs as the
`lua_api` test under `ctest`. The script is readable — it's the quickest way to
see exactly what every call does and returns.

## Object API (Renoise-style, dot properties)

In addition to the flat `zt.get_*/set_*` functions, there are proxy objects
with **dot-property access** (read *and* write — no parentheses):

```lua
-- the song (a singleton object)
zt.song.bpm                 --> 138
zt.song.bpm = 145           -- writes it (and reschedules playback)
zt.song.tpb
zt.song.name = "My Tune"
zt.song.cur_pattern, zt.song.cur_track, zt.song.cur_row, zt.song.cur_instrument

-- instruments (zt.instrument(i); i defaults to the current instrument)
local ins = zt.instrument(2)
print(ins.name, ins.channel, ins.device)
ins.name = "Bass"
-- properties: name, channel, device, transpose, bank, volume, patch, index

-- patterns (zt.pattern(p); p defaults to the current pattern)
local pat = zt.pattern(0)
pat.length                       -- property (read/write); also .index, .empty
pat:set_note(0, 0, 60)           -- :set_note(track, row, note [,inst [,vol]])
print(pat:note(0, 0))            --> 60   :note(track, row)
local trk = pat:track(0)         -- a track bound to this pattern

-- tracks (zt.track(t [,pattern]); default current track + current pattern)
local t = zt.track(0)
t.muted = true                   -- property (read/write); also .index, .pattern
t:set_note(0, 60)                -- :set_note(row, note [,inst [,vol]])
print(t:note(0))                 -- :note(row)

-- transport (singleton)
print(zt.transport.playing)      -- read-only bool
zt.transport:play()              -- :play() :stop() :play_pattern() :panic()

-- cells: FULL per-cell access (note, instrument, volume, length, effect, effect_data)
local c = zt.cell(0, 0)          -- zt.cell(track, row [,pattern]); also pat:cell(t,r), trk:cell(r)
c.note   = zt.note_value("C-5")  -- 60 in zTracker's octave convention (octave = note/12)
c.instrument = 2
c.volume = 100
c.effect, c.effect_data = 0x53, 0x1234
print(c.name, c.note, c.empty)   -- "C-5", 60, false ; .pattern/.track/.row are read-only
c:clear()                        -- empty the cell
-- empty cells read back sentinels: note 0x80 (zt.NOTE_EMPTY), inst 0xFF, vol 0x80

-- order list (the song's pattern sequence)
zt.orders[0] = 2                 -- play pattern 2 first
print(zt.orders[0], zt.orders.count, #zt.orders)
zt.orders[1] = zt.BREAK          -- 0x100 end-of-song (zt.SKIP = 0x101)

-- note names + constants
zt.note_name(60)                 --> "C-5"   (also "^^^" cut, "===" off, "..." empty)
zt.note_value("F#3")             --> 42
-- zt.MAX_PATTERNS (256), zt.MAX_TRACKS (64), zt.MAX_INSTRUMENTS (100), zt.MAX_ORDERS (256)
-- zt.NOTE_EMPTY (0x80), zt.NOTE_CUT (0x81), zt.NOTE_OFF (0x82), zt.MIDDLE_C (60)
-- zt.BREAK (0x100), zt.SKIP (0x101)

-- song message (the F10 text)
zt.song.message = "made with zTracker"   ;  print(zt.song.message)

-- more instrument fields (on the same object as #148)
local ins = zt.instrument(0)
ins.global_volume, ins.default_length, ins.flags = 110, 24, 0
ins.ccizer_bank = "microfreak.txt"

-- MIDI macros (the F4 slots)  -- referencing a slot creates it
local m = zt.midimacro(0)
m.name = "Bank LSB"              ; print(m.empty, m.syx)
m:set(0, 0xB0) ; m:set(1, 0x20) ; m:set(2, zt.MACRO_PARAM) ; m:set(3, zt.MACRO_END)

-- arpeggios (the Shift+F4 slots)
local a = zt.arpeggio(0)
a.name, a.length, a.speed, a.repeat_pos = "up", 3, 2, 0
a:set_pitch(0, 0) ; a:set_pitch(1, 4) ; a:set_pitch(2, 7)   -- semitone offsets (nil = empty)
a.num_cc = 1 ; a:set_cc(0, 74) ; a:set_ccval(0, 0, 100)

-- file load / save
zt.save("backup.zt")             -- zt.save(path [, compressed=true]) -> ok
-- zt.load("song.zt")            -- replaces the current song; returns ok
```

These are the layer to reach for when you have Renoise/Paketti muscle
memory. The flat `zt.get_*/set_*` functions still work; the objects are
just nicer. Note properties use dot access (`pat.length`, `cell.note`);
methods use the colon call (`pat:note(0,0)`, `cell:clear()`), Renoise-style.

### Example: generate a pattern from Lua

```lua
local pat = zt.pattern(0)
pat.length = 64
for row = 0, pat.length - 1, 4 do            -- a note every 4th row
  local c = pat:cell(0, row)
  c.note = zt.MIDDLE_C
  c.instrument = zt.song.cur_instrument
  c.volume = 96
end
zt.orders[0] = 0
zt.transport:play_pattern()
```

## API Reference

All functions are in the `zt` table (call them with `()`):

### Pattern Access
```lua
zt.get_note(pattern, track, row)           -> note_value
zt.set_note(pattern, track, row, note [, inst, vol])
zt.get_pattern_length(pattern)             -> length
zt.set_pattern_length(pattern, length)
```

### Playback
```lua
zt.play()           -- play song from current pattern
zt.stop()
zt.play_pattern()   -- loop current pattern
zt.get_bpm()        -> bpm
zt.set_bpm(bpm)
zt.get_tpb()        -> tpb
zt.set_tpb(tpb)
```

### Current State
```lua
zt.cur_pattern()    -> pattern_index
zt.cur_track()      -> track_index
zt.cur_row()        -> row_index
zt.cur_instrument() -> instrument_index
zt.set_cur_pattern(p)
zt.set_cur_row(r)
```

### MIDI
```lua
zt.midi_note_on(device, note, channel [, velocity])
zt.midi_note_off(device, note, channel)
```

### Status / Output
```lua
zt.status(message)   -- show message in status bar
zt.print(...)        -- print to Lua console scrollback
print(...)           -- also redirected to console scrollback
```

## Examples

Fill pattern 0 track 0 with C-4 (note 60) every other row:
```lua
for r = 0, zt.get_pattern_length(0)-1, 2 do
  zt.set_note(0, 0, r, 60)
end
```

Double the BPM:
```lua
zt.set_bpm(zt.get_bpm() * 2)
```

## Files Added

- `extlibs/lua/` — Lua 5.4.7 source (MIT license)
- `src/lua_engine.h` / `src/lua_engine.cpp` — engine + API bindings
- `src/CUI_LuaConsole.cpp` — REPL console page

## Changes to Existing Files

- `src/CMakeLists.txt` — added `lua_static` library, linked to `zt`
- `src/CUI_Page.h` — added `CUI_LuaConsole` class declaration
- `src/zt.h` — added `STATE_LUA_CONSOLE`, `CMD_SWITCH_LUA_CONSOLE`, extern `UIP_LuaConsole`
- `src/keybindings.h` — added `ZT_ACTION_SWITCH_LUA_CONSOLE`
- `src/keybindings.cpp` — added action name + default binding (Ctrl+Alt+L)
- `src/main.cpp` — init/shutdown, page allocation, key handler, CMD dispatch
