/*

    +:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+

    ztracker - (c)2000-2001 christopher micali <cmicali@users.sourceforge.net>

    +:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+

    Error executing bscmake.exe.

    zt.exe - 3367 error(s), 0 warning(s)

    At least we didn't have any warnings ....

    +:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+:+

    the code is ugly, but such is the way of things

    Much of this was written from scratch by c.micali
    But.. huge source contributions and general help from:

    Austin Luminais (lipid)
    Nic Soudee (zonaj)
    Daniel Kahlin (tlr)

    Gotta say, they are awesome.

    the action() loop is the main program loop
    activepage holds a pointer to the active CUI_Page instance that has the
       current screen's contents

    You can try adding stuff to CUI_Patterneditor.cpp

    This is my first try at something like this... I am an amature
    C++/win32 coder.  It works, and that's what counts, right? (that's zt's ehm, motto)

    If any of this helps you, feel free to use it.
    Please send any source contributions to cmicali@users.sourceforge.net.
    If you use code, please credit somewhere.

    Good luck!


    CUI_*.cpp          -  The main screen containers
    CDataBuf.cpp       -  Databuffer class.. used for loading/saving
    conf.cpp           -  .conf loader class (reused from old project, crusty and schwill)
    edit_cols.cpp      -  Terrible hack for editing column data, figure it out if you can ..
    font.cpp           -  VGA bitmap font loader/drawer functions (should be a class?)
    import_export.cpp  -  Lipid and cmicali's IT->zt translation code
    it.cpp             -  Lipid's IT loading routines
    keybuffer.cpp      -  Keybuffering routines
    list.cpp           -  Old bad shit llist class for the conf loader
    main.cpp           -  This file - Housekeeping, main loop, init, deinit, keyboard buffer filling
                            routines, global key handling, global variables
    midiOutDev...      -  midiOutDevice and main midiOut classes
    module.cpp         -  Module/event structures/classes, .zt module save/load
    playback.cpp       -  Playback routines, timing, pattern parsing, event queuing (meaty code)
    UserInterface.cpp  -  UI routines, widgets, etc etc
    ztfile-io.cpp      -  .zt file load/save routines (module.h is the header for this)

    zt.h               -  Whole bunch of defines, structs, and externs

*/


#include <cmath>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "zt.h"
#include "lua_engine.h"
#include "keybindings.h"
#include "save_key_dispatch.h"
#include "zt_crash.h"
#include "midi_mappings.h"

#ifdef __APPLE__
extern "C" void zt_macos_disable_cmd_q(void);
extern "C" void zt_macos_disable_cmd_w(void);
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#include <limits.h>
#include <cstring>
#endif


// zt.h defines SDL_MAIN_HANDLED globally so <SDL_main.h> is not pulled into
// every translation unit (which would emit WinMain in every TU on Windows and
// cause LNK2005). On Windows -- in the single TU that owns main() -- we undo
// the define and include <SDL_main.h> exactly once so SDL can emit its
// WinMain -> SDL_main entry shim into this object file only.
#if defined(_WIN32)
#  ifdef SDL_MAIN_HANDLED
#    undef SDL_MAIN_HANDLED
#  endif
#  include <SDL_main.h>
#  ifndef main
#    define main SDL_main
#  endif
#endif


#ifdef DEBUG
    BarGraph *playbuff1_bg,*playbuff2_bg,*keybuff_bg;
#endif


// #define USE_BG_IMAGE  // This is for a stupid background image

CScreenUpdateManager screenmanager;
Skin *CurrentSkin = NULL;

SDL_Window *zt_main_window = NULL;
char zt_pending_window_title[256] = "";
volatile int zt_pending_window_title_dirty = 0;
bool zt_text_input_is_active = false;
static SDL_Renderer *zt_renderer = NULL;
static SDL_Texture *zt_frame_texture = NULL;
static Uint64 g_last_autosave_tick_ms = 0;

bool bScrollLock;

ZTConf zt_config_globals;
char* zt_directory;
int postAction ();
int preAction ();

bool bIsFullscreen = false;
int pe_modification = 2;
Drawable * pe_buf = NULL;

int need_popup_refresh = 0;

int select_row_start,select_row_end;
int light_pos = 0, need_update_lights = 0;

char errstr[2048] ;

player *ztPlayer;

UserInterface *UI = NULL ;
UserInterface *InstEditorUI = NULL;



/*
ResourceStream *Skin = NULL;
BitmapCache *BM_Cache = NULL;
*/

char *cur_dir = NULL;

int file_changed = 0;
char *file_directory;
int modal=0,sel_pat = 0x100,sel_order = 0x0;;
int LastX=0,LastY=0,MousePressX=0,MousePressY=0,MouseBttnL=0,MouseBttnR=0;

unsigned int last_cmd_keyjazz=0,last_keyjazz=0;

int fast_update=0, FU_x=0, FU_y=0, FU_dx=0, FU_dy=0;

int Force_CON_Reactivated=0;
int select_track_start,select_track_end;
int selected=0;

int zclear_flag, zclear_presscount;
int already_changed_default_directory = 0;
char ls_filename[MAX_PATH + 1];

//int default_midistopstart = 1;
//int default_midiclock = 1;


int updated=1;
int cur_state,need_refresh=1,need_update=0;
int cur_volume_percent = 100;
int cur_naturalization_percent = 0;
int do_exit = 0;
int editing = 0;
int cur_edit_order = 0;
int cur_edit_track = 0;
int cur_edit_col   = 0;
int cur_edit_row   = 0;
int cur_edit_row_disp = 0;
int cur_edit_pattern = 0;
int cur_edit_track_disp = 0;
int cur_edit_column = 0;

bool bMouseIsDown = false;

int cur_inst = 0;
    int fixmouse=0;

const char *col_desc[41];

int base_octave = BASE_OCTAVE_DEFAULT ;
int cur_step = DEFAULT_CURSOR_STEP ;

int keypress=0;
int keywait = 0;
zt_timer_handle keytimer = 0;
unsigned int keyID = 0;
static unsigned int keyCode = 0;

int status_change = 0;

KeyBuffer Keys;
//conf Config;//,ColorsFile;

int clear_popup = 0;

edit_col edit_cols[41];

//int BeatsPerMinute = 138;
//int TicksPerBeat   = 8; // Rows per beat;

int key_jazz = 0x0;
static std::mutex g_jazz_mutex;
static std::unordered_map<int, mbuf> g_jazz;

mbuf jazz_get_state(int key) {
    std::lock_guard<std::mutex> lock(g_jazz_mutex);
    const auto it = g_jazz.find(key);
    if (it == g_jazz.end()) {
        mbuf empty = {0x80, 0};
        return empty;
    }
    return it->second;
}

bool jazz_note_is_active(int key) {
    std::lock_guard<std::mutex> lock(g_jazz_mutex);
    const auto it = g_jazz.find(key);
    return it != g_jazz.end() && it->second.note != 0x80;
}

void jazz_set_state(int key, unsigned char note, unsigned char chan) {
    std::lock_guard<std::mutex> lock(g_jazz_mutex);
    g_jazz[key].note = note;
    g_jazz[key].chan = chan;
}

void jazz_clear_state(int key) {
    std::lock_guard<std::mutex> lock(g_jazz_mutex);
    const auto it = g_jazz.find(key);
    if (it != g_jazz.end()) {
        it->second.note = 0x80;
    }
}

void jazz_clear_all_states(void) {
    std::lock_guard<std::mutex> lock(g_jazz_mutex);
    g_jazz.clear();
}

int attempt_fullscreen_toggle();
static void zt_request_ui_full_refresh(void);
static void zt_autosave_tick(void);
static int zt_autosave_now(void);
static std::string zt_make_autosave_filename(void);
static void zt_prune_autosaves(size_t keep_count);

/*
Bitmap *ZTLOGO = NULL;
#ifdef USE_BG_IMAGE
    Bitmap *BG_IMAGE = NULL;
#endif
Bitmap *TOOLBAR = NULL;
Bitmap *MOUSEBACK = NULL;

Bitmap *VS = NULL;
*/

//char *fontfile;
//char *ztlogofile;
//char *zttoolbarfile;
//char *skinfile;
//char *colorfile;

const char *statusmsg = " ";
char szStatmsg[1024];

//unsigned long numMidiDevs;

UserInterface *UI_Toolbar = NULL;
Bitmap *bmToolbarRepeater = NULL;

event blank_event;
instrument blank_instrument(0);

zt_module *song;
char zt_filename[1024];
//pattern patterns[1];

//Screen *S;
//Screen *RealScreen = NULL;
int doredraw=0,colors=0;

CClipboard *clipboard;
WStack window_stack;

bool bDontKeyRepeat=false;

midiOut *MidiOut = NULL;
midiIn *MidiIn = NULL;

CUI_Page *ActivePage = NULL, *LastPage = NULL, *PopupWindow = NULL;

CUI_About *UIP_About = NULL;
CUI_MainMenu *UIP_MainMenu = NULL;
CUI_InstEditor *UIP_InstEditor = NULL;
CUI_Logoscreen *UIP_Logoscreen = NULL;
CUI_Loadscreen *UIP_Loadscreen = NULL;
CUI_Savescreen  *UIP_Savescreen = NULL;
CUI_Ordereditor *UIP_Ordereditor = NULL;
CUI_Playsong *UIP_Playsong = NULL;
CUI_Songconfig *UIP_Songconfig = NULL;
CUI_Sysconfig *UIP_Sysconfig = NULL;
CUI_PaletteEditor *UIP_PaletteEditor = NULL;
CUI_Config *UIP_Config = NULL;
CUI_Patterneditor *UIP_Patterneditor = NULL;
CUI_PEParms *UIP_PEParms = NULL;
CUI_PEVol *UIP_PEVol = NULL;
CUI_PENature *UIP_PENature = NULL;
CUI_SliderInput *UIP_SliderInput = NULL;
CUI_LoadMsg *UIP_LoadMsg = NULL;
CUI_SaveMsg *UIP_SaveMsg = NULL;
CUI_NewSong *UIP_NewSong = NULL;
CUI_RUSure *UIP_RUSure = NULL;
CUI_Help *UIP_Help = NULL;
CUI_SongDuration *UIP_SongDuration = NULL;
CUI_SongMessage *UIP_SongMessage = NULL;
CUI_Arpeggioeditor *UIP_Arpeggioeditor = NULL;
CUI_Midimacroeditor *UIP_Midimacroeditor = NULL;
CUI_LuaConsole *UIP_LuaConsole = NULL;
CUI_KeyBindings *UIP_KeyBindings = NULL;
CUI_CcConsole *UIP_CcConsole = NULL;
int g_cc_drawmode = 0;



SDL_Surface *screen_buffer_surface ;

Screen *screen_buffer = NULL ;

int set_video_mode(int w, int h, char *errstr) ;

