/*************************************************************************
 *
 * FILE  ztfile-io.cpp
 * $Id: ztfile-io.cpp,v 1.28 2001/10/31 06:26:55 cmicali Exp $
 *
 * DESCRIPTION 
 *   .zt module load/save routines.
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
#include <stdarg.h>
#include "zt.h"

/* test flag, define to force uncompressed song files */
//#define SAVE_UNCOMPRESSED

/* convert on non slide on subtick slides */
#define CONVERT_SLIDEONSUBTICK

/* convert on non precision slides */
//#define CONVERT_SLIDEPRECISION

extern int load_lock;

#define FILEFLAG_SEND_MIDI_CLOCK        (1<<0)
#define FILEFLAG_SEND_MIDI_STOP_START   (1<<1)
#define FILEFLAG_COMPRESSED             (1<<2)
#define FILEFLAG_SLIDEONSUBTICK         (1<<3)
#define FILEFLAG_SLIDEPRECISION         (1<<4)

#define ZT_FILE_HEADER      "ZThd"
#define ZT_FILE_HEADER_NEW  "ZThd"


// ------------------------------------------------------------------------------------------------
//
//
void zt_module::writedata(char *data, int size, int compressed, std::ofstream &of, DeflateStream *o) 
{
  if(compressed) {
    if (o) o->write(data,size);
  } 
  else of.write(data,size);
}



// ------------------------------------------------------------------------------------------------
//
//
int zt_module::readdata(char *data, int size, int compressed, std::ifstream &ifs, InflateStream *i) 
{
  if (compressed) {

    if (i) {

      i->read(data,size);
      if (i->eof()) return -1;
    }
  }
  else {
    
    ifs.read(data,size);
    if (ifs.eof()) return -1;
  }

  return(0) ;
}



// ------------------------------------------------------------------------------------------------
//
//
void zt_module::writeblock(char *headid, CDataBuf *buf, int compressed, std::ofstream &of, DeflateStream *o) 
{
  int size;
  
  writedata(headid,4,compressed,of,o); 
  size = buf->getsize();
  writedata((char *)&size,sizeof(int),compressed,of,o);
  writedata((char *)buf->getbuffer(),size,compressed,of,o);
  buf->flush();
}




// ------------------------------------------------------------------------------------------------
//
//
int zt_module::readblock(char headid[5], CDataBuf *buf, int compressed, std::ifstream &ifs, InflateStream *is)
{
  int size,ret;
  char *lpBuf;
  
  buf->flush();

  ret = readdata(headid,4,compressed,ifs,is); headid[4]=0;
  if (ret == -1) return -1;

  ret = readdata((char *)&size,sizeof(int),compressed,ifs,is);
  if (ret == -1) return -1;

  buf->setbufsize(size);
  lpBuf = buf->getbuffer();
  
  if (!lpBuf) return -1; // fail if no buffer
  readdata(lpBuf,size,compressed,ifs,is);
  
  buf->reset_read() ;
  return(size) ;
}


// ------------------------------------------------------------------------------------------------
//
//
int zt_module::read_ZT_header(CDataBuf *buf, std::ifstream &ifs) 
{
  int size;
  int compressed;
  char c,headid[5],*lpBuf;
  
  buf->flush();
  readdata(headid,4,0,ifs,NULL); headid[4]=0;

  if (!cmp_hd(headid,ZT_FILE_HEADER)) {

    if (cmp_hd(headid,"ZTM\x0")) return -2;
    else return -1;
  }

  readdata((char *)&size,sizeof(int),0,ifs,NULL);
  buf->setbufsize(size);
  lpBuf = buf->getbuffer();
  ifs.read(lpBuf,size);
  buf->reset_read();
  this->bpm = buf->getuch();
  this->tpb = buf->getuch();
  c = buf->getuch(); // ignore the max_tracks
  c = buf->getuch(); // get the flags


  for (int i=0; i < ZTM_SONGTITLE_MAXLEN; i++) {
    
    this->title[i] = buf->getch();
  }

  this->title[ZTM_SONGTITLE_MAXLEN - 1] = '\0' ;

  buf->flush();
  
  flag_SendMidiClock     = (c&FILEFLAG_SEND_MIDI_CLOCK)?1:0;
  flag_SendMidiStopStart = (c&FILEFLAG_SEND_MIDI_STOP_START)?1:0;
  flag_SlideOnSubtick    = (c&FILEFLAG_SLIDEONSUBTICK)?1:0;
#ifdef CONVERT_SLIDEPRECISION
  flag_SlidePrecision    = (c&FILEFLAG_SLIDEPRECISION)?1:0;
#endif /* CONVERT_SLIDEPRECISION */
  compressed = (c&FILEFLAG_COMPRESSED)?1:0;
  
  return(compressed) ;
}



