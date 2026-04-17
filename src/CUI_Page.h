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

class CUI_KeyBindings : public CUI_Page {
    public:
        int cursor_y;        // currently selected ZtAction (0..ZT_ACTION_COUNT-1)
        int list_start;      // first visible action
        int capturing;       // 0 = browsing, 1 = waiting for new key combo
        int dirty;            // unsaved edits?
        char status_line[160];

        CUI_KeyBindings();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        int visible_rows(void) const;
};
#endif
