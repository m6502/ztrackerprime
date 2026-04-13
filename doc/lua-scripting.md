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

## API Reference

All functions are in the `zt` table:

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
