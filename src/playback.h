/*************************************************************************
 *
 * FILE  playback.h
 * $Id: playback.h,v 1.8 2002/04/11 22:39:09 zonaj Exp $
 *
 * DESCRIPTION 
 *   definitions for the ztracker player code.
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
#ifndef __PLAYBACK_H
#define __PLAYBACK_H

#include "zt.h"


enum Emeventtypes { 
    ET_NOTE_ON,
    ET_NOTE_OFF,
    ET_CC,
    ET_CLOCK,
    ET_PC,
    ET_STATUS,
    ET_AT,
    ET_TPB,
    ET_BPM,
    ET_LIGHT,
    ET_PATT,
    ET_PITCH,
    ET_MSTART,
    ET_LOOP                   // <Manu> definimos el evento de loop
};

struct midi_event {
    short int tick;
    unsigned int device;
    Emeventtypes type;//unsigned char type;
    unsigned char command;
    unsigned short data1;
    unsigned short data2;
    unsigned char track;
    unsigned char inst;
    unsigned short int extra;
    midi_event *next;
};

class midi_buf {
    public:
        char ready_to_play;
        midi_event *event_buffer;

        int size, maxsize, event_cursor, listhead;

        midi_buf();
        ~midi_buf();
        void reset(void);
        void insert(midi_event *e);
        void insert(short int tick, unsigned char type, unsigned int device, unsigned char command=0, unsigned short data1=0, unsigned short data2=0, unsigned char track=MAX_TRACKS, unsigned char inst=MAX_INSTS, unsigned short int extra=0);
        void rollback_cursor(void);
        midi_event *get_next_event(void);
};

class player {
    private:
        MMRESULT wTimerID;
        TIMECAPS tc;
        UINT SetTimerCallback(UINT msInterval);

        struct s_note_off {
            signed char note;
            unsigned char inst;
            unsigned char track;
            unsigned char pad;
            unsigned short int length;
        };

    public:
        UINT     wTimerRes;
        
        s_note_off *noteoff_eventlist;
        int noteoff_size, noteoff_cur;
        int tpb, bpm, starts;

        HANDLE hThread;
        unsigned long iID;

//      hires_timer *hr_timer;

        int cur_buf,playing_cur_row,playing_cur_pattern,playing_cur_order;
        int prebuffer,playing_tick,num_real_orders;

        midi_buf *play_buffer[2],*playing_buffer;

        int playmode; // 0 = loop / 1 = song
        int cur_subtick;
        int cur_row;
        int cur_pattern;
        int cur_order;
        int counter;
        int subticks;
        int pinit;

//      int edit_lock;

        int subtick_len_ms;//, subtick_count;
        int subtick_len_mms;
        int subtick_error,subtick_add;
        int tick_len_ms;
        int clock_len_ms, clock_count;

        int subtick_mode,clock_mode;

        int playing;
        int fillbuff;
        int light_counter;
        int skip;
        int loop_mode;

        zt_module *song;
        pattern patt_memory;
//      int clock_counter,clock_len_ms,clock_len_mms,clock_error,clock_error_flag;
        
        player(int res,int prebuffer_rows, zt_module *ztm);
        ~player(void);

        int init(void);
        int start_timer(void);
        int stop_timer(void);
        
        void prepare_play(int row, int pattern,int pm, int loopmode);
        void play(int row, int pattern, int pm, int loopmode=1);
        void play_current_row();
        void play_current_note();
        void stop(void);
        void callback(void); // reads buffers and actually plays them
        void playback(midi_buf *buffer, int ticks); // fills buffer with x ticks
        int calc_pos(int midi_songpos, int *rowptr, int *orderptr);

        void insert_no(unsigned char note, unsigned char chan, short int len, unsigned char track);
        void process_noel(midi_buf *buffer,int p_tick);
        void clear_noel(void);
        //event *get_noel(unsigned note, unsigned chan);
        int get_noel(unsigned char note, unsigned char inst);
        int kill_noel_with_midiout(unsigned char note, unsigned char inst);
        int kill_noel(unsigned char note, unsigned char inst);
        int kill_noel(int i);
        void clear_noel_with_midiout(void);

        void set_speed();

        int next_order(void);
        void num_orders(void);
        int seek_order(int pattern);

        void ffwd(void);
        void rewind(void);

        int last_pattern;
};

#endif
/* eof */
