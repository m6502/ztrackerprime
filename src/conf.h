#ifndef _CONF_H
#define _CONF_H

#include "list.h"

#define MAX_PATH 260


#define DEFAULT_RESOLUTION_X       1024
#define DEFAULT_RESOLUTION_Y       700

enum E_edit_viewmode { VIEW_SQUISH, VIEW_REGULAR, VIEW_FX, VIEW_BIG }; //, VIEW_EXTEND };

// Where to land after a successful song load. Default is Instrument
// Editor (F3) — matches the long-standing behaviour. Pattern Editor
// (F2) suits arrangers who go straight to writing notes; Song Config
// (F11) suits people who want to verify BPM/TPB/order list first.
enum E_post_load_page {
    POST_LOAD_PATTERN_EDIT = 0,
    POST_LOAD_INST_EDIT,
    POST_LOAD_PLAYSONG,        // F5 (Playsong page)
    POST_LOAD_SONG_CONFIG,     // F11 (Order list / Song config)
    POST_LOAD_SONG_MESSAGE     // F10 (Song message editor)
};
#define POST_LOAD_PAGE_COUNT 5


//  New class ZTConf puts all global variables in one place
//  with convenient high level functions to handle I/O



class conf {

    private:
        list *hash;
        char *filename;
        void stripspace(char *buf);
        int hex2dec(char *c);
    public:
        conf();
        conf(const char *filen);
        ~conf();
        //int load(istream *is);
        int load(const char *filen);
        int save(const char *filen);
        char* get(const char *key);
        void set(const char *key, const char *value,int dat=0);
        int getcolor(const char *key, int part);
        void remove(const char *key);
};

class ZTConf {

    public:
        ZTConf();
        ~ZTConf();
        int load();
        int save();
        int getFlag(const char *key);
        
        // Here are the variables
        conf *Config;
        int full_screen;
//        int do_fade; // fade_in_out ?
        int auto_open_midi;
        char skin[MAX_PATH + 1];
        char work_directory[MAX_PATH + 1];
        char autoload_ztfile_filename[MAX_PATH + 1];
        int autoload_ztfile;
        int midi_in_sync; // flag_midiinsync
        int midi_in_sync_chase_tempo; // slave bpm to incoming 0xF8 MIDI Clock
        int auto_send_panic; // flag_autosendpanic
        int highlight_increment;
        int lowlight_increment;
        int pattern_length;
        int key_repeat_time;
        int key_wait_time;
        int autosave_interval_seconds;
        int midi_clock; // default_midiclock;
        int midi_stop_start; // default_midistopstart;
        int instrument_global_volume;
        int cur_edit_mode;
        int default_tpb;
        int default_bpm;
        int prebuffer_rows;
        int step_editing;
        int centered_editing;
        int screen_width;
        int screen_height;
        float zoom;
        char scale_filter[32];
        int control_navigation_amount;
        char default_directory[MAX_PATH + 1];
        int record_velocity;
        int post_load_page;        // E_post_load_page — page to switch to after a successful load
        char window_icon[MAX_PATH + 1];
        // How far the cursor advances after the 4 / 8 audition keys
        // fire on the note column. 0 = stay put, 1 = always step by
        // one row, 2 = use the current EditStep value (legacy behaviour).
        // Default is 1 -- the historical behaviour drove the cursor by
        // EditStep, which surprised users who set EditStep > 1 to
        // sequence at coarse intervals but still wanted single-row
        // auditioning.
        int note_audition_step_mode;

        // Folder containing CCizer-format `.txt` files for the CC Console
        // (Shift+F3). Empty = use the default-resolve order in
        // zt_ccizer_resolve_folder().
        char ccizer_folder[MAX_PATH + 1];
};

// Allowed values for zt_config_globals.note_audition_step_mode.
#define ZT_NAS_NONE     0
#define ZT_NAS_ONE      1
#define ZT_NAS_EDITSTEP 2

#endif