// ------------------------------------------------------------------------------------------------
//
//
void zt_module::build_ZT_header(CDataBuf *buf, int compr) 
{
  unsigned char t,b,tracks=ZTM_MAX_TRACKS;

  t = this->tpb;
  b = this->bpm;
  buf->write((const char *)&b, sizeof(unsigned char));
  buf->write((const char *)&t, sizeof(unsigned char));
  buf->write((const char *)&tracks, sizeof(unsigned char));
  
  t=0;
  
  if(flag_SendMidiClock) t|=FILEFLAG_SEND_MIDI_CLOCK;
  if(flag_SendMidiStopStart) t|=FILEFLAG_SEND_MIDI_STOP_START;
  if(compr) t|=FILEFLAG_COMPRESSED;
  if(flag_SlideOnSubtick) t|=FILEFLAG_SLIDEONSUBTICK;

#ifdef CONVERT_SLIDEPRECISION
  if (flag_SlidePrecision) t|=FILEFLAG_SLIDEPRECISION;
#endif /* CONVERT_SLIDEPRECISION */

  buf->write((const char *)&t, sizeof(unsigned char));
  buf->write((const char *)&this->title[0], ZTM_SONGTITLE_MAXLEN);
}




// ------------------------------------------------------------------------------------------------
//
//
void zt_module::build_ZT_order_list(CDataBuf *buf) 
{
  unsigned short int o[ZTM_ORDERLIST_LEN];

  for (int i=0;i<ZTM_ORDERLIST_LEN;i++) o[i] = this->orderlist[i];
  buf->write((char *)&o[0],sizeof(unsigned short int)*ZTM_ORDERLIST_LEN);
}




// ------------------------------------------------------------------------------------------------
//
//
void zt_module::build_ZT_pattern_lengths(CDataBuf *buf) 
{
  unsigned short int o[ZTM_MAX_PATTERNS];

  for (int i=0;i<ZTM_MAX_PATTERNS;i++) o[i] = this->patterns[i]->length;
  buf->write((char *)&o[0],sizeof(unsigned short int)*ZTM_MAX_PATTERNS);
}



// ------------------------------------------------------------------------------------------------
//
//
void zt_module::build_ZT_pattern_properties(CDataBuf *buf) 
{
  unsigned short int o[ZTM_MAX_PATTERNS];

  for (int i=0;i<ZTM_MAX_PATTERNS;i++)
  {
      o[i] = i ;//this->patterns[i]->length;

      // Pattern theme color
      // Pattern bg color
      // Pattern Highlight
      // Pattern Lowlight
      // Pattern name
      // Pattern type
      // Pattern display options
  }

  buf->write((char *)&o[0],sizeof(unsigned short int)*ZTM_MAX_PATTERNS);
}




// ------------------------------------------------------------------------------------------------
//
//
void zt_module::build_ZT_track_mutes(CDataBuf *buf) 
{
  unsigned char tm[ZTM_MAX_TRACKS/8];
  unsigned char p,mask;
  
  for (int i=0;i<ZTM_MAX_TRACKS/8;i++) {
    
    mask = 0x1;
    tm[i] = 0;

    for (p=0;p<8;p++) {

      if (this->track_mute[(i*8)+p]) tm[i] |= mask;
      mask <<= 1;
    }
  }

  buf->write((char *)&tm[0],sizeof(unsigned char)*(ZTM_MAX_TRACKS/8));
}

