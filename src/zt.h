#ifndef _ZT_H
#define _ZT_H

// This adds some debugging stuff and some on-screen debug indicators
//#define DEBUG

// This makes every updated rect appear randomly colored so you can see what gets updated
//#define DEBUG_SCREENMANAGER

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <io.h>
#include <direct.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <mmsystem.h>
#include <math.h>


#include <sdl.h>
#include <SDL_main.h>
//#include "sdl_mixer.h"  // this is for audio testing



// ------------------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------------------

//#define VER_MAJ 0

// <Manu> antes era 98
//#define VER_MIN 986

#define ZTRACKER_VERSION                "zTracker' v20171228"
 
//#define _ENABLE_AUDIO                 1  // this enables audio init and audio plugins

#define ZOOM                            2.0f

#define INTERNAL_RESOLUTION_X           ((int)(zt_config_globals.screen_width  * (1.0f / (float)ZOOM)))
#define INTERNAL_RESOLUTION_Y           ((int)(zt_config_globals.screen_height * (1.0f / (float)ZOOM)))

#define RESOLUTION_X                    (zt_config_globals.screen_width)
#define RESOLUTION_Y                    (zt_config_globals.screen_height)

#define FACTOR_ESCALAX                  ((float)INTERNAL_RESOLUTION_X / (float)RESOLUTION_X)
#define FACTOR_ESCALAY                  ((float)INTERNAL_RESOLUTION_Y / (float)RESOLUTION_Y)



#define SCREEN_BPP                      32

#define BASE_OCTAVE_MIN                 0
#define BASE_OCTAVE_MAX                 9
#define BASE_OCTAVE_DEFAULT             4

#define DEFAULT_CURSOR_STEP             1

#define EDIT_LOCK_TIMEOUT               800 // ms

//#define PATTERN_EDIT_ROWS               (32+4)
extern int PATTERN_EDIT_ROWS;

#define DIK_MOUSE_1_ON                  0xF1
#define DIK_MOUSE_1_OFF                 0xF2
#define DIK_MOUSE_2_ON                  0xF3
#define DIK_MOUSE_2_OFF                 0xF4

#define GET_LOW_BYTE(x)                 ((unsigned char)(x & 0x00FF))
#define GET_HIGH_BYTE(x)                ((unsigned char)((x & 0xFF00)>>8))

#define LOW_MIDI_BYTE(x)                ((x) & 0x007F)
#define HIGH_MIDI_BYTE(x)               ((x) & 0x3F80)

#ifdef RGB
  #undef RGB
#endif

#define RGB(r,g,b)                      (long)((r)+((g)<<8)+((b)<<16))

//// Some hacks
#define mutetrack(t)                    song->track_mute[t] = 1; MidiOut->mute_track(t)
#define unmutetrack(t)                  song->track_mute[t] = 0; MidiOut->unmute_track(t)
#define toggle_track_mute(t)            song->track_mute[t] = !song->track_mute[t];    if (song->track_mute[t]) MidiOut->mute_track(t); else MidiOut->unmute_track(t)
////


#define MAX_MIDI_DEVS                   64 // Max midi devices in lists

#define MAX_MIDI_OUTS                   MAX_MIDI_DEVS
#define MAX_MIDI_INS                    MAX_MIDI_DEVS

#define MAX_TRACKS                      64 // Max # of tracks
#define MAX_INSTS                       100 // Max # of instruments


#include "../resource.h"         // resource includes for win32 icon

#include "fxml.h"
#include "lc_sdl_wrapper.h"      // libCON wrapper 
#include "zlib_wrapper.h"        // zlib wrapper 
#include "CDataBuf.h"            // data buffer for building chunks before writing to disk
#include "timer.h"               // timer 
#include "module.h"              // module load/save and memory/events 
#include "UserInterface.h"       // UI drawing, widgets, and UI managment
#include "font.h"                // font drawer
#include "keybuffer.h"           // keyboard driver
#include "conf.h"                // config class (assoc. array class)
#include "midi-io.h"             // MIDI in/out 
#include "OutputDevices.h"       // in/out plugins
#include "edit_cols.h"           // pattern editor columns hack
#include "playback.h"            // playing 
#include "CUI_Page.h"            // main UI pages
#include "import_export.h"       // file import/export
#include "img.h"                 // image loading/scaling

