# Recipe: F2-F2 "Piano (Ableton/Logic) keyjazz" checkbox

**Status:** IMPLEMENTED (branch `esa/keyjazz-piano-layout`, 2026-05-28).
Shared table `src/keyjazz_map.h`; checkbox in `CUI_PEParms.cpp` (element 10,
label `PianoKey:`); conf key `keyjazz_piano`; test suite
`tests/test_keyjazz_map.cpp` (12th CTest target).

**Decisions taken (from the user, 2026-05-28):**
- Z-row in piano mode = **match Logic exactly**: Z/X/C/V are NOT note keys
  (Logic uses Z/X for octave shift, C/V for velocity). Only the home row +
  the row above are notes in piano mode.
- Upper range = **extend through L ; ' and O P** (white to F, black to D#).
- Scope = **all note-entry pages** (Pattern + Instrument + Arpeggio editors).

**Follow-up not in this PR:** the rest of Logic Musical Typing's *control*
keys -- Z/X octave shift, C/V velocity, Tab sustain, 1/2 pitch bend, 3-8
modulation. These collide with existing always-on pattern-editor editing keys
and need their own collision-resolution + scoping decision; the note layout
ships first. zTracker already has base_octave +/- and record-velocity, so some
of those Logic controls map to existing features.

---

## Original design note follows.


**Goal:** add a checkbox to the Pattern Editor Config (F2 F2) that switches the
computer-keyboard note layout from the classic *tracker* layout to the *piano*
layout used by Ableton Live / Logic / GarageBand Musical Typing — so a user who
keyjazzes in those DAWs all day doesn't fumble when they come back to zTracker.

---

## The two layouts, precisely

**Tracker layout (current, the only one today).** Two chromatic rows, each a
full octave climbing left-to-right:

```
top row    Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P     = base_octave,    semitones 0..16
bottom row Z S X D C V G B H N J M               = base_octave-1,  semitones 0..11
```

**Piano layout (the request — `awsedftgyhujk`).** The home row is the *white*
keys, the row above holds the *black* keys, exactly mimicking a piano keyboard's
geography — this is the Ableton/Logic mapping:

```
black     W E   T Y U     O P
white    A S D F G H J K  L ;
          C C# D D# E F F# G G# A A# B  C  ...
```

Decoded the way Esa wrote it: `a`=C `w`=C# `s`=D `e`=D# `d`=E `f`=F `t`=F#
`g`=G `y`=G# `h`=A `u`=A# `j`=B `k`=C. That is `awsedftgyhujk`.

The two are **mutually exclusive** — `S D G H J` (and friends) mean different
notes in each. They cannot coexist on the same keys, which is exactly why a
mode toggle (checkbox) is the right shape, not an additive binding.

---

## Why this is bigger than "swap a table" — the duplication problem

The keyjazz layout is **hardcoded in four places today**, three as inline
`switch(scancode)` blocks and one as an offset function:

| # | File | Lines | Context |
|---|------|-------|---------|
| 1 | `src/CUI_Patterneditor.cpp` | 1756–1784 | mouse-draw-mode audition |
| 2 | `src/CUI_Patterneditor.cpp` | 3408–3444 | T_NOTE cell direct entry |
| 3 | `src/CUI_InstEditor.cpp` | 371–404 | instrument-list audition |
| 4 | `src/CUI_Arpeggioeditor.cpp` | 144–179 | `ar_keyjazz_offset_for_scancode()` |

Adding a second layout by editing four copies is exactly the foot-gun
`CLAUDE.md` warns about ("extract the decision logic into a header… add tests").
**So step 1 of the feature is a pure refactor that should ship even on its own:
collapse all four onto one shared, SDL-free, unit-tested table — then the layout
toggle is a one-line branch inside that single function.**

