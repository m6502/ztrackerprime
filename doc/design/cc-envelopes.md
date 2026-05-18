# CC Envelopes — design theory

Status: design sketch, not implemented. Author: Claude (with Esa, 2026-05-18).

## Goal

A note triggers an **envelope** that sends MIDI CC values over time, interpolated linearly between user-placed nodes, with sustain-loop on note-on and loop/fadeout on note-off. Effectively: per-instrument (or per-effect-arg) automation curves driven by the tracker's tick clock.

User's framing: "trigger note with C-4 → envelope sends to CC #x which is followed with the runtime."

## Why this fits ztrackerprime

`src/playback.cpp:1510-1572` already does the hard part. The R-effect (`Rxxxx`) walks an arpeggio at `speed` subticks per step and emits `ET_CC` events into the playback buffer via `buffer->insert(step_tick, ET_CC, device, cc, value, chan, t, inst)`. That's the exact mechanism a CC envelope needs — the only differences are:

| | Arpeggio (`R`) | CC Envelope |
|---|---|---|
| Storage | dense `ccval[c][step]` array, fixed length | sparse `node[i] = {x:tick, y:value}` |
| Interpolation | none — stepped | linear between adjacent nodes |
| Release | none — schedule-and-forget, max 256 steps | sustain region holds until note-off, then loop/fadeout |
| Trigger | `Rxx` effect column | implicit on note-on (per-instrument), or explicit effect (e.g. `Vxxyy` = envelope V on CC yy) |

Schism's IT envelopes (volume/pan/pitch) are the canonical reference (`schismtracker/include/player/sndfile.h:485-493`, `player/sndmix.c:470-502`). They use 32 nodes × `{tick:int32, value:uint8}` + loop start/end + sustain start/end + flags (enabled/loop/sustain/carry). Per-voice runtime state is one int: `vol_env_position`.

## Proposed data model

Mirror Schism's struct, adapt range to MIDI CC (0–127):

```cpp
#define ZTM_CCENV_MAX_NODES   32
#define ZTM_MAX_CCENVELOPES   64   // global pool, like arpeggios/midimacros

class ccenvelope {
public:
    char name[ZTM_CCENVNAME_MAXLEN];
    unsigned char cc;              // CC# this envelope drives (0-127)
    unsigned char flags;           // bit 0=enabled, 1=sustain, 2=loop, 3=carry
    unsigned char num_nodes;       // 2..32
    unsigned char loop_start, loop_end;
    unsigned char sustain_start, sustain_end;
    unsigned short tick[ZTM_CCENV_MAX_NODES];  // x-axis: subticks from note-on
    unsigned char  value[ZTM_CCENV_MAX_NODES]; // y-axis: 0-127 MIDI CC value
    int speed;                     // subticks per tick of envelope time (like arp.speed)
    int isempty(void);
};
```

Storage cost: 32×3 = 96 bytes/envelope + ~10 bytes header. 64 envelopes = ~7 KB max. Trivial.

## Persistence

New optional chunk `CENV` in the `.zt` save format. **The existing `readblock` machinery silently skips unknown chunks** (this is how `CCBN` was added in the CCizer wave without breaking old zTracker — see `module.h:399`-area pattern). Old binaries open new files; new binaries open old files with envelopes absent.

Mirror the arpeggio chunk pattern in `module.cpp` (`build_arpeggio` / `load_arpeggio` ~ line 383/399):
- `build_CC_envelope(CDataBuf *buf, int num)` → `CENV` chunk per envelope
- `int load_CC_envelope(CDataBuf *buf)` → restore into `ccenvelopes[idx]`

## Per-instrument binding

Add `instrument.ccenv_default[N]` — a small list (4-8) of envelope indices that auto-arm when an instrument's note triggers. Saved via the same `CCBN`-style optional chunk (call it `IENV`).

