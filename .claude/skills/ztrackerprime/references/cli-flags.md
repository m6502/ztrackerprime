# CLI flags + Copy Relaunch Command

zt accepts named CLI flags parsed by `zt_parse_cli` in `main.cpp`.

## Flags

| Flag | Effect |
|---|---|
| `-h`, `--help` | Print usage + exit. Works without SDL/MIDI/display. |
| `--list-midi-in` | List MIDI input ports + exit. |
| `--midi-in <name\|index>` | Open the named port at startup. Repeatable. |
| `--midi-clock <name\|index>` | Open + enable `midi_in_sync` + `midi_in_sync_chase_tempo`. |

First positional arg = .zt song to load.

## Parser conventions

- **Order-independent** — flags can appear before or after the song path.
- **Two value forms** — `--flag value` and `--flag=value` both work.
- **Dash-tolerant** — `-flag` and `--flag` both parse. (Bare words still need at least one `-`; `zt help` does *not* match `--help`.)
- **Repeatable `--midi-in`** — internal cap is `midi_in_ports[16]`.

## Helpers

- `zt_print_cli_help` — prints usage, called for `-h`/`--help` and on parse error.
- `zt_resolve_midi_in_port` — name-or-index lookup against MIDI in port table.
- `zt_cli_match_flag` / `zt_cli_match_flag_with_value` — flag matchers tolerating `=` and dash count.
- `zt_cli_strip_leading_dashes` — requires ≥1 dash, so bare positional args don't accidentally match flags.

## Copy Relaunch Command (ESC menu)

`mm_copy_relaunch_cmd()` in `src/CUI_MainMenu.cpp` reads current session state — open MIDI in ports, MIDI clock chase target, loaded song path — and emits a one-line `zt --midi-clock "X" --midi-in "Y" song.zt` invocation to the system clipboard via `SDL_SetClipboardText`.

Pairs with the CLI flags for stage / live / chat-handoff workflows: copy the relaunch line on one machine, paste on another to bring up the same MIDI routing + song.

The menu entry lives under File: `{MM_FUNC, "Copy Relaunch Command", NULL, 0, mm_copy_relaunch_cmd}`.
