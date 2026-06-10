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

#include "winmm_compat.h"

//#include "zt.h"
#include "module.h"
class zt_module ;

// ---------------------------------------------------------------------------
// CC envelope runtime hooks. Envelopes live on the instrument
// (inst_envelope inst->envelopes[]). Pattern note-on AND keyjazz
// audition both fire all enabled envelopes on the active instrument.
//
// `cc_env_voice` is one armed envelope on one track. Sized to
// MAX_TRACKS * ZTM_INST_MAX_ENVELOPES (= 64 * 32 = 2048) inside the
// player; pre-allocated so playback never touches the heap.

struct cc_env_voice {
    unsigned char inst;         // instrument that armed this voice
    unsigned char slot;         // index into inst->envelopes[]
    unsigned char active;       // 1 = voice live; 0 = empty slot
    unsigned char key_off;      // 1 = note released; sustain region abandoned
    unsigned char done;         // envelope finished (caller frees on emit)
    int           position;     // subticks elapsed since note-on
    int           last_emitted; // last CC value sent (-1 = none yet)
    int           speed_counter;
};

// Audition pool. Independent of the per-track playback grid -- this
// drives envelopes for keyjazz / test-fire from the main UI thread
// using SDL_GetTicks for timing.
void zt_audition_env_arm(int sdlk_key, int inst);
void zt_audition_env_release(int sdlk_key);
void zt_audition_env_pump(void);
void zt_audition_env_clear_all(void);

// Test-fire one envelope from the editor without going through
// jazz_set_state. `sentinel_key` is a tag the editor can pass to
// later release this voice via zt_audition_env_release.
void zt_audition_env_arm_one(int inst, int slot, int sentinel_key);

// Editor live-view: returns 1 if any voice (audition or pattern) is
// processing inst->envelopes[slot]. Fills out_position with the
// current tick + out_last_value with the most recent emitted CC.
int zt_envelope_live_position(int inst, int slot,
                              int *out_position, int *out_last_value);
// ---------------------------------------------------------------------------

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
    ET_LOOP,                  // <Manu> definimos el evento de loop [EN: define the loop event]
    ET_RAW                    // raw 3-byte short MIDI msg packed in command|data1|data2
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
        zt_timer_handle ztTimerID;
#ifdef _WIN32
        TIMECAPS tc;
#endif
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

        zt_thread_handle hThread;
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
        // Monotonic count of stop() calls. Observers (the Ableton Link glue's
        // per-frame pump) watch it to react to ANY stop -- including the
        // counter-thread song-end stop and prepare_play's internal restart --
        // without the player knowing about them. Plain int on purpose: bumped
        // from the timer thread, read advisorily from the main thread.
        int stop_count;
        // Phase-chase observer surface, same contract as stop_count: the
        // player stays sync-protocol-agnostic and just exposes its position
        // plus one advisory knob. played_subticks counts subticks since
        // play start (timer thread writes, pump reads); chase_skew_us is a
        // signed per-subtick length adjustment in microseconds (pump writes,
        // timer thread reads) that an external phase lock uses to slew the
        // engine onto a shared timeline. 0 = run at nominal speed.
        //
        // The 1 ms timer can't resolve a sub-ms threshold change (the
        // subtick delta-sigma would re-absorb it), so the skew accrues in
        // chase_err_us per subtick and pays out as whole +/-1 ms quanta in
        // chase_adj -- average added length == chase_skew_us exactly.
        // chase_err_us/chase_adj are timer-thread-private.
        int played_subticks;
        int chase_skew_us;
        int chase_err_us;
        int chase_adj;
        int fillbuff;
        int light_counter;
        int skip;
        int loop_mode;
        // Wall-clock timestamp (SDL_GetTicks ms) when play() was invoked.
        // Used by the status display to show real elapsed time, immune
        // to row-quantization artifacts (.4s skip) that happen when
        // calcSongMs jumps in tpb*bpm-row chunks.
        uint64_t playback_start_ms;

        zt_module *song;
        pattern patt_memory;

        // Per-track CC envelope runtime state. cc_env[t][s] is one
        // armed voice on track t driving inst->envelopes[s]. The
        // playback loop advances each active voice every subtick.
        cc_env_voice cc_env[MAX_TRACKS][ZTM_INST_MAX_ENVELOPES];
        void clear_cc_envs(void);
        void arm_track_envelopes(int track, int inst);    // on note-on
        void release_track_envelopes(int track);          // on note-off
//      int clock_counter,clock_len_ms,clock_len_mms,clock_error,clock_error_flag;
        
        player(int res,int prebuffer_rows, zt_module *ztm);
        ~player(void);

        int init(void);
        int start_timer(void);
        int stop_timer(void);
        
        void prepare_play(int row, int pattern,int pm, int loopmode);
        void play(int row, int pattern, int pm, int loopmode=1);
        // Immediate start, bypassing play()'s Ableton Link quantize defer.
        // Only the Link glue's downbeat fire should call this directly.
        void play_immediately(int row, int pattern, int pm, int loopmode=1);
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

        // Slave the live playback tempo to an externally supplied BPM
        // (derived from incoming MIDI Clock). Does NOT mutate song->bpm,
        // so the song file's stored tempo is preserved. Called from the
        // MIDI-input callback thread at most once per 24 received clocks.
        void chase_external_tempo(int new_bpm);

        int next_order(void);
        void num_orders(void);
        int seek_order(int pattern);

        void ffwd(void);
        void rewind(void);

        int last_pattern;
};

#endif
/* eof */