/*************************************************************************
 *
 * NAME  zt_module::build_song_message()
 *
 * SYNOPSIS
 *   ret = zt_module::build_song_message (buf)
 *   int zt_module::build_song_message (CDataBuf *)
 *
 * DESCRIPTION
 *   Build a song message (SMSG) chunk to the buffer.
 *
 * INPUTS
 *   buf              - buffer to write to.
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
void zt_module::build_song_message(CDataBuf *buf) {
    unsigned int len;
    songmsg *smsg;
    //CDataBuf *smsg;

    smsg = this->songmessage;

    //len = smsg->getsize();
//  len = strlen(smsg->songmessage);
    len = smsg->songmessage->getsize();

    buf->pushui(len);
    //buf->write(smsg->getbuffer(), len); 
    buf->write(smsg->songmessage->getbuffer(),len);
}


/*************************************************************************
 *
 * NAME  zt_module::build_arpeggio()
 *
 * SYNOPSIS
 *   ret = zt_module::build_arpeggio (buf, num)
 *   int zt_module::build_arpeggio (CDataBuf *, int)
 *
 * DESCRIPTION
 *   Build a arpeggio (ARPG) chunk to the buffer.
 *
 * INPUTS
 *   buf              - buffer to write to.
 *   num              - arpeggio number.
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
void zt_module::build_arpeggio(CDataBuf *buf, int num) {
    int i,j;
    unsigned short len;
    unsigned char num_cc;
    unsigned short num_entries;
    arpeggio *arp;

    arp=this->arpeggios[num];

    // get num_cc
    num_cc=arp->num_cc;

    // use length as number of entries.
    num_entries=arp->length;

    // write number
    buf->pushusi(num);

    // write name
    len = strlen (arp->name);
    buf->pushusi(len);
    buf->write(arp->name,len);

    buf->pushusi(arp->length);
    buf->pushuc(arp->num_cc);
    buf->pushusi(arp->speed);
    buf->pushusi(arp->repeat_pos);

    // write cc
    for (i=0; i<(int)num_cc; i++)
        buf->pushuc(arp->cc[i]);

    // write the actual arpeggio
    // pitch
    for (i=0; i<(int)num_entries; i++)
        buf->pushusi(arp->pitch[i]);

    // ccval
    for (j=0; j<(int)num_cc; j++) {
        for (i=0; i<(int)num_entries; i++) {
            buf->pushuc(arp->ccval[j][i]);
        }
    }

    return;
}

/*************************************************************************
 *
 * NAME  zt_module::build_MIDI_macro()
 *
 * SYNOPSIS
 *   ret = zt_module::build_MIDI_macro (buf, num)
 *   int zt_module::build_MIDI_macro (CDataBuf *, int)
 *
 * DESCRIPTION
 *   Build a midi macro (MMAC) chunk to the buffer.
 *
 * INPUTS
 *   buf              - buffer to write to.
 *   num              - midi macro number.
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
void zt_module::build_MIDI_macro(CDataBuf *buf, int num) {
    int i;
    unsigned short len,num_entries;
    midimacro *mmac;

    mmac=this->midimacros[num];

    // calculate length by finding end marker.
    for (i=0; i<ZTM_MIDIMACRO_MAXLEN; i++) {
        if (mmac->data[i]==ZTM_MIDIMAC_END)
            break;
    }
    num_entries=i;

    // write number
    buf->pushusi(num);

    // write name
    len = strlen (mmac->name);
    buf->pushusi(len);
    buf->write(mmac->name,len);

    buf->pushusi(num_entries);

    // write the actual midimacro
    for (i=0; i<(int)num_entries; i++)
        buf->pushusi(mmac->data[i]);

    return;
}

/*
unsigned char note;
unsigned char inst;
unsigned char vol;
unsigned char effect;
unsigned short int row;
unsigned short int length;
unsigned short int effect_data;
*/
void zt_module::build_ZT_event_list(CDataBuf *buf) {
    event dEvent, *e;
    unsigned char def_pattern=0;
    unsigned char def_track=0;
    for(int pattern=0;pattern<ZTM_MAX_PATTERNS;pattern++) {
        for (int track=0; track<ZTM_MAX_TRACKS; track++) {
            e = this->patterns[pattern]->tracks[track]->event_list;
            while (e) {

                /* BYTE MARKERS */
                if (e->note != dEvent.note) {
                    dEvent.note = e->note;
                    buf->pushc(0x01); // Note;
                    buf->pushc(e->note);
                }
                if (e->inst != dEvent.inst) {
                    dEvent.inst = e->inst;
                    buf->pushc(0x02); // inst;
                    buf->pushc(e->inst);
                }
                if (e->vol != dEvent.vol) {
                    dEvent.vol = e->vol;
                    buf->pushc(0x03); // vol;
                    buf->pushc(e->vol);
                }
                if (e->effect != dEvent.effect) {
                    dEvent.effect = e->effect;
                    buf->pushc(0x04); // effect;
                    buf->pushc(e->effect);
                }
                if (def_pattern != pattern) {
                    def_pattern = pattern;
                    buf->pushc(0x05); // pattern
                    buf->pushc(def_pattern);
                }
                if (def_track != track) {
                    def_track = track;
                    buf->pushc(0x06); // track
                    buf->pushc(def_track);
                }


                /* WORD Markers */
                if (e->length != dEvent.length) {
                    dEvent.length = e->length;
                    buf->pushc(0x42); // length;
                    buf->pushsi(e->length);
                }
                if (e->effect_data != dEvent.effect_data) {
                    dEvent.effect_data = e->effect_data;
                    buf->pushc(0x43); // effect data;
                    buf->pushsi(e->effect_data);
                }

                buf->pushc(0x41); // Event @ row
                buf->pushsi(e->row);

                e = e->next_event;
            }
        }
    }
}

