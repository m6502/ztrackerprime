# CLAUDE.md — contributor notes for AI assistants

This file is loaded automatically by Claude Code (and other AI agents that support `CLAUDE.md`) when working in this repo. It distills the conventions and gotchas that have been learned the hard way during zTracker Prime development. **Read this before making changes.**

**This file is self-contained** — you do not need the skill to contribute. For *deeper* material — headless-screenshot recipes, the full list-pane construction idiom (with code), effect reference, glossary — see [`.claude/skills/ztrackerprime/SKILL.md`](.claude/skills/ztrackerprime/SKILL.md) and the `references/` + `recipes/` directories alongside it. The skill is the continuously-maintained companion; CLAUDE.md is the standalone briefing that mirrors its essentials for agents and humans who don't load skills.

> **Last synced with the codebase: 2026-06-15.** If that date is more than ~2 weeks old when you read this, your first move is to check what merged since — `gh pr list --repo m6502/ztrackerprime --state merged --json number,title,mergedAt` — and reconcile the key-files table, shortcut map, feature sections, and test list below against current `master` before relying on them. Bump this date in the same commit that fixes any drift. (This guard exists because CLAUDE.md silently fell ~7 weeks out of date once: it duplicated the skill's content but, unlike the skill, had no reconcile step.)

---

# Coding Standards

- **Zero-noise comments:** Do not comment on obvious code, functions, or variable names.
- **Comment limits:** Only add comments if a block contains complex, non-obvious algorithmic logic or tricky workarounds.
- **Refactoring:** If the code is so weird it needs a paragraph of explanation, refactor it to be self-documenting instead.

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
| `src/CUI_CCEnvelopeEditor.cpp` | CC Envelope Editor (**Shift+F6**) — per-instrument, CCizer-aware CC envelopes (Schism-style). Persisted in the optional `INSE` chunk; loads/saves `.env` presets. See `ccenv_{advance,interp,io}.{h,cpp}`. |
| `src/CUI_LuaConsole.cpp` | Lua Console (**Ctrl+Alt+L**) — Renoise-style interactive console over the embedded Lua engine. Tab cycling, `rprint`/`oprint`/`help`, wrapping scrollback. |
| `src/CUI_Page.{h,cpp}` | Base class for all `CUI_*` pages. |
| `src/CUI_*.cpp` | Other pages: Sysconfig, Songconfig, Help, About, InstEditor, MainMenu, etc. |
| `src/UserInterface.{h,cpp}` | Widget classes — `CheckBox`, `ValueSlider`, `TextInput`, `Frame`, `Button`, `TextBox`, `ListBox`, `MidiOutDeviceOpener`, `SkinSelector`, `VUPlay`. **Fix rendering bugs in the widget class, not at every caller.** |
| `src/keybuffer.{h,cpp}` | Key buffer + KS_ALT/KS_CTRL/KS_META/KS_SHIFT state. `KS_HAS_ALT(s)` macro accepts ALT or macOS Cmd. Header has a `ZT_TEST_NO_SDL` guard so it can be compiled into SDL-free unit tests via `tests/sdl_stub.h`. |
| `src/font.cpp` | `print()` / `printtitle()` / `printBG()` drawing primitives. |
| `src/zt.h` | Kitchen-sink: `STATE_*` enum (incl. `STATE_CCCONSOLE`, `STATE_SYSEX_LIB`), `CMD_*` enum, page globals (`UIP_CcConsole`, `UIP_SysExLibrarian`), `g_cc_drawmode`, layout macros. |
| `src/module.{h,cpp}` | Song data: patterns, tracks, events, instruments, arpeggios, midimacros. `instrument` carries `ccizer_bank[256]` (per-instrument Paketti file). |
| `src/playback.{h,cpp}` | MIDI output timing, playback state, R-effect arpeggio + Z-effect macro dispatch. **Z on a `*.syx`-named midimacro** dispatches the file as SysEx via `MidiOut->sendSysEx`. Drives the Ableton Link pump. |
| `src/lua_engine.{cpp,h}` | Embedded Lua API: object model (`zt`, `song`, `transport`, `pattern`, `track`, cells, order list) + notifiers (`zt.on/off/fire` for play/stop/row/idle). `lua/selftest.lua` exercises the surface via `--lua-test`. |
| `src/ableton_link.{cpp,h}` | Ableton Link tempo + transport sync (PR #159). C API: `zt_ableton_link_startup/teardown/pump/defer_play/available/get_tempo`. Driven from `playback.cpp`, configured from F11, conf keys in `conf.cpp`. |
| `src/ccenv_{advance,interp,io}.{h,cpp}` | CC Envelope engine: advance/step logic, interpolation, and `.env` preset + `INSE` chunk I/O. Backs `CUI_CCEnvelopeEditor`. |
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
| **Shift+F6** | **CC Envelope Editor** — per-instrument CCizer-aware CC envelopes (PR #132) |
| F10 / F11 / F12 | Song Message / Song Config / System Config (incl. CCizer Folder input) |
| **Ctrl+Alt+L** | **Lua Console** — interactive Lua over the embedded engine (siblings: Ctrl+Alt+K Keybindings, Ctrl+Alt+N New Song) |
| **Ctrl+Shift+§** | **CC drawmode toggle** — incoming CC writes `Sxxyy`, PB writes `Wxxxx` (Windows: `Ctrl+Shift+`` ` ``) |

Sub-modes worth knowing:
- **F2 again** from the Pattern Editor → PEParms. **F2 F2 toggles "PianoKey"**, the Ableton/Logic piano keyjazz layout (PR #135).
- **F3 again** from the Instrument Editor → "Create 16 Channels" popup: fills the next empty instrument slots with the current device on ch 1..16 (PR #136). F3 row 11 shows `CCizer Bank: <basename>`.
- **F11** Song Config hosts the **Ableton Link** controls (enable / start-stop-sync checkboxes, quantize-offset slider).
- **CC drawmode** is extended by PRs #123–#129: mouse drag-to-draw drawbars, CCizer-slot cycling, one-key arm-and-draw, double-click reset, `Ctrl+F2` slot cycle, keyjazz audition while drawing. `[CC DRAW]` badge top-right while active.

---

## CCizer + SysEx (PRs #78–#84, end of April 2026)

Three feature areas wired in seven stacked PRs:

1. **CC Console** (Shift+F3, `CUI_CcConsole.cpp`). Loads CCizer-format `.txt` files from `ccizer_folder`. Each slot is shown as a slider or knob; tweaking sends MIDI CC out. `V` toggles the widget choice (persists in a `<file>.cc-view` sidecar). `L` enters MIDI Learn — the next incoming `0xB0..0xBF` / `0xE0..0xEF` binds to the focused slot (persists in `<file>.cc-midi`). `B` assigns the loaded file as the current instrument's bank.

2. **Per-instrument CCizer bank**. `instrument.ccizer_bank[256]` (in `module.h`) is the filename. Persisted via a NEW optional `CCBN` chunk in the `.zt` save format; old zTracker silently skips it via `readblock`'s built-in advance-past behavior, so format compatibility is bidirectional. Pattern Editor auto-loads the focused instrument's bank on `cur_inst` change so the user's "1st CC for instrument N" mental model maps directly to slot 1 of the right file.

3. **SysEx**. Foundation in `winmm_compat.h` (`zt_midi_out_sysex`) + `OutputDevice::sendSysEx` + receive queue `sysex_inq.{h,cpp}`. macOS callback parses incoming `F0..F7` into the queue. **SysEx Librarian** (Shift+F5) lists `.syx` files in `syx_folder`, sends on Enter, auto-saves received messages as `recv_<timestamp>.syx`. **Pattern dispatch convention**: a midimacro whose `name` ends in `.syx` (case-insensitive, len > 4) is dispatched as a SysEx file send by the Z-effect handler — no `.zt` format change because the existing MMAC chunk already round-trips the name.

Cross-platform status (verified 2026-05-02): SysEx **send** works on macOS (CoreMIDI `MIDIPacketListAdd`), Windows (WinMM `midiOutLongMsg` + MHDR_DONE poll, PR #87), Linux (ALSA `snd_seq_ev_set_sysex`). SysEx **receive** works on macOS and Windows (`MIM_LONGDATA` accumulator, PR #90). Linux ALSA MIDI input landed in PR #113 (marked "needs hardware verification") — confirm SysEx-receive-on-Linux against real hardware before relying on it.

Tests for all three: `test_ccizer` (51 checks), `test_sysex_inq` (~25), `test_sysex_macro` (~20). Bundled assets under `assets/ccizer/` (Paketti subset) and `assets/syx/` (universal device inquiry).

---

## CC Envelope Editor (Shift+F6, PR #132)

Per-instrument, CCizer-aware CC envelopes (Schism-style). Page is `CUI_CCEnvelopeEditor.cpp`; the engine is split across `ccenv_advance.h` (per-row advance), `ccenv_interp.h` (interpolation), and `ccenv_io.{cpp,h}` (`.env` preset files + persistence). Envelopes are stored in a new optional `INSE` chunk in the `.zt` save format (forward/backward compatible the same way `CCBN` is — old zTracker skips the unrecognised chunk). The editor loads/saves `.env` presets from a configurable folder.

---

## Lua scripting (PRs #148–#157)

An embedded Lua engine + interactive console. Treat it as a first-class subsystem.

- **Engine** (`src/lua_engine.{cpp,h}`) — a Renoise-flavoured object model: `zt` (top-level), `song`, `transport`, `pattern`, `track`, plus cell access, the order list, note names, constants. Notifier API: `zt.on(event, fn)` / `zt.off(...)` / `zt.fire(...)` for `play` / `stop` / `row` / `idle`, pumped from the main loop. MIDI macros expose `:send()`.
- **Console** (`src/CUI_LuaConsole.cpp`, **Ctrl+Alt+L**) — Tab cycling, `rprint` / `oprint` / `help`, wrapping scrollback.
- **Self-test** — `lua/selftest.lua` exercises the whole API surface; runs via `--lua-test` (the `lua_api` ctest target) and is wired into macOS CI. **When you add or rename a Lua binding, update `lua/selftest.lua` in the same PR** — CI catches regressions there.

---

## Ableton Link (PR #159, merged 2026-06-10)

Tempo + transport sync with Ableton Live and other Link-aware apps over the local network.

- **Core** (`src/ableton_link.{cpp,h}`) — C API: `zt_ableton_link_startup` / `_teardown` / `_pump` / `_defer_play(row, pattern, pm)` / `_available` / `_get_tempo`. Pumped from `playback.cpp`; the pump can move `song->bpm` underneath the UI, so F11 re-reads it each frame.
- **Config** (`conf.cpp`, persisted in `zt.conf`): `ableton_link_enable` (**OFF by default** — no surprise LAN traffic), `ableton_link_start_stop_sync` (ON by default; only effective once Link is enabled), `ableton_link_quantum` (default 4 = one bar of 4/4), `ableton_link_offset_ms`.
- **UI** — F11 Song Configuration hosts the enable + start/stop-sync checkboxes and the offset slider.
- **Precedence:** if both Ableton Link and MIDI Sync are enabled, **Link wins**.

---

## Test harness

`tests/` builds a growing set of CTest suites. **`ctest --test-dir build -N` is the source of truth for the current list** (the count drifts as suites are added). As of the last sync there are 14:

- `presets` — preset arrays + apply functions.
- `selector` — listbox click / arrow / Space / P-cycle decision logic.
- `page_sync` — apply→refresh→sync per-frame cycle.
- `save_key` — global Ctrl-S dispatch.
- `keybuffer` — real `KeyBuffer` from `src/keybuffer.cpp`, compiled under `ZT_TEST_NO_SDL` against `tests/sdl_stub.h`.
- `ccizer` — Paketti CCizer parser (`PB`/CC# tokens, comments, blanks, out-of-range), `.cc-view` view-hint sidecar round-trip, MIDI byte builder for CC + 14-bit Pitchbend, folder resolution, `.txt` directory scan.
- `sysex_inq` — process-wide SysEx receive queue: empty / push-pop / FIFO / overflow drops oldest / undersized pop buffer / invalid input rejection.
- `sysex_stress` — SysEx queue under concurrent load.
- `sysex_macro` — `*.syx`-named-midimacro convention helpers: predicate, path resolution against `syx_folder`, framing-checked file reader.
- `ccbn_roundtrip` — per-instrument `CCBN` chunk save/load round-trip.
- `ccenv` — CC Envelope advance / interpolation / `.env` + `INSE` I/O.
- `keyjazz_map` — Ableton/Logic PianoKey layout + the two layouts' mutual exclusivity.
- `ableton_link` — Ableton Link config + precedence (`ZT_TEST_NO_SDL`).
- `lua_api` — full Lua API self-test: drives the real `zt` binary headless (`--headless --lua-test`) against a scratch song, runs `lua/selftest.lua`, asserts exit 0.

Run all: `ctest --test-dir build --output-on-failure`. Linux CI runs the SDL-free suites after every push; the `lua_api` self-test runs on macOS CI.

When you fix a class of bug that has bitten more than once (preset apply, save-key dispatch, button-up consumption), extract the decision logic into a header (`preset_selector.h`, `save_key_dispatch.h`, `page_sync.h`, etc.) and add tests. Bug-class regression test scaffolding has paid off multiple times in the F4/Shift+F4 work.

---

## INVARIANT: every page key handler must `need_refresh++`

`main.cpp`'s outer loop only calls `ActivePage->draw()` when `need_refresh != 0` (search `if (need_refresh)` near line 3184). A page that mutates state (cursor, scroll, focus, value, status line, learn flag) on a key without bumping `need_refresh` looks frozen to the user — the new state is computed but never drawn.

**Rules:**
1. Every `if (key == ...)` branch in a page's `update()` that changes any visible state ends with `need_refresh++;` before `return;`.
2. `enter()` bumps `need_refresh++` so the first frame after page switch always paints.
3. Background activity that mutates state (MIDI-in pump, file watch) bumps `need_refresh++` per change.
4. Mouse-driven widget changes are handled by the widget itself (`ValueSlider::mouseupdate` already does `need_refresh++` and `need_redraw++`); the page doesn't need to bump again for those.
5. **No "iterate later" placeholders for visible interactive elements.** If a page advertises clickable sliders, knobs, buttons — they must be real `UserInterfaceElement` widgets registered with `UI->add_element`, not `printBG` ASCII art. ASCII bars look the same as widgets in a screenshot and the user only finds out they can't click when they try. Widgets get drawn by `UI->draw(S)`; their min/max/value flow through `changed` flags the page absorbs after `UI->update()`.

Failure mode if rule 1 is violated: arrow keys move the highlighted cursor in memory but the screen doesn't repaint until something else (mouse move, resize, MIDI in) bumps `need_refresh`. Looks like the page froze.

Failure mode if rule 5 is violated: shipped pages with non-interactive ASCII "sliders" that the user discovers don't respond to mouse — embarrassment + rework. (Cf. PR #78 CC Console v1, fixed in a follow-up.)

## Recurring foot-guns

### `midiInQueue` thread safety (was: latent race; defused as of PR #116)

The process-wide `miq` instance (`src/midi-io.cpp:255`, `extern miq midiInQueue` in `midi-io.h`) is the bridge between **the platform MIDI thread** (CoreMIDI client thread / WinMM driver thread / ALSA poll thread, depending) and **the main UI thread** (Pattern Editor / InstEditor / PEParms / CcConsole drain loops). Before PR #116 every member function — `insert` from the producer thread, `pop` / `check` / `size` / `clear` from the consumer thread — was unsynchronised: plain `int qhead, qtail, qelems` with no mutex, no atomics. Symptoms were subtle: occasional missed messages, occasional duplicates, occasional torn `dwParam1` reads under heavy CC streams or `clear()` racing the producer.

PR #116 added a `std::mutex` to `miq` and lock-guards every member. Forward rule for any new producer or consumer of `midiInQueue`: just use the public methods — they're thread-safe. **Never** access `qhead/qtail/qelems` directly from outside the class.

### ListBox subclass mousestate gotcha

`ListBox::mouseupdate` relies on `mousestate` being non-zero when `BUTTON_UP_LEFT` arrives, because `act++` is gated on it. Two ways this gets clobbered:

1. **Pre-switch reset.** `if (!bMouseIsDown) mousestate = 0;` at the **top** of `mouseupdate` clears mousestate BEFORE the switch sees BUTTON_UP. Result: the up event never increments `act`, `Keys.getkey()` is never called, the up event sits forever in the FIFO blocking every subsequent input. **The reset must run AFTER the switch.**

2. **Premature `ListBox::OnSelect(selected)` in a subclass override.** Historically, base `ListBox::OnSelect` set `mousestate = 0`, and a subclass override calling it during click handling (between BUTTON_DOWN and BUTTON_UP) cleared mousestate early — same freeze pattern. **Defused as of PR #112: base `ListBox::OnSelect` is now a documented no-op**, so subclasses can safely call `ListBox::OnSelect(p)` from inside their override without freezing. The rule that survives: **never write `mousestate = 0` inside any `OnSelect` override either**; the BUTTON_UP_LEFT case in `mouseupdate` is the only place that should clear it.

Symptom for the user (the historic version): "click on listbox freezes the UI; Cmd-Tab unfreezes it." The Cmd-Tab unfreeze is `SDL_EVENT_WINDOW_FOCUS_GAINED → Keys.flush()` (see `main.cpp::zt_handle_platform_window_event`).

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
- Tag commits with `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>` (or current model).
- Don't merge to master without Manuel's approval — open PRs and let him review/merge.
- `doc/CHANGELOG.txt` gets a section per PR.
- Don't duplicate content across PRs.
- Push branches directly to `origin` (no fork needed; Esa has write access).

### Bug reports = branch + PR + push, no asking

When Esa reports a bug, the **default response is**: investigate → fix → branch → commit → push → open PR. Do not ask "want me to fix?" or "should I open a PR?" — that's the implicit ask. Just do it.

The only acceptable preamble is one or two sentences naming the root cause and the fix, then go.

- Branch name: `esa/<short-descriptive>` (e.g. `esa/cc-console-file-switch-leak`).
- Build clean + run `ctest` before pushing.
- PR body: summary, root cause, fix, test plan checklist. Include the user's verbatim quote of the symptom if they gave one.
- Push to `origin`; Esa has write access.

Ask for clarification only if the bug **report itself** is ambiguous about the symptom — never about whether to fix it.

---

## Common user behaviour (learned)

- **"Launch the app"** = visibly running on the desktop, not just a process ID. Use `open .../zt.app`.
- **Don't ask the user to test.** Run the binary yourself first and verify basic rendering, ideally with `scripts/zt-screenshot.sh`. Only ask the user when something requires their interactive judgement.
- They use **Finnish keyboard layout**. § is critical. Plain § = Note Off, Shift+§ = drawmode.
- **PR quality over PR count.** Finish one well before starting another.
- **Don't lecture.** Be specific, show diffs, explain *why*, then act.
- When the user pushes back ("that's not right"), they mean it. Stop, re-read literally, ask a clarifying question.
