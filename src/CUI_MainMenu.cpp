/*************************************************************************
 *
 * FILE  CUI_MainMenu.cpp
 *
 * DESCRIPTION
 *   The ESC-triggered main menu — a Schism Tracker-style modal that
 *   lists every page / action the user can reach, so they don't have to
 *   memorise the F-key map. Cursor up/down to navigate, Enter to fire.
 *   ESC closes the menu without doing anything.
 *
 *   Implemented as a popup_window() overlay so the underlying page is
 *   preserved — closing the menu via ESC drops you back where you were.
 *
 ******/
#include "zt.h"
#include "Button.h"

// ----------------------------------------------------------------------
// Action kinds. Keep the table at the bottom of this file as the single
// source of truth for ordering.
// ----------------------------------------------------------------------
enum mm_action_kind {
    MM_CMD,        // dispatch a CMD_* via Keys.insert
    MM_FUNC,       // call a void() function
    MM_SEPARATOR,  // non-selectable spacer row
    MM_SUBHEADING  // non-selectable label row (light header inside a section)
};

struct mm_entry {
    mm_action_kind  kind;
    const char     *label;
    const char     *shortcut;   // hint shown right-aligned
    int             command;    // for MM_CMD
    void          (*func)(void); // for MM_FUNC
};

// ----------------------------------------------------------------------
// Externs from main.cpp.
// ----------------------------------------------------------------------
extern bool bScrollLock;
extern int  attempt_fullscreen_toggle(void);
extern void quit(void);

// ----------------------------------------------------------------------
// Functions that don't have a dedicated CMD_* enum. Implemented inline
// so the menu can list everything.
// ----------------------------------------------------------------------
static void mm_toggle_fullscreen(void) {
    (void)attempt_fullscreen_toggle();
}

static void mm_toggle_follow_playback(void) {
    bScrollLock = !bScrollLock;
    sprintf(szStatmsg, "Follow playback %s", bScrollLock ? "ON" : "OFF");
    statusmsg = szStatmsg;
    status_change = 1;
    need_refresh++;
}

static void mm_panic(void) {
    MidiOut->panic();
    sprintf(szStatmsg, "Panic");
    statusmsg = szStatmsg;
    status_change = 1;
    need_refresh++;
}

static void mm_hard_panic(void) {
    MidiOut->hardpanic();
    sprintf(szStatmsg, "Hard Panic");
    statusmsg = szStatmsg;
    status_change = 1;
    need_refresh++;
}

static void mm_quit(void) {
    quit();
}

static void mm_toggle_cc_drawmode(void) {
    // Use the shared toggler in zt.h so the undo-session marker resets
    // too -- a bare g_cc_drawmode = !g_cc_drawmode would skip the reset
    // and leave the next drawmode-on session unsnapped (audit H2).
    zt_toggle_cc_drawmode();
    snprintf(szStatmsg, sizeof(szStatmsg), "CC drawmode: %s",
             g_cc_drawmode ? "ON  (incoming CC writes S/W effects)" : "OFF");
    statusmsg = szStatmsg;
    status_change = 1;
}

// Build a CLI invocation string that re-launches zt with the same
// state the user has now (open MIDI in ports, MIDI clock chase target,
// loaded song file) and copy it to the system clipboard. Pairs with
// the --midi-in / --midi-clock CLI flags so the user can paste the
// command anywhere (Discord, a launcher script, a stage cheat sheet)
// to reproduce this session.
static void mm_copy_relaunch_cmd(void) {
    char buf[1024];
    int n = 0;
    n += snprintf(buf + n, sizeof(buf) - n, "zt");

    // MIDI clock chase: which input port is the master? Use the first
    // open one when chase is on. (Multi-master clock makes no sense.)
    int clock_port_idx = -1;
    if (zt_config_globals.midi_in_sync_chase_tempo && MidiIn) {
        for (unsigned j = 0; j < MidiIn->numMidiDevs; j++) {
            if (MidiIn->QueryDevice(j)) { clock_port_idx = (int)j; break; }
        }
    }
    if (clock_port_idx >= 0) {
        n += snprintf(buf + n, sizeof(buf) - n,
                      " --midi-clock \"%s\"",
                      MidiIn->midiInDev[clock_port_idx]->szName);
    }

    // Every OTHER open MIDI in port becomes a --midi-in flag.
    if (MidiIn) {
        for (unsigned j = 0; j < MidiIn->numMidiDevs; j++) {
            if ((int)j == clock_port_idx) continue;
            if (!MidiIn->QueryDevice(j)) continue;
            n += snprintf(buf + n, sizeof(buf) - n,
                          " --midi-in \"%s\"",
                          MidiIn->midiInDev[j]->szName);
        }
    }

    // Song filename (positional). The codebase stores it as an
    // unsigned char[] that may contain a leading space sentinel for
    // "no name", so guard against that.
    if (song && song->filename[0] != '\0' && song->filename[0] != ' ') {
        n += snprintf(buf + n, sizeof(buf) - n,
                      " %s", (const char *)song->filename);
    }

    SDL_SetClipboardText(buf);
    snprintf(szStatmsg, sizeof(szStatmsg), "Copied to clipboard: %s", buf);
    statusmsg = szStatmsg;
    status_change = 1;
}

