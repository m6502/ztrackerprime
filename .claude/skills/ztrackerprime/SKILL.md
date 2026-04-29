---
name: ztrackerprime
description: zTracker Prime (m6502/ztrackerprime) contributor skill — picks up context for zTracker development, PR management, and collaboration with maintainer Manuel Montoto (@m6502).
domain: repository-specific
source_repo: https://github.com/m6502/ztrackerprime
source_platform: github
tags: [cpp, sdl3, tracker, midi, cross-platform]
triggers:
  keywords:
    primary: [ztracker, ztrackerprime, zt, zTracker]
    secondary: [m6502, manuel montoto, tracker, MIDI tracker, impulse tracker, paketti, SDL3 tracker]
---

# zTracker Prime Development Skill

## Repository Overview

**zTracker Prime** (m6502/ztrackerprime) is Manuel Montoto's maintained fork of zTracker — an Impulse-Tracker-inspired MIDI tracker originally by Christopher Micali (2000–2001) with contributions from Daniel Kahlin. Manuel brought it to SDL 3 and cross-platform (Windows XP through 11, macOS, Linux).

**Primary working directory:** `/Users/esaruoho/work/ztrackerprime` (main worktree — don't commit experiments here).

Active feature branches live in the main worktree (no separate working dirs as of 2026-04-29). `git worktree list` if any have been created since.

## Collaboration model

- **Esa has write access** to m6502/ztrackerprime. Push branches directly to `origin` (not a fork). Don't merge PRs without Manuel's approval — open them and let him review/merge.
- `origin` = `git@github.com:m6502/ztrackerprime.git` (the upstream).
- `esaruoho` = `git@github.com:esaruoho/ztrackerprime.git` (the old fork — keep it around as a safety net but don't push to it routinely anymore).
- Manuel merges PRs in batches. Many have landed in one day. Expect async multi-day review cycles.

## Build system

CMake. Each worktree needs `extlibs/` populated (libpng + zlib + lua):
```bash
cd <worktree>
ditto /Users/esaruoho/work/ztrackerprime/extlibs extlibs   # or rsync / cp -R
mkdir -p build && cd build
cmake -G "Unix Makefiles" ..
make -j8
```
Binary lands at `build/Program/zt` (or `build/Program/zt.app/Contents/MacOS/zt` on macOS with the .app-bundle branch).

SDL3 comes from Homebrew on macOS (`brew install sdl3`). Manuel's CMakeLists finds it via pkg-config or explicit `SDL3_INCLUDE_DIR` / `SDL3_LIBRARY`.

## Launching the app on macOS

Plain binary from a shell often exits silently when stdin is detached. Use one of:
- `open <path>/zt.app` (for the .app bundle branch)
- `open <path>/zt` (raw binary, opens via Finder's open)
- Run from Terminal.app directly

Never `./zt &` from within Claude's bash tool — it will exit on exit-code 0 without rendering. `open` gives it its own session.

## Screenshot automation (macOS dev tool)

`scripts/zt-screenshot.sh` on the PR-L branch. Launches zt.app, presses F1/F2/F3/F11/F12/Ctrl+F12, crops each page via `screencapture -R` using AppleScript to read the zt window's position+size. Output at `/tmp/zt-shots/`. macOS-only — uses AppleScript + screencapture.

**Use it to self-verify layout changes instead of asking the user to visually re-test every iteration.** The user gets frustrated fast when you keep asking them to screenshot.

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
| `tests/` | Unit-test executables (CTest). 5 suites, 285+ checks. Run via `ctest --test-dir build`. See "Test harness" section. |
| `CMakeLists.txt` | Build, includes Lua static lib + tests subdirectory (gated on `ZT_BUILD_TESTS`, default ON) |
| `doc/help.txt` | In-app F1 help — update when adding keybinds. Has a Command-Line Arguments section as of PR #68. |
| `doc/CHANGELOG.txt` | Release notes style, chronological |
| `.github/workflows/build.yml` | Multi-platform CI (Win x86/x64, macOS arm64/x86_64, Linux x86_64). Linux job runs `ctest`. |

## Coordinate system (tripped me up multiple times)

- `row(N)` = `N * FONT_SIZE_Y` = `N * 8` pixels (returned as a Y-pixel offset but named for character rows)
- `col(M)` = `M * FONT_SIZE_X` = `M * 8` pixels
- `print(x, y, str, color, drawable)` — x first, then y
- **Some legacy code calls `print(row(2), col(13), ...)`** — that's `x = row(2)` (16 px) and `y = col(13)` (104 px = character row 13). Confusing but works because FONT_SIZE_X == FONT_SIZE_Y.

Layout macros:
- `INITIAL_ROW = 1`
- `PAGE_TITLE_ROW_Y = INITIAL_ROW + 8 = 9` (title separator row)
- `TRACKS_ROW_Y = PAGE_TITLE_ROW_Y + 2 = 11` (track-frame top in Pattern Editor; user's preference)
- `HEADER_ROW = 1`

## Keyboard conventions

- **macOS Cmd key** is `KS_META`. On Apple, `SDL_KMOD_GUI` maps to `KS_META | KS_ALT` so every Alt+ shortcut works as Cmd+ too.
- Use `KS_HAS_ALT(kstate)` macro for Alt-accepts-Cmd checks. Bare `kstate == KS_ALT` fails on macOS because Cmd produces a compound state.
- **§ key on Finnish/EU ISO keyboards** sends `SDL_SCANCODE_NONUSBACKSLASH` (sometimes INTERNATIONAL1/2/3). In `keyhandler()` (main.cpp), we remap those scancodes to `SDLK_GRAVE` so existing bindings (§ = Note Off, Shift+§ = drawmode toggle) work on non-US layouts.
- When adding shortcuts, also update `doc/help.txt`.

## PR discipline

- **Keep PRs small and focused.** Manuel's preferred size: 1 feature, ~30–300 LOC.
- `doc/CHANGELOG.txt` gets a section per PR.
- Tag commits with `Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>`.
- Title format Manuel prefers: descriptive. No enforced convention but keep it under ~80 chars.
- **Don't duplicate content across PRs.** If PR-B has pattern ops and PR-L wants them too, either (a) merge PR-B first then rebase PR-L, or (b) drop them from PR-L.

## PR #21 (kitchen sink) policy

The `.app`-bundle PR grew to 36 commits covering many concerns. Manuel tolerated this but asked for it to be well-documented. Body has 11 numbered sections. Don't add more unrelated features to it; open separate PRs.

## Coordinate with Manuel

- He reviews async. Respond to his questions promptly and specifically — he appreciates code snippets and "why" explanations, not just "done".
- When he says something is "on him" (like the mouse-wheel window-resize bug), offer to tackle it as a separate PR rather than bundling into an existing one.
- Tag him (`@m6502`) when you want review; he doesn't always watch inbox otherwise.

## Fix widget bugs upstream, not at every caller

When a rendering or behavior bug appears on **multiple pages with the same widget**, the fix belongs in `src/UserInterface.cpp` (the widget class), not in each `CUI_*.cpp` caller.

If you find yourself editing the same property (`xsize`, `frame`, color, padding) across more than two callers to fix the *same visual problem*, **stop and refactor**. The call sites are symptomatic; the widget's `::draw()` or constructor is the real bug.

Concrete patterns this catches:
- **Default values in the constructor.** If every caller sets `cb->xsize = 5` (or 3, or anything), set it once in `CheckBox::CheckBox()` and let callers omit it.
- **Cap or ignore caller-supplied values inside `::draw()`** when callers historically passed bad values. Example: `CheckBox::draw` caps the wipe extent at 3 (the "Off" width) regardless of `xsize`, so legacy `xsize=5` callers don't bleed.
- **Make a flag a no-op** if every caller's correct value is the same. Example: `CheckBox::frame` is now a no-op — every checkbox audit ended with removing `frame=1`, so the frame block was deleted from `CheckBox::draw` entirely.
- **Sentinel cases stay in the caller.** A `MidiOutDeviceOpener` with a custom xsize, a `BankSelectCheckBox` overriding click behavior — those *are* per-caller. Don't push genuinely-distinct logic into the base widget.

Real-world example (commit `ab4996d`, 2026-04-27): user kept reporting yellow-stripe bleed next to On/Off checkboxes across F11, F12, Ctrl+F12 over multiple turns. Each round I patched per-caller `cb->xsize` and `cb->frame`. The right fix was always (a) cap the wipe in `CheckBox::draw`, and (b) delete the `if (frame) {...}` block. Two upstream commits would have replaced ~10 downstream edits and a dozen messages.

The user's word for the upstream-fix instinct: **"clever"**. Treat that as a reminder that downstream churn is a code smell, not a workflow.

## Test harness (PR #64+)

`tests/` has CTest-driven unit-test executables. 5 suites, 285 checks total at last count.

Run all: `cd build && ctest --output-on-failure`. Each suite is a standalone executable: `build/tests/test_presets`, `test_selector`, `test_page_sync`, `test_save_key`, `test_keybuffer`. CI runs them on the Linux job after every push.

The harness stays SDL-free by:
- Linking against `tests/module_stub.cpp` (minimal `arpeggio` / `midimacro` constructors only) instead of the full `src/module.cpp` (which `#include`s `zt.h` and pulls in the world).
- For `test_keybuffer`: defines `ZT_TEST_NO_SDL` so `keybuffer.h` includes `tests/sdl_stub.h` (a tiny header providing only the `SDL_KMOD_*` constants and a few `SDLK_*` codes used by the keybuffer logic) instead of `<SDL.h>`. This lets the production `keybuffer.cpp` compile straight into the test binary.

When you fix a class of bug that has bitten more than once (preset apply, save-key dispatch, button-up consumption, etc.), extract the decision logic into a header (`preset_selector.h`, `save_key_dispatch.h`, `page_sync.h`) and add tests. Bug-class regression test scaffolding has already paid off multiple times in the F4/Shift+F4 work.

## ListBox subclass mousestate gotcha (recurring foot-gun)

`ListBox::mouseupdate` has a subtle invariant: it relies on `mousestate` being non-zero when `BUTTON_UP_LEFT` arrives, because `act++` is gated on it. Two ways this gets clobbered:

1. **Pre-switch reset.** `if (!bMouseIsDown) mousestate = 0;` AT THE TOP OF mouseupdate (which the original code had) clears mousestate BEFORE the switch sees BUTTON_UP. Result: the up event never increments `act`, `Keys.getkey()` is never called, the up event sits forever in the FIFO blocking every subsequent input. The fix (in PR #65 / commit `87b2713`): move that reset to AFTER the switch so the BUTTON_UP_LEFT case can still observe `mousestate==1` for the listbox that actually owns the click.

2. **Premature `ListBox::OnSelect(selected)` call.** The base `ListBox::OnSelect` sets `mousestate = 0`. If a subclass override calls it during click handling (i.e. between BUTTON_DOWN and BUTTON_UP), mousestate gets cleared early — same freeze pattern. The fix in `ArPresetSelector::OnSelect` / `MmPresetSelector::OnSelect`: don't call `ListBox::OnSelect`. The natural BUTTON_UP_LEFT case clears mousestate itself.

If a user reports "click on widget freezes the UI; Cmd-Tab unfreezes it" — that's this. `Cmd-Tab → SDL_EVENT_WINDOW_FOCUS_GAINED → Keys.flush()` is the unfreeze path (see `src/main.cpp::zt_handle_platform_window_event`).

## Keyjazz slot pattern (Shift+F4)

The Arpeggio editor uses a dedicated focusable UIE (`ArKeyjazzFocus`, id `KEYJAZZ_ID`) as the keyjazz target. Pattern:

- The stub UIE has a non-trivial `xsize`/`ysize` so click-to-focus works.
- `update()` is a no-op; `mouseupdate()` handles click-to-focus and returns `this->ID`.
- `draw()` is empty — the page-level `draw()` paints the visible Keyjazz panel and switches its colors based on `UI->cur_element == KEYJAZZ_ID` (active = highlighted, inactive = dim).
- The page-level keypress handler gates on `focused == KEYJAZZ_ID` to fire `MidiOut->noteOn` + `jazz_set_state(sym, note, chan)`. main.cpp's existing key-up path (line 2578-area) detects `jazz_note_is_active(sym)` on release and fires `noteOff` automatically.

This pattern is preferable to "always-on" keyjazz (which competes with letter-key handlers in TextInputs / listbox typeahead) because the user has an explicit, visible mode toggle.

## CLI flags + Copy Relaunch Command (PR #68/#69)

zt accepts named CLI flags (parsed by `zt_parse_cli` in `main.cpp`):

- `-h`, `--help` — print usage + exit (works without SDL/MIDI/display).
- `--list-midi-in` — list MIDI input ports + exit.
- `--midi-in <name|index>` — open the named port at startup; repeatable.
- `--midi-clock <name|index>` — open + enable `midi_in_sync` + `midi_in_sync_chase_tempo`.

Order-independent. Accepts both `--flag value` and `--flag=value`. Dash-count tolerant (`-flag` works as well as `--flag`). First positional arg = .zt song to load.

The ESC main-menu entry "Copy Relaunch Command" (under File) reads the current session state and emits a one-line `zt --midi-clock ... --midi-in ... song.zt` invocation to the system clipboard via `SDL_SetClipboardText`. Pairs with the CLI flags for stage / live / chat-handoff workflows.

## Common user behaviors / preferences (learned)

- **"Launch the app"** = they want the app visibly running on their desktop, not just a process ID. Use `open .../zt.app` and verify with AppleScript visible-process check if needed.
- **"TEST" is a four-letter word** — don't ask the user to test until you've launched the app yourself and verified at least basic rendering. Use the screenshot script where possible.
- They conflate *.app, *.exe, and binary in casual speech. Disambiguate when it matters.
- They use Finnish keyboard layout. The § key is critical. Plain § → Note Off, Shift+§ → drawmode.
- They want PR quality over PR count. Stop spawning speculative PRs; finish one well before starting the next.
- When they push back ("that's not right / you're fucking around"), they mean it. Stop, re-read their message literally, and ask a clarifying question if uncertain. Do not make 3 successive edits trying to guess.
- **Don't lecture.** Be specific, show diffs, explain *why*, then act.

## HARD RULE: User-reported inputs are ground truth. Never substitute.

When the user says "I pressed Ctrl-S", that IS what they pressed. Do not respond with "you were probably pressing Cmd-S" / "you might have meant X" / "most likely cause is you actually pressed Y". That is gaslighting — overwriting their observation with a hypothesis that's more convenient for me because my mental model can't account for the bug.

Banned phrasings (any variant):
- "Most likely cause: you were pressing X (instead of what you said)"
- "You probably / likely / actually pressed [something else]"
- "It might be that what you meant was..."
- "Maybe you're hitting [different key]"
- Any sentence whose effect is to substitute the user's reported input with my speculation.

When I trace the reported input through the code and can't see how it produces the reported symptom, the model is wrong, not the user. Allowed responses in that situation:

1. **Say I'm stuck.** "I traced Ctrl-S through the dispatch and can't see why it would freeze. The code path I see drains the buffer and returns. I'm missing something."
2. **Ask for more detail.** "Can you describe the freeze — is the cursor still moving, do the toolbar buttons still respond to clicks, or is the whole window unresponsive? Does the status bar update? When you press another key after Ctrl-S, does anything happen?"
3. **Ask them to verify the binary.** "Are you running build/Program/zt.app or build-mac/Program/zt.app? The latter is older."
4. **Add diagnostic output.** Insert a status_change=1 + statusmsg= line in the suspected code path so the next time they reproduce, the message tells us which branch fired.

This rule overrides any pull toward "be helpful" or "make progress" — substituting the user's input is not helpful, it's destructive. Reason: the 2026-04-28 Ctrl-S incident, when the user reported Ctrl-S freezing F4 and I responded with "most likely you were pressing Cmd-S (macOS habit)" despite their explicit report. They called it disrespect. They were right.

## Paketti inspiration

Paketti (the Renoise tool by Esa) is a reference for the *quality-of-life* tracker features esaruoho wants ported to zTracker. Relevant bits already landed: Replicate at Cursor (PR #14), Clone Pattern (PR #14), Humanize Velocities (PR #22), MIDI Macro + Arpeggio editors (PR #64).

Future candidates: Fill-selection-with-note-at-cursor, Find/Replace note, Transpose selection, Groove/swing templates, Chord memory.

zTracker is MIDI-only (no samples/audio), so Paketti's sample-editor features are N/A.

## Branch/worktree cleanup

Delete local and remote branches confirmed merged to `origin/master` via:
```bash
git branch --merged origin/master          # locals
git branch -r --merged origin/master       # remotes
```
Be careful of worktrees — must `git worktree remove <path>` before `git branch -D <name>`. The main worktree can't be removed; just switch it to `master`.

## Current open PR queue (as of 2026-04-29)

| PR | Branch | Purpose | Depends |
|---|---|---|---|
| #65 | esa/hotfixes-again | F3 InstEditor frame revert (PR #61 regression) + dedicated Keyjazz slot in Shift+F4 + DISABLE_UNFINISHED_F4_*/F10_* gate cleanup | — |
| #66 | esa/vu-meters | Re-enable VU meters; harden `VUPlay::draw` + `draw_one_row` against null/stale instrument deref | — |
| #67 | esa/vu-bar-formula | VU bar math: replace 6-level Frankenstein cast chain with `init_vol * longevity / (init_longevity * 8)` | #66 |
| #68 | esa/cli-midi-args | `--help`, `--list-midi-in`, `--midi-in <port>` (repeatable), `--midi-clock <port>` (sets `midi_in_sync` + `midi_in_sync_chase_tempo`) | — |
| #69 | esa/copy-relaunch-cmd | ESC main-menu "Copy Relaunch Command" — captures session state to clipboard as a `zt --midi-clock ... --midi-in ... song.zt` line | #68 |

All `MERGEABLE`. CI green across all 5 platforms. Manu emailed the bundle 2026-04-29.

## Merged landmarks (most recent)

`eba3cf6` PR #64 MIDI Macro + Arpeggio editors + R/Z effect playback + 285-check unit-test harness · `eccfca3` PR #63 mouse/keyboard UX + run-log · `fea7e2b` PR #62 macOS crash fixes (MIDI-thread NSWindow + multi-MIDI-In + Cmd+Q wedge) · `b2e037c` PR #61 F11/F12/Ctrl+F12 layout polish + widget upstream cleanup · `78efde8` PR #23 mouse-wheel zoom · `a051975` PR #22 Humanize · `2818686` PR #20 multichannel MIDI · `c6ebf8c` PR #14 pattern ops · `4dc5d56` PR #13 keybindings.

---

*Update this skill when the project's context shifts materially (new PR, architecture change, new feature area).*
