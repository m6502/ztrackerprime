#ifndef _UI_PAGE_H_
#define _UI_PAGE_H_

#include "platform.h"
#include "lc_sdl_wrapper.h"

class Button ;
class CheckBox ;
class DirList ;
class DriveList ;
class FileList ;
class InstEditor ;
class OrderEditor ;
class PatternDisplay ;
class TextBox ;
class TextInput ;
class UserInterface ;
class UserInterfaceElement ;
class ValueSlider ;
class ListBox ;

typedef void (*VFunc)();

class CUI_Page {
    public:
        UserInterface *UI;

        CUI_Page();
        virtual ~CUI_Page();
        virtual void enter(void)=0;
        virtual void leave(void)=0;
        virtual void update(void)=0;
        virtual void draw(Drawable *S)=0;
};

class CUI_InstEditor : public CUI_Page {
    public:
        int reset;
        UserInterfaceElement *trackerModeButton;
        InstEditor *ie;

        CUI_InstEditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Playsong : public CUI_Page {
    public:

        PatternDisplay *pattern_display;

        UserInterface *UI_PatternDisplay;
        UserInterface *UI_VUMeters;

        int clear;

        CUI_Playsong();
        ~CUI_Playsong();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

};

class CUI_About : public CUI_Page {
    public:


//        Bitmap *work, *dest;
//        TColor color;
//        int textx,texty,numtexts,curtext;
//        char *texts[3];

        CUI_About();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
        void draw_plasma(int x, int y, int xsize, int ysize, Drawable *S);
        void blur(int x, int y, int xsize, int ysize, Drawable *S);
};

class CUI_Songconfig : public CUI_Page {
    public:

        OrderEditor *oe;

        int rev_tpb_tab[97];
        int tpb_tab[9];// = ;

        CUI_Songconfig();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};


class CUI_Sysconfig : public CUI_Page {
    public:

        CUI_Sysconfig();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_PaletteEditor : public CUI_Page {
    public:
        int selected_slot;     // index within the focused panel
        int focus_panel;       // 0 = swatch grid, 1 = preset panel
        int channel_edit;      // 0 = none, 1 = R, 2 = G, 3 = B
        int dirty;             // unsaved changes vs. snapshot
        TColor snapshot[32];   // saved COLORS.* on entry (the "anchor" the
                               // global brightness/contrast/tint operate on)
        char status_line[128];

        // Dynamic preset list — built at enter() from palettes/*.conf and
        // skins/*/colors.conf so users can switch between every shipped look
        // without leaving the editor.
        int  num_presets;
        char preset_label[64][32];
        char preset_path[64][512];
        int  preset_is_skin[64];

        // Global adjustments applied on top of the snapshot so they're
        // composable and reversible. Brightness in [-255..255] is added per
        // channel, contrast in [-100..+100] scales around mid-grey,
        // tint_index selects one of g_tints (0 = none), tint_amount in
        // [0..255] is the blend weight toward the tint color.
        int brightness;
        int contrast;
        int tint_index;
        int tint_amount;

        // When true, clicking a palette preset (Light Blue, Gold, etc.)
        // resets the skin to "default" first so the palette renders against
        // its canonical PNG templates. When false, the palette is applied on
        // top of whatever skin is currently loaded. Skin presets always
        // do a full skin switch regardless of this toggle.
        int reset_skin_on_palette;

        CUI_PaletteEditor();
        ~CUI_PaletteEditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        void load_palette_file(const char *path_or_fname);
        // Full skin switch — same code path as F12 Sysconfig skin selector.
        // Used when the user picks a "[skin] xxx" preset so the new skin's
        // PNG templates are loaded from disk, not just the colors.conf.
        void load_skin_full(const char *colors_conf_path);
        void save_palette_file(const char *fname);
        void apply_channel_delta(int delta);
        void apply_channel_set(int value);

        // Globals: recomputes COLORS from snapshot + brightness/contrast/tint.
        void recompute_globals(void);
        void rebuild_preset_list(void);
};

class CUI_Config : public CUI_Page {
    public:

        CUI_Config();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        TextBox *tb;
};

class CUI_Ordereditor : public CUI_Page {
    public:

