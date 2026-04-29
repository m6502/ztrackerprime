---
name: ztrackerprime
description: zTracker Prime (m6502/ztrackerprime) contributor skill — context for zTracker development, conventions, recurring foot-guns, and PR workflow.
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

## Repository Overview

**zTracker Prime** (m6502/ztrackerprime) is Manuel Montoto's maintained fork of zTracker — an Impulse-Tracker-inspired MIDI tracker originally by Christopher Micali (2000–2001) with contributions from Daniel Kahlin. Manuel brought it to SDL 3 and cross-platform (Windows XP through 11, macOS, Linux).

- **Language / build:** C++17, CMake, SDL 3.
- **No audio:** zTracker is **MIDI-only**. There is no sample playback, no audio mixing. Pattern data drives MIDI Out events.
- **Cross-platform:** Linux (apt), macOS (Homebrew SDL3), Windows MSVC (SDL3 dev zip), Windows MinGW XP-compat. CI builds all five.

## Collaboration model

- Push topic branches to `origin` and open PRs against `master`. Don't merge without maintainer approval.
- Manuel reviews async, sometimes in batches. Expect multi-day cycles.

## Build system

CMake. `extlibs/` (libpng + zlib + lua) must be populated; CI fetches them from upstream tarballs (see `.github/workflows/build.yml`).

```sh
mkdir -p build && cd build
cmake -G "Unix Makefiles" ..   # or Ninja, MSVC, MinGW
make -j8                       # produces build/Program/zt(.exe) (or zt.app on macOS)
ctest --output-on-failure      # runs the unit-test harness
```

CMake option `ZT_BUILD_TESTS` (default ON) controls whether the `tests/` subdirectory builds. Production builds are unaffected.

SDL3 comes from Homebrew on macOS (`brew install sdl3`).

## Launching the app on macOS

A plain binary launched from a backgrounded shell can exit silently when stdin is detached. Prefer:
- `open <path>/zt.app` (for the .app bundle build)
- `open <path>/zt` (raw binary, opens via Finder)
- Run from Terminal.app directly

Avoid `./zt &` from a non-interactive shell — it can exit with code 0 without rendering. `open` gives the app its own session.

## Screenshot automation (macOS dev tool)

`scripts/zt-screenshot.sh` launches zt.app and captures F1/F2/F3/F11/F12/Ctrl+F12 via `screencapture -R` using AppleScript to read window position+size. Output at `/tmp/zt-shots/`. macOS-only.

Use it to self-verify layout changes rather than asking the user to re-screenshot every iteration.

## Key files

