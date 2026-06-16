# Sample Playback — Design Doc

**Status:** Proposal / discussion. Nothing here is merged.
**Author:** drafted with Claude (Opus 4.8), 2026-05-29, at Esa's request.
**Audience:** Manuel (maintainer), @Debvgger, Esa.
**Companion:** [`sample-playback-pr-plan.md`](sample-playback-pr-plan.md) — the incremental landing plan.

---

## 0. TL;DR

zTracker is "MIDI-only" by design, but a **complete, dormant SDL3 audio output path
already exists** in the tree behind a commented-out `_ENABLE_AUDIO` flag. A spike
(branch `spike/enable-audio-backend`) flipped the flag on and confirmed the path is
alive under current SDL3 (3.4.4): the device opens via CoreAudio at 44100/S16/stereo,
the mixer callback pumps continuously, and the bundled TestTone/Noise generators
produce audible output.

Adding **sample playback** therefore is *not* a from-scratch audio build. It is:

1. a place to **store** sample PCM (a song-level sample pool + per-instrument link),
2. a **loader** (WAV import via SDL3's native `SDL_LoadWAV`; revive the dead IT-sample reader),
3. a real **engine** — a `SampleOutputDevice : AudioOutputDevice` whose `work()` does
   polyphonic, pitch-resampled voice mixing,
4. a **sample editor** page (load / assign / set loop points / root note),
5. a couple of **plumbing fixes** the spike exposed (device-index ordering, S16 format).

It is a real, multi-PR feature touching file format, the audio engine, and the UI —
and it is as much a **product decision for Manuel** (does the "MIDI-only" identity
change?) as an engineering one. This doc lays out the architecture so that decision can
be made against something concrete.

---

## 1. What already exists (verified in-tree, 2026-05-29)

### 1.1 The output-device polymorphism

`src/midi-io.h`:

- `OutputDevice` (line 65) — abstract base. Pure-virtual `noteOn / noteOff / afterTouch /
  pitchWheel / progChange / sendCC`; `sendSysEx` defaults to a no-op. Carries
  `notestates[128][16]`, `type`, `opened`.
- `AudioOutputDevice : OutputDevice` (line 124) — adds exactly one pure virtual:
  `virtual void work(void *udata, Uint8 *stream, int len) = 0;` — the buffer-fill callback.
- `midiOut::MixAudio(udata, stream, len)` (line 159) — walks the open-device list and,
  for every device whose `type == OUTPUTDEVICE_TYPE_AUDIO`, calls its `work()` to fill
  the buffer. **This is the mixing entry point an engine plugs into.**

### 1.2 The SDL3 backend

`src/main.cpp` (gated by `_ENABLE_AUDIO`):

- `audio_mixer()` (line ~3764) — SDL audio-stream callback. `malloc`s a scratch buffer,
  calls `MidiOut->MixAudio(...)`, pushes the result with `SDL_PutAudioStreamData`.
- `zt_backend_audio_start()` (line ~3780) — `SDL_OpenAudioDeviceStream(..., 44100,
  SDL_AUDIO_S16, 2, ...)` + `SDL_ResumeAudioStreamDevice`.
- `zt_backend_audio_stop()` — teardown. Both wired into the main-loop init/shutdown.
- `SDL_Init` already requests `SDL_INIT_AUDIO`.

### 1.3 The dummy generators

`src/OutputDevices.{h,cpp}` (gated by `_ENABLE_AUDIO`):

- `TestToneOutputDevice` — `noteOn` sets `makenoise=1`; `work()` copies a 256-entry square
  wave table into the buffer.
- `NoiseOutputDevice` — `work()` fills with `rand()&0x3F`.
- `InitializePlugins()` registers both, **then** appends the MIDI devices.

These are proof-of-concept toys. They are where the `SampleOutputDevice` slots in.

### 1.4 The note-dispatch seam (the crux)

`src/playback.cpp` (line ~695) fires:

```cpp
MidiOut->noteOn(inst->midi_device, set_note, inst->channel, vol, track, 0);
```

`midiOut::noteOn(dev, note, chan, vol, ...)` (`midi-io.h:200`) routes to
`outputDevices[dev]->noteOn(note, chan, vol)` if that device is open.

**Critical consequence:** the device is selected by the instrument's `midi_device` field,
and the device's `noteOn` receives **only `(note, chan, vol)`** — *not* the instrument
index, *not* a sample id. The engine must derive "which sample" from `(chan)` plus state
it was told earlier. See §3.3.

### 1.5 The IT sample reader (currently disabled)

`src/it.{h,cpp}`:

- `ITSampleHeader` (`it.h` ~280) has the full metadata: `length`, `loopBegin/loopEnd`,
  `C5speed`, `sustainLoopBegin/End`, `sampleOffset`, `BYTE* sampleData`.
- `ITSampleHeader::LoadSampleData()` (`it.cpp:401`) has its `fread` body **commented out**
  with `// We dont use the sample data, so why bother...` and just `return 0;`.
- On `.it` import (`import_export.cpp`) only metadata is used — sample *name* → instrument
  title, sample *volume* → MIDI velocity scaling.

So IT files already give us correctly-parsed sample headers for free; only the data read
is switched off.

### 1.6 What is absent

- No PCM storage in the in-memory `instrument` (`module.h:142` — `midi_device`, `channel`,
  `bank`, envelopes, `ccizer_bank[256]`, but no sample pointer/length/loop).
- No song-level sample pool.
- No resampling / voice allocation / pitch-from-note logic.
- No `.zt` chunk for sample data.
- No sample-editor UI.

---

## 2. Spike findings (branch `spike/enable-audio-backend`)

Flipping `_ENABLE_AUDIO` on, building, and running on macOS / SDL3 3.4.4:

| Check | Result |
|---|---|
| Builds clean with flag on | ✅ (only an unrelated `/usr/local/opt/zlib` search-path warning) |
| `SDL_OpenAudioDeviceStream` opens | ✅ via **coreaudio**, 44100/S16/stereo |
| `audio_mixer` callback pumps | ✅ ~150 calls in 4 s, 4096 bytes/call |
| Audible output | ✅ Esa confirmed "weird warbly noise" from TestTone/Noise |

**Two real defects surfaced** (documented, not fixed in the spike):

1. **Format mismatch (the "warble").** `TestToneOutputDevice::work()` writes single
   `Uint8` values into a buffer that is actually **S16 stereo** (`stream[i] = wave[...]`).
   Each 16-bit L/R sample is overwritten byte-wise, so the intended clean square wave
   aliases into warble. Any real engine must write *interleaved `Sint16` L/R frames*, not
   bytes. → handled correctly in the `SampleOutputDevice` design (§3.2).

2. **Device-index shift.** `InitializePlugins()` inserts the audio devices at indices
   **0 and 1 before** the MIDI devices. Turning audio on shifts every MIDI device index
   by +2, which would **break saved device selections** (instruments store `midi_device`
   as an integer index). → must register audio devices **after** MIDI, or address devices
   by a stable id, before this ships to anyone with saved songs (§5.1).

---

## 3. Proposed architecture

### 3.1 Storage: song-level sample pool + per-instrument link

Add a sample pool owned by `module` (the song), and a link field on `instrument`.

```cpp
// new: src/sample.h
struct zt_sample {
    char     name[32];
    Sint16  *pcm;          // interleaved if stereo; mono is the common case
    Uint32   frames;       // frame count (not bytes)
    Uint8    channels;     // 1 or 2
    Uint32   rate;         // source sample rate (Hz) — e.g. IT C5speed
    Uint32   loop_start, loop_end;   // in frames; loop_end==0 => no loop
    Uint8    root_note;    // MIDI note at which pcm plays at `rate` (default 60 = C5)
    Uint8    flags;        // loop on/off, ping-pong, 16-vs-8-bit-source, etc.
    Sint8    finetune;     // cents/16ths
    Uint8    default_vol;  // 0..127
};
```

- The pool lives in `module` as a small fixed array or `std::vector<zt_sample>` (decision
  in PR-1; fixed array keeps `.zt` simple and matches the codebase's existing style).
- `instrument` gains **one** field: `signed short int sample_index;` (`-1` = none).
  Instruments stay primarily MIDI; a sample link is additive and optional.

**Why a pool + index, not inline-per-instrument PCM:** multiple instruments can share a
sample at different root notes / loop settings without duplicating multi-MB PCM, and it
mirrors the IT mental model (samples are a numbered bank).

### 3.2 The engine: `SampleOutputDevice : AudioOutputDevice`

A new device in `OutputDevices.{h,cpp}`:

```cpp
class SampleOutputDevice : public AudioOutputDevice {
    struct voice {
        const zt_sample *smp;
        double  pos;        // fractional read position in frames
        double  step;       // pos increment per output frame = (smp->rate/44100) * pitch
        Uint8   note, chan, vel;
        bool    active, released;
    };
    voice    voices[MAX_VOICES];     // e.g. 32
    int      sample_for_chan[16];    // channel -> pool index, set via progChange (§3.3)
    // ...
    void noteOn (unsigned char note, unsigned char chan, unsigned char vol) override;
    void noteOff(unsigned char note, unsigned char chan, unsigned char vol) override;
    void progChange(int program, int bank, unsigned char chan) override; // sets sample_for_chan
    void work(void *udata, Uint8 *stream, int len) override;             // the mixer
};
```

`work()` — the heart — for each output frame:
1. zero an `Sint32` accumulator (L, R),
2. for each active voice: linear-interpolate `smp->pcm` at `voice.pos`, scale by velocity,
   add to accumulator, advance `pos += step`, handle loop wrap / end-of-sample,
3. clamp accumulator to `Sint16` and write **interleaved L/R** into `stream`.

`step` derives pitch from the note: `step = (smp->rate / 44100.0) * pow(2, (note -
smp->root_note + finetune/...) / 12.0)`. Pitch-bend (`pitchWheel`) scales `step` live.

This is a standard mono/stereo software sampler voice. Linear interpolation is enough for
v1; cubic/sinc is a later polish.

### 3.3 Routing: how a note picks a sample (no API change needed)

The device's `noteOn(note, chan, vol)` doesn't get the instrument or sample. We resolve it
through the **already-present `progChange` path**:

- An instrument routed to the sample engine has `midi_device = <sample device index>`.
- When that instrument becomes active, playback sends `progChange(program=sample_index,
  bank=0, chan=inst->channel)` — exactly as it would select a MIDI program. The engine
  stores `sample_for_chan[chan] = sample_index`.
- On `noteOn(note, chan, vol)`, the engine allocates a voice for
  `pool[sample_for_chan[chan]]` at the note's pitch.

**This keeps the MIDI-shaped `OutputDevice` interface intact** — no signature changes
rippling through every device. The one playback change is ensuring a `progChange` is
emitted for sample-routed instruments (it largely already happens for bank/program).

*Alternative considered:* extend `noteOn` to carry the instrument index. Rejected for v1 —
it touches every `OutputDevice` subclass and the `midiOut` wrappers, for no gain over the
channel-map approach.

*Constraint this implies:* one active sample per channel at a time (16 channels). That is
the natural tracker model and is fine for v1. Per-note sample selection (drum kits) is a
later extension via a key→sample map on the engine.

### 3.4 Loading

- **WAV:** SDL3 ships `SDL_LoadWAV` (no SDL_mixer dependency — the old commented
  `Mix_LoadWAV` lines in `OutputDevices.cpp` predate SDL3 and should be deleted). Convert
  to S16 via `SDL_ConvertAudioSamples` if needed, store in the pool.
- **IT:** revive `ITSampleHeader::LoadSampleData()` (un-comment the `fread`, return real
  status), and on `.it` import copy header + PCM into the pool. IT 8-bit/16-bit and
  compressed-sample handling is the fiddly part; v1 can support uncompressed PCM and
  defer IT215 compression.

### 3.5 Persistence (`.zt`)

Follow the **`CCBN`-chunk precedent** (PR #84): add a new optional chunk, e.g. `SMPL`,
holding the pool. Old zTracker skips unknown chunks via `readblock`'s advance-past
behavior, so the format stays backward+forward compatible. The per-instrument
`sample_index` rides in the existing instrument chunk as one new field (or a small
parallel chunk if touching the instrument record is undesirable).

PCM is large; the chunk should store length-prefixed raw S16 (optionally deflate via the
zlib already linked). A song with no samples writes no `SMPL` chunk → zero overhead and
byte-identical saves for existing MIDI-only songs.

### 3.6 The sample editor page

A new `CUI_SampleEditor.cpp` page (proposed key: **Shift+F7**, or fold into the Instrument
Editor). Must obey the skill's page invariants:

- A real `ListBox` subclass for the sample list (never hand-drawn) — see
  `references/list-pane-idiom.md`.
- Real `ValueSlider`/`TextInput` widgets for loop start/end, root note, volume, finetune —
  never `printBG` ASCII art (INVARIANT #2).
- `need_refresh++` on every state-mutating key (INVARIANT #1); per-frame `need_redraw=1`
  on visible widgets (INVARIANT #4).
- Load button → file picker → `SDL_LoadWAV` → pool.
- A waveform display (read-only) is a nice-to-have, not v1-blocking.

---

## 4. Open product questions for Manuel

1. **Does this break the "MIDI-only" identity on purpose?** Sample playback is a genuine
   change of scope. Is it opt-in (off by default, no UI unless enabled) or a headline
   feature?
2. **Scope of v1:** mono WAV one-shot + loop is the minimal useful slice. Is IT-sample
   import in-scope for v1 or a follow-up?
3. **Default device registration:** are we comfortable shipping with `_ENABLE_AUDIO` on by
   default (every user gets an audio device), or behind a config key / build option so
   pure-MIDI users are unaffected?
4. **Polyphony / CPU budget:** 32 voices of linear-interp mixing is trivial CPU, but the
   audio callback runs on the CoreAudio/WinMM/ALSA thread — voice state shared with the UI
   thread needs the same care the `midiInQueue` mutex work (PR #116) established.

---

## 5. Risks & gotchas

### 5.1 Device-index ordering (must fix before any user sees it)
Registering audio devices before MIDI shifts saved `midi_device` indices. **Fix:** append
audio devices after MIDI in `InitializePlugins`, or migrate to stable string-id device
addressing. Non-negotiable before shipping to anyone with saved songs.

### 5.2 Audio-thread / UI-thread sharing
Voice array, sample pool lifetime, and "note on/off" hand-off cross the audio callback
thread and the UI thread. Reuse the lesson from `midiInQueue` (PR #116): a mutex or a
lock-free SPSC command queue feeding the audio thread. Never free a `zt_sample` the audio
thread might still be reading.

### 5.3 S16 interleaved frames, not bytes
The spike's warble is the cautionary tale. The engine writes `Sint16` L/R pairs; treat
`len` as bytes (`frames = len / 4` for stereo S16).

### 5.4 Sample memory in `.zt`
Multi-MB PCM in song files. Length-prefix, consider deflate, and keep the chunk optional
so MIDI-only songs are unaffected.

### 5.5 Cross-platform audio output
Spike proved CoreAudio. WinMM/WASAPI and ALSA paths go through the same SDL3 abstraction
so they *should* just work, but each needs a real-hardware smoke test (same discipline as
the SysEx cross-platform matrix).

---

## 6. Why this is tractable

The genuinely hard, hard-to-discover part — getting a stable PCM stream out of the OS and
a polymorphic mixing entry point wired into the song's output path — **is already built
and now proven to run.** What remains is well-understood sampler work (storage, a resampling
voice mixer, a loader, a UI) layered onto a working foundation, landable in small focused
PRs. See the [PR plan](sample-playback-pr-plan.md).
