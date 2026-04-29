// MIDI Mappings — bind incoming MIDI messages to internal actions.
//
// Storage is now keyed on ZtAction (from keybindings.h), so every
// remappable shortcut shares a single action index across the
// keyboard binding system, the MIDI mapping system, and the unified
// Shortcuts & MIDI Mappings page. Adding an action to the ZtAction
// enum automatically gives it a row in the UI and a midi_map_<name>
// entry in zt.conf without further plumbing on this side.
//
// Each action gets ZT_MM_SLOTS_PER_ACTION independent slots so a
// foot switch AND a launchpad pad can fire the same trigger.
//
// Threading
// ---------
//   The MIDI input dispatcher runs on the MIDI callback thread
//   (CoreMIDI read proc / WinMM callback / ALSA seq thread). Action
//   handlers touch shared editor state (cur_edit_row, song->patterns,
//   ...) so they MUST run on the main thread. The dispatcher posts
//   the matching action enum into a small mutex-guarded ring; the
//   main loop drains the ring once per frame and fires the handlers.
//
//   Learn mode is also driven through the same dispatcher: when
//   the learn target is set, the next incoming message is captured
//   into that slot instead of dispatched.

#ifndef ZT_MIDI_MAPPINGS_H
#define ZT_MIDI_MAPPINGS_H

#include "keybindings.h"

#define ZT_MM_SLOTS_PER_ACTION 3

struct ZtMmBinding {
    int  valid;            // 0 = empty slot
    unsigned char status;  // top nibble matters: 0x90 note-on, 0xB0 CC, etc.
    unsigned char data1;   // note number / CC number
};

struct ZtMmMappings {
    // Indexed by ZtAction so every remappable shortcut has a column
    // of MIDI slots. Slot 0 of ZT_ACTION_NONE is unused (reserved as
    // the "unassigned" sentinel inside the enum).
    ZtMmBinding bindings[ZT_ACTION_COUNT][ZT_MM_SLOTS_PER_ACTION];
};

// Direct accessor for the mapping table. Used by the UI page and
// the conf load/save path. Mutating from outside is allowed.
ZtMmMappings *zt_mm_get_mappings(void);

// Try to dispatch an incoming MIDI message. Called from the MIDI
// input callback thread. Returns 1 if the message was consumed
// (matched a binding OR captured by learn mode), 0 if it should
// flow on to normal MIDI-In processing.
int zt_mm_dispatch(unsigned char status, unsigned char data1, unsigned char data2);

// Drain pending action posts. Called from the main loop once per
// frame. Runs handlers on the main thread.
void zt_mm_drain_actions(void);

// Learn mode -- set with a (action, slot) pair to redirect the next
// incoming MIDI message into that slot instead of dispatching it.
// Pass action == ZT_ACTION_NONE to cancel.
void zt_mm_set_learn_target(int action, int slot);
int  zt_mm_get_learn_action(void);  // ZT_ACTION_NONE if not active
int  zt_mm_get_learn_slot(void);    // -1 if not active

// Slot mutation -- used by the UI page's "Clear" affordance.
void zt_mm_clear_slot(int action, int slot);

// Pretty-print a binding into a small fixed buffer ("90 3C", "B0 7F",
// or "----"). Buffer must be at least 6 bytes.
void zt_mm_format_binding(const ZtMmBinding *b, char *out, int out_len);

#endif // ZT_MIDI_MAPPINGS_H