void zt_module::load_ZT_event_list(CDataBuf *buf) {
    event dEvent;
    unsigned char cmd,c;
    unsigned short tmp_data;
    short int si;
    int dint;
    unsigned char track=0,pattern=0;
    while(!buf->eob()) {
        cmd = buf->getuch();
        if (cmd < 0x40) { // Byte Event
            c = buf->getuch();
            switch(cmd) {
                case 0x01:
                    dEvent.note = c;
                    break;
                case 0x02:
                    dEvent.inst = c;
                    break;
                case 0x03:
                    dEvent.vol = c;
                    break;
                case 0x04:
                    dEvent.effect = c;
                    break;
                case 0x05:
                    pattern = c;
                    break;
                case 0x06:
                    track = c;
                    break;
            }
        } 
        else if (cmd < 0x7F) { // Word event 
            si = buf->getusi();
            switch(cmd) {
                case 0x41:  

                    tmp_data=dEvent.effect_data; // preserve
                    // convert pitch slides
                    if (dEvent.effect=='E' || dEvent.effect=='F') {
#ifdef CONVERT_SLIDEONSUBTICK
                        if (!flag_SlideOnSubtick) {
                            // rescale old pitch slides.
                            // old slides updated only on every tick (i.e once a row)
                            // new slides update every subtick. (i.e 24 ppqn)
                            // this means they depend on which tpb is set.
                            dEvent.effect_data=dEvent.effect_data*tpb/24;
                        }
#endif /* CONVERT_SLIDEONSUBTICK */
#ifdef CONVERT_SLIDEPRECISION
                        if (!flag_SlidePrecision)
                            dEvent.effect_data<<=2;
#endif /* CONVERT_SLIDEPRECISION */
                    }
                    this->patterns[pattern]->tracks[track]->update_event(si,&dEvent);

                    dEvent.effect_data=tmp_data; // restore
                    break;
                case 0x42:
                    dEvent.length = si;
                    break;
                case 0x43:
                    dEvent.effect_data = si;
                    break;
            }
        } 
        else if (cmd < 0xC0) { // DWord event 
            dint = buf->getuch();
        } 
        else { // Text event    
            si = buf->getusi();
            buf->seek(si, CDB_SEEK_FROMCUR);
        }
    }
}


/*
DWORD: size (variable)
BYTE: inst number
WORD: bank
BYTE: patch
BYTE: midi device
BYTE: channel + (flags << 4)
BYTE: default volume
BYTE: global volume
WORD: default length
BYTE: transpose
BYTE: [25] title
*/
void instrument::save(CDataBuf *buf, unsigned char inum) {
    unsigned char c;
    c = this->channel + (this->flags<<4);
    buf->write((const char *)&inum,sizeof(unsigned char));                     // byte
    buf->write((const char *)&this->bank,sizeof(signed short int));            // word
    buf->write((const char *)&this->patch,sizeof(unsigned char));              // byte
    buf->write((const char *)&this->midi_device,sizeof(unsigned char));        // byte
    buf->write((const char *)&c,sizeof(unsigned char));                        // byte
    buf->write((const char *)&this->default_volume,sizeof(unsigned char));     // byte
    buf->write((const char *)&this->global_volume,sizeof(unsigned char));      // byte
    buf->write((const char *)&this->default_length,sizeof(short int));         // word
    buf->write((const char *)&this->transpose,sizeof(signed char));          // byte
    buf->write((const char *)this->title,25);                                  // byte x 25
}




