---
name: ztrackerprime
description: zTracker Prime (m6502/ztrackerprime) contributor skill — orienting context, key files, and pointers to deeper reference material.
domain: repository-specific
source_repo: https://github.com/m6502/ztrackerprime
source_platform: github
tags: [cpp, sdl3, tracker, midi, cross-platform]
triggers:
  keywords:
    primary: [ztracker, ztrackerprime, zt, zTracker]
    secondary: [m6502, tracker, MIDI tracker, impulse tracker, SDL3 tracker]
---

# zTracker Prime Development Skill

## What this is

**zTracker Prime** (m6502/ztrackerprime) is Manuel Montoto's maintained fork of zTracker — an Impulse-Tracker-inspired MIDI tracker originally by Christopher Micali (2000–2001) with contributions from Daniel Kahlin. SDL 3, cross-platform (Windows XP through 11, macOS, Linux). C++17, CMake.

**MIDI-only.** No sample playback, no audio mixing. Pattern data drives MIDI Out events.

## Build

```sh
mkdir -p build && cd build
cmake -G "Unix Makefiles" ..   # or Ninja, MSVC, MinGW
make -j8                       # → build/Program/zt(.exe) or build/Program/zt.app
ctest --output-on-failure      # unit-test harness (Linux CI runs this)
```

`extlibs/` (libpng + zlib + lua) must be populated; CI fetches them from upstream tarballs (see `.github/workflows/build.yml`). SDL3 from Homebrew on macOS (`brew install sdl3`).

`ZT_BUILD_TESTS` CMake option (default ON) controls whether `tests/` builds. Production binaries are unaffected.

## Launching the app on macOS

`open <path>/zt.app` (or `open <path>/zt`) — gives the app its own session. Avoid `./zt &` from a non-interactive shell; it can exit silently when stdin is detached.

`scripts/zt-screenshot.sh` (macOS) launches zt.app and captures F1/F2/F3/F11/F12/Ctrl+F12 via `screencapture -R` + AppleScript. Output at `/tmp/zt-shots/`. Use it to self-verify layout changes.

## Key files

