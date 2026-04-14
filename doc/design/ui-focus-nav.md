# Focus / cursor navigation on config pages

**Status:** design proposal, not yet implemented.
**Pages affected:** Sysconfig (F12), Config (Ctrl+F12), Palette Editor (Shift+Ctrl+F12).
**Symptom:** arrow-key navigation on Config "page 2" sometimes lands on nothing — no element is highlighted and further arrow presses do nothing.

## Current scaffolding

`UserInterface` keeps `cur_element` (an integer ID) pointing at the focused UI element. Navigation is driven by elements themselves:

- Tab → `UserInterface::update()` calls `advance_focus(cur_element, +1)`
- Shift+Tab → `advance_focus(cur_element, -1)`
- Up / Down → the **focused element's** `update()` decides what to do. The convention every element follows:
  - `return +1` → caller advances to next focusable element
  - `return -1` → caller retreats to previous
  - `return 0` → stay put (and/or consume the key for its own purposes)

Most elements (`Button`, `CheckBox`, `ValueSlider`) map Up→`-1`, Down→`+1`, so arrow keys feel like Tab / Shift-Tab when they're held. `TextBox` consumes Up/Down for scrolling and never returns non-zero. `TextInput` has its own in-text cursor.

`advance_focus` skips elements where `is_tab_stop() == false`. Default: every element is a tab-stop.

## Why focus goes AWOL on Config page 2

### Bug 1 — phantom zero-size ValueSlider (element #5)

`src/CUI_Config.cpp` adds a ValueSlider for `cur_edit_mode` (the View Mode setting) conditionally compiled with `#ifndef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS`:

```cpp
vs->min = VIEW_BIG;
vs->max = VIEW_BIG;
vs->xsize = 0;
vs->ysize = 0;
```

`ValueSlider::update()` early-returns `0` whenever `xsize <= 0 || ysize <= 0`:

```cpp
if (xsize <= 0 || ysize <= 0) {
    return 0;
}
```

But the slider is still registered in the element list and `is_tab_stop()` returns the default `true`. Result:

- Tab (or Down arrow from element #4) lands on #5
- #5 draws nothing visible → "nothing is highlighted"
- #5's `update()` returns 0 on every Up/Down → "can't be highlighted / can't leave"
- Only Tab/Shift-Tab (handled by the enclosing UserInterface rather than the element) can unstick it

### Bug 2 — TextBox trap (element #9 on Config, the "Current Settings" dump)

`TextBox::update()`:
- consumes Up/Down for scrolling
- returns 0 unconditionally

TextBox inherits `is_tab_stop() == true`. Tabbing onto it works; arrow-keying out does not. User perceives: "I pressed Down, now I'm scrolling a block of text I didn't want to interact with and can't get back."

### Bug 3 — no "nothing focused" recovery

If `cur_element` ever references a non-tab-stop (e.g. the two above, or a ListBox that has become empty), no code resets focus to a valid tab-stop. Tab works around this by scanning, but arrow keys do not.

### Bug 4 — `enter()` doesn't guarantee a valid starting focus

Neither `CUI_Config::enter()` nor `CUI_Sysconfig::enter()` sets `cur_element` to a known-good tab-stop. Whichever element happens to be ID 0 is where focus starts. On Sysconfig page-2 (after "Go to page 2") the first element is fine; on the return trip it can land on a non-focusable item depending on insertion order.

## Proposed fix — scoped for one small PR

### 1. Declare zero-sized ValueSliders non-focusable

`Sliders.h` / `Sliders.cpp` — override `is_tab_stop` so the phantom slider becomes invisible to `advance_focus`:

```cpp
bool ValueSlider::is_tab_stop() const override {
    return xsize > 0 && ysize > 0;
}
```

Cost: one method, three lines. Fixes Bug 1 for any page that ever shrinks a slider to zero.

### 2. Declare read-only TextBox non-focusable

`UserInterface.h` — override `is_tab_stop` on TextBox:

```cpp
bool is_tab_stop() const override { return false; }
```

Rationale: TextBox in this codebase is always a read-only text display (Help, Config's settings dump, SongMessage). Users reach it via scroll/click, not Tab. If a truly focusable scrollable text ever shows up, that's a new class (or an opt-in flag).

Fixes Bug 2.

### 3. Focus-sanitize inside `UserInterface::update()`

At the top of `UserInterface::update()`, before dispatching the key to the focused element:

```cpp
UserInterfaceElement *focused = get_element(cur_element);
if (!focused || !focused->is_tab_stop()) {
    // Element went away (freed) or stopped being focusable. Advance to a
    // real tab-stop so Up/Down don't die on a stale pointer.
    int recovered = advance_focus(cur_element, +1);
    if (recovered != cur_element) {
        cur_element = recovered;
        notify_focus(cur_element);
        full_refresh();
    }
}
```

This repairs the "focus is on a non-tab-stop element" state that bugs 1 and 2 can create as legacy data before the fix, and protects against future regressions (hidden panels, toggled visibility, etc.).

### 4. Seed focus in `enter()`

Helper on `UserInterface`:

```cpp
void UserInterface::focus_first_tab_stop() {
    for (int id = 0; id < num_elems; ++id) {
        UserInterfaceElement *e = get_element(id);
        if (e && e->is_tab_stop()) { cur_element = id; return; }
    }
}
```

Call it from `CUI_Config::enter()`, `CUI_Sysconfig::enter()`, `CUI_PaletteEditor::enter()` (as a safety net). Cost: one helper + three one-line calls.

## Deferred / out of scope for this PR

- **Geometric arrow-nav**: Up/Down currently relies on insertion order matching visual order. A clean fix would pick the next element by comparing `(x, y)` coords. Non-trivial rework; most pages are OK as long as you insert elements top-to-bottom (which they already do).
- **Page-1 / Page-2 indicator on Sysconfig ↔ Config**: a breadcrumb like `[ Page 1 | *Page 2* ]` next to the title would help users notice they changed pages. Cosmetic; separable.
- **Palette Editor focus cleanup**: the Palette Editor (Shift+Ctrl+F12) manages its own focus manually (`focus_panel`, `selected_slot`) and doesn't use UserInterface's element-based system at all. Still being iterated in PR #19; this PR won't touch it.
- **Sysconfig button/list interactions**: `MidiOutDeviceOpener`, `BankSelectCheckBox`, `SkinSelector`, `LatencyValueSlider` all have custom `update()` methods. They'd benefit from the same "visible-and-focusable" contract but each needs individual review.

## Risk

Low. All changes are:
- Additive overrides (opt-out of tab-stop on specific classes)
- A defensive sanity-check in the existing `update()` loop
- Seeding initial focus

No public API change, no element layout change, no behavior change on a page where focus was already working.

## Files this PR will touch

- `src/UserInterface.h` — `TextBox::is_tab_stop` override, `UserInterface::focus_first_tab_stop` declaration
- `src/UserInterface.cpp` — `focus_first_tab_stop` impl, focus-sanitize in `update()`
- `src/Sliders.h` — `ValueSlider::is_tab_stop` override declaration
- `src/Sliders.cpp` — `ValueSlider::is_tab_stop` impl
- `src/CUI_Config.cpp` — `enter()` calls `UI->focus_first_tab_stop()`
- `src/CUI_Sysconfig.cpp` — `enter()` calls `UI->focus_first_tab_stop()`

Expected diff: ~40 lines added, no deletions.

## Test plan

- Manual: Ctrl+F12 → Config page 2. Every Tab / Shift-Tab / Up / Down press leaves focus on a visibly highlighted element. Verify the "Return to page 1" button is reachable and the "Current Settings" textbox is NOT a tab stop.
- Manual: F12 → Sysconfig. Tab lands on the first interactive widget. Arrow keys advance through MIDI-out list / skin list / buttons.
- Manual: Shift+Ctrl+F12 → Palette Editor. Unaffected (it uses its own focus model).
- CI: must stay green on Linux / Windows / macOS — no `#ifdef` gating required, all changes are cross-platform.
