/*************************************************************************
 *
 * FILE  playback.h
 *
 * DESCRIPTION
 *   definitions for the ztracker player code.
 *   Ported to use std::thread + std::chrono instead of Win32 multimedia timers.
 *
 * Copyright (c) 2000-2001, Christopher Micali
 * Copyright (c) 2001, Daniel Kahlin
 * All rights reserved. (BSD-3-Clause)
 *
 ******/
#ifndef __PLAYBACK_H
#define __PLAYBACK_H

#include <thread>
#include <atomic>
#include <chrono>

class zt_module;

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
    ET_LOOP
};

struct midi_event {
    short int tick;
    unsigned int device;
    Emeventtypes type;
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
        // Portable timer — replaces Win32 timeSetEvent / TIMECAPS / wTimerID.
        std::thread *timerThread;
        std::atomic<bool> timerRunning;
        unsigned int timerIntervalMs;
        void timerThreadFunc(unsigned int intervalMs);

        unsigned int SetTimerCallback(unsigned int msInterval);

        struct s_note_off {
            signed char note;
            unsigned char inst;
            unsigned char track;
            unsigned char pad;
            unsigned short int length;
        };

    public:
        unsigned int wTimerRes;

        s_note_off *noteoff_eventlist;
        int noteoff_size, noteoff_cur;
        int tpb, bpm, starts;

        std::thread *counterThread;
        unsigned long iID;

        int cur_buf,playing_cur_row,playing_cur_pattern,playing_cur_order;
        int prebuffer,playing_tick,num_real_orders;

        midi_buf *play_buffer[2],*playing_buffer;

        int playmode;
        int cur_subtick;
        int cur_row;
        int cur_pattern;
        int cur_order;
        int counter;
        int subticks;
        int pinit;

        int subtick_len_ms;
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
        void callback(void);
        void playback(midi_buf *buffer, int ticks);
        int calc_pos(int midi_songpos, int *rowptr, int *orderptr);

        void insert_no(unsigned char note, unsigned char chan, short int len, unsigned char track);
        void process_noel(midi_buf *buffer,int p_tick);
        void clear_noel(void);
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