| File | Purpose |
|---|---|
| `src/main.cpp` | Entry, main loop, key dispatch, `switch_page`, CLI parser |
| `src/CUI_Patterneditor.cpp` | Pattern editor (F2) + 10+ pattern operations + Ctrl+Shift+§ CC drawmode |
| `src/CUI_Midimacroeditor.cpp` / `CUI_Arpeggioeditor.cpp` | F4 / Shift+F4 editors |
| `src/CUI_KeyBindings.cpp` | Unified Shortcuts & MIDI Mappings page (**Shift+F2** — moved from Shift+F3 to free that slot) |
| `src/CUI_CcConsole.cpp` | CC Console (**Shift+F3**) — Paketti CCizer file load + sliders/knobs send-out + MIDI Learn |
| `src/CUI_SysExLibrarian.cpp` | SysEx Librarian (**Shift+F5**) — `.syx` file send + auto-capture of incoming SysEx |
| `src/CUI_*.cpp` | Other pages (Sysconfig, Songconfig, Help, InstEditor, MainMenu, Playsong, etc.) |
| `src/CUI_Page.{h,cpp}` | Base class for pages |
| `src/UserInterface.{h,cpp}` | Widget classes — `CheckBox`, `ValueSlider`, `TextInput`, `Frame`, `Button`, `TextBox`, `ListBox`, `MidiOutDeviceOpener`, `SkinSelector`, `VUPlay`. Fix rendering bugs HERE, not in callers. |
| `src/keybuffer.{h,cpp}` | Key state (KS_ALT/KS_CTRL/KS_META/KS_SHIFT) + `KS_HAS_ALT` macro. `ZT_TEST_NO_SDL` guard for SDL-free unit tests. |
| `src/font.cpp` | `print()` / `printtitle()` / `printBG()` drawing primitives |
| `src/zt.h` | STATE_/CMD_ enums, page globals (`UIP_CcConsole`, `UIP_SysExLibrarian`, ...), layout macros, `g_cc_drawmode` |
| `src/conf.{h,cpp}` | zt.conf parsing, `zt_config_globals`. Keys include `ccizer_folder`, `syx_folder` |
| `src/module.{h,cpp}` | Song data (patterns, tracks, events, instruments, arpeggios, midimacros). `instrument` carries `ccizer_bank` (per-instrument Paketti CCizer file). |
| `src/playback.{h,cpp}` | MIDI output timing, R-effect arpeggio + Z-effect macro dispatch. **Z on a `*.syx`-named midimacro** dispatches the file as SysEx (see `sysex_macro.h`). |
| `src/winmm_compat.h` | Cross-platform MIDI shim. Exposes `midiOut*` (WinMM-style API) on Win/macOS/Linux. Adds `zt_midi_out_sysex(handle, bytes, len)` for SysEx send. macOS `zt_coremidi_read_proc` parses incoming `F0..F7` into `zt_sysex_inq_push`. |
| `src/sysex_inq.{h,cpp}` | Process-wide receive queue for incoming SysEx (16 slots × 8 KB; overflow drops oldest). `std::mutex` protected. |
| `src/sysex_macro.{h,cpp}` | Helpers for the `*.syx`-named-midimacro convention: predicate, path resolution against `syx_folder`, file read with `F0..F7` framing check. |
| `src/ccizer.{h,cpp}` | CCizer parser, `.cc-view` slider/knob sidecar, `.cc-midi` MIDI Learn sidecar, folder scan, MIDI byte builder, `zt_ccizer_current_file()` (read by Pattern Editor for friendly slot names in CC drawmode status). |
| `src/preset_data.h` | F4/Shift+F4 preset arrays + pure apply functions |
| `src/preset_selector.h` | Pure listbox decision logic (click / arrow / Space / P-cycle) |
| `src/page_sync.h` | F4/Shift+F4 widget↔data sync helpers |
| `src/save_key_dispatch.h` | Global Ctrl-S dispatch decision |
| `src/editor_layout.h` | Shared character-grid constants for F4/Shift+F4 |
| `assets/ccizer/` | Bundled CCizer banks (sc88st, microfreak, minilogue, monologue, Prophet6, deepmind6, waldorf_blofeld, tb03, se02, pro800, polyend_*, midi_control_example) — copied from Paketti. README.md documents the format. |
| `assets/syx/` | Bundled SysEx files. `request_universal_inquiry.syx` = `F0 7E 7F 06 01 F7` for first-test handshakes. README.md documents the librarian workflow. |
| `tests/` | CTest unit-test executables — 8 suites: `presets`, `selector`, `page_sync`, `save_key`, `keybuffer`, `ccizer`, `sysex_inq`, `sysex_macro` |
| `doc/help.txt` | In-app F1 help — update when adding keybinds or CLI flags |
| `doc/CHANGELOG.txt` | Release notes, chronological |
| `.github/workflows/build.yml` | 5-platform CI matrix; Linux job runs `ctest` |

For the live PR queue: `gh pr list --repo m6502/ztrackerprime`.

## Page / shortcut map (current)

| Key | Page | Notes |
|---|---|---|
| F1 | Help | |
| F2 | Pattern Editor | F2 again from PE → PEParms |
| F3 | Instrument Editor | F3 row 11 shows `CCizer Bank: <basename>` (per-instrument bank) |
| F4 | MIDI Macro Editor | A macro whose name ends in `.syx` is dispatched as a SysEx file send by the playback engine; data grid is ignored (hint shown). |
| **Shift+F2** | **Shortcuts & MIDI Mappings** | Was Shift+F3 historically; moved 2026-04-30 to free the slot for CC Console. |
| **Shift+F3** | **CC Console** | Loads CCizer `.txt`, sliders/knobs send CC out, `L` to MIDI Learn, `B` to assign as current instrument's bank. |
| Shift+F4 | Arpeggio Editor | |
| F5 | Play Song | |
| **Shift+F5** | **SysEx Librarian** | Lists `.syx` files. Enter sends. Incoming SysEx auto-saved as `recv_<timestamp>.syx`. |
| F6 / F7 / F8 / F9 | Play Pattern / From Cursor / Stop / Panic | |
| F10 | Song Message Editor | |
| F11 | Song Configuration / Order | |
| F12 | System Configuration | Includes `CCizer Folder` text input (binds to `ccizer_folder` zt.conf key). |
| **Ctrl+Shift+§** | **CC drawmode toggle** | While ON, incoming CC writes `Sxxyy` and PB writes `Wxxxx` at the cursor row. Per-session UNDO_SAVE. `[CC DRAW]` badge shown top-right of Pattern Editor while active. |

