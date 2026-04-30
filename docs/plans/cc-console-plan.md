# CC Console for zTracker Prime — Plan

Date: 2026-04-29
Status: proposed, awaiting user sign-off

## Goal

Add a **CC Console** page that loads Paketti CCizer-format `.txt` files and lets the user send MIDI CCs out to the current MIDI Out device via on-screen sliders or knobs. Each row's widget type (slider vs. knob) is user-toggleable and persisted.

Out of scope for this plan: incoming-CC monitor, pattern CC inserter, NRPN / RPN / SysEx, MIDI Learn against incoming CC. Those are follow-ups.

## CCizer file format (Paketti convention)

```
<CC# or "PB"> <Name>
```

- One slot per non-comment, non-empty line.
- `PB` = pitchbend (status `0xE0`, 14-bit value).
- `<CC#>` = decimal `0..127`, status `0xB0 | channel`.
- Lines starting with `#` are comments. Blank lines OK.
- Slot index = the line's position among non-comment/non-empty lines (1-based).
- File extension `.txt`. UTF-8.

Example (`sc88st.txt`):
```
PB Pitchbend
1 Mod
7 Volume
74 Cutoff
```

We add **no markers** to the format. Slider/knob choice is stored separately so CCizer files stay round-trippable with Paketti.

## Bundled examples

Copy a curated subset from `~/work/paketti/ccizer` into `assets/ccizer/` in the repo. Suggested starter set (broad coverage of synth styles):

- `sc88st.txt` (SC-88 / general MIDI baseline)
- `microfreak.txt`, `minilogue.txt`, `monologue.txt`, `prophet6.txt` (mono/poly synths)
- `deepmind6.txt`, `waldorf_blofeld.txt` (wide-CC synths)
- `tb03.txt`, `se02.txt`, `pro800.txt` (analog mono)
- `polyend_synth_mode.txt`, `polyend_performance_mode.txt` (Polyend)
- `midi_control_example.txt` (generic teaching file)

Plus a new `assets/ccizer/README.md` explaining the format with one example, written *for the user*, not for developers — this is the in-folder teaching surface.

Install paths:
- **macOS .app:** copy into `zt.app/Contents/Resources/ccizer/` via the existing CMake `RESOURCES` block.
- **Linux:** install to `${CMAKE_INSTALL_PREFIX}/share/zt/ccizer/` plus a fallback to `<exe-dir>/ccizer/` for run-from-build.
- **Windows:** `<exe-dir>/ccizer/` next to `zt.exe`.

## Folder selection

New `zt.conf` key: `ccizer_folder=<path>`.

Resolution order at startup:
1. `ccizer_folder` from zt.conf if set and exists
2. `<exe-dir>/ccizer/` (run-from-build / Windows)
3. Platform install dir (macOS Resources, Linux share)
4. `~/.config/zt/ccizer/` (user override)
5. Empty list with a "no ccizer folder configured" hint on the page

Sysconfig (F11) gets a new "CCizer Folder" text input + Browse button so the user can point at any folder (e.g. their Paketti checkout directly: `~/work/paketti/ccizer`).

## Page layout — Shift+F5

Why Shift+F5: F5 is Play; Shift+F5 is unused; sits naturally next to the playback keys, easy to reach mid-session. ESC main menu gets a "CC Console" entry.

```
 ┌─ Files ─────────────────┐  ┌─ sc88st.txt ─────────────────────────────┐
 │ deepmind6.txt           │  │  Slot  CC   Name           View   Value │
 │ microfreak.txt          │  │   1    PB   Pitchbend      slider  64   │
 │ minilogue.txt           │  │   2    1    Mod            knob    23   │
 │ monologue.txt           │  │   3    7    Volume         slider  100  │
 │▶sc88st.txt              │  │   4    10   Pan            knob    64   │
 │ tb03.txt                │  │   5    74   Cutoff         slider  90   │
 │ ...                     │  │   ...                                    │
 └─────────────────────────┘  └──────────────────────────────────────────┘

 Channel: 1   Out: IAC Bus 1     [Tab] focus grid · [V] toggle slider/knob
```

- Left pane: file listbox, populated from the configured folder. Enter loads.
- Right pane: per-slot grid. Up/Down picks slot, Left/Right adjusts value (fine = ±1, Shift = ±8, Ctrl = ±16). Mouse drag on slider/knob also adjusts. `V` on focused row toggles slider↔knob.
- Header row shows current MIDI channel + MIDI Out port name. Channel changeable with `[` / `]` (matches existing convention).
- Status bar: "Sent CC 74 = 90 on ch 1" on each tweak.

## Widgets

Two new widget classes in `UserInterface.{h,cpp}`:

- `CcSlider` — horizontal bar, fills left-to-right, value 0..127 (or 0..16383 for PB). Renders in ~10 character columns.
- `CcKnob` — circular indicator drawn with arc characters (using the existing font glyphs we already have for VU meters / similar), ~3×3 character cells.