extern ZTConf zt_config_globals;

class colorset {

public:

    TColor Background;     // Background of the ztracker "panel"
    TColor Highlight;      // highlight of the ztracker "panel"
    TColor Lowlight;       // lowlight of the ztracker "panel"
    TColor Text;           // text that goes on the zracker "panel" and on muted track names
    TColor Data;           // text that is used for the info boxes at the top of the screen
    TColor Black;          // background of the top information boxes
    TColor EditText;       // text that is in the pattern editor and all other boxes except top info boxes
    TColor EditBG;         // background of the pattern editor and all other boxes except top info boxes
    TColor EditBGlow;      // background of pattern editor (lowlight)
    TColor EditBGhigh;     // background of pattern editor (highlight)
    TColor Brighttext;     // text that goes above each track when they are not muted
    TColor SelectedBGLow;  // background of pattern editor (selected not on a lowlight or highlight)
    TColor SelectedBGHigh; // background of pattern editor (selected ON a lowlight or highlight), also cursor row selected
    TColor LCDHigh;        // beat display at bottom left corner high color
    TColor LCDMid;         // beat display at bottom left corner hid color
    TColor LCDLow;         // beat display at bottom left corner low color
    TColor CursorRowHigh;  // cursor row on a lowlight or highlight
    TColor CursorRowLow;   // cursor row not on a lowlight or highlight

    colorset() {
        setDefaultColors();
    }

    TColor getColor(Uint8 Red, Uint8 Green, Uint8 Blue) {
        return (Blue + (Green<<8) + (Red<<16));
    }
    
    TColor get_color_from_hex(char *str, conf *ColorsFile) {
        unsigned char r,g,b;
        r = ColorsFile->getcolor(str,0);
        g = ColorsFile->getcolor(str,1);
        b = ColorsFile->getcolor(str,2);
        return getColor(r,g,b);
    }

    int load(char *file) {
        conf ColorsFile;
        if (!ColorsFile.load(file))
            return 0;
        Background =     get_color_from_hex("Background",&ColorsFile);
        Highlight=       get_color_from_hex("Highlight",&ColorsFile);    
        Lowlight =       get_color_from_hex("Lowlight",&ColorsFile);
        Text =           get_color_from_hex("Text",&ColorsFile);
        Black =          get_color_from_hex("Black",&ColorsFile);
        Data =           get_color_from_hex("Data",&ColorsFile);
        EditText =       get_color_from_hex("EditText",&ColorsFile);
        EditBG   =       get_color_from_hex("EditBG",&ColorsFile);
        EditBGlow =      get_color_from_hex("EditBGlow",&ColorsFile);
        EditBGhigh =     get_color_from_hex("EditBGhigh",&ColorsFile);
        Brighttext =     get_color_from_hex("Brighttext",&ColorsFile);
        SelectedBGLow =  get_color_from_hex("SelectedBGLow",&ColorsFile);
        SelectedBGHigh = get_color_from_hex("SelectedBGHigh",&ColorsFile);
        LCDLow =         get_color_from_hex("LCDLow",&ColorsFile);
        LCDMid =         get_color_from_hex("LCDMid",&ColorsFile);
        LCDHigh =        get_color_from_hex("LCDHigh",&ColorsFile);
        CursorRowHigh =  get_color_from_hex("CursorRowHigh",&ColorsFile);
        CursorRowLow =   get_color_from_hex("CursorRowLow",&ColorsFile);

//        setDefaultColors() ;

        return 1;
    }
    
    void setDefaultColors() {

        // <Manu> El color del Scream Tracker era ligeramente distinto
        //Background =     getColor(0xA4,0x90,0x54);
        Background =     getColor(0xA1,0x91,0x55);
        //Background =     getColor(0x7F,0x00,0x00);

        Highlight=       getColor(0xFF,0xDC,0x84);
        Lowlight =       getColor(0x50,0x44,0x28);
        Text =           getColor(0x00,0x00,0x00);
        Black =          getColor(0x00,0x00,0x00);
        Data =           getColor(0x00,0xFF,0x00);
        EditText =       getColor(0x80,0x80,0x80);
        EditBG   =       Black;
        EditBGlow =      getColor(0x14,0x10,0x0C);
        EditBGhigh =     getColor(0x20,0x20,0x14);
        Brighttext =     getColor(0xcf,0xcf,0xcf);
        SelectedBGLow =  getColor(0x00,0x00,0x80);
        SelectedBGHigh = getColor(0x00,0x00,0xA8);
        LCDLow =         getColor(0x60,0x00,0x00);
        LCDMid =         getColor(0xA0,0x00,0x00);
        LCDHigh =        getColor(0xFF,0x00,0x00);
        CursorRowHigh =  getColor(0x20,0x20,0x20);
        CursorRowLow =   getColor(0x10,0x10,0x10);
    }

} ;

