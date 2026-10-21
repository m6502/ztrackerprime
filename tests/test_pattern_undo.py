#!/usr/bin/env python3
"""Exercise history against production event/track/pattern and teardown bodies."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import ROOT, function

source = r'''
#include "CDataBuf.h"
#include "platform/undo.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
// Reproduce Windows headers' function-like macros at the include site.
#define min(a, b) windows_min_macro_must_not_expand(a, b)
#define max(a, b) windows_max_macro_must_not_expand(a, b)
#include "block_transform.h"
#undef min
#undef max
int file_changed = 0;
zt_module *song = nullptr;
struct { int pattern_length = 64; } zt_config_globals;
constexpr int EDIT_LOCK_TIMEOUT = 800;
bool lock_available = true;
int locks = 0;
int zt_mutex_lock(zt_mutex_handle, int) {
    if (!lock_available) return 0;
    assert(locks == 0); ++locks; return 1;
}
int zt_mutex_unlock(zt_mutex_handle) { assert(locks == 1); --locks; return 1; }
int lock_mutex(zt_mutex_handle m, int t) { return zt_mutex_lock(m, t); }
int unlock_mutex(zt_mutex_handle m) { return zt_mutex_unlock(m); }
// Use production teardown, including ownership of the notes buffer.
zt_module::zt_module(int, int)
    : songmessage(nullptr), position_notes(new CDataBuf), hEditMutex(nullptr) {
    position_notes->pushc('\n');
    for (auto &p : patterns) p = nullptr;
    for (auto &i : instruments) i = nullptr;
    for (auto &a : arpeggios) a = nullptr;
    for (auto &m : midimacros) m = nullptr;
}
zt_module::~zt_module() { de_init(); }
instrument::~instrument() {}
songmsg::~songmsg() {}
arpeggio::~arpeggio() {}
midimacro::~midimacro() {}
'''
# Keep real buffer allocation/destruction without the SDL-dependent methods.
for signature in [
    'CDataBuf::CDataBuf()', 'CDataBuf::~CDataBuf()',
    'void CDataBuf::pushc(const char c)', 'void CDataBuf::flush(void)',
]:
    source += function('src/CDataBuf.cpp', signature)
for signature in [
    'event::event(void)', 'event::event(event *e)', 'event::~event(void)', 'int event::blank(void)',
    'track::track(short int len)', 'track::~track(void)', 'void track::reset(void)',
    'void track::del_event(', 'void track::ins_row(', 'void track::del_row(',
    'void track::update_event(unsigned short int row, int note,',
    'event* track::get_event(', 'event* track::get_next_event(',
    'pattern::pattern(void)', 'pattern::pattern(int len)', 'pattern::~pattern(void)',
    'void pattern::resize(', 'int pattern::isempty(', 'void zt_module::de_init(void)',
]:
    source += function('src/module.cpp', signature)
source += r'''
void put(int pattern, int track, int row, int note) {
    song->patterns[pattern]->tracks[track]->update_event(row, note, 3, 100, 48, 'S', 0x4aff);
}
event *get(int pattern, int track, int row) {
    return song->patterns[pattern]->tracks[track]->get_event(row);
}
void check(int pattern, int track, int row, int note) {
    event *e = get(pattern, track, row);
    assert(e && e->note == note && e->inst == 3 && e->vol == 100 &&
           e->length == 48 && e->effect == 'S' && e->effect_data == 0x4aff);
}
int main() {
    zt_module fixture(8, 120), other(8, 120);
    song = &fixture;
    fixture.patterns[0] = new pattern(64);
    fixture.patterns[1] = new pattern(32);
    other.patterns[0] = new pattern(64);
    auto &history = g_pattern_undo;
    assert(history.undo(song) == -1 && history.redo(song) == -1);

    { PatternUndo::Edit edit(history, song); edit.watch(0); put(0, 0, 0, 60); }
    { PatternUndo::Edit edit(history, song); edit.watch(0); put(0, 0, 1, 62); }
    assert(history.undo_count() == 2);
    // Removing newly inserted notes must clear their allocations, too.
    assert(history.undo(song) == 0 && !get(0, 0, 1)); check(0, 0, 0, 60);
    assert(history.undo(song) == 0 && !get(0, 0, 0));
    assert(history.undo(song) == -1);
    assert(history.redo(song) == 0); check(0, 0, 0, 60);
    assert(history.redo(song) == 0); check(0, 0, 1, 62);
    assert(history.redo(song) == -1);

    // Multi-track deletion is atomic; every field survives the round trip.
    put(0, 63, 63, 127);
    { PatternUndo::Edit edit(history, song); edit.watch(0);
      for (int t : {0, 63}) for (int r : {0, 1, 63})
          fixture.patterns[0]->tracks[t]->del_event(r); }
    assert(!get(0, 0, 0) && !get(0, 63, 63));
    history.undo(song); check(0, 0, 0, 60); check(0, 0, 1, 62); check(0, 63, 63, 127);
    // Navigation/copy and rewriting identical data don't consume undo or redo.
    size_t count = history.undo_count();
    { PatternUndo::Edit edit(history, song); edit.watch(0); put(0, 0, 0, 60); }
    assert(history.undo_count() == count && history.redo_count() == 1);
    history.redo(song); assert(!get(0, 0, 0) && !get(0, 63, 63));
    history.undo(song);
    { PatternUndo::Edit edit(history, song); edit.watch(0); get(0, 0, 0)->note = 65; }
    assert(history.redo(song) == -1); // New edits invalidate the old branch.
    history.undo(song); check(0, 0, 0, 60);
    history.redo(song); assert(get(0, 0, 0)->note == 65);

    // Shrinking restores notes deleted beyond the new length; growing again
    // via redo does not leak notes from a previous incarnation of the pattern.
    { PatternUndo::Edit edit(history, song); edit.watch(0); fixture.patterns[0]->resize(32); }
    assert(fixture.patterns[0]->length == 32 && !get(0, 63, 63));
    history.undo(song); assert(fixture.patterns[0]->length == 64); check(0, 63, 63, 127);
    history.redo(song); assert(fixture.patterns[0]->length == 32 && !get(0, 63, 63));
    history.undo(song);
    auto *original_track = fixture.patterns[0]->tracks[0];
    original_track->default_inst = 17;
    { PatternUndo::Edit edit(history, song); edit.watch(0); original_track->ins_row(0); }
    history.undo(song); assert(fixture.patterns[0]->tracks[0] == original_track);
    assert(original_track->default_inst == 17 && get(0, 0, 0)->note == 65);
    history.redo(song); assert(!get(0, 0, 0) && get(0, 0, 1)->note == 65);
    { PatternUndo::Edit edit(history, song); edit.watch(0); original_track->del_row(0); }
    history.undo(song); assert(!get(0, 0, 0) && get(0, 0, 1)->note == 65);

    // Multiple patterns form one edit, with independent pre/post focus.
    { PatternUndo::Edit edit(history, song); edit.watch(0); edit.watch(1); edit.focus(0, 1);
      put(0, 2, 5, 50); fixture.patterns[1]->resize(96); put(1, 4, 90, 70); }
    assert(history.undo(song) == 0 && fixture.patterns[1]->length == 32);
    assert(!get(0, 2, 5) && !get(1, 4, 90));
    assert(history.redo(song) == 1 && fixture.patterns[1]->length == 96);
    check(0, 2, 5, 50); check(1, 4, 90, 70);
    // Clone into a previously absent slot can also be undone/redone.
    { PatternUndo::Edit edit(history, song); edit.watch(2); edit.focus(0, 2);
      fixture.patterns[2] = new pattern(64); put(2, 0, 0, 42); }
    assert(history.undo(song) == 0 && !fixture.patterns[2]);
    assert(history.redo(song) == 2); check(2, 0, 0, 42);

    history.clear();
    // One gesture, many frames; a second gesture and an intervening edit
    // are separate history entries even if they use the same drawing mode.
    for (int r = 0; r < 10; ++r) {
        PatternUndo::Edit edit(history, song); edit.watch(1); edit.group(1, r != 0); put(1, 0, r, 40);
    }
    assert(history.undo_count() == 1);
    history.undo(song); for (int r = 0; r < 10; ++r) assert(!get(1, 0, r));
    history.redo(song); for (int r = 0; r < 10; ++r) check(1, 0, r, 40);
    { PatternUndo::Edit edit(history, song); edit.watch(1); edit.group(1, false); put(1, 0, 0, 40); }
    { PatternUndo::Edit edit(history, song); edit.watch(1); edit.group(1, true); put(1, 0, 9, 41); }
    assert(history.undo_count() == 2);
    history.undo(song); check(1, 0, 9, 40);
    { PatternUndo::Edit edit(history, song); edit.watch(1); edit.group(1, false);
      edit.group(1, true); put(1, 0, 10, 41); }
    { PatternUndo::Edit edit(history, song); edit.watch(1); put(1, 0, 11, 42); }
    { PatternUndo::Edit edit(history, song); edit.watch(1); edit.group(1, true); put(1, 0, 12, 43); }
    assert(history.undo_count() == 4);

    // A failed mutex acquisition must neither mutate data nor consume history.
    lock_available = false;
    assert(history.undo(song) == -1 && history.undo_count() == 4);
    lock_available = true; assert(history.undo(song) == 1 && !get(1, 0, 12));
    history.clear();
    for (size_t n = 0; n < PatternUndo::max_entries + 10; ++n) {
        PatternUndo::Edit edit(history, song); edit.watch(1); put(1, 0, 0, n % 128);
    }
    assert(history.undo_count() == PatternUndo::max_entries);
    for (size_t n = 0; n < PatternUndo::max_entries; ++n) assert(history.undo(song) == 1);
    assert(history.undo(song) == -1 && history.redo_count() == PatternUndo::max_entries);

    // Production teardown clears both histories even when the module address
    // is reused by a new song. Also invalidate a transaction across teardown.
    { PatternUndo::Edit edit(history, song); edit.watch(0);
      fixture.de_init(); fixture.patterns[0] = new pattern(64); }
    assert(fixture.position_notes == nullptr);
    assert(history.undo_count() == 0 && history.redo_count() == 0);
    assert(history.undo(song) == -1 && history.redo(song) == -1);
    { PatternUndo::Edit edit(history, song); edit.watch(0); put(0, 0, 0, 60); }
    song = &other;
    assert(history.undo(song) == -1 && history.redo(song) == -1 && !get(0, 0, 0));
    // Tearing down an unrelated module must not erase the active song's history.
    { PatternUndo::Edit edit(history, song); edit.watch(0); put(0, 0, 0, 70); }
    fixture.de_init(); assert(history.undo_count() == 1);
    history.undo(song); assert(!get(0, 0, 0));
    // Block transforms overwrite only their destination on selected tracks,
    // keep the shared pattern length, and restore every field through undo.
    zt_module blocks(8, 120);
    song = &blocks;
    blocks.patterns[0] = new pattern(64);
    for (int t : {4, 5, 6})
        for (int r : {7, 8, 9, 10, 11, 12, 14, 20, 61, 62, 63})
            put(0, t, r, 30 + r);
    history.clear();
    auto transform = [&](BlockRange &range, BlockTransform operation) {
        PatternUndo::Edit edit(history, song); edit.watch(0);
        return transform_block(song, 0, range, operation);
    };
    BlockRange range{4, 5, 9, 11};
    assert(transform(range, BlockTransform::Expand) == BlockTransformResult::Changed);
    assert(range.first_row == 9 && range.last_row == 14);
    for (int t : {4, 5}) {
        check(0, t, 8, 38); check(0, t, 9, 39); check(0, t, 11, 40); check(0, t, 13, 41);
        for (int r : {10, 12, 14}) assert(!get(0, t, r));
        check(0, t, 20, 50); check(0, t, 63, 93);
    }
    check(0, 6, 10, 40); check(0, 6, 14, 44);
    assert(transform(range, BlockTransform::Halve) == BlockTransformResult::Changed);
    assert(range.last_row == 11);
    check(0, 4, 9, 39); check(0, 4, 10, 40); check(0, 4, 11, 41);
    assert(!get(0, 4, 12) && !get(0, 4, 14));
    history.undo(song); assert(!get(0, 4, 10));
    history.undo(song); check(0, 4, 12, 42); check(0, 4, 14, 44);
    history.redo(song); check(0, 4, 13, 41);
    history.undo(song);

    range = {4, 5, 9, 11};
    assert(transform(range, BlockTransform::Duplicate) == BlockTransformResult::Changed);
    assert(range.last_row == 14);
    for (int t : {4, 5}) {
        check(0, t, 9, 39); check(0, t, 10, 40); check(0, t, 11, 41);
        check(0, t, 12, 39); check(0, t, 13, 40); check(0, t, 14, 41);
        check(0, t, 20, 50); check(0, t, 63, 93);
    }
    check(0, 6, 12, 42);
    history.undo(song); check(0, 4, 12, 42);
    range = {4, 5, 9, 11};
    assert(transform(range, BlockTransform::Halve) == BlockTransformResult::Changed);
    assert(range.last_row == 10); // Keep offsets 0 and 2 of an odd-sized block.
    check(0, 4, 9, 39); check(0, 4, 10, 41);
    assert(!get(0, 4, 11)); check(0, 4, 12, 42); check(0, 4, 20, 50);
    history.undo(song); check(0, 4, 10, 40); check(0, 4, 11, 41);

    range = {4, 4, 62, 63};
    assert(transform(range, BlockTransform::Expand) == BlockTransformResult::Changed);
    assert(range.last_row == 63);
    check(0, 4, 61, 91); check(0, 4, 62, 92); assert(!get(0, 4, 63));
    assert(!get(0, 4, 64) && blocks.patterns[0]->length == 64);
    check(0, 5, 63, 93);
    history.undo(song); check(0, 4, 63, 93);
    auto undo_count = history.undo_count();
    range = {4, 4, 62, 63};
    transform(range, BlockTransform::Duplicate); // Copy falls entirely beyond the end.
    assert(history.undo_count() == undo_count); check(0, 4, 63, 93);
    range = {4, 4, 61, 62};
    transform(range, BlockTransform::Duplicate); // Only the first copied row fits.
    assert(range.last_row == 63 && blocks.patterns[0]->length == 64);
    check(0, 4, 61, 91); check(0, 4, 62, 92); check(0, 4, 63, 91);
    assert(!get(0, 4, 64)); check(0, 5, 63, 93);
    history.undo(song); check(0, 4, 63, 93);
    range = {4, 4, 9, 9};
    assert(transform(range, BlockTransform::Halve) == BlockTransformResult::Unchanged);
    range = {0, MAX_TRACKS - 1, 0, 63};
    transform(range, BlockTransform::Halve);
    assert(range.last_row == 31 && blocks.patterns[0]->length == 64);
    for (track *trk : blocks.patterns[0]->tracks) assert(trk->length == 64);
    check(0, 4, 31, 92); assert(!get(0, 4, 32));
    history.undo(song);
    range = {0, 0, 0, 64};
    assert(transform(range, BlockTransform::Expand) == BlockTransformResult::InvalidSelection);
    range = {0, 0, 0, 63};
    lock_available = false;
    assert(transform(range, BlockTransform::Expand) == BlockTransformResult::LockTimeout);
    lock_available = true;
    // Song tools use reversible permutations; repeated orders and references
    // after blank/skip markers keep their musical meaning.
    zt_module tools(8, 120);
    song = &tools;
    std::array<pattern *, ZTM_MAX_PATTERNS> original_patterns;
    for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) {
        tools.patterns[p] = new pattern(32 + (p % 3) * 32);
        original_patterns[p] = tools.patterns[p];
        put(p, 0, 0, p % 128);
        put(p, 63, tools.patterns[p]->length - 1, (p + 1) % 128);
    }
    for (int &entry : tools.orderlist) entry = 0x100;
    int sequence[] = {1, 4, 3, 7, 3, 7, 9};
    for (int i = 0; i < 7; ++i) tools.orderlist[i] = sequence[i];
    tools.orderlist[80] = 0x101; tools.orderlist[100] = 42;
    auto original_orders = std::array<int, ZTM_ORDERLIST_LEN>{};
    std::copy(std::begin(tools.orderlist), std::end(tools.orderlist), original_orders.begin());
    for (int t = 0; t < MAX_TRACKS; ++t) {
        std::snprintf(tools.track_name[t], ZTM_TRACKNAME_MAXLEN, "Track %d", t);
        tools.track_color[t] = 1000 + t; tools.track_mute[t] = t % 2;
    }
    int draw_modes[MAX_TRACKS]; for (int t = 0; t < MAX_TRACKS; ++t) draw_modes[t] = t;
    history.clear();
    g_song_tools_selection.sync(song, 1);
    g_song_tools_selection.source_pattern = tools.patterns[7];
    auto defrag = song_defrag_order(tools);
    assert(defrag.valid() && defrag.destination[1] == 0 && defrag.destination[4] == 1 &&
           defrag.destination[3] == 2 && defrag.destination[7] == 3 && defrag.destination[9] == 4 &&
           defrag.destination[42] == 5 && defrag.destination[0] == 6 && defrag.destination[2] == 7);
    assert(history.reorder(song, defrag, 7, 3, 0, 0));
    assert(history.next_is_reorder(false) && g_song_tools_selection.pattern_index(song) == 3);
    for (int p = 0; p < ZTM_MAX_PATTERNS; ++p)
        assert(tools.patterns[defrag.destination[p]] == original_patterns[p]);
    for (int i = 0; i < ZTM_ORDERLIST_LEN; ++i) {
        if (original_orders[i] < 256) assert(tools.patterns[tools.orderlist[i]] == original_patterns[original_orders[i]]);
        else assert(tools.orderlist[i] == original_orders[i]);
    }
    assert(song_defrag_order(tools).identity());
    assert(history.undo(song) == 7 && g_song_tools_selection.pattern_index(song) == 7);
    for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) assert(tools.patterns[p] == original_patterns[p]);
    assert(std::equal(original_orders.begin(), original_orders.end(), tools.orderlist));
    assert(history.redo(song) == 3); history.undo(song);
    for (bool swap : {false, true}) for (auto direction : {std::pair<int,int>{1,7}, {7,1}}) {
        auto reorder = make_song_reorder(SongReorderKind::Patterns, direction.first, direction.second, swap);
        assert(history.reorder(song, reorder, direction.second, direction.second, 0, 0));
        assert(tools.patterns[direction.second] == original_patterns[direction.first]);
        for (int i = 0; i < ZTM_ORDERLIST_LEN; ++i)
            if (original_orders[i] < 256) assert(tools.patterns[tools.orderlist[i]] == original_patterns[original_orders[i]]);
        history.undo(song);
        for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) assert(tools.patterns[p] == original_patterns[p]);
    }
    history.clear();
    auto used = song_used_patterns(tools);
    assert(used[42] && !used[0] && !used[2]);
    { PatternUndo::Edit edit(history, song); edit.focus(0,0);
      for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) if (!used[p]) edit.watch(p);
      assert(edit.ready() && clear_song_unused_patterns(song) == 250); }
    assert(!song_pattern_has_events(tools.patterns[0]) && song_pattern_has_events(tools.patterns[42]));
    assert(tools.patterns[0]->length == 32 && std::equal(original_orders.begin(), original_orders.end(), tools.orderlist));
    history.undo(song); check(0,0,0,0); check(42,0,0,42);
    history.redo(song); assert(!song_pattern_has_events(tools.patterns[0]));
    history.undo(song);

    // Track data moves independently per pattern; song-wide metadata follows
    // only for whole-song operations. Unequal pattern lengths stay intact.
    std::array<std::array<track *, MAX_TRACKS>, ZTM_MAX_PATTERNS> tracks_before;
    for (int p = 0; p < ZTM_MAX_PATTERNS; ++p)
        for (int t = 0; t < MAX_TRACKS; ++t) tracks_before[p][t] = tools.patterns[p]->tracks[t];
    for (int scope : {0, -1}) for (bool swap : {false,true})
        for (auto direction : {std::pair<int,int>{0,63}, {63,0}}) {
            history.clear();
            auto reorder = make_song_reorder(SongReorderKind::Tracks, direction.first, direction.second, swap, scope);
            assert(history.reorder(song, reorder, 0,0,direction.first,direction.second,draw_modes));
            for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) for (int t = 0; t < MAX_TRACKS; ++t) {
                int destination = scope < 0 || scope == p ? reorder.destination[t] : t;
                assert(tools.patterns[p]->tracks[destination] == tracks_before[p][t]);
                assert(tools.patterns[p]->tracks[destination]->length == tools.patterns[p]->length);
            }
            for (int t = 0; t < MAX_TRACKS; ++t) {
                int target = scope < 0 ? reorder.destination[t] : t;
                assert(tools.track_color[target] == static_cast<unsigned long>(1000+t));
                assert(tools.track_mute[target] == t%2 && draw_modes[target] == t);
                char expected[16]; std::snprintf(expected,sizeof(expected),"Track %d",t);
                assert(std::strcmp(tools.track_name[target],expected)==0);
            }
            assert(history.undo(song) == 0 && history.restored_track() == direction.first);
            for (int p = 0; p < ZTM_MAX_PATTERNS; ++p) for (int t = 0; t < MAX_TRACKS; ++t)
                assert(tools.patterns[p]->tracks[t] == tracks_before[p][t]);
            assert(history.redo(song) == 0 && history.restored_track() == direction.second);
            history.undo(song);
        }
    auto invalid = make_song_reorder(SongReorderKind::Tracks, 0,1,true);
    invalid.destination[1] = invalid.destination[0];
    assert(!apply_song_reorder(song, invalid));
    auto unchanged_reorder = make_song_reorder(SongReorderKind::Patterns, 3,3,false);
    assert(!history.reorder(song, unchanged_reorder,0,0,0,0));
    lock_available = false;
    assert(!history.reorder(song, make_song_reorder(SongReorderKind::Patterns,0,255,true),0,0,0,0));
    lock_available = true;
    g_song_tools_selection.source_track = 12;
    g_song_tools_selection.sync(song,2);
    assert(!g_song_tools_selection.source_pattern && g_song_tools_selection.source_track == -1);
    assert(locks == 0);
    puts("Pattern undo: data restoration, redo branches, gestures, bounds and song teardown passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='zt-pattern-undo-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    flags = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror=delete-incomplete', '-I' + str(ROOT / 'src')]
    if '--sanitize' in sys.argv:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + flags +
                   [str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