int zt_module::save(char *fn, int compressed) 
{
    static char ls_filename[MAX_PATH + 1];
    int i ;

    DeflateStream *lpDS=NULL;

#ifdef SAVE_UNCOMPRESSED
    compressed = 0;
#endif /* SAVE_UNCOMPRESSED */

    CDataBuf buffer;

    setstatusstr(NULL);

    if (fn) {
        strcpy(ls_filename,fn);
//      strcpy((char *)&this->filename[0],ls_filename);
    }
    fn = (char*)&this->filename[0];
    
    std::ofstream f(fn, std::ios::out | std::ios::binary);
    
    if (f.fail()) {
        setstatusstr("Error saving %s (Could not open file)",fn);
        return -1;  
    }

    // ZThd block is never compressed 
    build_ZT_header(&buffer,compressed);
    writeblock(ZT_FILE_HEADER,&buffer,0,f,lpDS);

    if (compressed) {
//      DeflateStream ds(f); // passing the file stream as a parameter
        lpDS = new DeflateStream(&f);//&ds;
    }

    /* write song message if not empty */
    if (this->songmessage &&  !this->songmessage->isempty()) {
        build_song_message(&buffer);
        writeblock("SMSG",&buffer,compressed,f,lpDS);
    }
    build_ZT_order_list(&buffer);
    writeblock("ZTol",&buffer,compressed,f,lpDS);
    build_ZT_pattern_lengths(&buffer);
    writeblock("ZTpl",&buffer,compressed,f,lpDS);
    build_ZT_pattern_properties(&buffer);
    writeblock("ZTpp",&buffer,compressed,f,lpDS);
    build_ZT_track_mutes(&buffer);
    writeblock("ZTtm",&buffer,compressed,f,lpDS);

    for(i=0;i<ZTM_MAX_INSTS;i++) {
        if (!this->instruments[i]->isempty()) {
            this->instruments[i]->save(&buffer,i);
            writeblock("ZTin",&buffer,compressed,f,lpDS);
        }
    }
    
    for(i=0;i<ZTM_MAX_ARPEGGIOS;i++) {
        if (this->arpeggios[i] && !this->arpeggios[i]->isempty()) {
            build_arpeggio(&buffer,i);
            writeblock("ARPG",&buffer,compressed,f,lpDS);
        }
    }
    
    for(i=0;i<ZTM_MAX_MIDIMACROS;i++) {
        if (this->midimacros[i] && !this->midimacros[i]->isempty()) {
            build_MIDI_macro(&buffer,i);
            writeblock("MMAC",&buffer,compressed,f,lpDS);
        }
    }
    
    build_ZT_event_list(&buffer);
    writeblock("ZTev",&buffer,compressed,f,lpDS);
    
    if (compressed) {

        delete lpDS;
        setstatusstr("Saved %s (compressed)",fn);
    } 
    else setstatusstr("Saved %s (uncompressed)",fn);

    file_changed = 0;
    return 0;
}

int zt_module::cmp_hd(char *s1,char *s2) {
    for(int i=0;i<4;i++)
        if (s1[i] != s2[i])
            return 0;
    return 1;
}


