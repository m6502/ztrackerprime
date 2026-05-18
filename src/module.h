/*************************************************************************
 *
 * FILE  module.h
 * $Id: module.h,v 1.19 2001/10/16 05:45:06 cmicali Exp $
 *
 * DESCRIPTION 
 *   Definitions of the .zt module format, and all functions to operate
 *   on modules, including load and save.
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
#ifndef _MODULE_H
#define _MODULE_H

#include <fstream>

#include "platform.h"


class CDataBuf ;
class DeflateStream ;
class InflateStream ;

/* experimental arpeggio support */
#define USE_ARPEGGIOS

/* experimental midi macro support */
#define USE_MIDIMACROS

/* CC envelopes — Schism-style node + interp + sustain/loop curves
 * that emit MIDI CC during the lifetime of a note. See
 * doc/design/cc-envelopes.md.
 */
#define USE_CC_ENVELOPES

/*************************************************************************
 *
 *  ztracker module definitions.
 *
 ******/

/* these should be phased out from zt.h and put here */

#define MAX_TRACKS                      64 // Max # of tracks
#define MAX_INSTS                       100 // Max # of instruments


#define ZTM_MAX_TRACKS                      MAX_TRACKS
#define ZTM_MAX_INSTS                       MAX_INSTS
#define ZTM_MAX_PATTERNS                    256

#define ZTM_ORDERLIST_LEN                   256
#define ZTM_SONGTITLE_MAXLEN                26
#define ZTM_INSTTITLE_MAXLEN                26
#define ZTM_FILENAME_MAXLEN                 256

#define ZTM_SONGMESSAGE_MAXLEN              2048

#ifdef USE_ARPEGGIOS
#define ZTM_ARPEGGIONAME_MAXLEN             40
#define ZTM_MAX_ARPEGGIOS                   256
#define ZTM_ARPEGGIO_NUM_CC                 4
#define ZTM_ARPEGGIO_LEN                    256
#endif /* USE_ARPEGGIOS */

#ifdef USE_MIDIMACROS
#define ZTM_MIDIMACRONAME_MAXLEN            40
#define ZTM_MAX_MIDIMACROS                  256
#define ZTM_MIDIMACRO_MAXLEN                64
#endif /* USE_MIDIMACRO */

#ifdef USE_CC_ENVELOPES
#define ZTM_CCENVNAME_MAXLEN                40
#define ZTM_MAX_CCENVELOPES                 128
#define ZTM_CCENV_MAX_NODES                 32
/* envelope flags */
#define ZTM_CCENVF_ENABLED                  0x01
#define ZTM_CCENVF_LOOP                     0x02
#define ZTM_CCENVF_SUSTAIN                  0x04
#define ZTM_CCENVF_CARRY                    0x08
/* sentinel for instrument.ccenv_default[].env_idx meaning "unused" */
#define ZTM_CCENV_NONE                      0xFF
/* per-instrument simultaneous envelope slots */
#define ZTM_CCENV_PER_INST                  4
#endif /* USE_CC_ENVELOPES */

/* max length of the status string returned from I/O functions */
#define ZTM_STATUSSTR_MAXLEN                256

#define ZT_MODULE_VERSION                   7

#define ZT_BANK_CHANGE_MODULE_VERSION       7
#define ZT_ROWSIZE_CHANGE_VERSION           5

#define ZT_MODULE_SUPPORTED_VERSION         5

#define INSTFLAGS_NONE                      0x00
#define INSTFLAGS_REGRIGGER_NOTE_ON_UNMUTE  0x01
#define INSTFLAGS_CHANVOL                   0x02
#define INSTFLAGS_TRACKERMODE               0x04

#define LEN_INF                             1000



class instrument
{
public:

  instrument(int p);
  ~instrument(void);

  int isempty(void);

  void load(CDataBuf *buf);
  void save(CDataBuf *buf, unsigned char inum);

  // <Manu> 
  void MarkAsUsed() { bHasBeenUsed = true ; }
  void MarkAsUnused() { bHasBeenUsed = false ; }
  bool IsUsed() { return bHasBeenUsed ; }

public:

  // <Manu> Flag que indica si el instrumento se ha utilizado en la última reproducción [EN: flag indicating whether the instrument was used in the last playback]
  bool bHasBeenUsed ;

  signed char patch;
  unsigned char midi_device;
  unsigned char channel;
  unsigned char flags;
  signed char transpose;
  unsigned char global_volume;
  unsigned char default_volume;
  unsigned char title[ZTM_INSTTITLE_MAXLEN];
  signed short int bank;

  unsigned short int default_length;

