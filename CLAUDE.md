# CLAUDE.md — contributor notes for AI assistants

This file is loaded automatically by Claude Code (and other AI agents that support `CLAUDE.md`) when working in this repo. It distills the conventions and gotchas that have been learned the hard way during zTracker Prime development. **Read this before making changes.**

---

## Repository overview

**zTracker Prime** (m6502/ztrackerprime) is Manuel Montoto's maintained fork of zTracker — an Impulse-Tracker-inspired MIDI tracker originally by Christopher Micali (2000–2001) with contributions from Daniel Kahlin. Manuel brought it to SDL 3 and cross-platform (Windows XP through 11, macOS, Linux).

- **Language / build:** C++17, CMake, SDL 3.
- **No audio:** zTracker is **MIDI-only**. There is no sample playback, no audio mixing. Pattern data drives MIDI Out events.
- **Cross-platform:** Linux (apt), macOS (Homebrew SDL3), Windows MSVC (SDL3 dev zip), Windows MinGW XP-compat. CI builds all five.

---

## Build

```sh
mkdir -p build && cd build
cmake -G "Unix Makefiles" ..   # or Ninja, MSVC, MinGW
make -j8                       # produces build/Program/zt(.exe) (or zt.app on macOS)
ctest --output-on-failure      # runs the unit-test harness
```

`extlibs/libpng` and `extlibs/zlib` must be populated; CI fetches them from upstream tarballs (see `.github/workflows/build.yml`). Lua is statically linked from `extlibs/lua/`.

CMake option `ZT_BUILD_TESTS` (default ON) controls whether the `tests/` subdirectory builds. Production builds are unaffected.

---

## Key files

| File | Purpose |
|---|---|
| `src/main.cpp` | Entry, main loop, key dispatch, `switch_page`, CLI parser. |
| `src/CUI_Patterneditor.cpp` | Pattern editor (F2). 10+ pattern operations. |
| `src/CUI_Midimacroeditor.cpp` | MIDI Macro editor (F4). |
| `src/CUI_Arpeggioeditor.cpp` | Arpeggio editor (Shift+F4). |
| `src/CUI_*.cpp` | Other pages: Sysconfig, Songconfig, Help, About, InstEditor, MainMenu, etc. |
| `src/UserInterface.{h,cpp}` | Widget classes — `CheckBox`, `ValueSlider`, `TextInput`, `Frame`, `Button`, `TextBox`, `ListBox`, `MidiOutDeviceOpener`, `SkinSelector`, `VUPlay`. **Fix rendering bugs in the widget class, not at every caller.** |
| `src/keybuffer.{h,cpp}` | Key buffer + KS_ALT/KS_CTRL/KS_META/KS_SHIFT state. `KS_HAS_ALT(s)` macro accepts ALT or macOS Cmd. Header has a `ZT_TEST_NO_SDL` guard so it can be compiled into SDL-free unit tests via `tests/sdl_stub.h`. |
| `src/font.cpp` | `print()` / `printtitle()` / `printBG()` drawing primitives. |
| `src/zt.h` | Kitchen-sink: `STATE_*` enum, `CMD_*` enum, page globals, layout macros (`TRACKS_ROW_Y`, `CHARS_Y`, `SPACE_AT_BOTTOM`, etc.). |
| `src/module.{h,cpp}` | Song data: patterns, tracks, events, instruments, arpeggios, midimacros. |
| `src/playback.{h,cpp}` | MIDI output timing, playback state, R-effect arpeggio + Z-effect macro dispatch. |
| `src/preset_data.h` | F4/Shift+F4 preset arrays + pure `*_apply_preset_to(struct*, idx)` apply functions. Header-only, SDL-free, exhaustively unit-tested. |
| `src/preset_selector.h` | Pure decision logic for the preset listbox (click / arrow / Space / P-cycle). Unit-tested. |
| `src/page_sync.h` | F4/Shift+F4 widget↔data sync helpers. Prevents the "preset reverts on next frame" class of bug. |
| `src/save_key_dispatch.h` | Global Ctrl-S dispatch (open save dialog / pass through / let page handle). Unit-tested. |
| `src/editor_layout.h` | Shared character-grid constants for F4/Shift+F4 editors. |
| `tests/` | CTest unit-test executables. 5 suites, 285+ checks. Linux CI runs them. |
| `doc/help.txt` | In-app F1 help. **Update this when adding keybinds or CLI flags.** |
| `doc/CHANGELOG.txt` | Release notes, chronological. |
| `.github/workflows/build.yml` | CI: 5-platform build matrix + Linux ctest run. |