/*************************************************************************
 *
 * NAME  zt_module::load_song_message()
 *
 * SYNOPSIS
 *   ret = zt_module::load_song_message (buf)
 *   int zt_module::load_song_message (CDataBuf *)
 *
 * DESCRIPTION
 *   Load a song message (SMSG) chunk into the module.
 *
 * INPUTS
 *   buf              - buffer to read from.
 *
 * RESULT
 *   ret              - 0 if all is ok.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
int zt_module::load_song_message(CDataBuf *buf) {
    char c;
    unsigned int len,i;
    songmsg *smsg;

    len = buf->getui();

    // delete old song message if it exists
    delete this->songmessage;
    this->songmessage=NULL;

    smsg=new songmsg();
    if (!smsg)
        return -1;


    for (i=0; i<len; i++) {
        c = buf->getuch();
        smsg->songmessage->pushc(c);
    }
    smsg->songmessage->pushc(0x0);

    this->songmessage=smsg;

    return 0;
}

/*************************************************************************
 *
 * NAME  zt_module::load_arpeggio()
 *
 * SYNOPSIS
 *   ret = zt_module::load_arpeggio (buf)
 *   int zt_module::load_arpeggio (CDataBuf *)
 *
 * DESCRIPTION
 *   Load an arpeggio (ARPG) chunk into the module.
 *
 * INPUTS
 *   buf              - buffer to read from.
 *
 * RESULT
 *   ret              - 0 if all is ok.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
int zt_module::load_arpeggio(CDataBuf *buf) {
    char c;
    int i,j;
    unsigned char num_cc;
    unsigned short num,name_len,num_entries;
    arpeggio *arp;

    num = buf->getusi();
    // in range?
    if (num > ZTM_MAX_ARPEGGIOS-1)
        return -1;

    // delete old arpeggio if it exists
    if (this->arpeggios[num]) {
        delete this->arpeggios[num];
        this->arpeggios[num]=NULL;
    }

    arp = new arpeggio;
    if (!arp)
        return -1;

    // get short string (length limited)
    name_len = buf->getusi();
    for (i=0, j=0; i<(int)name_len; i++) {
        c=buf->getch();
        if (i<ZTM_ARPEGGIONAME_MAXLEN-1) {
            arp->name[j]=c;
           j++;
        }
    }
    arp->name[j]=0;

    // does number of entries fit?
    num_entries = buf->getusi();
    if (num_entries > ZTM_ARPEGGIO_LEN) { 
        delete arp;
        return -1;
    }
    // length
    arp->length=num_entries;

    // does number of cc fit?
    num_cc=buf->getuch();
    if (num_cc > ZTM_ARPEGGIO_NUM_CC) { 
        delete arp;
        return -1;
    }
    // num_cc
    arp->num_cc=num_cc;

    // speed
    arp->speed=buf->getusi();

    // repeat_pos
    arp->repeat_pos=buf->getusi();

    // load cc
    for (i=0; i<(int)num_cc; i++)
        arp->cc[i]=buf->getuch();

    // load the actual arpeggio
    // pitch
    for (i=0; i<(int)num_entries; i++)
        arp->pitch[i]=buf->getusi();

    // ccval
    for (j=0; j<(int)num_cc; j++) {
        for (i=0; i<(int)num_entries; i++) {
            arp->ccval[j][i]=buf->getuch();
        }
    }

    // all is well, install the arpeggio
    this->arpeggios[num]=arp;
    return 0;
}

/*************************************************************************
 *
 * NAME  zt_module::load_MIDI_macro()
 *
 * SYNOPSIS
 *   ret = zt_module::load_MIDI_macro (buf)
 *   int zt_module::load_MIDI_macro (CDataBuf *)
 *
 * DESCRIPTION
 *   Load a midimacro (MMAC) chunk into the module.
 *
 * INPUTS
 *   buf              - buffer to read from.
 *
 * RESULT
 *   ret              - 0 if all is ok.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
int zt_module::load_MIDI_macro(CDataBuf *buf) {
    char c;
    int i,j;
    unsigned short num,name_len,num_entries;
    midimacro *mmac;

    num = buf->getusi();
    // in range?
    if (num >= ZTM_MAX_MIDIMACROS)
        return -1;

    // delete old midimacro if it exists
    if (this->midimacros[num]) {
        delete this->midimacros[num];
        this->midimacros[num]=NULL;
    }

    mmac = new midimacro;
    if (!mmac)
        return -1;

    // get short string (length limited)
    name_len = buf->getusi();
    for (i=0, j=0; i<(int)name_len; i++) {
        c=buf->getch();
        if (i<ZTM_MIDIMACRONAME_MAXLEN-1) {
            mmac->name[j]=c;
           j++;
        }
    }
    mmac->name[j]=0;

    // does number of entries fit?
    num_entries = buf->getusi();
    if (num_entries > ZTM_MIDIMACRO_MAXLEN) { 
        delete mmac;
        return -1;
    }

    // load the actual midimacro
    for (i=0; i<(int)num_entries; i++)
        mmac->data[i]=buf->getusi();

    // all is well, install the midimacro
    this->midimacros[num]=mmac;
    return 0;
}

// ------------------------------------------------------------------------------------------------
//
//
void zt_module::load_ZT_track_mutes(CDataBuf *buf) {
    unsigned char tm[MAX_TRACKS/8];
    unsigned char p,mask;
    for (int i=0;i<MAX_TRACKS/8;i++) {
        mask = 0x1;
        tm[i] = buf->getuch();
        for (p=0;p<8;p++) {
            if (tm[i]&mask)
                this->track_mute[(i*8)+p] = 1;
            else
                this->track_mute[(i*8)+p] = 0;
            mask <<= 1;
        }
    }
}

// ------------------------------------------------------------------------------------------------
//
//
void zt_module::load_ZT_pattern_lengths(CDataBuf *buf) {
    unsigned short int size;
    for (int i=0;i<256;i++) {
        size = buf->getusi();
        this->patterns[i]->resize(size);
    }
}


// ------------------------------------------------------------------------------------------------
//
//
void zt_module::load_ZT_pattern_properties(CDataBuf *buf) {
    unsigned short int size;
    for (int i=0;i<256;i++) {
        size = buf->getusi();
        //this->patterns[i]->resize(size);
    }
}


// ------------------------------------------------------------------------------------------------
//
//
void zt_module::load_ZT_order_list(CDataBuf *buf) {
    unsigned short int order;
    for (int i=0;i<ZTM_ORDERLIST_LEN;i++) {
        order = buf->getusi();
        this->orderlist[i] = order;
    }
}

// ------------------------------------------------------------------------------------------------
//
//
void zt_module::load_ZT_instrument(CDataBuf *buf) {
    unsigned char inst;
    inst = buf->getuch();
    if (inst<ZTM_MAX_INSTS)
        this->instruments[inst]->load(buf);
}



// ------------------------------------------------------------------------------------------------
//
//
void instrument::load(CDataBuf *buf) 
{
    unsigned char c;
    c = this->channel + (this->flags<<4);

    this->bank = buf->getsi();
    this->patch = buf->getuch();
    
    this->midi_device = buf->getuch();

    // <Manu> Aqui es donde se puede hacer un parche que ponga el primer dispositivo
    //        MIDI que este abierto en los instrumentos que quieran usar uno que
    //        no este disponible
    
/*    
    if(MidiOut->outputDevices[this->midi_device]->handle==NULL) {
    
      int a ;

      a=5 ;
    }
    else {
    
      int b ;

      b=5 ;
    }


*/


    this->channel = buf->getuch();
    this->flags = this->channel;
    this->channel &= 0x0F;
    this->flags   &= 0xF0; this->flags >>= 4;
    this->default_volume = buf->getuch();
    this->global_volume = buf->getuch();
    this->default_length = buf->getsi();
    this->transpose = buf->getch();
    
    for (int i=0; i < ZTM_INSTTITLE_MAXLEN; i++) {
        
      this->title[i] = buf->getch() ;
    }

    this->title[ZTM_INSTTITLE_MAXLEN - 1] = '\0' ;
}