| File | Purpose |
|---|---|
| `src/main.cpp` | Entry, main loop, key dispatch, video mode, `switch_page`, CLI parser (`zt_parse_cli` + `ZtCliArgs`) |
| `src/CUI_Patterneditor.cpp` | Pattern editor page + 10+ pattern operations |
| `src/CUI_*.cpp` | Other pages (Sysconfig, Songconfig, Help, InstEditor, Config, MIDI Macro, Arpeggio, etc.) |
| `src/keybuffer.{h,cpp}` | Key state (KS_ALT, KS_CTRL, KS_META, KS_SHIFT) + KS_HAS_ALT macro. Use `ZT_TEST_NO_SDL` to compile against `tests/sdl_stub.h` for unit tests. |
| `src/font.cpp` | `print()` / `printtitle()` / `printBG()` drawing primitives |
| `src/UserInterface.{h,cpp}` | **Widget classes**: `CheckBox`, `ValueSlider`, `TextInput`, `Frame`, `Button`, `TextBox`, `ListBox`, `MidiOutDeviceOpener`, `SkinSelector`, `VUPlay`. Each has its own `::draw(Drawable*, int active)`. Fix rendering bugs HERE, not in callers. |
| `src/zt.h` | STATE_ enum, CMD_ enum, page globals, layout macros |
| `src/conf.{h,cpp}` | zt.conf parsing, zt_config_globals struct |
| `src/module.{h,cpp}` | Song data: patterns, tracks, events, instruments, arpeggios, midimacros |
| `src/playback.{h,cpp}` | MIDI output timing, playback state, R-effect arpeggio + Z-effect macro dispatch |
| `src/preset_data.h` | F4/Shift+F4 preset arrays + pure `*_apply_preset_to(struct*, idx)` functions. Pure data, header-only. |
| `src/preset_selector.h` | F4/Shift+F4 preset listbox decision logic (click / arrow / Space / P-cycle). Pure functions, exhaustively unit-tested. |
| `src/page_sync.h` | F4/Shift+F4 widget↔data sync helpers. Prevents the "preset reverts on next frame" class of bug. |
| `src/save_key_dispatch.h` | Global Ctrl-S dispatch decision (open save dialog / pass through / let page handle / swallow). Unit-tested. |
| `src/editor_layout.h` | Shared character-grid constants for F4/Shift+F4 (label right-edge, input start). |
| `tests/` | Unit-test executables (CTest). Suites cover preset logic, listbox decisions, page sync, save-key dispatch, and the keybuffer. Run via `ctest --test-dir build`. |
| `CMakeLists.txt` | Build, includes Lua static lib + tests subdirectory (gated on `ZT_BUILD_TESTS`, default ON) |
| `doc/help.txt` | In-app F1 help — update when adding keybinds or CLI flags. |
| `doc/CHANGELOG.txt` | Release notes style, chronological |
| `.github/workflows/build.yml` | Multi-platform CI (Win x86/x64, macOS arm64/x86_64, Linux x86_64). Linux job runs `ctest`. |

For the live PR queue: `gh pr list --repo m6502/ztrackerprime`.

## Coordinate system (has tripped up multiple changes)

- `row(N)` = `N * FONT_SIZE_Y` = `N * 8` pixels (returned as a Y-pixel offset but named for character rows)
- `col(M)` = `M * FONT_SIZE_X` = `M * 8` pixels
- `print(x, y, str, color, drawable)` — x first, then y
- **Some legacy code calls `print(row(2), col(13), ...)`** — that's `x = row(2)` (16 px) and `y = col(13)` (104 px = character row 13). Confusing but works because FONT_SIZE_X == FONT_SIZE_Y.

Layout macros:
- `INITIAL_ROW = 1`
- `PAGE_TITLE_ROW_Y = INITIAL_ROW + 8 = 9` (title separator row)
- `TRACKS_ROW_Y = PAGE_TITLE_ROW_Y + 2 = 11` (track-frame top in Pattern Editor)
- `HEADER_ROW = 1`

## Keyboard conventions

- **macOS Cmd key** is `KS_META`. On Apple, `SDL_KMOD_GUI` maps to `KS_META | KS_ALT` so every Alt+ shortcut works as Cmd+ too.
- Use `KS_HAS_ALT(kstate)` macro for Alt-accepts-Cmd checks. Bare `kstate == KS_ALT` fails on macOS because Cmd produces a compound state.
- **§ key on Finnish/EU ISO keyboards** sends `SDL_SCANCODE_NONUSBACKSLASH` (sometimes INTERNATIONAL1/2/3). In `keyhandler()` (main.cpp), those scancodes are remapped to `SDLK_GRAVE` so existing bindings (§ = Note Off, Shift+§ = drawmode toggle) work on non-US layouts.
- macOS `Cmd-Q` is stripped from the auto-created NSApp menu so the Pattern Editor can use it (Transpose-Up). macOS `Cmd-W` is stripped similarly.
- When adding shortcuts, also update `doc/help.txt`.

## PR discipline

- **Keep PRs small and focused.** Preferred size: 1 feature, ~30–300 LOC.
- `doc/CHANGELOG.txt` gets a section per PR.
- Tag commits with `Co-Authored-By: Claude <noreply@anthropic.com>` (or current model identifier) when AI-assisted.
- Title format: descriptive, under ~80 chars.
- **Don't duplicate content across PRs.** If two branches need the same change, merge one first then rebase the other, or drop the duplicate.