---

## Coordinate system (read this; it has tripped us up multiple times)

- `row(N)` returns `N * FONT_SIZE_Y` = `N * 8` pixels. Despite the name, it's a **Y-pixel offset**.
- `col(M)` returns `M * FONT_SIZE_X` = `M * 8` pixels. Despite the name, it's an **X-pixel offset**.
- `print(x, y, str, color, drawable)` takes x first, y second.
- Some legacy code calls `print(row(2), col(13), ...)` — that's **x = row(2)** (16 px) and **y = col(13)** (104 px = character row 13). Confusing but works because `FONT_SIZE_X == FONT_SIZE_Y`.

Layout macros:
- `INITIAL_ROW = 1`
- `PAGE_TITLE_ROW_Y = INITIAL_ROW + 8 = 9` (title separator row)
- `TRACKS_ROW_Y = PAGE_TITLE_ROW_Y + 2 = 11`
- `SPACE_AT_BOTTOM = 8` (toolbar reserves the bottom 8 character rows)
- `CHARS_Y` = total visible character rows for current resolution

---

## Keyboard conventions

- **macOS Cmd** maps to `KS_META | KS_ALT`. Use `KS_HAS_ALT(kstate)` for "Alt or Cmd, but not Ctrl/Shift." Plain `kstate == KS_ALT` fails on macOS because Cmd produces a compound state.
- **§ key on Finnish/EU ISO keyboards** is remapped from `SDL_SCANCODE_NONUSBACKSLASH` (and `INTERNATIONAL1`/`2`/`3`) to `SDLK_GRAVE` in `keyhandler()`. Plain § = Note Off; Shift+§ = drawmode toggle.
- macOS `Cmd-Q` is stripped from the auto-created NSApp menu so the Pattern Editor can use it as Transpose-Up. macOS `Cmd-W` is stripped similarly so it doesn't dismiss the SDL window.
- When adding shortcuts, **also update `doc/help.txt`**.

---

## CLI flags (see PR #68 / #69)

```
zt [options] [song.zt]

  -h, --help                  Show usage and exit.
      --list-midi-in          List MIDI input ports and exit.
      --midi-in <name|index>  Open the named MIDI input. Repeatable.
      --midi-clock <name|index>
                              Open + enable midi_in_sync +
                              midi_in_sync_chase_tempo (chase incoming F8).
```

Order-independent, accepts both `--flag value` and `--flag=value`, dash-count tolerant. First positional = .zt song.

The ESC menu has a "Copy Relaunch Command" entry that captures current state (open MIDI in ports, MIDI clock chase target, loaded song) and emits a one-line `zt ...` invocation to the system clipboard. Pairs with the CLI flags for stage / live / chat-handoff workflows.

---

## Test harness

`tests/` builds five CTest executables:

- `test_presets` — preset arrays + apply functions.
- `test_selector` — listbox click / arrow / Space / P-cycle decision logic.
- `test_page_sync` — apply→refresh→sync per-frame cycle.
- `test_save_key` — global Ctrl-S dispatch.
- `test_keybuffer` — real `KeyBuffer` from `src/keybuffer.cpp`, compiled under `ZT_TEST_NO_SDL` against `tests/sdl_stub.h`.

Run all: `ctest --test-dir build --output-on-failure`. CI runs them on the Linux job after every push.