Alternative: add a tracker effect column letter (currently free letters in `playback.cpp` switch around line 1510) — e.g. `Vxxyy` = arm envelope `xx` (or `yyzz` for index + CC# override). The arpeggio R-effect is the model. Both can coexist; effect column wins over per-instrument default if both present.

## Runtime — the critical piece

Two options. Pick **(B)** for robustness.

### (A) Schedule-and-forget, like R-effect

Walk the envelope at note-on and push every `ET_CC` event into the playback buffer ahead of time. Pros: zero per-tick CPU during playback; reuses existing scheduler. Cons: no honest note-off response (you'd have to scan and cancel future events, ugly), and re-trigger semantics get fragile. The R-effect tolerates this because arpeggios are step-based and the user re-triggers manually. Envelopes are continuous; users will expect note-off to gate them.

### (B) Per-track runtime state, advance every subtick — RECOMMENDED

Add to `player` (or a parallel struct keyed by track index):

```cpp
struct cc_env_voice {
    int     env_idx;        // -1 = inactive
    int     position;       // current subtick within envelope timeline
    int     last_emitted;   // last value sent (suppress duplicates)
    bool    key_off;        // note-off received; transition out of sustain
    int     base_tick;      // p_tick at note-on (for scheduler alignment)
};
cc_env_voice cc_env[ZTM_MAX_TRACKS][ZTM_CCENV_PER_TRACK];
```

Per-subtick in the existing `playback()` loop (the loop where `cur_subtick` advances around `playback.cpp:1230-1290`):

1. For each active `cc_env_voice`: compute `value = interp(env, position)` (linear between the two bracketing nodes).
2. If `value != last_emitted`, `buffer->insert(p_tick, ET_CC, device, env->cc, value, chan, t, inst)`.
3. Advance `position++`.
4. Sustain handling:
   - If `!key_off` and `flags & SUSTAIN` and `position >= tick[sustain_end]`: wrap to `tick[sustain_start]`.
   - Else if `flags & LOOP` and `position >= tick[loop_end]`: wrap to `tick[loop_start]`.
   - Else if `position >= tick[num_nodes-1]`: deactivate (emit final value once).
5. Note-on on this track: allocate a `cc_env_voice` slot per envelope listed on the instrument (or by `V`-effect); reset `position=0`, `key_off=false`.
6. Note-off on this track: set `key_off=true` on all active envelopes for that track. Sustain branch then falls through to loop/fadeout.

This is **exactly** Schism's `_process_envelope` (`sndmix.c:470-502`) ported to the zt scheduler. Interpolation math from `sndmix.c:264-265`:

```cpp
envval = values[pt-1] + ((pos - tick[pt-1]) * (values[pt] - values[pt-1])) / (tick[pt] - tick[pt-1]);
```

### Why (B) is robust

- **Honest note-off**: sustain loop releases when key is up. This is the feature.
- **Carry flag** (Schism's `ENV_VOLCARRY`): new note resumes at previous position. Cheap to add (skip `position=0` reset on retrigger).
- **No buffer pollution**: events are inserted just-in-time, not 256 deep.
- **Thread-safety**: runtime state lives entirely in the playback thread (which already owns `cur_subtick` etc.). No new mutex.
- **Stop / Panic**: `player::stop()` clears `cc_env[][]`. Existing `F9` (panic) flow already covers the MIDI side.

## Editor UI

Mirror the Arpeggio Editor (`CUI_Arpeggioeditor.cpp`, Shift+F4). New page `CUI_CCEnvelopeEditor.cpp` on a free slot — candidates with Shift+F6 free (F6 is Play Pattern). Layout:

- Header: envelope index, name, CC#, flags (enabled / sustain / loop / carry), node count.
- Grid: time on X (subticks), CC value on Y (0–127). Nodes drawn as bullets, lines between them.
- Editing: Arrow keys move selection; Enter adds a node at cursor; Del removes; `L` toggles loop start/end at selected node; `S` toggles sustain start/end; `Tab` cycles between envelopes; mouse drag to move a node.
- Speed field (subticks per envelope tick).

Don't ship ASCII art that *looks* clickable — see the existing `INVARIANT: every page key handler must need_refresh++` and rule 5 in CLAUDE.md. The node markers must be real `UserInterfaceElement` widgets or a custom widget that registers itself with `UI->add_element` and handles its own mouse hit-test + `need_refresh++`.

## Test scaffolding

Pure-logic headers, SDL-free, unit-tested under `ZT_TEST_NO_SDL` — same pattern as `preset_data.h` / `preset_selector.h` / `save_key_dispatch.h`:

- `ccenv_interp.h` — `int ccenv_value_at(const ccenvelope&, int position)` linear interp. Test: 2-node ramp, 3-node V-shape, plateau (equal y nodes), out-of-range positions clamped.
- `ccenv_advance.h` — `void ccenv_step(ccenv_voice&, const ccenvelope&)` one-subtick advance including sustain/loop/fadeout. Test: sustain holds with key down, releases on key-off, regular loop wraps, no-loop terminates, carry flag resumes.
- `ccenv_persist.h` — `CENV` chunk round-trip (write→read produces equal struct). Test all flag combinations + min/max node counts.

Add `test_ccenv` to the CTest harness alongside the existing 8 suites. Linux CI runs it automatically.

## Sketch of the staged rollout (small PRs, per Manuel's preference)

1. **PR-1**: `ccenvelope` class in `module.h/cpp`, `CENV` chunk save/load, `test_ccenv` (round-trip). No runtime, no UI. ~150 LOC.
2. **PR-2**: `ccenv_interp.h` + `ccenv_advance.h` pure logic + tests. ~100 LOC.
3. **PR-3**: Playback integration — `cc_env_voice` state in `player`, hook into the per-subtick loop, allocate on note-on, release on note-off. Manual test via a default-bound envelope. ~200 LOC.
4. **PR-4**: Per-instrument default-envelope binding (`IENV` chunk + Instrument Editor field). ~100 LOC.
5. **PR-5**: CC Envelope Editor page on Shift+F6. Widget-correct (no fake ASCII clickables). ~400 LOC.
6. **PR-6 (optional)**: `V`-effect column dispatch (parallel to R for arpeggios) so envelopes can be armed mid-pattern. ~80 LOC.

Each PR ships independently. PR-1 alone gives forward-compat persistence so old zTracker won't choke on files saved by future versions.

## Open questions for Esa

1. **CC# locked to envelope, or per-arm?** Current sketch locks `cc` into the `ccenvelope` struct. Alternative: envelope is shape-only, CC# specified at arm time (effect column / instrument binding). The arm-time version is more flexible; the locked version is what Schism does.
2. **One envelope per instrument or many?** Schism allows three (vol/pan/pitch). For CC, "many simultaneous" (4-8 per instrument) is realistic — e.g. arm filter cutoff + resonance + modulation depth from one note.
3. **Tempo basis for X axis.** Subticks (BPM-locked) is the natural choice and matches arpeggio `speed`. Real-time milliseconds would survive tempo changes but break musical timing. Subticks recommended.
4. **Pitchbend envelope as a free bonus?** Same engine, different output event type (`ET_PB` instead of `ET_CC`), value range -8192..+8191 instead of 0..127. The runtime is identical; only the emit step differs. A `kind` field in the struct toggles between CC / Pitchbend / Channel Pressure.

## References

- Schism volume/pan/pitch envelope: `~/work/schismtracker/include/player/sndfile.h:485-493`, `player/sndmix.c:264-265,470-513`
- Schism IT envelope persistence: `~/work/schismtracker/fmt/iti.c:36-48,192-234,420-502`
- Existing arpeggio R-effect (closest precedent in this repo): `src/playback.cpp:1510-1572`
- Arpeggio class + persistence: `src/module.h:247-261`, `src/module.cpp:671-733`, `build_arpeggio`/`load_arpeggio` ~ `module.cpp` around the ARPG chunk
- Forward-compat-chunk pattern: `CCBN` instrument bank field (CLAUDE.md → CCizer section)
- Editor UI invariants: CLAUDE.md → "INVARIANT: every page key handler must need_refresh++" (rules 1 + 5)
- Test scaffolding pattern: `tests/test_presets.cpp`, `tests/test_ccizer.cpp`, etc.