// ------------------------------------------------------------------------------------------------
//
//
int zt_module::load(char *fn)
{
//  unsigned char buf[10];

    static char ls_filename[MAX_PATH + 1];
    int compressed=0;
    InflateStream *input = NULL;
    ZTImportExport *zti;
    CDataBuf buffer;
    int itpb,ibpm;
    char ititle[ZTM_SONGTITLE_MAXLEN];
    int imidiclock, imidistopstart;
    int islideonsubtick;

#ifdef CONVERT_SLIDEPRECISION
    int islideprecision;
#endif /* CONVERT_SLIDEPRECISION */

    if (ztPlayer)
        if (ztPlayer->playing)
            ztPlayer->stop();

    setstatusstr(NULL);

    ls_filename[0] = 0;
    if (fn) {
        strcpy(ls_filename,fn);
//      strcpy((char *)&this->filename[0],ls_filename);
    } else
        strcpy(ls_filename,(const char *)&this->filename[0]);
    
    fn = (char*)&ls_filename[0];


    if (check_ext(fn,".it")) {

        //SDL_Delay(1);

        if (!lock_mutex(this->hEditMutex)) {
            setstatusstr("Error loading %s (Mutex lock timeout)",fn);
            load_lock = 0;
            return -1;
        }

        zti = new ZTImportExport;
        status_change = 1;  /* should this be here?  |  who said this?  i have no idea [cm] */
        int ret = zti->ImportIT(fn,this);
        if (song->orderlist[0] < 0x100)
            cur_edit_pattern = song->orderlist[0];
        UIP_InstEditor->reset = 1;
        ztPlayer->num_orders();
        unlock_mutex(this->hEditMutex);
        if (ret) {
            setstatusstr("Imported IT module %s",fn);
            file_changed = 0;
            delete zti;
            load_lock = 0;
            return 0;
        } else {
            setstatusstr("Error importing IT module %s",fn);
            delete zti;
            load_lock = 0;
            return -1;
        }
    }


    std::ifstream f(fn, std::ios::in | std::ios::binary);
    if (f.fail()) {
        setstatusstr("Error loading %s (Unable to read file)",fn);
        load_lock = 0;
        return -1;
    }

    compressed = read_ZT_header(&buffer,f);

    if (compressed == -2) {
        f.close();
        setstatusstr("Error loading %s (old format, use zt <0.923 to conv)",fn);
        load_lock = 0;
        return -1;
    }

    if (compressed == -1) {
        setstatusstr("Error loading %s (Doesn't seem to be a zt file)",fn);
        load_lock = 0;
        return -1;
    }
       
    //SDL_Delay(1);

    if (!lock_mutex(this->hEditMutex)) {
        setstatusstr("Error loading %s (Mutex lock timeout)",fn);
        load_lock = 0;
        return -1;
    }

    itpb = this->tpb;
    ibpm = this->bpm;

    imidiclock = flag_SendMidiClock;
    imidistopstart = flag_SendMidiStopStart;
    memcpy(ititle, this->title, ZTM_SONGTITLE_MAXLEN);
    islideonsubtick = flag_SlideOnSubtick;
#ifdef CONVERT_SLIDEPRECISION
    islideprecision = flag_SlidePrecision;
#endif /* CONVERT_SLIDEPRECISION */
    SDL_Delay(10);
    this->de_init();
    SDL_Delay(10);
    this->init();
    this->tpb = itpb;
    this->bpm = ibpm;

    flag_SendMidiClock = imidiclock;
    flag_SendMidiStopStart = imidistopstart;
    memcpy(this->title,ititle,ZTM_SONGTITLE_MAXLEN);
    flag_SlideOnSubtick = islideonsubtick;
#ifdef CONVERT_SLIDEPRECISION
    flag_SlidePrecision = islideprecision;
#endif /* CONVERT_SLIDEPRECISION */
    
    load_lock = 0;
    SDL_Delay(100);
    UIP_SaveMsg->filetype = 1;
    char header[5];

    if (compressed) {
//      InflateStream is(f);
        input = new InflateStream(&f);   //&is;
    }

    int ret = 0;
    while(ret!=-1) {
        ret = readblock(&header[0],&buffer,compressed,f,input);
        if (ret!=-1) {
            if (cmp_hd(&header[0], "SMSG"))
                load_song_message(&buffer);
            if (cmp_hd(&header[0], "ARPG"))
                load_arpeggio(&buffer);
            if (cmp_hd(&header[0], "MMAC"))
                load_MIDI_macro(&buffer);
            if (cmp_hd(&header[0], "ZTtm"))
                load_ZT_track_mutes(&buffer);
            if (cmp_hd(&header[0], "ZTol"))
                load_ZT_order_list(&buffer);
            if (cmp_hd(&header[0], "ZTpl"))
                load_ZT_pattern_lengths(&buffer);
            if (cmp_hd(&header[0], "ZTpp"))
                load_ZT_pattern_properties(&buffer);
            if (cmp_hd(&header[0], "ZTin"))
                load_ZT_instrument(&buffer);
            if (cmp_hd(&header[0], "ZTev"))
                load_ZT_event_list(&buffer);
        } SDL_Delay(1);
    }
#ifdef CONVERT_SLIDEONSUBTICK
    // slides already converted in the loading phase
    // just put player into slide on subtick mode.
    flag_SlideOnSubtick=1;
#endif /* CONVERT_SLIDEONSUBTICK */
#ifdef CONVERT_SLIDEPRECISION
    // slides already converted in the loading phase
    // just mark that we have enhanced slide precision.
    flag_SlidePrecision=1;
#endif /* CONVERT_SLIDEPRECISION */

    if (compressed) {
        delete input;
        setstatusstr("Loaded %s (compressed)",fn);
    } else
        setstatusstr("Loaded %s (uncompressed)",fn);
    if (song->orderlist[0] < 0x100)
        cur_edit_pattern = song->orderlist[0];
    strcpy((char*)&this->filename[0],fn);
    file_changed = 0;
    UIP_InstEditor->reset = 1;
    ztPlayer->num_orders();
    unlock_mutex(this->hEditMutex);
    return 0;
}

