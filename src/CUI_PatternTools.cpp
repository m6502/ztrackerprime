#include "zt.h"
#include "Button.h"
#include "PatternDisplay.h"
#include "platform/undo.h"
#include "song_tools.h"
#include <cstdio>

namespace {
constexpr int tools_width = 64, tools_height = 47;
enum Action { Defrag, ClearUnused, MarkPattern, MovePattern, SwapPattern,
              TrackScope, MarkTrack, MoveTrack, SwapTrack, Close };

void reordered(const SongReorder &reorder) {
    if (reorder.kind == SongReorderKind::Patterns) {
        auto remap = [&](int &index) {
            if (index >= 0 && index < ZTM_MAX_PATTERNS) index = reorder.destination[index];
        };
        remap(cur_edit_pattern);
        remap(sel_pat);
        if (ztPlayer) {
            remap(ztPlayer->cur_pattern);
            remap(ztPlayer->playing_cur_pattern);
        }
        if (UIP_Playsong && UIP_Playsong->pattern_display)
            remap(UIP_Playsong->pattern_display->disp_pattern);
    }
    g_cc_draw_session_snapped = 0;
    if (UIP_Patterneditor) UIP_Patterneditor->clear = 1;
}

class ToolsButton : public Button {
public:
    bool enabled = true;
    bool is_tab_stop() const override { return enabled; }
    int update() override { return enabled ? Button::update() : 0; }
    int mouseupdate(int current) override { return enabled ? Button::mouseupdate(current) : current; }
    void draw(Drawable *surface, int active) override {
        Button::draw(surface, enabled ? active : 0);
        if (!enabled) printBG(col(x), row(y), caption, COLORS.Lowlight, COLORS.Background, surface);
    }
};

void tools_clicked(UserInterfaceElement *button) { UIP_PatternTools->activate(button->ID); }

void box(Drawable *surface, int x, int y, int width, int height, const char *title) {
    printline(col(x), row(y), 0x86, width, COLORS.Highlight, surface);
    printline(col(x), row(y + height), 0x81, width, COLORS.Lowlight, surface);
    for (int cy = y + 1; cy < y + height; ++cy) {
        printchar(col(x - 1), row(cy), 0x84, COLORS.Highlight, surface);
        printchar(col(x + width), row(cy), 0x83, COLORS.Lowlight, surface);
    }
    printBG(col(x + 2), row(y), title, COLORS.Text, COLORS.Background, surface);
}

void finish_tools_edit() {
    selected = 0;
    g_cc_draw_session_snapped = 0;
    if (UIP_Patterneditor) UIP_Patterneditor->clear = 1;
    if (song && song->patterns[cur_edit_pattern] &&
        cur_edit_row >= song->patterns[cur_edit_pattern]->length)
        cur_edit_row = song->patterns[cur_edit_pattern]->length - 1;
    file_changed++;
    need_refresh++; need_popup_refresh++; status_change = 1;
}
} // namespace

CUI_PatternTools::CUI_PatternTools() {
    g_song_reorder_observer = reordered;
    UI = new UserInterface;
    const char *captions[] = {
        " Defrag song", " Clear unused patterns",
        " Select current pattern for move or swap",
        " Move pattern into this position", " Swap pattern with current position",
        " Scope: Current pattern  (click to change)",
        " Select current track for move or swap",
        " Move track into this position", " Swap track with current position", " Close"
    };
    for (int i = 0; i < 10; ++i) {
        buttons[i] = new ToolsButton;
        buttons[i]->caption = captions[i];
        buttons[i]->OnClick = tools_clicked;
        buttons[i]->ysize = 1;
        UI->add_element(buttons[i], i);
    }
    layout();
}

void CUI_PatternTools::layout() {
    const int x = (CHARS_X - tools_width) / 2;
    const int y = (CHARS_Y - tools_height) / 2;
    const int rows[] = {6, 9, 17, 20, 23, 30, 33, 36, 39, 45};
    for (int i = 0; i < 10; ++i) {
        buttons[i]->x = x + 4;
        buttons[i]->y = y + rows[i];
        buttons[i]->xsize = tools_width - 8;
    }
    buttons[Close]->x = x + tools_width / 2 - 5;
    buttons[Close]->xsize = 10;
}

void CUI_PatternTools::refresh_buttons() {
    g_song_tools_selection.sync(song, zt_song_generation);
    if (context_generation != zt_song_generation) {
        context_generation = zt_song_generation;
        whole_song = 0;
        message[0] = '\0';
    }
    const int source_pattern = g_song_tools_selection.pattern_index(song);
    const int source_track = g_song_tools_selection.source_track;
    for (int i : {MovePattern, SwapPattern})
        static_cast<ToolsButton *>(buttons[i])->enabled = source_pattern >= 0 && source_pattern != cur_edit_pattern;
    for (int i : {MoveTrack, SwapTrack})
        static_cast<ToolsButton *>(buttons[i])->enabled = source_track >= 0 && source_track != cur_edit_track;
    buttons[TrackScope]->caption = whole_song ? " Scope: Whole song       (click to change)" :
                                              " Scope: Current pattern  (click to change)";
    auto *focused = static_cast<ToolsButton *>(buttons[UI->cur_element]);
    if (!focused->enabled) UI->cur_element = Close;
}

void CUI_PatternTools::enter() {
    if (UIP_Patterneditor) UIP_Patterneditor->mousedrawing = 0;
    midiInQueue.clear();
    g_cc_draw_session_snapped = 0;
    layout();
    UI->cur_element = Defrag;
    refresh_buttons();
    UI->full_refresh();
    need_refresh++;
}
void CUI_PatternTools::leave() {
    midiInQueue.clear();
    g_cc_draw_session_snapped = 0;
}

