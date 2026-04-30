# CCizer files for zTracker Prime

This folder holds **CCizer** files — small `.txt` lists that name MIDI Control
Change numbers so the **CC Console** (Shift+F3) can show meaningful labels next
to its sliders and knobs.

The files in this folder are reference banks copied from the
[Paketti](https://github.com/esaruoho/paketti) project. Drop your own `.txt`
files here — or point the **CCizer Folder** field on the F12 System
Configuration page at any directory of your choosing.

## Format

One slot per line. Lines starting with `#` are comments. Blank lines are OK.

```
<CC# 0..127 | "PB"> <name>
```

Example (`sc88st.txt`):

```
PB Pitchbend
1  Mod
7  Volume
10 Pan
74 Cutoff
71 Resonance
73 Attack
```

* `PB` = pitchbend (status byte `0xE0`, 14-bit value).
* A decimal `0..127` = a Control Change (status `0xB0`, 7-bit value).
* The slot number is its position among non-comment / non-blank lines, starting
  at 1. Slot 1 in the example above is "Pitchbend"; slot 5 is "Cutoff".

## Slider vs. knob

Each slot can be displayed as a horizontal **slider** or a small **knob**. The
choice is per slot per file. Press `V` on the focused row in the CC Console
to toggle. The choice is written to a sidecar file named
`<basename>.cc-view` next to the `.txt` so the Paketti `.txt` itself stays
untouched.

## Adding your own

Copy this `README.md`'s example, save as `mysynth.txt`, and you're done. The
CC Console picks it up the next time it scans the folder.

## Bundled files

These come from Paketti and are kept in sync occasionally. They're a starting
point, not the source of truth — keep your own banks under version control if
you tweak them heavily.

| File | Device |
|---|---|
| `sc88st.txt` | Roland SC-88 / general MIDI baseline |
| `microfreak.txt` | Arturia MicroFreak |
| `minilogue.txt`, `monologue.txt` | Korg minilogue / monologue |
| `Prophet6.txt` | Sequential Prophet-6 |
| `deepmind6.txt` | Behringer DeepMind 6 |
| `waldorf_blofeld.txt` | Waldorf Blofeld |
| `tb03.txt` | Roland Boutique TB-03 |
| `se02.txt` | Roland Boutique SE-02 |
| `pro800.txt` | Behringer Pro-800 |
| `polyend_synth_mode.txt` | Polyend Synth |
| `polyend_performance_mode.txt` | Polyend Tracker performance |
| `midi_control_example.txt` | Generic teaching file |