## Coordinate system

- `row(N)` = `N * 8` pixels (Y offset). Named for character rows but returns pixels.
- `col(M)` = `M * 8` pixels (X offset).
- `print(x, y, str, color, drawable)` — x first, y second.
- Some legacy code calls `print(row(2), col(13), ...)` — this works only because `FONT_SIZE_X == FONT_SIZE_Y == 8`. Confusing; don't propagate.

Layout macros: `INITIAL_ROW=1`, `PAGE_TITLE_ROW_Y=9`, `TRACKS_ROW_Y=11`, `SPACE_AT_BOTTOM=8`.

## CCizer + SysEx architecture (2026-04-29 → 2026-04-30, PRs #78–#84)

Seven-PR feature stack landed end of April 2026 wiring Paketti-style CCizer banks and a complete SysEx librarian into zTracker. The architecture has three layers worth keeping in mind:

**1. Per-page (UI)**
- **Shift+F3 CC Console** (`CUI_CcConsole.cpp`) — file list of `.txt` CCizer banks, slot grid with sliders/knobs, V toggles slider↔knob, L to MIDI Learn (`(status, data1)` → slot), U to unbind, B to assign loaded file as current instrument's bank. `[/]` adjusts MIDI channel.
- **Shift+F5 SysEx Librarian** (`CUI_SysExLibrarian.cpp`) — file list of `.syx` files in `syx_folder`. Enter/S sends. Receive log auto-pops from `zt_sysex_inq` and writes new files as `recv_<TS>.syx`. R rescans, C clears log.
- **Pattern Editor CC drawmode** (`CUI_Patterneditor.cpp`) — Ctrl+Shift+§ flips `g_cc_drawmode`. While on, the existing MIDI-in pump in update() routes `0xB0..0xBF` / `0xE0..0xEF` to `update_event(...)` writing `S<cc><val>` / `W<14bit>`. Cursor advances by EditStep. Single UNDO_SAVE per session (file-static `g_cc_draw_session_snapped`).

**2. Process-wide**
- `g_cc_drawmode` (in `zt.h`) — toggle.
- `zt_ccizer_current_file()` (in `ccizer.h`) — last-loaded file pointer, so the Pattern Editor's drawmode status can show friendly slot names ("Cutoff = 92") instead of raw CC numbers.
- `zt_sysex_inq_*` (in `sysex_inq.h`) — ring buffer of incoming complete SysEx messages. Filled by the macOS CoreMIDI read_proc; drained by `CUI_SysExLibrarian::drain_recv` every update().
- `MidiOut->sendSysEx(dev, bytes, len)` — virtual on `OutputDevice`, default no-op. `MidiOutputDevice` impl asserts F0/F7 framing and calls `zt_midi_out_sysex` in `winmm_compat.h`. Backed by CoreMIDI `MIDIPacketListAdd` (256-byte chunks), Win `midiOutLongMsg`, ALSA `snd_seq_ev_set_sysex`.

**3. Persistence**
- `ccizer_folder` zt.conf key — folder of `.txt` files. Resolution: override → `<exe-dir>/ccizer` → macOS `Resources/ccizer` → Linux `share/zt/ccizer` → `~/.config/zt/ccizer`. F12 page exposes the text input.
- `syx_folder` zt.conf key — folder of `.syx` files. Resolution: override → `./syx` (next to binary or in `.app Resources`) → `~/.config/zt/syx`. Auto-created.
- `<file>.cc-view` sidecar — per-CCizer slot slider/knob choice. Written next to the `.txt`.
- `<file>.cc-midi` sidecar — per-CCizer slot MIDI Learn binding. Format: `<slot> <status hex> <data1 hex>`.
- `instrument.ccizer_bank[256]` — per-instrument CCizer file. Persisted via new optional `CCBN` chunk in `.zt`. Format: `short int count` then per entry `unsigned char inst_idx, short int length, bytes path`. **Fully backward+forward compatible**: old zTracker reading new `.zt` skips the unrecognized chunk via `readblock`'s built-in advance-past behavior.

**SysEx-by-filename convention** (`sysex_macro.h`): a midimacro whose `name` ends in `.syx` (case insensitive, requires len > 4) is dispatched by the playback engine's `Z` effect handler as a file send. The macro's data array is ignored. **No `.zt` format change** — the existing MMAC chunk already round-trips the name. Old zTracker sees an empty data array and silently does nothing — safe forward-compat.

