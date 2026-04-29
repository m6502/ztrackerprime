# CCizer follow-up work — TODO after CC Console v1 ships

The CC Console PR (this branch: `esa/ccizer-console`) lands the read+send surface only.
The four sub-features below were scoped out and are tracked here for follow-up PRs.

## TODO 0 — F11 Sysconfig picker for `ccizer_folder`
Add a TextInput + Browse button on the F12 System Configuration page bound to
`zt_config_globals.ccizer_folder`. v1 only ships the underlying conf key + the
resolution chain (`./ccizer` next to binary, `Resources/ccizer` in macOS .app,
`~/.config/zt/ccizer`); to override you edit `zt.conf` directly.

## TODO 1 — Per-instrument CCizer bank assignment
Add a "CCizer file" field to each instrument (F3 Instrument Editor). When set, that
file's slots become "1st CC for instrument N", "2nd CC for instrument N", etc.
Storage: extend `instrument` struct + zt save format with a string path field.
Migration: empty string = no bank (backward compatible).

## TODO 2 — Shift+§ CC drawing mode
Overlay on the existing Shift+§ drawmode. When CC Console has a slot focused (or via
auto-select from TODO 4), tweaking the value writes an `S<cc><val>` effect into the
current track at the cursor row, with two follow modes:
- **Scroll-follow**: auto-advance one row per value change.
- **Step-follow**: one row per tracker step (synced to playback or local clock).
.zt format already supports this — `S` effect = `Sxxyy` send CC xx with value yy
(see `playback.cpp:1490`). No format change needed.

## TODO 3 — CCizer slot MIDI Learn
Add a third "MIDI binding" column to the CC Console grid (same pattern as the
Shortcuts & MIDI Mappings page on Shift+F2). Bind incoming `(status, data1)` to a
specific `(file, slot)`. Persist as a sidecar `<file>.cc-midi` next to `.cc-view`,
or in `zt.conf` keyed by `<file>:<slot>`.

## TODO 4 — Auto-select-and-draw on mapped knob tweak
When Shift+§ drawmode is on AND a controller knob bound via TODO 3 is tweaked:
- Auto-select that slot in the CC Console.
- Pipe the incoming value through the same write path as TODO 2 (writes
  `S<cc><val>` at cursor, advances by follow mode).
This is the headline workflow: physical knob → automatic pattern recording.

## TODO 5 — Multi-CC simultaneous draw (research)
.zt's one-effect-per-row limit means only one CC per track-row. To draw multiple CCs
in lockstep, either (a) use multiple tracks (one per CC), or (b) extend the format
with a multi-effect column. (b) is invasive; (a) is the natural .zt idiom.
No PR planned — document and decide later.
