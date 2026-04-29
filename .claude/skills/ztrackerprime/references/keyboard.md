# Keyboard conventions

## Modifier state

`keybuffer.h` tracks four modifier bits in `kstate`:

| Bit | Modifier |
|---|---|
| `KS_SHIFT` | Shift |
| `KS_CTRL` | Control |
| `KS_ALT` | Alt / Option |
| `KS_META` | macOS Cmd / Windows Super |

## macOS Cmd is a compound state

`SDL_KMOD_GUI` on Apple maps to `KS_META | KS_ALT`. So every Alt+ shortcut also fires under Cmd+. **Don't write `kstate == KS_ALT`** — it fails on macOS. Use the macro:

```c
#define KS_HAS_ALT(s) (((s) & KS_ALT) && !((s) & (KS_CTRL | KS_SHIFT)))
```

`KS_HAS_ALT(s)` is true for plain Alt and for Cmd, false when Ctrl or Shift is also held.

## § (section sign) on Finnish/EU ISO keyboards

The § key sends `SDL_SCANCODE_NONUSBACKSLASH` (sometimes `INTERNATIONAL1`/`2`/`3`). In `keyhandler()` (`src/main.cpp`), those scancodes are remapped to `SDLK_GRAVE` so existing bindings work on non-US layouts:

- Plain § → Note Off.
- Shift+§ → drawmode toggle.

If you bind a new key, **also test with Finnish layout** — § / `<` / `>` / `[` / `]` are positioned differently from US layouts.

## macOS NSApp menu strips

`Cmd-Q` is stripped from the auto-created NSApp menu so the Pattern Editor can use it (Transpose-Up). `Cmd-W` is stripped similarly so it doesn't dismiss the SDL window.

## Updating help.txt

When adding a shortcut, update `doc/help.txt` in the same PR. The in-app F1 help reads from this file; out-of-date help is an outright bug.