// ----------------------------------------------------------------------
// Menu table. Order on screen = order here.
// ----------------------------------------------------------------------
static const mm_entry MM_ENTRIES[] = {
    {MM_SUBHEADING, "Views",                    NULL,                   0,                          NULL},
    {MM_CMD,        "Pattern Editor",           "F2",                   CMD_SWITCH_PEDIT,           NULL},
    {MM_CMD,        "Instrument Editor",        "F3",                   CMD_SWITCH_IEDIT,           NULL},
    {MM_CMD,        "Song Configuration",       "F11",                  CMD_SWITCH_SONGCONF,        NULL},
    {MM_CMD,        "Song Message",             "F10",                  CMD_SWITCH_SONGMSG,         NULL},
    {MM_CMD,        "MIDI Macro Editor",        "F4",                   CMD_SWITCH_MIDIMACEDIT,     NULL},
    {MM_CMD,        "Arpeggio Editor",          "Shift-F4",             CMD_SWITCH_ARPEDIT,         NULL},
    {MM_CMD,        "Help",                     "F1",                   CMD_SWITCH_HELP,            NULL},
    {MM_CMD,        "About",                    "Alt+F12",              CMD_SWITCH_ABOUT,           NULL},

    {MM_SEPARATOR,  NULL,                       NULL,                   0,                          NULL},
    {MM_SUBHEADING, "Playback",                 NULL,                   0,                          NULL},
    {MM_CMD,        "Play Song",                "F5",                   CMD_PLAY,                   NULL},
    {MM_CMD,        "Play Pattern",             "F6",                   CMD_PLAY_PAT,               NULL},
    {MM_CMD,        "Play From Cursor",         "F7",                   CMD_PLAY_PAT_LINE,          NULL},
    {MM_CMD,        "Play From Order",          "Shift+F7",             CMD_PLAY_ORDER,             NULL},
    {MM_CMD,        "Stop",                     "F8",                   CMD_STOP,                   NULL},
    {MM_FUNC,       "Panic (all MIDI off)",     "F9",                   0,                          mm_panic},
    {MM_FUNC,       "Hard Panic",               "Shift+F9",             0,                          mm_hard_panic},
    {MM_FUNC,       "Toggle Follow Playback",   "ScrollLock",           0,                          mm_toggle_follow_playback},

    {MM_SEPARATOR,  NULL,                       NULL,                   0,                          NULL},
    {MM_SUBHEADING, "File",                     NULL,                   0,                          NULL},
    {MM_CMD,        "Load Song...",             "Ctrl+L",               CMD_SWITCH_LOAD,            NULL},
    {MM_CMD,        "Save Song / Save As...",   "Ctrl+S",               CMD_SWITCH_SAVE,            NULL},
    {MM_FUNC,       "Copy Relaunch Command",    NULL,                   0,                          mm_copy_relaunch_cmd},
    // CMD_NEWSONG / CMD_MIDI_EXPORT don't exist as enum values; the
    // user-facing shortcut still works (Ctrl+Alt+N / Ctrl+Shift+M),
    // so we list it as info-only here. Future: wire to dedicated cmds.

    {MM_SEPARATOR,  NULL,                       NULL,                   0,                          NULL},
    {MM_SUBHEADING, "Settings",                 NULL,                   0,                          NULL},
    {MM_CMD,        "System Configuration",     "F12",                  CMD_SWITCH_SYSCONF,         NULL},
    {MM_CMD,        "Global Configuration",     "Ctrl+F12",             CMD_SWITCH_CONFIG,          NULL},
#ifndef DISABLE_PALETTE_EDITOR
    {MM_CMD,        "Palette Editor",           "Shift+Ctrl+F12",       CMD_SWITCH_PALETTE,         NULL},
#endif
    {MM_CMD,        "Shortcuts & MIDI Mappings","Shift+F2",             CMD_SWITCH_KEYBINDINGS,     NULL},
    {MM_CMD,        "CC Console",               "Shift+F3",             CMD_SWITCH_CCCONSOLE,       NULL},
    {MM_CMD,        "SysEx Librarian",          "Shift+F5",             CMD_SWITCH_SYSEX_LIB,       NULL},
    {MM_FUNC,       "Toggle CC Drawmode",       "Ctrl+Shift+\xA7",      0,                          mm_toggle_cc_drawmode},
    {MM_CMD,        "Lua Console",              "Ctrl+Alt+L",           CMD_SWITCH_LUA_CONSOLE,     NULL},
    {MM_FUNC,       "Toggle Fullscreen",        "Alt+Enter",            0,                          mm_toggle_fullscreen},

    {MM_SEPARATOR,  NULL,                       NULL,                   0,                          NULL},
    {MM_FUNC,       "Quit",                     "Ctrl+Q",               0,                          mm_quit},
};

