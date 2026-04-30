# Recurring foot-guns

Bug classes that have bitten more than once. Read before debugging UI freezes, click-handling weirdness, or "the same fix in five files."

## ListBox subclass mousestate fragility

`ListBox::mouseupdate` has a subtle invariant: `mousestate` must be non-zero when `BUTTON_UP_LEFT` arrives, because `act++` is gated on it. If `act` doesn't increment, `Keys.getkey()` is never called and the up event sits forever in the FIFO, blocking every subsequent input.

Two ways `mousestate` gets clobbered prematurely:

1. **Pre-switch reset.** `if (!bMouseIsDown) mousestate = 0;` AT THE TOP of `mouseupdate` clears it before the switch sees BUTTON_UP. The reset must run AFTER the switch so the BUTTON_UP_LEFT case can still observe `mousestate==1` for the listbox that owns the click.

2. **Premature `ListBox::OnSelect(selected)` from a subclass.** The base `ListBox::OnSelect` sets `mousestate = 0`. If a subclass override calls it during click handling (between BUTTON_DOWN and BUTTON_UP), mousestate clears early — same freeze. Don't call `ListBox::OnSelect` from a subclass; the BUTTON_UP_LEFT case clears mousestate itself.

**Symptom:** "click on widget freezes the UI; Cmd-Tab unfreezes it." The Cmd-Tab unfreeze is `SDL_EVENT_WINDOW_FOCUS_GAINED → Keys.flush()` in `src/main.cpp::zt_handle_platform_window_event`.

## Widget-class fixes belong upstream

When a rendering or behavior bug appears on **multiple pages with the same widget**, fix it in `src/UserInterface.cpp` (the widget class), not in each `CUI_*.cpp` caller.

If you're editing the same property (`xsize`, `frame`, color, padding) across more than two callers to fix the *same visual problem*, **stop and refactor**. The call sites are symptomatic; the widget's `::draw()` or constructor is the real bug.

Concrete patterns this catches:

- **Default values in the constructor.** Every caller setting `cb->xsize = 5` → set it once in the constructor.
- **Cap or ignore caller-supplied values inside `::draw()`** when callers historically passed bad values. Example: `CheckBox::draw` caps the wipe extent at 3 (the "Off" width) regardless of `xsize`, so legacy `xsize=5` callers don't bleed.
- **Make a flag a no-op** if every caller's correct value is the same. Example: `CheckBox::frame` is a no-op — every audit ended with removing `frame=1`, so the frame block was deleted from `CheckBox::draw`.
- **Sentinel cases stay in the caller.** A `MidiOutDeviceOpener` with custom `xsize`, a `BankSelectCheckBox` overriding click behavior — those *are* per-caller. Don't push genuinely-distinct logic into the base widget.

Heuristic: if a series of caller-side edits keep fixing the same visual issue, the next fix should be upstream.

## User-reported inputs are ground truth

When the user reports a specific input ("I pressed Ctrl-S"), that IS what they pressed. Do not respond by substituting a different input ("you were probably pressing Cmd-S" / "you might have meant X"). That is gaslighting — overwriting their observation with a hypothesis convenient for the model.

**Banned phrasings (any variant):**
- "Most likely cause: you were pressing X (instead of what you said)"
- "You probably / likely / actually pressed [something else]"
- "Maybe you're hitting [different key]"
- Any sentence whose effect is to substitute the user's reported input with model speculation.

**When the trace doesn't explain the symptom**, the model is wrong, not the user. Allowed responses:

1. Say so. "I traced Ctrl-S through the dispatch and can't see why it would freeze. I'm missing something."
2. Ask for more detail. "Is the cursor still moving? Do toolbar clicks still respond? Does the status bar update?"
3. Ask which binary they're running.
4. Add diagnostic output. Insert `status_change=1; statusmsg=...` in the suspected branch to verify which path fired.

## Pre-switch / pre-return cleanup that defeats observability

Generalisation of the ListBox case. Resetting state at the *top* of an event handler — before the switch dispatches the current event — clobbers the very state the dispatch is supposed to see. If a state field gates downstream behavior (`act++`, `dirty=1`, an event-consumed flag), reset it AFTER the switch, not before. When in doubt: trace one event end-to-end, note which fields it depends on at each step, and only clear those fields *after* the last reader.

## Vanishing sibling widgets on `needaclear`

`UserInterface::draw` skips elements whose `need_redraw == 0`. Most widgets self-arm `need_redraw=1` only on internal change — value tweak, mouse drag, focus change. So once a widget has rendered, it sits dormant.

The trap: when *any* sibling fires `needaclear++` (the numeric-input popup return path in `ValueSlider::update` is one example), `UI::draw` clears row 12+ wholesale and only `need_redraw==1` elements repaint that frame. Every dormant widget silently disappears until *its* next state change.

Symptoms:

- Drag one slider → all the others vanish.
- Click into a list → the slot grid disappears.
- Tab into a different pane → the previously-active pane vanishes.

**Fix idiom:** any page with always-visible widgets stamps `need_redraw = 1` on each one *every frame* in the page's `update()` (or `position_sliders()` for pool widgets). Cheap (an int write) and eliminates the bug class. Worked example: `CUI_CcConsole::position_sliders` + `update()` after PR #96.

## Global ESC handler shadows page ESC

`main.cpp::global_keys` intercepts ESC and pops up the Main Menu when `cur_state` isn't in an exclusion list (`STATE_HELP`, `STATE_ABOUT`, `STATE_LUA_CONSOLE`, `STATE_SONG_MESSAGE`, `STATE_KEYBINDINGS`, ...). It runs *before* the active page's `update()`, so any page whose own ESC handler does something user-visible — "Capture cancelled.", "Learn cancelled.", "Discard unsaved?" — is shadowed by default. The page's ESC code only fires when ESC happens to be in a context the global handler doesn't claim.

When you add a new page whose ESC has dedicated meaning, **add its `STATE_*` to that exclusion list in the same PR**. PR #95 was a one-line fix for `STATE_KEYBINDINGS` after the user reported pressing ESC and getting the menu instead of "Capture cancelled."

## Saturated ring buffer with `head == tail`

`KeyBuffer` originally checked `head != tail` to gate read/peek. Fails when the buffer is exactly full (head wraps back to tail). Fix: gate on `cursize > 0`. If you write a fixed-capacity ring with two pointers, also keep a count.
