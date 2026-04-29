#include "midi_mappings.h"

#include "zt.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

// File-private state -----------------------------------------------------------

namespace {

ZtMmMappings g_mappings = {};

// Pending-action ring. MIDI thread writes via post_action_locked;
// main thread drains via zt_mm_drain_actions. SDL_Mutex protects head/tail.
struct ActionRing {
    int actions[16];
    int head;          // write index
    int tail;          // read index
    SDL_Mutex *mtx;    // lazily created in init_ring
};
ActionRing g_ring = { {0}, 0, 0, NULL };

// Learn target. ZT_ACTION_NONE means inactive. Reads/writes are
// protected by the same mutex as the ring (the dispatcher locks for
// both checks).
int g_learn_action = ZT_ACTION_NONE;
int g_learn_slot   = -1;

void init_ring() {
    if (!g_ring.mtx) {
        g_ring.mtx = SDL_CreateMutex();
    }
}

void post_action_locked(int action) {
    int next = (g_ring.head + 1) % (int)(sizeof(g_ring.actions) / sizeof(g_ring.actions[0]));
    if (next == g_ring.tail) {
        // Ring full -- drop the oldest to keep the latest. A burst
        // of mapped MIDI events past the ring depth is rare; preferring
        // recent over stale is the right tradeoff for an audition key.
        g_ring.tail = (g_ring.tail + 1) % (int)(sizeof(g_ring.actions) / sizeof(g_ring.actions[0]));
    }
    g_ring.actions[g_ring.head] = action;
    g_ring.head = next;
}

bool match_binding(const ZtMmBinding *b, unsigned char status, unsigned char data1) {
    if (!b->valid) return false;
    return b->status == status && b->data1 == data1;
}

} // namespace

// Public API -------------------------------------------------------------------

ZtMmMappings *zt_mm_get_mappings(void) {
    return &g_mappings;
}

void zt_mm_clear_slot(int action, int slot) {
    if (action <= ZT_ACTION_NONE || action >= ZT_ACTION_COUNT) return;
    if (slot < 0 || slot >= ZT_MM_SLOTS_PER_ACTION) return;
    g_mappings.bindings[action][slot] = ZtMmBinding{0, 0, 0};
}

void zt_mm_set_learn_target(int action, int slot) {
    init_ring();
    SDL_LockMutex(g_ring.mtx);
    if (action <= ZT_ACTION_NONE || slot < 0 ||
        action >= ZT_ACTION_COUNT ||
        slot >= ZT_MM_SLOTS_PER_ACTION) {
        g_learn_action = ZT_ACTION_NONE;
        g_learn_slot   = -1;
    } else {
        g_learn_action = action;
        g_learn_slot   = slot;
    }
    SDL_UnlockMutex(g_ring.mtx);
}

int zt_mm_get_learn_action(void) {
    init_ring();
    SDL_LockMutex(g_ring.mtx);
    int a = g_learn_action;
    SDL_UnlockMutex(g_ring.mtx);
    return a;
}

int zt_mm_get_learn_slot(void) {
    init_ring();
    SDL_LockMutex(g_ring.mtx);
    int s = g_learn_slot;
    SDL_UnlockMutex(g_ring.mtx);
    return s;
}

