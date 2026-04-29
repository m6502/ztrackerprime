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
| `src/CUI_Patterneditor.cpp` | Pattern editor (F2) + 10+ pattern operations |
| `src/CUI_Midimacroeditor.cpp` / `CUI_Arpeggioeditor.cpp` | F4 / Shift+F4 editors |
| `src/CUI_*.cpp` | Other pages (Sysconfig, Songconfig, Help, InstEditor, MainMenu, Playsong, etc.) |
| `src/CUI_Page.{h,cpp}` | Base class for pages |
| `src/UserInterface.{h,cpp}` | Widget classes — `CheckBox`, `ValueSlider`, `TextInput`, `Frame`, `Button`, `TextBox`, `ListBox`, `MidiOutDeviceOpener`, `SkinSelector`, `VUPlay`. Fix rendering bugs HERE, not in callers. |
| `src/keybuffer.{h,cpp}` | Key state (KS_ALT/KS_CTRL/KS_META/KS_SHIFT) + `KS_HAS_ALT` macro. `ZT_TEST_NO_SDL` guard for SDL-free unit tests. |
| `src/font.cpp` | `print()` / `printtitle()` / `printBG()` drawing primitives |
| `src/zt.h` | STATE_/CMD_ enums, page globals, layout macros |
| `src/conf.{h,cpp}` | zt.conf parsing, `zt_config_globals` |
| `src/module.{h,cpp}` | Song data (patterns, tracks, events, instruments, arpeggios, midimacros) |
| `src/playback.{h,cpp}` | MIDI output timing, R-effect arpeggio + Z-effect macro dispatch |
| `src/preset_data.h` | F4/Shift+F4 preset arrays + pure apply functions |
| `src/preset_selector.h` | Pure listbox decision logic (click / arrow / Space / P-cycle) |
| `src/page_sync.h` | F4/Shift+F4 widget↔data sync helpers |
| `src/save_key_dispatch.h` | Global Ctrl-S dispatch decision |
| `src/editor_layout.h` | Shared character-grid constants for F4/Shift+F4 |
| `tests/` | CTest unit-test executables (preset / selector / page-sync / save-key / keybuffer) |
| `doc/help.txt` | In-app F1 help — update when adding keybinds or CLI flags |
| `doc/CHANGELOG.txt` | Release notes, chronological |
| `.github/workflows/build.yml` | 5-platform CI matrix; Linux job runs `ctest` |

For the live PR queue: `gh pr list --repo m6502/ztrackerprime`.

## Coordinate system

- `row(N)` = `N * 8` pixels (Y offset). Named for character rows but returns pixels.
- `col(M)` = `M * 8` pixels (X offset).
- `print(x, y, str, color, drawable)` — x first, y second.
- Some legacy code calls `print(row(2), col(13), ...)` — this works only because `FONT_SIZE_X == FONT_SIZE_Y == 8`. Confusing; don't propagate.

Layout macros: `INITIAL_ROW=1`, `PAGE_TITLE_ROW_Y=9`, `TRACKS_ROW_Y=11`, `SPACE_AT_BOTTOM=8`.

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
