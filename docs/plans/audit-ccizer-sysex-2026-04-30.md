# Brittleness audit — CCizer + SysEx work (PRs #78–#84)

Date: 2026-04-30
Scope: everything merged to `master` from PRs #78 through #85.

This is a fresh-eyes review against the merged code. Items below are concrete:
each one names the file/function, what's wrong, what fails in practice, and a
specific fix. Severity = how likely a real user hits it.

## Severity HIGH — real bugs

### H1. Windows SysEx send leaks the `MIDIHDR` and starts failing after the first call
**Where:** `src/winmm_compat.h::zt_midi_out_sysex` (Windows branch, lines 19–32).

**Bug:** Calls `midiOutUnprepareHeader` immediately after `midiOutLongMsg`. Per
MS docs, `midiOutUnprepareHeader` fails with `MIDIERR_STILLPLAYING` until
`MHDR_DONE` is set in `dwFlags`. The driver still owns the buffer; we tear
down the `MIDIHDR` while it's queued. Subsequent calls to `midiOutLongMsg`
on the same handle may return `MIDIERR_STILLPLAYING` because the previous
buffer is still "in flight" from the driver's POV.

**Symptom:** First SysEx might land. The second and onward likely fail
silently. Worse on synths that take 100+ ms to consume long dumps.

**Fix:** Poll `(hdr.dwFlags & MHDR_DONE) == 0` with a short sleep loop
before unpreparing, OR keep the `MIDIHDR` (heap-allocated) on a per-handle
"in-flight" list and unprepare lazily on the next send / handle close. The
poll-loop is the smaller change:

```c
while ((hdr.dwFlags & MHDR_DONE) == 0) Sleep(1);
midiOutUnprepareHeader(h, &hdr, sizeof(hdr));
```

Caveat: blocks the calling thread for the duration of the dump (typical
2-byte/ms = ~1 ms per 2 bytes). For SysEx librarian use that's acceptable;
for playback-thread Z-effect dispatch it's blocking time the player can't
afford.

### H2. ESC menu's "Toggle CC Drawmode" leaves undo-session marker stale
**Where:** `src/CUI_Patterneditor.cpp` (line 22, file-static
`g_cc_draw_session_snapped`) and `src/CUI_MainMenu.cpp::mm_toggle_cc_drawmode`.

**Bug:** `g_cc_draw_session_snapped` is a `static int` inside
`CUI_Patterneditor.cpp`, invisible from the menu. The Ctrl+Shift+§ keypath
in the Pattern Editor resets it, but `mm_toggle_cc_drawmode` only flips
`g_cc_drawmode` and never touches the session marker.

**Symptom:** Toggle drawmode OFF via the keypath (session marker reset to 0).
Toggle ON via the ESC menu (session marker still 0 — but only by coincidence,
since it was zeroed on the previous toggle). Toggle OFF via menu (still 0).
Toggle ON via menu (still 0). Tweak knob — `UNDO_SAVE` fires (correct, since
marker was 0). Tweak knob 100 more times — no further snapshots. Then toggle
OFF/ON via menu without leaving Pattern Editor — marker is still 1 from the
first session, so the SECOND knob run has no undo snapshot.

**Fix:** Lift `g_cc_draw_session_snapped` to `extern int` in `zt.h`, define
in `main.cpp` next to `g_cc_drawmode`, and reset it from
`mm_toggle_cc_drawmode` too. Or wrap both toggles in a shared inline helper
`zt_toggle_cc_drawmode()` defined in one place.

### H3. File I/O in the playback thread (Z-effect SysEx dispatch)
**Where:** `src/playback.cpp::case 'Z'` (lines 1589–1599) calls into
`zt_sysex_macro_resolve_path` + `zt_sysex_macro_read_file` which do `stat`,
`fopen`, `fread`, `malloc` inline.

**Bug:** The Z-effect handler runs in the playback thread (`counter_thread` /
`player_callback`). A cold disk read on the song's first SysEx-Z effect
blocks that thread for whatever the disk takes. macOS spotlight reads or
spinning-disk Linux setups can stall hundreds of milliseconds.

**Symptom:** First playback of a pattern with a SysEx-Z effect drops MIDI
ticks / late-arrival of subsequent notes. Subsequent plays are fine
(filesystem cache).

**Fix:** Resolve and read all `*.syx`-named macros at song-load time
(`zt_module::load`) into an in-memory cache keyed by macro slot. Z-effect
dispatch then just looks up the bytes, no I/O. Re-read on song save or
on conf-change of `syx_folder`.

