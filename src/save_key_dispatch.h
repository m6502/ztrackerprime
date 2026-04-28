// save_key_dispatch.h
//
// Pure decision logic for the global Ctrl-S handler in main.cpp's
// global_keys(). Extracted so the per-page consume / pass-through
// rules (most pages: open Save Song popup; Keybindings: page handles
// it; F4/Shift+F4: silently swallow so we don't yank the user out of
// a table-edit) can be unit-tested without spinning up the SDL key
// pipeline.
//
// History this file pays back: a `break;` exit without consuming the
// key left Ctrl-S in the buffer forever, and the page appeared to
// freeze because every subsequent checkkey() returned the same stuck
// event.

#ifndef ZT_SAVE_KEY_DISPATCH_H
#define ZT_SAVE_KEY_DISPATCH_H

enum SaveKeyAction {
    SAVE_KEY_PASS_THROUGH,    // Not Ctrl-S. Caller does NOT consume.
    SAVE_KEY_LET_PAGE_HANDLE, // Ctrl-S but the current page consumes it
                              //   itself (e.g. Keybindings saves bindings).
                              //   Caller does not consume here either.
    SAVE_KEY_SWALLOW,         // Ctrl-S in a context where it should be a
                              //   no-op (F4/Shift+F4 table editors). Caller
                              //   MUST consume the key from Keys, otherwise
                              //   it sticks in the buffer and every
                              //   subsequent checkkey() returns it -- the
                              //   "page froze on Ctrl-S" symptom.
    SAVE_KEY_OPEN_SAVE_AS,    // Open the Save As dialog (no current filename
                              //   or Shift held).
    SAVE_KEY_OPEN_SAVE_POPUP  // Show the in-place save popup (existing file).
};

// Bag of state pulled from the call-site globals. Decouples the
// dispatcher from zt.h (so the unit test can include this header
// without compiling the world) and makes every input visible.
struct SaveKeyContext {
    bool kstate_ctrl;          // KS_CTRL bit set
    bool kstate_shift;         // KS_SHIFT bit set
    bool kstate_has_alt;       // KS_HAS_ALT(kstate): Alt or macOS Cmd
                               //   (KS_ALT or KS_META) without Ctrl/Shift.
                               //   Lets the F4/Shift+F4 swallow rule
                               //   catch Cmd-S on macOS, where users
                               //   reflexively press Cmd-S.
    bool is_keybindings_state; // cur_state == STATE_KEYBINDINGS
    bool is_macroedit_state;   // cur_state == STATE_MIDIMACEDIT
    bool is_arpedit_state;     // cur_state == STATE_ARPEDIT
    bool song_has_filename;    // song->filename[0] is a real path
};

inline SaveKeyAction dispatch_save_key(const SaveKeyContext &c) {
    // F4 / Shift+F4 must swallow ANY S-with-modifier so the user is
    // never yanked out of the table editor mid-stroke. Both Ctrl-S
    // (Linux/Windows habit) and Cmd-S (macOS habit) are caught here.
    // Without this, the unhandled key sat in the Keys buffer and
    // every subsequent checkkey() returned the same event -- the
    // "page froze on Ctrl-S" symptom.
    if (c.is_macroedit_state && (c.kstate_ctrl || c.kstate_has_alt))
        return SAVE_KEY_SWALLOW;
    if (c.is_arpedit_state   && (c.kstate_ctrl || c.kstate_has_alt))
        return SAVE_KEY_SWALLOW;

    if (!c.kstate_ctrl)          return SAVE_KEY_PASS_THROUGH;
    if (c.is_keybindings_state)  return SAVE_KEY_LET_PAGE_HANDLE;
    if (c.kstate_shift)          return SAVE_KEY_OPEN_SAVE_AS;
    if (c.song_has_filename)     return SAVE_KEY_OPEN_SAVE_POPUP;
    return SAVE_KEY_OPEN_SAVE_AS;
}

#endif // ZT_SAVE_KEY_DISPATCH_H
