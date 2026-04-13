// Simple single-level undo for pattern editing.
// Saves a snapshot of one pattern's event data before destructive operations.

#ifndef ZT_UNDO_H
#define ZT_UNDO_H

#include "module.h"

struct UndoEvent {
    unsigned short row;
    unsigned char note, inst, vol, effect;
    unsigned short length, effect_data;
};

struct UndoTrack {
    UndoEvent *events;
    int count;
};

class PatternUndo {
public:
    int saved_pattern;
    int saved_length;
    UndoTrack tracks[MAX_TRACKS];
    bool has_undo;

    PatternUndo() : saved_pattern(-1), saved_length(0), has_undo(false) {
        for (int i = 0; i < MAX_TRACKS; i++) {
            tracks[i].events = nullptr;
            tracks[i].count = 0;
        }
    }

    ~PatternUndo() { clear(); }

    void clear() {
        for (int i = 0; i < MAX_TRACKS; i++) {
            delete[] tracks[i].events;
            tracks[i].events = nullptr;
            tracks[i].count = 0;
        }
        has_undo = false;
    }

    // Save a snapshot of the given pattern
    void save(zt_module *song, int pat_idx) {
        clear();
        if (!song->patterns[pat_idx]) return;

        saved_pattern = pat_idx;
        saved_length = song->patterns[pat_idx]->length;

        for (int t = 0; t < MAX_TRACKS; t++) {
            track *trk = song->patterns[pat_idx]->tracks[t];
            if (!trk) continue;

            // Count events
            int count = 0;
            for (int r = 0; r < saved_length; r++) {
                if (trk->get_event(r)) count++;
            }

            if (count == 0) continue;

            tracks[t].events = new UndoEvent[count];
            tracks[t].count = count;
            int idx = 0;
            for (int r = 0; r < saved_length; r++) {
                event *e = trk->get_event(r);
                if (e) {
                    tracks[t].events[idx].row = r;
                    tracks[t].events[idx].note = e->note;
                    tracks[t].events[idx].inst = e->inst;
                    tracks[t].events[idx].vol = e->vol;
                    tracks[t].events[idx].effect = e->effect;
                    tracks[t].events[idx].length = e->length;
                    tracks[t].events[idx].effect_data = e->effect_data;
                    idx++;
                }
            }
        }
        has_undo = true;
    }

    // Restore the saved snapshot
    int restore(zt_module *song) {
        if (!has_undo || saved_pattern < 0) return -1;
        if (!song->patterns[saved_pattern]) return -1;

        // Clear all events in the pattern
        for (int t = 0; t < MAX_TRACKS; t++) {
            track *trk = song->patterns[saved_pattern]->tracks[t];
            if (!trk) continue;
            trk->reset();
        }

        // Resize if needed
        if (song->patterns[saved_pattern]->length != saved_length) {
            song->patterns[saved_pattern]->resize(saved_length);
        }

        // Restore events
        for (int t = 0; t < MAX_TRACKS; t++) {
            track *trk = song->patterns[saved_pattern]->tracks[t];
            if (!trk || tracks[t].count == 0) continue;

            for (int i = 0; i < tracks[t].count; i++) {
                UndoEvent &ue = tracks[t].events[i];
                trk->update_event(ue.row, ue.note, ue.inst, ue.vol, ue.length, ue.effect, ue.effect_data);
            }
        }

        has_undo = false;
        return saved_pattern;
    }
};

#endif
