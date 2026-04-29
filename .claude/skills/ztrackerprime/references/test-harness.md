# Test harness

`tests/` has CTest-driven unit-test executables. Linux CI runs them after every push.

## Run

```sh
cd build && ctest --output-on-failure
```

Each suite is a standalone executable: `build/tests/test_presets`, `test_selector`, `test_page_sync`, `test_save_key`, `test_keybuffer`.

## Suites

| Suite | What it covers |
|---|---|
| `test_presets` | Preset arrays + `*_apply_preset_to(struct*, idx)` functions in `preset_data.h` |
| `test_selector` | Listbox decision logic (click / arrow / Space / P-cycle) in `preset_selector.h` |
| `test_page_sync` | F4/Shift+F4 widget↔data sync per-frame cycle in `page_sync.h` |
| `test_save_key` | Global Ctrl-S dispatch decisions in `save_key_dispatch.h` |
| `test_keybuffer` | Real `src/keybuffer.cpp` compiled SDL-free |

## Staying SDL-free

Two stub strategies in `tests/`:

1. **`tests/module_stub.cpp`** — minimal `arpeggio` / `midimacro` constructors. Linked instead of `src/module.cpp` (which `#include`s `zt.h` and pulls in the world).

2. **`tests/sdl_stub.h`** — tiny header providing only the `SDL_KMOD_*` constants and a few `SDLK_*` codes used by keybuffer logic. Activated by defining `ZT_TEST_NO_SDL` before `#include "keybuffer.h"`. Lets the production `keybuffer.cpp` compile straight into a test binary.

## Decision-extraction pattern

When a class of UI bug has bitten more than once (preset apply, save-key dispatch, button-up consumption), don't write yet another targeted patch. Extract the decision logic into a header (`preset_selector.h`, `save_key_dispatch.h`, `page_sync.h`) — pure functions, no SDL — and add a CTest suite. Each header is one bug class with regression coverage.

The pattern: callers in `CUI_*.cpp` collect inputs (mouse coords, key state, listbox row), pass them to a pure decision function, and act on the returned action enum. The pure function is what the test exercises; the caller is mechanical glue.