  // CCizer bank attached to this instrument. Filename only (basename of a
  // `.txt` in the configured ccizer_folder), e.g. "microfreak.txt". Empty
  // string = no bank attached. Persisted in the .zt file via the optional
  // CCBN chunk; the chunk is skipped by older zTracker versions, which
  // load such files cleanly with this field empty (forward-compat).
  char ccizer_bank[256];

#ifdef USE_CC_ENVELOPES
  // CC envelopes auto-armed for this instrument. Each slot stores an
  // envelope index (0..ZTM_MAX_CCENVELOPES-1) or ZTM_CCENV_NONE.
  // cc_override is 0..127 for "use this CC# instead of the envelope's
  // own cc field", or 0x80 for "use the envelope's cc". Persisted in
  // the optional IENV chunk; older zTracker silently skips it.
  unsigned char ccenv_default[ZTM_CCENV_PER_INST];
  unsigned char ccenv_cc_override[ZTM_CCENV_PER_INST];
#endif /* USE_CC_ENVELOPES */

} ;



class event
{
public:

  unsigned char note;
  unsigned char inst;
  unsigned char vol;
  unsigned char effect;
  unsigned short int row;
  unsigned short int length;
  unsigned short int effect_data;
  event *next_event;
  event(void);
  event(event *e);
  ~event(void);
  int blank(void);
};

class track
{
public:

  // ---------------------------------------------------------------------
  bool custom_colors = false ;

  unsigned long EditText;       // text that is in the pattern editor and all other boxes except top info boxes
  unsigned long EditBG;         // background of the pattern editor and all other boxes except top info boxes
  unsigned long EditBGlow;      // background of pattern editor (lowlight)
  unsigned long EditBGhigh;     // background of pattern editor (highlight)
  // ---------------------------------------------------------------------

  unsigned char default_inst;
  unsigned char last_note,last_inst;
  unsigned char default_controller;

  unsigned short int default_length, default_fxp, default_delay ;
  unsigned short int default_data;
  short int pitch_slide;

  short int length;

  signed int pitch_setting;
  event *event_list;

  void update_event_safe(unsigned short int row, int note, int inst, int vol, int length, int effect, int effect_data);
  void update_event(unsigned short int row, int note, int inst, int vol, int length, int effect, int effect_data);
  void update_event(unsigned short int row, event *src);
  event *get_event(unsigned short int row);
  event *get_next_event(unsigned short int row);

  track(short int len);
  ~track(void);
  void reset(void);
  void del_event(unsigned short int row, int needlock=1);
  void del_row(unsigned short int which);
  void ins_row(unsigned short int which);
};
 
class pattern
{
public:

  // ---------------------------------------------------------------------
  bool custom_colors = false ;

  unsigned long Background ;     // Background of "panel"
  unsigned long Highlight ;      // highlight of "panel"
  unsigned long Lowlight ;       // lowlight of "panel"
  // ---------------------------------------------------------------------

  track *tracks[ZTM_MAX_TRACKS];
  short int length;

  pattern(void);
  pattern(int len);
  ~pattern(void);

  void resize(int newsize);
  int isempty(void);
};


#ifdef USE_ARPEGGIOS

#define ZTM_ARP_EMPTY_PITCH 0x8000
#define ZTM_ARP_EMPTY_CCVAL 0x80
#define ZTM_ARP_EMPTY_CC    0x80

class arpeggio {
    public:
        char     name[ZTM_ARPEGGIONAME_MAXLEN];
        int      speed;
        int      repeat_pos;
        int      length;
        int      num_cc;
        unsigned char  cc[ZTM_ARPEGGIO_NUM_CC];
        unsigned short pitch[ZTM_ARPEGGIO_LEN];
        unsigned char  ccval[ZTM_ARPEGGIO_NUM_CC][ZTM_ARPEGGIO_LEN];
        arpeggio(void);
        ~arpeggio(void);

        int isempty(void);
};

#endif /* USE_ARPEGGIOS */


#ifdef USE_MIDIMACROS

#define ZTM_MIDIMAC_END    0x100
#define ZTM_MIDIMAC_PARAM1 0x101

class midimacro {
    public:
        char     name[ZTM_MIDIMACRONAME_MAXLEN];
        unsigned short data[ZTM_MIDIMACRO_MAXLEN];

        // Runtime-only SysEx cache (audit H3). When `name` ends in
        // `.syx`, zt_module::cache_sysex_macros() reads the file at
        // song-load time and stashes the bytes here so the Z-effect
        // dispatch in the playback thread never touches the disk on
        // its critical path. NOT serialized in the MMAC chunk -- the
        // bytes live on disk, the cache is just a per-load mirror.
        unsigned char *syx_cache_bytes;
        int            syx_cache_len;