/*************************************************************************
 *
 * NAME  zt_module::setstatusstr()
 *
 * SYNOPSIS
 *   zt_module::setstatusstr (fmtstr, ...)
 *   void zt_module::setstatusstr (const char *, ...)
 *
 * DESCRIPTION
 *   Set a status message which can be retrieved by calling getstatusstr().
 *   If fmtstr is NULL, the status string will be cleared.
 *
 * INPUTS
 *   fmtstr           - format string
 *   ...              - vararg parameters
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
void zt_module::setstatusstr(const char *fmtstr, ...) {
    va_list ap;

    if (!fmtstr) {
        statusstr=NULL;
        return;
    }
    va_start(ap, fmtstr);
    vsprintf(statusstr_buffer, fmtstr, ap);
    va_end(ap);
    statusstr=statusstr_buffer;
    return;
}

/*************************************************************************
 *
 * NAME  zt_module::getstatusstr()
 *
 * SYNOPSIS
 *   str = zt_module::getstatusstr ()
 *   char *zt_module::getstatusstr (void)
 *
 * DESCRIPTION
 *   Get a status message previously set by setstatusstr()
 *
 * INPUTS
 *   none
 *
 * RESULT
 *   str              - status string or NULL if no message is pending.
 *
 * KNOWN BUGS
 *   none
 *
 ******/
char *zt_module::getstatusstr(void) {
    char *ret;
    ret=statusstr;
    statusstr=NULL;
    return ret;
}

/* eof */
