#ifndef _UserInterface_H
#define _UserInterface_H

#include "module.h"
#include "lc_sdl_wrapper.h"

void midi_sel(int dev);

enum
{
    NO_ANCHOR    = 0,
    ANCHOR_RIGHT = 1 << 0,
    ANCHOR_LEFT  = 1 << 1,
    ANCHOR_UP    = 1 << 2,
    ANCHOR_DOWN  = 1 << 3
} ;

class UserInterfaceElement {

    public:

        int anchor_type = NO_ANCHOR ;
        int anchor_x = 0 ;
        int anchor_y = 0 ;
        int anchor_offset_x = 0 ;
        int anchor_offset_y = 0 ;

        virtual void auto_anchor_at_current_pos(int how) ;
        virtual void auto_update_anchor() ;

        int x,y;
        int xsize,ysize;
        int changed;
        int ID,mousestate;
        int frame;
        int need_redraw;
        int no_tab_stop = 0;  // opt-out of TAB/UP/DOWN focus cycling

        UserInterfaceElement(void);
        virtual ~UserInterfaceElement(void) = default ;

        virtual int mouseupdate(int cur_element);
        virtual void enter(void) {}
        virtual int update()=0;
        virtual void draw(Drawable *S, int active)=0;
        virtual bool is_text_input() const { return false; }
        virtual bool is_tab_stop() const { return !no_tab_stop; }
        virtual void on_focus() {}

        UserInterfaceElement *next;
};

typedef void (*ActFunc)(UserInterfaceElement *b);

class UserInterface {

    private:

        UserInterfaceElement *UIEList,*GFXList;

    public:

        UserInterface();
        ~UserInterface();

        signed int cur_element,num_elems;
        int dontmess;

        void update();
        void draw(Drawable *S);
        void set_redraw(int ID);
        void full_refresh(void);
        void enter(void);
        void add_element(UserInterfaceElement *uie, int ID);
        void add_gfx(UserInterfaceElement *uie, int ID);
        void set_focus(int id);
        UserInterfaceElement *get_element(int ID);  
        UserInterfaceElement *get_gfx(int ID);
        bool has_text_focus() const;

        void reset(void);
};

extern int g_ui_tab_dir;


class Frame : public UserInterfaceElement {
    public:
        int type;
        Frame();
        ~Frame() = default ;
        int update();
        void draw(Drawable *S, int active);
};

class CheckBox : public UserInterfaceElement {
    public:
        int *value;
        CheckBox();
        ~CheckBox() = default ;
        int mouseupdate(int cur_element);
        int update();
        void draw(Drawable *S, int active);
};


class TextInput : public UserInterfaceElement {
    public:
        int cursor;
        int length;
        unsigned char *str;
        TextInput();
        ~TextInput() = default ;
        int update() override;
        void draw(Drawable *S, int active) override;
        bool is_text_input() const override { return true; }

        void auto_anchor_at_current_pos(int how) override;
        void auto_update_anchor() override;
};



#define LSFILE_NONE 0
#define LSFILE_ZT 1
#define LSFILE_IT 2



class GfxButton : public UserInterfaceElement {
    public:
        int state;
        int updown;
        int StuffKey,StuffKeyState;
        // Optional second binding used when Ctrl is held at click time
        // (e.g. Ctrl-click on the Conf toolbar button goes to System Config).
        int CtrlStuffKey,CtrlStuffKeyState;
        ActFunc OnClick;

        Bitmap *bmDefault;
        Bitmap *bmOnMouseOver;
        Bitmap *bmOnClick;

        GfxButton();
        ~GfxButton();

        int update();
        void draw(Drawable *S, int active);
        void setOnClick(ActFunc func);
        int mouseupdate(int cur_element);

};

struct playedinfo {

    event e;
    int longevity;
    int init_vol;
    int init_longevity;
};

class VUPlay : public UserInterfaceElement {

    public:

        int cur_row;
        int cur_pattern;
        int cur_order;
        int num_channels;
        int starttrack;
        playedinfo latency[64]; // These are set to some number when a note on track n is played
                                // and is decremented each run for doing a fake-ass volume

        VUPlay();
        ~VUPlay() = default ;

        int update();
        void draw(Drawable *S, int active);
        int draw_one_row(char str[72], event *e, int track);

};


class BarGraph : public UserInterfaceElement {

    public:
        int max, maxval, value, trackmaxval;

        BarGraph();
        ~BarGraph() = default ;

        int update();
        void draw(Drawable *S, int active);

        void setvalue(int val);
};

class LCDDisplay : public UserInterfaceElement {
    public:
        int length;
        char *str;
        char istr[200];
        LCDDisplay();
        ~LCDDisplay() = default ;

        int update();
        void draw(Drawable *S, int active);
        void setstr(char *s);
};

class AboutBox : public UserInterfaceElement {
    public:
//        int length;
//        char *str;
        Frame frm;
        int q;
        AboutBox();
        ~AboutBox() = default ;

        int update();
        void draw(Drawable *S, int active);

};

class TextBox : public UserInterfaceElement {
    public:
        bool bEof,bUseColors,bWordWrap;
        int startline, soff,mousestate,starti,startl;
        const char *text;
        Frame frm;

        TextBox();
        ~TextBox() = default ;

        int update();
        void draw(Drawable *S, int active);
        int mouseupdate(int cur_element);

        // Responsive full-width helper: 78 cols at 640px-wide windows,
        // grows by 1 col per 8px of extra resolution. Centralized so
        // callers don't copy-paste the formula.
        static int full_width_xsize();
};

