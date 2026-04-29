# Recipe: add a new CTest suite

The harness is for pure-decision regression tests. If you've extracted a class of bug into a pure-function header (`*_decision.h`, `*_apply.h`), add a suite.

## What to write

1. **Pure header in `src/`** — no SDL, no globals, no I/O. A struct of inputs, a struct of outputs, one pure function. See `src/preset_selector.h`, `src/save_key_dispatch.h`, `src/page_sync.h` for examples.

2. **Test file in `tests/test_<name>.cpp`** — `#include` the header, write a `main()` that runs assertion-style checks and prints a one-line summary. Existing suites use simple `if (!cond) { fprintf(stderr, "FAIL: ..."); return 1; }` patterns — no test framework.

3. **CMakeLists registration** in `tests/CMakeLists.txt`:
   - Add `add_executable(test_<name> test_<name>.cpp ...)` — list any extra source files (e.g., `module_stub.cpp`) the test needs.
   - Add `add_test(NAME test_<name> COMMAND test_<name>)`.

## Staying SDL-free

If your code under test pulls in SDL transitively, two escape hatches exist:

- **`tests/sdl_stub.h`** — defines the minimal `SDL_KMOD_*` and `SDLK_*` symbols used by `keybuffer.h`. Activate by defining `ZT_TEST_NO_SDL` before including the header. Used by `test_keybuffer` to pull in production `src/keybuffer.cpp` directly.

- **`tests/module_stub.cpp`** — minimal `arpeggio` / `midimacro` constructors. Link this instead of `src/module.cpp` (which `#include`s `zt.h` and pulls the world).

If your header needs more SDL surface than `sdl_stub.h` exposes, prefer extending the stub over linking SDL into the test binary — keep tests fast and CI-portable.

## Run

```sh
cd build && ctest --output-on-failure
```

Or run a single suite directly: `./build/tests/test_<name>`.

## When to extract

Don't pre-emptively extract every helper into a tested header. The trigger is: **a class of bug has bitten more than once** (preset apply revert, save-key dispatch confusion, button-up event consumption, ring-buffer drain). At that point the cost of writing a test is amortised across the bugs it'll catch.
