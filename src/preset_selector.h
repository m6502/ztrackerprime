// preset_selector.h
//
// Pure event-handling logic for the F4 / Shift+F4 inline preset
// listbox. Extracted out of CUI_Midimacroeditor.cpp / CUI_Arpeggioeditor.cpp
// so the click / arrow / space / P-cycle decisions can be unit-tested
// without instantiating ListBox, KeyBuffer, or any SDL globals.
//
// Each "preset_on_*" entry point is a pure function: takes the current
// cursor / active / count state plus event input, returns a decision
// that the caller (the listbox subclass) translates into mutation:
//
//   * fire_apply       -- run *_apply_preset_to() and mark
//                         *_preset_just_applied so the page's update()
//                         pulls fresh data into slider widgets.
//   * new_cursor_row   -- update cur_sel / y_start (negative = unchanged).
//   * consumed         -- the event was handled; don't pass through to
//                         the base ListBox handler (avoids fighting its
//                         "Up at top surrenders focus" / typeahead-eats-P
//                         behaviours).
//
// The bug history this file pays back:
//   * Click on the active row was hidden because both the parent ListBox
//     (cur_sel != click_row at the moment of the check) and our override
//     (int_data dedupe) skipped OnSelect. preset_on_click now ALWAYS
//     apply()-s on a valid click row.
//   * Up at top / Down at bottom passed focus to the previous/next tab
//     stop, breaking arrow-after-click. preset_on_up / preset_on_down
//     stay focused at the boundary.
//   * P-cycle was eaten by ListBox typeahead (use_key_select=true) so
//     it never reached the page's cycle handler. preset_on_cycle is
//     called explicitly by the override before the base sees the key.

#ifndef ZT_PRESET_SELECTOR_H
#define ZT_PRESET_SELECTOR_H

struct PresetCursor {
    int num_items;     // Total preset rows.
    int active_index;  // Index of the "applied" preset (drives the checkmark).
    int cursor_row;    // Highlighted row (0..num_items-1).
};

struct PresetEvent {
    bool fire_apply;       // Caller should run *_apply_preset_to() now.
    int  new_cursor_row;   // -1 = leave cursor where it is.
    bool consumed;         // Event handled; don't fall through to base ListBox.
    bool stay_focused;     // When false, the override returns ListBox::update's
                           //   "ret = -1/1" (the tab-stop surrender). For us
                           //   this is always true on edge cases -- the field
                           //   exists to make the contract explicit and to leave
                           //   room for future "tab away on Up at top" if asked.
};

inline PresetEvent preset_event_default() {
    PresetEvent e;
    e.fire_apply       = false;
    e.new_cursor_row   = -1;
    e.consumed         = false;
    e.stay_focused     = true;
    return e;
}

// Mouse click landed on row `click_row` of the listbox. click_row is the
// CONTENT-relative row (cur_sel + y_start in ListBox terms), not the
// pixel-row. Caller must clamp negatives / out-of-range before calling
// or pass them through here -- we silently no-op on out-of-bounds.
inline PresetEvent preset_on_click(const PresetCursor &s, int click_row) {
    PresetEvent e = preset_event_default();
    if (s.num_items <= 0)             return e;
    if (click_row  < 0)               return e;
    if (click_row  >= s.num_items)    return e;
    e.fire_apply     = true;
    e.new_cursor_row = click_row;
    e.consumed       = true;
    return e;
}

// Up arrow. At top of the list we stay focused (do NOT surrender to the
// previous tab stop the way bare ListBox does).
inline PresetEvent preset_on_up(const PresetCursor &s) {
    PresetEvent e = preset_event_default();
    if (s.cursor_row > 0) {
        e.new_cursor_row = s.cursor_row - 1;
        e.consumed       = true;
    } else {
        // At top -- consume the event but don't move. The bare ListBox
        // would return ret=-1 here and lose focus.
        e.consumed = true;
    }
    return e;
}

// Down arrow. At bottom of the list we stay focused.
inline PresetEvent preset_on_down(const PresetCursor &s) {
    PresetEvent e = preset_event_default();
    if (s.cursor_row < s.num_items - 1) {
        e.new_cursor_row = s.cursor_row + 1;
        e.consumed       = true;
    } else {
        e.consumed = true;
    }
    return e;
}

// Enter or Space: apply the preset under the cursor without moving it.
inline PresetEvent preset_on_apply(const PresetCursor &s) {
    PresetEvent e = preset_event_default();
    if (s.num_items <= 0) return e;
    e.fire_apply = true;
    e.consumed   = true;
    return e;
}

// P key (no modifiers): cycle to the next preset.
inline PresetEvent preset_on_cycle(const PresetCursor &s) {
    PresetEvent e = preset_event_default();
    if (s.num_items <= 0) return e;
    e.fire_apply     = true;
    e.new_cursor_row = (s.active_index + 1) % s.num_items;
    e.consumed       = true;
    return e;
}

#endif // ZT_PRESET_SELECTOR_H
