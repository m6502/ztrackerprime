# Sample Playback — Incremental PR Plan

**Status:** Proposal. **Companion:** [`sample-playback-design.md`](sample-playback-design.md).
**Style target:** Manuel's preference — 1 feature, ~30–300 LOC, CHANGELOG section per PR,
ctest green before push, no kitchen-sink PRs.

This sequences the sample subsystem into small, independently-reviewable PRs. Each one
builds and passes `ctest` on its own; nothing user-visible ships until the foundation +
fixes are in. Where possible, logic is extracted into SDL-free headers so it can be
unit-tested under `ZT_TEST_NO_SDL` (the project's established pattern).

---

## Pre-req decisions (resolve with Manuel before PR-1)

- [ ] Is sample playback in scope at all / does it change the "MIDI-only" identity? (§4 of design)
- [ ] `_ENABLE_AUDIO` default: on for everyone, or behind a config key / build option?
- [ ] v1 scope: WAV-only, or IT-sample import too?

These gate the whole sequence — answer first.

---

## PR-0 — Wake the backend safely (foundation + the two spike fixes)

**Goal:** turn the dormant audio path on *correctly*, with no user-facing feature yet.
Just: audio device exists, it's harmless, and it doesn't scramble anyone's saved songs.

- Enable `_ENABLE_AUDIO` (config-keyed or build-option per the pre-req decision — not a
  bare `#define` if we want pure-MIDI users unaffected).
- **Fix device ordering** (`InitializePlugins`): register audio devices *after* MIDI so
  existing `midi_device` indices in saved songs don't shift. (design §5.1)
- **Fix the S16/byte format bug** in `TestToneOutputDevice::work` (write interleaved
  `Sint16` frames), or delete TestTone/Noise outright if we don't want demo generators
  shipping. Recommendation: delete them in the same PR that adds the real engine (PR-3),
  keep a corrected TestTone here only if useful as a manual smoke test.
- Delete the dead `Mix_LoadWAV` / `sdl_mixer.h` references (SDL3 has no SDL_mixer here).

**LOC:** ~40–80. **Tests:** build matrix green on all 5 platforms; manual: app launches,
no audio-related stderr, saved song's device routing unchanged. **Risk:** low.

---

## PR-1 — Sample data model + `.zt` `SMPL` chunk (no playback yet)

**Goal:** a place to put samples and round-trip them through save/load. Still silent.

- `src/sample.h` — `zt_sample` struct (design §3.1).
- `module` gains the sample pool; `instrument` gains `signed short int sample_index` (-1).
- New optional `SMPL` chunk in `.zt` save/load, following the `CCBN` precedent (PR #84):
  length-prefixed S16 PCM + metadata; old zTracker skips it via `readblock` advance-past.
  No chunk written when the pool is empty (byte-identical saves for MIDI-only songs).
- Pure (SDL-free) serialize/parse helpers in a header so they unit-test under
  `ZT_TEST_NO_SDL`.

**LOC:** ~150–250. **Tests:** new `test_sample_chunk` suite — empty pool writes nothing,
round-trip of N samples preserves bytes/loop/root, forward-compat (old reader skips chunk),
`sample_index` survives save/load. **Risk:** medium (format change — must be backward+forward
compatible; copy the CCBN test discipline exactly).

---

## PR-2 — WAV loader

**Goal:** get PCM into the pool from a file. Still no playback (no engine yet), but loadable
and saveable.

- `src/sample_load.{h,cpp}` — `SDL_LoadWAV` → convert to S16 (`SDL_ConvertAudioSamples`) →
  `zt_sample`. Default `root_note=60`, `rate` from the WAV header, no loop.
- A minimal CLI/test hook or a temporary debug path to load a WAV into pool slot 0 (real UI
  comes in PR-5).

**LOC:** ~100–150. **Tests:** `test_sample_load` — load a tiny committed test `.wav`
(mono + stereo), assert frames/rate/channels; reject malformed. (Test WAVs are a few hundred
bytes, fine to commit.) **Risk:** low–medium (format conversion edge cases).

---

## PR-3 — The engine: `SampleOutputDevice` (first sound!)

**Goal:** notes routed to the sample device actually play the assigned sample, pitched.

- `SampleOutputDevice : AudioOutputDevice` in `OutputDevices.{h,cpp}` (design §3.2): voice
  array, linear interpolation, loop handling, velocity scaling, **interleaved S16 output**.
- `progChange` sets `sample_for_chan[]` (design §3.3); `noteOn` allocates a voice at the
  note's pitch; `noteOff` releases.
- Replace the TestTone/Noise demo devices with this real one in `InitializePlugins`.
- Audio-thread/UI-thread safety for voice state + pool lifetime (mutex or SPSC command
  queue — reuse the `midiInQueue`/PR #116 lesson). Design §5.2.
- Extract the **pure DSP** (pitch→step math, interpolation, loop-wrap) into a SDL-free
  header so it unit-tests without an audio device.

**LOC:** ~200–300. **Tests:** `test_sample_engine` (pure DSP) — step computation for known
note/root/rate combos, interpolation at fractional positions, loop wrap-around, end-of-sample
deactivation, voice stealing when all voices busy. **Risk:** medium–high (real-time thread
safety + correctness). This is the keystone PR; keep it tight and well-tested.

---

## PR-4 — Playback routing wire-up

**Goal:** make an ordinary tracked instrument target the sample engine end-to-end.

- In `playback.cpp`, when an instrument's `sample_index >= 0` and it's routed to the sample
  device, ensure a `progChange(sample_index, 0, chan)` is emitted at instrument-select /
  note time so the engine knows which sample (most of the bank/program path already exists).
- Pitch-bend / volume CC → engine (`pitchWheel` scales voice `step`; volume scales gain).
  Optional: tie into the existing CC-envelope system.

**LOC:** ~80–150. **Tests:** mostly manual (play a pattern, hear the sample at correct
pitches); any pure routing-decision logic extracted + unit-tested per project habit.
**Risk:** medium.

---

## PR-5 — Sample editor page

**Goal:** load / assign / edit samples without leaving the app.

- `CUI_SampleEditor.cpp` (proposed **Shift+F7**, or folded into Instrument Editor).
- **Strict adherence to the page invariants** (design §3.6 / skill INVARIANTS): real
  `ListBox` subclass for the sample list, real `ValueSlider`/`TextInput` for loop
  start/end/root/vol/finetune, `need_refresh++` on every mutating key, per-frame
  `need_redraw=1`, column-per-`print`, add `STATE_SAMPLE_EDITOR` to the global-ESC
  exclusion list if it has its own ESC handling.
- Load button → file picker → PR-2 loader. Assign-to-instrument action sets `sample_index`.
- Waveform display: nice-to-have, can be a follow-up.

**LOC:** ~250–350 (UI pages run large; split if it balloons). **Tests:** visual-verification
gate (INVARIANT #8) — launch, drive every key, screenshot-diff via `scripts/zt-screenshot.sh`.
**Risk:** medium (UI correctness, the foot-gun-heavy area).

---

## PR-6 — IT sample import (optional, follow-up)

**Goal:** loading an `.it` brings its samples in playable.

- Revive `ITSampleHeader::LoadSampleData()` (un-comment the `fread`, return real status).
- On `.it` import, copy header (loop points, C5speed→rate, root) + PCM into the pool and set
  instrument `sample_index`.
- v1: uncompressed 8/16-bit PCM. Defer IT215 compressed samples.

**LOC:** ~120–200. **Tests:** `test_it_sample` — load a small committed `.it` with one
sample, assert PCM length + loop metadata. **Risk:** medium (IT format quirks).

---

## PR-7 — Polish (each its own small PR, as desired)

- Cubic/sinc interpolation option.
- Ping-pong / sustain loops.
- Per-key sample mapping (drum kits) — extends the channel-map to a key→sample table.
- Waveform display + visual loop-point editing in the sample editor.
- Deflate PCM in the `SMPL` chunk (zlib already linked).
- Cross-platform audio smoke tests (Win WASAPI/WinMM, Linux ALSA) — design §5.5.

---

## Dependency graph

```
PR-0 (wake + fixes)
  └─ PR-1 (data model + .zt chunk)
       ├─ PR-2 (WAV loader) ─────────┐
       └─ PR-3 (engine) ─────────────┤
                                      ├─ PR-4 (routing)  → audible end-to-end
                                      └─ PR-5 (editor)   → usable
PR-6 (IT import)  depends on PR-1 + PR-3
PR-7 (polish)     depends on PR-3/PR-5
```

**Minimum "it plays samples" milestone:** PR-0 → PR-1 → PR-2 → PR-3 → PR-4.
**Minimum "a user can use it" milestone:** + PR-5.

---

## Testing discipline (applies to every PR)

- Build clean + `ctest --output-on-failure` green before push (CLAUDE.md rule).
- Extract decision/DSP logic into SDL-free headers and add a CTest suite — the project's
  established pattern (presets, selector, ccizer, sysex_*). Bug-class regression scaffolding
  has paid off repeatedly here.
- UI PRs: visual-verification gate (launch, drive keys, screenshot-diff). Build-green ≠ works.
- `doc/CHANGELOG.txt` section per PR; `doc/help.txt` updated for any new keybind/page.
- Branch `esa/<short-descriptive>`; push to `origin`; PR against `master`; don't merge
  without Manuel's approval.