// ------------------------------------------------------------------------------------------------
//
//
int check_ext(const char *str, const char *ext)
{
    int i,j,k=0;
    i=strlen(str); j=strlen(ext);
    i-=j;
    if (i<0) return 0;
    while(str[i] && ext[k]) {
        if (tolower(str[i]) != tolower(ext[k]))
            return 0;
        i++; k++;
    }
    if (k==j)
        return 1;
    else
        return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void reset_editor(void)
{
    cur_edit_col = cur_edit_column = cur_edit_row = cur_edit_pattern = 0;
    cur_edit_row_disp = cur_edit_pattern = cur_edit_track = cur_edit_track_disp = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
int lock_mutex(zt_mutex_handle hMutex, int timeout)
{
    return zt_mutex_lock(hMutex, timeout);
}




// ------------------------------------------------------------------------------------------------
//
//
int unlock_mutex(zt_mutex_handle hMutex)
{
    return zt_mutex_unlock(hMutex);
}



// ------------------------------------------------------------------------------------------------
//
//
void popup_window(CUI_Page *page)
{
    if (page->UI)
        page->UI->full_refresh();
    window_stack.push(page);
    screenmanager.UpdateAll();
    modal++;
}



// ------------------------------------------------------------------------------------------------
//
//
void close_popup_window(void)
{
    if (!window_stack.isempty()) {
        window_stack.pop();
        clear_popup = 1;
        modal--;
    }
    if (ActivePage->UI)
        ActivePage->UI->full_refresh();
//    screenmanager.UpdateAll();
    doredraw++;
    need_refresh++;
}



// ------------------------------------------------------------------------------------------------
//
//
void switch_page(CUI_Page *page)
{
    LastPage = ActivePage;
    if (LastPage)
        LastPage->leave();
    ActivePage = page;
    ActivePage->enter();
    if (ActivePage->UI)
        ActivePage->UI->full_refresh();
    // Clear any stale status message from the previous page so it
    // doesn't bleed onto the newly activated page — but only when
    // we're stopped. While playing, the playhead status line should
    // carry through page changes; otherwise it briefly blanks and
    // flashes back in on every F1/F2/F3/etc switch.
    if (!ztPlayer || !ztPlayer->playing) {
        statusmsg = (char*)" ";
    }


  // <Manu> TO-DO Check if still needed, but not problematic if not
  // Force full screen clear to prevent previous page from bleeding through (#17).
    if (screen_buffer)
        screen_buffer->fillRect(0, 0, INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y, COLORS.Background);


    screenmanager.UpdateAll();
    doredraw++;
    need_refresh++;
}




// ------------------------------------------------------------------------------------------------
//
//
int zcmp(const char *s1, const char *s2)
{
    int i=0;
    if (!s1 || !s2) return 0;
    while (s1[i]!=0 && s2[i]!=0)
        if (s1[i] != s2[i])
            return 0;
        else
            i++;
    return -1;
}





// ------------------------------------------------------------------------------------------------
//
//
int zcmpi(const char *s1, const char *s2)
{
    int i=0;
    if (!s1 || !s2) return 0;
    while (s1[i]!=0 && s2[i]!=0)
        if (tolower(s1[i]) != tolower(s2[i]))
            return 0;
        else
            i++;
    return -1;
}





// ------------------------------------------------------------------------------------------------
//
//
int checkclick(int x1, int y1, int x2, int y2)
{
    if (MousePressX>=x1 && MousePressX<=x2 &&
        MousePressY>=y1 && MousePressY<=y2
       )
       return 1;
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
int checkmousepos(int x1, int y1, int x2, int y2)
{
    if (LastX>=x1 && LastX<=x2 &&
        LastY>=y1 && LastY<=y2
       )
       return 1;
    return 0;
}



// ------------------------------------------------------------------------------------------------
//
//
Bitmap *load_cached_bitmap(char *name)
{
    (void)name;
    return NULL;
}



// ------------------------------------------------------------------------------------------------
//
//
Bitmap *load_bitmap(char *name)
{
    (void)name;
    return NULL;
}



// ------------------------------------------------------------------------------------------------
//
//
WStackNode::WStackNode(CUI_Page *p)
{
    page = p;
    p->enter();
    next = NULL;
}




// ------------------------------------------------------------------------------------------------
//
//
WStackNode::~WStackNode()
{
    page->leave();
}




// ------------------------------------------------------------------------------------------------
//
//
WStack::WStack()
{
    head = NULL;
}




// ------------------------------------------------------------------------------------------------
//
//
WStack::~WStack()
{
    WStackNode *t;
    if (!UI) return; // UI Gets deteled before UIP_*, so safe to check for that
    while(head) {
        t = head->next;
        delete head;
        head = t;
    }
}




// ------------------------------------------------------------------------------------------------
//
//
bool WStack::isempty(void)
{
    return (head == NULL);
}

// ------------------------------------------------------------------------------------------------
//
CUI_Page *WStack::top(void)
{
    if (!head) return NULL;
    WStackNode *p = head;
    while (p->next) p = p->next;
    return p->page;
}




// ------------------------------------------------------------------------------------------------
//
//
void WStack::push(CUI_Page *p)
{
    WStackNode *t = new WStackNode(p);
    if (head) {
        WStackNode *p=head;
        while(p->next) p = p->next;
        p->next = t;
    } else {
        head = t;
    }
}



// ------------------------------------------------------------------------------------------------
//
//
CUI_Page *WStack::pop(void)
{
    WStackNode *p = head;
    CUI_Page *c = NULL;
    if (head) {
        if (!p->next) {
            c = p->page;
            delete p;
            head = NULL;
        } else {
            while(p->next->next) p = p->next;
            c = p->next->page;
            delete p->next;
            p->next = NULL;
        }
    }
    return c;
}






// ------------------------------------------------------------------------------------------------
//
//
void WStack::update(void)
{
    WStackNode *p = head;
    if (head) {
        if (!p->next) {
            p = head;
        } else {
            while(p->next) p = p->next;
        }
        p->page->update();
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void WStack::draw(Drawable *S)
{
    if (!head) return;
    WStackNode *p = head;
    while(p) {
        p->page->draw(S);
        p = p->next;
    }
}





/* Clipboard */

// ------------------------------------------------------------------------------------------------
//
//
CClipboard::CClipboard()
{
    int i;
    for (i=0;i<MAX_TRACKS;i++) {
        this->event_list[i] = NULL;
    }
    tracks = rows = full = 0;
}






// ------------------------------------------------------------------------------------------------
//
//
CClipboard::~CClipboard()
{
    this->clear();
}




// ------------------------------------------------------------------------------------------------
//
//
void CClipboard::copy(void)
{
    int i;
    event *h,*t;
    this->clear();
    if (!selected) return;
    tracks = select_track_end - select_track_start;
    if ( (select_track_start == 0) &&
         (select_track_end == MAX_TRACKS-1) &&
         (select_row_start == 0) &&
         (select_row_end == song->patterns[cur_edit_pattern]->length-1) )
         this->full = 1;
    else
        full = 0;

    if (tracks>=0) tracks++; else tracks = 0;
    rows = select_row_end - select_row_start;
    if (rows>=0) rows++; else rows = 0;
    for(i=select_track_start;i<=select_track_end;i++) {
        h = song->patterns[cur_edit_pattern]->tracks[i]->event_list;
        while(h) {
            if (h->row>=select_row_start && h->row<=select_row_end) {
                t = new event(h);
                if (!t) {
                    SDL_Delay(1);
                }
                if (i-select_track_start <0 || i-select_track_start>=MAX_TRACKS) {
                    SDL_Delay(1);
                }
                t->next_event = this->event_list[i-select_track_start];
                t->row -= select_row_start;
                this->event_list[i-select_track_start] = t;
            }
            h = h->next_event;
        }
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void CClipboard::paste(int start_track, int start_row, int mode)
{  // 0 = insert, 1 = overwrite, 2 = merge



    int i,j,target_row;
    event *h;//,*t;

    file_changed++;

    if (start_track == 0 && start_row == 0 && this->full==1)
        song->patterns[cur_edit_pattern]->resize(rows);

    if (mode == 0) // Push em down
        for(i=start_track;i<start_track+tracks && i < MAX_TRACKS;i++)
            for (j=start_row;j<start_row+rows;j++)
                song->patterns[cur_edit_pattern]->tracks[i]->ins_row(j);

    if (mode == 1)  // Clear the space
        for(i=start_track;i<start_track+tracks && i< MAX_TRACKS;i++)
            for(j=start_row;j<start_row+rows && j<song->patterns[cur_edit_pattern]->length ;j++)
                song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,0x80,MAX_INSTS,0x80,0x0,0xff,0x0);

    j = start_track;
    for (i=0;i<MAX_TRACKS;i++) {
        h = this->event_list[i];
        while(h) {
            target_row = h->row + start_row;
            if (j<MAX_TRACKS  &&  target_row<song->patterns[cur_edit_pattern]->length) {
                song->patterns[cur_edit_pattern]->tracks[j]->update_event(target_row,h);
            }
            h = h->next_event;
        }
        j++;
    }
}




// ------------------------------------------------------------------------------------------------
//
//
void CClipboard::clear(void)
{
    int i;
    event *head,*t;
    for (i=0;i<MAX_TRACKS;i++) {
        head = event_list[i];
        while (head) {
            t = head->next_event;
            delete head;
            head = t;
        }
        event_list[i] = NULL;
    }
}


/* End Clipboard */

/*
int loadconf(void) {
    char * temp;


    return 1;
}
*/




// ------------------------------------------------------------------------------------------------
//
//
char *hex2note(char *str,unsigned char note)
{   /* Thanks to FSM for great ideas!! */

    char szLetters[] = "C-C#D-D#E-F-F#G-G#A-A#B-";

    if (note<0x80) {
        str[0]=szLetters[2*(note%12)];
        str[1]=szLetters[2*(note%12)+1];
        str[2]='0'+(note/12);
    }
    else {

        switch(note) {

            case 0x81:
                str[0]=str[1]=str[2]='^'; /* Note cut */
                break;
            case 0x82:
                str[0]=str[1]=str[2]='='; /* Note off */
                break;

            default:                      /* Unknown or blank */
                str[0]=str[1]=str[2]='.';

//                str[0]='B' ;
//                str[1]='U' ;
//                str[2]='G' ;
                break;
        }
    }
    return str;
}




// ------------------------------------------------------------------------------------------------
//
//
void status(const char *msg,Drawable *S)
{
    // Wipe the full status row up to the right screen edge before
    // drawing the new message. Hardcoded 76-space string left a stale
    // strip at the right when a long Playing/Looping line shrank to
    // "Stopped" on a window wider than 640px.
    const int max_cols = INTERNAL_RESOLUTION_X / FONT_SIZE_X;
    char wipe[256];
    int n = max_cols - 3;
    if (n < 0) n = 0;
    if (n > (int)sizeof(wipe) - 1) n = sizeof(wipe) - 1;
    memset(wipe, ' ', n);
    wipe[n] = '\0';
    printBG(col(3),row(INITIAL_ROW + 6),wipe,COLORS.Text,COLORS.Background,S);
    printBGCC(col(3),row(INITIAL_ROW + 6),msg,COLORS.Text,COLORS.Background,S);
    screenmanager.Update(col(3),row(INITIAL_ROW + 6),col(max_cols)-1,row(10));
}



// ------------------------------------------------------------------------------------------------
//
//
void status(Drawable *S)
{
    status(statusmsg,S);
}


extern int max_displayable_rows ;
extern int g_posx_tracks ;


// ------------------------------------------------------------------------------------------------
//
//
void update_status(Drawable *S)
{
  int i,o=0;
  char str[10];

  // Refresh the playing/looping status line on every page so the user
  // can always see where playback is, regardless of which view they're
  // currently editing in.
  if (ztPlayer->playing) {

    char time[128],time2[64];
    // Elapsed time: wall-clock since play() started. This is immune to
    // row-quantization (calcSongMs jumps by tpb*bpm row chunks, which
    // skips intermediate tenths — at BPM=120 TPB=4 the .4 tenth never
    // shows because rows hit 0.375 and 0.500 with no value in between).
    int ms = (int)((uint64_t)SDL_GetTicks() - ztPlayer->playback_start_ms);
    int sec = ms / 1000;
    int tenth = (ms % 1000) / 100;
    sprintf(time2, "|H|%.2d|U|:|H|%.2d|U|.|H|%d|U|", sec/60, sec%60, tenth);
    // Total time: still derived from song length (independent of playhead).
    ms = calcSongMs();
    sec = ms / 1000;
    tenth = (ms % 1000) / 100;
    sprintf(time, "%s/|H|%.2d|U|:|H|%.2d|U|.|H|%d|U|", time2, sec/60, sec%60, tenth);

    if (ztPlayer->playmode) {
      sprintf(szStatmsg,"Playing, Ord: |H|%.3d|U|/|H|%.3d|U|, Pat: |H|%.3d|U|/|H|255|U|, Row: |H|%.3d|U|/|H|%.3d|U|, Time: %s, BPM: |H|%d|U|, TPB: |H|%d|U|, Step: |H|%d|U|  ",ztPlayer->playing_cur_order,ztPlayer->num_real_orders,ztPlayer->playing_cur_pattern,ztPlayer->playing_cur_row,song->patterns[ztPlayer->playing_cur_pattern]->length,time,song->bpm,song->tpb,cur_step);
    }
    else {

      sprintf(szStatmsg,"Looping, Pattern: |H|%.3d|U|/|H|255|U|, Row: |H|%.3d|U|/|H|%.3d|U|, Time: %s, BPM: |H|%d|U|, TPB: |H|%d|U|, Step: |H|%d|U|  ",ztPlayer->playing_cur_pattern,ztPlayer->playing_cur_row,song->patterns[ztPlayer->playing_cur_pattern]->length,time,song->bpm,song->tpb,cur_step);
    }

    statusmsg = szStatmsg;

    // Follow Playback (Scroll Lock): in pattern editor, cursor follows
    // the playhead in real time.
    if (bScrollLock && cur_state == STATE_PEDIT) {
      cur_edit_pattern = ztPlayer->playing_cur_pattern;
      cur_edit_row     = ztPlayer->playing_cur_row;
      need_refresh++;
    }
  }

  if (S->lock() == 0) {

    status(S);

    if (cur_state == STATE_PEDIT) {

      if (cur_edit_pattern == ztPlayer->playing_cur_pattern) {

        //<Manu> Hecho as este bucle pintaba nmeros de lnea sin comprobar cuntas tena el pattern actual [EN: as written, this loop was drawing line numbers without checking how many the current pattern actually has]
//        for(i = cur_edit_row_disp; i < (cur_edit_row_disp + PATTERN_EDIT_ROWS); i++) {

        //int max_row = song->patterns[cur_edit_pattern]->length ;

        int max_row = cur_edit_row_disp + max_displayable_rows ;
        if(max_row > song->patterns[cur_edit_pattern]->length) max_row = song->patterns[cur_edit_pattern]->length ;

        for(i = cur_edit_row_disp; i < max_row; i++) {

          TColor line_number_color ;

          if (i == ztPlayer->playing_cur_row) line_number_color = COLORS.Highlight ;
          else line_number_color = COLORS.Text ;

          sprintf(str,"%.3d", i) ;
          printBG(col(g_posx_tracks - 4), row(TRACKS_ROW_Y+1+o), str, line_number_color, COLORS.Background, S) ;

          o++;
        }
      }
    }

    if (ztPlayer->playing_cur_row == 1 ) {

      if(cur_state == STATE_ORDER) need_refresh++;
      draw_status_vars(S);
    }

    status_change = 0;
    //updated++;
    screenmanager.UpdateWH(col(g_posx_tracks - 4), row(TRACKS_ROW_Y+1), 3 * FONT_SIZE_X, PATTERN_EDIT_ROWS * FONT_SIZE_Y) ;
    S->unlock();
  }
}



/*
int initConsole(int& Width, int& Height, int& FullScreen, int& Flags, Screen* S) {
    int i;
    char str[80];
    istream *is;
    cur_dir = (char *)malloc(256);
    zt_get_current_directory(256, cur_dir);
    if (!loadconf()) {
        zt_show_error("zt: error", "Fatal: Unable to load zt.conf");
        return -1;
    }
    g_keybindings.setDefaults();
    g_keybindings.load(zt_config_globals.Config);
    Skin = newResourceStream(skinfile);
    if (!Skin) {
        sprintf(str,"Fatal: Could not load skin resource: %s\n",skinfile);
        MessageBox(NULL,str,"zt: error",MB_OK | MB_ICONERROR );
        return -1;
    }
    is = Skin->getStream("font.fnt");
    if (font_load(is)) {
        sprintf(str,"Fatal: Could not load font.fnt from %s\n",skinfile);
        MessageBox(NULL,str,"zt: error",MB_OK | MB_ICONERROR );
        return -1;
    }
    Skin->freeStream(is);
    if (!S->isModeAvailable(INTERNAL_RESOLUTION_X,INTERNAL_RESOLUTION_Y)) {
        sprintf(str,"Fatal: Screen mode (%dx%dx16) is not support by your gfx card",INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y);
        zt_show_error("zt: error", str);
        return -1;
    }

    BM_Cache = new BitmapCache(*Skin);
    UI = new UserInterface;
    UI_Toolbar = new UserInterface;
    UI_Toolbar->dontmess = 1;
    init_edit_cols();
    MidiOut = new midiOut;
    MidiIn  = new midiIn;
    clipboard = new CClipboard;
    song = new zt_module(8,138);

    ztPlayer = new player(1,prebuffer_rows, song); // 1ms resolution;

    ///////////////////////////////////////////////////////////////////////
    // Here we set the highlight and lowlight according to either specific
    // zt.conf parameters or ticks per beat (default)

    if(default_highlight_increment)
        highlight_count = default_highlight_increment;
    else
        highlight_count = song->tpb;
    if(default_lowlight_increment)
        lowlight_count = default_lowlight_increment;
    else
        lowlight_count = song->tpb >> 1 / song->tpb / 2;

    ////////////////////////////////////////////////////////////////////////

    jazz_clear_all_states();

    HICON icon=(HICON)LoadImage(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ZTICON),IMAGE_ICON,0,0,0);
    setWindowIcon(icon);
    setWindowTitle("zt");
    Width=INTERNAL_RESOLUTION_X;
    Height=INTERNAL_RESOLUTION_Y;

    if (zcmp(Config.get("fullscreen"),"yes")) {
        GUIMODE = 0;
        FullScreen=1;
        FULLSCREEN=1;
        Flags=SingleBuffer;
    } else {
        GUIMODE = 1;
        FullScreen=0;
        FULLSCREEN=0;
        Flags=WindowsFrame+SingleBuffer;
    }
    Flags |= NoSound; // Do not allocate audio

    need_refresh = 1;

    UIP_About = new CUI_About;
    UIP_InstEditor = new CUI_InstEditor;
    UIP_Loadscreen = new CUI_Loadscreen;
    UIP_Logoscreen = new CUI_Logoscreen;
    UIP_Savescreen = new CUI_Savescreen;
    UIP_LoadMsg = new CUI_LoadMsg;
    UIP_SaveMsg = new CUI_SaveMsg;
    UIP_Ordereditor = new CUI_Ordereditor;
    UIP_Playsong = new CUI_Playsong;
    UIP_Songconfig = new CUI_Songconfig;
    UIP_Sysconfig = new CUI_Sysconfig;
    UIP_PaletteEditor = new CUI_PaletteEditor;
    UIP_Config = new CUI_Config;
    UIP_KeyBindings = new CUI_KeyBindings;
    UIP_CcConsole = new CUI_CcConsole;
    UIP_Patterneditor = new CUI_Patterneditor;
    UIP_PEParms = new CUI_PEParms;
    UIP_PEVol = new CUI_PEVol;
    UIP_SliderInput = new CUI_SliderInput;
    UIP_NewSong = new CUI_NewSong;
    UIP_RUSure = new CUI_RUSure;
    UIP_Help = new CUI_Help;



    setPreAction(preAction);
    setPostAction(postAction);

    return 0;
}
*/




// ------------------------------------------------------------------------------------------------
//
//
void draw_status_vars(Drawable *S)
{
  char s[64];

  int posx_columna2 = (INTERNAL_RESOLUTION_X / FONT_SIZE_X) - 30 ;


  sprintf(s,"%.2d",song->instruments[cur_inst]->channel+1);
  printBG(col(posx_columna2),row(INITIAL_ROW + 2),s,COLORS.Data,COLORS.Black,S); // Channel

  sprintf(s,"%.1d",base_octave);
  printBG(col(posx_columna2),row(INITIAL_ROW + 3),s,COLORS.Data,COLORS.Black,S); // Octave

  sprintf(s,"%.3d/255",cur_edit_order);
  printBG(col(12),row(INITIAL_ROW + 2),s,COLORS.Data,COLORS.Black,S); // ORDER

  sprintf(s,"%.3d/255",cur_edit_pattern);
  printBG(col(12),row(INITIAL_ROW + 3),s,COLORS.Data,COLORS.Black,S); // PATTERN

  sprintf(s,"%.3d/%.3d",cur_edit_row,song->patterns[cur_edit_pattern]->length-1);
  printBG(col(12),row(INITIAL_ROW + 4),s,COLORS.Data,COLORS.Black,S); // ROW

  sprintf(s,"%.3d/%.3d",song->tpb,song->bpm);

  if (ztPlayer) {
    if (ztPlayer->playing) {
      sprintf(s,"%.3d/%.3d",ztPlayer->tpb,ztPlayer->bpm);
    }
  }

  printBG(col(posx_columna2),row(INITIAL_ROW + 1),s,COLORS.Data,COLORS.Black,S); // ORDER

  sprintf(s,"%.2d:                       ",cur_inst);
  printBG(col(posx_columna2),row(INITIAL_ROW),s,COLORS.Data,COLORS.Black,S); // ORDER

  sprintf(s,"%.2d: %.24s",cur_inst,song->instruments[cur_inst]->title);
  printBG(col(posx_columna2),row(INITIAL_ROW),s,COLORS.Data,COLORS.Black,S); // ORDER

  screenmanager.Update( col(10), row(INITIAL_ROW - 1), col(79), row(10) );

}



// ------------------------------------------------------------------------------------------------
//
//
void draw_status(Drawable *S)
{
    /* S MUST BE LOCKED! */
    char fn[256] ;
    int i ;

#ifdef USE_BG_IMAGE
    S->unlock();
    S->copy(BG_IMAGE,0,0);
    S->lock();
#endif
    print(col(2),row(INITIAL_ROW),    "Song Name",COLORS.Text,S);
    print(col(2),row(INITIAL_ROW + 1),"File Name",COLORS.Text,S);
    print(col(2),row(INITIAL_ROW + 2),"    Order",COLORS.Text,S);
    print(col(2),row(INITIAL_ROW + 3),"  Pattern",COLORS.Text,S);
    print(col(2),row(INITIAL_ROW + 4),"      Row",COLORS.Text,S);


    int posx_columna2 = (INTERNAL_RESOLUTION_X / FONT_SIZE_X) - 30 - 12 ;

    print(col(posx_columna2),row(INITIAL_ROW),    " Instrument",COLORS.Text,S);
    print(col(posx_columna2),row(INITIAL_ROW + 1),"    TPB/BPM",COLORS.Text,S);
    print(col(posx_columna2),row(INITIAL_ROW + 2),"    Channel",COLORS.Text,S);
    print(col(posx_columna2),row(INITIAL_ROW + 3),"     Octave",COLORS.Text,S);

    printline(col(12),row(INITIAL_ROW - 1),0x86,25,COLORS.Lowlight,S);
    printline(col(19),row(INITIAL_ROW + 2),0x81,18,COLORS.Highlight,S);
    printBG(col(12),row(INITIAL_ROW),"                         ",COLORS.Data,COLORS.Black,S);
    printBG(col(12),row(INITIAL_ROW),(char *)song->title,COLORS.Data,COLORS.Black,S);
    printBG(col(12),row(INITIAL_ROW + 1),"                         ",COLORS.Data,COLORS.Black,S);
    strcpy(fn,(const char *)song->filename);
    fn[25] = 0;
    printBG(col(12),row(INITIAL_ROW + 1),(char *)fn,COLORS.Data,COLORS.Black,S);

    draw_status_vars(S);

//  sprintf(s,"%.2d: %.24s",cur_inst,song->instruments[cur_inst]->title);
//  printBG(col(50),row(3),s,COLORS.Data,COLORS.Black,S); // ORDER

    printline(col(12),row(INITIAL_ROW + 5),0x81,7,COLORS.Highlight,S);

    for (i=INITIAL_ROW;i<=(INITIAL_ROW + 4);i++)
        printchar(col(11),row(i),0x84,COLORS.Lowlight,S);
    for (i=(INITIAL_ROW + 2);i<=(INITIAL_ROW + 4);i++)
        printchar(col(19),row(i),0x83,COLORS.Highlight,S);
    for (i=INITIAL_ROW;i<=(INITIAL_ROW + 1);i++)
        printchar(col(37),row(i),0x83,COLORS.Highlight,S);




    posx_columna2 += 12 ;

    printline(col(posx_columna2),row(INITIAL_ROW - 1),0x86,28,COLORS.Lowlight,S);         // Over inst

    printchar(col(posx_columna2 + 28),row(INITIAL_ROW),0x83,COLORS.Highlight,S); // end of inst

    printchar(col(posx_columna2 + 7),row(INITIAL_ROW + 1),0x83,COLORS.Highlight,S);
    printchar(col(posx_columna2 + 2),row(INITIAL_ROW + 2),0x83,COLORS.Highlight,S);
    printchar(col(posx_columna2 + 1),row(INITIAL_ROW + 3),0x83,COLORS.Highlight,S);
    printchar(col(posx_columna2 + 1),row(INITIAL_ROW + 3),0x81,COLORS.Highlight,S);

    printchar(col(posx_columna2),row(INITIAL_ROW + 4),0x81,COLORS.Highlight,S);

    printline(col(posx_columna2 + 7),row(INITIAL_ROW + 1),0x81,21,COLORS.Highlight,S);       // Under inst

    printline(col(posx_columna2 + 2),row(INITIAL_ROW + 2),0x81,5,COLORS.Highlight,S);

    for (i=0;i<4;i++)
        printchar(col(posx_columna2 - 1),row(INITIAL_ROW + i),0x84,COLORS.Lowlight,S);

    /*
    {
      char texto_debug[] = "012345678910        20        30        40        50        60        70        80        90       100       110       120" ;
      int resx_chars = INTERNAL_RESOLUTION_X / FONT_SIZE_X ;
      texto_debug[resx_chars] = '\0' ;

      print(col(0),row(0),texto_debug,COLORS.Text,S);
    }
    */

    screenmanager.Update( col(10), row(INITIAL_ROW - 1), col(INTERNAL_RESOLUTION_X / FONT_SIZE_X) - 2, row(INITIAL_ROW + 7) );
}



// ------------------------------------------------------------------------------------------------
//
//
void doquit() {
  do_exit = 1;
}




// ------------------------------------------------------------------------------------------------
//
//
void quit() {
  UIP_RUSure->str = " Sure to quit?";
  UIP_RUSure->OnYes = (VFunc)doquit;
  popup_window(UIP_RUSure);
}








// ------------------------------------------------------------------------------------------------
//
//
void global_keys(Drawable *S)
{
    KBKey key,clear=0,kstate;
    int command = CMD_NONE,full=0;
    key = Keys.checkkey();
    kstate = Keys.getstate();

    if (!key) return;

    if (!modal) {
        // ESC opens the main menu (Schism-style overlay) when no
        // popup is already up. Pages that want to consume ESC for
        // their own purposes (Help / About return-to-pattern, etc.)
        // are reached AFTER global_keys, so they still get to see
        // ESC if the menu doesn't open it (we only fire if the user
        // is not on a page where ESC has dedicated meaning).
        if (key == SDLK_ESCAPE
            && kstate == KS_NO_SHIFT_KEYS
            && window_stack.isempty()
            && cur_state != STATE_HELP
            && cur_state != STATE_ABOUT
            && cur_state != STATE_LUA_CONSOLE
            && cur_state != STATE_SONG_MESSAGE
            && UIP_MainMenu) {
            (void)Keys.getkey();
            popup_window(UIP_MainMenu);
            need_refresh++;
            return;
        }
        switch(key) {
            case SDLK_RIGHT:
                if (key==SDLK_RIGHT && kstate == KS_CTRL)
                    if (ztPlayer->playing)
                        ztPlayer->ffwd();
                break;
            case SDLK_LEFT:
                if (key==SDLK_LEFT && kstate == KS_CTRL)
                    if (ztPlayer->playing)
                        ztPlayer->rewind();
                break;

            case SDLK_RETURN:
                if (kstate & KS_ALT) {
                    (void)attempt_fullscreen_toggle();
                    (void)Keys.getkey();
                    Keys.flush();
                    midiInQueue.clear();
                    if (ActivePage->UI)
                        ActivePage->UI->full_refresh();
                    screenmanager.UpdateAll();
                    need_refresh++;
                    doredraw++;
                    return;
                }
                break;
            case SDLK_SCROLLLOCK:
                if (bScrollLock)
                    bScrollLock = 0;
                else
                    bScrollLock = 1;
                statusmsg = (char*)(bScrollLock ? "Follow playback ON" : "Follow playback OFF");
                status_change = 1; need_refresh++;
                break;
            case SDLK_Q:
                // Keybindings editor: let the page receive the keypress
                // so the user can bind it (Ctrl-Q, Shift-Ctrl-Alt-Q,
                // etc.) instead of triggering the global quit.
                if (cur_state == STATE_KEYBINDINGS) break;
                // Plain Ctrl-Q quits, same as the long-form Ctrl-Alt-Q.
                // (Cmd-Q on macOS is reserved for the pattern-editor
                // Transpose-up shortcut via KS_HAS_ALT, so we don't bind
                // Cmd-Q here.)
                if (kstate & KS_CTRL) {
                    command=CMD_QUIT;
                    key = Keys.getkey();
                }
                break;
            case SDLK_M:
                // Ctrl-M = open Midimacro editor (the obvious-name
                // shortcut; works regardless of macOS F-key mode).
                if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT)) {
                    command = CMD_SWITCH_MIDIMACEDIT;
                    key = Keys.getkey();
                    break;
                }
                // Ctrl-Shift-M = quick MIDI export to .mid file.
                if ((kstate & KS_CTRL) && (kstate & KS_SHIFT)) {
                    // Export current song as .mid file
                    char midfn[512];
                    if (song->filename[0]) {
                        strncpy(midfn, (const char*)song->filename, 500);
                        midfn[500] = 0;
                        // Replace extension with .mid
                        char *dot = strrchr(midfn, '.');
                        if (dot) strcpy(dot, ".mid");
                        else strcat(midfn, ".mid");
                    } else {
                        strcpy(midfn, "export.mid");
                    }
                    ZTImportExport zie;
                    if (zie.ExportMID(midfn, 1) == 0) {
                        sprintf(szStatmsg, "Exported MIDI: %s", midfn);
                    } else {
                        sprintf(szStatmsg, "MIDI export failed");
                    }
                    statusmsg = szStatmsg;
                    status_change = 1;
                    need_refresh++;
                    key = Keys.getkey();
                }
                break;
            case SDLK_N: // new song
                if (kstate & KS_ALT) {
                    popup_window(UIP_NewSong);
                    key = Keys.getkey();
                    clear++;
                }
                break;
            case SDLK_P: // song duration
                if (kstate & KS_ALT) {
                    popup_window(UIP_SongDuration);
                    key = Keys.getkey();
                    clear++;
                }
                break;
            case SDLK_F7: // Play from Order
                if (kstate & KS_SHIFT) {
                    command = CMD_PLAY_ORDER;
                }
                break;

            case SDLK_F2: // Shift+F2 = Shortcuts & MIDI Mappings page
                // Plain F2 opens the Pattern Editor (handled in the
                // no-modifier block below). Shift+F2 lands on the
                // unified Shortcuts & MIDI Mappings page so the
                // user can see and edit BOTH the keyboard binding
                // and the MIDI bindings for every action in one
                // place. (Moved from Shift+F3 to free that slot for
                // the CC Console page.)
                if (kstate & KS_SHIFT) {
                    command = CMD_SWITCH_KEYBINDINGS;
                    key = Keys.getkey();
                }
                break;

            case SDLK_F3: // Shift+F3 = CC Console page
                // Plain F3 opens the Instrument editor (handled in
                // the no-modifier block below). Shift+F3 opens the
                // CC Console — load CCizer-format files and send
                // MIDI CCs out via on-screen sliders / knobs.
                if (kstate & KS_SHIFT) {
                    command = CMD_SWITCH_CCCONSOLE;
                    key = Keys.getkey();
                }
                break;


            case SDLK_F4:
                // Shift+F4 opens the Arpeggio editor. Plain F4 (handled
                // in the no-modifier block below) opens the MIDI Macro
                // editor — the more commonly used of the two. Shift+
                // Ctrl/Cmd/Alt+F4 also work as long as Shift is held,
                // since Shift+F4 sidesteps every OS-level F-key
                // shortcut (macOS Cmd-F4 zoom, Linux/Win Alt-F4
                // close-window).
                if (kstate & KS_SHIFT) {
                    command=CMD_SWITCH_ARPEDIT;
                }
                break;


            //case SDLK_F9: // load
            case SDLK_L: // load (Ctrl+L) or Lua console (Ctrl+Alt+L)
                if ((kstate & KS_CTRL) && (kstate & KS_ALT)) {
                    command = CMD_SWITCH_LUA_CONSOLE;
                    key = Keys.getkey();
                } else if (kstate & KS_CTRL) {
                    command = CMD_SWITCH_LOAD;
                    key = Keys.getkey();
                }
                break;

            case SDLK_K: // Keybindings editor (Ctrl+Alt+K)
                if ((kstate & KS_CTRL) && (kstate & KS_ALT)) {
                    command = CMD_SWITCH_KEYBINDINGS;
                    key = Keys.getkey();
                }
                break;

#ifndef DISABLE_UNFINISHED_F10_SONG_MESSAGE_EDITOR
            case SDLK_F10:

                if (kstate == KS_NO_SHIFT_KEYS) {

                    command = CMD_SWITCH_SONGMSG;
                }
#endif

              break ;

            case SDLK_S: {
                // IMPORTANT: this is Ctrl-S only. Cmd-S (which is
                // KS_META | KS_ALT on macOS) is reserved for the
                // Pattern Editor's "Set Instrument on selection"
                // shortcut (CUI_Patterneditor.cpp gated on
                // KS_HAS_ALT). Don't widen this check to KS_META --
                // doing so steals Cmd-S from that shortcut.
                //
                // Decision split into a pure function (save_key_dispatch.h)
                // so the per-page rules (Keybindings page handles it
                // itself; F4/Shift+F4 swallow it; everything else opens
                // Save / Save-As) can be unit-tested.
                SaveKeyContext c;
                c.kstate_ctrl          = (kstate & KS_CTRL)  != 0;
                c.kstate_shift         = (kstate & KS_SHIFT) != 0;
                c.kstate_has_alt       = KS_HAS_ALT(kstate);
                c.is_keybindings_state = (cur_state == STATE_KEYBINDINGS);
                c.is_macroedit_state   = (cur_state == STATE_MIDIMACEDIT);
                c.is_arpedit_state     = (cur_state == STATE_ARPEDIT);
                c.song_has_filename    = (song->filename[0] != '\0' &&
                                          song->filename[0] != ' ');
                switch (dispatch_save_key(c)) {
                    case SAVE_KEY_PASS_THROUGH:
                    case SAVE_KEY_LET_PAGE_HANDLE:
                        break;
                    case SAVE_KEY_SWALLOW:
                        // Drain the buffer to avoid the "stuck Ctrl-S
                        // freezes the page" bug.
                        (void)Keys.getkey();
                        return;
                    case SAVE_KEY_OPEN_SAVE_POPUP:
                        popup_window(UIP_SaveMsg);
                        (void)Keys.getkey();
                        clear++;
                        break;
                    case SAVE_KEY_OPEN_SAVE_AS:
                        command = CMD_SWITCH_SAVE;
                        (void)Keys.getkey();
                        clear++;
                        break;
                }
                break;
            }



            case SDLK_F11: // Set Current Order to Playing order or current pattern
                if(kstate & KS_SHIFT) {
                    if(ztPlayer->playing) {
                        cur_edit_order = ztPlayer->cur_order;
                    }
                    else {

                        int i ;

                        for(i = 0; i < ZTM_ORDERLIST_LEN; i++) {
                            if(song->orderlist[i] == cur_edit_pattern) {
                                i = -1;
                                break;
                            }
                        }

                        if (i == -1) {

                          for(i = cur_edit_order+1;; i++) {

                              if(i == cur_edit_order) break;

                              if(i == ZTM_ORDERLIST_LEN) {
                                  i = 0;
                              }

                              if(song->orderlist[i] == cur_edit_pattern) {
                                  cur_edit_order = i;
                                  draw_status_vars(S);
                                  break;
                              }
                          }
                       }
                    }
                }
                break;
            case SDLK_F12:
                if ((kstate & KS_CTRL) && (kstate & KS_SHIFT)) {
                    command = CMD_SWITCH_PALETTE;
                } else if (kstate & KS_ALT) {
                    command = CMD_SWITCH_ABOUT;
                } else if (cur_state == STATE_SYSTEM_CONFIG) {
                    command = CMD_SWITCH_CONFIG;
                } else if (cur_state == STATE_CONFIG) {
                    command = CMD_SWITCH_SYSCONF;
                } else if (kstate & KS_CTRL) {
                    command = CMD_SWITCH_CONFIG;
                } else {
                    command = CMD_SWITCH_SYSCONF;
                }
                break;

        }



        if (kstate == KS_NO_SHIFT_KEYS) {

            switch(key)
            {
                // ----------------------------------------------
                case SDLK_F1: command=CMD_SWITCH_HELP;      break;
                // ----------------------------------------------
                case SDLK_F2: command=CMD_SWITCH_PEDIT;     break;
                // ----------------------------------------------
                case SDLK_F3: command=CMD_SWITCH_IEDIT;     break;

                // ----------------------------------------------
                // F4 from anywhere -> MIDI Macro editor.
                // F4 while already on MIDI Macro -> Arpeggio editor.
                // F4 while already on Arpeggio -> back to MIDI Macro.
                // (Shift+F4 always jumps directly to Arpeggio.)
                case SDLK_F4:
                    if (cur_state == STATE_MIDIMACEDIT)
                        command = CMD_SWITCH_ARPEDIT;
                    else if (cur_state == STATE_ARPEDIT)
                        command = CMD_SWITCH_MIDIMACEDIT;
                    else
                        command = CMD_SWITCH_MIDIMACEDIT;
                    break;
                // ----------------------------------------------
                case SDLK_F5: command = CMD_PLAY;           break;
                // ----------------------------------------------
                case SDLK_F6: command = CMD_PLAY_PAT;       break;
                // ----------------------------------------------
                case SDLK_F7: command = CMD_PLAY_PAT_LINE;  break;
                // ----------------------------------------------
                case SDLK_F8: command = CMD_STOP;           break;
                // ----------------------------------------------
                case SDLK_F9:

                    if((kstate & KS_ALT) == 0) {
                        command = CMD_PANIC;
                    }

                    break ;

                // ----------------------------------------------
                //case SDLK_F10: command=CMD_SWITCH_SONGCONF; break;
                // ----------------------------------------------
                case SDLK_F11: command=CMD_SWITCH_SONGCONF; break;

                // ----------------------------------------------
                default:
                    break ;
            }
        }
        else {

            if((kstate & KS_ALT) == 0) {

                switch(key)
                {
                    case SDLK_F9: command = CMD_HARD_PANIC;  break ;
                    default:
                        break ;
                } ;
            }

        }

    }



    switch(command)
    {
        // ------------------------------------------------------------------------
        case CMD_QUIT: quit(); break;
        case CMD_SWITCH_LOAD:
            switch_page(UIP_Loadscreen);
            clear++; doredraw++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_SAVE:
            switch_page(UIP_Savescreen);
            clear++; doredraw++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_SONGCONF:
            switch_page(UIP_Songconfig);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_SONGMSG:
            switch_page(UIP_SongMessage);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_ARPEDIT:
            switch_page(UIP_Arpeggioeditor);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_MIDIMACEDIT:
            switch_page(UIP_Midimacroeditor);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_PEDIT:
            if (cur_state == STATE_PEDIT) {
                popup_window(UIP_PEParms); clear++;
                doredraw++;
            } else {
                switch_page(UIP_Patterneditor);
                doredraw++; clear++;
            }
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_IEDIT:
            switch_page(UIP_InstEditor);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_ORDERLIST:
            switch_page(UIP_Ordereditor);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_SYSCONF:
            switch_page(UIP_Sysconfig);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_CONFIG:
            switch_page(UIP_Config);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_PALETTE:
            switch_page(UIP_PaletteEditor);
            doredraw++; clear++;
            break;

        // ------------------------------------------------------------------------
        case CMD_SWITCH_ABOUT:
            switch_page(UIP_About);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_HELP:
            if (ActivePage == UIP_Help && LastPage && LastPage != UIP_Help)
                switch_page(LastPage);
            else
                switch_page(UIP_Help);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_LUA_CONSOLE:
            switch_page(UIP_LuaConsole);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_KEYBINDINGS:
            switch_page(UIP_KeyBindings);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_SWITCH_CCCONSOLE:
            switch_page(UIP_CcConsole);
            doredraw++; clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_PLAY:
            draw_status_vars(S);
            if (cur_state != STATE_PLAY) {
                switch_page(UIP_Playsong);
                doredraw++;
                clear++;
            }
            if (song->orderlist[0] != 0x100 && !ztPlayer->playing) {
                ztPlayer->play(0,cur_edit_pattern,1);
                clear++;
            }
            break;
        // ------------------------------------------------------------------------
        case CMD_PLAY_ORDER:
            draw_status_vars(S);
            if(song->orderlist[cur_edit_order] != 0x100)
            {
                ztPlayer->play(0,cur_edit_order,3);
                clear++;
            }
            break;
        // ------------------------------------------------------------------------
        case CMD_PLAY_PAT:
            draw_status_vars(S);
            ztPlayer->play(0,cur_edit_pattern,0);
            clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_PLAY_PAT_LINE:
            if (  ((cur_state == STATE_SONG_CONFIG) || (cur_state == STATE_ORDER )) && sel_pat < 0x100) {
                ztPlayer->play(0,sel_order,3);
            } else {
                ztPlayer->play(cur_edit_row,cur_edit_pattern,2);
            }
            clear++;
            break;
        // ------------------------------------------------------------------------
        case CMD_STOP: ztPlayer->stop(); draw_status_vars(S);clear++; break;
        // ------------------------------------------------------------------------
        case CMD_PANIC: MidiOut->panic(); draw_status_vars(S); statusmsg = "Panic"; clear++; break;
        // ------------------------------------------------------------------------
        case CMD_HARD_PANIC: MidiOut->hardpanic(); draw_status_vars(S); statusmsg = "Hard Panic"; clear++; break;
        // ------------------------------------------------------------------------
        default:
            break;
    };


    if (clear) {
        Keys.flush();
        midiInQueue.clear();
        if (full)
            redrawscreen(S);
        if (S->lock()==0) {
            status(S);
            //updated=2;
            screenmanager.UpdateAll();
            S->unlock();
        }
    }
}




// <Manu> Variables para saber si se van produciendo eventos de estos tipos [EN: variables that track whether these kinds of events are occurring]
extern int mim_moredata ;
extern int mim_error ;
extern int mim_longerror ;



// ------------------------------------------------------------------------------------------------
//
//
void redrawscreen(Drawable *S)
{
  if(load_lock) return ;

  // <Manu> header era 80 y he cambiado el texto del sprintf [EN: header was 80 and I changed the sprintf text]

  static char header[256];
  //  sprintf(header,"%s ||||| DEBUG: moredata = %d || error = %d || longerror = %d", ZTRACKER_VERSION, mim_moredata, mim_error, mim_longerror) ;

    sprintf(header,"%s", ZTRACKER_VERSION) ;

    if (S->lock()==0) {

        S->fillRect(0,0,INTERNAL_RESOLUTION_X-1,INTERNAL_RESOLUTION_Y-1/*410*/,COLORS.Background);

//      S->fillRect(0,465,INTERNAL_RESOLUTION_X,479,COLORS.LCDLow);
        printline(0,0,0x81,INTERNAL_RESOLUTION_X/8,COLORS.Highlight,S);
        printchar(0,0,0x80,COLORS.Highlight,S);

        for(int y=0;y<INTERNAL_RESOLUTION_Y/8;y++)
            printchar(0,row(y),131,COLORS.Highlight,S);

        print(col(textcenter(header)),row(HEADER_ROW),header,COLORS.Text,S);
//      sprintf(header,"015/128 - C:012 N:013");
//      printLCD(col(2),row(55),header,S);
        draw_status(S);

        if (ActivePage) {
            if (ActivePage->UI) {
                ActivePage->UI->full_refresh();
//                ActivePage->UI->draw(S);
            }
//            ActivePage->draw(S);
        }
        S->unlock();
    }


    S->copy(CurrentSkin->bmToolbar, 0,INTERNAL_RESOLUTION_Y-55/*425*/,0,0,640,55);
    int remblk = (INTERNAL_RESOLUTION_X-640)/80;
    for(int cx = 0; cx<remblk;cx++)
        S->copy(bmToolbarRepeater, 640+(cx*80), INTERNAL_RESOLUTION_Y-55,0,0,80,55);
    UI_Toolbar->full_refresh();



    doredraw=0;
    //updated=2;
    screenmanager.UpdateAll();
}



// ------------------------------------------------------------------------------------------------
//
//
void make_toolbar(void)
{

    Bitmap *BUTTONS;
    GfxButton *gb;
    int id=0;

    UI_Toolbar->reset();

// This is beautiful isnt it?   I should be punished for this

#define BUTTON_X_SIZE 28
#define BUTTON_Y_SIZE 16
#define grab_buttons(gb,x,y) gb->bmDefault = newBitmap(BUTTON_X_SIZE,BUTTON_Y_SIZE,1); \
                         gb->bmDefault->copy(BUTTONS,0,0,x,y,x+BUTTON_X_SIZE,y+BUTTON_X_SIZE); \
                         gb->bmOnClick = newBitmap(BUTTON_X_SIZE,BUTTON_Y_SIZE,1); \
                         gb->bmOnClick->copy(BUTTONS,0,0,x,y+BUTTON_Y_SIZE*2 +2,x+BUTTON_X_SIZE,y+BUTTON_Y_SIZE + (BUTTON_X_SIZE*2 +2)); \
                         gb->bmOnMouseOver = newBitmap(BUTTON_X_SIZE,BUTTON_Y_SIZE,1); \
                         gb->bmOnMouseOver->copy(BUTTONS,0,0,x,y + (BUTTON_Y_SIZE*4 +4),x + BUTTON_X_SIZE,y + (BUTTON_Y_SIZE*4 +4) + BUTTON_Y_SIZE); \

#define make_button(gb,x,y,BUTTON_X, BUTTON_Y) gb->bmDefault = newBitmap(BUTTON_X,BUTTON_Y,1); \
    gb->bmDefault->copy(BUTTONS,0,0,x,y,x+BUTTON_X,y+BUTTON_X); \
    gb->bmOnClick = newBitmap(BUTTON_X,BUTTON_Y,1); \
    gb->bmOnClick->copy(BUTTONS,0,0,x,y+BUTTON_Y,x+BUTTON_X,y+BUTTON_Y + (BUTTON_Y ));

    /* Play Button */

    BUTTONS = CurrentSkin->bmButtons;//load_bitmap("buttons.bmp");

    delete bmToolbarRepeater;


#define TOOLBAR_SIZE_X               80
#define TOOLBAR_SIZE_Y               55

#define NORMAL_BUTTONS_SIZE_X        28
#define NORMAL_BUTTONS_SIZE_Y        16

#define TOOLBAR_BUTTONS_BASE_X       (INTERNAL_RESOLUTION_X  - 170)
#define TOOLBAR_BUTTONS_BASE_Y       (INTERNAL_RESOLUTION_Y - (480 - 442))


#define PLAY_BUTTON_POS_X            (TOOLBAR_BUTTONS_BASE_X + 0)
#define PLAY_BUTTON_POS_Y            (TOOLBAR_BUTTONS_BASE_Y + 0)

#define LOAD_BUTTON_POS_X            (TOOLBAR_BUTTONS_BASE_X + NORMAL_BUTTONS_SIZE_X + BUTTON_INTERSPACE)
#define LOAD_BUTTON_POS_Y            (TOOLBAR_BUTTONS_BASE_Y + 0)

#define CONF_BUTTON_POS_X            (TOOLBAR_BUTTONS_BASE_X + NORMAL_BUTTONS_SIZE_X + BUTTON_INTERSPACE + NORMAL_BUTTONS_SIZE_X + BUTTON_INTERSPACE)
#define CONF_BUTTON_POS_Y            (TOOLBAR_BUTTONS_BASE_Y + 0)


#define STOP_BUTTON_POS_X            (TOOLBAR_BUTTONS_BASE_X + 0)
#define STOP_BUTTON_POS_Y            (TOOLBAR_BUTTONS_BASE_Y + NORMAL_BUTTONS_SIZE_Y + BUTTON_INTERSPACE)

#define SAVE_BUTTON_POS_X            (TOOLBAR_BUTTONS_BASE_X + NORMAL_BUTTONS_SIZE_X + BUTTON_INTERSPACE)
#define SAVE_BUTTON_POS_Y            (TOOLBAR_BUTTONS_BASE_Y + NORMAL_BUTTONS_SIZE_Y + BUTTON_INTERSPACE)

#define EXIT_BUTTON_POS_X            (TOOLBAR_BUTTONS_BASE_X + NORMAL_BUTTONS_SIZE_X + BUTTON_INTERSPACE + NORMAL_BUTTONS_SIZE_X + BUTTON_INTERSPACE)
#define EXIT_BUTTON_POS_Y            (TOOLBAR_BUTTONS_BASE_Y + NORMAL_BUTTONS_SIZE_Y + BUTTON_INTERSPACE)


#define ABOUT_BUTTON_POS_X           (INTERNAL_RESOLUTION_X - ABOUT_BUTTON_SIZE_X)//(TOOLBAR_BUTTONS_BASE_X - ABOUT_BUTTON_SIZE_X)//(TOOLBAR_BUTTONS_BASE_X + 0)
#define ABOUT_BUTTON_POS_Y           (INTERNAL_RESOLUTION_Y - ABOUT_BUTTON_SIZE_Y)//(TOOLBAR_BUTTONS_BASE_Y + 0)

#define ABOUT_BUTTON_SIZE_X          80
#define ABOUT_BUTTON_SIZE_Y          55

#define BUTTON_INTERSPACE            2


    bmToolbarRepeater = newBitmap(TOOLBAR_SIZE_X, TOOLBAR_SIZE_Y);
    bmToolbarRepeater->copy(BUTTONS, 0, 0, 165, 0, 165+TOOLBAR_SIZE_X, TOOLBAR_SIZE_Y);


    /* Play */
    gb = new GfxButton();
    gb->x = PLAY_BUTTON_POS_X ;
    gb->y = PLAY_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;
    gb->xsize = NORMAL_BUTTONS_SIZE_X ;
    gb->ysize = NORMAL_BUTTONS_SIZE_Y ;
    grab_buttons(gb,0,0);
    gb->StuffKey = SDLK_F5;
    UI_Toolbar->add_element(gb,id++);

    /* Load */
    gb = new GfxButton();
    gb->x = LOAD_BUTTON_POS_X ;
    gb->y = LOAD_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;
    gb->xsize = NORMAL_BUTTONS_SIZE_X ;
    gb->ysize = NORMAL_BUTTONS_SIZE_Y ;
    grab_buttons(gb,29,0);
    //gb->StuffKey = SDLK_F9;
    gb->StuffKey = SDLK_L;
    gb->StuffKeyState = SDL_KMOD_CTRL;
    UI_Toolbar->add_element(gb,id++);

    /* Conf */
    gb = new GfxButton();
    gb->x = CONF_BUTTON_POS_X ;
    gb->y = CONF_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;
    gb->xsize = NORMAL_BUTTONS_SIZE_X ;
    gb->ysize = NORMAL_BUTTONS_SIZE_Y ;
    grab_buttons(gb,57,0);
    gb->StuffKey = SDLK_F11;
    // Ctrl-click jumps to System Configuration (F12) instead of Song Config.
    gb->CtrlStuffKey = SDLK_F12;
    gb->CtrlStuffKeyState = KS_NO_SHIFT_KEYS;
    UI_Toolbar->add_element(gb,id++);

    /* Stop */
    gb = new GfxButton();
    gb->x = STOP_BUTTON_POS_X ;
    gb->y = STOP_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;
    gb->xsize = NORMAL_BUTTONS_SIZE_X ;
    gb->ysize = NORMAL_BUTTONS_SIZE_Y ;
    grab_buttons(gb,0,17);
    gb->StuffKey = SDLK_F8;
    UI_Toolbar->add_element(gb,id++);

    /* Save */
    gb = new GfxButton();
    gb->x = SAVE_BUTTON_POS_X ;
    gb->y = SAVE_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;
    gb->xsize = NORMAL_BUTTONS_SIZE_X ;
    gb->ysize = NORMAL_BUTTONS_SIZE_Y ;
    grab_buttons(gb,29,17);
     gb->StuffKey = SDLK_S;         // bugfix #3, tlr
     gb->StuffKeyState = SDL_KMOD_CTRL | SDL_KMOD_SHIFT ;  // bugfix #3, tlr
    //gb->StuffKey = SDLK_F10;          // bugfix #3, tlr
    //gb->StuffKeyState = SDL_KMOD_CTRL;     // bugfix #3, tlr
    UI_Toolbar->add_element(gb,id++);

    /* Exit */
    gb = new GfxButton();
    gb->x = EXIT_BUTTON_POS_X ;
    gb->y = EXIT_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;
    gb->xsize = NORMAL_BUTTONS_SIZE_X ;
    gb->ysize = NORMAL_BUTTONS_SIZE_Y ;
    grab_buttons(gb,57,17);
    gb->StuffKey = SDLK_Q;
    gb->StuffKeyState = SDL_KMOD_CTRL | SDL_KMOD_ALT;
    UI_Toolbar->add_element(gb,id++);



    /* ZT About */

    gb = new GfxButton();

    gb->x = ABOUT_BUTTON_POS_X ;
    gb->y = ABOUT_BUTTON_POS_Y ;
    gb->auto_anchor_at_current_pos(ANCHOR_RIGHT | ANCHOR_DOWN) ;

    gb->xsize = ABOUT_BUTTON_SIZE_X ;
    gb->ysize = ABOUT_BUTTON_SIZE_Y ;

    make_button(gb,85,0,80,55)
    // gb->StuffKey = SDLK_F1;                 // bugfix #2, tlr
    // gb->StuffKeyState = KS_CTRL | KS_ALT;  // bugfix #2, tlr
    gb->StuffKey = SDLK_F12;                   // bugfix #2, tlr
    gb->StuffKeyState = SDL_KMOD_ALT;               // bugfix #2, tlr
    UI_Toolbar->add_element(gb,id++);


//    RELEASEINT(BUTTONS);

#ifdef DEBUG
    playbuff1_bg = new BarGraph();
    playbuff1_bg->x = 2; playbuff1_bg->y = 442; playbuff1_bg->xsize = 80; playbuff1_bg->ysize = 12;
    playbuff1_bg->max = 4200; playbuff1_bg->trackmaxval = 1;
    UI_Toolbar->add_element(playbuff1_bg,id++);

    playbuff2_bg = new BarGraph();
    playbuff2_bg->x = 2; playbuff2_bg->y = 456; playbuff2_bg->xsize = 80; playbuff2_bg->ysize = 12;
    playbuff2_bg->max = 4200; playbuff2_bg->trackmaxval = 1;
    UI_Toolbar->add_element(playbuff2_bg,id++);

    keybuff_bg = new BarGraph();
    keybuff_bg->x = 37; keybuff_bg->y = 470; keybuff_bg->xsize = 80-35; keybuff_bg->ysize = 9;
    keybuff_bg->max = Keys.maxsize; keybuff_bg->trackmaxval = 1;
    UI_Toolbar->add_element(keybuff_bg,id++);

#endif
    /*
    LCDDisplay *l;
    debug_display = l = new LCDDisplay;
    l->x = 16; l->y = 16;
    l->length = 20; l->str = "debug";
    UI_Toolbar->add_element(l, id++);
    */
}





// ------------------------------------------------------------------------------------------------
//
//
void encode(char *str, char w[256])
{
    char *q, *r;
    //*w = (char*)malloc(strlen(str));
    q = w;
    r = str;
    for(;*r != '\0'; r++, q++) {
        if((*r >= 'a' && *r <= 'z') || (*r >= 'A' && *r <= 'Z') || (*r >= '0' && *r <= '9')) {
            *q = *r;
        }
        else
            *q = '_';
    }
    *q = '\0';
    //return(w);
}






// ------------------------------------------------------------------------------------------------
//
//
void setup_midi()
{
    char *name, *temp, szKey[512], tt[256];
    conf DeviceConfig((char *)"devices.conf");
    conf *Config = &DeviceConfig;

//	tt = NULL;
    for (unsigned j=0;j<MidiOut->numOuputDevices;j++) {

        name = MidiOut->outputDevices[j]->szName;
//		if(tt != NULL)
//			free(tt);
		encode(name, tt);

        sprintf(szKey,"latency_%s",tt);
        temp = Config->get(&szKey[0]);
        if(temp) {
	    MidiOut->outputDevices[j]->delay_ticks = atoi(temp);
        }
        sprintf(szKey,"alias_%s",tt);
        temp = Config->get(&szKey[0]);

        // <Manu> Cuidado con esto... [EN: be careful with this]. [EN: be careful with this]
        if (MidiOut->outputDevices[j]->alias) {
            free(MidiOut->outputDevices[j]->alias);
            MidiOut->outputDevices[j]->alias = NULL;
        }
        if(temp && temp[0] != '\0') {
            MidiOut->outputDevices[j]->alias = strdup(temp);
        }
        else MidiOut->outputDevices[j]->alias = strdup("");
        sprintf(szKey,"bank_%s",tt);
        temp = Config->get(szKey);
        if(temp) {
	        if(temp[0] == 'Y' || temp[0] == 'y')
    		    ((MidiOutputDevice*)(MidiOut->outputDevices[j]))->reverse_bank_select = 1;
                else
                    ((MidiOutputDevice*)(MidiOut->outputDevices[j]))->reverse_bank_select = 0;
		}
    }
//	if(tt != NULL)
//		free(tt);

  if (zt_config_globals.auto_open_midi) {

    for (unsigned i=0;i<MAX_MIDI_OUTS;i++){

      sprintf(szKey,"open_out_device_%d",i);
      name = Config->get(szKey);

      if (name) {

        for (unsigned j=0;j<MidiOut->numOuputDevices;j++) {

          if (zcmp(MidiOut->outputDevices[j]->szName,name)) {

            MidiOut->AddDevice(j);
          }
        }
      }
    }



    for (unsigned i=0;i<MAX_MIDI_INS;i++){

      sprintf(szKey,"open_in_device_%d",i);
      name = Config->get(&szKey[0]);

      if (name) {

        for (unsigned j=0;j<MidiIn->numMidiDevs;j++) {

          if (zcmp(MidiIn->midiInDev[j]->szName,name)) {

            MidiIn->AddDevice(j);
          }
        }
      }
    }
  }
}





// ------------------------------------------------------------------------------------------------
//
//
int initGFX ()
{  // Init functions


//    TOOLBAR = load_cached_bitmap("toolbar.bmp");
#ifdef USE_BG_IMAGE
//    BG_IMAGE = load_cached_bitmap("bgimage.bmp");
#endif
    // Preload to avoid weird crashes
//    Bitmap *img = load_cached_bitmap("save.bmp");
//    img = load_cached_bitmap("load.bmp");
//    img = load_cached_bitmap("about.bmp");

//   setDefaultColors(S);

/*
    MOUSEBACK=newBitmap(12,20,0);
    VS = newBitmap(INTERNAL_RESOLUTION_X,INTERNAL_RESOLUTION_Y);
    VS->clear(0);
    MOUSEBACK->clear(0);
    LastX=C->getMouse()->getX();
    LastY=C->getMouse()->getY();
*/


    make_toolbar();

//    switch_page(UIP_Logoscreen);
    switch_page(UIP_Patterneditor) ;


//	UIP_Sysconfig = new CUI_Sysconfig;
    return 0;
}



// ------------------------------------------------------------------------------------------------
//
//
int postAction ()
{  // Deinit functions

    // Guard against cleanup crashes when initialization failed (e.g. zt.conf not found).
    if (!cur_dir || !MidiOut || !MidiIn) {
        return 0;
    }

    intlist *mod;
    OutputDevice *m;
    midiInDevice *mi;
    static char blah[1024];
    unsigned i;
    static char name[256];
	static char val[256];
	static char tt[256];
    conf DeviceConfig((char *)"devices.conf");

    std::filesystem::current_path(cur_dir);
    for (i=0;i<MAX_MIDI_OUTS;i++) {
        sprintf(name,"open_out_device_%d",i);
        DeviceConfig.remove(&name[0]);
        sprintf(name,"known_out_device_%d",i);
        DeviceConfig.remove(&name[0]);
    }
    for (i=0;i<MAX_MIDI_INS;i++) {
        sprintf(name,"open_in_device_%d",i);
        DeviceConfig.remove(&name[0]);
        sprintf(name,"known_in_device_%d",i);
        DeviceConfig.remove(&name[0]);
    }
	//tt = NULL;

    for (unsigned j=0;j<MidiOut->numOuputDevices;j++) {

        m = MidiOut->outputDevices[j];
        sprintf(name,"known_out_device_%d",j);
        DeviceConfig.set(&name[0], m->szName,0);
//		if(tt != NULL)
//			free(tt);
		encode(m->szName, tt);
//        zt_config_globals.Config->set(&m->szName[0], (m->alias != NULL)?m->alias:"",0);
		sprintf(blah,"latency_%s",tt);
		sprintf(val,"%d",m->delay_ticks);
		DeviceConfig.set(&blah[0], val);
		sprintf(blah,"bank_%s",tt);
		DeviceConfig.set(&blah[0], ( ((MidiOutputDevice*)m )->reverse_bank_select)?"Yes":"No");
        if(m->alias != NULL && strlen(m->alias) > 1) {
            sprintf(blah,"alias_%s",tt);
            DeviceConfig.set(&blah[0], m->alias);
        }
    }
	//if(tt != NULL)
	//	free(tt);
    if (MidiOut && MidiIn) {
        if (zt_config_globals.auto_open_midi) {
            i=0;
            mod = MidiOut->devlist_head;
            while(mod) {
                m = MidiOut->outputDevices[mod->key];
                sprintf(name,"open_out_device_%d",i);
				DeviceConfig.set(&name[0], m->szName,0);
                mod = mod->next; i++;
            }
            i=0;
            mod = MidiIn->devlist_head;
            while(mod) {
                mi = MidiIn->midiInDev[mod->key];
                sprintf(name,"open_in_device_%d",i);
                DeviceConfig.set(&name[0], mi->szName,0);
                mod = mod->next; i++;
            }
        }
        for (unsigned j=0; j<MidiIn->numMidiDevs; j++) {
            mi = MidiIn->midiInDev[j];
            sprintf(name,"known_in_device_%d",j);
            DeviceConfig.set(&name[0], mi->szName,0);
        }
    }
    DeviceConfig.save((char *)"devices.conf");
    zt_config_globals.save();

    if (cur_dir) free(cur_dir);
    if (ztPlayer) ztPlayer->stop();
    if (MidiOut) MidiOut->panic();
    if (clipboard) delete clipboard;

    zt_timer_stop(keytimer);

    if (UI_Toolbar) {
#ifdef DEBUG
        playbuff1_bg = NULL;
        playbuff2_bg = NULL;
#endif
        delete UI_Toolbar;
    }

    delete InstEditorUI;
    delete UI; UI=NULL;
    delete UIP_About;
    delete UIP_MainMenu;
    delete UIP_LoadMsg;
    delete UIP_SaveMsg;
    delete UIP_InstEditor;
    delete UIP_Logoscreen;
    delete UIP_Loadscreen;
    delete UIP_Savescreen;
    delete UIP_Ordereditor;
    delete UIP_Playsong;
    delete UIP_Songconfig;
    delete UIP_Sysconfig;
    delete UIP_Patterneditor;
    delete UIP_PEParms;
    delete UIP_SliderInput;
    delete UIP_NewSong;
    delete UIP_RUSure;
    delete UIP_Help;
    delete UIP_Config;
    delete UIP_PEVol;
    delete UIP_PENature;
    delete UIP_SongDuration;
    delete UIP_SongMessage;
    delete UIP_Arpeggioeditor;
    delete UIP_Midimacroeditor;
    delete UIP_LuaConsole;
    delete UIP_KeyBindings;
    delete UIP_CcConsole;
    g_lua.shutdown();
    delete ztPlayer;
    delete MidiIn;
    delete MidiOut;
    delete song;
    delete CurrentSkin;
    delete bmToolbarRepeater;

    return 0;
}



// ------------------------------------------------------------------------------------------------
//
//
void update_lights(Drawable *S)
{
    if (ztPlayer->playing) {
        if (need_update_lights) {
            need_update_lights = 0;
            if (S->lock() == 0) {
                int addy = INTERNAL_RESOLUTION_Y-480;
                for (int x=0;x<4;x++) {
                    TColor c;
                    if (x==light_pos && x==0) {
                        c = COLORS.LCDHigh;
                    } else if (x==light_pos){
                        c = COLORS.LCDMid;
                    } else {
                        c = COLORS.LCDLow;
                    }
                    S->fillRect(2+(x*8),471+addy,2+(x*8)+6,477+addy,c);
                }
                S->unlock();
                screenmanager.Update(2,471+addy,2+(3*8)+6+1,478+addy);
            }
        }
    }
}


void TP_Keyboard_Repeat(void) {
    if (zt_text_input_is_active) {
        const bool allow_repeat = (keyID == SDLK_BACKSPACE || keyID == SDLK_DELETE ||
                                   keyID == SDLK_LEFT || keyID == SDLK_RIGHT ||
                                   keyID == SDLK_UP || keyID == SDLK_DOWN);
        if (!allow_repeat) {
            zt_timer_stop(keytimer);
            keywait = 0;
            return;
        }
    }
    if (keyID) {
        const int active_jazz_note = jazz_note_is_active(keyID);
        if ((!key_jazz) || !active_jazz_note)
            if (!(active_jazz_note && cur_state==STATE_IEDIT))
                if (!bDontKeyRepeat && keyID != SDLK_8 && keyID != SDLK_4)
                    Keys.insert(keyID,KS_LAST_STATE,0,keyCode);
        if (active_jazz_note && !key_jazz) {
            if (cur_state!=STATE_IEDIT) {
                const mbuf st = jazz_get_state(keyID);
                if(keyID == SDLK_8)
                    MidiOut->panic();
                else
                    MidiOut->noteOff(song->instruments[cur_inst]->midi_device,st.note,st.chan,0x0,0);
                jazz_clear_state(keyID);
            }
        }
    }
    if (keywait == 1) {
        keywait = 0;
        zt_timer_stop(keytimer);
        keytimer = zt_timer_start(keytimer, zt_config_globals.key_repeat_time, TP_Keyboard_Repeat);
    }
}



/* MAIN */

void keyhandler(SDL_KeyboardEvent *e) {
    KBKey id = (KBKey)e->key;
    unsigned int scancode = e->scancode;
    KBMod mod = (KBMod)e->mod;
    unsigned char actual_ch = 0;
    int pressed = e->down ? 1 : 0;

    /* We implement key-repeat ourselves via TP_Keyboard_Repeat + timer.
       Ignore SDL auto-repeat key-down events to avoid queue buildup/stutter. */
    if (pressed && e->repeat) {
        return;
    }

    if (id == SDLK_KP_ENTER)
      id = SDLK_RETURN;

    if (pressed && id == SDLK_RETURN) {
        actual_ch = 10;
    }

    // EU/Finnish ISO keyboards: the § key. On Linux/Windows SDL reports it
    // as NONUSBACKSLASH / INTERNATIONAL1..3 / NONUSHASH depending on layout,
    // so force those to SDLK_GRAVE so the GRAVE handlers (plain § -> Note
    // Off, Shift+§ -> drawmode toggle) fire without a US keyboard layout.
#ifndef __APPLE__
    if (e->scancode == SDL_SCANCODE_NONUSBACKSLASH ||
        e->scancode == SDL_SCANCODE_INTERNATIONAL1 ||
        e->scancode == SDL_SCANCODE_INTERNATIONAL2 ||
        e->scancode == SDL_SCANCODE_INTERNATIONAL3 ||
        e->scancode == SDL_SCANCODE_NONUSHASH) {
      id = SDLK_GRAVE;
    }
#else
    // macOS Finnish ISO (verified via ZT_KEYDEBUG):
    //  * The top-left §-key reports scancode=SDL_SCANCODE_GRAVE with a
    //    locale-specific keycode (0xA7 §), not SDLK_GRAVE. Normalize so
    //    the GRAVE handlers (plain § -> Note Off, Shift+§ -> drawmode
    //    toggle) fire. Also safe on US Mac layouts where the same
    //    scancode already resolves to SDLK_GRAVE.
    //  * The <>-key left of Z reports scancode=SDL_SCANCODE_NONUSBACKSLASH
    //    with keycode=0x3C (<). Map it to the bracket-key octave controls
    //    so `<` = octave down and Shift+`<` (`>`) = octave up.
    if (e->scancode == SDL_SCANCODE_GRAVE) {
      id = SDLK_GRAVE;
    } else if (e->scancode == SDL_SCANCODE_NONUSBACKSLASH) {
      id = (e->mod & SDL_KMOD_SHIFT) ? SDLK_RIGHTBRACKET : SDLK_LEFTBRACKET;
    }
#endif
    // Opt-in per-keydown tracing. Set ZT_KEYDEBUG=1 to enable; default is
    // off so this path adds zero per-key cost (the env check runs once and
    // is cached in a static). When enabled, every keydown is logged to
    // stderr and ./zt-keydebug.log for keyboard-layout debugging.
    if (pressed) {
      static int init_done = 0;
      static int enabled = 0;
      static FILE *dbg_fp = NULL;
      if (!init_done) {
        init_done = 1;
        const char *env = getenv("ZT_KEYDEBUG");
        enabled = (env && env[0] && env[0] != '0') ? 1 : 0;
        if (enabled) {
          dbg_fp = fopen("zt-keydebug.log", "w");
          if (dbg_fp) {
            fprintf(dbg_fp, "# ZT_KEYDEBUG active. Columns: scancode (name) | keycode (name) | mod | mod names\n");
            fflush(dbg_fp);
          }
        }
      }
      if (enabled) {
        int sc = (int)e->scancode;
        const char *sc_name = SDL_GetScancodeName(e->scancode);
        const char *kc_name = SDL_GetKeyName(e->key);
        char line[256];
        snprintf(line, sizeof(line),
                 "scancode=%d(%s) keycode=0x%X(%s) mod=0x%X%s%s%s%s",
                 sc, sc_name ? sc_name : "?",
                 (unsigned)e->key, kc_name ? kc_name : "?",
                 (unsigned)e->mod,
                 (e->mod & SDL_KMOD_SHIFT) ? " SHIFT" : "",
                 (e->mod & SDL_KMOD_CTRL) ? " CTRL" : "",
                 (e->mod & SDL_KMOD_ALT) ? " ALT" : "",
                 (e->mod & SDL_KMOD_GUI) ? " GUI/CMD" : "");
        fprintf(stderr, "[zt-key] %s\n", line);
        fflush(stderr);
        if (dbg_fp) {
          fprintf(dbg_fp, "%s\n", line);
          fflush(dbg_fp);
        }
      }
    }

    if (id != SDLK_LALT && id != SDLK_RALT && id != SDLK_RCTRL && id != SDLK_LCTRL && id != SDLK_LSHIFT && id != SDLK_RSHIFT) {
        if (zt_text_input_is_active && pressed) {
            const bool is_edit_or_nav =
                (id == SDLK_UP || id == SDLK_DOWN || id == SDLK_LEFT || id == SDLK_RIGHT ||
                 id == SDLK_BACKSPACE || id == SDLK_DELETE || id == SDLK_RETURN ||
                 id == SDLK_TAB || id == SDLK_HOME || id == SDLK_END ||
                 id == SDLK_PAGEUP || id == SDLK_PAGEDOWN);
            // Modifier combos (Ctrl/Alt/Cmd+X) are shortcuts, not text input
            // — they must flow through to global_keys / page update, not be
            // swallowed in favor of SDL_TEXTINPUT. Without this, Ctrl+Alt+Q
            // (quit), Ctrl+L (load), etc. stop working on any text-input
            // page (Lua Console, Song Message).
            const bool has_cmd_mod =
                (mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;
            if (!is_edit_or_nav && !has_cmd_mod) {
                if (id >= 0x20 && id < 0x7f) {
                    return;
                }
            }
        }
        if (zclear_flag)
            zclear_presscount = 0;
        else
            zclear_flag = 0;

        if (!pressed) {
            if (id == last_keyjazz) {
                last_cmd_keyjazz = 0;
                last_keyjazz = 0;
            }
            if (id == keyID) {
                keyID = 0;
                keyCode = 0;
                zt_timer_stop(keytimer);
            }
            if (jazz_note_is_active((int)id)) {
                const mbuf st = jazz_get_state((int)id);
                if(id == SDLK_8)
                    MidiOut->panic();
                else
                    MidiOut->noteOff(song->instruments[cur_inst]->midi_device,st.note,st.chan,0x0,0);
                jazz_clear_state((int)id);
            }
        } else  {
            Keys.insert(id, mod, actual_ch, scancode);
            const bool allow_repeat = (!zt_text_input_is_active) ||
                (id == SDLK_BACKSPACE || id == SDLK_DELETE ||
                 id == SDLK_LEFT || id == SDLK_RIGHT ||
                 id == SDLK_UP || id == SDLK_DOWN ||
                 id == SDLK_HOME || id == SDLK_END);
            if (allow_repeat && !(id == SDLK_RETURN && (mod & SDL_KMOD_ALT))) {
                keytimer = zt_timer_start(keytimer, zt_config_globals.key_wait_time, TP_Keyboard_Repeat);
                keywait = 1;
                keyID = id;
                keyCode = scancode;
            } else if (!allow_repeat) {
                zt_timer_stop(keytimer);
                keywait = 0;
                keyID = 0;
                keyCode = 0;
            }
        }
    }
}

void textinputhandler(const SDL_Event *e) {
    if (!zt_text_input_is_active) {
        return;
    }
    if (!e || !e->text.text || !e->text.text[0]) {
        return;
    }

    const unsigned char ch = (unsigned char)e->text.text[0];
    // Opt-in text-input logging. Set ZT_KEYDEBUG=1 to enable. Default off
    // keeps this hot path free of getenv/fopen per event.
    {
        static int init_done = 0;
        static int enabled = 0;
        if (!init_done) {
            init_done = 1;
            const char *env = getenv("ZT_KEYDEBUG");
            enabled = (env && env[0] && env[0] != '0') ? 1 : 0;
        }
        if (enabled) {
            fprintf(stderr, "[zt-key] TEXT_INPUT='%s' (first byte=0x%02X)\n",
                    e->text.text, ch);
            fflush(stderr);
            FILE *fp = fopen("zt-keydebug.log", "a");
            if (fp) {
                fprintf(fp, "TEXT_INPUT='%s' (first byte=0x%02X)\n",
                        e->text.text, ch);
                fclose(fp);
            }
        }
    }
    // Newline (\n, 0x0A) is delivered via the dedicated SDLK_RETURN
    // keydown path (keyhandler sets actual_ch=10 for SDLK_RETURN).
    // If we ALSO let it through here, every Enter press pushes two
    // newlines in widgets that handle both paths. Same logic for Tab —
    // SDLK_TAB has its own meaning (widget focus cycling) and shouldn't
    // be appended as text.
    if (ch >= 0x20) {
        Keys.insert(SDLK_SPACE, SDL_KMOD_NONE, ch, SDL_SCANCODE_SPACE);
    }
}

static void zt_update_mouse_position(float win_x, float win_y) {
    int window_w = RESOLUTION_X;
    int window_h = RESOLUTION_Y;
    if (zt_main_window) {
        SDL_GetWindowSize(zt_main_window, &window_w, &window_h);
    }
    if (window_w <= 0) {
        window_w = RESOLUTION_X;
    }
    if (window_h <= 0) {
        window_h = RESOLUTION_Y;
    }

    const float sx = (float)INTERNAL_RESOLUTION_X / (float)window_w;
    const float sy = (float)INTERNAL_RESOLUTION_Y / (float)window_h;
    LastX = (int)(win_x * sx);
    LastY = (int)(win_y * sy);

    if (LastX < 0) LastX = 0;
    if (LastY < 0) LastY = 0;
    if (LastX >= INTERNAL_RESOLUTION_X) LastX = INTERNAL_RESOLUTION_X - 1;
    if (LastY >= INTERNAL_RESOLUTION_Y) LastY = INTERNAL_RESOLUTION_Y - 1;
}

void mousemotionhandler(SDL_MouseMotionEvent *e) {
    zt_update_mouse_position((float)e->x, (float)e->y);
}

void mouseupbuttonhandler(SDL_MouseButtonEvent *e) {
    zt_update_mouse_position((float)e->x, (float)e->y);
    switch(e->button) {
        case SDL_BUTTON_LEFT:
            Keys.insert(((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT)));
            bMouseIsDown = false;
            break;
        case SDL_BUTTON_RIGHT:
            Keys.insert(((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_RIGHT)));
        break;
    }
    MousePressX = LastX;
    MousePressY = LastY;
}


void mousedownbuttonhandler(SDL_MouseButtonEvent *e) {
    zt_update_mouse_position((float)e->x, (float)e->y);

    switch(e->button) {
        case SDL_BUTTON_LEFT:
            Keys.insert(((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT)));
            bMouseIsDown = true;
            break;
        case SDL_BUTTON_RIGHT:
            Keys.insert(((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT)));
        break;
    }

    MousePressX = LastX;
    MousePressY = LastY;
}

void mousewheelhandler(SDL_MouseWheelEvent *e) {
    if (!e) {
        return;
    }
    const SDL_Keymod mods = SDL_GetModState();
    if ((mods & SDL_KMOD_CTRL) == 0) {
        // Plain scroll (no Ctrl): drive the active page's text view
        // by synthesising Up/Down keypresses. TextBox / CommentEditor
        // already handle these for line-by-line scrolling, so the wheel
        // just feeds them their existing input. Limited to text-heavy
        // states (F1 Help, About, Song Message) -- other pages keep
        // their current "wheel does nothing" behaviour.
        if (e->y != 0 &&
            (cur_state == STATE_HELP ||
             cur_state == STATE_ABOUT ||
             cur_state == STATE_SONG_MESSAGE)) {
            const KBKey synth = (e->y > 0) ? SDLK_UP : SDLK_DOWN;
            int steps = (e->y > 0) ? e->y : -e->y;
            // 3 lines per detent feels right -- single-line was sluggish.
            steps *= 3;
            if (steps > 64) steps = 64;
            for (int i = 0; i < steps; ++i) Keys.insert(synth);
        }
        return;
    }

    const float old_zoom = ZOOM;
    static int zoom_escape_dir = 0;

    int wheel_steps = e->y;
    while (wheel_steps != 0) {
        const int dir = (wheel_steps > 0) ? 1 : -1;
        const float nearest_integer = std::round(ZOOM);
        const float distance_to_integer = std::fabs(ZOOM - nearest_integer);
        const bool at_integer_zoom = distance_to_integer < 0.001f;

        /* Integer zoom levels are sticky: first detent is absorbed, second detent exits. */
        if (at_integer_zoom) {
            if (zoom_escape_dir != dir) {
                zoom_escape_dir = dir;
                wheel_steps -= dir;
                continue;
            }
        }

        zoom_escape_dir = 0;
        ZOOM += 0.1f * (float)dir;
        ZOOM = std::round(ZOOM * 10.0f) / 10.0f;
        if (ZOOM > 3.0f) ZOOM = 3.0f;
        if (ZOOM < 0.5f) ZOOM = 0.5f;

        wheel_steps -= dir;
    }

    if (ZOOM != old_zoom) {
        // Preserve the user's current window size across zoom changes.
        // Previously we passed RESOLUTION_X/Y (initial size from zt.conf),
        // which snapped the window back every time the user scrolled —
        // any manual drag-resize was lost on the next zoom tick.
        int win_w = RESOLUTION_X;
        int win_h = RESOLUTION_Y;
        if (zt_main_window) {
            SDL_GetWindowSize(zt_main_window, &win_w, &win_h);
        }
        if (!set_video_mode(win_w, win_h, errstr)) {
            ZOOM = old_zoom;
            (void)set_video_mode(win_w, win_h, errstr);
            return;
        }
        zt_request_ui_full_refresh();
    }
}

static void zt_request_ui_full_refresh(void)
{
    if (UI) UI->full_refresh();
    if (UI_Toolbar) UI_Toolbar->full_refresh();
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

static void zt_queue_full_redraw(void)
{
    doredraw++;
    need_refresh++;
    screenmanager.UpdateAll();
}

static void zt_handle_resize_event(int w, int h)
{
    if (!set_video_mode(w, h, errstr)) {
        return;
    }
    zt_request_ui_full_refresh();
}

static int zt_autosave_now(void)
{
    if (!song) {
        return 0;
    }

    const std::string autosave_file = zt_make_autosave_filename();
    const char *autosave_tmp_file = "__autosave.tmp.zt";

    const int dirty_before = file_changed;
    unsigned char original_filename[ZTM_FILENAME_MAXLEN];
    memcpy(original_filename, song->filename, sizeof(original_filename));

    memset(song->filename, 0, sizeof(song->filename));
    strncpy((char *)song->filename, autosave_tmp_file, sizeof(song->filename) - 1);

    const int save_ret = song->save((char *)song->filename);

    memcpy(song->filename, original_filename, sizeof(song->filename));
    file_changed = dirty_before;

    if (save_ret != 0) {
        ZT_DEBUG_LOG("[autosave] save failed\n");
        return 0;
    }

    std::error_code ec;
    std::filesystem::remove(autosave_file, ec);
    ec.clear();
    std::filesystem::rename(autosave_tmp_file, autosave_file, ec);
    if (ec) {
        ZT_DEBUG_LOG("[autosave] rename failed: %s\n", ec.message().c_str());
        return 0;
    }

    zt_prune_autosaves(3);
    ZT_DEBUG_LOG("[autosave] wrote %s\n", autosave_file.c_str());
    return 1;
}

static std::string zt_make_autosave_filename(void)
{
    const std::time_t now = std::time(NULL);
    std::tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &now);
#else
    localtime_r(&now, &tm_now);
#endif

    char name[64];
    std::snprintf(name, sizeof(name), "__autosave_%04d_%02d_%02d_%02d_%02d_%02d.zt",
                  tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                  tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    return std::string(name);
}

static void zt_prune_autosaves(size_t keep_count)
{
    std::vector<std::filesystem::path> autosaves;
    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec) {
        return;
    }

    for (const auto &entry : std::filesystem::directory_iterator(cwd, ec)) {
        if (ec) {
            return;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string fn = entry.path().filename().string();
        if (fn.rfind("__autosave_", 0) == 0 && fn.size() > 13 && fn.substr(fn.size() - 3) == ".zt") {
            autosaves.push_back(entry.path());
        }
    }

    std::sort(autosaves.begin(), autosaves.end(), [](const std::filesystem::path &a, const std::filesystem::path &b) {
        return a.filename().string() > b.filename().string();
    });

    for (size_t i = keep_count; i < autosaves.size(); ++i) {
        std::filesystem::remove(autosaves[i], ec);
        ec.clear();
    }
}

static void zt_autosave_tick(void)
{
    const int interval_seconds = zt_config_globals.autosave_interval_seconds;
    if (interval_seconds <= 0) {
        return;
    }

    const Uint64 now_ms = SDL_GetTicks();
    if (g_last_autosave_tick_ms == 0) {
        g_last_autosave_tick_ms = now_ms;
        return;
    }

    const Uint64 interval_ms = (Uint64)interval_seconds * 1000ULL;
    if (now_ms - g_last_autosave_tick_ms < interval_ms) {
        return;
    }
    g_last_autosave_tick_ms = now_ms;

    if (file_changed <= 0) {
        return;
    }
    if (ztPlayer && ztPlayer->playing) {
        return;
    }

    (void)zt_autosave_now();
}

static void zt_backend_destroy_frame_texture(void)
{
    if (zt_frame_texture) {
        SDL_DestroyTexture(zt_frame_texture);
        zt_frame_texture = NULL;
    }
}

static bool zt_zoom_is_integer(float zoom_value)
{
    const float nearest_integer = std::round(zoom_value);
    return std::fabs(zoom_value - nearest_integer) < 0.001f;
}

static SDL_ScaleMode zt_get_configured_scale_mode(void)
{
    const char *scale_filter = zt_config_globals.scale_filter;
    if (!scale_filter || !scale_filter[0]) {
        return zt_zoom_is_integer(ZOOM) ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
    }
    if (SDL_strcasecmp(scale_filter, "nearest") == 0) {
        return SDL_SCALEMODE_NEAREST;
    }
    if (SDL_strcasecmp(scale_filter, "linear") == 0) {
        return zt_zoom_is_integer(ZOOM) ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR;
    }
#ifdef SDL_SCALEMODE_PIXELART
    if (SDL_strcasecmp(scale_filter, "pixelart") == 0) {
        return SDL_SCALEMODE_PIXELART;
    }
#endif
    ZT_DEBUG_LOG("[sdl3] unknown scale_filter '%s' in zt.conf, using linear\n", scale_filter);
    return SDL_SCALEMODE_LINEAR;
}

static int zt_backend_ensure_frame_texture(int w, int h, char *errstr)
{
    if (!zt_renderer) {
        return 0;
    }
    if (zt_frame_texture) {
        float tw = 0.0f;
        float th = 0.0f;
        if (SDL_GetTextureSize(zt_frame_texture, &tw, &th) && (int)tw == w && (int)th == h) {
            return 1;
        }
        zt_backend_destroy_frame_texture();
    }

    zt_frame_texture = SDL_CreateTexture(zt_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!zt_frame_texture) {
        if (errstr) {
            snprintf(errstr, 2048, "Couldn't create frame texture: %s", SDL_GetError());
        }
        return 0;
    }
    if (!SDL_SetTextureScaleMode(zt_frame_texture, zt_get_configured_scale_mode())) {
        if (errstr) {
            snprintf(errstr, 2048, "Couldn't set frame texture scale mode: %s", SDL_GetError());
        }
        return 0;
    }
    if (!SDL_SetTextureBlendMode(zt_frame_texture, SDL_BLENDMODE_NONE)) {
        if (errstr) {
            snprintf(errstr, 2048, "Couldn't set frame texture blend mode: %s", SDL_GetError());
        }
        return 0;
    }
    return 1;
}

static void zt_backend_present_frame(void)
{
    if (!zt_renderer || !zt_frame_texture || !screen_buffer || !screen_buffer->surface) {
      return;
    }

    if (!SDL_UpdateTexture(zt_frame_texture, NULL, screen_buffer->surface->pixels, screen_buffer->surface->pitch)) {
      ZT_DEBUG_LOG("[sdl3] update texture failed: %s\n", SDL_GetError());
      return;
    }

    if (!SDL_RenderClear(zt_renderer)) {
      ZT_DEBUG_LOG("[sdl3] render clear failed: %s\n", SDL_GetError());
      return;
    }
    if (!SDL_RenderTexture(zt_renderer, zt_frame_texture, NULL, NULL)) {
      ZT_DEBUG_LOG("[sdl3] render texture failed: %s\n", SDL_GetError());
      return;
    }
    SDL_RenderPresent(zt_renderer);
}

static int zt_handle_platform_window_event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        // macOS Cmd-Tab away and back: SDL stops delivering key-up
        // events for any keys held during focus loss, so the Keys
        // queue can come back with stale entries that look like
        // permanently-pressed arrows. Flush on focus regain so the
        // app responds to the next real keypress.
        Keys.flush();
        zt_request_ui_full_refresh();
        return 1;
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESTORED:
        zt_request_ui_full_refresh();
        return 1;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        zt_handle_resize_event((int)e->window.data1, (int)e->window.data2);
        return 1;
    default:
        return 0;
    }
}




// ------------------------------------------------------------------------------------------------
//
//
int action(Screen *S)
{
    if (cur_state!=STATE_LOGO) {
        if (!modal)
            UI_Toolbar->update();
        global_keys(S);
        S = screen_buffer;
        if (!S) {
            return 1;
        }
    }

    static int old_width = 0 ;
    static int old_height = 0 ;

    if(old_width != INTERNAL_RESOLUTION_X || old_height != INTERNAL_RESOLUTION_Y) {

        old_width = INTERNAL_RESOLUTION_X ;
        old_height = INTERNAL_RESOLUTION_Y ;

        //doredraw = 1 ;
        need_update++ ;
        need_popup_refresh = 1 ;
        clear_popup = 1 ;
    }


    /*//doredraw = 1 ;
    need_refresh = 1 ;
        need_update++ ;
        need_popup_refresh = 1 ;
        clear_popup = 1 ;*/

    if (doredraw) {

        if (cur_state != STATE_LOGO)
            redrawscreen(S);
        fixmouse++;
        //Keys.flush();
        doredraw=0;
        UI->full_refresh();
        need_refresh++;
    }

    if (status_change) {
        update_status(S);
    }


//  if (PopupWindow)
//      PopupWindow->update(K);
    if (!window_stack.isempty())
        window_stack.update();
    else
        ActivePage->update();

#ifdef DEBUG
    playbuff1_bg->setvalue(ztPlayer->play_buffer[0]->size);
    playbuff2_bg->setvalue(ztPlayer->play_buffer[1]->size);
    keybuff_bg->setvalue(Keys.cursize);
#endif

    if (need_popup_refresh) {
//      if (PopupWindow)
//          PopupWindow->draw(S);
        if (!window_stack.isempty())
            window_stack.draw(S);
        else
            need_popup_refresh = 0;
    }

    if (need_refresh) {
        fixmouse++;
#ifdef FORCE_FULL_SCREEN_REFRESH
        UI->full_refresh();
        if (UI_Toolbar) UI_Toolbar->full_refresh();
#endif

        if (need_update) ActivePage->update();

        if (clear_popup) {

            if (S->lock()==0) {

                // Clean UI page background
                // <Manu> Creo que este clean es mejorable y segn cmo redundante [EN: I think this clean can be improved and is somewhat redundant]
                S->fillRect(col(1),row(12),INTERNAL_RESOLUTION_X-CHARACTER_SIZE_X,INTERNAL_RESOLUTION_Y - (480-424),/*0x00FF00*/COLORS.Background);
                S->unlock();

                screenmanager.UpdateAll();
            }
            clear_popup = 0;
        }

        ActivePage->draw(S);
        if (!window_stack.isempty())
            window_stack.draw(S);
        if (cur_state!=STATE_LOGO) {
            UI_Toolbar->draw(S);
        }
    }
    if (do_exit) {
        return 1;
    }

    if (screenmanager.NeedRefresh())
        need_update_lights = 1;

    update_lights(S);

    //screenmanager.Refresh(S);

    if (updated) {
        updated=0;
    }

    return 0;
}



// ------------------------------------------------------------------------------------------------
//
//
static int zt_backend_set_video_mode(char *errstr)
{
  Uint32 window_flags = SDL_WINDOW_RESIZABLE;
  if (zt_config_globals.full_screen) {
    window_flags |= SDL_WINDOW_FULLSCREEN;
    bIsFullscreen = true;
  } else {
    bIsFullscreen = false;
  }

  if (!zt_main_window) {
    zt_main_window = SDL_CreateWindow("zt", RESOLUTION_X, RESOLUTION_Y, window_flags);
    if (!zt_main_window) {
      sprintf(errstr, "Couldn't create SDL window: %s\n", SDL_GetError());
      zt_show_error("Error", errstr);
      return 0;
    }
#ifdef __APPLE__
    // Free Cmd-Q from the auto-created NSApp menu so the Pattern Editor can
    // use it as Transpose-Up (KS_HAS_ALT treats Cmd as Alt). Quit reachable
    // via Ctrl-Q / Ctrl-Alt-Q.
    zt_macos_disable_cmd_q();
    // Free Cmd-W from the Window menu's Close item so it reaches our
    // keyhandler instead of dismissing the SDL window. Pattern Editor
    // treats Cmd-W as Ctrl-W (clear unused volumes in selection).
    zt_macos_disable_cmd_w();
#endif
    zt_renderer = SDL_CreateRenderer(zt_main_window, NULL);
    if (!zt_renderer) {
      snprintf(errstr, 2048, "Couldn't create SDL renderer: %s\n", SDL_GetError());
      zt_show_error("Error", errstr);
      return 0;
    }
    if (!SDL_SetRenderVSync(zt_renderer, 1)) {
      ZT_DEBUG_LOG("Warning: couldn't enable renderer vsync: %s\n", SDL_GetError());
    }

    // Load window icon (configurable via zt.conf 'window_icon'; default 'zt_icon.png').
    // On macOS this also updates the Dock icon at runtime; a failure is silent.
    {
      const char *icon_path = zt_config_globals.window_icon[0]
        ? zt_config_globals.window_icon
        : "zt_icon.png";
      SDL_Surface *icon = SDL_LoadPNG(icon_path);
      if (icon) {
        SDL_SetWindowIcon(zt_main_window, icon);
        SDL_DestroySurface(icon);
      }
    }
  } else {
    SDL_SetWindowSize(zt_main_window, RESOLUTION_X, RESOLUTION_Y);
    SDL_SetWindowFullscreen(zt_main_window, zt_config_globals.full_screen ? SDL_WINDOW_FULLSCREEN : 0);
  }
  return 1;
}

int set_video_mode(int w, int h, char *errstr)
{
  RESOLUTION_X = w ;
  RESOLUTION_Y = h ;

  if((RESOLUTION_X / ZOOM) < MINIMUM_SCREEN_WIDTH)
  {
      RESOLUTION_X = MINIMUM_SCREEN_WIDTH * ZOOM ;
  }

  if((RESOLUTION_Y / ZOOM) < MINIMUM_SCREEN_HEIGHT)
  {
      RESOLUTION_Y = MINIMUM_SCREEN_HEIGHT * ZOOM ;
  }

  if (!zt_backend_set_video_mode(errstr)) {

    // <Manu> El mensaje de error estaba mal [EN: the error message was wrong]

    //sprintf(errstr, "Couldn't set 640x480x32 video mode: %s\n", SDL_GetError());
    sprintf(errstr, "Couldn't set %dx%dx32 video mode: %s\n", INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y, SDL_GetError());

    zt_show_error("Error", errstr);
    return 0;
  }
  // <Manu> Cuidado con esto...
  if (screen_buffer_surface != NULL) {
    zt_destroy_surface(screen_buffer_surface);
    screen_buffer_surface = NULL;
  }

  screen_buffer_surface = SDL_CreateSurface(INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y, SDL_PIXELFORMAT_ARGB8888);

  if(screen_buffer_surface == NULL) {

    // Error opening the skin !
    exit(1);
  }

  if(screen_buffer != NULL) delete screen_buffer ;

  screen_buffer = new Screen(screen_buffer_surface, false);
  if (!zt_backend_ensure_frame_texture(INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y, errstr)) {
    zt_show_error("Error", errstr);
    return 0;
  }

  /*if(UI) delete UI ;
  UI = new UserInterface;

  if(UI_Toolbar) delete UI_Toolbar ;
  UI_Toolbar = new UserInterface;
  UI_Toolbar->dontmess = 1;*/


  return 1;
}

static int zt_backend_init_runtime(char *errstr)
{
  if (!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)) {
    sprintf(errstr,"Could not initialize SDL: %s.\n", SDL_GetError());
    zt_show_error("Error", errstr);
    return 0;
  }
  return 1;
}






// ------------------------------------------------------------------------------------------------
//
//
int initSDL(void)
{
  if(zt_config_globals.default_directory[0] != '\0') zt_set_current_directory(zt_config_globals.default_directory);

  cur_dir = (char *)malloc(256);
  zt_get_current_directory(256, cur_dir);

  if (zt_config_globals.load()) {
    if (zt_config_globals.save()) {
      zt_show_error("zt: error", "Fatal: Unable to create default zt.conf");
      return 0;
    }
    ZT_DEBUG_LOG("[conf] zt.conf not found, created default configuration\n");
  }
  /*
  if (zcmp(Config.get("fullscreen"),"yes")) {
  //FULLSCREEN=1;
  zt_config_globals.full_screen = 1;
  } else {
  //FULLSCREEN=0;
  zt_config_globals.full_screen = 0;
  }
  */
  CurrentSkin = new Skin;
  if (! CurrentSkin->load(zt_config_globals.skin) ) {

    if(!CurrentSkin->load("default")) {

        // <Manu> :-/
        return 0;
    }
  }


  if (!zt_backend_init_runtime(errstr)) {
    return 0;
  }

  if (!set_video_mode(zt_config_globals.screen_width, zt_config_globals.screen_height, errstr)) {
    return 0;
  }


//    char str[80];
//    istream *is;
/*
    Skin = newResourceStream(skinfile);
    if (!Skin) {
        sprintf(str,"Fatal: Could not load skin resource: %s\n",skinfile);
        MessageBox(NULL,str,"zt: error",MB_OK | MB_ICONERROR );
        return -1;
    }
    is = Skin->getStream("font.fnt");
    Skin->freeStream(is);
    if (!S->isModeAvailable(INTERNAL_RESOLUTION_X,INTERNAL_RESOLUTION_Y)) {
        sprintf(str,"Fatal: Screen mode (%dx%dx16) is not support by your gfx card",INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y);
        zt_show_error("zt: error", str);
        return -1;
    }
*/
//    BM_Cache = new BitmapCache(*Skin);
/*
    if (font_load(".\\skin\\font.fnt")) {
        sprintf(str,"Fatal: Could not load font.fnt from %s\n",skinfile);
        MessageBox(NULL,str,"zt: error",MB_OK | MB_ICONERROR );
        return NULL;
    }
*/

  MidiOut = new midiOut;
  MidiIn  = new midiIn;
	setup_midi();

  UI = new UserInterface;
  UI_Toolbar = new UserInterface;
  UI_Toolbar->dontmess = 1;
  init_edit_cols();
  clipboard = new CClipboard;

  song = new zt_module(zt_config_globals.default_tpb, zt_config_globals.default_bpm);

  ztPlayer = new player(1,zt_config_globals.prebuffer_rows, song); // 1ms resolution;

    ///////////////////////////////////////////////////////////////////////
    // Here we set the highlight and lowlight according to either specific
    // zt.conf parameters or ticks per beat (default)

//    if(zt_config_globals.highlight_increment)
  //      highlight_count = zt_config_globals.highlight_increment;
//    else
  //      highlight_count = song->tpb;
//    if(zt_config_globals.lowlight_increment)
  //      lowlight_count = zt_config_globals.lowlight_increment;
//    else
  //      lowlight_count = song->tpb >> 1 / song->tpb / 2;

    ////////////////////////////////////////////////////////////////////////

    jazz_clear_all_states();

#ifdef __MANU__OLD_AND_UNNEEDED
    // This loads the icon from the resource and attaches it to the main window
    HICON icon=(HICON)LoadImage(GetModuleHandle(NULL),MAKEINTRESOURCE(IDI_ZTICON),IMAGE_ICON,0,0,0);
    HWND w = FindWindow("zt","zt");
    SetClassLong(w, GCL_HICON, (LONG)icon);
#endif
    zt_set_window_title("zt");


    UIP_About = new CUI_About;
    UIP_InstEditor = new CUI_InstEditor;
    UIP_Loadscreen = new CUI_Loadscreen;
    UIP_Logoscreen = new CUI_Logoscreen;
    UIP_Savescreen = new CUI_Savescreen;
    UIP_LoadMsg = new CUI_LoadMsg;
    UIP_SaveMsg = new CUI_SaveMsg;
    UIP_Ordereditor = new CUI_Ordereditor;
    UIP_Playsong = new CUI_Playsong;
    UIP_Songconfig = new CUI_Songconfig;
    UIP_Sysconfig = new CUI_Sysconfig;
    UIP_PaletteEditor = new CUI_PaletteEditor;
// this guy has been moved to initGFX because the MIDI out devices are not setup yet
    UIP_Config = new CUI_Config;
    UIP_Patterneditor = new CUI_Patterneditor;
    UIP_PEParms = new CUI_PEParms;
    UIP_PEVol = new CUI_PEVol;
    UIP_PENature = new CUI_PENature;
    UIP_SliderInput = new CUI_SliderInput;
    UIP_NewSong = new CUI_NewSong;
    UIP_SongMessage = new CUI_SongMessage;
    UIP_Arpeggioeditor = new CUI_Arpeggioeditor;
    UIP_Midimacroeditor = new CUI_Midimacroeditor;
    UIP_RUSure = new CUI_RUSure;
    UIP_Help = new CUI_Help;
    UIP_SongDuration = new CUI_SongDuration;
    UIP_LuaConsole = new CUI_LuaConsole;
    UIP_KeyBindings = new CUI_KeyBindings;
    UIP_CcConsole = new CUI_CcConsole;
    g_lua.init();
    UIP_PaletteEditor = new CUI_PaletteEditor;
    UIP_MainMenu = new CUI_MainMenu;
    //SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL );

    return 1;

}


#ifdef _ENABLE_AUDIO
static SDL_AudioStream *g_audio_stream = NULL;

static void audio_mixer(void *udata, SDL_AudioStream *stream, int additional_amount, int)
{
  if (additional_amount <= 0) {
    return;
  }

  Uint8 *mixbuf = (Uint8 *)malloc((size_t)additional_amount);
  if (!mixbuf) {
    return;
  }

  MidiOut->MixAudio(udata, mixbuf, additional_amount);
  SDL_PutAudioStreamData(stream, mixbuf, additional_amount);
  free(mixbuf);
}

static int zt_backend_audio_start(void)
{
  SDL_AudioSpec wanted;
  SDL_zero(wanted);
  wanted.freq = 44100;
  wanted.format = SDL_AUDIO_S16;
  wanted.channels = 2;

  g_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wanted, audio_mixer, NULL);
  if (!g_audio_stream) {
    fprintf(stderr, "Couldn't open audio stream: %s\n", SDL_GetError());
    return 0;
  }
  if (!SDL_ResumeAudioStreamDevice(g_audio_stream)) {
    fprintf(stderr, "Couldn't start audio stream: %s\n", SDL_GetError());
    SDL_DestroyAudioStream(g_audio_stream);
    g_audio_stream = NULL;
    return 0;
  }
  return 1;
}

static void zt_backend_audio_stop(void)
{
  if (g_audio_stream) {
    SDL_DestroyAudioStream(g_audio_stream);
    g_audio_stream = NULL;
  }
}
#endif

static void zt_backend_release_frame_resources(void)
{
  zt_backend_destroy_frame_texture();
}

static void zt_backend_shutdown_runtime(void)
{
  if (zt_renderer) {
    SDL_DestroyRenderer(zt_renderer);
    zt_renderer = NULL;
  }
  if (zt_main_window) {
    SDL_DestroyWindow(zt_main_window);
    zt_main_window = NULL;
  }
  SDL_Quit();
}


// ------------------------------------------------------------------------------------------------
//
//
// ---------------------------------------------------------------------------
// CLI parsing.
// Recognised flags:
//   -h, --help                   print usage + exit
//       --list-midi-in           after MIDI init, list input ports + exit
//       --midi-in <name|index>   open this MIDI input port at startup
//       --midi-clock <name|index>  open + enable midi_in_sync +
//                                  midi_in_sync_chase_tempo so the song's
//                                  BPM follows incoming MIDI clock.
// First positional arg (if any) is treated as a .zt song to load on boot.

struct ZtCliArgs {
    bool        show_help       = false;
    bool        list_midi_in    = false;
    const char *song_filename   = NULL;
    // --midi-in can repeat so users with multiple ports (or the
    // ESC-menu "Copy relaunch command" feature that round-trips every
    // currently-open port) can capture them all in one invocation.
    static const int MAX_MIDI_IN_FLAGS = 16;
    const char *midi_in_ports[MAX_MIDI_IN_FLAGS] = {NULL};
    int         num_midi_in_ports = 0;
    // --midi-clock is one source by definition; multiple master clocks
    // would fight each other.
    const char *midi_clock_port = NULL;
};

static void zt_print_cli_help(const char *progname) {
    fprintf(stdout,
        "Usage: %s [options] [song.zt]\n"
        "\n"
        "Options:\n"
        "  -h, --help                  Show this help and exit.\n"
        "      --list-midi-in          List available MIDI input ports and\n"
        "                              exit. Use the printed index or name\n"
        "                              with --midi-in / --midi-clock.\n"
        "      --midi-in <name|index>  Open the named MIDI input port at\n"
        "                              startup. <name> matches by case-\n"
        "                              sensitive substring against the\n"
        "                              OS-reported device name; <index> is\n"
        "                              the 0-based number from\n"
        "                              --list-midi-in. May be repeated to\n"
        "                              open multiple ports.\n"
        "      --midi-clock <name|index>\n"
        "                              Open the port AND enable MIDI clock\n"
        "                              chase: turns on midi_in_sync +\n"
        "                              midi_in_sync_chase_tempo so the song\n"
        "                              tempo follows incoming F8 clock.\n"
        "\n"
        "Examples:\n"
        "  %s mysong.zt\n"
        "  %s --midi-in \"IAC Driver Bus 1\"\n"
        "  %s --midi-clock 0 mysong.zt\n",
        progname, progname, progname, progname);
}

// Strip leading '-' or '--' so we accept "-midi-in" or "--midi-in" --
// dash-count tolerant. Returns NULL if `s` had no leading dash (the
// caller treats that as a positional argument, not a flag).
static const char *zt_cli_strip_leading_dashes(const char *s) {
    if (!s || s[0] != '-') return NULL;
    while (*s == '-') ++s;
    if (*s == '\0') return NULL;   // bare "-" or "--" => not a flag we know
    return s;
}

// Match a flag name. Accepts both `--flag value` (next argv element) and
// `--flag=value` (= separator) forms, and is dash-count tolerant. On a
// match, *value_out points either at argv[i+1] (consuming it) or at the
// substring after `=`. Returns 1 on match (and possibly advances *i if
// the next-argv form), 0 on no-match, -1 on missing-arg error.
static int zt_cli_match_flag_with_value(int argc, char *argv[], int *i,
                                        const char *flag_name,
                                        const char **value_out) {
    const char *a = argv[*i];
    if (!a) return 0;
    const char *body = zt_cli_strip_leading_dashes(a);
    if (!body) return 0;            // not a flag-shaped argv
    const char *eq   = strchr(body, '=');
    size_t name_len  = eq ? (size_t)(eq - body) : strlen(body);
    size_t need_len = strlen(flag_name);
    if (name_len != need_len) return 0;
    if (strncmp(body, flag_name, need_len) != 0) return 0;
    if (eq) {
        *value_out = eq + 1;
        return 1;
    }
    if (*i + 1 >= argc) {
        fprintf(stderr, "zt: --%s requires a port name or index\n", flag_name);
        return -1;
    }
    *value_out = argv[++(*i)];
    return 1;
}

static bool zt_cli_match_flag(const char *a, const char *flag_name) {
    const char *body = zt_cli_strip_leading_dashes(a);
    if (!body) return false;        // not flag-shaped, never a flag match
    if (strchr(body, '=')) return false;   // "--flag=foo" not handled here
    return strcmp(body, flag_name) == 0;
}

static int zt_parse_cli(int argc, char *argv[], ZtCliArgs *out) {
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a || a[0] == '\0') continue;

        if (zt_cli_match_flag(a, "h") || zt_cli_match_flag(a, "help")) {
            out->show_help = true;
            return 0;
        }
        if (zt_cli_match_flag(a, "list-midi-in")) {
            out->list_midi_in = true;
            continue;
        }
        const char *value = NULL;
        int r = zt_cli_match_flag_with_value(argc, argv, &i, "midi-in", &value);
        if (r < 0) return -1;
        if (r > 0) {
            if (out->num_midi_in_ports < ZtCliArgs::MAX_MIDI_IN_FLAGS) {
                out->midi_in_ports[out->num_midi_in_ports++] = value;
            } else {
                fprintf(stderr, "zt: too many --midi-in flags (max %d)\n",
                        ZtCliArgs::MAX_MIDI_IN_FLAGS);
                return -1;
            }
            continue;
        }
        r = zt_cli_match_flag_with_value(argc, argv, &i, "midi-clock", &value);
        if (r < 0) return -1;
        if (r > 0) { out->midi_clock_port = value; continue; }

        if (a[0] == '-') {
            fprintf(stderr, "zt: unknown flag '%s' (try --help)\n", a);
            return -1;
        }
        // First positional = song filename. Subsequent positionals ignored.
        if (!out->song_filename) out->song_filename = a;
    }
    return 0;
}

