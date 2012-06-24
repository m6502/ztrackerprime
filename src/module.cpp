/*************************************************************************
 *
 * FILE  module.cpp
 * $Id: module.cpp,v 1.19 2001/12/10 06:10:51 cmicali Exp $
 *
 * DESCRIPTION 
 *   Module structures and event memory handling.
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

#include "zt.h"

// NOT IN VL CH LEN fx PARM*5


int default_instrument_global_volume=0x7F;

/* these are the defaults for an instrument.  
   they are also used for determining if an instrument is empty */
#define ZTM_INST_DEFAULT_PATCH           -1
#define ZTM_INST_DEFAULT_BANK            -1
#define ZTM_INST_DEFAULT_MIDI_DEVICE     0xff
#define ZTM_INST_DEFAULT_CHANNEL         0
#define ZTM_INST_DEFAULT_TRANSPOSE       0x0
#define ZTM_INST_DEFAULT_DEFAULT_VOLUME  0x7f
#define ZTM_INST_DEFAULT_DEFAULT_LENGTH  24

// <Manu>
//#define ZTM_INST_DEFAULT_FLAGS           INSTFLAGS_NONE
#define ZTM_INST_DEFAULT_FLAGS           INSTFLAGS_TRACKERMODE


instrument::instrument(int p) 
{
  patch          = ZTM_INST_DEFAULT_PATCH ;
  bank           = ZTM_INST_DEFAULT_BANK ;
  midi_device    = ZTM_INST_DEFAULT_MIDI_DEVICE ;
  channel        = ZTM_INST_DEFAULT_CHANNEL ;
  transpose      = ZTM_INST_DEFAULT_TRANSPOSE ;
  default_volume = ZTM_INST_DEFAULT_DEFAULT_VOLUME ;
  global_volume  = default_instrument_global_volume ;
  default_length = ZTM_INST_DEFAULT_DEFAULT_LENGTH ;
  flags          = ZTM_INST_DEFAULT_FLAGS ;
  
  // this is not good!  The default ought to be a 0 in the first position,
  // but the ui expects this to be all spaces!
  memset(title,' ',ZTM_INSTTITLE_MAXLEN);
  title[ZTM_INSTTITLE_MAXLEN-1]=0;

  // <Manu>
  MarkAsUnused() ;
}
    
instrument::~instrument(void) {
}

int instrument::isempty(void) {
    if (patch != ZTM_INST_DEFAULT_PATCH)
        return 0;
    if (bank != ZTM_INST_DEFAULT_BANK)
        return 0;
    if (midi_device != ZTM_INST_DEFAULT_MIDI_DEVICE)
        return 0;
    if (channel != ZTM_INST_DEFAULT_CHANNEL)
        return 0;
    if (transpose != ZTM_INST_DEFAULT_TRANSPOSE)
        return 0;
    if (default_volume != ZTM_INST_DEFAULT_DEFAULT_VOLUME)
        return 0;
    if (global_volume != default_instrument_global_volume)
        return 0;
    if (default_length != ZTM_INST_DEFAULT_DEFAULT_LENGTH)
        return 0;
    if (flags != ZTM_INST_DEFAULT_FLAGS)
        return 0;

    // check if the title is the default
    // this is not good!  The default ought to be a 0 in the first position,
    // but the ui expects this to be all spaces!
    for (int i=0; i<ZTM_INSTTITLE_MAXLEN-1; i++) {
        if (title[i]!=' ')
            return 0;
    }
    if (title[ZTM_INSTTITLE_MAXLEN-1] != 0)
        return 0;

    // all are the same, we are an empty instrument.
    return -1;
}

event::event(void) {
    note = 0x80;
    inst = 0xFF;
    vol = 0x80;
    length = 0x0000;
    effect = 0xFF;
    effect_data = 0x0000;
    next_event = NULL;
}
event::event(event *e) {
    note = e->note;
    inst = e->inst;
    vol = e->vol;
    length = e->length;
    effect = e->effect;
    effect_data = e->effect_data;
    row = e->row;
    next_event = NULL;
}
event::~event(void) {
}

