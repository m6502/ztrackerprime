/*************************************************************************
 *
 * FILE  conf.cpp
 * $Id: conf.cpp,v 1.3 2001/11/23 17:46:15 cmicali Exp $
 *
 * DESCRIPTION 
 *   .conf file loading class.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2000-2001, Christopher Micali <micali@concentric.net>
 * Copyright (c) 2001, Daniel Kahlin <tlr@users.sourceforge.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS´´ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******/


#include "stdafx.h"
#include "conf.h"

int zcmp(char *s1, char *s2) {
    int i=0;
    if (!s1 || !s2) return 0;
    while (s1[i]!=0 && s2[i]!=0)
        if (s1[i] != s2[i])
            return 0;
        else
            i++;
    return -1;
}



conf::conf() {
    filename = NULL;
    hash = new list;
}
conf::conf(char *filen) {
    hash = new list;
    filename = NULL;
    load(filen);
}

conf::~conf() {
    if (filename)
        free(filename);
    delete hash;
}

void conf::stripspace(char *buf) {
    int i=0,l,p,o=0;
    l=strlen(buf);
    if (buf[0] != ' ')
        return;
    while(buf[i] == ' ' && buf[i]) i++;
    for(p=i;p<l;p++)
        buf[o++] = buf[p];
    buf[l-i]=0;
}
int conf::load(char *filen=NULL) {
    FILE *fp;
    char buf[512];
    char *p1,*p2,*p3;
    if (filen) {
        if (filename)
            free(filename);
        filename = strdup(filen);
    }
    if (!(fp=fopen(filename,"rt")))
        return 0;
    while(!feof(fp)) {
        fgets(buf,512,fp);
        stripspace(buf);
        if (buf[0] != '#') {
            p1 = p2 = buf;
            while((*p2 != ':') && (*p2 != 0))
                p2++;
            if (*p2 == ':') {
                *p2 = 0;
                p2+=2;
                while(*p2==' ' && *p2)
                    p2++;
                p3=p2;
                while(*p2) {
                    if (*p2 == '\n' || *p2 =='\r')
                        *p2=0;
                    else
                        p2++;
                }
                p2--;
                while(*p2==' ' && p2>p3)
                    p2--;
                p2++; *p2=0;
                hash->insert(p1,p3);
            }
        }
    }
    fclose(fp);
    return -1;
}

/*
int conf::load(istream *is) {
    char buf[512];
    char *p1,*p2,*p3;
    if (!is) return 0;
    while(!is->eof()) {
        
        is->getline((char *)&buf[0],4,'\n');
//      is >> (char *)buf;
        stripspace(buf);
        if (buf[0] != '#') {
            p1 = p2 = buf;
            while((*p2 != ':') && (*p2 != 0))
                p2++;
            if (*p2 == ':') {
                *p2 = 0;
                p2+=2;
                while(*p2==' ' && *p2)
                    p2++;
                p3=p2;
                while(*p2) {
                    if (*p2 == '\n' || *p2 =='\r')
                        *p2=0;
                    else
                        p2++;
                }
                p2--;
                while(*p2==' ' && p2>p3)
                    p2--;
                p2++; *p2=0;
                hash->insert(p1,p3);
            }
        }
    }
    return -1;
}
*/
int conf::hex2dec(char *c) {
    if (c[0] >= 'A' && c[0]<='F')
        return (c[0]-'A'+10);
    else
        return (c[0]-'0');
}
int conf::getcolor(char *key, int part) { /* 0=r 1=g 2=b */
    int result=0;
    char c[2];
    key = get(key);
    if (key) {
        c[0] = toupper(key[(part*2)+1]);
        c[1] = toupper(key[(part*2)+2]);
        result = hex2dec(&c[0]) << 4;
        result += hex2dec(&c[1]);
    }
    return result;
}
int conf::save(char *filen=NULL) {
    char *key;
    FILE *fp;
    hash->reset();
    if (filen) {
        if (filename)
            free(filename);
        filename = strdup(filen);
    }
    if (!(fp=fopen(filename,"wt")))
        return 0;
    while(key=hash->getnextkey())
        fprintf(fp,"%s: %s\n",key,hash->getstrdata(key));
    fclose(fp);
    return 1;
}

char* conf::get(char *key) {
    return hash->getstrdata(key);
}
void conf::remove(char *key) {
    hash->remove(key);
}
void conf::set(char *key,char *value,int dat) {
    if (hash->remove(key) == -1)
        Sleep(1);
    if (hash->insert(key,value,dat)==-1)
        Sleep(1);
}

ZTConf::ZTConf() {

    // Initialize those global variables
    conf_filename = "zt.conf";

    Config = new conf;//NULL;
    full_screen = 1;
//    do_fade = 1; // fade_in_out ?
    auto_open_midi = 1;
    strcpy(skin, "default");
//    skin[0] = '\0';
    work_directory[0] = '\0';
    midi_in_sync = 0; // flag_midiinsync
    auto_send_panic = 1; // flag_autosendpanic
    highlight_increment = 8;
    lowlight_increment = 8;
    pattern_length = 128;
    key_repeat_time = 30;
    key_wait_time = 250;
    midi_clock = 1; // default_midiclock;
    midi_stop_start = 1; // default_midistopstart;
    instrument_global_volume = 127;
    cur_edit_mode = VIEW_REGULAR;
    default_tpb = 8;
    default_bpm = 138;
    prebuffer_rows = 8;
    step_editing = 1;
	centered_editing = 1;
    screen_width = 640;
    screen_height = 480;
    autoload_ztfile = 0;
    autoload_ztfile_filename[0] = '\0';
}

