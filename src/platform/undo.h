// Bounded, song-local history for pattern data. Editor transactions keep a
// whole block operation (or a drawing gesture) together and ignore no-op edits.
#ifndef ZT_UNDO_H
#define ZT_UNDO_H

#include "module.h"
#include "song_tools.h"
#include <algorithm>
#include <array>
#include <deque>
#include <memory>
#include <tuple>
#include <vector>

class PatternUndo {
    struct Note {
        int track;
        unsigned short row, length, effect_data;
        unsigned char note, inst, vol, effect;
        auto fields() const {
            return std::tie(track, row, length, effect_data, note, inst, vol, effect);
        }
        bool operator==(const Note &other) const { return fields() == other.fields(); }
    };
    struct Snapshot {
        int index, length = 0;
        bool present = false;
        std::array<bool, MAX_TRACKS> tracks{};
        std::vector<Note> notes;
        bool operator==(const Snapshot &other) const {
            return index == other.index && length == other.length &&
                   present == other.present && tracks == other.tracks && notes == other.notes;
        }
    };
    struct Change { Snapshot before, after; };
    struct Entry {
        std::vector<Change> changes;
        int before_focus = -1, after_focus = -1, group = 0;
        int before_track = -1, after_track = -1;
        std::unique_ptr<SongReorder> reorder;
        int *draw_modes = nullptr;
    };
    zt_module *owner_ = nullptr;
    unsigned generation_ = 0;
    int restored_track_ = -1;
    std::deque<Entry> undo_, redo_;

    struct Lock {
        zt_mutex_handle mutex;
        bool held;
        explicit Lock(zt_module *song) : mutex(song->hEditMutex),
            held(zt_mutex_lock(mutex, 800) != 0) {}
        ~Lock() { if (held) zt_mutex_unlock(mutex); }
    };

    void bind(zt_module *song) {
        if (owner_ != song) { clear(); owner_ = song; }
    }
    static bool capture(zt_module *song, int index, Snapshot &out) {
        if (!song || index < 0 || index >= ZTM_MAX_PATTERNS) return false;
        Lock lock(song);
        if (!lock.held) return false;
        out.index = index;
        pattern *pat = song->patterns[index];
        if (!pat) return true;
        out.present = true;
        out.length = pat->length;
        for (int t = 0; t < MAX_TRACKS; ++t) {
            track *trk = pat->tracks[t];
            out.tracks[t] = trk != nullptr;
            if (!trk) continue;
            for (event *e = trk->event_list; e; e = e->next_event)
                out.notes.push_back({t, e->row, e->length, e->effect_data,
                                     e->note, e->inst, e->vol, e->effect});
        }
        // Linked-list insertion order is immaterial to the pattern contents.
        std::sort(out.notes.begin(), out.notes.end(),
                  [](const Note &a, const Note &b) { return a.fields() < b.fields(); });
        return true;
    }
    static size_t bytes(const Entry &entry) {
        size_t size = sizeof(Entry) + (entry.reorder ? sizeof(SongReorder) : 0);
        for (const auto &change : entry.changes)
            size += sizeof(Change) + sizeof(Note) *
                (change.before.notes.capacity() + change.after.notes.capacity());
        return size;
    }
    void trim() {
        size_t size = 0;
        for (const auto &entry : undo_) size += bytes(entry);
        while (undo_.size() > 1 && (undo_.size() > max_entries || size > max_bytes)) {
            size -= bytes(undo_.front());
            undo_.pop_front();
        }
    }
    int apply(zt_module *song, bool redo) {
        restored_track_ = -1;
        if (!song) return -1;
        bind(song);
        auto &from = redo ? redo_ : undo_;
        auto &to = redo ? undo_ : redo_;
        if (from.empty()) return -1;
        const Entry &entry = from.back();
        if (entry.reorder) {
            SongReorder reorder = redo ? *entry.reorder : entry.reorder->inverse();
            if (!apply_song_reorder(song, reorder, entry.draw_modes)) return -1;
        } else {
            // Allocate replacements before taking the playback edit mutex. Keep
            // existing pattern/track objects and their playback defaults intact.
            std::vector<std::unique_ptr<pattern>> prepared;
            for (const auto &change : entry.changes) {
                const Snapshot &state = redo ? change.after : change.before;
                std::unique_ptr<pattern> pat;
                if (state.present) {
                    pat.reset(new pattern(state.length));
                    for (int t = 0; t < MAX_TRACKS; ++t)
                        if (!state.tracks[t]) { delete pat->tracks[t]; pat->tracks[t] = nullptr; }
                    for (const auto &note : state.notes) {
                        event *e = new event;
                        e->row = note.row; e->note = note.note; e->inst = note.inst;
                        e->vol = note.vol; e->length = note.length;
                        e->effect = note.effect; e->effect_data = note.effect_data;
                        track *trk = pat->tracks[note.track];
                        e->next_event = trk->event_list;
                        trk->event_list = e;
                    }
                }
                prepared.push_back(std::move(pat));
            }
            {
                Lock lock(song);
                if (!lock.held) return -1;
                for (size_t i = 0; i < entry.changes.size(); ++i) {
                    const Snapshot &state = redo ? entry.changes[i].after : entry.changes[i].before;
                    pattern *&target = song->patterns[state.index];
                    pattern *replacement = prepared[i].get();
                    if (!target || !replacement) {
                        std::swap(target, replacement);
                        prepared[i].release();
                        prepared[i].reset(replacement);
                        continue;
                    }
                    target->length = state.length;
                    for (int t = 0; t < MAX_TRACKS; ++t) {
                        track *old_track = target->tracks[t], *new_track = replacement->tracks[t];
                        if (!old_track || !new_track) {
                            std::swap(target->tracks[t], replacement->tracks[t]);
                        } else {
                            old_track->length = state.length;
                            std::swap(old_track->event_list, new_track->event_list);
                        }
                    }
                }
            }
        }
        int focus = redo ? entry.after_focus : entry.before_focus;
        restored_track_ = redo ? entry.after_track : entry.before_track;
        to.push_back(std::move(from.back()));
        from.pop_back();
        ++generation_; // Invalidates any transaction spanning an undo/redo.
        return focus;
    }

public:
    static constexpr size_t max_entries = 128;
    static constexpr size_t max_bytes = 32 * 1024 * 1024;

