#ifndef _CONF_H
#define _CONF_H

#include "list.h"


#define DEFAULT_RESOLUTION_X       1360
#define DEFAULT_RESOLUTION_Y       710


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
        conf(char *filen);
        ~conf();
        //int load(istream *is);
        int load(char *filen);
        int save(char *filen);
        char* get(char *key);
        void set(char *key, char *value,int dat=0);
        int getcolor(char *key, int part);
        void remove(char *key);
};

class ZTConf {

    public:
        ZTConf();
        ~ZTConf();
        int load();
        int save();
        int getFlag(char *key);
        
        // Here are the variables
        char *conf_filename;

        conf *Config;
        int full_screen;
//        int do_fade; // fade_in_out ?
        int auto_open_midi;
        char skin[256];
        char work_directory[256];
        char autoload_ztfile_filename[256];
        int autoload_ztfile;
        int midi_in_sync; // flag_midiinsync
        int auto_send_panic; // flag_autosendpanic
        int highlight_increment;
        int lowlight_increment;
        int pattern_length;
        int key_repeat_time;
        int key_wait_time;
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
        int control_navigation_amount;
        char default_directory[512];
        int record_velocity;
};

#endif