Good news: all four already speak the **same coordinate convention** once you
look closely. The arp function returns an *offset from `12*base_octave`*
(top row 0..16, bottom row −12..−1). The Pattern/Inst switches compute
`12*base_octave + N` for the top row and `12*(base_octave-1) + N` for the
bottom — which is algebraically identical (`12*(base_octave-1)+N == 12*base_octave + (N-12)`).
So a single function `int keyjazz_offset(scancode, layout)` returning offset-from-
`12*base_octave` (or a sentinel for "not a note key") can feed all four callers
unchanged in behavior.

---

## Proposed architecture

### 1. New header `src/keyjazz_map.h` (SDL-free, testable)

Mirrors the existing pattern of `preset_data.h` / `preset_selector.h` /
`save_key_dispatch.h` — pure logic, no SDL, compiled into a CTest suite under
`ZT_TEST_NO_SDL` against `tests/sdl_stub.h`.

```cpp
enum KeyjazzLayout { KJ_TRACKER = 0, KJ_PIANO = 1 };

constexpr int KJ_NOT_A_NOTE = -127;   // matches arp's existing sentinel

// Returns semitone offset from (12 * base_octave), or KJ_NOT_A_NOTE.
// Callers do:  int n = 12*base_octave + off;  if (off == KJ_NOT_A_NOTE) skip;
inline int keyjazz_offset(int scancode, KeyjazzLayout layout);
```

`KJ_TRACKER` reproduces the current table verbatim. `KJ_PIANO` is:

```
home  (white) A=0  S=2  D=4  F=5  G=7  H=9  J=11 K=12        (C..C)
upper (black) W=1  E=3  T=6  Y=8  U=10  O=13  P=15           (C#,D#,F#,G#,A#, then C#,D#)
extend(white) L=14 SEMICOLON=16                              (D, E)
lower octave  Z=-12 X=-10 C=-8 V=-7 B=-5 N=-3 M=-1           (C..B, one octave down — optional)
```

The lower-octave Z-row in piano mode is a **design choice to confirm with Esa**
(see "Open questions"). Ableton/Logic themselves reserve Z/X for *octave shift*,
not notes — but zTracker shifts octave via `base_octave` +/- already, so the
Z-row is free to double the playable range like the tracker layout does. Two
options:
  - **(a) Match Logic exactly:** Z-row unused for notes in piano mode (cleaner
    muscle-memory transfer for DAW users; only one octave on the keyboard).
  - **(b) zTracker bonus:** keep Z-row as the lower octave (two-octave span like
    today). Recommended default — strictly more notes, no downside since zTracker
    doesn't use Z/X for octave shift.

### 2. Config flag — wire exactly like `centered_editing`

The end-to-end path an existing checkbox follows (verified):

- `src/conf.h` (~line 88): `int keyjazz_piano_layout;`
- `src/conf.cpp` ctor (~244): `keyjazz_piano_layout = 0;`  // tracker by default
- `src/conf.cpp` load (~360): `if (Config->get("keyjazz_piano")) keyjazz_piano_layout = getFlag("keyjazz_piano");`
- `src/conf.cpp` save (~529): `Config->set("keyjazz_piano", keyjazz_piano_layout ? "yes" : "no");`

Default **off** = zero behavior change for every existing user.

### 3. The checkbox in `CUI_PEParms.cpp`

The config panel already has 5 checkboxes (`Centered`, `StepEdit`, `RecVeloc`,
`DrawMode`, `CCOver`) laid out on two rows at `y = +14` and `y = +18`. Add a
sixth, e.g. label `"Piano keys:"`, next free UI element id (`10`), following the
`cb_centered` recipe end-to-end:

- declare `CheckBox *cb_keyjazz_piano;` in `CUI_Page.h` (class `CUI_PEParms`, ~387)
- create in the ctor, `UI->add_element(cb_keyjazz_piano, 10)`,
  `->value = &zt_config_globals.keyjazz_piano_layout;`
- re-point `cb->value` in `enter()`
- absorb `if (cb->changed) zt_config_globals.keyjazz_piano_layout = *(cb->value);` in `update()`
- `print(...,"   Piano keys:",...)` label + position in `draw()`