void CUI_PatternTools::activate(int action) {
    refresh_buttons();
    if (action < 0 || action >= 10 || !static_cast<ToolsButton *>(buttons[action])->enabled) return;
    if (action == Close) {
        Keys.flush(); close_popup_window(); fixmouse++; need_refresh++;
        return;
    }
    if (action == MarkPattern) {
        g_song_tools_selection.source_pattern = song->patterns[cur_edit_pattern];
        std::snprintf(message, sizeof(message), "Pattern %03d selected. Close, then choose a destination.", cur_edit_pattern);
    } else if (action == MarkTrack) {
        g_song_tools_selection.source_track = cur_edit_track;
        std::snprintf(message, sizeof(message), "Track %02d selected. Close, then choose a destination.", cur_edit_track + 1);
    } else if (action == TrackScope) {
        whole_song = !whole_song;
    } else {
        if (ztPlayer->playing) ztPlayer->stop();
        if (action == ClearUnused) {
            PatternUndo::Edit edit(g_pattern_undo, song);
            edit.focus(cur_edit_pattern, cur_edit_pattern);
            auto used = song_used_patterns(*song);
            for (int p = 0; p < ZTM_MAX_PATTERNS; ++p)
                if (!used[p] && song_pattern_has_events(song->patterns[p])) edit.watch(p);
            int count = edit.ready() ? clear_song_unused_patterns(song) : -1;
            if (count >= 0) {
                std::snprintf(message, sizeof(message), "Cleared %d unused pattern%s.", count, count == 1 ? "" : "s");
                if (count) finish_tools_edit();
            } else std::snprintf(message, sizeof(message), "Could not acquire the song edit lock.");
        } else {
            SongReorder reorder;
            int after_pattern = cur_edit_pattern, after_track = cur_edit_track;
            if (action == Defrag) {
                reorder = song_defrag_order(*song);
                after_pattern = reorder.destination[cur_edit_pattern];
            } else if (action == MovePattern || action == SwapPattern) {
                reorder = make_song_reorder(SongReorderKind::Patterns,
                    g_song_tools_selection.pattern_index(song), cur_edit_pattern, action == SwapPattern);
            } else {
                reorder = make_song_reorder(SongReorderKind::Tracks,
                    g_song_tools_selection.source_track, cur_edit_track, action == SwapTrack,
                    whole_song ? -1 : cur_edit_pattern);
            }
            if (reorder.identity()) std::snprintf(message, sizeof(message), "Already in the requested order.");
            else if (g_pattern_undo.reorder(song, reorder, cur_edit_pattern, after_pattern,
                                           cur_edit_track, after_track, g_cc_drawmode)) {
                cur_edit_pattern = after_pattern;
                cur_edit_track = after_track;
                std::snprintf(message, sizeof(message), "%s complete. Ctrl+Z to undo in the editor.",
                              action == Defrag ? "Defrag" :
                              action == MovePattern || action == MoveTrack ? "Move" : "Swap");
                finish_tools_edit();
            } else std::snprintf(message, sizeof(message), "Could not acquire the song edit lock.");
        }
    }
    refresh_buttons();
    UI->full_refresh(); need_refresh++; need_popup_refresh++;
}

void CUI_PatternTools::update() {
    layout(); refresh_buttons();
    KBKey key = Keys.checkkey();
    KBMod modifiers = Keys.getstate();
    if (key == SDLK_ESCAPE || (key == SDLK_F2 && modifiers == (KS_CTRL | KS_SHIFT)) ||
        key == ((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT)) {
        Keys.getkey(); activate(Close); return;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
        Keys.getkey(); activate(UI->cur_element); return;
    }
    UI->update();
    if (Keys.size()) Keys.getkey();
}

void CUI_PatternTools::draw(Drawable *surface) {
    layout(); refresh_buttons();
    if (surface->lock() != 0) return;
    const int x = (CHARS_X - tools_width) / 2;
    const int y = (CHARS_Y - tools_height) / 2;
    surface->fillRect(col(x - 1), row(y), col(x + tools_width + 1) - 1,
                      row(y + tools_height + 1) - 1, COLORS.Background);
    box(surface, x, y, tools_width, tools_height, " Pattern tools ");
    box(surface, x + 2, y + 4, tools_width - 4, 8, " Song cleanup ");
    box(surface, x + 2, y + 13, tools_width - 4, 12, " Move / Swap Patterns ");
    box(surface, x + 2, y + 26, tools_width - 4, 16, " Move / Swap Tracks ");
    char text[96], source[16];
    int pattern_source = g_song_tools_selection.pattern_index(song);
    if (pattern_source >= 0) std::snprintf(source, sizeof(source), "%03d", pattern_source);
    else std::snprintf(source, sizeof(source), "---");
    std::snprintf(text, sizeof(text), "Source pattern: %s     Current position: %03d", source, cur_edit_pattern);
    printBG(col(x + 4), row(y + 15), text, COLORS.Text, COLORS.Background, surface);
    int track_source = g_song_tools_selection.source_track;
    if (track_source >= 0) std::snprintf(source, sizeof(source), "%02d", track_source + 1);
    else std::snprintf(source, sizeof(source), "--");
    std::snprintf(text, sizeof(text), "Source track: %s        Current track: %02d", source, cur_edit_track + 1);
    printBG(col(x + 4), row(y + 28), text, COLORS.Text, COLORS.Background, surface);
    printBG(col(x + 3), row(y + 43), message, COLORS.Text, COLORS.Background, surface);
    UI->full_refresh(); UI->draw(surface);
    surface->unlock();
    screenmanager.UpdateAll(); need_refresh = 0; need_popup_refresh = 0; updated++;
}