        midimacro(void);
        ~midimacro(void);

        int isempty(void);
};
#endif /* USE_MIDIMACROS */

#ifdef USE_CC_ENVELOPES
/* Schism-style envelope adapted for MIDI CC output.
 *
 * Nodes are stored sparsely: tick[] is the x axis (subticks since
 * note-on, monotonically increasing), value[] is the y axis (0..127
 * MIDI CC value). The runtime linearly interpolates between adjacent
 * nodes to produce a per-subtick CC value.
 *
 * Loop and sustain regions are inclusive node-index ranges. When
 * `flags & SUSTAIN` and the note is still held, the runtime wraps the
 * envelope position between tick[sustain_start] and tick[sustain_end].
 * On note-off, sustain is abandoned; if `flags & LOOP`, the position
 * wraps between tick[loop_start] and tick[loop_end]; otherwise the
 * envelope plays to its last node and stops.
 *
 * `kind` controls what kind of MIDI message the value drives:
 *   0 = CC          (value 0..127  -> Bn cc value)
 *   1 = Pitchbend   (value 0..127  -> En lo hi, scaled to 0..16383)
 *   2 = Channel Pressure (Dn value)
 *
 * Persistence: CENV chunk. Pure logic (interp, advance) lives in
 * src/ccenv_interp.h + src/ccenv_advance.h and is unit-tested.
 */
class ccenvelope {
    public:
        char name[ZTM_CCENVNAME_MAXLEN];
        unsigned char cc;          // CC# this envelope drives (0..127)
        unsigned char kind;        // 0=CC, 1=Pitchbend, 2=ChanPressure
        unsigned char flags;       // ZTM_CCENVF_*
        unsigned char num_nodes;   // 2..ZTM_CCENV_MAX_NODES
        unsigned char loop_start, loop_end;
        unsigned char sustain_start, sustain_end;
        unsigned short tick[ZTM_CCENV_MAX_NODES];   // x: subticks
        unsigned char  value[ZTM_CCENV_MAX_NODES];  // y: 0..127
        unsigned short speed;      // subticks per envelope tick (>=1)

        ccenvelope(void);
        ~ccenvelope(void);

        int isempty(void) const;
        // Default 2-node ramp 0..127 over 64 subticks, enabled.
        void reset_to_default(void);
};
#endif /* USE_CC_ENVELOPES */
/*
class songmsg {
    public:
        char *songmessage;
        unsigned int size;
        songmsg(int size);
        ~songmsg(void);
        int resize(int size);
        int isempty(void);
};
*/

class songmsg
{
    public:

        CDataBuf *songmessage;
        songmsg();
        ~songmsg();
        int isempty();
};


class zt_module
{
    public:
        unsigned char version;
        unsigned char title[ZTM_SONGTITLE_MAXLEN];
        unsigned char filename[ZTM_FILENAME_MAXLEN];
        int tpb,bpm;

#ifdef USE_ARPEGGIOS
        arpeggio *arpeggios[ZTM_MAX_ARPEGGIOS];
#endif /* USE_ARPEGGIOS */

#ifdef USE_MIDIMACROS
        midimacro *midimacros[ZTM_MAX_MIDIMACROS];
#endif /* USE_MIDIMACROS */

#ifdef USE_CC_ENVELOPES
        ccenvelope *ccenvelopes[ZTM_MAX_CCENVELOPES];
#endif /* USE_CC_ENVELOPES */
        
        songmsg *songmessage;
        //CDataBuf *songmessage;
        
        pattern *patterns[ZTM_MAX_PATTERNS];
        instrument *instruments[ZTM_MAX_INSTS];
        int orderlist[ZTM_ORDERLIST_LEN];

        zt_mutex_handle hEditMutex;

        unsigned char track_mute[ZTM_MAX_TRACKS];

        int flag_SlidePrecision;
        int flag_SlideOnSubtick;
        int flag_SendMidiClock;
        int flag_SendMidiStopStart;

        zt_module(int tpb,int bpm);
        ~zt_module();
        void init(void);
        void de_init(void);
        void init_pattern(int which);
        void reset(void);


        int query(char *fn=NULL);
        int load(char *fn=NULL);
        int save(char *fn=NULL,int compressed = 1);
        char *getstatusstr(void);

        // Pre-load every `*.syx`-named midimacro's file content into
        // its in-memory cache so the playback thread's Z-effect
        // dispatch never hits the disk on the critical path. Called
        // automatically from load() and from the SysEx Librarian's
        // syx_folder change. Audit H3.
        void cache_sysex_macros(void);