// Resolve a MIDI in port spec to an index in MidiIn->midiInDev[].
// Accepts an integer index or a case-sensitive substring of the device
// name. Returns -1 if not found.
static int zt_resolve_midi_in_port(const char *spec) {
    if (!spec || !MidiIn) return -1;
    char *end = NULL;
    long n = strtol(spec, &end, 10);
    if (end && *end == '\0' && n >= 0 && (unsigned)n < MidiIn->numMidiDevs) {
        return (int)n;
    }
    for (unsigned j = 0; j < MidiIn->numMidiDevs; j++) {
        if (MidiIn->midiInDev[j] && strstr(MidiIn->midiInDev[j]->szName, spec)) {
            return (int)j;
        }
    }
    return -1;
}

int main(int argc, char *argv[])
{
  // Install fatal-signal handlers as the very first thing so any
  // crash during startup, song load, or playback drops a useful
  // backtrace to stderr + ./zt-crash.log instead of the bare
  //   "fish: Job 1, './zt' terminated by signal SIGABRT (Abort)"
  // that gave no clue which code path crashed.
  zt_install_crash_handler();

  ZtCliArgs cli_args;
  if (zt_parse_cli(argc, argv, &cli_args) != 0) {
      return 2;
  }
  if (cli_args.show_help) {
      const char *prog = (argc > 0 && argv[0]) ? argv[0] : "zt";
      // Strip path so "Usage: /path/to/zt" doesn't dominate the line.
      const char *base = strrchr(prog, '/');
      if (!base) base = strrchr(prog, '\\');
      zt_print_cli_help(base ? base + 1 : prog);
      return 0;
  }

  char *w;
  const char path_sep =
#if defined(_WIN32)
    '\\';
#else
    '/';
#endif

  // Build zt_filename from cwd + the positional song arg (if any).
  // Kept for parity with the historical argv[1] handling; the actual
  // song load below uses cli_args.song_filename directly.
  if (cli_args.song_filename && cli_args.song_filename[0] != '\0') {
      zt_get_current_directory(1024, zt_filename);
      strcat(zt_filename, (path_sep == '\\') ? "\\" : "/");
      strcat(zt_filename, cli_args.song_filename);
  }

  bool launched_from_bundle = false;
  char bundle_resources_path[1024] = "";
#if defined(__APPLE__)
  // When launched as a .app bundle (double-clicked in Finder), argv[0] points
  // at zt.app/Contents/MacOS/zt and the CWD is '/'. Jump into Contents/Resources
  // so relative skin/doc/zt.conf lookups work, remember the absolute Resources
  // path for zt_directory (CUI_Help.cpp and others build absolute paths from it),
  // and suppress the later zt_set_current_directory(argv[0]) override.
  {
    char exe_path[PATH_MAX];
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
      const char *macos_marker = "/Contents/MacOS/";
      char *marker = strstr(exe_path, macos_marker);
      if (marker) {
        *marker = '\0';
        snprintf(bundle_resources_path, sizeof(bundle_resources_path),
                 "%s/Contents/Resources", exe_path);
        if (chdir(bundle_resources_path) == 0) launched_from_bundle = true;
      }
    }
  }
