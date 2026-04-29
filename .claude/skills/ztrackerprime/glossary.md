# Glossary

Codebase abbreviations, acronyms, and prefixes. Look here before guessing or grepping.

## Tracker / MIDI domain

| Term | Meaning |
|---|---|
| TPB | Ticks Per Beat — set by effect `Axx`. Pattern-row resolution. |
| BPM | Beats Per Minute — set by effect `Txx`. |
| MMAC | MIDI Macro — multi-byte MIDI message template, triggered by `Z` effect. Stored in the song's MMAC chunk. |
| ARPG | Arpeggio — note-offset sequence triggered by `R` effect. Stored in the song's ARPG chunk. |
| CC | Continuous Controller — MIDI control change. Sent via effect `Sxxyy` (xx = CC number, yy = value). |
| F8 | MIDI clock pulse byte. `--midi-clock` chases this to derive tempo. |
| Note Off | MIDI note-off message. Triggered by `§` (or remapped scancode). |
| Keyjazz | Audition mode — PC keyboard plays MIDI notes through the current instrument. |

## Code prefixes

| Prefix | Meaning |
|---|---|
| `KS_` | Key state bits — `KS_SHIFT`, `KS_CTRL`, `KS_ALT`, `KS_META`. |
| `KS_HAS_ALT(s)` | Macro: true when Alt or Cmd is held without Ctrl/Shift. |
| `STATE_` | Page-state enum in `zt.h` — `STATE_PEDIT`, `STATE_IEDIT`, etc. Tracks which page is active. |
| `CMD_` | Command enum (similar to STATE; some pages use it for action dispatch). |
| `CUI_` | Page class — `CUI_Patterneditor`, `CUI_Sysconfig`. Subclass of `CUI_Page`. |
| `UIP_` | Global page instance — `UIP_Patterneditor`, `UIP_Sysconfig`. Pointer to the singleton `CUI_*` page. |
| `ZTM_` | Constants from `module.h` — `ZTM_MAX_PATTERNS = 256`, etc. |
| `MAX_` | Limits — `MAX_TRACKS = 64`, `MAX_INSTS = 100`, `MAX_MIDI_INS = 64`. |
| `BUTTON_` | Mouse button event constants — `BUTTON_DOWN_LEFT`, `BUTTON_UP_LEFT`. |
| `MM_` | Main menu entry types — `MM_FUNC` (run a callback), `MM_PAGE` (jump to a page), etc. |
| `SDLK_` / `SDL_SCANCODE_` | SDL3 key symbol / scancode. |

## Layout macros (`zt.h`)

| Macro | Value |
|---|---|
| `INITIAL_ROW` | 1 |
| `PAGE_TITLE_ROW_Y` | `INITIAL_ROW + 8` = 9 |
| `TRACKS_ROW_Y` | `PAGE_TITLE_ROW_Y + 2` = 11 |
| `SPACE_AT_BOTTOM` | 8 (toolbar reserves bottom 8 char rows) |
| `CHARS_X` / `CHARS_Y` | Total character grid for current resolution |
| `FONT_SIZE_X` / `FONT_SIZE_Y` | 8 / 8 (pixels per character cell) |

## Page F-keys

| Key | Page |
|---|---|
| F1 | Help |
| F2 | Pattern Editor (F2 again → Pattern Settings popup) |
| F3 | Instrument Editor |
| F4 | MIDI Macro Editor (toggle to Arpeggio Editor when already on it) |
| Shift+F4 | Arpeggio Editor |
| F5 | Song Play |
| F6 | Pattern Play |
| F7 | Start at current row / Shift+F7: from current order |
| F8 | Stop playback |
| F9 | Panic / Shift+F9: hard panic |
| F10 | Song Message Editor |
| F11 | Song Configuration / Order Editor |
| F12 | System Configuration |
| Shift+Ctrl+F12 | Palette Editor |
| Alt+F12 | About |

## File chunks (`.zt` song format)

| Chunk | Contents |
|---|---|
| MMAC | MIDI Macros |
| ARPG | Arpeggios |
| (other chunks: see `module.cpp` save/load paths) | |
