# Pattern effect reference

Effects are letters in the effect column of a pattern row. Each takes up to 4 hex digits of parameter. Dispatched in `src/playback.cpp`.

| Letter | Mnemonic | Effect |
|---|---|---|
| `A..xx` | TPB | Set Ticks Per Beat. |
| `B 00` | Marker `[` | Set "[" marker in MIDI export. |
| `B 01` | Marker `]` | Set "]" marker in MIDI export. |
| `Cxxxx` | Pattern break | End the pattern; jump to next pattern, row 0 (`C0000`). |
| `D..xx` | Delay | Delay note/volume/cut by xxxx sub-ticks. |
| `Exxxx` | Pitch slide down | `E0000` repeats the last portamento command. |
| `Fxxxx` | Pitch slide up | `F0000` repeats the last portamento (down) command. |
| `P....` | Program change | Send program change for the current instrument. |
| `Q..xx` | Retrigger | Retrig note every xx subticks. |
| `R..xx` | Arpeggio | Start arpeggio xx (from the song's ARPG chunk). `R0000` continues the last. |
| `Sxxyy` | CC | Send Continuous Controller xx with value yy (yy < 0x80). yy ≥ 0x80 sends last value. Example: `S0142` sets CC#01 (Mod Wheel) to 0x42. |
| `T..xx` | Tempo | Set tempo to xx BPM. |
| `Wxxxx` | Pitch bend (absolute) | 0..0x3FFF; 0x2000 = no bend, 0 = max down, 0x3FFF = max up. |
| `X..xx` | Pan | 00 = left, 40 = center, 7F = right. |
| `Zxxyy` | MIDI Macro | Send macro xx (from the song's MMAC chunk) with parameter yy. `00` repeats the last value. |

## Where each is implemented

Most effects are dispatched from a switch in `playback.cpp` keyed on the effect letter. R (arpeggio) and Z (MIDI macro) read from the song's ARPG / MMAC chunks; their definitions come from the F4 and Shift+F4 editors.

When adding a new effect:
1. Pick an unused letter.
2. Add the dispatch case in `playback.cpp`.
3. Document in `doc/help.txt` (Effects section).
4. If the effect references a song-level data structure (like Z → MMAC), wire the editor to populate it.

## Effects that "remember"

Several effects use `0000` as "repeat last":
- `E0000` — repeat last portamento.
- `F0000` — repeat last portamento (down).
- `R0000` — continue last arpeggio.
- `Z..00` — repeat last MIDI macro parameter.
- `Sxx80` and above — send CC's last value rather than a new one.

If you add an effect with state, follow the pattern (zero-param or high-bit means reuse).