#endif

  char *last_backslash = strrchr(argv[0], '\\');
  char *last_slash = strrchr(argv[0], '/');
  char *last_sep = last_backslash;
  if (last_slash && (!last_sep || last_slash > last_sep)) {
    last_sep = last_slash;
  }

  if (launched_from_bundle) {
    // Inside a .app: set zt_directory to Contents/Resources so help.txt and
    // other <zt_directory>/... lookups resolve to bundled files. cwd is
    // already Contents/Resources (done above), so don't call zt_set_current_directory.
    zt_directory = strdup(bundle_resources_path);
  }
  else if(last_sep) {
    w = last_sep;

    if(w != argv[0]) {
      *w = '\0';
      zt_directory = strdup(argv[0]);
      zt_set_current_directory(argv[0]);
      *w = path_sep;
    }
  }
  else zt_directory = strdup("");

  doredraw++;

  if (!initSDL()) {
    return -1;
  }

  // CLI MIDI handling. Runs after initSDL because that's where MidiIn
  // is populated (setup_midi() inside initSDL fills numMidiDevs and
  // midiInDev[]). --list-midi-in is a one-shot informational mode that
  // prints + exits before we touch any song state.
  if (cli_args.list_midi_in) {
      fprintf(stdout, "MIDI input ports (%u):\n", MidiIn ? MidiIn->numMidiDevs : 0u);
      if (MidiIn) {
          for (unsigned j = 0; j < MidiIn->numMidiDevs; j++) {
              if (MidiIn->midiInDev[j]) {
                  fprintf(stdout, "  [%u] %s\n", j, MidiIn->midiInDev[j]->szName);
              }
          }
      }
      return 0;
  }
  for (int p = 0; p < cli_args.num_midi_in_ports; ++p) {
      const char *spec = cli_args.midi_in_ports[p];
      int idx = zt_resolve_midi_in_port(spec);
      if (idx >= 0) {
          MidiIn->AddDevice(idx);
          fprintf(stderr, "zt: opened MIDI in [%d] %s\n",
                  idx, MidiIn->midiInDev[idx]->szName);
      } else {
          fprintf(stderr, "zt: --midi-in port not found: %s\n", spec);
      }
  }
  if (cli_args.midi_clock_port) {
      int idx = zt_resolve_midi_in_port(cli_args.midi_clock_port);
      if (idx >= 0) {
          MidiIn->AddDevice(idx);
          zt_config_globals.midi_in_sync             = 1;
          zt_config_globals.midi_in_sync_chase_tempo = 1;
          fprintf(stderr, "zt: opened MIDI clock source [%d] %s "
                          "(sync + tempo chase enabled)\n",
                  idx, MidiIn->midiInDev[idx]->szName);
      } else {
          fprintf(stderr, "zt: --midi-clock port not found: %s\n",
                  cli_args.midi_clock_port);
      }
  }

  if (screen_buffer_surface != NULL) {

    initGFX();

    if (cli_args.song_filename && cli_args.song_filename[0] != '\0') {
      song->load((char *)cli_args.song_filename);
    } else {
      if (zt_config_globals.autoload_ztfile) {
        if (strlen(zt_config_globals.autoload_ztfile_filename)) {
          song->load(zt_config_globals.autoload_ztfile_filename);
        }
      }
    }

#ifdef _ENABLE_AUDIO
    if (!zt_backend_audio_start()) {
      return(-1);
    }
#endif

    while (1) {
      CUI_Page *focus_page = NULL;
      if (!window_stack.isempty()) {
        focus_page = window_stack.top();
      } else {
        focus_page = ActivePage;
      }
      bool want_text_input =
          (focus_page && focus_page->UI && focus_page->UI->has_text_focus());
      // Lua console has no UI widget system but still needs text input
      // (SDL_TEXTINPUT events are the only path that delivers typed
      // characters with a valid actual_char; plain key-downs give 0).
      if (cur_state == STATE_LUA_CONSOLE) want_text_input = true;
      if (want_text_input) {
        if (!zt_text_input_is_active) {
          zt_text_input_start();
        }
      } else {
        if (zt_text_input_is_active) {
          zt_text_input_stop();
        }
      }

      SDL_Event e;

      while (SDL_PollEvent(&e)) {
        switch (e.type) {
          case SDL_EVENT_KEY_DOWN:
          case SDL_EVENT_KEY_UP:
            keyhandler(&e.key);
            break;

          case SDL_EVENT_MOUSE_MOTION:
            mousemotionhandler(&e.motion);
            break;

          case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mousedownbuttonhandler(&e.button);
            break;

          case SDL_EVENT_MOUSE_BUTTON_UP:
            mouseupbuttonhandler(&e.button);
            break;

          case SDL_EVENT_MOUSE_WHEEL:
            mousewheelhandler(&e.wheel);
            break;

          case SDL_EVENT_TEXT_INPUT:
            textinputhandler(&e);
            break;

          case SDL_EVENT_QUIT:
            // macOS Cmd+Q / Dock "Quit" / system shutdown: honour the
            // OS-level quit request immediately. Going through quit()
            // pushed a RUSure popup that the OS-driven event loop
            // never delivered keystrokes to (and repeated SDL_EVENT_QUIT
            // events stacked further popups), wedging the app.
            doquit();
            break;

          default:
            if (zt_handle_platform_window_event(&e)) {
              break;
            }
            break;
        }
      }

      if (action(screen_buffer)) {
        break;
      }

      zt_apply_pending_window_title();

      // Drain any MIDI mapping action posts captured by the MIDI
      // input thread since the last frame. Action handlers run here,
      // on the main thread, so they can safely touch song / cursor
      // globals.
      zt_mm_drain_actions();

      static int next_frame_redraw_all = 0;
      if(next_frame_redraw_all) {
        next_frame_redraw_all = 0;
        zt_queue_full_redraw();
      }

	      static int old_resx = 0;
	      static int old_resy = 0;
	      if(old_resx != INTERNAL_RESOLUTION_X || old_resy != INTERNAL_RESOLUTION_Y) {
	        old_resx = INTERNAL_RESOLUTION_X;
	        old_resy = INTERNAL_RESOLUTION_Y;
	        next_frame_redraw_all = 1;
	      }

	      zt_autosave_tick();

	      if(screenmanager.Refresh(screen_buffer)) {
        zt_backend_present_frame();
      }
      else {
        SDL_Delay(1);
      }
    }

#ifdef _ENABLE_AUDIO
    zt_backend_audio_stop();
#endif

    if (screen_buffer_surface != NULL) {
      zt_destroy_surface(screen_buffer_surface);
      screen_buffer_surface = NULL;
    }
    zt_backend_release_frame_resources();
  }

  postAction();
  zt_backend_shutdown_runtime();
  return 0;
}


// ------------------------------------------------------------------------------------------------
//
//
static int zt_backend_toggle_fullscreen(void)
{
    if (!zt_main_window) {
        return 0;
    }

    const bool enable_fullscreen = !bIsFullscreen;
    if (!SDL_SetWindowFullscreen(zt_main_window, enable_fullscreen ? SDL_WINDOW_FULLSCREEN : 0)) {
        return 0;
    }

    bIsFullscreen = enable_fullscreen;
    zt_config_globals.full_screen = enable_fullscreen ? 1 : 0;

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(zt_main_window, &window_w, &window_h);
    if (window_w > 0) {
        RESOLUTION_X = window_w;
    }
    if (window_h > 0) {
        RESOLUTION_Y = window_h;
    }
    SDL_SetWindowMouseGrab(zt_main_window, enable_fullscreen);
    return 1;
}

int attempt_fullscreen_toggle()
{
    if (!zt_backend_toggle_fullscreen()) {
        return 0;
    }

    zt_queue_full_redraw();
    return 1;
}