        CUI_Ordereditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Help : public CUI_Page {
    public:

        TextBox *tb;

        int needfree;

        // Section anchor lines (line numbers in tb->text where section
        // headers start). Built once at load time; used by Tab/Shift-Tab
        // to jump tb->startline forward/back to the next/prev section.
        int *section_lines;
        int section_count;

        CUI_Help();
        ~CUI_Help();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_SongMessage : public CUI_Page {
    public:

        int needfree;
        CDataBuf *buffer;

        CUI_SongMessage();
        ~CUI_SongMessage();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Arpeggioeditor : public CUI_Page {
    public:

        CUI_Arpeggioeditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Midimacroeditor : public CUI_Page {
    public:

        CUI_Midimacroeditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Loadscreen : public CUI_Page {
    public:
//        Bitmap *img;
        int clear;

        FileList *fl;
        DirList *dl;
        DriveList *dr;

        CUI_Loadscreen();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Savescreen : public CUI_Page {
    public:
//        Bitmap *img;
        int clear;

        FileList *fl;
        DirList *dl;
        DriveList *dr;

        TextInput *ti;
        Button *b_zt;
        Button *b_mid;
        Button *b_mid_mc;
        Button *b_mid_pertrack;
        //Button *b_gba;

        CUI_Savescreen();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};
class CUI_LoadMsg : public CUI_Page {
    public:

        zt_thread_handle hThread;

        int OldPriority;
        unsigned long iID;
        int strselect;
        zt_timer_handle strtimer;

        CUI_LoadMsg();
        ~CUI_LoadMsg();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_SaveMsg : public CUI_Page {
    public:

        zt_thread_handle hThread;
        int OldPriority;
        unsigned long iID;
        int strselect;
        zt_timer_handle strtimer;
        int filetype;

        CUI_SaveMsg();
        ~CUI_SaveMsg();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Logoscreen : public CUI_Page {
    public:

        int ready_fade, faded;

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

enum { PEM_REGULARKEYS, PEM_MOUSEDRAW };
enum { MD_VOL=0, MD_FX, MD_FX_SIGNED, MD_END};

class CUI_Patterneditor : public CUI_Page {
    public:
        int tracks_shown, field_size, cols_shown, clear, mode, md_mode, mousedrawing;
        int last_cur_pattern ;
        int last_pattern_size ;

        CUI_Patterneditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};


class CUI_PEParms : public CUI_Page {
    public:

        ValueSlider *vs_step ;
        ValueSlider *vs_pat_length ;
        ValueSlider *vs_highlight ;
        ValueSlider *vs_lowlight ;

        CheckBox *cb_centered ;
        CheckBox *cb_stepedit ;
        CheckBox *cb_recveloc ;
        CheckBox *cb_drawmode ;
        int      drawmode_val ;

        ValueSlider *vs_speedup ;

        CUI_PEParms();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_PEVol : public CUI_Page {
public:

    CUI_PEVol();

    void enter(void);
    void leave(void);
    void update(void);
    void draw(Drawable *S);
};

class CUI_PENature : public CUI_Page {
public:

    CUI_PENature();

    void enter(void);
    void leave(void);
    void update(void);
    void draw(Drawable *S);
};

class CUI_SliderInput : public CUI_Page {
    public:

        int result;
        int state, prev_state;
        int canceled;
        char str[32];
        int checked;

        CUI_SliderInput();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
        int getresult(void);

        void setfirst(int val);
};


class CUI_NewSong : public CUI_Page {
    public:

        Button *b_yes;
        Button *b_no;

        CUI_NewSong();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};


class CUI_RUSure : public CUI_Page {
    public:

        Button *button_yes;
        Button *button_no;

        const char *str;
        VFunc OnYes;

        CUI_RUSure();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

// ESC main menu — Schism-style modal popup that lists every page /
// action so users don't have to memorise the F-key map. ESC opens it
// from any page; ESC again closes; cursor up/down navigates;
// Enter dispatches.
class CUI_MainMenu : public CUI_Page {
    public:
        int cur_sel;

        CUI_MainMenu();
        ~CUI_MainMenu();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_SongDuration : public CUI_Page {
    public:
        int x,y;
        int xsize,ysize;
        int seconds;

        CUI_SongDuration();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_LuaConsole : public CUI_Page {
    public:
        CUI_LuaConsole();
        ~CUI_LuaConsole();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

// SysEx Librarian (Shift+F5). Lists `.syx` files in the configured
// syx_folder so the user can:
//   1. Send a request-dump SysEx to a synth (e.g. Universal Device
//      Inquiry) by hitting Enter on the file.
//   2. Watch incoming SysEx auto-save as `recv_<timestamp>.syx` in
//      the same folder so the captured patches appear in the file
//      list immediately.
//   3. Re-send a previously captured `.syx` to dump the patch back
//      to the synth.
class CUI_SysExLibrarian : public CUI_Page {
    public:
        int  file_cur;                  // selected file
        int  file_top;                  // scroll offset
        int  num_files;
        char folder[1024];
        char files[256][256];
        // Most recent received messages: (timestamp, length, first 8
        // bytes preview). Auto-pruned to RECENT_LOG_MAX entries.
        static const int RECENT_LOG_MAX = 8;
        struct RecentLog {
            char timestamp[16];
            int  length;
            unsigned char preview[8];
            char saved_as[64];
        };
        RecentLog recent[RECENT_LOG_MAX];
        int       recent_count;
        int       recv_seq;             // monotonic counter for filenames
        char      status_line[160];

        CUI_SysExLibrarian();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        void rescan_folder(void);
        void resolve_folder(void);
        void send_selected(void);
        void drain_recv(void);          // pop sysex_inq, save, push to recent[]
};

// CC Console (Shift+F3). Loads CCizer-format `.txt` files from the
// configured ccizer_folder. Each slot has a real ValueSlider widget
// that's mouse-clickable AND keyboard-adjustable; tweaking fires MIDI
// CC (or 14-bit Pitchbend for "PB" slots) out to the current MIDI Out
// device. Up/Dn navigate slot_cur through the slot list, scrolling the
// visible window; the slider widgets are repositioned per-frame to
// match scroll state, and off-screen sliders get xsize=0 which
// disables their mouse / keyboard / draw paths.
class CUI_CcConsole : public CUI_Page {
    public:
        int focus;          // 0 = file list, 1 = slot grid
        int file_cur;       // selected file index
        int file_top;       // scroll offset for file list
        int slot_cur;       // selected slot in loaded file
        int slot_top;       // scroll offset for slot grid
        int channel;        // 1..16
        int loaded;         // is `loaded_file` populated?
        int learning;       // 0 = browsing, 1 = waiting for incoming MIDI to learn-bind to slot_cur
        char status_line[160];
        char folder[1024];
        int  num_files;
        char files[256][256];

        // One ValueSlider per max-slot; positioned per-frame based on
        // slot_top + visible window. Off-screen sliders get xsize=0.
        ValueSlider *sliders[128];          // ZT_CCIZER_MAX_SLOTS
        int          last_values[128];      // last seen value to detect changes
        int          last_visible_count;    // for repaint optimization

        // Real ListBox for the Files pane (matches F11 SkinSelector and
        // F4/Shift+F4 preset list — black-bar highlight, typeahead,
        // mouse click). Owned by `UI`'s element pool. Items mirror
        // `files[]`; rebuilt by rescan_folder() and load_by_basename().
        ListBox                 *file_selector;
        // 1x1 stub UIE that owns the "we are on the slot grid" tab
        // focus state. Owned by `UI`.
        UserInterfaceElement    *grid_focus;
        void rebuild_file_list_items(void);

        CUI_CcConsole();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        void rescan_folder(void);
        void load_selected(void);
        // Look up `bn` (basename, e.g. "microfreak.txt") in the current
        // ccizer folder and load it.
        void load_by_basename(const char *bn);

        // Reposition the slider widget pool: visible slots get x/y/xsize/
        // min/max/value, off-screen slots get xsize=0.
        void position_sliders(int grid_max_rows);
        // After UI->update() reports widget activity, copy any user-driven
        // value changes back to slots and fire send_slot for each.
        void absorb_slider_changes(void);
};

// Unified Shortcuts & MIDI Mappings page. cursor_y picks an action;
// cursor_x picks the column to edit:
//   0 = keyboard binding
//   1..ZT_MM_SLOTS_PER_ACTION = MIDI slot 0..N-1
class CUI_KeyBindings : public CUI_Page {
    public:
        int cursor_y;        // currently selected ZtAction (0..ZT_ACTION_COUNT-1)
        int cursor_x;        // currently selected column (see above)
        int list_start;      // first visible action
        int capturing;       // 0 = browsing, 1 = waiting for new key combo
        int dirty;           // unsaved edits?
        char status_line[160];

        CUI_KeyBindings();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        int visible_rows(void) const;
};

#endif
