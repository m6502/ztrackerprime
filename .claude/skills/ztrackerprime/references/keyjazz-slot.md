# Keyjazz slot pattern (Shift+F4)

The Arpeggio editor uses a dedicated focusable UIE (`ArKeyjazzFocus`, id `KEYJAZZ_ID`) as the keyjazz target instead of always-on keyjazz handling.

## Why a slot, not always-on

Always-on keyjazz competes with letter-key handlers in TextInputs and listbox typeahead. A slot gives an explicit, visible mode toggle: focus the slot to keyjazz, focus anything else to type/scroll.

## Anatomy

- **Class:** subclass of `UserInterfaceElement` with id `KEYJAZZ_ID`.
- **`xsize`/`ysize`:** non-trivial so click-to-focus works (the click hit-test runs against the bounding rect).
- **`update()`:** no-op.
- **`mouseupdate()`:** handles click-to-focus and returns `this->ID`.
- **`draw()`:** empty — the page-level `draw()` paints the visible Keyjazz panel and switches its colors based on `UI->cur_element == KEYJAZZ_ID` (active = highlighted, inactive = dim).
- **Page-level keypress handler:** gates on `focused == KEYJAZZ_ID` to fire `MidiOut->noteOn` + `jazz_set_state(sym, note, chan)`.
- **Note-off:** main.cpp's existing key-up path detects `jazz_note_is_active(sym)` on release and fires `noteOff` automatically — no extra wiring needed in the slot.

## Replicating the pattern elsewhere

If another page wants keyjazz audition, mirror the same five pieces: stub UIE with non-trivial bounds, page-level highlight in `draw()`, page-level keydown gated on focus, rely on main.cpp's keyup path.

Pitfall: if `xsize`/`ysize` are zero, click-to-focus silently fails — the user sees no response and assumes the panel is decorative.