There's room on the `y = +18` row next to `CCOver`, or start a third row at
`y = +22` if it reads cleaner. (Bump `need_refresh++` is handled by the widget;
PEParms is a popup so the INVARIANT is already satisfied by `enter()`.)

### 4. Each of the four callers becomes a one-liner

Replace the inline `switch` / function body with:

```cpp
KeyjazzLayout layout = zt_config_globals.keyjazz_piano_layout ? KJ_PIANO : KJ_TRACKER;
int off = keyjazz_offset(scancode, layout);
if (off != KJ_NOT_A_NOTE) { int note = 12*base_octave + off; /* existing use */ }
```

In `CUI_Patterneditor.cpp:3408` the call sits *before* the existing "EDITING
KEYS" cases (`1`→0x81, grave→0x82, period→0x80, `4`/`8` peek-play). Those special
keys are **not** musical note keys and must survive in both layouts, so they stay
as their own cases after the `keyjazz_offset` call returns the sentinel for them.

---

## The collision audit — the real risk, and what to check

Piano mode recruits keys the tracker layout never used as notes: **A, K** (and
**L, ;** if extending). Before shipping, confirm none of these already do
something in the note-entry context:

- In the `T_NOTE` scancode switch (`CUI_Patterneditor.cpp:3406+`) the only
  non-note cases are `1`, GRAVE, PERIOD, `4`, `8` — **A/K/L/; are clear there.** ✓
- Piano mode uses **no number keys at all** (black keys come from W/E/T/Y/U/O/P),
  so the entire number row stays free — `4` and `8` peek-play keep working,
  unlike tracker mode where they're wedged between note keys. A nice side benefit.
- **Still to verify:** that `A` and `K` aren't bound as *global* Pattern Editor
  commands elsewhere in `update()` (outside the T_NOTE column switch), and the
  same for the Instrument Editor audition path (`CUI_InstEditor.cpp:371`). Grep
  `SDL_SCANCODE_A` / `SDL_SCANCODE_K` across the editors. If either is a command,
  decide precedence (in piano mode, note entry on the note column should win; the
  command can keep working on non-note columns).

---

## Tests (new CTest suite `test_keyjazz_map`)

Following the eight existing suites:
- tracker layout reproduces the legacy table exactly for all ~29 keys + sentinel
  for everything else (this is the regression guard for the refactor);
- piano layout maps `awsedftgyhujk` → 0,1,2,3,4,5,6,7,8,9,10,11,12;
- both share the same sentinel value and the same offset-from-`12*base_octave`
  convention;
- a couple of "this key means different notes in the two layouts" assertions
  (e.g. `S` → 2 in piano, −11 in tracker) to lock the mutual exclusivity in.

---

## Suggested PR split (Manuel likes small, focused PRs)

1. **Refactor only:** introduce `keyjazz_map.h` (tracker layout only) + the
   `test_keyjazz_map` regression suite + repoint all four callers. Pure no-op,
   provable by tests. Low risk, mergeable on its own.
2. **Feature:** add `KJ_PIANO` to the header + its tests, the `keyjazz_piano`
   conf key, and the F2-F2 checkbox. Update `doc/help.txt` and
   `doc/CHANGELOG.txt`.

---

## Open questions for Esa

1. **Z-row in piano mode** — option (a) match Logic (Z/X unused, one octave) or
   (b) zTracker bonus lower octave (recommended)?
2. **How far to extend upward** — stop at `K`=C, or carry on `L ; '` and
   `O P [ ]` to give nearly two octaves on the home+upper rows?
3. **Label wording** in the F2-F2 panel — `"Piano keys"`, `"DAW layout"`,
   `"Ableton/Logic"`? Space is tight (the panel is character-grid).
4. **Should Instrument Editor + Arpeggio Editor honor the same flag?** Almost
   certainly yes (one global feel), and the shared header makes it free — worth
   confirming there's no reason to scope it to the Pattern Editor only.