int zt_mm_dispatch(unsigned char status, unsigned char data1, unsigned char /*data2*/) {
    // Real-time messages (clock, start, stop, ...) are noise here.
    // Only short channel messages can be bound.
    if (status < 0x80 || status >= 0xF0) return 0;

    init_ring();
    SDL_LockMutex(g_ring.mtx);

    // Learn mode: capture into the requested slot and consume.
    if (g_learn_action > ZT_ACTION_NONE && g_learn_slot >= 0) {
        ZtMmBinding *b = &g_mappings.bindings[g_learn_action][g_learn_slot];
        b->valid  = 1;
        b->status = status;
        b->data1  = data1;
        g_learn_action = ZT_ACTION_NONE;
        g_learn_slot   = -1;
        SDL_UnlockMutex(g_ring.mtx);
        return 1;
    }

    // Otherwise look for a matching binding. First match wins, scan
    // skips ZT_ACTION_NONE.
    int matched_action = ZT_ACTION_NONE;
    for (int a = ZT_ACTION_NONE + 1; a < ZT_ACTION_COUNT && matched_action == ZT_ACTION_NONE; a++) {
        for (int s = 0; s < ZT_MM_SLOTS_PER_ACTION; s++) {
            if (match_binding(&g_mappings.bindings[a][s], status, data1)) {
                matched_action = a;
                break;
            }
        }
    }
    if (matched_action != ZT_ACTION_NONE) {
        post_action_locked(matched_action);
    }
    SDL_UnlockMutex(g_ring.mtx);
    return matched_action != ZT_ACTION_NONE ? 1 : 0;
}

// Action handlers --------------------------------------------------------------
//
// Today only the audition actions have runtime dispatch wired up.
// Other actions still appear in the page (so the user can SEE that
// they're remappable and bind MIDI to them) but pressing the bound
// MIDI key is a no-op until the corresponding action is plumbed in.
// Adding a new dispatch is just a switch arm here.

extern int cur_edit_pattern;
extern int cur_edit_track;
extern int cur_edit_row;
extern int cur_edit_row_disp;
extern int cur_edit_col;
extern int cur_step;
extern int PATTERN_EDIT_ROWS;
extern int need_refresh;

namespace {

bool in_pattern_editor_note_col() {
    if (!UIP_Patterneditor || ActivePage != UIP_Patterneditor) return false;
    if (cur_edit_col < 0 || cur_edit_col >= 41) return false;
    return edit_cols[cur_edit_col].type == T_NOTE;
}

int audition_advance_amount() {
    switch (zt_config_globals.note_audition_step_mode) {
        case ZT_NAS_NONE:     return 0;
        case ZT_NAS_EDITSTEP: return cur_step;
        case ZT_NAS_ONE:
        default:              return 1;
    }
}

void audition_advance_after() {
    int advance = audition_advance_amount();
    if (advance <= 0) return;
    cur_edit_row += advance;
    if ((cur_edit_row - 1 - cur_edit_row_disp) >= (PATTERN_EDIT_ROWS - 1)) {
        if (cur_edit_row_disp <
            (song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
            cur_edit_row_disp += advance;
        }
    }
    need_refresh++;
}

void run_action(int action) {
    if (!ztPlayer) return;
    switch (action) {
        case ZT_ACTION_NOTE_AUDITION:
            if (!in_pattern_editor_note_col()) return;
            ztPlayer->play_current_note();
            audition_advance_after();
            break;
        case ZT_ACTION_ROW_AUDITION:
            if (!in_pattern_editor_note_col()) return;
            ztPlayer->play_current_row();
            audition_advance_after();
            break;
        default:
            // Other actions are listed in the UI for binding but
            // not yet wired to a runtime dispatcher. Adding them is
            // just a new case here.
            break;
    }
}

} // namespace

void zt_mm_drain_actions(void) {
    init_ring();
    while (true) {
        int action = ZT_ACTION_NONE;
        SDL_LockMutex(g_ring.mtx);
        if (g_ring.tail != g_ring.head) {
            action = g_ring.actions[g_ring.tail];
            g_ring.tail = (g_ring.tail + 1) %
                (int)(sizeof(g_ring.actions) / sizeof(g_ring.actions[0]));
        }
        SDL_UnlockMutex(g_ring.mtx);
        if (action == ZT_ACTION_NONE) break;
        run_action(action);
    }
}

void zt_mm_format_binding(const ZtMmBinding *b, char *out, int out_len) {
    if (!out || out_len <= 0) return;
    if (!b || !b->valid) {
        snprintf(out, out_len, "----");
        return;
    }
    snprintf(out, out_len, "%02X %02X",
             (unsigned)b->status, (unsigned)b->data1);
}
