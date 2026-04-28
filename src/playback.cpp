/*************************************************************************
 *
 * FILE  playback.cpp
 * $Id: playback.cpp,v 1.30 2002/06/15 17:08:25 cmicali Exp $
 *
 * DESCRIPTION 
 *   The actual ztracker player code.
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
#include <algorithm>
#include <stdint.h>

int mctr = 0;

#define TARGET_RESOLUTION 1         // 1-millisecond target resolution

void CALLBACK player_callback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

#ifndef _WIN32
static void player_timer_callback_posix(void) {
    player_callback(0, 0, 0, 0, 0);
}
#endif


// ------------------------------------------------------------------------------------------------
//
//
void CALLBACK player_callback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
    (void)wTimerID;
    (void)msg;
    (void)dwUser;
    (void)dw1;
    (void)dw2;

  if(!ztPlayer) return ;
  if(!ztPlayer->playing) return ;

  ztPlayer->counter += ztPlayer->wTimerRes*1000;

  if (ztPlayer->counter >= (ztPlayer->subtick_len_ms+ztPlayer->subtick_add)) {

    if (zt_config_globals.midi_in_sync) {

      if (mctr >= 4) {

        if (g_midi_in_clocks_received) {
        
          g_midi_in_clocks_received--;
          mctr = 0;
        }
        else {
        
          //goto skip ;
        }
      }
      else mctr++;
    }

    ztPlayer->counter = 0;

    if (ztPlayer->play_buffer[0]->ready_to_play && ztPlayer->play_buffer[1]->ready_to_play) SDL_Delay(0);

    ztPlayer->callback();

    if (ztPlayer->subtick_add) ztPlayer->subtick_error = ztPlayer->subtick_len_mms - (1000 - ztPlayer->subtick_error);
    else ztPlayer->subtick_error = ztPlayer->subtick_len_mms + ztPlayer->subtick_error;

    if (ztPlayer->subtick_error >= 500) ztPlayer->subtick_add = 1000;
    else ztPlayer->subtick_add = 0;

//skip: ;

  }
} 




// ------------------------------------------------------------------------------------------------
//
//
unsigned long ZT_THREAD_CALL counter_thread(void *) {
    int buf;
    zt_thread_set_current_priority(ZT_THREAD_PRIORITY_TIME_CRITICAL);
    while(1) {
         if (ztPlayer && ztPlayer->playing) {
            buf = ztPlayer->cur_buf; 
            if (buf) buf=0; else buf=1;
            if (ztPlayer->play_buffer[buf]->ready_to_play == 0) {
                ztPlayer->playback(ztPlayer->play_buffer[buf],ztPlayer->prebuffer);
                if (buf) buf=0; else buf=1;
            }
        }
        SDL_Delay(1);
    }

}




// ------------------------------------------------------------------------------------------------
//
//
midi_buf::midi_buf() {
    this->event_buffer = new midi_event[4512]; //4512
    this->ready_to_play = 0;
    this->event_cursor = this->listhead = 0;
    size = 0;
    maxsize = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
midi_buf::~midi_buf() {
    delete[] this->event_buffer;
    this->reset();
}




// ------------------------------------------------------------------------------------------------
//
//
void midi_buf::reset() {
    this->event_cursor = this->listhead = 0;
    this->ready_to_play = 0;
    this->size = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void midi_buf::rollback_cursor() {
    this->event_cursor = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
midi_event *midi_buf::get_next_event(void) {
    midi_event *ret;
    if (event_cursor >= listhead)
        return NULL;
    ret = &event_buffer[event_cursor++];
    this->size--;
    return ret;
}




// ------------------------------------------------------------------------------------------------
//
//
void midi_buf::insert(midi_event *e) {
    this->insert( e->tick,
            e->type,
            e->device,
            e->command,
            e->data1,
            e->data2,
            e->track,
            e->inst,
            e->extra
            );

}




// ------------------------------------------------------------------------------------------------
//
//
void midi_buf::insert(short int tick, unsigned char type, unsigned int device, unsigned char command, unsigned short data1, 
                      unsigned short data2, unsigned char track, unsigned char inst, unsigned short int extra) {
                      
    midi_event *temp;
    temp = &this->event_buffer[this->listhead++];
    temp->tick = tick; 
    temp->type = (Emeventtypes)type; 
    temp->device=device; 
    temp->command = command; 
    temp->data1 = data1; 
    temp->data2 = data2; 
    temp->track = track;
    temp->inst  = inst;
    temp->extra = extra;
    this->size++;
    if (this->maxsize < this->size)
        this->maxsize = this->size;
    return;
}




// ------------------------------------------------------------------------------------------------
//
//
player::player(int res,int prebuffer_rows, zt_module *ztm) {
//  this->tpb = song->tpb; this->bpm = song->bpm;
    this->song = ztm;
    this->wTimerRes = res;
    this->fillbuff = this->playing = this->counter = this->wTimerID = 0;
    this->ztTimerID = 0;
    this->playback_start_ms = 0;
//  this->hr_timer = new hires_timer;
    this->playing_cur_row = 1;
    init();

    //this->noteoff_eventlist = NULL;
    this->noteoff_eventlist = new s_note_off[4000];
    noteoff_cur = noteoff_size = 0;

    this->play_buffer[0] = new midi_buf;
    this->play_buffer[1] = new midi_buf;
    //this->edit_lock = 0;
    pinit = 0;
    prebuffer = (96/song->tpb) * prebuffer_rows; // 96ppqn, so look ahead is 1 beat
    
}   




// ------------------------------------------------------------------------------------------------
//
//
player::~player(void) {
    //this->edit_lock = 0;
    unlock_mutex(song->hEditMutex);
    this->stop_timer();
//  delete this->hr_timer;
    this->wTimerID = 0;
    delete this->play_buffer[1];
    delete this->play_buffer[0];
    delete[] this->noteoff_eventlist;
}




// ------------------------------------------------------------------------------------------------
//
//
UINT player::SetTimerCallback(UINT msInterval) {
#ifdef _WIN32
    wTimerID = timeSetEvent(msInterval, TARGET_RESOLUTION, player_callback, 0x0, TIME_PERIODIC);
    if (!wTimerID)
        return -1;
    return 0;
#else
    ztTimerID = zt_timer_start(ztTimerID, msInterval, player_timer_callback_posix);
    return ztTimerID ? 0 : (UINT)-1;
#endif
} 




// ------------------------------------------------------------------------------------------------
//
//
int player::start_timer(void) {
    SetTimerCallback(wTimerRes);
    hThread = zt_thread_create(counter_thread, NULL, &iID);
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
int player::stop_timer(void) {
#ifdef _WIN32
    if (wTimerID)
        timeKillEvent(wTimerID);
#else
    if (ztTimerID) {
        zt_timer_stop(ztTimerID);
        ztTimerID = 0;
    }
#endif
    zt_thread_terminate(hThread, 0);
    zt_thread_close(hThread);
    hThread = NULL;
    clear_noel();
    return 0;
}


//#include <algorithm>

// ------------------------------------------------------------------------------------------------
//
//
int player::init(void) {
#ifdef _WIN32
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
        return -1;
    wTimerRes = (std::min)((std::max)(tc.wPeriodMin, wTimerRes), tc.wPeriodMax);
    timeBeginPeriod(wTimerRes);
#else
    if (wTimerRes == 0)
        wTimerRes = 1;
#endif 
    cur_row = 0;
    cur_pattern = 0;
    playmode = 0; //loop
    this->tpb = song->tpb;
    this->bpm = song->bpm;
    set_speed();
    start_timer();
//    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST );
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::set_speed() {
//int t,int b) {
    int64_t a;
//  tpb = t; bpm = b;

    // <Manu> Apply changes in real-time :-)
    this->bpm = song->bpm ;
    this->tpb = song->tpb ;

    a = 1000000 / wTimerRes;
    subtick_len_ms = (int)(60 * a /(96*this->bpm));
    subtick_len_mms = subtick_len_ms % 1000;
    subtick_len_ms -= subtick_len_mms;
    subtick_error = subtick_len_mms;
    if (subtick_error > 500)
        subtick_add = 1000;
    else
        subtick_add = 0;
    clock_len_ms = 4;
    tick_len_ms = 96 / this->tpb;
}


// ------------------------------------------------------------------------------------------------
//
// Slave playback tempo to an externally-computed BPM (from incoming
// MIDI Clock). Mirrors the subtick-length math in set_speed() but
// deliberately does NOT touch song->bpm, so saving the song preserves
// the original authored tempo. New BPM is clamped to a sane range;
// out-of-range values are ignored.
void player::chase_external_tempo(int new_bpm) {
    if (new_bpm < 20 || new_bpm > 500) return;
    if (new_bpm == this->bpm) return;
    this->bpm = new_bpm;
    int64_t a = 1000000 / wTimerRes;
    subtick_len_ms = (int)(60 * a / (96 * this->bpm));
    subtick_len_mms = subtick_len_ms % 1000;
    subtick_len_ms -= subtick_len_mms;
    subtick_error = subtick_len_mms;
    if (subtick_error > 500) subtick_add = 1000;
    else                     subtick_add = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
int player::seek_order(int pattern) {
    int o;
    for (o=0;o<256;o++)
        if (song->orderlist[o] == pattern)
            return o;
    return -1;
}




/*************************************************************************
 *
 * NAME  player::calc_pos ()
 *
 * SYNOPSIS
 *   ret = player::calc_pos(midi_songpos, rowptr, orderptr)
 *   int player::calc_pos(int, int *, int *)
 *
 * DESCRIPTION
 *   Calculate row/order from a MIDI Song Position Pointer
 *   The midi_songpos is expressed in MIDI Beats, each MIDI Beat spans
 *   6 MIDI Clocks, ie a MIDI Beat is a 16th note, which means 16 MIDI Beats
 *   per bar.  In ztracker a bar is 16 rows at tpb=4, 32 rows at tpb=8,
 *   48 rows at tpb=12, etc...
 *
 * INPUTS
 *   midi_songpos   - The MIDI Song Position Pointer value
 *   rowptr         - pointer to a row variable to be filled in.
 *   orderptr       - pointer to a order variable to be filled in.
 *                        
 * RESULT
 *   ret            - status (0=OK)
 *
 * KNOWN BUGS
 *   Does not handle pattern breaks or tpb commands.
 *
 * SEE ALSO
 *   none
 *
 ******/