int event::blank(void) {
    if ( (note == 0x80) &&
         (inst > ZTM_MAX_INSTS) &&
         (vol >= 0x80) &&
         (length == 0x0000) &&
         (effect == 0xFF) &&
         (effect_data == 0x0000) )
         return 1;
    return 0;
}

track::track(short int len) {
    reset();
    event_list = NULL;
    length = len;
}

track::~track(void) {
    event *e,*next;
    e = event_list;
    while(e) {
        next = (event *)e->next_event;
        delete e;
        e = next;
    }
}
void track::reset(void) {
    default_inst = MAX_INSTS;
    default_fxp = 0;
    default_length = 0;
    default_controller = 0x0;
    default_data = 0x0;
    default_delay = 0x0;
    last_note = 0x80;
    last_inst = MAX_INSTS;
    pitch_setting = 0x2000;
    pitch_slide=0;
}
void track::del_event(unsigned short int row, int needlock) {
    event *e,*e2;
    if (event_list) {
        if (needlock)
            lock_mutex(song->hEditMutex,EDIT_LOCK_TIMEOUT);

        if (event_list->row == row) {
            e = (event *)event_list->next_event;
            delete event_list;
            event_list = e;
            file_changed++;
        } else {
            if (e=get_next_event(row)) {
                e2 = (event *)e->next_event;
                e->next_event = e2->next_event;
                delete e2;
                file_changed++;
            }
        }
        if (needlock)
            unlock_mutex(song->hEditMutex);
    }
}


void track::ins_row(unsigned short int which) {
    event *e,*deleted=NULL,*n=NULL;

    if (lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT)) {

        e = get_event(length-1);
        if (e) {
            n = (event *)e->next_event;
            deleted = e;
            delete e;
        }
        e = event_list;
        if (e == deleted) { 
            e = event_list = n;
        }
        while(e) {
            if (e->row>=which)
                e->row++;
            if (deleted) {
                if (e->next_event == deleted)
                    e->next_event = n;
            }
            e = (event *)e->next_event;
        }
        file_changed++;
        unlock_mutex(song->hEditMutex);
    }
}


void track::update_event(unsigned short int row, int note, int inst, int vol, int length, int effect, int effect_data) {
    event *e;
    int a;
    if (lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT)) {
        

        if (e = get_event(row))
            a=0;
        else {
            e = new event;
            e->row = row;
            a=1;
        }
        if (note>=0)
            e->note = (unsigned char)note;
        if (vol>=0)
            e->vol = (unsigned char)vol;
        if (inst>=0)
            e->inst = (unsigned char)inst;
        if (length>=0)
            e->length = (unsigned short int)length;
        if (effect>=0)
            e->effect = (unsigned char)effect;
        if (effect_data>=0)
            e->effect_data = (unsigned short int)effect_data;
        if (a) {
            if (e->blank() || row>=this->length) 
                delete e;
            else {
                e->next_event = event_list;
                event_list = e;
            }
        } else {
            if (e->blank())
                del_event(row,0);
        }
        file_changed++;
        unlock_mutex(song->hEditMutex);
    }
}