## Fix widget bugs upstream, not at every caller

When a rendering or behavior bug appears on **multiple pages with the same widget**, the fix belongs in `src/UserInterface.cpp` (the widget class), not in each `CUI_*.cpp` caller.

If you find yourself editing the same property (`xsize`, `frame`, color, padding) across more than two callers to fix the *same visual problem*, **stop and refactor**. The call sites are symptomatic; the widget's `::draw()` or constructor is the real bug.

Concrete patterns this catches:
- **Default values in the constructor.** If every caller sets `cb->xsize = 5` (or 3, or anything), set it once in `CheckBox::CheckBox()` and let callers omit it.
- **Cap or ignore caller-supplied values inside `::draw()`** when callers historically passed bad values. Example: `CheckBox::draw` caps the wipe extent at 3 (the "Off" width) regardless of `xsize`, so legacy `xsize=5` callers don't bleed.
- **Make a flag a no-op** if every caller's correct value is the same. Example: `CheckBox::frame` is a no-op — every audit ended with removing `frame=1`, so the frame block was deleted from `CheckBox::draw` entirely.
- **Sentinel cases stay in the caller.** A `MidiOutDeviceOpener` with a custom xsize, a `BankSelectCheckBox` overriding click behavior — those *are* per-caller. Don't push genuinely-distinct logic into the base widget.

Heuristic: if a series of caller-side edits keep fixing the same visual issue, the next fix should be upstream.

## Test harness

`tests/` has CTest-driven unit-test executables covering:

- preset arrays + apply functions
- listbox click / arrow / Space / P-cycle decision logic
- page sync (apply→refresh→sync per-frame cycle)
- global Ctrl-S dispatch
- the real `KeyBuffer` (compiled SDL-free)

Run all: `cd build && ctest --output-on-failure`. CI runs them on the Linux job after every push.

The harness stays SDL-free by:
- Linking against `tests/module_stub.cpp` (minimal `arpeggio` / `midimacro` constructors) instead of the full `src/module.cpp` (which `#include`s `zt.h` and pulls in the world).
- For `test_keybuffer`: defines `ZT_TEST_NO_SDL` so `keybuffer.h` includes `tests/sdl_stub.h` (a tiny header providing only the `SDL_KMOD_*` constants and a few `SDLK_*` codes used by the keybuffer logic) instead of `<SDL.h>`. This lets the production `keybuffer.cpp` compile straight into the test binary.

When fixing a class of bug that has bitten more than once (preset apply, save-key dispatch, button-up consumption, etc.), extract the decision logic into a header (`preset_selector.h`, `save_key_dispatch.h`, `page_sync.h`) and add tests.

## ListBox subclass mousestate gotcha (recurring foot-gun)

`ListBox::mouseupdate` has a subtle invariant: it relies on `mousestate` being non-zero when `BUTTON_UP_LEFT` arrives, because `act++` is gated on it. Two ways this gets clobbered:

1. **Pre-switch reset.** `if (!bMouseIsDown) mousestate = 0;` AT THE TOP OF `mouseupdate` clears mousestate BEFORE the switch sees BUTTON_UP. Result: the up event never increments `act`, `Keys.getkey()` is never called, the up event sits forever in the FIFO blocking every subsequent input. The reset must run AFTER the switch so the BUTTON_UP_LEFT case can still observe `mousestate==1` for the listbox that owns the click.

2. **Premature `ListBox::OnSelect(selected)` call.** The base `ListBox::OnSelect` sets `mousestate = 0`. If a subclass override calls it during click handling (between BUTTON_DOWN and BUTTON_UP), mousestate gets cleared early — same freeze pattern. Don't call `ListBox::OnSelect` from a subclass; the BUTTON_UP_LEFT case clears mousestate itself.

Symptom: "click on widget freezes the UI; Cmd-Tab unfreezes it." The Cmd-Tab unfreeze is `SDL_EVENT_WINDOW_FOCUS_GAINED → Keys.flush()` (see `src/main.cpp::zt_handle_platform_window_event`).

