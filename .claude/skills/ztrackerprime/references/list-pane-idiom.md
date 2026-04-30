# List-pane idiom (file list, preset list, device list, ...)

Any pane that shows a vertical list of named items the user picks from must use the **real `ListBox` widget**, never a hand-drawn `> filename` text marker. Worked examples in master:

- `SkinSelector` (F11 Sysconfig — skin picker)
- `MmPresetSelector` / `ArPresetSelector` (F4 / Shift+F4 — preset picker)
- `MidiOutDeviceSelector` / `MidiInDeviceOpener` (F12 — device picker)
- `CcFileSelector` (Shift+F3 — CCizer file picker; PR #96)

The pattern is identical across all of them. New list-driven pages (and rewrites of old ones) MUST follow it.

## Why a real `ListBox`, not a hand-drawn list

A hand-drawn `printBG` list looks visually similar in a single screenshot but breaks in every other dimension: no mouse click, no typeahead, no scroll, no consistent highlight color, no tab-focus integration, no widget-upstream bug fixes. The user **will** find it by trying to click. (CC Console shipped this way at #78 and got rebuilt at #96 — don't repeat that.)

## The pattern (5 pieces)

### 1. The list widget — `ListBox` subclass

In an anonymous namespace at the top of the page's `.cpp`:

```cpp
namespace {
class FooListSelector : public ListBox {
public:
    FooListSelector() {
        empty_message       = "(none found)";
        is_sorted           = true;        // alphabetical, matches F11
        use_checks          = true;        // checkmark on the active item
        use_key_select      = true;        // typeahead jump
        wrap_focus_at_edges = false;       // clamp at edges (no Tab spillover)
        frame               = 1;
    }
    void OnChange()       override {}
    void OnSelectChange() override {}
    int update() override {
        // Mirror F4/Shift+F4: Space applies, just like Enter.
        // Base ListBox::update only treats Enter as a select trigger.
        KBKey k = Keys.checkkey();
        if (k == SDLK_SPACE) {
            Keys.getkey();
            LBNode *p = getNode(cur_sel + y_start);
            if (p) OnSelect(p);
            need_refresh++;
            need_redraw++;
            return 0;
        }
        return ListBox::update();
    }
    void OnSelect(LBNode *selected) override {
        if (!selected || !g_self_for_listbox) return;
        g_self_for_listbox->load_selected();
        selectNone();
        selected->checked = true;
        // NEVER call ListBox::OnSelect from a subclass — see foot-guns.md
        // (it sets mousestate=0 mid-click, freezes the UI on every BUTTON_UP).
    }
};
} // namespace
```

The `g_self_for_listbox` static back-pointer is set in the page constructor: `g_self_for_listbox = this;`. Use it because anonymous-namespace types can't carry a typed page pointer without leaking the class name into the header.

### 2. The non-list pane — 1x1 stub UIE

For the *other* pane on the page (slot grid, edit form, etc.) — the part that doesn't have a real widget but needs Tab focus — add a 1x1 stub UIE so Tab can claim "we are on the grid":

```cpp
class FooGridFocus : public UserInterfaceElement {
public:
    FooGridFocus() { xsize = 1; ysize = 1; }
    int update() override { return 0; }
    void draw(Drawable *S, int active) override { (void)S; (void)active; }
};
```

The page's `update()` then routes arrow keys / letter keys to its own logic when `UI->cur_element == FOO_GRIDFOCUS_ID`. (See `MmGridFocus` on F4, `CcSlotGridFocus` on Shift+F3.)

### 3. Constructor — register both, plus any pool widgets

```cpp
#define FOO_LIST_ID        50
#define FOO_GRIDFOCUS_ID   51
#define FOO_SLIDER_ID_BASE 100

CUI_FooPage::CUI_FooPage() {
    UI = new UserInterface;
    g_self_for_listbox = this;

    auto *fs = new FooListSelector;
    fs->x = LIST_X; fs->y = LIST_Y;
    fs->xsize = LIST_W; fs->ysize = 16;   // ysize = LAST visible row index
    UI->add_element(fs, FOO_LIST_ID);
    list_selector = fs;

    auto *gf = new FooGridFocus;
    gf->x = GRID_X; gf->y = GRID_Y;
    UI->add_element(gf, FOO_GRIDFOCUS_ID);
    grid_focus = gf;

    // ...slider/widget pool registration...
}
```

`list_selector` and `grid_focus` are members typed as `ListBox *` and `UserInterfaceElement *` in the page's header. Forward-declare both at the top of `CUI_Page.h`.

### 4. Per-frame `update()` — derive focus, stamp `need_redraw`, resize

```cpp
void CUI_FooPage::update(void) {
    int total_rows    = INTERNAL_RESOLUTION_Y / CHARACTER_SIZE_Y;
    int grid_max_rows = total_rows - GRID_Y - SPACE_BOTTOM - 2;
    if (grid_max_rows < 4) grid_max_rows = 4;

    // Resize listbox to current viewport. ysize is LAST row index.
    if (list_selector) {
        list_selector->ysize = grid_max_rows - 2;
        list_selector->need_redraw = 1;          // see "vanishing-sibling" below
    }
    if (grid_focus) grid_focus->need_redraw = 1;

    // Page-level focus is DERIVED from cur_element; never stored.
    int focus_now = (UI->cur_element == FOO_LIST_ID) ? 0 : 1;

    // ...position_sliders() etc, each visible widget gets need_redraw = 1...

    UI->update();
    // ...absorb widget changes...

    if (!Keys.size()) return;
    KBMod kstate = Keys.getstate();
    KBKey key   = Keys.getkey();

    if (key == SDLK_TAB) {
        UI->cur_element = (UI->cur_element == FOO_LIST_ID)
                              ? FOO_GRIDFOCUS_ID
                              : FOO_LIST_ID;
        need_refresh++;
        return;
    }
    if (focus_now == 0) {
        // Up/Down/Enter/Space/typeahead are handled by the listbox's
        // own update() via UI->update() above. Nothing to do here.
        return;
    }
    // ...slot-grid / edit-form key handlers...
}
```

### 5. `enter()` — rebuild items, land focus on the list

```cpp
void CUI_FooPage::enter(void) {
    cur_state = STATE_FOOPAGE;
    rescan_folder();        // populates files[] and calls rebuild_list_items()
    UI->cur_element = FOO_LIST_ID;
    Keys.flush();
    need_refresh++;
    UI->enter();
}

void CUI_FooPage::rebuild_list_items(void) {
    if (!list_selector) return;
    list_selector->clear();
    for (int i = 0; i < num_files; i++) {
        LBNode *p = list_selector->insertItem(files[i]);
        if (p && active_basename_matches(files[i])) p->checked = true;
    }
    list_selector->setCursor(active_index_in_files);
}
```

## Vanishing-sibling fix (must read)

`UserInterface::draw` skips elements with `need_redraw == 0`:

```cpp
while (e) {
    if (e->need_redraw) {
        e->draw(S, (e->ID == cur_element));
        e->need_redraw = 0;
    }
    e = e->next;
}
```

If anything in the same UI fires `needaclear++` (e.g. the numeric-input popup return path in `ValueSlider::update`), the *whole* element area is cleared — and any sibling widget that didn't independently set `need_redraw=1` that frame **vanishes** until the next time *it* changes.

The fix is the convention shown in #4 above: any per-frame `update()` that has a "always-visible widget" (the listbox, a stub UIE, a pool of sliders) **stamps `need_redraw = 1` on each one every frame**. This is cheap (it's just an int write; `draw()` is still gated by widget-internal flags) and it eliminates the entire bug class. Worked example: `CUI_CcConsole.cpp::position_sliders` + `update()` in PR #96.

You'll know you forgot it because: drag one slider → the others disappear; click into the list → the slot grid disappears; Tab → the previously-focused pane vanishes.

## Layout: column-per-print, never one packed sprintf

When laying out a row of (slot, CC#, name, slider, value, learn) — paint each column as its own `printBG` at its own `col(CC_*_COL)`. **Never** pack adjacent columns into one `snprintf("%-Ns%-Ms ...", ...)`. Two reasons:

1. **Overlap.** A 12-char packed prefix at `col=30` + a name field starting at `col=39` will smash the prefix tail into the name (`PBPitchbend`, `1 Mod Wheel`, `43Filter Cutoff`).
2. **Header drift.** Headers and data rows must use the *same column constants*. If the data row is one packed string but the headers are separate prints, they go out of sync the first time you adjust a column.

Headers should also be one print per column, anchored to the same constants:

```cpp
TColor hdr_fg = (focus_now == 1) ? COLORS.Brighttext : COLORS.Text;
print(col(CC_SLOT_COL),  row(GRID_Y - 1), "Slot",   hdr_fg, S);
print(col(CC_CC_COL),    row(GRID_Y - 1), "CC#",    hdr_fg, S);
print(col(CC_NAME_COL),  row(GRID_Y - 1), "Name",   hdr_fg, S);
print(col(CC_SLIDER_X),  row(GRID_Y - 1), "Slider", hdr_fg, S);
print(col(CC_LEARN_COL), row(GRID_Y - 1), "Learn",  hdr_fg, S);
```

Don't tack value-display words like `"Val"` onto the slider header — it overlaps the next column ("LeUani" in the wild). The slider widget shows its own value to the right of its bar.

## Global ESC handler exclusion

`global_keys()` in `main.cpp` intercepts `ESC` and pops up the Main Menu unless `cur_state` is in an exclusion list. **Any new page whose own ESC handler matters (cancel-capture, cancel-learn, in-progress dialog, etc.) must add its `STATE_*` to that exclusion list** — otherwise the page's ESC handler is shadowed and only fires when the menu is dismissed. PR #95 was a one-line fix for `STATE_KEYBINDINGS`; the same trap applies to any new page.

## Don't claim "feature works" from `tests pass` + a screencap

CI green and 8/8 ctest passing tells you the page compiles and the unit-tested pure modules behave. They do not tell you the page renders, that Tab toggles focus, that the listbox repopulates, or that the slider stays painted under drag. Before declaring a list-driven page done:

1. Launch the binary yourself (`open .../zt.app`).
2. Send F-key to switch to the page (osascript `key code` works for F-keys; AppleScript permissions need to be enabled for your terminal).
3. Capture by `CGWindowListCopyWindowInfo` window-id (`scripts/zt-screenshot.sh` post-#94 does this — use it).
4. Send each key the user is supposed to press: arrow Down, Tab, Space, Enter, Esc.
5. Diff the screenshots. Verify the listbox highlight moves, the slider stays painted, the value updates, ESC actually exits.

If the page passes its tests but you can't visually verify it, say "I can't verify the UI; I checked X, Y, Z but not the actual page render." Don't ship hopes — the user finds out by trying.