Smaller alternative: read the file lazily at first use but cache for
the rest of the playback session (per-macro `unsigned char *cached_bytes`
+ `int cached_len` on the `midimacro` struct).

### H4. CC Console `learning` flag persists across page-leave
**Where:** `src/CUI_CcConsole.cpp::enter` (179–186), `::leave` (188–189).

**Bug:** `learning` is reset on ESC, on captured-MIDI-event, and on `L` press.
Never reset on `enter()` or `leave()`. If the user presses `L`, then opens
the ESC menu and switches to another page (without pressing ESC inside the
console), `learning = 1` is preserved. Re-entering Shift+F3 silently leaves
learn mode on; the next incoming CC binds to `slot_cur` with no warning.

**Symptom:** Quietly mis-binds knobs in the rare case where the user changes
pages mid-Learn.

**Fix:** Add `learning = 0;` to `enter()` (and ideally `leave()` too).

## Severity MEDIUM — correctness on edge cases

### M5. zt.conf paths are read from playback thread without synchronization
**Where:** `src/CUI_Sysconfig.cpp:461` binds the F12 `TextInput` directly at
`zt_config_globals.ccizer_folder`'s buffer; `src/sysex_macro.cpp::zt_sysex_macro_resolve_path`
reads the same buffer from the playback thread.

**Bug:** Per-keystroke writes from F12 race with reads in the playback
thread. C strings without locking is data-race-undefined-behavior even if
the buffer is character-by-character non-tearing on x86.

**Realistic impact:** Almost zero — users don't tweak F12 paths while
playback is running. But it's a flagged data race that a sanitizer build
would catch.

**Fix:** Take a snapshot of the path on `zt_module::load` / `enter()` of
the relevant page, or guard reads with a tiny `std::shared_mutex`.

### M6. macOS SysEx send uses a 17 KB stack buffer
**Where:** `src/winmm_compat.h::zt_midi_out_sysex` (macOS branch, line 587).

**Bug:** `Byte buf[MAX_SYSEX + 1024]` = 17 KB on the stack. Default macOS
thread stack is 512 KB so today this is fine. But if anyone later spawns a
worker thread with a smaller stack and calls `sendSysEx` from it, this
overflows.

**Fix:** Heap-allocate the packet buffer. One-line change.

### M7. Receive auto-save can collide on filenames after restart
**Where:** `src/CUI_SysExLibrarian.cpp::drain_recv` — uses `recv_seq` (member,
reset to 0 in ctor).

**Bug:** `recv_seq` doesn't survive process restart. After restart, a new
capture arriving in the same second as a pre-existing `recv_<TS>_000.syx`
clobbers the file.

**Realistic impact:** Very low — requires restart + same-second capture +
matching wall clock seconds. But silent data loss when it happens.

**Fix:** On `enter()`, scan the folder for the highest existing
`recv_*_NNN` and seed `recv_seq` past it. Or include subsecond precision
in the timestamp.

### M8. CC Console doesn't auto-create the default folder
**Where:** `src/CUI_CcConsole.cpp::resolve_folder` (lines 137–155).

**Bug:** SysEx Librarian uses `make_dir_recursive` to create its folder.
CC Console does not. If `ccizer_folder` zt.conf key is unset AND
`./ccizer` doesn't exist (e.g. user moved the binary out of the build
dir), `num_files = 0` with no clear error.

**Fix:** Add `make_dir_recursive(folder)` call after resolve, mirroring
the librarian. (Or: extract a shared helper.)

### M9. `localtime()` is not thread-safe
**Where:** `src/CUI_SysExLibrarian.cpp::drain_recv`.

**Bug:** `localtime` returns a pointer to a static `struct tm`. Called only
from the UI thread today, so no actual race. But standard portability
gotcha.

**Fix:** `localtime_r(&now, &tmbuf)` (POSIX) /
`localtime_s(&tmbuf, &now)` (MSVC). Tiny.

### M10. SysEx file size cap is 64 KB at playback dispatch
**Where:** `src/playback.cpp:1594` calls `zt_sysex_macro_read_file(..., 65536)`.

**Bug:** Some real-world dumps exceed 64 KB. Roland Integra-7, Korg Kronos,
modular synth global state. Files larger than 64 KB silently fail to
dispatch.

**Fix:** Bump to 1 MB or remove the cap and rely on file-system size only.
The `MAX_SYSEX = 16384` cap inside the macOS sender is a separate, lower
cap — also worth raising.

## Severity LOW — brittle patterns / future-proofing