#include "skins.h"




enum state { 
  
  STATE_PEDIT, 
  STATE_IEDIT, 
  STATE_PLAY, 
  STATE_LOGO, 
  STATE_ABOUT, 
  STATE_SONG_CONFIG, 
  STATE_SYSTEM_CONFIG,
  STATE_CONFIG, 
  STATE_ORDER, 
  STATE_PEDIT_WIN, 
  STATE_HELP,
  STATE_LOAD, 
  STATE_SAVE, 
  STATE_STATUS, 
  STATE_SLIDER_INPUT,
  STATE_SONG_MESSAGE, 
  STATE_ARPEDIT, 
  STATE_MIDIMACEDIT
} ;


#define DEFAULT_STATE               STATE_PEDIT
#define DEFAULT_STATE_UIP           UIP_Patterneditor

#define MAX_UPDATE_RECTS            512


// ------------------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------------------

class CScreenUpdateManager 
{
public: 

  SDL_Rect r[MAX_UPDATE_RECTS];
  int updated_rects;
  bool update_all;

  CScreenUpdateManager() 
  {

    updated_rects = 0;
    update_all = false;
  }
  
  ~CScreenUpdateManager() 
  {
  
  }



  // ----------------------------------------------------------------------------------------------
  // <Manu 06 de Julio de 2005> He optimizado ligeramente esto y he limpiado el codigo
  //
  void Update(Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2) 
  {
    if (update_all) return;
    if (updated_rects < MAX_UPDATE_RECTS-1) {

      if (x1 < 0) x1 = 0 ; 
      if (y1 < 0) y1 = 0 ;
      if (x2 > (INTERNAL_RESOLUTION_X - 1)) x2 = INTERNAL_RESOLUTION_X - 1 ;
      if (y2 > (INTERNAL_RESOLUTION_Y - 1)) y2 = INTERNAL_RESOLUTION_Y - 1 ;

      SDL_Rect *rect = &r[updated_rects] ;

      rect->x = x1 ;
      rect->y = y1 ;
      rect->w = x2 - x1 ;
      rect->h = y2 - y1 ;
        
      updated_rects++ ;
    }
  }



  // ----------------------------------------------------------------------------------------------
  //
  //
  void UpdateWH(Sint16 x1, Sint16 y1, Uint16 w, Uint16 h) 
  {
    if (update_all) return;
    if (updated_rects < MAX_UPDATE_RECTS-1) {
      
      SDL_Rect *rect=&r[updated_rects] ;
      
      rect->x = x1 ;
      rect->y = y1 ;
      rect->w = w ;
      rect->h = h ;
      
      updated_rects++ ;
    }
  }




  // ----------------------------------------------------------------------------------------------
  //
  //
  void UpdateAll(void) 
  {
    update_all = true;
  }




  // ----------------------------------------------------------------------------------------------
  //
  //
  void Reset(void) 
  {
    updated_rects = 0;
  }




  // ----------------------------------------------------------------------------------------------
  //
  //
  void Refresh(Drawable *S) 
  {
    if (update_all) {

      SDL_UpdateRect(S->surface,0,0,0,0);
      update_all = false;
      updated_rects = 0;
    } 
    else {

      if (updated_rects > 0) {
        
#ifdef DEBUG_SCREENMANAGER
        for (int i=0;i < updated_rects; i++)
          SDL_FillRect(S->surface,&r[i], rand() );
#endif
          /*
          for (int i=0;i<updated_rects;i++)
          if (r[i].x<0 || r[i].y<0 || r[i].x>INTERNAL_RESOLUTION_X-1 || r[i].y>INTERNAL_RESOLUTION_Y-1)
          SDL_Delay(1);
        */
        SDL_UpdateRects(S->surface, updated_rects, &r[0]);
        updated_rects = 0;
      }
    }

  }



