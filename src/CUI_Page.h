#ifndef _UI_PAGE_H_
#define _UI_PAGE_H_

#include "zt.h"

typedef void (*VFunc)();

class CUI_Page {
    public:
        UserInterface *UI;
       
        CUI_Page();
        ~CUI_Page();
        virtual void enter(void)=0;
        virtual void leave(void)=0;
        virtual void update(void)=0;
        virtual void draw(Drawable *S)=0;
};

class CUI_Popup {
    public:
        int x,y;
        int xsize,ysize;
};

class CUI_InstEditor : public CUI_Page {
    public:
        int reset;
        UserInterfaceElement *trackerModeButton;

        CUI_InstEditor();
        ~CUI_InstEditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Playsong : public CUI_Page {
    public:
      
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
        ~CUI_About();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
        void draw_plasma(int x, int y, int xsize, int ysize, Drawable *S);
        void blur(int x, int y, int xsize, int ysize, Drawable *S);
};

class CUI_Songconfig : public CUI_Page {
    public:

        int rev_tpb_tab[97];
        int tpb_tab[9];// = ;

        CUI_Songconfig();
        ~CUI_Songconfig();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};


class CUI_Sysconfig : public CUI_Page {
    public:

        CUI_Sysconfig();
        ~CUI_Sysconfig();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Config : public CUI_Page {
    public:

        CUI_Config();
        ~CUI_Config();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);

        TextBox *tb;
};

class CUI_Ordereditor : public CUI_Page {
    public:

        CUI_Ordereditor();
        ~CUI_Ordereditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Help : public CUI_Page {
    public:

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
        ~CUI_Arpeggioeditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Midimacroeditor : public CUI_Page {
    public:
        
        CUI_Midimacroeditor();
        ~CUI_Midimacroeditor();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Loadscreen : public CUI_Page {
    public:
//        Bitmap *img;
        int clear;

        CUI_Loadscreen();
        ~CUI_Loadscreen();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};

class CUI_Savescreen : public CUI_Page {
    public:
//        Bitmap *img;
        int clear;
        
        CUI_Savescreen();
        ~CUI_Savescreen();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);
};
class CUI_LoadMsg : public CUI_Page {
    public:

        HANDLE hThread;
        
        int OldPriority;
        unsigned long iID;
        int strselect;
        int strtimer;

        CUI_LoadMsg();
        ~CUI_LoadMsg();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
};

class CUI_SaveMsg : public CUI_Page {
    public:

        HANDLE hThread;
        int OldPriority;
        unsigned long iID;
        int strselect;
        int strtimer;
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

        CUI_Logoscreen();
        ~CUI_Logoscreen();

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
        ~CUI_Patterneditor();
        
        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
};


class CUI_PEParms : public CUI_Page {
    public:

        CUI_PEParms();
        ~CUI_PEParms();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
};

class CUI_PEVol : public CUI_Page {
public:
    
    CUI_PEVol();
    ~CUI_PEVol();
    
    void enter(void);
    void leave(void);
    void update(void);
    void draw(Drawable *S);                 
};

class CUI_PENature : public CUI_Page {
public:
    
    CUI_PENature();
    ~CUI_PENature();
    
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
        ~CUI_SliderInput();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
        int getresult(void);

        void setfirst(int val);
};

class CUI_NewSong : public CUI_Page {
    public:

        CUI_NewSong();
        ~CUI_NewSong();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
};

class CUI_RUSure : public CUI_Page {
    public:

        char *str;
        VFunc OnYes;

        CUI_RUSure();
        ~CUI_RUSure();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
};

class CUI_SongDuration : public CUI_Page, public CUI_Popup {
    public:
        int seconds;

        CUI_SongDuration();
        ~CUI_SongDuration();

        void enter(void);
        void leave(void);
        void update(void);
        void draw(Drawable *S);                 
};
#endif