class CommentEditor : public TextBox {
    public:

        CDataBuf *target;

        CommentEditor();
        ~CommentEditor();

        int update() override;
        // CommentEditor accepts typed characters (Song Message editor),
        // so the main loop must keep SDL text input enabled while it
        // is the focused element. Without this override the parent
        // TextBox's default is_text_input()=false makes the main
        // event loop call zt_text_input_stop() every frame, and no
        // SDL_EVENT_TEXT_INPUT events ever reach textinputhandler().
        bool is_text_input() const override { return true; }

        // Refresh the internal null-terminated display copy from
        // target. CDataBuf is a binary buffer (pushc does not append
        // a trailing \0), so feeding getbuffer() directly into
        // TextBox::draw — which walks text[sc] until it hits a null —
        // would read uninitialized heap past the actual data and
        // render garbage / hang. Call this after every push/pop and
        // once per draw to keep the displayed string clean.
        void refresh_display();

    private:
        // Managed null-terminated mirror of target's contents.
        char *_display;
        int   _display_alloc;
};
class LBNode {
    public:
        LBNode() {
            next = NULL;
            caption = NULL;
            int_data = 0;
            dimmed = false;
        }
        ~LBNode() {
            if (caption)
                delete[] caption;
        }
        char *caption;
        bool checked;
        bool dimmed;
        int int_data;
        LBNode *next;
};

class ListBox : public UserInterfaceElement {
    protected:
    public:
        int cur_sel;
        int y_start;
        int num_elements;
//        int sorted;
        bool use_checks, use_key_select, is_sorted;
        unsigned char check_on, check_off;
        const char *empty_message;
        LBNode *items;
        Frame frm;
            
        TColor *color_itemsel, *color_itemnosel;


        ListBox();
        ~ListBox();

        virtual int update();
        virtual void draw(Drawable *S, int active);

        int getCurrentItemIndex(void);
        const char *getCurrentItem(void);
        LBNode *getNode(int index);
        char *getItem(int index);
        int findItem(char *text);
        LBNode *insertItem(char *text);
        void removeItem(int index);
        void clear(void);
        bool getCheck(int index);
        void setCheck(int index, bool state);
        void selectAll(void);
        void selectNone(void);
        void setCursor(int index);
        void strc(char *dst, const char *src);
        int sortstr(const char *s1, const char *s2);
        int mouseupdate(int cur_element);
        LBNode *findNodeWithChar(char c, LBNode *start);
        LBNode *findNodeWithPrefix(const char *prefix);

        // Typeahead state: accumulate typed chars and match against item
        // prefixes (case-insensitive). Resets after ~800ms of idle.
        char   typeahead_buf[64];
        int    typeahead_len;
        Uint64 typeahead_last_ms;

        virtual void OnChange()=0;
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange()=0;
};

class SkinSelector : public ListBox {
    public:

        SkinSelector();
        ~SkinSelector();

//        virtual int update();
//        virtual void draw(Drawable *S, int active);

        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange();
};


class MidiOutDeviceSelector : public ListBox {
    public:

        MidiOutDeviceSelector();
        ~MidiOutDeviceSelector() = default ;

//        virtual int update();
//        virtual void draw(Drawable *S, int active);

        void OnChange() override;
        void OnSelect(LBNode *selected) override;
        void OnSelectChange() override {}
        void enter(void) override;
        bool is_tab_stop() const override { return num_elements > 0; }

};



class MidiInDeviceOpener : public ListBox {
    public:

        MidiInDeviceOpener ();
        ~MidiInDeviceOpener () = default ;

        virtual int update();
//        virtual void draw(Drawable *S, int active);

        virtual int mouseupdate(int cur_element);
        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange() {}
        virtual void enter(void);

};

void dev_sel(int dev, MidiOutDeviceSelector *mds );


class MidiOutDeviceOpener : public ListBox {
    public:

        MidiOutDeviceOpener();
        ~MidiOutDeviceOpener() = default ;
        UserInterfaceElement *lvs;
        UserInterfaceElement *bscb;
        UserInterfaceElement *al;
//        virtual int update();
//        virtual void draw(Drawable *S, int active);
        virtual int mouseupdate(int cur_element);

        virtual void OnChange();
        virtual void OnSelect(LBNode *selected);
        virtual void OnSelectChange() {}
        virtual void enter(void);
        virtual int update(void);

};



#include "Sliders.h"  // <Manu> Un poco feo poner esto aqui, pero asi puedo ir separando en modulos esto [EN: a bit ugly to put this here, but it lets me start splitting this into modules]




class LatencyValueSlider : public ValueSlider {
    public:
        LatencyValueSlider(MidiOutDeviceOpener *m);
        ~LatencyValueSlider() = default ;
        MidiOutDeviceOpener *listbox;
        virtual int update();
        void sync(void);
        int sel;
}; 
class BankSelectCheckBox : public CheckBox {
    public:
        int i_value,sel;
        MidiOutDeviceOpener *listbox;
        BankSelectCheckBox(MidiOutDeviceOpener *m);
        ~BankSelectCheckBox() = default ;
        virtual int update();
        void sync();
};

class AliasTextInput : public TextInput {
    public:
        char alias[1024];
        int sel;
        MidiOutDeviceOpener *listbox;
        AliasTextInput(MidiOutDeviceOpener *m);
        ~AliasTextInput() = default ;
        virtual int update();
        void sync();
};

extern int LastX,LastY,MousePressX,MousePressY;

#endif