  // ----------------------------------------------------------------------------------------------
  //
  //
  bool NeedRefresh(void) 
  {
    return (updated_rects > 0);
  }
};


extern CScreenUpdateManager screenmanager;









// ------------------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------------------

class CClipboard {
    public:
        event *event_list[MAX_TRACKS];
        int tracks;
        int rows;
        int full;

        CClipboard();
        ~CClipboard();
        void copy(void);
        void paste(int start_track, int start_row, int mode); // 0 = insert, 1 = overwrite, 2 = merge
        void clear(void);
};


// ------------------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------------------

class WStackNode {
    public:
        CUI_Page *page;
        WStackNode *next;
        
        WStackNode(CUI_Page *p);
        ~WStackNode();
};


// ------------------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------------------

class WStack {  // The window stack.. used for showing popup windows
    private:
        WStackNode *head;
    public:
        WStack();
        ~WStack();
        void push(CUI_Page *p);
        CUI_Page *pop(void);
        void update(void);
        void draw(Drawable *S);
        bool isempty(void);
};

// ------------------------------------------------------------------------------------------------
//
// ------------------------------------------------------------------------------------------------


extern WStack window_stack;

typedef struct {
    unsigned char r;
    unsigned char g;   //239 6912 // 239 6914
    unsigned char b;
} color;

typedef struct {
    int startx;
    int type;
    int place;
} edit_col;

typedef struct {
    unsigned char note;
    unsigned char chan;
} mbuf;

enum E_col_type { T_NOTE, T_OCTAVE, T_INST, T_VOL, T_CHAN, T_LEN, 
                  T_FX, T_FXP};

    enum Ecmd {
        CMD_NONE,
        CMD_SWITCH_HELP,
        CMD_SWITCH_PEDIT, 
        CMD_SWITCH_IEDIT,
        CMD_SWITCH_SONGCONF,
        CMD_SWITCH_ORDERLIST,
        CMD_SWITCH_SYSCONF,
        CMD_SWITCH_ABOUT,
        CMD_SWITCH_LOAD,
        CMD_SWITCH_SAVE,
        CMD_SWITCH_CONFIG,
        CMD_PLAY,
        CMD_PLAY_PAT,
        CMD_PLAY_PAT_LINE,
        CMD_STOP,
        CMD_PANIC,
        CMD_HARD_PANIC,
        CMD_QUIT,
        CMD_PLAY_ORDER,
        CMD_SWITCH_SONGLEN,
        CMD_SWITCH_SONGMSG,
        CMD_SWITCH_ARPEDIT,
        CMD_SWITCH_MIDIMACEDIT
    };


extern Skin *CurrentSkin;

extern CClipboard *clipboard;

extern bool bScrollLock;
extern bool bMouseIsDown;

//extern int FULLSCREEN;
//extern int FADEINOUT;
//extern int AUTOOPENMIDI;
//extern char SKINFILE[256];
//extern char COLORFILE[256];
//extern char WORKDIRECTORY[256];

#ifdef DEBUG
    extern BarGraph *playbuff1_bg,*playbuff2_bg,*keybuff_bg;
#endif

int lock_mutex(HANDLE hMutex, int timeout = 2000L);
int unlock_mutex(HANDLE hMutex);

void reset_editor(void);

int zcmp(char *s1, char *s2);
int zcmpi(char *s1, char *s2);

int checkclick(int x1, int y1, int x2, int y2);

extern int sel_pat,sel_order;
extern int modal;
//extern int prebuffer_rows;
extern int LastX,LastY,MousePressX,MousePressY;
extern edit_col edit_cols[41];
extern int zclear_flag, zclear_presscount;
extern int fast_update, FU_x, FU_y, FU_dx, FU_dy;
extern zt_module *song;
extern player *ztPlayer;
extern int editing; // editing flag/mutex (so it doesnt play a null pointer or something 
extern char *col_desc[41];
extern int status_change;
extern int cur_edit_row,cur_edit_row_disp,cur_edit_pattern;
extern instrument blank_instrument;
extern KeyBuffer Keys;
extern int cur_state,need_refresh;
extern int cur_volume_percent;
extern int cur_naturalization_percent;
extern int fixmouse;
extern int cur_inst;
extern char* zt_directory;