ZTConf::~ZTConf() {
    if (Config)
        delete Config;
}

int ZTConf::load() {
    if (!Config->load(conf_filename))
        return -1;
//    colorfile = Config->get("color_file");
//    strcpy(COLORFILE,colorfile);
    char *temp = Config->get("skin");
    if(temp)
        strcpy(skin,temp);
    ////////////////////////////////////////////////
    // here we load some new options
    
    if(Config->get("screen_width"))
        screen_width = atoi(Config->get("screen_width"));
    if(Config->get("screen_height"))
        screen_height = atoi(Config->get("screen_height"));


    if(Config->get("default_pattern_length"))
        pattern_length = atoi(Config->get("default_pattern_length"));
    if(Config->get("default_instrument_global_volume"))
        instrument_global_volume = atoi(Config->get("default_instrument_global_volume"));
    if(Config->get("default_highlight_increment"))
        highlight_increment = atoi(Config->get("default_highlight_increment"));
    if(Config->get("default_lowlight_increment"))
        lowlight_increment = atoi(Config->get("default_lowlight_increment"));
	if(Config->get("default_view_mode"))
		cur_edit_mode = atoi(Config->get("default_view_mode"));
    
    ////////////////////////////////////////////////
    
    if (Config->get("key_repeat"))
        key_repeat_time = atoi(Config->get("key_repeat"));
    if (Config->get("key_wait"))
        key_wait_time = atoi(Config->get("key_wait"));
    if (Config->get("prebuffer_rows"))
        prebuffer_rows = atoi(Config->get("prebuffer_rows"));
//    if (Config->get("do_fade"))
//        do_fade = atoi(Config->get("do_fade"));
//    FADEINOUT = do_fade;

    if (Config->get("send_panic_on_stop"))
        auto_send_panic = getFlag("send_panic_on_stop");
    if (Config->get("midi_in_sync"))
        midi_in_sync = getFlag("midi_in_sync");

    full_screen = getFlag("fullscreen");
    step_editing = getFlag("step_editing");
    auto_open_midi = getFlag("auto_open_midi");
    autoload_ztfile = getFlag("autoload_ztfile");
	centered_editing = getFlag("centered_edit");
    if(Config->get("autoload_ztfile_filename"))
        strcpy(autoload_ztfile_filename, Config->get("autoload_ztfile_filename"));

    return(0);
}

int ZTConf::getFlag(char *key) {
    char *a = Config->get(key);
    if (a) {
        if (zcmp(a,"yes"))
            return 1;
        if (zcmp(a,"no"))
            return 0;
        int i = atoi(a);
        if (i>0)
            return 1;
    }
    return 0;
}

int ZTConf::save() {
    char s[512];    
    
    sprintf(s, "%d", screen_width);
    Config->set("screen_width", s);
    sprintf(s, "%d", screen_height);
    Config->set("screen_height", s);

    sprintf(s, "%d", pattern_length);
    Config->set("default_pattern_length", s);

    sprintf(s, "%d", instrument_global_volume);
    Config->set("default_instrument_global_volume", s);
    sprintf(s, "%d", highlight_increment);
    Config->set("default_highlight_increment", s);
    sprintf(s, "%d", lowlight_increment);
    Config->set("default_lowlight_increment", s);

    ////////////////////////////////////////////////
    
    sprintf(s, "%d", key_repeat_time);
    Config->set("key_repeat", s);
    sprintf(s, "%d", key_wait_time);
    Config->set("key_wait", s);
    sprintf(s, "%d", prebuffer_rows);
    Config->set("prebuffer_rows", s);
    sprintf(s, "%d", cur_edit_mode);
    Config->set("default_view_mode", s);

    Config->set("skin", skin);

    if (auto_send_panic)
        Config->set("send_panic_on_stop","yes");
    else
        Config->set("send_panic_on_stop","no");
    
    if (midi_in_sync)
        Config->set("midi_in_sync","yes");
    else
        Config->set("midi_in_sync","no");

    if (full_screen)
        Config->set("fullscreen","yes");
    else
        Config->set("fullscreen","no");

    if (auto_open_midi)
        Config->set("auto_open_midi","yes");
    else
        Config->set("auto_open_midi","no");

    if (step_editing)
        Config->set("step_editing","yes");
    else
        Config->set("step_editing","no");

    if (autoload_ztfile)
        Config->set("autoload_ztfile","yes");
    else
        Config->set("autoload_ztfile","no");

    if (centered_editing)
        Config->set("centered_edit","yes");
    else
        Config->set("centered_edit","no");

    sprintf(s, "%s", autoload_ztfile_filename);
    Config->set("autoload_ztfile_filename", s);
    
    
    
    
    Config->save(conf_filename);
    return(0);
}
