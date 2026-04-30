# Headless / scripted operation

`zt --headless --script <file>` runs zTracker without opening a window, driving the running app from a script and dumping PNG screenshots on demand. Designed for CI smoke tests, headless layout regression, and AI-driven page verification — anywhere "spin up a real GUI session, press keys, take a screenshot" is too heavy.

## Quick start

```sh
mkdir -p /tmp/zt-headless-shots
./build/Program/zt --headless --script tests/scripts/smoke.txt
ls /tmp/zt-headless-shots/
```

The bundled `tests/scripts/smoke.txt` walks F1 / F2 / F4 / Shift+F2 / Shift+F3 / Shift+F4 / Shift+F5 / F11 / F12 and dumps one PNG per page. About 2 seconds end-to-end on a modern machine.

## How it works

`--headless` sets `SDL_HINT_VIDEO_DRIVER=dummy` (and the audio equivalent) before `SDL_Init`, so SDL never opens a window. The renderer + texture pipeline still runs against an in-memory `SDL_Surface` with format `ARGB8888` — exactly the surface the pages already paint into. The script driver dumps that surface to PNG via libpng's simplified API (the surface's BGRA byte order matches `PNG_FORMAT_BGRA`, so no per-pixel byte-swap is needed).

`--script <file>` makes the main loop call `zt_headless_pump()` once per frame *before* `SDL_PollEvent`. The pump reads commands from the script file and either injects them into the global `Keys` ring, defers them via `SDL_GetTicks`, dumps a PNG, or sets `do_exit`. From the page's perspective the synthetic keys are indistinguishable from real ones — `Keys.insert(SDLK_F2, SDL_KMOD_NONE)` is exactly what the real `keyhandler()` would have done after `SDL_EVENT_KEY_DOWN`.

`--headless` and `--script` are independent. `--headless` alone still requires some external way to feed input (typically not useful, but lets you check that the binary boots clean under the dummy driver). `--script` without `--headless` is also valid — it drives a normal windowed session, useful when you want to *see* the script run.

## Script syntax

Each line is one command. Blank lines and lines starting with `#` are ignored.

| Command | Effect |
|---|---|
| `key <chord>` | Inject a synthetic keystroke. Chord syntax: zero or more `shift+ / ctrl+ / alt+ / meta+ / cmd+` modifier prefixes followed by the key name. |
| `wait <ms>` | Defer the next command for `<ms>` milliseconds (capped at 60000). The main loop keeps running, so the page renders and processes the last batch of keys before the next fires. |
| `shot <path>` | Dump the current screen surface to a PNG at `<path>`. Parent directory must already exist. |
| `quit` (or `exit`) | Set `do_exit`. The main loop tears down on its next pass. CI scripts should always end with this. |

### Key names

Lowercase, case-insensitive in practice:

- Function keys: `f1` … `f12`
- Navigation: `up`, `down`, `left`, `right`, `home`, `end`, `pageup`, `pagedown`
- Editing: `tab`, `return` (alias `enter`), `space`, `esc` (alias `escape`), `backspace`, `delete`
- Punctuation: `grave` (the `§` key on Finnish ISO), `leftbracket`, `rightbracket`
- Letters: `a` … `z` (single character)
- Digits: `0` … `9` (single character)

### Modifier prefixes

`shift+`, `ctrl+`, `alt+`, `meta+` (alias `cmd+`). Combine with `+`:

```
key shift+f3            # CC Console
key ctrl+s              # save
key ctrl+shift+r        # reset all keyboard bindings to defaults
key meta+q              # macOS Cmd+Q (Pattern Editor: Transpose-Up)
```

## Worked example

A self-contained smoke test that verifies the CC Console renders, accepts arrow Down, loads a file with Space, and exits clean:

```
# Boot.
wait 200

# Open CC Console.
key shift+f3
wait 150
shot /tmp/cc-1-empty.png

# Move down two files, load with Space.
key down
wait 50
key down
wait 50
key space
wait 200
shot /tmp/cc-2-loaded.png

# Tab to the slot grid, arrow-Down twice, capture.
key tab
key down
key down
wait 100
shot /tmp/cc-3-grid-down.png

quit
```

`zt --headless --script that.txt` produces three PNGs and exits with status 0.

## Limitations

- **No mouse input.** The script driver currently injects keystrokes only. Click / drag would require synthesising `SDL_EVENT_MOUSE_*` events into the page's `mouse*handler` paths; possible but not yet wired. The major pages have full keyboard coverage.
- **MIDI subsystem still initialises.** On macOS, `--headless` opens CoreMIDI as usual; you'll see whatever virtual ports the OS exposes. To suppress: drop the `MidiOut = new midiOut` call behind `if (!cli_args.headless)` in main. Not yet done because some scripts may want to drive `--midi-in <port>` headlessly.
- **macOS dock icon.** The dummy driver still registers the process with the OS in a way that may briefly show in `Activity Monitor`. No window opens, no Dock icon, no menu bar. The process exits the moment the script's `quit` fires.

## Future work

- **Golden-image diff suite.** A new `tests/test_pages.cpp` could ship per-platform golden PNGs, run the smoke script, and pixel-diff. Linux CI would catch "I broke the CC Console layout" before review. Pixel tolerances per platform need empirical calibration first — that's a follow-up PR.
- **Mouse synthesis.** `mouse_move <x> <y>`, `mouse_click <x> <y>` for drag-test coverage of widgets with click-only behaviour (slider drags, listbox click-to-focus).
- **Status-bar capture.** A `status` command that dumps the current `status_line` text to stdout, so tests can assert "after Ctrl+S, status reads 'Saved …'" without pixel-diffing.