int player::calc_pos(int midi_songpos, int *rowptr, int *orderptr) {

    int i,rowspassed,lastrowspassed,pat,row,order,found;
    
    // <Manu> Por si acaso [EN: just in case]
    row = 0 ;
    order = 0 ;

    /* Safety first! Return if orderlist is empty. */
    if (song->orderlist[0] >= 0x100) 
        return -1;

    /* this should in the future also compensate for any pattern breaks and 
       tpb commands. Remember to set the tpb to the last encountered value! */

    found=0;
    rowspassed=0;
    while (!found) {
        /* loop thru song as many times as required to achive the desired position */
        i=0;
        while ((pat = song->orderlist[i]) < 0x100) {
            lastrowspassed=rowspassed;
            rowspassed += song->patterns[pat]->length;
            if ((4*rowspassed / song->tpb) > midi_songpos) {
                /* we just passed the position, calculate the row from the
                   position of the last pattern */
                row=(midi_songpos - (4*lastrowspassed / song->tpb))*song->tpb/4;
                order=i;
                found=1;
                break;
            }
            i++;
        }

    }

    /* all values calculated, return */
    *rowptr=row;
    *orderptr=order;
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::prepare_play(int row, int pattern, int pm, int loopmode)
{
  if (playing) {
    stop();
    SDL_Delay(50);
  }


  song->reset();

  for (int t=0;t<MAX_TRACKS;t++) {

    patt_memory.tracks[t]->reset();
  }

  // <Manu> A veces no entraba abajo? [EN: sometimes it didn't fall through to the block below?]
  while (!lock_mutex(song->hEditMutex)) {
  
    SDL_Delay(1) ;
  }

  /*if (lock_mutex(song->hEditMutex))*/ {

    for(int var_instrument = 0; var_instrument < ZTM_MAX_INSTS; var_instrument++) {

      song->instruments[var_instrument]->MarkAsUnused() ;
    }

    clear_noel();
    need_update_lights = 1;
    light_pos = 0;
    playmode = pm;
    light_counter = 0;
    cur_order = 0;
    cur_row = row-1;
    cur_pattern = pattern;

    cur_subtick = 0;        
    counter = 0;
    clock_count = 0;

    mctr = 0;

    skip = 0xFFF;
    this->tpb = song->tpb;
    this->bpm = song->bpm;
    set_speed();
    loop_mode = loopmode;

    switch(pm)
    {

    case 3:
      cur_order = cur_pattern;
      cur_pattern = song->orderlist[cur_order];
      playmode = pm = 2;
      break;

    case 2:
      cur_order = this->seek_order(pattern);
      if (cur_order == -1)
        playmode = pm = 0;
      else
        cur_pattern = song->orderlist[cur_order];
      break;

    case 1:
      if (song->orderlist[cur_order] > 0xFF)
        this->next_order();
      else
        cur_pattern = song->orderlist[cur_order];
      break;
    } ;

    num_orders();
    
    if (cur_pattern > 0xFF) return;
    
    cur_buf = playing_tick = 0;
    
    playing_cur_pattern = cur_pattern;
    playing_cur_order = cur_order;
    playing_cur_row = cur_row;
    
    last_pattern = -1;
    pinit = 1;
    
    unlock_mutex(song->hEditMutex);
  }

}




// ------------------------------------------------------------------------------------------------
//
//
void player::play(int row, int pattern,int pm, int loopmode)
{
// <Manu> esta comprobacion no es necesaria ? [EN: is this check unnecessary?]

/*    if (song->orderlist[pattern] == 0x100) 
        return;
*/


  if(song->orderlist[pattern] == 0x101) {

    int i = pattern;

    while(song->orderlist[i+1] == 0x101) {
      
      i++;
    }

    if (song->orderlist[i+1] == 0x100) return;
  }

  prepare_play(row,pattern,pm,loopmode);


  //  play_buffer[cur_buf]->reset();
  //  play_buffer[cur_buf]->insert(0,ET_MSTART,0); // First event is MIDI Start

  if(song->flag_SendMidiStopStart) MidiOut->sendGlobal(0xFA);
  SDL_Delay(20);

  starts = 1;

  this->playback(play_buffer[cur_buf],prebuffer);

  playing_buffer = play_buffer[cur_buf];

  playing = 1;
  playback_start_ms = SDL_GetTicks();
  zt_set_window_title("zt - [playing]");
}




// ------------------------------------------------------------------------------------------------
//
//
void player::play_current_row()
{
  event *pEvent;
  unsigned char set_note;
  unsigned int p1;

  for(int var_track = 0; var_track < 64; var_track++) {

    if(!song->track_mute[var_track]) {
    
      pEvent = song->patterns[cur_edit_pattern]->tracks[var_track]->get_event(cur_edit_row);
      
      if(pEvent && pEvent->inst < MAX_INSTS) {

        p1 = song->instruments[pEvent->inst]->default_volume;
        
        if (song->instruments[pEvent->inst]->global_volume != 0x7F && p1>0) {
          p1 *= song->instruments[pEvent->inst]->global_volume;
          p1 /= 0x7f;
        }
        
        // <Manu> if(A = B) ?
        /*if (pEvent = song->patterns[cur_edit_pattern]->tracks[var_track]->get_event(cur_edit_row))*/ {
          
          if (pEvent->vol<0x80) p1 = pEvent->vol ;
        }

        set_note = pEvent->note;
        set_note += song->instruments[pEvent->inst]->transpose; 

        // <Manu> Transposición [EN: transposition]


        if (set_note>0x7f) set_note = 0x7f;

        if (song->instruments[pEvent->inst]->midi_device!=0xff && song->instruments[pEvent->inst]->midi_device<=MAX_MIDI_DEVS) {

          MidiOut->noteOn(song->instruments[pEvent->inst]->midi_device,set_note,song->instruments[pEvent->inst]->channel,p1,MAX_TRACKS,0);            
          jazz_set_state(SDLK_8, set_note, song->instruments[pEvent->inst]->channel);
        }
      }
    }
  }
}




// ------------------------------------------------------------------------------------------------
//
//
void player::play_current_note()
{
  event *e;
  unsigned char set_note;
  unsigned int p1;

  e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);

  if(e && e->inst<MAX_INSTS) {

    p1 = song->instruments[e->inst]->default_volume;

    if (song->instruments[e->inst]->global_volume != 0x7F && p1>0) {

      p1 *= song->instruments[e->inst]->global_volume;
      p1 /= 0x7f;
    }

    // <Manu> if(A = B) ?

    /*if (e=song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row))*/ {
      
      if (e->vol<0x80) p1 = e->vol;
    }

    set_note = e->note;
    set_note += song->instruments[e->inst]->transpose;

    // <Manu> Transposición


    if (set_note>0x7f) set_note = 0x7f;

    if (song->instruments[e->inst]->midi_device!=0xff && song->instruments[e->inst]->midi_device<=MAX_MIDI_DEVS) {

      MidiOut->noteOn(song->instruments[e->inst]->midi_device,set_note,song->instruments[e->inst]->channel,p1,MAX_TRACKS /*cur_edit_track*/,0);

      jazz_set_state(SDLK_4, set_note, song->instruments[e->inst]->channel);
    }
  }
}