static const int MM_COUNT = sizeof(MM_ENTRIES) / sizeof(MM_ENTRIES[0]);

// ----------------------------------------------------------------------
// CUI_MainMenu implementation. Selectable entries skip separators and
// subheadings — cursor up/down only stops on actionable rows.
// ----------------------------------------------------------------------

static int mm_first_selectable(int from, int delta) {
    int i = from;
    for (int n = 0; n < MM_COUNT; n++) {
        if (i < 0) i = MM_COUNT - 1;
        if (i >= MM_COUNT) i = 0;
        if (MM_ENTRIES[i].kind == MM_CMD || MM_ENTRIES[i].kind == MM_FUNC)
            return i;
        i += delta;
    }
    return 0;
}

CUI_MainMenu::CUI_MainMenu(void) {
    UI = new UserInterface;
    cur_sel = mm_first_selectable(0, 1);
}

CUI_MainMenu::~CUI_MainMenu(void) {
}

void CUI_MainMenu::enter(void) {
    need_refresh = 1;
    cur_sel = mm_first_selectable(0, 1);
}

void CUI_MainMenu::leave(void) {
}

void CUI_MainMenu::update(void) {
    if (!Keys.size()) return;
    int key = Keys.getkey();
    KBMod kstate = Keys.getstate();
    (void)kstate;

    int act = 0;
    switch (key) {
        case SDLK_ESCAPE:
            close_popup_window();
            need_refresh++;
            return;

        case SDLK_UP: {
            int next = cur_sel - 1;
            cur_sel = mm_first_selectable(next, -1);
            need_refresh++;
            break;
        }

        case SDLK_DOWN: {
            int next = cur_sel + 1;
            cur_sel = mm_first_selectable(next, 1);
            need_refresh++;
            break;
        }

        case SDLK_HOME: {
            cur_sel = mm_first_selectable(0, 1);
            need_refresh++;
            break;
        }

        case SDLK_END: {
            cur_sel = mm_first_selectable(MM_COUNT - 1, -1);
            need_refresh++;
            break;
        }

        case SDLK_RETURN:
        case SDLK_SPACE: {
            act = 1;
            break;
        }

        default:
            break;
    }

    if (act) {
        const mm_entry &e = MM_ENTRIES[cur_sel];
        // Close the popup BEFORE dispatching, so the action can
        // switch_page or open another popup without our overlay
        // sitting on top of it.
        close_popup_window();
        Keys.flush();
        need_refresh++;
        if (e.kind == MM_FUNC && e.func) {
            e.func();
        } else if (e.kind == MM_CMD) {
            // Dispatch via the command pipeline. The cleanest way is
            // to inject a Keys event that the command dispatcher will
            // pick up — but the pipeline requires the corresponding
            // SDL keycode, which is awkward. Instead we directly
            // switch_page() for the SWITCH_* commands and synthesise
            // the rest by triggering the same action the keyhandler
            // would.
            switch (e.command) {
                case CMD_SWITCH_PEDIT:        switch_page(UIP_Patterneditor);     break;
                case CMD_SWITCH_IEDIT:        switch_page(UIP_InstEditor);        break;
                case CMD_SWITCH_SONGCONF:     switch_page(UIP_Songconfig);        break;
                case CMD_SWITCH_SONGMSG:      switch_page(UIP_SongMessage);       break;
                case CMD_SWITCH_HELP:         switch_page(UIP_Help);              break;
                case CMD_SWITCH_ABOUT:        switch_page(UIP_About);             break;
                case CMD_SWITCH_LOAD:         switch_page(UIP_Loadscreen);        break;
                case CMD_SWITCH_SAVE:         switch_page(UIP_Savescreen);        break;
                case CMD_SWITCH_SYSCONF:      switch_page(UIP_Sysconfig);         break;
                case CMD_SWITCH_CONFIG:       switch_page(UIP_Config);            break;
                case CMD_SWITCH_PALETTE:      switch_page(UIP_PaletteEditor);     break;
                case CMD_SWITCH_KEYBINDINGS:  switch_page(UIP_KeyBindings);       break;
                case CMD_SWITCH_LUA_CONSOLE:  switch_page(UIP_LuaConsole);        break;
                case CMD_SWITCH_MIDIMACEDIT:  switch_page(UIP_Midimacroeditor);   break;
                case CMD_SWITCH_ARPEDIT:      switch_page(UIP_Arpeggioeditor);    break;
                case CMD_PLAY:
                    if (cur_state != STATE_PLAY) switch_page(UIP_Playsong);
                    if (song->orderlist[0] != 0x100 && !ztPlayer->playing) {
                        ztPlayer->play(0, cur_edit_pattern, 1);
                    }
                    break;
                case CMD_PLAY_PAT:
                    ztPlayer->play(0, cur_edit_pattern, 0);
                    break;
                case CMD_PLAY_PAT_LINE:
                    ztPlayer->play(cur_edit_row, cur_edit_pattern, 2);
                    break;
                case CMD_PLAY_ORDER:
                    if (song->orderlist[cur_edit_order] != 0x100)
                        ztPlayer->play(0, cur_edit_order, 3);
                    break;
                case CMD_STOP:
                    ztPlayer->stop();
                    break;
                default:
                    break;
            }
        }
    }
}