    // Reorder records contain a permutation, not copies of song events.
    bool reorder(zt_module *song, const SongReorder &reorder, int before_pattern,
                 int after_pattern, int before_track, int after_track, int *draw_modes = nullptr) {
        if (!song || !reorder.valid() || reorder.identity()) return false;
        bind(song);
        Entry entry;
        entry.reorder.reset(new SongReorder(reorder));
        entry.before_focus = before_pattern; entry.after_focus = after_pattern;
        entry.before_track = before_track; entry.after_track = after_track;
        entry.draw_modes = draw_modes;
        undo_.push_back(std::move(entry));
        if (!apply_song_reorder(song, reorder, draw_modes)) { undo_.pop_back(); return false; }
        redo_.clear();
        ++generation_;
        trim();
        return true;
    }
    int restored_track() const { return restored_track_; }
    bool next_is_reorder(bool redo) const {
        const auto &entries = redo ? redo_ : undo_;
        return !entries.empty() && entries.back().reorder != nullptr;
    }

    void clear(zt_module *song = nullptr) {
        if (song && owner_ != song) return;
        undo_.clear(); redo_.clear(); owner_ = nullptr; restored_track_ = -1; ++generation_;
    }
    int undo(zt_module *song) { return apply(song, false); }
    int redo(zt_module *song) { return apply(song, true); }
    size_t undo_count() const { return undo_.size(); }
    size_t redo_count() const { return redo_.size(); }

    class Edit {
        PatternUndo &history_;
        zt_module *song_;
        unsigned generation_;
        std::vector<Snapshot> before_;
        int before_focus_ = -1, after_focus_ = -1, group_ = 0;
        bool merge_ = false, valid_ = true;
    public:
        Edit(PatternUndo &history, zt_module *song) : history_(history), song_(song) {
            history_.bind(song);
            generation_ = history_.generation_;
        }
        Edit(const Edit &) = delete;
        Edit &operator=(const Edit &) = delete;
        void watch(int index) {
            if (!valid_) return;
            for (const auto &state : before_) if (state.index == index) return;
            Snapshot state;
            if (!capture(song_, index, state)) { valid_ = false; return; }
            before_.push_back(std::move(state));
            if (before_focus_ < 0) before_focus_ = after_focus_ = index;
        }
        bool ready() const { return valid_ && history_.owner_ == song_ && history_.generation_ == generation_; }
        void focus(int before, int after) { before_focus_ = before; after_focus_ = after; }
        void group(int group, bool merge) {
            // A MIDI burst can contain several writes in one frame. Keep
            // the first write's decision about whether this starts a run.
            if (group_ == group) return;
            group_ = group; merge_ = merge;
            // A new stroke may start on an unchanged cell. It still needs
            // a boundary so later frames cannot merge into an older stroke.
            if (!merge && !history_.undo_.empty()) history_.undo_.back().group = 0;
        }
        ~Edit() {
            if (!valid_ || history_.owner_ != song_ || history_.generation_ != generation_) return;
            Entry entry;
            entry.before_focus = before_focus_; entry.after_focus = after_focus_; entry.group = group_;
            for (auto &before : before_) {
                Snapshot after;
                if (!capture(song_, before.index, after)) return;
                if (!(before == after))
                    entry.changes.push_back({std::move(before), std::move(after)});
            }
            if (entry.changes.empty()) return; // Navigation/copy/no-op preserves redo.
            history_.redo_.clear();
            bool combine = merge_ && group_ != 0 && !history_.undo_.empty();
            if (combine) {
                const Entry &previous = history_.undo_.back();
                combine = previous.group == group_ && previous.changes.size() == entry.changes.size();
                for (size_t i = 0; combine && i < entry.changes.size(); ++i)
                    combine = previous.changes[i].after == entry.changes[i].before;
            }
            if (combine) {
                Entry &previous = history_.undo_.back();
                for (size_t i = 0; i < entry.changes.size(); ++i)
                    previous.changes[i].after = std::move(entry.changes[i].after);
                previous.after_focus = after_focus_;
            } else {
                history_.undo_.push_back(std::move(entry));
            }
            history_.trim();
        }
    };
};

// Shared by the editor, its popups and the song lifecycle. de_init() clears
// this before freeing the old song, including load, import and New Song.
inline PatternUndo g_pattern_undo;
#endif