// ------------------------------------------------------------------------------------------------
//
//
void player::stop(void)
{
    playing = 0;
    pinit = 0;
    lock_mutex(song->hEditMutex);
    if (song->flag_SendMidiStopStart)
        MidiOut->sendGlobal(0xFC);
    clear_noel_with_midiout();
    if (zt_config_globals.auto_send_panic)
        MidiOut->hardpanic();
    MidiOut->panic();
    sprintf(szStatmsg,"Stopped"/*,ztPlayer->cur_pattern,ztPlayer->cur_row,song->patterns[cur_pattern]->length*/);
    statusmsg = szStatmsg;
    status_change = 1;
    play_buffer[0]->reset();
    play_buffer[1]->reset();
    unlock_mutex(song->hEditMutex);
    zt_set_window_title("zt");
}




// ------------------------------------------------------------------------------------------------
//
//
void player::ffwd(void) {
    if (!playmode) return;
    playing = 0;
    lock_mutex(song->hEditMutex);
    if (song->flag_SendMidiStopStart)
        MidiOut->sendGlobal(0xFC);
    play_buffer[0]->reset();
    play_buffer[1]->reset();
    clear_noel_with_midiout();
    MidiOut->panic();
    song->reset();
    cur_row = 0xFFFF;
    need_update_lights = 1; light_pos = 0;  light_counter = 0;
    cur_subtick = 0; counter = 0; clock_count   = 0;
    if (song->flag_SendMidiStopStart)
        MidiOut->sendGlobal(0xFA);
    starts = 1;
        cur_buf = playing_tick = 0;
    this->playback(play_buffer[cur_buf],prebuffer);
    playing_buffer = play_buffer[cur_buf];
    unlock_mutex(song->hEditMutex);
    playing = 1;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::rewind(void) {
    if (!playmode) return;
    playing = 0;
    lock_mutex(song->hEditMutex);
    if (song->flag_SendMidiStopStart)
        MidiOut->sendGlobal(0xFC);
    play_buffer[0]->reset();
    play_buffer[1]->reset();
    clear_noel_with_midiout();
    MidiOut->panic();
    song->reset();
//    cur_row = 0xFFFF;
    cur_row = 0;
    if (playing_cur_order>0) {
        cur_order = --playing_cur_order;
        while( song->orderlist[cur_order]==0x101 && cur_order>0 )
            cur_order--;
    }
    cur_pattern = song->orderlist[cur_order];
    need_update_lights = 1; light_pos = 0;  light_counter = 0;
    cur_subtick = 0; counter = 0; clock_count   = 0;
    if (song->flag_SendMidiStopStart)
        MidiOut->sendGlobal(0xFA);
    starts = 1;
        cur_buf = playing_tick = 0;
    this->playback(play_buffer[cur_buf],prebuffer);
    playing_buffer = play_buffer[cur_buf];
    unlock_mutex(song->hEditMutex);
    playing = 1;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::insert_no(unsigned char note, unsigned char inst, short int len, unsigned char track) {
    noteoff_eventlist[noteoff_size].inst   = inst;
    noteoff_eventlist[noteoff_size].length = len;   
    noteoff_eventlist[noteoff_size].note   = note;
    noteoff_eventlist[noteoff_size].track  = track; 
    noteoff_size++;
}




// ------------------------------------------------------------------------------------------------
//
//
int player::get_noel(unsigned char note, unsigned char inst) {
    int i;
    for (i=0;i<noteoff_size;i++)
        if (noteoff_eventlist[i].inst == inst && noteoff_eventlist[i].note == note)
            return i;
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
int player::kill_noel(unsigned char note, unsigned char inst) {
    int i;
    for (i=0;i<noteoff_size;i++)
        if (noteoff_eventlist[i].inst == inst && noteoff_eventlist[i].note == note) {
            inst = noteoff_eventlist[i].inst;
            memcpy(&noteoff_eventlist[i], &noteoff_eventlist[noteoff_size - 1], sizeof(s_note_off));
            noteoff_eventlist[noteoff_size - 1].length = -1;
            noteoff_size--;
            return inst;
        }
    return MAX_INSTS;   
}




// ------------------------------------------------------------------------------------------------
//
//
int player::kill_noel_with_midiout(unsigned char note, unsigned char inst) {
    int i;
    for (i=0;i<noteoff_size;i++)
        if (noteoff_eventlist[i].inst == inst && noteoff_eventlist[i].note == note) {
            MidiOut->noteOff(song->instruments[noteoff_eventlist[i].inst]->midi_device,noteoff_eventlist[i].note,song->instruments[noteoff_eventlist[i].inst]->channel,0x0,0);
            inst = noteoff_eventlist[i].inst;
            memcpy(&noteoff_eventlist[i], &noteoff_eventlist[noteoff_size - 1], sizeof(s_note_off));
            noteoff_eventlist[noteoff_size - 1].length = -1;
            noteoff_size--;
            return inst;
        }
    return MAX_INSTS;   
}




// ------------------------------------------------------------------------------------------------
//
//
int player::kill_noel(int i) {
    int inst;
    inst = noteoff_eventlist[i].inst;
    if (i<noteoff_size-1) {
        memcpy(&noteoff_eventlist[i], &noteoff_eventlist[noteoff_size - 1], sizeof(s_note_off));
        noteoff_eventlist[noteoff_size - 1].length = -1;
    }
    noteoff_size--;
    return inst;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::clear_noel(void) {
    noteoff_size = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::clear_noel_with_midiout(void) {
    for (int i=0; i<noteoff_size; i++)
        MidiOut->noteOff(song->instruments[noteoff_eventlist[i].inst]->midi_device,noteoff_eventlist[i].note,song->instruments[noteoff_eventlist[i].inst]->channel,0x0,0);
    noteoff_size = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::process_noel(midi_buf *buffer, int p_tick) {
// event *e,*last=NULL;
    s_note_off *e;
    track *tr;
    for(int i=0; i < noteoff_size; ) {
        e = &noteoff_eventlist[i];
        if (e->length) e->length--;
        if (!e->length) {
            tr = song->patterns[cur_pattern]->tracks[e->track];
            if (tr->last_note == e->note)
                tr->last_note = 0x80;
            buffer->insert(p_tick,ET_NOTE_OFF,song->instruments[e->inst]->midi_device,e->note,song->instruments[e->inst]->channel,0x0,e->track,e->inst);
            kill_noel(i);
        } else {
        //    e->length--;
            i++;
        }
    }
}













// ------------------------------------------------------------------------------------------------
// j se usa primero como effect_data, y luego como var1
//
void player::callback(void) {
    midi_buf *buf;
    midi_event *e;
    int set=0;
    unsigned short int usi;

    buf = play_buffer[cur_buf];

    if (buf->ready_to_play) {
        while (!lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT));
        e = buf->get_next_event();
        while(e) {
            if (e->tick == playing_tick) {
                switch(e->type) {
                    case ET_LOOP:
                        // loop markers are consumed by the pattern loop logic elsewhere
                        break;
                    case ET_MSTART:
                        if (song->flag_SendMidiStopStart)
                            MidiOut->sendGlobal(0xFA);
                        break;
                    case ET_CLOCK:
                        MidiOut->clock();
                        break;
                    case ET_PITCH:
                        if (!song->track_mute[e->track]) {
                            usi = (unsigned short int)e->data1<<8; usi+=e->data2;
                            MidiOut->pitchWheel(e->device,e->command,usi);
                        }
                        break;
                    case ET_CC:
                        if (!song->track_mute[e->track])
                            MidiOut->sendCC(e->device,e->command,(unsigned char)e->data1,(unsigned char)e->data2);
                        break;
                    case ET_PC:
                        if (!song->track_mute[e->track])
                            MidiOut->progChange(e->device,e->command,e->data1,(unsigned char)e->data2);
                        break;
                    case ET_NOTE_ON:
                            
                            // <Manu>
                            song->instruments[e->inst]->MarkAsUsed() ;
                            MidiOut->noteOn(e->device,e->command,(unsigned char)e->data1&0x0F,(unsigned char)e->data2,e->track,song->track_mute[e->track],(e->data1&0xF0)>>4);
                            //fprintf(stderr, "%d: Note On\n", e->tick);
                        break;
                    case ET_NOTE_OFF:
                            MidiOut->noteOff(e->device,e->command,(unsigned char)e->data1,(unsigned char)e->data2,song->track_mute[e->track]);
                            //fprintf(stderr, "%d: Note Off\n", e->tick);
                        break;
                    case ET_AT:
                        if (!song->track_mute[e->track])
                            MidiOut->afterTouch(e->device,e->command,(unsigned char)e->data1,(unsigned char)e->data2);
                        break;
                    case ET_PATT:
                        last_pattern = e->command;
                        break;
                    case ET_STATUS:
                        status_change = 1;
                        playing_cur_order = e->command;
                        playing_cur_pattern = e->data1;
                        playing_cur_row = e->extra;//e->data2;
                        break;
                    case ET_RAW:
                        if (!song->track_mute[e->track]) {
                            unsigned int msg = (unsigned int)e->command
                                             | ((unsigned int)e->data1 << 8)
                                             | ((unsigned int)e->data2 << 16);
                            MidiOut->send(e->device, msg);
                        }
                        break;
                    case ET_LIGHT:
                        if (++ztPlayer->light_counter>=this->tpb) {
                            need_update_lights++; 
                            light_counter = 0;
                            light_pos++; 
                            if (light_pos>3) 
                                light_pos = 0;
                        }                       
                        break;
                    case ET_BPM:
                        this->bpm = e->command; set++;
                        break;
                    case ET_TPB:
                        this->tpb = e->data1; set++;
                        break;
                }
            }
            e = buf->get_next_event();
        }
        unlock_mutex(song->hEditMutex);
    }
    playing_tick++;
    if (playing_tick>=prebuffer) {
        playing_tick = 0;
        if (cur_buf) cur_buf=0; else cur_buf=1;
        midi_buf *b = play_buffer[cur_buf];
        buf->rollback_cursor();
        e = buf->get_next_event();
        if (e) {
            while (!lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT));
            while ( e ) {
                if (e->tick >= prebuffer) {
                    e->tick-=prebuffer;
                    b->insert( e );
                }
                e = buf->get_next_event();
            }
            unlock_mutex(song->hEditMutex);
        }
        buf->ready_to_play=0;
    } else
        buf->rollback_cursor();
    if (set) { 
        //tpb = song->tpb; bpm = song->bpm;
        set_speed();//song->tpb, this->bpm);
        need_refresh++;
    }

}




// ------------------------------------------------------------------------------------------------
//
//
int player::next_order(void) {
    // increment player's current order.
    // return 0 if successfull, 1 if not

    int pass=0;
    cur_order++;
    while(song->orderlist[cur_order] > 0xFF && pass<3) {
        if (song->orderlist[cur_order] == 0x100) {
            cur_order = 0;
            cur_pattern = song->orderlist[cur_order];
            return 1;
        }
        while(song->orderlist[cur_order] == 0x101 && cur_order < 256) 
            cur_order++;
        pass++;
    }
    cur_pattern = song->orderlist[cur_order];
    return 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void player::num_orders(void) {
    int i=0;
    while ( (song->orderlist[i] == 0x101 || song->orderlist[i] <=0xFF) && i<256)
        i++;
    this->num_real_orders = i;
}





#define _clamp( num, max) if (num>max) num = max

// ------------------------------------------------------------------------------------------------
//
//
void player::playback(midi_buf *buffer, int ticks) 
{
  int t,add,j,jstep ;
  unsigned char vol,chan,inst,flags ;
  int unote,uvol ;
  short int len ;
  track *Track ;
  instrument *i ;
  event *evento ;
  int p_tick, die ;
  int is_pitchslide ;           // feature 419820, tlr


  die=0 ;


/*
  int timeout = 0;
  while(this->edit_lock && timeout<20) {
  timeout++; SDL_Delay(1);
  } 
*/


  if (!buffer->ready_to_play && 
    lock_mutex(song->hEditMutex, EDIT_LOCK_TIMEOUT)) {

      //      this->edit_lock = 1;

      if (!starts) buffer->reset();
      else starts = 0;

      for (p_tick = 0; p_tick < ticks; p_tick++) {

        if (clock_count >= clock_len_ms) clock_count = 0;

        if (clock_count == 0) {

          if (song->flag_SendMidiClock) buffer->insert(p_tick,ET_CLOCK,0x0); //MidiOut->clock();
        }

        if (clock_count == 0 && song->flag_SlideOnSubtick) {    // feature 419820, tlr

          /* run pitchslides on clock_count. */                 // feature 419820, tlr

          for(t=0;t<MAX_TRACKS;t++) {                           // feature 419820, tlr
            //Track = song->patterns[cur_pattern]->tracks[t];   // feature 419820, tlr
            Track = patt_memory.tracks[t];

            if (Track->pitch_slide) {                           // feature 419820, tlr

              Track->pitch_setting += Track->pitch_slide;       // feature 419820, tlr

              if (Track->pitch_setting<0)                       // feature 419820, tlr
                Track->pitch_setting = 0;                       // feature 419820, tlr

              if (Track->pitch_setting>0x3FFF)                  // feature 419820, tlr
                Track->pitch_setting = 0x3FFF;                  // feature 419820, tlr

              inst = Track->default_inst;                       // feature 419820, tlr

              if (inst<MAX_INSTS) {

                i = song->instruments[inst];                    // feature 419820, tlr

                buffer->insert(p_tick,                          // feature 419820, tlr
                  ET_PITCH,                                     // feature 419820, tlr
                  i->midi_device,i->channel,                    // feature 419820, tlr
                  GET_HIGH_BYTE(Track->pitch_setting),          // feature 419820, tlr
                  GET_LOW_BYTE(Track->pitch_setting),           // feature 419820, tlr
                  t,                                            // feature 419820, tlr
                  inst);                                        // feature 419820, tlr
              }
            }                                                   // feature 419820, tlr
          }                                                     // feature 419820, tlr
        }                                                       // feature 419820, tlr

        //clock_count++;                                        // feature 419820, tlr

        if (cur_subtick >= tick_len_ms)
          cur_subtick = 0;

        if (cur_subtick == 0) {
          cur_row++;
          if (skip<0xFFF) {
            if (playmode > 0) {
              if (this->next_order() && !loop_mode)
                die++;
            }
            if (skip>=song->patterns[cur_pattern]->length)
              skip = song->patterns[cur_pattern]->length-1;
            cur_row = skip;
            skip = 0xFFF;

            // i've commented out the following two lines.
            // they were causing midi export to end at effect C0000
            // there should be no reason playback should stop when skip is set
            // and not in loop mode? 
            // note that loop_mode is true whether playing from a pattern or from the song
            // order list.
            //      - lipid
            //if (!loop_mode)
            //  die++;                  
          }

          if (cur_row>=song->patterns[cur_pattern]->length) {

            if (playmode == 0) { // LOOP
              cur_row=0;
            } 
            else { // Song

              cur_row=0;
              buffer->insert(p_tick,ET_PATT,0,cur_pattern);
              if (this->next_order() && !loop_mode)
                die++;
            }
          }

          if ((cur_pattern > 0xFF) || die) {
            this->stop();
            editing = 0;
            sprintf(szStatmsg,"Stopped");
            statusmsg = szStatmsg;
            status_change = 1;
            //                  goto itsover;
          }
        }

        process_noel(buffer,p_tick);

        if (cur_subtick == 0) {

          if (die==0)

            for(t=0;t<MAX_TRACKS;t++) {

              add=0;  
              is_pitchslide=0;          // feature 419820, tlr
              track *real_tr = song->patterns[cur_pattern]->tracks[t];
              //memory_tr = patt_memory.tracks[t];
              Track = patt_memory.tracks[t];
              evento = real_tr->get_event(cur_row);



              inst = Track->default_inst ;

              if (evento) {

                if(evento->effect<0xFF) { 

                  switch(evento->effect) 
                  {   // Global Effects


                  case 'C': // Pattern break      

                    skip = evento->effect_data;

                    break;


                  case 'B': // <Manu> No se si hace falta poner aqui el evento de loop [EN: not sure whether the loop event needs to be placed here]

                    {
                      //                                  buffer->insert(p_tick,ET_LOOP,0,0,evento->effect_data,0,t);
                      buffer->insert( p_tick, ET_LOOP, 0, 0 , evento->effect_data, 0, t, inst, 0) ;
                    }

                    break ;



                  default:
                    break;
                  }
                }


                if (evento->inst < MAX_INSTS) Track->default_inst = inst = evento->inst;
              }


              if (inst < MAX_INSTS) i = song->instruments[inst];
              else i = &blank_instrument;

              vol   = i->default_volume;
              chan  = i->channel;
              len   = i->default_length;
              flags = i->flags;

              //if (Track->default_length > 0)
              //    len = Track->default_length;
              if (Track->default_length > 0) len = Track->default_length;

              if (i->flags & INSTFLAGS_TRACKERMODE) len = LEN_INF;

              if (evento) {

                if (evento->vol < 0x80) vol = evento->vol;
                if (evento->length>0x0) Track->default_length = len = evento->length;
                //Track->default_length = len = evento->length;

                jstep = tick_len_ms;

                if (evento->effect<0xFF) {

                  switch(evento->effect) 
                  {
                  case 'T':

                    j = evento->effect_data;

                    if (j<60) j=60;
                    if (j>240) j=240;

                    buffer->insert(p_tick,ET_BPM,0,j,0,0,t);

                    break;

                  case 'A':

                    if (evento->effect_data == 2  || 
                      evento->effect_data == 4  ||
                      evento->effect_data == 6  ||
                      evento->effect_data == 8  ||
                      evento->effect_data == 12 ||
                      evento->effect_data == 24 ||
                      evento->effect_data == 48 
                      ) {

                        buffer->insert(p_tick,ET_TPB,0,0,evento->effect_data,0,t);
                    }
                    break;

                    /*
                    case 'B': // <Manu> No se si hace falta poner aqui el evento de loop

                    {
                    int pollo ;

                    pollo=40 ;
                    //                                  buffer->insert(p_tick,ET_LOOP,0,0,evento->effect_data,0,t);
                    buffer->insert( p_tick, ET_LOOP, 0, 0 , evento->effect_data, 0, t, inst, 0) ;
                    }
                    */

                    break ;

                  case 'D':

                    if (evento->effect_data) Track->default_delay = evento->effect_data;

                    add = Track->default_delay;

                    if (len<1000) {

                      len += Track->default_delay;
                      if (len>=1000) len=999;
                    }

                    break;

                  case 'E': // Pitchwheel down, feature 419820, tlr

                    if (evento->effect_data>0) {

                      //    Track->default_data = evento->effect_data;
                      Track->pitch_slide = -evento->effect_data;
                    } 
                    else {

                      if (Track->pitch_slide > 0) Track->pitch_slide = -Track->pitch_slide;
                    }

                    is_pitchslide=1;

                    if (!song->flag_SlideOnSubtick) {

                      Track->pitch_setting += Track->pitch_slide;

                      if (Track->pitch_setting<0) Track->pitch_setting = 0;
                      if (Track->pitch_setting>0x3FFF) Track->pitch_setting = 0x3FFF;

                      buffer->insert(p_tick,ET_PITCH,i->midi_device,chan,GET_HIGH_BYTE(Track->pitch_setting), GET_LOW_BYTE(Track->pitch_setting),t,inst);
                    }

                    break;

                  case 'F': // Pitchwheel up, feature 419820, tlr

                    if (evento->effect_data>0) {      

                      //  Track->default_data = evento->effect_data;
                      Track->pitch_slide = evento->effect_data;
                    } 
                    else {

                      if (Track->pitch_slide < 0) Track->pitch_slide = -Track->pitch_slide ;
                    }

                    is_pitchslide=1;

                    if (!song->flag_SlideOnSubtick) {

                      Track->pitch_setting += Track->pitch_slide;
                      if (Track->pitch_setting<0) Track->pitch_setting = 0;
                      if (Track->pitch_setting>0x3FFF) Track->pitch_setting = 0x3FFF;
                      buffer->insert(p_tick,ET_PITCH,i->midi_device,chan,GET_HIGH_BYTE(Track->pitch_setting), GET_LOW_BYTE(Track->pitch_setting),t,inst);
                    }

                    break;

                  case 'W': // Pitchwheel set

                    buffer->insert(p_tick,ET_PITCH,i->midi_device,chan,GET_HIGH_BYTE(evento->effect_data), GET_LOW_BYTE(evento->effect_data),t, inst);
                    Track->pitch_setting = evento->effect_data&0x3FFF;

                    break;

                  case 'Q':

                    if (evento->effect_data) Track->default_fxp = evento->effect_data;
                    jstep = Track->default_fxp;
                    if (!jstep) jstep = tick_len_ms;

                    break ;

                  case 'P':  // P.... Send program change message
                    buffer->insert(p_tick,ET_PC,i->midi_device,i->patch,i->bank,chan,t,inst);
                    //MidiOut->progChange(i->midi_device,i->patch,i->bank,chan);
                    break;

                  case 'S':  // Sxxyy Send CC xx with value yy, >=0x80 is default values

                    if (inst < MAX_INSTS) {

                      Track->default_controller = GET_HIGH_BYTE(evento->effect_data);
                      _clamp(Track->default_controller, 0x7F);

                      Track->default_data = GET_LOW_BYTE(evento->effect_data);
                      _clamp(Track->default_data, 0x7F);

                      buffer->insert(p_tick,ET_CC,i->midi_device,Track->default_controller,Track->default_data,chan,t,inst);
                    }
                    //MidiOut->sendCC(i->midi_device,Track->default_controller,Track->default_data,chan);
                    break;

                  case 'X':  // X..xx set panning to xx (0-0x7F)
                    buffer->insert(p_tick,ET_CC,i->midi_device,0xA,evento->effect_data&0x7F,chan,t);
                    break;
#ifdef USE_ARPEGGIOS
                  case 'R':  // Rxxxx start arpeggio xxxx (loops at repeat_pos)
                    {
                        int arp_idx = evento->effect_data & 0xFF;
                        arpeggio *arp = (arp_idx >= 0 && arp_idx < ZTM_MAX_ARPEGGIOS)
                                            ? song->arpeggios[arp_idx] : NULL;
                        if (arp && !arp->isempty() && arp->length > 0) {
                            int speed_subticks = arp->speed > 0 ? arp->speed : 1;
                            unsigned char base_note = (evento->note < 0x80)
                                                          ? evento->note
                                                          : Track->last_note;
                            // Schedule up to 256 effective steps so a short
                            // looping arpeggio (e.g. length 4, repeat 0)
                            // covers a full pattern at typical tempos. The
                            // user retriggers (or note-off + re-R) to
                            // replace; without per-track ongoing state in
                            // the player, this is the cleanest way to get
                            // real loop behaviour without rewriting the
                            // scheduler.
                            int repeat_pos = arp->repeat_pos;
                            if (repeat_pos < 0)              repeat_pos = 0;
                            if (repeat_pos >= arp->length)   repeat_pos = arp->length - 1;
                            const int MAX_SCHEDULED_STEPS = 256;
                            int s_logical = 0;   // index into arp data
                            for (int n_emitted = 0;
                                 n_emitted < MAX_SCHEDULED_STEPS;
                                 ++n_emitted) {
                                int step_tick = p_tick + add + n_emitted * speed_subticks;
                                unsigned short raw_p = arp->pitch[s_logical];
                                if (raw_p != ZTM_ARP_EMPTY_PITCH) {
                                    int offset = (int16_t)raw_p;
                                    int note = (int)base_note + offset;
                                    if (note < 0)   note = 0;
                                    if (note > 127) note = 127;
                                    buffer->insert(step_tick, ET_NOTE_ON,
                                                   i->midi_device, (unsigned char)note,
                                                   chan | (flags << 4), 0x7F, t, inst);
                                    int off_tick = p_tick + add + (n_emitted + 1) * speed_subticks - 1;
                                    if (off_tick <= step_tick) off_tick = step_tick + 1;
                                    buffer->insert(off_tick, ET_NOTE_OFF,
                                                   i->midi_device, (unsigned char)note,
                                                   chan, 0x40, t, inst);
                                }
                                int nc = arp->num_cc;
                                if (nc > ZTM_ARPEGGIO_NUM_CC) nc = ZTM_ARPEGGIO_NUM_CC;
                                for (int c = 0; c < nc; ++c) {
                                    unsigned char v = arp->ccval[c][s_logical];
                                    if (v == ZTM_ARP_EMPTY_CCVAL) continue;
                                    buffer->insert(step_tick, ET_CC,
                                                   i->midi_device, arp->cc[c],
                                                   v, chan, t, inst);
                                }
                                // Advance — loop back to repeat_pos when
                                // we walk past the end. If repeat_pos == 0
                                // and length == 1 the same step plays
                                // forever (same as the user asked).
                                ++s_logical;
                                if (s_logical >= arp->length) {
                                    s_logical = repeat_pos;
                                }
                            }
                        }
                    }
                    break;
#endif /* USE_ARPEGGIOS */
#ifdef USE_MIDIMACROS
                  case 'Z':  // Zxxyy send midimacro xx with parameter yy
                    {
                        int macro_idx = GET_HIGH_BYTE(evento->effect_data);
                        int param     = GET_LOW_BYTE(evento->effect_data);
                        midimacro *m = (macro_idx >= 0 && macro_idx < ZTM_MAX_MIDIMACROS)
                                           ? song->midimacros[macro_idx] : NULL;
                        if (m && !m->isempty()) {
                            // Walk macro data, packing 3-byte short MIDI msgs
                            // (status + up to 2 data bytes), substituting
                            // PARAM1 with the yy parameter. A new status byte
                            // (high bit set) flushes the previous packed msg
                            // and starts a new one.
                            unsigned int packed = 0;
                            int byte_count = 0;
                            for (int k = 0; k < ZTM_MIDIMACRO_MAXLEN; ++k) {
                                unsigned short v = m->data[k];
                                if (v == ZTM_MIDIMAC_END) break;
                                unsigned char b = (v == ZTM_MIDIMAC_PARAM1)
                                                      ? (unsigned char)param
                                                      : (unsigned char)v;
                                if (b & 0x80) {
                                    if (byte_count > 0) {
                                        buffer->insert(p_tick + add, ET_RAW,
                                                       i->midi_device,
                                                       packed & 0xFF,
                                                       (packed >> 8) & 0xFF,
                                                       (packed >> 16) & 0xFF,
                                                       t, inst);
                                    }
                                    packed = b;
                                    byte_count = 1;
                                } else {
                                    if (byte_count == 0) continue; // running status not supported
                                    packed |= ((unsigned int)b) << (byte_count * 8);
                                    byte_count++;
                                }
                                if (byte_count == 3) {
                                    buffer->insert(p_tick + add, ET_RAW,
                                                   i->midi_device,
                                                   packed & 0xFF,
                                                   (packed >> 8) & 0xFF,
                                                   (packed >> 16) & 0xFF,
                                                   t, inst);
                                    packed = 0;
                                    byte_count = 0;
                                }
                            }
                            if (byte_count > 0) {
                                buffer->insert(p_tick + add, ET_RAW,
                                               i->midi_device,
                                               packed & 0xFF,
                                               (packed >> 8) & 0xFF,
                                               (packed >> 16) & 0xFF,
                                               t, inst);
                            }
                        }
                    }
                    break;
#endif /* USE_MIDIMACROS */
                  default:
                    break;
                  }
                }

                add += MidiOut->get_delay_ticks(i->midi_device);
                if (evento->note == 0x80) {
                  if (evento->vol<0x80) {
                    //i = song->instruments[Track->last_inst];
                    /*
                    if (evento->inst < MAX_INSTS)
                    i = song->instruments[evento->inst];
                    else
                    i = song->instruments[Track->last_inst];
                    */
                    if (i->flags & INSTFLAGS_CHANVOL) buffer->insert(p_tick+add,ET_CC,i->midi_device,0x07,evento->vol,chan,t,inst);
                    else buffer->insert(p_tick+add,ET_AT,i->midi_device,Track->last_note,i->channel,evento->vol,t,inst);
                  }
                }

                if (evento->note < 0x80) {


                  // Aqui j se usa como var1



                  for(j=0;j<tick_len_ms;j+=jstep) {

                    //MidiOut->noteOn(i->midi_device,evento->note,chan,vol);
                    uvol = vol;

                    if (i->global_volume != 0x7F && vol>0) {

                      uvol *= i->global_volume;
                      uvol /= 0x7f;

                      if (!uvol) uvol=vol;
                    }

                    unote = evento->note+i->transpose;

                    if (unote<0) unote = 0;
                    if (unote>0x7F) unote = 0x7F;

                    if (i->flags & INSTFLAGS_TRACKERMODE) {

                      instrument *ii = i;

                      if (Track->last_note < 0x80) {

                        unsigned char ti;

                        ti = kill_noel(Track->last_note,Track->last_inst);

                        if (ti < MAX_INSTS) {

                          ii = song->instruments[ti];
                          buffer->insert(p_tick+add,ET_NOTE_OFF,ii->midi_device,Track->last_note,ii->channel,j,t,ti);
                        } 
                        else {

                          if (Track->last_inst < MAX_INSTS) {

                            ii = song->instruments[Track->last_inst];
                            buffer->insert(p_tick+add,ET_NOTE_OFF,ii->midi_device,Track->last_note,ii->channel,j,t,Track->last_inst);
                          } 
                          else buffer->insert(p_tick+add,ET_NOTE_OFF,ii->midi_device,Track->last_note,ii->channel,j,t,inst);
                        }

                        Track->last_note = 0x80;
                      }   
                    }

                    if (i->flags & INSTFLAGS_CHANVOL) {

                      buffer->insert(p_tick+add,ET_CC,i->midi_device,0x07,uvol,chan,t,inst);
                      buffer->insert(p_tick+add+j,ET_NOTE_ON,i->midi_device,unote,chan | (flags<<4), 0x7F,t,inst);
                    } 
                    else {

                      buffer->insert(p_tick+add+j,ET_NOTE_ON,i->midi_device,unote,chan | (flags<<4),uvol,t,inst);
                    }

                    Track->last_note = unote;
                    Track->last_inst = inst;

                    if (len<1000 && len>=0) {

                      insert_no(unote,inst,len+add+j,t);
                      //char s[100];
                      //sprintf(s, "l:%d a:%d j:%d  ",len,add,j);
                      //debug_display->setstr(s);
                    }// else
                    //  Track->last_inst = MAX_INSTS;
                  }   
                }



                if (evento->note == 0x81 || evento->note==0x82) {

                  (evento->note == 0x81) ? (j=0x0) : (j=0x7F);

                  if (Track->last_note < 0x80) {

                    chan = kill_noel(Track->last_note,Track->last_inst);

                    if ( chan < MAX_INSTS ) {

                      i = song->instruments[chan];
                      buffer->insert(p_tick+add,ET_NOTE_OFF,i->midi_device,Track->last_note,i->channel,j,t,chan);
                    } 
                    else {

                      if (Track->last_inst < MAX_INSTS) {

                        i = song->instruments[Track->last_inst];
                        buffer->insert(p_tick+add,ET_NOTE_OFF,i->midi_device,Track->last_note,i->channel,j,t,Track->last_inst);
                      } 
                      else buffer->insert(p_tick+add,ET_NOTE_OFF,i->midi_device,Track->last_note,i->channel,j,t,inst);
                    }

                    Track->last_note = 0x80;
                  }   
                }   
              }


              // if no pitch slide this tick, then reset the slide counter. // feature 419820, tlr

              if (!is_pitchslide) Track->pitch_slide=0;       // feature 419820, tlr

              //          }  // mute
            }  // For thru tracks

            buffer->insert(p_tick,ET_STATUS,0x0,ztPlayer->cur_order,ztPlayer->cur_pattern,0,0,0,ztPlayer->cur_row);
            buffer->insert(p_tick+1,ET_LIGHT,0x0);
        } // If subtick = 0

        clock_count++;  // feature 419820, tlr
        // do subtick update
        cur_subtick++;
        //            process_noel(buffer,p_tick);
      }

      buffer->ready_to_play=1;

      //      this->edit_lock = 0;
  }

  unlock_mutex(song->hEditMutex) ;
}




/* eof */