void track::update_event_safe(unsigned short int row, int note, int inst, int vol, int length, int effect, int effect_data) {
    event *e;
    int a;
    if (lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT)) {

        if (row>=this->length) row=this->length-1;

        if (e = get_event(row))
            a=0;
        else {
            e = new event;
            e->row = row;
            a=1;
        }
    
        if (note>=0)
            e->note = (unsigned char)note;
        if (vol>0x7f) vol=0x80;
        if (vol>=0)
            e->vol = (unsigned char)vol;
        if (inst>=ZTM_MAX_INSTS) inst=ZTM_MAX_INSTS-1;
        if (inst>=0)
            e->inst = (unsigned char)inst;
        if (length>999)length=999;
        if (length>=0)
            e->length = (unsigned short int)length;
        if (effect>=0)
            e->effect = (unsigned char)effect;
        if (effect_data>=0)
            e->effect_data = (unsigned short int)effect_data;
        if (a) {
            if (e->blank() || row>=this->length) 
                delete e;
            else {
                e->next_event = event_list;
                event_list = e;
            }
        } else {
            if (e->blank())
                del_event(row,0);
        }
        file_changed++;
        unlock_mutex(song->hEditMutex);
    }
}


void track::update_event(unsigned short int row, event *src) {
    int note = src->note;
    int inst = src->inst;
    int vol = src->vol;
    int length = src->length;
    int effect = src->effect;
    int effect_data = src->effect_data;
    event *e;
    int a;

    if (lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT)) {
        

        if (e = get_event(row))
            a=0;
        else {
            e = new event;
            e->row = row;
            a=1;
        }
        if (note>=0)
            e->note = (unsigned char)note;
        if (vol>=0)
            e->vol = (unsigned char)vol;
        if (inst>=0)
            e->inst = (unsigned char)inst;
        if (length>=0)
            e->length = (unsigned short int)length;
        if (effect>=0)
            e->effect = (unsigned char)effect;
        if (effect_data>=0)
            e->effect_data = (unsigned short int)effect_data;
        if (a) {
            if (e->blank() || row>=this->length) 
                delete e;
            else {
                e->next_event = event_list;
                event_list = e;
            }
        } else {
            if (e->blank())
                del_event(row,0);
        }
        file_changed++;
        unlock_mutex(song->hEditMutex);
    }
}

void track::del_row(unsigned short int which) {
    event *e;
/*
    int timeout = 0;
    while(ztPlayer->edit_lock && timeout<EDIT_LOCK_TIMEOUT) {
        timeout++; SDL_Delay(1);
    } if (ztPlayer->edit_lock) return;
    ztPlayer->edit_lock = 1;
*/
    del_event(which,0);
    e = event_list;
    while(e) {
        if (e->row>which)
            e->row--;
        e = (event *)e->next_event;
    }
    file_changed++;

//  ztPlayer->edit_lock = 0;
}

event* track::get_event(unsigned short int row) {
    event *e;
    e = event_list;
    while(e) {
        if (e->row == row)
            return e;
        e = (event *)e->next_event;
    }
    return NULL;
}
event* track::get_next_event(unsigned short int row) {
    event *e,*e2;
    e = event_list;
    while(e) {
        if (e->next_event) {
            e2 = (event *)e->next_event;
            if (e2->row == row)
                return e;
        }
        e = (event *)e->next_event;
    }
    return NULL;
}

void pattern::resize(int newsize) {
    int j;
    if (newsize<32 || newsize>999) return;
    if (newsize == this->length) return;
    file_changed++;

    for (j=0;j<ZTM_MAX_TRACKS;j++)
        this->tracks[j]->length = newsize;

    if (newsize>this->length) {
        this->length = newsize;
        return;
    }

    for (j=0;j<ZTM_MAX_TRACKS;j++)
        for (int i=newsize+1; i<this->length; i++)
            this->tracks[j]->del_event(i);
    this->length=newsize;
}

int pattern::isempty(void) {
    for (int j=0;j<ZTM_MAX_TRACKS;j++)
        if (this->tracks[j]->event_list != NULL)
            return 0;
    return -1;
}

pattern::pattern(void) {
    int i;
    length = zt_config_globals.pattern_length;
    for(i=0;i<ZTM_MAX_TRACKS;i++)
        tracks[i] = new track(length);
}
pattern::pattern(int len) {
    int i;
    for(i=0;i<ZTM_MAX_TRACKS;i++)
        tracks[i] = new track(length);
}
pattern::~pattern(void) {
    int i;
    for(i=0;i<ZTM_MAX_TRACKS;i++)
        delete tracks[i];
}