Both share a base `CcRow` that owns CC#, name, value, view-hint, and the send call.

Width budget: file list ~24 cols, grid ~52 cols on the 80-col layout. Knob takes a 3×3 block; slider takes a 10-col strip. Both fit in the same "View" column at different visual densities.

## Persistence of slider/knob choice

**Sidecar file** `<basename>.cc-view` next to each CCizer `.txt`, format:

```
1 slider
2 knob
3 slider
```

Missing entries default to `slider`. Sidecar is written when the user toggles `V`. Paketti's CCizer files stay untouched, so the same folder works for both projects.

Why not zt.conf: keeps per-file customization with the file (portable across machines if user copies the folder), avoids zt.conf bloat with one entry per (file, slot).

## MIDI send path

- Reuse the existing `MidiOut->sendShortMessage(status, data1, data2)` from `playback.cpp`.
- For CC: `status = 0xB0 | (channel - 1)`, `data1 = ccnum`, `data2 = value & 0x7F`.
- For PB: `status = 0xE0 | (channel - 1)`, `data1 = lsb`, `data2 = msb` (14-bit value 0..16383, default 8192 center).
- Channel is per-page state (shared default 1). Possibly bind to current pattern track's MIDI channel later — out of scope for v1.

Sends fire on every value change. Throttle: rate-limit to one message per `data2` value transition; no per-tick spam.

## Recording into pattern (deferred)

Not in v1. Add later as a "Record CC" toggle that captures tweaks into the current track's Z-effect column. Documented in the plan for follow-up but not built.

## Help / teaching

- `doc/help.txt` gets a "CC Console (Shift+F5)" section with the keybinds and a 4-line CCizer format spec.
- `assets/ccizer/README.md` is the in-folder teaching surface — explains the format, points at the bundled examples, and tells the user "drop your own .txt files here, or point CCizer Folder in F11 to a different directory."
- F1 on the page itself shows a one-screen quick reference (CC format + page keys).

## Tests

`tests/test_ccizer.cpp` (new suite, SDL-free):

- Parser: parses `sc88st.txt` correctly, ignores comments, indexes slots, accepts `PB`, rejects out-of-range CC#, handles empty lines and trailing whitespace.
- View-hint sidecar: round-trip read/write, missing-file default, partial-file default-fill.
- MIDI byte builder: `(cc, channel, value) → (status, data1, data2)` for normal CC, PB low/high split, channel clamping.

Wired into existing CTest harness alongside the other 5 suites.

## File breakdown (estimated)

| File | Adds |
|---|---|
| `src/ccizer.{h,cpp}` (new) | Parser, sidecar, MIDI byte builder, folder scan |
| `src/CUI_CcConsole.{h,cpp}` (new) | Page implementation |
| `src/UserInterface.{h,cpp}` | `CcSlider`, `CcKnob` widget classes |
| `src/main.cpp` | Shift+F5 dispatch, switch_page integration |
| `src/CUI_MainMenu.cpp` | "CC Console" entry under appropriate submenu |
| `src/CUI_Sysconfig.cpp` | `ccizer_folder` text input + browse button |
| `src/conf.{h,cpp}` | New `ccizer_folder` key |
| `assets/ccizer/*.txt` + `README.md` | Bundled examples |
| `CMakeLists.txt` | RESOURCES copy, install rules |
| `doc/help.txt` | New section |
| `doc/CHANGELOG.txt` | Entry |
| `tests/test_ccizer.cpp` | New suite |

Estimated total: ~700 LOC + assets.

## PR split

Keeping under Manuel's "1 feature, ~30–300 LOC" preference:

- **PR-A — Format + assets + folder scan + read-only page** (~250 LOC + assets)
  Parser, sidecar reader, bundled `assets/ccizer/`, Shift+F5 page that lists files + displays slot grid as plain text (no widgets, no send, no toggle yet). Tests for parser + sidecar.

- **PR-B — Widgets + send + toggle + persist** (~300 LOC)
  `CcSlider`, `CcKnob`, `V` toggle, MIDI send path, sidecar writeback, channel selector. Builds on PR-A.

- **PR-C — Sysconfig folder picker** (~80 LOC)
  F11 entry for `ccizer_folder`, browse dialog. Small enough to defer if the default folder resolution is good enough.

Each PR has its own CHANGELOG entry, screenshot, and lands behind the previous one.

## Open questions for the user

1. **Page slot:** Shift+F5 OK, or prefer F6 / Shift+F4 / something else?
2. **Default folder:** ship bundled examples from Paketti subset (above) as the default, or default to `~/work/paketti/ccizer` directly so the user has the full set with no copying?
3. **Mouse on widgets:** drag-to-adjust required in v1, or keyboard-only first and add mouse later?
4. **Channel binding:** independent CC Console channel (default 1, user-changeable) — or should it follow the current pattern track's MIDI Out channel?
5. **PR split** — comfortable with the A→B→C sequence, or want it as one bigger PR?