### L11. `zt_sysex_inq` lock-protected memcpy of 8 KB on every push
Mutex contention between CoreMIDI callback thread and UI thread on every
SysEx received. Real but not measurable for typical patch-dump rates.
Lock-free SPSC ring would fix; almost certainly premature.

### L12. CCBN chunk loader silently truncates on length mismatch
`src/ztfile-io.cpp` CCBN load tolerates corrupt `length` (CDataBuf
`getch` returns 0 past end), but doesn't warn the user. A corrupted song
file results in instruments with bogus bank names.

**Fix:** Track bytes-remaining and log a warning if length > remaining.

### L13. F12 has no `syx_folder` TextInput
Only `ccizer_folder` is exposed in F12. User edits `zt.conf` directly to
move the SysEx folder. Inconsistent UX.

**Fix:** Mirror the `ccizer_folder` TextInput pattern for `syx_folder`.

### L14. F4 SysEx hint doesn't verify the file exists
`zt_sysex_macro_is_file(name)` checks suffix only. F4 shows
`(SysEx file mode: data grid is ignored)` even if the file is missing.
Z-effect playback drops silently in that case.

**Fix:** Resolve + `stat` at hint time; if missing, show
`(SysEx file: NOT FOUND)` in red so the user knows the dispatch will
no-op.

### L15. `recv_*.syx` files accumulate forever
No cap, no rotation. A busy session capturing every patch leaves
hundreds of files in the folder, slowing the file-list scan.

**Fix:** Optional `syx_recv_max_files` zt.conf key with LRU cleanup.
Or just document it and trust the user to clean up.

### L16. `g_loaded` (CC Console) and `g_ccizer_current` (process-wide)
**Where:** `src/CUI_CcConsole.cpp` file-static + `src/ccizer.cpp` file-static.

**Bug:** `zt_ccizer_set_current_file(&g_loaded)` exposes the address of a
file-static. If `delete UIP_CcConsole` ever runs while another thread is
reading the pointer, you get UAF. Currently the destructor is at app
shutdown only, but it's a fragile setup.

**Fix:** Either move ownership of the loaded file into a heap allocation
that the CC Console manages, or document the lifetime invariant clearly
in `ccizer.h`.

## Process / quality observations

### P17. Visual verification is still missing
Across PRs #78–#85 I never got an end-to-end screenshot. The AppleScript
window-detection issue I gave up on at session start. Build green +
tests green ≠ "looks right." Manuel may find layout bugs at review time.

**Fix:** Solve the SDL-window-detection issue properly. Two options:
(1) capture by CGWindow ID via `CGWindowListCopyWindowInfo`, or
(2) make the screenshot script detect the window by `pid` instead of
process name, since the SDL window may not have an AppKit-visible
NSWindow under all configurations.

### P18. No integration test for the CCBN chunk
`test_ccizer.cpp` covers parser only. There's no CTest that builds a tiny
song with a per-instrument bank, saves it, reloads it, and asserts the
bank survived. Save-format changes deserve roundtrip tests.

**Fix:** New `test_ccbn_roundtrip.cpp` that uses the existing module-stub
infrastructure to build → save → load → assert.

### P19. SysEx receive parsing is macOS-only
Documented as deferred, but worth restating: Linux ALSA-in is itself
stubbed in `winmm_compat.h`, and Windows `midiInCallback` doesn't push
SysEx into `zt_sysex_inq`. The librarian receive log will be empty on
Linux/Windows even when the synth replies.

**Fix:** Wire `MIM_LONGDATA` on Windows (`midi-io.cpp:344`-area) and add
ALSA `snd_seq_event_input` SysEx path on Linux.

## Suggested PR ordering for fixes

1. **PR-A (high-impact, small):** H2 + H4 + M9 + L13. Pure UI lifecycle
   + thread-safe time + new TextInput. ~80 LOC.
2. **PR-B (Windows correctness):** H1. Critical for Windows users.
   ~40 LOC + manual Win test.
3. **PR-C (playback robustness):** H3 (lazy cache of `*.syx` macros) +
   M10 (raise cap). ~150 LOC.
4. **PR-D (folder hygiene):** M7 + M8 + L14 + L15. Library/folder UX
   polish. ~100 LOC.
5. **PR-E (cross-platform SysEx receive):** P19 — Linux ALSA-in receive
   parse + Windows `midiInCallback` SysEx routing. ~250 LOC. Real
   testing required on each platform.
6. **PR-F (test debt):** P18 + screenshot scripts (P17). ~150 LOC.

Each PR is small enough to land in one Manuel review pass.
