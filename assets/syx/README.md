# SysEx Librarian files for zTracker Prime

Drop `.syx` files (raw MIDI System Exclusive byte dumps) here.

The **SysEx Librarian** (Shift+F5) lists the files in this folder and lets
you send any of them out the current MIDI Out device with Enter / `S`.
Synth responses are auto-captured and saved as `recv_<timestamp>_<seq>.syx`
in the same folder, so you can re-send them later to dump the patch back
into the synth.

## Storage

By default this folder lives at `<ccizer_folder>/syx`. Override with
`syx_folder=` in `zt.conf` to point anywhere.

## Format

Standard `.syx`: raw bytes, no header, no metadata. Must start with `F0`
and end with `F7`. zTracker refuses to send malformed files rather than
spit out a partial byte stream.

## Bundled examples

| File | Purpose |
|---|---|
| `request_universal_inquiry.syx` | `F0 7E 7F 06 01 F7` — the MIDI standard "Identity Request". Almost every modern synth replies with its manufacturer/family/version. Good first test for "is my SysEx wiring working?" |

## Workflow

1. **Get a request file.** Either bundled (above), exported from a
   librarian like SysEx Librarian.app, or hand-written.
2. **Open a MIDI Out device** in F12 System Configuration.
3. **Shift+F5**, pick the request file, press Enter.
4. **Watch the right pane** — the synth's response is captured and the
   file list refreshes with the new `recv_*.syx`.
5. **Re-send any captured `.syx`** later to load the patch back into the
   synth.

## Cross-platform note

Receive parsing is implemented on macOS (CoreMIDI). On Linux/Windows the
send path works but receive is silent — those land in a follow-up.