When you fix a class of bug that has bitten more than once (preset apply, save-key dispatch, button-up consumption), extract the decision logic into a header (`preset_selector.h`, `save_key_dispatch.h`, `page_sync.h`, etc.) and add tests. Bug-class regression test scaffolding has paid off multiple times in the F4/Shift+F4 work.

---

## Recurring foot-guns

### ListBox subclass mousestate gotcha

`ListBox::mouseupdate` relies on `mousestate` being non-zero when `BUTTON_UP_LEFT` arrives, because `act++` is gated on it. Two ways this gets clobbered:

1. **Pre-switch reset.** `if (!bMouseIsDown) mousestate = 0;` at the **top** of `mouseupdate` clears mousestate BEFORE the switch sees BUTTON_UP. Result: the up event never increments `act`, `Keys.getkey()` is never called, the up event sits forever in the FIFO blocking every subsequent input. **The reset must run AFTER the switch.**

2. **Premature `ListBox::OnSelect(selected)` in a subclass override.** Base `ListBox::OnSelect` sets `mousestate = 0`. If a subclass override calls it during click handling (between BUTTON_DOWN and BUTTON_UP), mousestate gets cleared early — same freeze. **Don't call `ListBox::OnSelect` from a subclass; the BUTTON_UP_LEFT case clears mousestate itself.**

Symptom for the user: "click on widget freezes the UI; Cmd-Tab unfreezes it." The Cmd-Tab unfreeze is `SDL_EVENT_WINDOW_FOCUS_GAINED → Keys.flush()` (see `main.cpp::zt_handle_platform_window_event`).

### Widget-class fixes belong upstream

When a rendering or behavior bug appears on multiple pages with the same widget, fix it in `src/UserInterface.cpp` (the widget class), not in each `CUI_*.cpp` caller. If you find yourself editing the same property (`xsize`, `frame`, color) across more than two callers to fix the same visual problem, **stop and refactor**. The call sites are symptomatic; the widget's `::draw()` or constructor is the real bug.

Examples:
- **Default values in the constructor.** If every caller sets `cb->xsize = 5`, set it once in the constructor.
- **Cap or ignore caller-supplied values inside `::draw()`** when callers historically passed bad values.
- **Make a flag a no-op** if every caller's correct value is the same.
- **Sentinel cases stay in the caller.** A `MidiOutDeviceOpener` with custom `xsize`, a `BankSelectCheckBox` overriding click behaviour — those *are* per-caller.

### User-reported inputs are ground truth

When the user says "I pressed Ctrl-S", that IS what they pressed. Do not respond with "you were probably pressing Cmd-S" / "most likely cause is X" / "you might have meant Y". That's gaslighting — overwriting their observation with a hypothesis that's more convenient because the model can't account for the bug.

If the trace doesn't show how the reported input produces the symptom, the model is wrong, not the user. Allowed responses: "I'm stuck" + ask for more detail; "are you running the latest binary?"; insert a status_change diagnostic in the suspected branch.

---

## PR discipline

- **Keep PRs focused.** Manuel's preferred size: 1 feature, ~30–300 LOC. Large kitchen-sink PRs are tolerated when the body has a clear table of contents.
- Tag commits with `Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>` (or current model).
- Don't merge to master without Manuel's approval — open PRs and let him review/merge.
- `doc/CHANGELOG.txt` gets a section per PR.
- Don't duplicate content across PRs.
- Push branches directly to `origin` (no fork needed; Esa has write access).

---

## Common user behaviour (learned)

- **"Launch the app"** = visibly running on the desktop, not just a process ID. Use `open .../zt.app`.
- **Don't ask the user to test.** Run the binary yourself first and verify basic rendering, ideally with `scripts/zt-screenshot.sh`. Only ask the user when something requires their interactive judgement.
- They use **Finnish keyboard layout**. § is critical. Plain § = Note Off, Shift+§ = drawmode.
- **PR quality over PR count.** Finish one well before starting another.
- **Don't lecture.** Be specific, show diffs, explain *why*, then act.
- When the user pushes back ("that's not right"), they mean it. Stop, re-read literally, ask a clarifying question.