## Keyjazz slot pattern (Shift+F4)

The Arpeggio editor uses a dedicated focusable UIE (`ArKeyjazzFocus`, id `KEYJAZZ_ID`) as the keyjazz target:

- The stub UIE has a non-trivial `xsize`/`ysize` so click-to-focus works.
- `update()` is a no-op; `mouseupdate()` handles click-to-focus and returns `this->ID`.
- `draw()` is empty — the page-level `draw()` paints the visible Keyjazz panel and switches its colors based on `UI->cur_element == KEYJAZZ_ID` (active = highlighted, inactive = dim).
- The page-level keypress handler gates on `focused == KEYJAZZ_ID` to fire `MidiOut->noteOn` + `jazz_set_state(sym, note, chan)`. main.cpp's existing key-up path detects `jazz_note_is_active(sym)` on release and fires `noteOff` automatically.

Preferable to "always-on" keyjazz, which competes with letter-key handlers in TextInputs / listbox typeahead — the slot gives an explicit, visible mode toggle.

## CLI flags + Copy Relaunch Command

zt accepts named CLI flags (parsed by `zt_parse_cli` in `main.cpp`):

- `-h`, `--help` — print usage + exit (works without SDL/MIDI/display).
- `--list-midi-in` — list MIDI input ports + exit.
- `--midi-in <name|index>` — open the named port at startup; repeatable.
- `--midi-clock <name|index>` — open + enable `midi_in_sync` + `midi_in_sync_chase_tempo`.

Order-independent. Accepts both `--flag value` and `--flag=value`. Dash-count tolerant (`-flag` works as well as `--flag`). First positional arg = .zt song to load.

The ESC main-menu entry "Copy Relaunch Command" (under File) reads the current session state and emits a one-line `zt --midi-clock ... --midi-in ... song.zt` invocation to the system clipboard via `SDL_SetClipboardText`. Pairs with the CLI flags for stage / live / chat-handoff workflows.

## HARD RULE: User-reported inputs are ground truth

When the user reports a specific input ("I pressed Ctrl-S"), that IS what they pressed. Do not respond by substituting a different input ("you were probably pressing Cmd-S" / "you might have meant X" / "most likely cause is you actually pressed Y"). That is gaslighting — overwriting their observation with a hypothesis convenient for the model.

Banned phrasings (any variant):
- "Most likely cause: you were pressing X (instead of what you said)"
- "You probably / likely / actually pressed [something else]"
- "Maybe you're hitting [different key]"
- Any sentence whose effect is to substitute the user's reported input with model speculation.

When tracing the reported input through the code can't explain the symptom, the model is wrong, not the user. Allowed responses:

1. **Say so.** "I traced Ctrl-S through the dispatch and can't see why it would freeze. I'm missing something."
2. **Ask for more detail.** "Is the cursor still moving? Do toolbar clicks still respond? Does the status bar update?"
3. **Ask them to verify the binary.** "Are you running build/Program/zt.app or build-mac/Program/zt.app?"
4. **Add diagnostic output.** Insert a status_change=1 + statusmsg= line in the suspected branch.

## Paketti as a feature reference

Paketti (a Renoise tool) is a reference for tracker quality-of-life features that have been ported or are candidates for porting to zTracker. Already landed: Replicate at Cursor, Clone Pattern, Humanize Velocities, MIDI Macro + Arpeggio editors.

Future candidates: Fill-selection-with-note-at-cursor, Find/Replace note, Transpose selection, Groove/swing templates, Chord memory.

zTracker is MIDI-only (no samples/audio), so Paketti's sample-editor features are N/A.

## Branch / worktree cleanup

Delete local and remote branches confirmed merged to `origin/master`:
```bash
git branch --merged origin/master          # locals
git branch -r --merged origin/master       # remotes
```
Worktrees must be removed (`git worktree remove <path>`) before `git branch -D <name>`. The main worktree can't be removed; switch it back to `master` instead.

---

*Update this skill when the project's context shifts materially (new architecture area, new feature region, new recurring foot-gun).*
