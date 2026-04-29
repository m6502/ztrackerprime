# Recipe: add a new widget

Widgets are subclasses of `UserInterfaceElement` (declared in `src/UserInterface.h`). Pages add them to their `UserInterface *UI`, which then dispatches input + render calls.

## The contract

`UserInterfaceElement` declares two pure-virtuals every subclass must implement:

```cpp
virtual int update() = 0;
virtual void draw(Drawable *S, int active) = 0;
```

Plus an optional override:

```cpp
virtual int mouseupdate(int cur_element);
```

Returns the new `cur_element` if the widget claims focus (typically `this->ID`), or the input value if not.

## Where to put it

- **Declare** in `src/UserInterface.h` alongside the existing widget classes.
- **Implement** in `src/UserInterface.cpp`.

Don't put widget code in a `CUI_*.cpp` file — pages are callers, widgets are reusable.

## What goes in each method

- **`update()`** — keyboard/state handling. Read `Keys` (the global `KeyBuffer`) for any keys you consume. Return non-zero if the widget changed state and the page should redraw.
- **`draw(Drawable *S, int active)`** — render to `S`. The `active` flag tells you whether this widget has focus (highlight differently when true).
- **`mouseupdate(int cur_element)`** — handle mouse hits. Test the click against your bounding rect (`x`, `y`, `xsize`, `ysize` are inherited). Return `this->ID` to claim focus; pass `cur_element` through otherwise.

## ListBox subclasses: read first

If you're subclassing `ListBox`, **read [`../references/foot-guns.md`](../references/foot-guns.md)** first. The `mousestate` invariant has caused hard-to-diagnose UI freezes more than once. Two rules:

1. Don't reset `mousestate` at the top of `mouseupdate` — must run after the switch.
2. Don't call `ListBox::OnSelect(selected)` from a subclass override during click handling — the base sets `mousestate=0`, which clobbers the BUTTON_UP path.

## Defaults belong in the constructor

If every caller will set the same value (`xsize`, color, padding), put it in the constructor — don't make every page configure the same thing. See [`../references/foot-guns.md`](../references/foot-guns.md) "widget-class fixes belong upstream."

## ID and registration

Widgets are added to a `UserInterface` and given an integer ID. Pages typically use a small enum of IDs at the top of the `.cpp` so navigation logic (`UI->cur_element`) can reference them by name.

## Pitfalls

- **Zero `xsize` / `ysize`** — mouse hit-tests fail silently. The widget appears on screen but can't be clicked or focused.
- **Returning the wrong value from `mouseupdate`** — return `this->ID` only when claiming focus; otherwise pass `cur_element` through.
- **Per-caller hacks for visual polish** — if multiple callers tweak the same property, fix the widget itself instead.