void CUI_MainMenu::draw(Drawable *S) {
    // Centre a fixed-width box on screen.
    const int box_w = 44;     // characters
    const int box_h = MM_COUNT + 4;  // 1 title + 1 spacer + N rows + 2 borders

    int x0 = ((INTERNAL_RESOLUTION_X / 8) - box_w) / 2;
    int y0 = ((INTERNAL_RESOLUTION_Y / 8) - box_h) / 2;

    if (S->lock() != 0) return;

    // Background fill + border, IT-popup style.
    S->fillRect(col(x0), row(y0), col(x0 + box_w) - 1, row(y0 + box_h) - 1,
                COLORS.Background);
    printline(col(x0), row(y0), 143, box_w, COLORS.Highlight, S);
    printline(col(x0), row(y0 + box_h - 1), 148, box_w, COLORS.Lowlight, S);
    for (int i = 0; i < box_h; i++) {
        printchar(col(x0),               row(y0 + i), 145, COLORS.Highlight, S);
        printchar(col(x0 + box_w - 1),   row(y0 + i), 146, COLORS.Lowlight,  S);
    }

    // Title.
    print(col(x0 + 2), row(y0 + 1), "Main Menu (ESC closes)",
          COLORS.Highlight, S);

    // Items.
    int row_y = y0 + 3;
    char buf[80];
    for (int i = 0; i < MM_COUNT; i++) {
        const mm_entry &e = MM_ENTRIES[i];
        if (e.kind == MM_SEPARATOR) {
            printline(col(x0 + 2), row(row_y), 148, box_w - 4,
                      COLORS.Lowlight, S);
        } else if (e.kind == MM_SUBHEADING) {
            snprintf(buf, sizeof(buf), " %s", e.label);
            print(col(x0 + 2), row(row_y), buf, COLORS.Highlight, S);
        } else {
            // Build "  Label                 Shortcut".
            int label_room = box_w - 6;
            char label_buf[64];
            const char *lbl = e.label ? e.label : "";
            const char *sc  = e.shortcut ? e.shortcut : "";
            int sc_len = (int)strlen(sc);
            int lbl_max = label_room - sc_len - 1;
            if (lbl_max < 0) lbl_max = 0;
            snprintf(label_buf, sizeof(label_buf), "%-*.*s %s",
                     lbl_max, lbl_max, lbl, sc);
            buf[0] = (i == cur_sel) ? '>' : ' ';
            buf[1] = ' ';
            strncpy(buf + 2, label_buf, sizeof(buf) - 3);
            buf[sizeof(buf) - 1] = '\0';
            TColor c = (i == cur_sel) ? COLORS.Highlight : COLORS.Text;
            print(col(x0 + 2), row(row_y), buf, c, S);
        }
        row_y++;
    }

    S->unlock();
    // Mark the popup region dirty so screenmanager flushes the
    // updated cursor highlight to SDL on the next Refresh — without
    // this, cur_sel changes from Up/Down don't make it to the screen.
    screenmanager.UpdateWH(col(x0), row(y0), col(box_w), row(box_h));
    need_refresh = 0;
    updated++;
}
