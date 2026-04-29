// MIDI Mappings — bind incoming MIDI messages to internal actions.
//
// First two actions wired up: Note Audition (== keyboard "4" on the
// Pattern Editor note column) and Row Audition (== "8"). Each action
// has up to ZT_MM_SLOTS_PER_ACTION independent bindings so a user can
// trigger the same action from multiple controllers (a foot switch
// AND a launchpad pad, for example).
//
// Architecture
// ------------
//   * ZtMmAction   -- enum of bindable actions. Add to the end; the
//                     names array in midi_mappings.cpp must stay in
//                     sync.
//   * ZtMmBinding  -- one captured MIDI message-shape (status byte +
//                     data1). data2 (note velocity / CC value) is
//                     intentionally not matched so a single binding
//                     fires regardless of how hard the user hits the
//                     pad.
//   * Three slots per action, persisted to zt.conf as
//         midi_map_<action_name>: <s1,d1> <s2,d2> <s3,d3>
//     Empty slots write as "-".
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
//   g_mm_learn_target is set, the next incoming message is captured
//   into that slot instead of dispatched.

#ifndef ZT_MIDI_MAPPINGS_H
#define ZT_MIDI_MAPPINGS_H

#ifdef __cplusplus
extern "C" {
#endif

enum ZtMmAction {
    ZT_MM_NOTE_AUDITION = 0,   // == keyboard "4" — play current note + advance
    ZT_MM_ROW_AUDITION,        // == keyboard "8" — play current row  + advance
    ZT_MM_NUM_ACTIONS
};

#define ZT_MM_SLOTS_PER_ACTION 3

struct ZtMmBinding {
    int  valid;          // 0 = empty slot
    unsigned char status; // top nibble matters: 0x90 note-on, 0xB0 CC, etc.
    unsigned char data1;  // note number / CC number
};

struct ZtMmMappings {
    ZtMmBinding bindings[ZT_MM_NUM_ACTIONS][ZT_MM_SLOTS_PER_ACTION];
};

// File-private state lives in midi_mappings.cpp; access through the
// helpers below.

// Action name keys used by zt.conf (e.g. "note_audition") and
// rendered as table headers in the UI ("Note Audition"). The two
// arrays are guaranteed to have ZT_MM_NUM_ACTIONS entries.
const char *zt_mm_action_conf_key(int action);
const char *zt_mm_action_display_name(int action);

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
// Pass action == -1 to cancel.
void zt_mm_set_learn_target(int action, int slot);
int  zt_mm_get_learn_action(void);  // -1 if not active
int  zt_mm_get_learn_slot(void);    // -1 if not active

// Slot mutation -- used by the UI page's "Clear" affordance.
void zt_mm_clear_slot(int action, int slot);

// Pretty-print a binding into a small fixed buffer ("90 3C", "B0 7F",
// or "----"). Buffer must be at least 6 bytes.
void zt_mm_format_binding(const ZtMmBinding *b, char *out, int out_len);

#ifdef __cplusplus
}
#endif

#endif // ZT_MIDI_MAPPINGS_H