**Cross-platform status** (as of 2026-04-30):
- SysEx send: works on macOS (CoreMIDI), Windows (WinMM), Linux (ALSA).
- SysEx receive: macOS only. Linux ALSA-in is itself stubbed in the platform layer; Windows WinMM-in receive parsing is on the followup TODO.

**Test suites added**:
- `tests/test_ccizer.cpp` — 51 checks: parser, comments/blanks, out-of-range, view sidecar round-trip, MIDI byte builder, folder resolution, dir scan.
- `tests/test_sysex_inq.cpp` — ~25 checks: empty / push-pop / FIFO / overflow / buffer-too-small / invalid input.
- `tests/test_sysex_macro.cpp` — ~20 checks: predicate, path resolution, valid/malformed/oversized/missing file read.

## INVARIANT: pages must bump `need_refresh` and use real widgets

`main.cpp` gates `ActivePage->draw()` on `need_refresh != 0`. Two non-negotiable rules for any page added or modified:

1. **Every key handler that mutates visible state ends with `need_refresh++;`** (and so does `enter()`, and any background pump that changes what should be on screen). Forget this and arrow keys silently mutate state without a redraw — the page looks frozen until some other path bumps the flag.

2. **Interactive elements are real widgets, not ASCII art.** If the user is supposed to click a slider, the slider is a `ValueSlider` registered with `UI->add_element`, drawn by `UI->draw(S)`, mutated by `ValueSlider::mouseupdate`, and absorbed by the page's `update()` reading the `changed` flag. **Never** ship `printBG` text bars as a stand-in for "we'll iterate later" — they are visually identical in a screenshot but completely unclickable, and the user finds out by trying. The pool-of-N pattern (pre-allocate `N` widgets in the ctor, position the visible window per-frame, hide off-screen ones with `xsize=0`) is the idiom for variable-count slot grids; `CUI_CcConsole.cpp::position_sliders` is a worked example.

Both rules were learnt the hard way during the CCizer / SysEx work — see PR #78 (text-bars regression) and the followup that wired real `ValueSlider`s + `need_refresh` bumps. Treat them as load-bearing for any new page.

## Deeper references (load on demand)

- [`references/foot-guns.md`](references/foot-guns.md) — recurring bug classes: ListBox mousestate fragility, widget-upstream rule, user-reported-inputs-as-ground-truth.
- [`references/keyjazz-slot.md`](references/keyjazz-slot.md) — Shift+F4 keyjazz UIE pattern.
- [`references/test-harness.md`](references/test-harness.md) — sdl_stub / module_stub / `ZT_TEST_NO_SDL`.
- [`references/cli-flags.md`](references/cli-flags.md) — `zt_parse_cli` + Copy Relaunch Command.
- [`references/keyboard.md`](references/keyboard.md) — Cmd→KS_META, `KS_HAS_ALT`, § remap, Cmd-Q strip.
- [`references/conventions.md`](references/conventions.md) — coding conventions, status-bar diagnostic idiom.
- [`recipes/add-test-suite.md`](recipes/add-test-suite.md) — adding a new CTest executable.
- [`glossary.md`](glossary.md) — abbreviation lookup (TPB, MMAC, ARPG, KS_/CMD_/STATE_, etc.).
- [`effects.md`](effects.md) — pattern-effect letter table.

## PR discipline

- Push topic branches to `origin`, open PRs against `master`, don't merge without maintainer approval.
- Keep PRs small and focused: 1 feature, ~30–300 LOC.
- `doc/CHANGELOG.txt` gets a section per PR.
- Tag AI-assisted commits: `Co-Authored-By: Claude <noreply@anthropic.com>` (or current model identifier).
- Don't duplicate content across PRs.

## Paketti as a feature reference

Paketti (a Renoise tool) is a reference for tracker quality-of-life features ported / portable to zTracker. Already landed: Replicate at Cursor, Clone Pattern, Humanize Velocities, MIDI Macro + Arpeggio editors. Future candidates: Fill-with-note-at-cursor, Find/Replace note, Transpose selection, Groove templates, Chord memory. zTracker is MIDI-only, so sample-editor features are N/A.

---

*Update this skill (and the relevant reference file) when the project's context shifts materially: new architecture area, new feature region, new recurring foot-gun.*