/*************************************************************************
 *
 * NAME  songmsg::songmsg()
 *
 * SYNOPSIS
 *   songmsg::songmsg (size)
 *   songmsg::songmsg (int)
 *
 * DESCRIPTION
 *   Constructor for songmsg.  Create a songmessage of size 'size'
 *
 * INPUTS
 *   size             - size.
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
songmsg::songmsg() {
    this->songmessage = new CDataBuf;
}

/*************************************************************************
 *
 * NAME  songmsg::~songmsg()
 *
 * SYNOPSIS
 *   songmsg::~songmsg ()
 *   songmsg::~songmsg (void)
 *
 * DESCRIPTION
 *   Destructor for songmsg.  Free up resources.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
songmsg::~songmsg() {
    delete this->songmessage;
}


/*************************************************************************
 *
 * NAME  songmsg::isempty()
 *
 * SYNOPSIS
 *   ret = songmsg::isempty ()
 *   int songmsg::isempty (void)
 *
 * DESCRIPTION
 *   Determine if the songmessage is empty.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   ret              - TRUE if songmessage is empty.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
int songmsg::isempty() {
    return this->songmessage->isempty();
}



#ifdef USE_MIDIMACROS
/*************************************************************************
 *
 * NAME  midimacro::midimacro()
 *
 * SYNOPSIS
 *   midimacro::midimacro ()
 *   midimacro::midimacro (void)
 *
 * DESCRIPTION
 *   Constructor for midimacro.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
midimacro::midimacro(void) {
    this->name[0]=0;
    this->data[0]=ZTM_MIDIMAC_END;
}

/*************************************************************************
 *
 * NAME  midimacro::~midimacro()
 *
 * SYNOPSIS
 *   midimacro::~midimacro ()
 *   midimacro::~midimacro (void)
 *
 * DESCRIPTION
 *   Destructor for midimacro.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
midimacro::~midimacro(void) {
}

/*************************************************************************
 *
 * NAME  midimacro::isempty()
 *
 * SYNOPSIS
 *   ret = midimacro::isempty ()
 *   int midimacro::isempty (void)
 *
 * DESCRIPTION
 *   Determine if the midimacro is empty.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   ret              - TRUE if midimacro is empty.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
int midimacro::isempty(void) {
    return (this->data[0]==ZTM_MIDIMAC_END && this->name[0]==0);
}

#endif /* USE_MIDIMACROS */

#ifdef USE_ARPEGGIOS
/*************************************************************************
 *
 * NAME  arpeggio::arpeggio()
 *
 * SYNOPSIS
 *   arpeggio::arpeggio ()
 *   arpeggio::arpeggio (void)
 *
 * DESCRIPTION
 *   Constructor for arpeggio.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
arpeggio::arpeggio(void) {
    int i,j;
    this->name[0]=0;
    this->repeat_pos=0;
    this->length=0;
    this->num_cc=ZTM_ARPEGGIO_NUM_CC;
    for (i=0; i<ZTM_ARPEGGIO_NUM_CC; i++)
        this->cc[i]=ZTM_ARP_EMPTY_CC;

    for (i=0; i<ZTM_ARPEGGIO_LEN; i++) {
        this->pitch[i]=ZTM_ARP_EMPTY_PITCH;
        for (j=0; j<ZTM_ARPEGGIO_NUM_CC; j++) 
            this->ccval[j][i]=ZTM_ARP_EMPTY_CCVAL;
    }
}

/*************************************************************************
 *
 * NAME  arpeggio::~arpeggio()
 *
 * SYNOPSIS
 *   arpeggio::~arpeggio ()
 *   arpeggio::~arpeggio (void)
 *
 * DESCRIPTION
 *   Destructor for arpeggio.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
arpeggio::~arpeggio(void) {
}

/*************************************************************************
 *
 * NAME  arpeggio::isempty()
 *
 * SYNOPSIS
 *   ret = arpeggio::isempty ()
 *   int arpeggio::isempty (void)
 *
 * DESCRIPTION
 *   Determine if the arpeggio is empty.
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   ret              - TRUE if arpeggio is empty.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
int arpeggio::isempty(void) {
    return (this->length==0 && this->name[0]==0);
}

#endif /* USE_ARPEGGIOS */

