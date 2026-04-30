# CLAUDE.md — contributor notes for AI assistants

This file is loaded automatically by Claude Code (and other AI agents that support `CLAUDE.md`) when working in this repo. It distills the conventions and gotchas that have been learned the hard way during zTracker Prime development. **Read this before making changes.**

For deeper material — recipes, foot-gun details, effect reference, glossary, coding conventions — see [`.claude/skills/ztrackerprime/SKILL.md`](.claude/skills/ztrackerprime/SKILL.md) and the `references/` + `recipes/` directories alongside it.

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
| `src/CUI_Patterneditor.cpp` | Pattern editor (F2). 10+ pattern operations. **Ctrl+Shift+§ toggles `g_cc_drawmode`** — incoming CC writes `Sxxyy` / PB writes `Wxxxx` at the cursor row. |
| `src/CUI_Midimacroeditor.cpp` | MIDI Macro editor (F4). A macro whose name ends in `.syx` dispatches as a SysEx file send (see `sysex_macro.h`); the data grid is ignored. |
| `src/CUI_Arpeggioeditor.cpp` | Arpeggio editor (Shift+F4). |
| `src/CUI_KeyBindings.cpp` | Unified Shortcuts & MIDI Mappings (**Shift+F2** — moved from Shift+F3 to free that slot for the CC Console). |
| `src/CUI_CcConsole.cpp` | CC Console (**Shift+F3**) — Paketti CCizer file load + sliders/knobs send-out + per-slot MIDI Learn (`L` to learn, `U` to unbind, `B` to assign as current instrument's bank). |
| `src/CUI_SysExLibrarian.cpp` | SysEx Librarian (**Shift+F5**) — `.syx` file send + auto-capture of incoming SysEx as `recv_<timestamp>.syx`. |
| `src/CUI_*.cpp` | Other pages: Sysconfig, Songconfig, Help, About, InstEditor, MainMenu, etc. |
| `src/UserInterface.{h,cpp}` | Widget classes — `CheckBox`, `ValueSlider`, `TextInput`, `Frame`, `Button`, `TextBox`, `ListBox`, `MidiOutDeviceOpener`, `SkinSelector`, `VUPlay`. **Fix rendering bugs in the widget class, not at every caller.** |
| `src/keybuffer.{h,cpp}` | Key buffer + KS_ALT/KS_CTRL/KS_META/KS_SHIFT state. `KS_HAS_ALT(s)` macro accepts ALT or macOS Cmd. Header has a `ZT_TEST_NO_SDL` guard so it can be compiled into SDL-free unit tests via `tests/sdl_stub.h`. |
| `src/font.cpp` | `print()` / `printtitle()` / `printBG()` drawing primitives. |
| `src/zt.h` | Kitchen-sink: `STATE_*` enum (incl. `STATE_CCCONSOLE`, `STATE_SYSEX_LIB`), `CMD_*` enum, page globals (`UIP_CcConsole`, `UIP_SysExLibrarian`), `g_cc_drawmode`, layout macros. |
| `src/module.{h,cpp}` | Song data: patterns, tracks, events, instruments, arpeggios, midimacros. `instrument` carries `ccizer_bank[256]` (per-instrument Paketti file). |
| `src/playback.{h,cpp}` | MIDI output timing, playback state, R-effect arpeggio + Z-effect macro dispatch. **Z on a `*.syx`-named midimacro** dispatches the file as SysEx via `MidiOut->sendSysEx`. |
| `src/winmm_compat.h` | Cross-platform MIDI shim. `zt_midi_out_sysex` for SysEx send (CoreMIDI `MIDIPacketListAdd` / WinMM `midiOutLongMsg` / ALSA `snd_seq_ev_set_sysex`). macOS `zt_coremidi_read_proc` parses incoming `F0..F7` into `zt_sysex_inq_push`. |
| `src/sysex_inq.{h,cpp}` | Process-wide receive queue for incoming SysEx (16 slots × 8 KB; `std::mutex` protected; overflow drops oldest). |
| `src/sysex_macro.{h,cpp}` | Helpers for the `*.syx`-named-midimacro convention: predicate + `syx_folder` path resolution + framing-checked file reader. |
| `src/ccizer.{h,cpp}` | CCizer parser, `.cc-view` slider/knob sidecar, `.cc-midi` MIDI Learn sidecar, folder scan, MIDI byte builder, `zt_ccizer_current_file()` (Pattern Editor reads it for friendly slot names in CC drawmode status). |
| `src/conf.{h,cpp}` | zt.conf — adds `ccizer_folder` + `syx_folder` keys. |
| `src/preset_data.h` | F4/Shift+F4 preset arrays + pure `*_apply_preset_to(struct*, idx)` apply functions. Header-only, SDL-free, exhaustively unit-tested. |
| `src/preset_selector.h` | Pure decision logic for the preset listbox (click / arrow / Space / P-cycle). Unit-tested. |
| `src/page_sync.h` | F4/Shift+F4 widget↔data sync helpers. Prevents the "preset reverts on next frame" class of bug. |
| `src/save_key_dispatch.h` | Global Ctrl-S dispatch (open save dialog / pass through / let page handle). Unit-tested. |
| `src/editor_layout.h` | Shared character-grid constants for F4/Shift+F4 editors. |
| `assets/ccizer/` | Bundled CCizer banks (Paketti subset) + README. CMake POST_BUILD copies into `.app Resources/ccizer` and `<exe-dir>/ccizer`. |
| `assets/syx/` | Bundled SysEx files. `request_universal_inquiry.syx` for first-test handshakes. |
| `tests/` | CTest unit-test executables. **8 suites**: presets / selector / page_sync / save_key / keybuffer / ccizer / sysex_inq / sysex_macro. Linux CI runs them. |
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

## Page / shortcut map

| Key | Page |
|---|---|
| F1 / F2 / F3 | Help / Pattern Editor / Instrument Editor |
| F4 / Shift+F4 | MIDI Macro Editor / Arpeggio Editor |
| **Shift+F2** | **Shortcuts & MIDI Mappings** (moved from Shift+F3 on 2026-04-30) |
| **Shift+F3** | **CC Console** — Paketti CCizer file load + sliders/knobs send-out + MIDI Learn |
| F5 / F6 / F7 / F8 / F9 | Play / Play Pattern / From Cursor / Stop / Panic |
| **Shift+F5** | **SysEx Librarian** — `.syx` send + auto-capture as `recv_<TS>.syx` |
| F10 / F11 / F12 | Song Message / Song Config / System Config (incl. CCizer Folder input) |
| **Ctrl+Shift+§** | **CC drawmode toggle** — incoming CC writes `Sxxyy`, PB writes `Wxxxx` |

---

## CCizer + SysEx (PRs #78–#84, end of April 2026)

Three feature areas wired in seven stacked PRs:

1. **CC Console** (Shift+F3, `CUI_CcConsole.cpp`). Loads CCizer-format `.txt` files from `ccizer_folder`. Each slot is shown as a slider or knob; tweaking sends MIDI CC out. `V` toggles the widget choice (persists in a `<file>.cc-view` sidecar). `L` enters MIDI Learn — the next incoming `0xB0..0xBF` / `0xE0..0xEF` binds to the focused slot (persists in `<file>.cc-midi`). `B` assigns the loaded file as the current instrument's bank.

2. **Per-instrument CCizer bank**. `instrument.ccizer_bank[256]` (in `module.h`) is the filename. Persisted via a NEW optional `CCBN` chunk in the `.zt` save format; old zTracker silently skips it via `readblock`'s built-in advance-past behavior, so format compatibility is bidirectional. Pattern Editor auto-loads the focused instrument's bank on `cur_inst` change so the user's "1st CC for instrument N" mental model maps directly to slot 1 of the right file.

3. **SysEx**. Foundation in `winmm_compat.h` (`zt_midi_out_sysex`) + `OutputDevice::sendSysEx` + receive queue `sysex_inq.{h,cpp}`. macOS callback parses incoming `F0..F7` into the queue. **SysEx Librarian** (Shift+F5) lists `.syx` files in `syx_folder`, sends on Enter, auto-saves received messages as `recv_<timestamp>.syx`. **Pattern dispatch convention**: a midimacro whose `name` ends in `.syx` (case-insensitive, len > 4) is dispatched as a SysEx file send by the Z-effect handler — no `.zt` format change because the existing MMAC chunk already round-trips the name.

Cross-platform status (2026-04-30): SysEx send works on all three platforms; receive parsing is macOS-only today (Linux/Windows on the followup TODO).

Tests for all three: `test_ccizer` (51 checks), `test_sysex_inq` (~25), `test_sysex_macro` (~20). Bundled assets under `assets/ccizer/` (Paketti subset) and `assets/syx/` (universal device inquiry).

---

## Test harness

`tests/` builds eight CTest executables:

- `test_presets` — preset arrays + apply functions.
- `test_selector` — listbox click / arrow / Space / P-cycle decision logic.
- `test_page_sync` — apply→refresh→sync per-frame cycle.
- `test_save_key` — global Ctrl-S dispatch.
- `test_keybuffer` — real `KeyBuffer` from `src/keybuffer.cpp`, compiled under `ZT_TEST_NO_SDL` against `tests/sdl_stub.h`.
- `test_ccizer` — Paketti CCizer parser (`PB`/CC# tokens, comments, blanks, out-of-range), `.cc-view` view-hint sidecar round-trip, MIDI byte builder for CC + 14-bit Pitchbend, folder resolution, `.txt` directory scan.
- `test_sysex_inq` — process-wide SysEx receive queue: empty / push-pop round-trip / FIFO ordering / overflow drops oldest / undersized pop buffer (message stays queued) / invalid input rejection.
- `test_sysex_macro` — `*.syx`-named-midimacro convention helpers: predicate (case-insensitive, rejects bare `.syx`), path resolution against `syx_folder`, and the framing-checked file reader (rejects missing `F0`/`F7`, oversized, missing).

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
