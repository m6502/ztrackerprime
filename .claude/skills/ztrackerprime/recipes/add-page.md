# Recipe: add a new page

A "page" is a top-level screen reachable via an F-key (or via the ESC main menu). Examples: F2 Pattern Editor, F3 Instrument Editor, F12 Sysconfig, Shift+F4 Arpeggio Editor.

## What to wire up

1. **Add a `STATE_<NAME>` to the enum in `src/zt.h`** (around line 328). Order matters only insofar as `DEFAULT_STATE` and any code switching on the enum keep working — append to the end if unsure.

2. **Declare the page class in `src/CUI_Page.h`** as a subclass of `CUI_Page`. Implement the four pure-virtuals:
   - `enter()` — called when the page becomes active. Allocate widgets, set initial state.
   - `leave()` — called when leaving. Free widgets if `enter()` allocated them.
   - `update()` — per-frame logic.
   - `draw(Drawable *S)` — per-frame rendering.

3. **Create `src/CUI_<Pagename>.cpp`** alongside the existing `CUI_*.cpp` files. Implement the four methods. The page typically owns a `UserInterface *UI` (inherited) and adds widgets to it during `enter()`.

4. **Add a global page instance** somewhere main.cpp can see (typically `extern CUI_<Pagename> *UIP_<Pagename>;` in a header, definition where the page list is constructed).

5. **Wire the F-key in `keyhandler()` in `src/main.cpp`** — find the existing block of `case SDLK_F<n>:` handlers (around line 1670+) and add a `switch_page(UIP_<Pagename>);` call. Set the matching `STATE_<NAME>` if the page tracks it.

6. **Add the source file to `CMakeLists.txt`.** Existing `CUI_*.cpp` files are listed near the build target's source list; insert there.

7. **Add a help section to `doc/help.txt`.** Match the existing `|L|%==|H|<Title>|L|==%|U|` format. Document the F-key and any page-specific keybinds.

8. **Add a CHANGELOG entry** to `doc/CHANGELOG.txt`.

## Optional but recommended

- **ESC main menu entry** in `src/CUI_MainMenu.cpp` — users who don't memorise F-keys reach the page through here. Add an entry of the form `{MM_FUNC, "Page Name", NULL, 0, mm_<page>_handler}` (or `MM_PAGE` if jumping directly).
- **Unit tests** if the page has non-trivial decision logic. Extract pure functions into a header (see `references/test-harness.md`) and add a CTest suite.

## Pitfalls

- **Forgetting `leave()` cleanup** — widgets allocated in `enter()` and not freed leak across page switches.
- **Skipping help.txt** — the in-app F1 help is the user's primary discovery path; out-of-date help is a real bug.
- **Forgetting CMakeLists.txt** — the file builds locally only if it's in the source list; missing entries cause CI failures across the 5-platform matrix.
- **Hard-coded layout coordinates** — use `INITIAL_ROW`, `PAGE_TITLE_ROW_Y`, `TRACKS_ROW_Y`, `SPACE_AT_BOTTOM` from `zt.h` instead of literals so the page survives resolution changes.