    private:

        char *statusstr;
        char statusstr_buffer[ZTM_STATUSSTR_MAXLEN];
   
        void setstatusstr(const char *fmtstr, ...);

        int cmp_hd(const char *s1,const char *s2);

        void build_ZT_header(CDataBuf *buf, int compr); /* ZThd - .zt header */
        void build_ZT_order_list(CDataBuf *buf);  /* ZTol - order list */
        void build_ZT_track_mutes(CDataBuf *buf);  /* ZTtm - track mutes */
        void build_ZT_event_list(CDataBuf *buf);  /* ZTev - event list */
        void build_ZT_pattern_lengths(CDataBuf *buf);  /* ZTpl - pattern lengths */
        void build_ZT_pattern_properties(CDataBuf *buf);  /* ZTpl - pattern lengths */
        void build_song_message(CDataBuf *buf);           /* SMSG - song message */
        void build_MIDI_macro(CDataBuf *buf, int num);  /* MMAC - midi macro */
        void build_arpeggio(CDataBuf *buf, int num);  /* ARPG - arpeggio */
#ifdef USE_CC_ENVELOPES
        void build_CC_envelope(CDataBuf *buf, int num);  /* CENV - cc envelope */
#endif /* USE_CC_ENVELOPES */
        //void build_PATT(CDataBuf *buf);  /* PATT - pattern */
        //void build_INST(CDataBuf *buf);  /* INST - instrument */
        //void build_TMUT(CDataBuf *buf);  /* TMUT - track mutes */
        //void build_OLST(CDataBuf *buf);  /* OLST - order list */
        //void build_ZTHD(CDataBuf *buf, int compr); /* ZTHD - .zt header */

        int read_ZT_header(CDataBuf *buf, std::ifstream &ifs); /* ZThd - .zt header */
        void load_ZT_track_mutes(CDataBuf *buf); /* ZTtm - track mutes */
        void load_ZT_pattern_lengths(CDataBuf *buf); /* ZTpl - pattern lengths */
        void load_ZT_pattern_properties(CDataBuf *buf); /* ZTpl - pattern lengths */
        void load_ZT_order_list(CDataBuf *buf); /* ZTol - order list */
        void load_ZT_instrument(CDataBuf *buf); /* ZTin - instrument */
        void load_ZT_event_list(CDataBuf *buf); /* ZTev - event list */
        int load_song_message(CDataBuf *buf);  /* SMSG - song message */
        int load_MIDI_macro(CDataBuf *buf);  /* MMAC - midi macro */
        int load_arpeggio(CDataBuf *buf);  /* ARPG - arpeggio */
#ifdef USE_CC_ENVELOPES
        int load_CC_envelope(CDataBuf *buf);  /* CENV - cc envelope */
#endif /* USE_CC_ENVELOPES */
        //void load_PATT(CDataBuf *buf);  /* PATT - pattern */
        //void load_INST(CDataBuf *buf);  /* INST - instrument */
        //void load_TMUT(CDataBuf *buf);  /* TMUT - track mutes */
        //void load_OLST(CDataBuf *buf);  /* OLST - order list */

        void writedata(const char *data, int size, int compressed, std::ofstream &of, DeflateStream *o);
        int readdata(char *data, int size, int compressed, std::ifstream &ifs, InflateStream *i);

        void writeblock(const char *headid, CDataBuf *buf, int compressed, std::ofstream &of, DeflateStream *o);
        int readblock(char headid[5], CDataBuf *buf, int compressed, std::ifstream &ifs, InflateStream *i);

        // high level, safe editing methods
        inline int set_note(unsigned char pattern, unsigned char track, unsigned short row, int note, int instrument=-1, int volume=-1, int length=-1) {
            if (this->patterns[pattern] && this->patterns[pattern]->tracks[track]) {
                this->patterns[pattern]->tracks[track]->update_event_safe(row,note,instrument,volume,length,-1,-1);
                return 1;
            }
            return 0;
        }

};


/*************************************************************************
 *
 *  ztracker module file (.zt) definitions.
 *
 ******/
#define ZTMF_MAX_TRACKS     ZTM_MAX_TRACKS
#define ZTMF_MAX_INSTS      ZTM_MAX_INSTS

#define ZTMF_ORDERLIST_LEN  ZTM_ORDERLIST_LEN
#define ZTMF_SONGTITLE_LEN  ZTM_SONGTITLE_LEN
#define ZTMF_INSTTITLE_LEN  ZTM_INSTTITLE_LEN


#endif /* _MODULE_H */
/* eof */