zt_module::zt_module(int t,int b) {
    statusstr=NULL;
    tpb = t; bpm = b;
    hEditMutex = CreateMutex(NULL, FALSE, TEXT("EditLock"));
    init();
}

zt_module::~zt_module() {
    de_init();
    
}

void zt_module::init(void) {
    int i;
    memset(title,0,ZTM_SONGTITLE_MAXLEN);
    memset(filename,0,ZTM_FILENAME_MAXLEN);
    
    songmessage = new songmsg();
//    songmessage = new CDataBuf();
    
    for(i=0;i<ZTM_MAX_PATTERNS;i++)
        patterns[i] = new pattern;
    for(i=0;i<ZTM_MAX_INSTS;i++)
        instruments[i] = new instrument(i);
    for(i=0;i<ZTM_MAX_TRACKS;i++)
        track_mute[i] = 0;
    for(i=0;i<ZTM_ORDERLIST_LEN;i++)
        orderlist[i] = 0x100; // 0x100 = Break, 0x101 = Skip
#ifdef USE_ARPEGGIOS
    for(i=0;i<ZTM_MAX_ARPEGGIOS;i++)
        arpeggios[i]=NULL;
#endif /* USE_ARPEGGIOS */
#ifdef USE_MIDIMACROS
    for(i=0;i<ZTM_MAX_MIDIMACROS; i++)
        midimacros[i]=NULL;
#endif /* USE_MIDIMACROS */
    flag_SendMidiClock = zt_config_globals.midi_clock;
    flag_SendMidiStopStart = zt_config_globals.midi_stop_start;
    flag_SlideOnSubtick = 1;
    version = ZT_MODULE_VERSION;
}

void zt_module::de_init(void) {
    int i;

    // delete patterns
    for(i=0;i<ZTM_MAX_PATTERNS;i++) {
        if (this->patterns[i])
                delete this->patterns[i];
        this->patterns[i] = NULL;
    }

    // delete instruments
    for(i=0;i<ZTM_MAX_INSTS;i++) {
        if (this->instruments[i])
            delete this->instruments[i];
        this->instruments[i] = NULL;
    }

    // delete songmessage
    if (this->songmessage)
        delete this->songmessage;

#ifdef USE_ARPEGGIOS
    // delete arpeggios
    for(i=0; i<ZTM_MAX_ARPEGGIOS; i++) {
        if (this->arpeggios[i])
            delete this->arpeggios[i];
        this->arpeggios[i]=NULL;
    }
#endif /* USE_ARPEGGIOS */
#ifdef USE_MIDIMACROS
    // delete midimacros
    for(i=0; i<ZTM_MAX_MIDIMACROS; i++) {
        if (this->midimacros[i])
            delete this->midimacros[i];
        this->midimacros[i]=NULL;
    }
#endif /* USE_MIDIMACROS */

}

void zt_module::init_pattern(int which) {
    if (patterns[which])
        delete patterns[which];
    patterns[which] = new pattern;
}

void zt_module::reset(void) {
    int i,j;
    for(i=0;i<ZTM_MAX_PATTERNS;i++) {
        if (patterns[i]) {
            for(j=0;j<ZTM_MAX_TRACKS;j++)
                if (patterns[i]->tracks[j])
                    patterns[i]->tracks[j]->reset();
        }
    }
    file_changed++;
}
/* eof */