extern bool bDontKeyRepeat;

extern char *statusmsg;
extern char szStatmsg[1024];

#define COLORS CurrentSkin->Colors

void status(char *msg,Drawable *S);
void status(Drawable *S);

void force_status(char *msg);

void redrawscreen(Drawable *S);
void disp_pattern(int tracks_shown,int field_size,int cols_shown, Drawable *S);

extern int select_row_start,select_row_end;
extern int select_track_start,select_track_end;
extern int selected;

extern int need_popup_refresh;

extern int NoFlicker;
extern int updated;
extern int cur_state,need_refresh;
extern int do_exit;
extern int editing;
extern int cur_edit_order;
extern int cur_edit_track;
extern int cur_edit_col;
extern int cur_edit_row;
extern int cur_edit_row_disp;
extern int cur_edit_pattern;
extern int cur_edit_track_disp;
extern int cur_edit_column;
extern int base_octave ;
extern int cur_step;

extern int keypress;
extern int keywait;
extern int keytimer;
extern int keyID;
extern int status_change;

extern event blank_event;
extern instrument blank_instrument;

extern int key_jazz;
extern mbuf jazz[512];

extern midiOut *MidiOut;
extern midiIn  *MidiIn;


extern UserInterface *UI;

enum E_edit_viewmode { VIEW_SQUISH, VIEW_REGULAR, VIEW_FX, VIEW_BIG }; //, VIEW_EXTEND };

extern int last_cmd_keyjazz,last_keyjazz;

void draw_status(Drawable *S); /* S MUST BE LOCKED! */
extern Bitmap *load_cached_bitmap(char *name);
extern char ls_filename[256],load_filename[256], save_filename[256];
extern void setGamma(float f, Screen *S);
extern int faded, doredraw;

extern Bitmap *load_bitmap(char *name);

extern void fadeIn(float step, Screen *S);

extern CUI_Page *ActivePage, *LastPage, *PopupWindow;

extern CUI_About *UIP_About;
extern CUI_InstEditor *UIP_InstEditor;
extern CUI_Logoscreen *UIP_Logoscreen;
extern CUI_Loadscreen *UIP_Loadscreen;
extern CUI_Savescreen  *UIP_Savescreen;
extern CUI_Ordereditor *UIP_Ordereditor;
extern CUI_Playsong *UIP_Playsong;
extern CUI_Songconfig *UIP_Songconfig;
extern CUI_Sysconfig *UIP_Sysconfig;
extern CUI_Config *UIP_Config;
extern CUI_Patterneditor *UIP_Patterneditor;
extern CUI_PEParms *UIP_PEParms;
extern CUI_PEVol   *UIP_PEVol;
extern CUI_PENature   *UIP_PENature;
extern CUI_SliderInput *UIP_SliderInput;
extern CUI_LoadMsg *UIP_LoadMsg;
extern CUI_SaveMsg *UIP_SaveMsg;
extern CUI_NewSong *UIP_NewSong;
extern CUI_RUSure *UIP_RUSure;
extern CUI_SongDuration *UIP_SongDuration;
extern CUI_SongMessage *UIP_SongMessage;
extern CUI_Arpeggioeditor *UIP_Arpeggioeditor;
extern CUI_Midimacroeditor *UIP_Midimacroeditor;

extern void switch_page(CUI_Page *page);
extern int need_update;

void popup_window(CUI_Page *page);
void close_popup_window();

extern int light_pos, need_update_lights ;

extern int checkmousepos(int x1, int y1, int x2, int y2);

extern char *hex2note(char *str,unsigned char note);
extern int zcmpi(char *s1, char *s2);

extern int check_ext(const char *str, const char *ext);

extern int file_changed;

extern int load_lock,save_lock;

extern void do_save(void);  
extern int already_changed_default_directory;
void draw_status_vars(Drawable *S); 
void begin_save(void);
extern LPSTR cur_dir;

extern int pe_modification;
extern Drawable * pe_buf;

extern int calcSongSeconds(int cur_row = -1, int cur_ord = -1);

#endif