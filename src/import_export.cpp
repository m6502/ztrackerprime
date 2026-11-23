/*************************************************************************
 *
 * FILE  import_export.cpp
 * $Id: import_export.cpp,v 1.18 2002/04/15 21:17:41 zonaj Exp $
 *
 * DESCRIPTION 
 *   Import and export code (mid, it, etc).
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

#include "it.h"

#include "import_export.h"
#include "CDataBuf.h"
#include "jump_command.h"

#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <vector>


//// .MID/.SMF FILE EXPORTER 

#define MID_TEXT       0x1
#define MID_COPYWRITE  0x2
#define MID_SEQNAME    0x3
#define MID_INSTRUMENT 0x4
#define MID_MARKER     0x6
// <MANU> Añado este define de arriba




// ------------------------------------------------------------------------------------------------
// Pushes a Little Endian int into a CDataBuf and converts it to Big Endian
//
void push_le_int(CDataBuf *buf, int i) 
{
  buf->pushc( i>>24 );               // MSB comes first
  buf->pushc( (i>>16)&0xFF );  
  buf->pushc( (i>>8)&0xFF ); 
  buf->pushc( (i)&0xFF );            // LSB comes last
}




// ------------------------------------------------------------------------------------------------
//
//
void push_varlen(CDataBuf *buf, unsigned int value)
{
  unsigned int buffer;
  buffer = value & 0x7F;
  
  while ( (value >>= 7) ) {

    buffer <<= 8;
    buffer |= ((value & 0x7F) | 0x80);
  }
  
  while(true) {

    buf->pushc((unsigned char)(buffer&0xFF)) ;
    
    if (buffer & 0x80) buffer >>= 8;
    else break;
  }
}




// ------------------------------------------------------------------------------------------------
// <Manu> Creo esta funcion [EN: I'm creating this function]
//
void push_marker(CDataBuf *buf, unsigned char type, char *str, int len=-1) 
{
  len=1 ;

  //push_varlen(buf, 0);          // El delta time es de tamaño variable, aunque sea lo mismo parece peligroso ponerlo a pelo

  buf->pushc(0);                  // Delta time
  buf->pushc(0xFF);               // Meta-Evento
  buf->pushc(type);               // Tipo del evento
  push_varlen(buf, len);          // Tamanyo de los datos
  buf->write(str,len);            // Cadena de texto
}




// ------------------------------------------------------------------------------------------------
// Jnnnn export: "[nnnn" at row 0 of every jump target order, "]nnnn" at the end of the J row.
// Returns '[' or ']' for a marker event, 0 otherwise.
//
static int jump_marker_kind(const midi_event *e, const bool targets[ZT_JUMP_MAX_ORDERS])
{
  if (e->type == ET_JUMP) return ']';
  if (e->type == ET_ORDER && e->data1 < ZT_JUMP_MAX_ORDERS && targets[e->data1]) return '[';
  return 0;
}

static void push_jump_marker(CDataBuf *buf, int &dtime, bool loop_start, unsigned short value)
{
  char text[8];
  int len = zt_jump_marker_text(text, loop_start, value);
  push_varlen(buf, dtime);
  dtime = 0;
  buf->pushuc(0xFF);
  buf->pushuc(0x06);               // Marker meta-event
  push_varlen(buf, len);
  buf->write(text, len);
}

static void collect_jump_targets(zt_module *song, bool targets[ZT_JUMP_MAX_ORDERS])
{
  zt_jump_collect_targets(song->orderlist, ZTM_ORDERLIST_LEN, targets,
    [song](int p, auto fn) {
      for (int t = 0; t < MAX_TRACKS; t++)
        for (event *e = song->patterns[p]->tracks[t]->event_list; e; e = e->next_event)
          fn(e->effect, e->effect_data);
    });
}




// ------------------------------------------------------------------------------------------------
// Note releases caused by J loops.
//
// Export never follows a jump, but a game looping on the markers does: a note
// still held at "]" goes on sounding into the loop body, and the tracker
// releases it there (a later note on its track cuts it, a note-off cell, or
// its length running out). The straight pass never writes those releases, so
// a probe pass follows every J once and records, for each note held at a jump,
// where in the loop body it was released: an order/row position plus the
// export ticks into that row. The write pass puts those NOTE_OFFs there.
//
static int  zt_export_jump_conflicts = 0;
static char zt_export_jump_conflict_where[64] = "";

const char *zt_export_jump_warning(void)
{
  static char msg[160];
  if (!zt_export_jump_conflicts) return NULL;
  snprintf(msg, sizeof(msg), "Warning: %d J loop note-off(s) also cut a note on the first pass (first: %s)",
           zt_export_jump_conflicts, zt_export_jump_conflict_where);
  return msg;
}

typedef std::tuple<unsigned int, int, int, int> note_key;        // device, channel, note, track
typedef std::tuple<unsigned int, int, int> voice_key;            // device, channel, note
struct jump_release { int ticks_into_row; midi_event note_off; };
typedef std::map<std::pair<int, int>, std::vector<jump_release>> release_plan;   // (order, row)

static bool is_note_on(const midi_event *e)  { return e->type == ET_NOTE_ON && e->data2 > 0; }
static bool is_note_off(const midi_event *e) { return e->type == ET_NOTE_OFF || (e->type == ET_NOTE_ON && e->data2 == 0); }
static note_key  key_of(const midi_event *e)   { return note_key(e->device, e->data1 & 0x0F, e->command, e->track); }
static voice_key voice_of(const midi_event *e) { return voice_key(e->device, e->data1 & 0x0F, e->command); }

// Tracks the export tick within the current row across playback(buf, 1) calls.
struct export_row_clock {
  unsigned int serial = ~0u;
  int ticks_into_row = 0;
  bool new_row = false;
  void step() {
    new_row = ztPlayer->row_serial != serial;
    serial = ztPlayer->row_serial;
    ticks_into_row = new_row ? 0 : ticks_into_row + 1;
  }
};

static const long EXPORT_TICK_LIMIT = 50000000;

static int export_tempo_event(midi_event *e);

static release_plan probe_jump_releases()
{
  release_plan plan;
  std::multimap<note_key, midi_event> sounding;
  std::map<int, std::multiset<note_key>> held_at_jump;   // jump site -> notes held when it jumped
  export_row_clock clock;
  midi_buf *buf = new midi_buf;
  midi_event *e;

  ztPlayer->jump_mode = ZT_JUMPS_FOLLOW_ONCE;
  ztPlayer->prepare_play(0, 0, 1, 0);

  for (long n = 0; ztPlayer->pinit && n < EXPORT_TICK_LIMIT; n++) {
    buf->reset();
    ztPlayer->playback(buf, 1);
    clock.step();
    const std::pair<int, int> pos(ztPlayer->cur_order, ztPlayer->cur_row);

    while ((e = buf->get_next_event())) {
      export_tempo_event(e);

      if (e->type == ET_JUMP) {
        const int site = zt_jump_site(e->data2, e->extra);
        // Back at a J already followed: what is still held rings through the
        // whole loop in the tracker too.
        held_at_jump.erase(site);
        if (e->command) {
          std::multiset<note_key> &held = held_at_jump[site];
          for (auto &s : sounding) held.insert(s.first);
        }
      }
      else if (is_note_on(e)) {
        sounding.insert(std::make_pair(key_of(e), *e));
      }
      else if (is_note_off(e)) {
        const note_key key = key_of(e);
        auto s = sounding.find(key);
        if (s != sounding.end()) sounding.erase(s);

        bool released = false;
        for (auto &h : held_at_jump) {
          auto it = h.second.find(key);
          if (it != h.second.end()) { h.second.erase(it); released = true; }
        }
        if (released) plan[pos].push_back({clock.ticks_into_row, *e});
      }
    }
  }

  delete buf;

  for (auto &p : plan)
    std::stable_sort(p.second.begin(), p.second.end(),
                     [](const jump_release &a, const jump_release &b) { return a.ticks_into_row < b.ticks_into_row; });
  return plan;
}

// Plays the song once, straight through, for a MIDI export. Hands every event
// to on_event() in file order and calls end_of_tick() after each export tick.
// On top of the player's own events it adds the loop releases from
// probe_jump_releases() and, at the end, a NOTE_OFF for every note still held
// (unless the song ends on a "]").
// Returns the number of loop releases that also cut a note sounding there on
// the first pass (see zt_export_jump_conflict_where).
template <class OnEvent, class EndOfTick>
static int play_for_export(OnEvent on_event, EndOfTick end_of_tick)
{
  const release_plan plan = probe_jump_releases();

  std::multimap<voice_key, midi_event> sounding;
  std::vector<midi_event> tick_events;
  std::vector<midi_event> releases;
  const std::vector<jump_release> *row_releases = NULL;
  size_t next_release = 0;
  export_row_clock clock;
  midi_buf *buf = new midi_buf;
  midi_event *e;
  int conflicts = 0;
  bool ends_on_loop = false;

  zt_export_jump_conflict_where[0] = 0;

  auto emit = [&](midi_event *ev) {
    if (is_note_on(ev)) sounding.insert(std::make_pair(voice_of(ev), *ev));
    else if (is_note_off(ev)) {
      auto s = sounding.find(voice_of(ev));
      if (s != sounding.end()) sounding.erase(s);
    }
    on_event(ev);
  };

  ztPlayer->jump_mode = ZT_JUMPS_IGNORE;
  ztPlayer->prepare_play(0, 0, 1, 0);

  for (long n = 0; ztPlayer->pinit && n < EXPORT_TICK_LIMIT; n++) {
    buf->reset();
    ztPlayer->playback(buf, 1);
    clock.step();

    tick_events.clear();
    while ((e = buf->get_next_event())) tick_events.push_back(*e);

    releases.clear();
    if (clock.new_row) {
      // Releases timed past the end of the row they belong to (a tempo change
      // inside the loop) are written at the start of the next one.
      while (row_releases && next_release < row_releases->size())
        releases.push_back((*row_releases)[next_release++].note_off);
      auto it = plan.find(std::make_pair(ztPlayer->cur_order, ztPlayer->cur_row));
      row_releases = (it == plan.end()) ? NULL : &it->second;
      next_release = 0;
    }
    while (row_releases && next_release < row_releases->size() &&
           (*row_releases)[next_release].ticks_into_row <= clock.ticks_into_row)
      releases.push_back((*row_releases)[next_release++].note_off);

    // File order: the player's events up to its last loop marker (so a
    // release on the "[" row is inside the loop), the loop releases, the rest.
    size_t split = 0;
    for (size_t i = 0; i < tick_events.size(); i++)
      if (tick_events[i].type == ET_JUMP || tick_events[i].type == ET_ORDER) split = i + 1;

    for (size_t i = 0; i < split; i++) emit(&tick_events[i]);

    std::set<voice_key> written;
    for (midi_event &off : releases) {
      const voice_key voice = voice_of(&off);
      bool already_released = !written.insert(voice).second;
      for (size_t i = split; i < tick_events.size() && !already_released; i++)
        already_released = is_note_off(&tick_events[i]) && voice_of(&tick_events[i]) == voice;
      if (already_released) continue;

      if (sounding.count(voice)) {
        if (!conflicts++)
          snprintf(zt_export_jump_conflict_where, sizeof(zt_export_jump_conflict_where),
                   "order %d row %d track %d", ztPlayer->cur_order, ztPlayer->cur_row, off.track + 1);
        fprintf(stderr, "MIDI export: the J loop release of note %d (track %d) at order %d row %d "
                "also cuts that note on the first pass\n",
                off.command, off.track + 1, ztPlayer->cur_order, ztPlayer->cur_row);
      }
      emit(&off);
    }

    for (size_t i = split; i < tick_events.size(); i++) emit(&tick_events[i]);

    ends_on_loop = false;
    for (const midi_event &ev : tick_events) ends_on_loop |= ev.type == ET_JUMP;

    // The tick the song stops on is its end, not one more tick of song.
    if (ztPlayer->pinit) end_of_tick();
  }

  // A song that ends on a "]" never gets past it in a looping game: its held
  // notes ring into the loop and are released there by the loop releases.
  // A NOTE_OFF here would sit on the "]" tick and cut them before the jump.
  if (!ends_on_loop) {
    for (auto &s : sounding) {
      midi_event off = s.second;
      off.type = ET_NOTE_OFF;
      off.data1 &= 0x0F;
      off.data2 = 0;
      on_event(&off);
    }
  }

  delete buf;
  ztPlayer->jump_mode = ZT_JUMPS_FOLLOW;
  zt_export_jump_conflicts = conflicts;
  return conflicts;
}




// ------------------------------------------------------------------------------------------------
//
//
void push_text(CDataBuf *buf, unsigned char type, char *str, int len=-1) 
{
  if (len==-1) len = strlen(str) ;
  
  //push_varlen(buf, 0);          // El delta time es de tamaño variable, aunque sea lo mismo parece peligroso ponerlo a pelo

  buf->pushc(0);                  // Delta time
  buf->pushc(0xFF);               // Meta-Evento
  buf->pushc(type);               // Tipo del evento
  push_varlen(buf, len);          // Tamanyo de los datos
  buf->write(str,len);            // Cadena de texto
}




// ------------------------------------------------------------------------------------------------
//
//
void find_insts(unsigned char iflag[MAX_INSTS], zt_module *song) 
{
  int i,j;
  
  event *e;
  
  for (i=0;i<256;i++) {

    for (j=0;j<MAX_TRACKS;j++){

      e = song->patterns[i]->tracks[j]->event_list;

      while(e) {

        if (e->inst < MAX_INSTS) iflag[e->inst]=1 ;
        e = e->next_event;
      }
    }
  }
}


// ------------------------------------------------------------------------------------------------
// Tempo (T) and ticks per beat (A) during an export.
//
// The player only ANNOUNCES a T or A change: it puts an ET_BPM / ET_TPB event in
// the buffer, and the live output loop is what applies it (playback.cpp, the
// ET_BPM / ET_TPB cases before apply_speed()). The exporters run the same player
// but only wrote the buffer's MIDI events, so a T or A in the song was lost: the
// file kept the song's starting tempo, and every row kept the starting TPB's
// length (which also changes how many slide steps fall in a row). Note that until
// 2026-11-12 live playback dropped them too: it called set_speed(), which reloads
// the SONG's bpm/tpb, so neither command ever did anything anywhere.
//
// export_tempo_event() does what the live loop does, for every exporter. It
// returns the new tempo in microseconds per quarter note when the caller must
// write a tempo meta-event (see push_tempo), and 0 otherwise. Applied as the
// buffer is read, like the live loop, so it takes effect from the next subtick
// and the row being played gets the new length -- the same as playing the song.
static int export_tempo_event(midi_event *e)
{
  if (e->type == ET_BPM) {
    if (e->command == 0) return 0;
    ztPlayer->bpm = e->command;
    ztPlayer->apply_speed();
    return 60000000 / e->command;
  }
  if (e->type == ET_TPB) {
    ztPlayer->tpb = e->data1;
    ztPlayer->apply_speed();
  }
  return 0;
}

// A tempo meta-event (FF 51 03 tttttt) at dtime_ref ticks since the track's last event.
static void push_tempo(CDataBuf *mp, int &dtime_ref, int us_per_quarter)
{
  push_varlen(mp, dtime_ref);
  dtime_ref = 0;
  mp->pushc(0xFF);
  mp->pushc(0x51);
  mp->pushc(0x03);
  mp->pushc((us_per_quarter >> 16) & 0xff);
  mp->pushc((us_per_quarter >> 8) & 0xff);
  mp->pushc((us_per_quarter) & 0xff);
}



// ------------------------------------------------------------------------------------------------
//
//
int ZTImportExport::ExportMID(const char *fn, int format)
{
  (void)format;
  CDataBuf buffer, mtrk[MAX_INSTS+1], *mp;
  int bpm, i, rstatus[MAX_INSTS+1], stat;
  FILE *fp;
  int dtime[MAX_INSTS+1], total_mtrks=1, trk;
  int mtrk_imap[MAX_INSTS];
  unsigned char iflag[MAX_INSTS];
  char* midiOutName;
  char str[1024];
//  char f[256];
  
  for(i=0;i<MAX_INSTS;i++) {

    mtrk_imap[i] = 0;
    iflag[i]=0;
    rstatus[i] = 0xFF;
    dtime[i] = 0;
  }
  
  find_insts(iflag, song);
  
  for (i=0;i<MAX_INSTS;i++) {
    
    if (iflag[i]) {
      
      if (!mtrk_imap[i+1]) {
        
        mtrk_imap[i+1] = total_mtrks;
        
        // song->instruments[i]->midi_device will be set to 255 if it is not set
        // to a valid midi device.

        if(song->instruments[i]->midi_device < MidiOut->numOuputDevices) {
        
          if((MidiOut->outputDevices[song->instruments[i]->midi_device]->alias != NULL &&
             strlen(MidiOut->outputDevices[song->instruments[i]->midi_device]->alias)>1)) {
            
            midiOutName=MidiOut->outputDevices[song->instruments[i]->midi_device]->alias ;
          }
          else {
          
            midiOutName=MidiOut->outputDevices[song->instruments[i]->midi_device]->szName ;
          }
          
          
        }
        else midiOutName = song->instruments[i]->midi_route.named() ?
            song->instruments[i]->midi_route.name : (char *)"Unavailable MIDI output";

        
        if(midiOutName == NULL) sprintf(str,"%s (%s)",song->instruments[i]->title, "") ;
        else                    sprintf(str,"%s (%s)",song->instruments[i]->title, midiOutName) ;
        
        
        push_text( &mtrk[total_mtrks], MID_SEQNAME, (char *) &str[0] ) ;
        
        mtrk[total_mtrks].pushc(0); 
        mtrk[total_mtrks].pushc(0xFF); 
        mtrk[total_mtrks].pushuc(0x21);
        mtrk[total_mtrks].pushuc(0x1);
        mtrk[total_mtrks].pushuc(song->instruments[i]->midi_device);
        
        total_mtrks++;
      }
    }
  }
  
  if (!total_mtrks) return(0) ;
  
  if(!(fp = fopen(fn, "wb"))) return(0) ;
  
  buffer.write("MThd",4);
  push_le_int(&buffer, 6);
  
  buffer.pushc(0); buffer.pushc(1); // MIDI Format 1
  buffer.pushc(0); buffer.pushc(total_mtrks); // Number of MTrks (1 per midi device)
  buffer.pushc(0); buffer.pushc(96); // 96 PPQN
  

  // Put good stuff into the first MTrk

//  <Manu> Elimino estos eventos que no sirven para nada [EN: removing these events that serve no purpose]
  
//  mtrk[0].pushuc(0);  
//  mtrk[0].pushuc(0xFF);
//  mtrk[0].pushuc(3);  
//  mtrk[0].pushuc(0);
//  push_text(&mtrk[0], MID_SEQNAME,  (char *)song->title );
  
//  strcpy(&f[0], "Original filename: ");
//  strcat((char *)f, (const char *)song->filename);
  
//  
//  push_text(&mtrk[0], MID_SEQNAME,  (char *)&f[0] );
//  push_text(&mtrk[0], MID_COPYWRITE, "Sequenced/exported with ztracker - http://ztracker.sourceforge.net/") ;
  
  // Metemos el tempo
  
  push_varlen(&mtrk[0], 0);
  mtrk[0].pushc(0xFF); 
  mtrk[0].pushc(0x51); 
  mtrk[0].pushc(0x03);  // Set tempo

  bpm = 60000000/song->bpm;
  mtrk[0].pushc( (bpm>>16) &0xff );
  mtrk[0].pushc( (bpm>>8) &0xff );
  mtrk[0].pushc( (bpm) &0xff );
  
  // Enough of that tomfoolery
  
  bool jump_targets[ZT_JUMP_MAX_ORDERS];
  collect_jump_targets(song, jump_targets);

  play_for_export([&](midi_event *e) {

      { // T / A: tempo into the conductor track (mtrk[0]), row length into the player
        int us = export_tempo_event(e);
        if (us) push_tempo(&mtrk[0], dtime[0], us);
      }

      if (int kind = jump_marker_kind(e, jump_targets)) {
        push_jump_marker(&mtrk[0], dtime[0], kind == '[', e->data1);
      }
      else {
          
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
      
      if (e->inst < MAX_INSTS) {
        
        trk = mtrk_imap[e->inst+1];
        mp = &mtrk[trk];

        switch(e->type)
        {
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          
        case ET_NOTE_OFF:   
          
          e->data2 = 0x0;
          
        case ET_NOTE_ON:
          
          push_varlen(mp, dtime[trk]);
          dtime[trk] = 0;
          stat = (e->data1&0x0F) + 0x90;
          //                      if (rstatus[chn] != stat) {
          mp->pushuc( stat ); // Note on
          //                          rstatus[chn] = stat;
          //                      }
          mp->pushuc( e->command );             // Note number
          mp->pushuc( (unsigned char)e->data2 );
          
          break;
          
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          
        case ET_PC: 
          
          if (e->data1 <= 0x3FFF) {
            
            unsigned short int bank;
            unsigned char hb,lb;

            bank = e->data1;
            bank &= 0x3fff;
            lb = bank&0x007F;
            hb = bank>>7;
            push_varlen(mp, dtime[trk]);
            dtime[trk] = 0;
            mp->pushuc(0xB0 + e->data2);
            mp->pushuc(0x0);
            mp->pushuc(lb);
            push_varlen(mp, 0);
            mp->pushuc(0xB0 + e->data2);
            mp->pushuc(0x20);
            mp->pushuc(hb);
          }
          
          push_varlen(mp, dtime[trk]);
          dtime[trk] = 0;
          mp->pushuc(0xC0 + e->data2);
          mp->pushuc(e->command);
          
          break;

          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          
        case ET_CC:
          
          push_varlen(mp, dtime[trk]);
          dtime[trk] = 0;
          mp->pushuc(0xB0 + e->data2);
          mp->pushuc(e->command);
          mp->pushuc((unsigned char)e->data1);
          
          break;

          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          
        case ET_PITCH:
          
          // <Manu> Añado llaves para que compile al poner el case de abajo ¿? [EN: adding braces so it compiles when the case below is added]
          
          { 
            unsigned short int usi ; 
            unsigned char d1,d2;


            usi = (unsigned short int)e->data1<<8; 
            usi+=e->data2;
            

            usi &= 0x3FFF;
            d1 = (usi & 0x007F);
            d2 = usi >> 7;
            push_varlen(mp, dtime[trk]);
            dtime[trk] = 0;
            mp->pushuc(0xe0 + e->command);
            mp->pushuc(d1);
            mp->pushuc(d2);
            
          }
          
          break;


        default:

          // Si llega aqui es algo raro?

          break ;
          
        } ;
          
      }

      } // Llave de mi if
  },
  [&]() {
    for (trk=0;trk<MAX_INSTS+1;trk++) dtime[trk]++;
  });
    
  // END MTrk         
  for (i=0;i<total_mtrks;i++) {
    
    // Delta dtime[], NOT 0: dtime is the ticks since this track's last event, so a
    // 0 here ends the MIDI track on its final note and throws away every tick of
    // trailing silence -- the rest of the last pattern, and any silent pattern
    // after it. A reader that sizes the song from its events then loops early.
    push_varlen(&mtrk[i], dtime[i]) ;
    mtrk[i].pushc(0xFF); 
    mtrk[i].pushc(0x2f); 
    mtrk[i].pushc(0x00);  // End of track

    buffer.write("MTrk",4);
    push_le_int(&buffer, mtrk[i].getsize());
    buffer.write(mtrk[i].getbuffer(),mtrk[i].getsize());
  }
  
  // Grabamos el buffer al disco, cerramos el fichero y volvemos sin error
  
  fwrite(buffer.getbuffer(),buffer.getsize(),1,fp) ;
  fclose(fp) ;

  return(1) ;
}




// ------------------------------------------------------------------------------------------------
// Helper: scan all patterns/tracks and mark which zTracker tracks have any
// event with an instrument assigned. Needed so empty tracks don't create
// stub MTrks.
//
// A MUTED track is not exported. Cleared by --export-muted, which puts back the
// behaviour every MIDI export had until 2026-09-08: these exports drive the
// player and drain its event buffer, and that buffer is filled BEFORE
// playback.cpp consults song->track_mute (the mute is applied further down, at
// the MIDI-out consumer), so a muted track came out of the file anyway.
//
// Muted means muted, hence the default. It is worth knowing what it costs
// downstream, though: two tracks sharing a MIDI channel produce overlapping
// notes, and a converter for a monophonic target (e.g. a chiptune) has to drop
// one of each pair -- so exporting muted tracks can silently wreck a conversion.
// The other direction has a price too: if a track is muted only so it does not
// reach your synth while you compose, it disappears from the export.
int zt_export_muted_tracks = 0;

static void find_used_tracks(unsigned char tflag[MAX_TRACKS], zt_module *song)
{
  for (int i = 0; i < MAX_TRACKS; i++) tflag[i] = 0;

  for (int p = 0; p < 256; p++) {

    for (int t = 0; t < MAX_TRACKS; t++) {

      if (!zt_export_muted_tracks && song->track_mute[t]) continue;

      event *e = song->patterns[p]->tracks[t]->event_list;

      while (e) {

        if (e->inst < MAX_INSTS) { tflag[t] = 1; break; }
        e = e->next_event;
      }
    }
  }
}




// ------------------------------------------------------------------------------------------------
// Write a single MIDI event into a per-track CDataBuf. Mirrors the event
// encoding used by ExportMID(). Returns nothing; mutates mp and dtime_ref.
//
static void push_midi_event(CDataBuf *mp, midi_event *e, int &dtime_ref)
{
  int stat;

  switch (e->type) {

    case ET_NOTE_OFF:
      e->data2 = 0x0;
      // fall through
    case ET_NOTE_ON:
      push_varlen(mp, dtime_ref);
      dtime_ref = 0;
      stat = (e->data1 & 0x0F) + 0x90;
      mp->pushuc(stat);
      mp->pushuc(e->command);
      mp->pushuc((unsigned char)e->data2);
      break;

    case ET_PC:
      if (e->data1 <= 0x3FFF) {
        unsigned short int bank;
        unsigned char hb, lb;
        bank = e->data1;
        bank &= 0x3fff;
        lb = bank & 0x007F;
        hb = bank >> 7;
        push_varlen(mp, dtime_ref);
        dtime_ref = 0;
        mp->pushuc(0xB0 + e->data2);
        mp->pushuc(0x0);
        mp->pushuc(lb);
        push_varlen(mp, 0);
        mp->pushuc(0xB0 + e->data2);
        mp->pushuc(0x20);
        mp->pushuc(hb);
      }
      push_varlen(mp, dtime_ref);
      dtime_ref = 0;
      mp->pushuc(0xC0 + e->data2);
      mp->pushuc(e->command);
      break;

    case ET_CC:
      push_varlen(mp, dtime_ref);
      dtime_ref = 0;
      mp->pushuc(0xB0 + e->data2);
      mp->pushuc(e->command);
      mp->pushuc((unsigned char)e->data1);
      break;

    case ET_PITCH: {
      unsigned short int usi = (unsigned short int)e->data1 << 8;
      usi += e->data2;
      unsigned char d1, d2;
      usi &= 0x3FFF;
      d1 = (usi & 0x007F);
      d2 = usi >> 7;
      push_varlen(mp, dtime_ref);
      dtime_ref = 0;
      mp->pushuc(0xe0 + e->command);
      mp->pushuc(d1);
      mp->pushuc(d2);
      break;
    }

    default:
      break;
  }
}




// ------------------------------------------------------------------------------------------------
// ExportMultitrackMID: writes a MIDI Type 1 file where each zTracker
// track that contains events becomes its own MIDI track. The MIDI channel
// encoded in each status byte is preserved from the source event's
// data1 low nybble (which playback populates from the instrument's
// channel assignment).
//
int ZTImportExport::ExportMultitrackMID(const char *fn)
{
  CDataBuf buffer;
  CDataBuf mtrk[MAX_TRACKS + 1]; // track 0 = conductor, 1..N = per-track
  FILE *fp;
  int dtime[MAX_TRACKS + 1];
  int mtrk_tmap[MAX_TRACKS]; // zt track idx -> mtrk idx (0 if unused)
  unsigned char tflag[MAX_TRACKS];
  int total_mtrks = 1; // conductor
  int bpm;
  char str[256];

  for (int i = 0; i < MAX_TRACKS + 1; i++) dtime[i] = 0;
  for (int i = 0; i < MAX_TRACKS; i++) mtrk_tmap[i] = 0;

  find_used_tracks(tflag, song);

  for (int t = 0; t < MAX_TRACKS; t++) {
    if (tflag[t]) {
      mtrk_tmap[t] = total_mtrks;
      sprintf(str, "Track %02d", t + 1);
      push_text(&mtrk[total_mtrks], MID_SEQNAME, (char *)&str[0]);
      total_mtrks++;
    }
  }

  if (total_mtrks <= 1) return 0; // nothing to export

  if (!(fp = fopen(fn, "wb"))) return 0;

  // MThd
  buffer.write("MThd", 4);
  push_le_int(&buffer, 6);
  buffer.pushc(0); buffer.pushc(1);           // format 1
  buffer.pushc((total_mtrks >> 8) & 0xFF);
  buffer.pushc(total_mtrks & 0xFF);           // ntrks
  buffer.pushc(0); buffer.pushc(96);          // 96 PPQN

  // Conductor track: tempo
  push_varlen(&mtrk[0], 0);
  mtrk[0].pushc(0xFF);
  mtrk[0].pushc(0x51);
  mtrk[0].pushc(0x03);
  bpm = 60000000 / song->bpm;
  mtrk[0].pushc((bpm >> 16) & 0xff);
  mtrk[0].pushc((bpm >> 8) & 0xff);
  mtrk[0].pushc((bpm) & 0xff);

  bool jump_targets[ZT_JUMP_MAX_ORDERS];
  collect_jump_targets(song, jump_targets);

  // Play through song, routing events by e->track.
  play_for_export([&](midi_event *e) {
    { // T / A: tempo into the conductor track (mtrk[0]), row length into the player
      int us = export_tempo_event(e);
      if (us) push_tempo(&mtrk[0], dtime[0], us);
    }

    if (int kind = jump_marker_kind(e, jump_targets)) {
      push_jump_marker(&mtrk[0], dtime[0], kind == '[', e->data1);
      return;
    }
    if (e->inst >= MAX_INSTS) return;
    if (e->track >= MAX_TRACKS) return;

    int trk = mtrk_tmap[e->track];
    if (trk == 0) return; // track wasn't tagged as used

    push_midi_event(&mtrk[trk], e, dtime[trk]);
  },
  [&]() {
    for (int i = 0; i < MAX_TRACKS + 1; i++) dtime[i]++;
  });

  // End-of-track + assemble chunks.
  //
  // The delta here is dtime[i], NOT 0, and that is the whole length of the song
  // as far as any reader is concerned. dtime[i] is the ticks elapsed on this
  // track since its last event, so with a 0 delta every MIDI track ends on its
  // final note and ALL TRAILING SILENCE IS LOST -- the rest of the last pattern,
  // and any whole pattern after the last note. A reader that derives the song
  // length from the events (a converter sizing a looping stream, for example)
  // then loops early: one song lost 62 of its 2048 rows, a full pattern.
  for (int i = 0; i < total_mtrks; i++) {

    push_varlen(&mtrk[i], dtime[i]);
    mtrk[i].pushc(0xFF);
    mtrk[i].pushc(0x2f);
    mtrk[i].pushc(0x00);

    buffer.write("MTrk", 4);
    push_le_int(&buffer, mtrk[i].getsize());
    buffer.write(mtrk[i].getbuffer(), mtrk[i].getsize());
  }

  fwrite(buffer.getbuffer(), buffer.getsize(), 1, fp);
  fclose(fp);

  return 1;
}




// ------------------------------------------------------------------------------------------------
// ExportPerTrackMID: writes one Type 0 .mid file per zTracker track with
// events. Output filenames: if fn ends in ".mid" (case-insensitive) the
// suffix is stripped to form a basename; each file is then written to
// "<basename>_track<NN>.mid".
//
int ZTImportExport::ExportPerTrackMID(const char *fn)
{
  unsigned char tflag[MAX_TRACKS];
  find_used_tracks(tflag, song);

  int any = 0;
  for (int t = 0; t < MAX_TRACKS; t++) if (tflag[t]) { any = 1; break; }
  if (!any) return 0;

  // Derive basename (strip trailing .mid if present).
  char base[512];
  {
    int n = (int)strlen(fn);
    if (n >= (int)sizeof(base)) n = (int)sizeof(base) - 1;
    memcpy(base, fn, n);
    base[n] = 0;
    if (n >= 4 &&
        (base[n-4] == '.') &&
        (base[n-3] == 'm' || base[n-3] == 'M') &&
        (base[n-2] == 'i' || base[n-2] == 'I') &&
        (base[n-1] == 'd' || base[n-1] == 'D')) {
      base[n-4] = 0;
    }
  }

  // First, collect per-track deltas + event buffers by playing the song once.
  CDataBuf mtrk[MAX_TRACKS];
  int dtime[MAX_TRACKS];
  for (int i = 0; i < MAX_TRACKS; i++) dtime[i] = 0;

  // Seed each used track with its sequence name.
  for (int t = 0; t < MAX_TRACKS; t++) {
    if (tflag[t]) {
      char str[64];
      sprintf(str, "Track %02d", t + 1);
      push_text(&mtrk[t], MID_SEQNAME, (char *)&str[0]);
    }
  }

  // Tempo goes into each per-track file (Type 0 has exactly one track that
  // must carry tempo if the file is to play standalone).
  int bpm = 60000000 / song->bpm;
  for (int t = 0; t < MAX_TRACKS; t++) {
    if (!tflag[t]) continue;
    push_varlen(&mtrk[t], 0);
    mtrk[t].pushc(0xFF);
    mtrk[t].pushc(0x51);
    mtrk[t].pushc(0x03);
    mtrk[t].pushc((bpm >> 16) & 0xff);
    mtrk[t].pushc((bpm >> 8) & 0xff);
    mtrk[t].pushc((bpm) & 0xff);
  }

  bool jump_targets[ZT_JUMP_MAX_ORDERS];
  collect_jump_targets(song, jump_targets);

  play_for_export([&](midi_event *e) {
    { // T / A: every file is played on its own, so each one carries the tempo
      int us = export_tempo_event(e);
      if (us) {
        for (int t = 0; t < MAX_TRACKS; t++)
          if (tflag[t]) push_tempo(&mtrk[t], dtime[t], us);
      }
    }

    if (int kind = jump_marker_kind(e, jump_targets)) {
      for (int t = 0; t < MAX_TRACKS; t++)
        if (tflag[t]) push_jump_marker(&mtrk[t], dtime[t], kind == '[', e->data1);
      return;
    }
    if (e->inst >= MAX_INSTS) return;
    if (e->track >= MAX_TRACKS) return;
    if (!tflag[e->track]) return;

    push_midi_event(&mtrk[e->track], e, dtime[e->track]);
  },
  [&]() {
    for (int i = 0; i < MAX_TRACKS; i++) dtime[i]++;
  });

  // Write each used track to its own file.
  int exported = 0;
  for (int t = 0; t < MAX_TRACKS; t++) {

    if (!tflag[t]) continue;

    // End-of-track
    // Delta dtime[], NOT 0: dtime is the ticks since this track's last event, so a
    // 0 here ends the MIDI track on its final note and throws away every tick of
    // trailing silence -- the rest of the last pattern, and any silent pattern
    // after it. A reader that sizes the song from its events then loops early.
    push_varlen(&mtrk[t], dtime[t]);
    mtrk[t].pushc(0xFF);
    mtrk[t].pushc(0x2f);
    mtrk[t].pushc(0x00);

    char outname[600];
    snprintf(outname, sizeof(outname), "%s_track%02d.mid", base, t + 1);

    FILE *fp = fopen(outname, "wb");
    if (!fp) continue;

    CDataBuf file;
    file.write("MThd", 4);
    push_le_int(&file, 6);
    file.pushc(0); file.pushc(0);   // format 0
    file.pushc(0); file.pushc(1);   // 1 track
    file.pushc(0); file.pushc(96);  // 96 PPQN

    file.write("MTrk", 4);
    push_le_int(&file, mtrk[t].getsize());
    file.write(mtrk[t].getbuffer(), mtrk[t].getsize());

    fwrite(file.getbuffer(), file.getsize(), 1, fp);
    fclose(fp);

    exported++;
  }

  return exported;
}





/*

// <Manu> BACKUP DE LA FUNCION DE ARRIBA [EN: backup of the function above]






int ZTImportExport::ExportMID(char *fn, int format) {
    CDataBuf buffer, mtrk[MAX_INSTS+1], *mp;
    midi_buf *buf;
    int bpm, i, rstatus[MAX_INSTS+1], stat;
    FILE *fp;
    midi_event *e;
    int dtime[MAX_INSTS+1], total_mtrks=1, trk, chn;
    int mtrk_imap[MAX_INSTS];
    unsigned char iflag[MAX_INSTS];
    char* midiOutName;

    for(i=0;i<MAX_INSTS;i++) {
        mtrk_imap[i] = 0;
        iflag[i]=0;
        rstatus[i] = 0xFF;
        dtime[i] = 0;
    }

    find_insts( iflag, song);
    char str[1024];
    for (i=0;i<MAX_INSTS;i++) {
        if (iflag[i]) {
            if (!mtrk_imap[i+1]) {
                mtrk_imap[i+1] = total_mtrks;

                // crash fix - lipid
                midiOutName = song->instruments[i]->midi_route.named() ?
                    song->instruments[i]->midi_route.name : (char *)"Unavailable MIDI output";
                // song->instruments[i]->midi_device will be set to 255 if it is not set
                // to a valid midi device.
                if (song->instruments[i]->midi_device < MidiOut->numOuputDevices) {
                    midiOutName = (MidiOut->outputDevices[song->instruments[i]->midi_device]->alias != NULL &&
                        strlen(MidiOut->outputDevices[song->instruments[i]->midi_device]->alias)>1) ?
                        MidiOut->outputDevices[song->instruments[i]->midi_device]->alias :
                        MidiOut->outputDevices[song->instruments[i]->midi_device]->szName;
                }

                sprintf(str,"%s (%s)",song->instruments[i]->title,midiOutName);
                push_text( &mtrk[total_mtrks],
                           MID_SEQNAME,
                           (char *) &str[0]
                           
                         );
                mtrk[total_mtrks].pushc(0); mtrk[total_mtrks].pushc(0xFF); 
                mtrk[total_mtrks].pushuc(0x21); mtrk[total_mtrks].pushuc(0x1);
                mtrk[total_mtrks].pushuc(song->instruments[i]->midi_device);

                total_mtrks++;
            }
        }
    }

    if (!total_mtrks) return 0;

    if (! (fp = fopen(fn, "wb") ) ) return 0;

    buffer.write("MThd",4);
    push_le_int(&buffer, 6);

    buffer.pushc(0); buffer.pushc(1); // MIDI Format 1
    buffer.pushc(0); buffer.pushc(total_mtrks); // Number of MTrks (1 per midi device)
    buffer.pushc(0); buffer.pushc(96); // 96 PPQN

// Put good stuff into the first MTrk

    mtrk[0].pushuc(0);  mtrk[0].pushuc(0xFF);
    mtrk[0].pushuc(3);  mtrk[0].pushuc(0);

    push_text(&mtrk[0], 
        MID_SEQNAME, 
        (char *)song->title
        );

    char f[256];
    strcpy(&f[0], "Original filename: ");
    strcat((char *)f, (const char *)song->filename);
    push_text(&mtrk[0], 
        MID_SEQNAME, 
        (char *)&f[0]
        );

    push_text(&mtrk[0], 
              MID_COPYWRITE, 
              "Sequenced/exported with ztracker - http://ztracker.sourceforge.net/"
             );
    push_varlen(&mtrk[0], 0);
    mtrk[0].pushc(0xFF); mtrk[0].pushc(0x51); mtrk[0].pushc(0x03);  // Set tempo
    bpm = 60000000/song->bpm;
    mtrk[0].pushc( (bpm>>16) &0xff );
    mtrk[0].pushc( (bpm>>8) &0xff );
    mtrk[0].pushc( (bpm) &0xff );
// Enough of that tomfoolery

    ztPlayer->prepare_play(0,0,1,0); // start row 1, playmode: song, loop: off
    buf = new midi_buf;

    while(ztPlayer->pinit) {
        buf->reset();
        ztPlayer->playback(buf,1);
         
//        buf->insert(0, ET_LOOP, 1, 0 , 0, 0, 0, 0, 0) ;

        while(e = buf->get_next_event()) {
            if (e->inst < MAX_INSTS) {
                trk = mtrk_imap[e->inst+1];
                mp = &mtrk[trk];
                chn = e->device;
                switch(e->type) {
                    case ET_NOTE_OFF:   
                        e->data2 = 0x0;
                    case ET_NOTE_ON:
                        push_varlen(mp, dtime[trk]);
                        dtime[trk] = 0;
                        stat = (e->data1&0x0F) + 0x90;
//                      if (rstatus[chn] != stat) {
                            mp->pushuc( stat ); // Note on
//                          rstatus[chn] = stat;
//                      }
                        mp->pushuc( e->command );             // Note number
                        mp->pushuc( (unsigned char)e->data2 );
                        break;
                    case ET_PC: 
                        if (e->data2 >= 0) {
                            unsigned short int bank;
                            unsigned char hb,lb;
                            bank = e->data2;
                            bank &= 0x3fff;
                            lb = bank&0x007F;
                            hb = bank>>7;
                            push_varlen(mp, dtime[trk]);
                            dtime[trk] = 0;
                            mp->pushuc(0xB0 + e->data2);
                            mp->pushuc(0x0);
                            mp->pushuc(lb);
                            push_varlen(mp, dtime[trk]);
                            mp->pushuc(0xB0 + e->data2);
                            mp->pushuc(0x20);
                            mp->pushuc(hb);
                        }
                        push_varlen(mp, dtime[trk]);
                        dtime[trk] = 0;
                        mp->pushuc(0xC0 + e->data2);
                        mp->pushuc(e->command);
                        break;
                    case ET_CC:
                        push_varlen(mp, dtime[trk]);
                        dtime[trk] = 0;
                        mp->pushuc(0xB0 + e->data2);
                        mp->pushuc(e->command);
                        mp->pushuc((unsigned char)e->data1);
                        break;
                    case ET_PITCH:
                      { // <Manu> Añado llaves para que compile al poner el case de abajo ¿? [EN: adding braces so it compiles when the case below is added]
                        unsigned short int usi = (unsigned short int)e->data1<<8; usi+=e->data2;
                        unsigned char d1,d2;
                        usi &= 0x3FFF;
                        d1 = (usi & 0x007F);
                        d2 = usi >> 7;
                        push_varlen(mp, dtime[trk]);
                        dtime[trk] = 0;
                        mp->pushuc(0xe0 + e->command);
                        mp->pushuc(d1);
                        mp->pushuc(d2);
                      } // <Manu> Cierro la llave de arriba [EN: closing the brace from above]
                        break;

                    case ET_LOOP: // <Manu> Exportamos el evento de loop

                      // Si entra aqui es un milagro 

//                      {
//                        mp->pushuc(0x06) ;
//                        mp->pushuc(0x03) ;

//                        if(e->data1==1) mp->pushuc('[') ;
//                        else mp->pushuc(']') ;
//                      }
                      


                      
                        push_varlen(mp, dtime[trk]);
                        dtime[trk] = 0;

                        if(e->data1==1) {
                          
                          push_text(&mtrk[0],  MID_MARKER,  "[" ) ;
                        }
                        else {
                          
                          push_text(&mtrk[0],  MID_MARKER,  "]" ) ;
                        }



                      break ;
                }

            }
        }
        for (trk=0;trk<MAX_INSTS+1;trk++)
            dtime[trk]++;
    }

    delete buf;

// END MTrk         
    for (i=0;i<total_mtrks;i++) {
    // Delta dtime[], NOT 0: dtime is the ticks since this track's last event, so a
    // 0 here ends the MIDI track on its final note and throws away every tick of
    // trailing silence -- the rest of the last pattern, and any silent pattern
    // after it. A reader that sizes the song from its events then loops early.
        push_varlen(&mtrk[i], dtime[i]);
        mtrk[i].pushc(0xFF); mtrk[i].pushc(0x2f); mtrk[i].pushc(0x00);  // End of track
        buffer.write("MTrk",4);
        push_le_int(&buffer, mtrk[i].getsize());
        buffer.write(mtrk[i].getbuffer(),mtrk[i].getsize());
    }

    fwrite(buffer.getbuffer(),buffer.getsize(),1,fp); // blast the buffer to the file

    fclose(fp);
    return 1;
}





  
*/



// ------------------------------------------------------------------------------------------------
//
//
// ------------------------------------------------------------------------------------------------
// IT patterns can be 1..200 rows, zTracker's are 32..999. A short pattern is
// padded to 32 rows with a pattern break on its last real row, so it still
// plays for its original length.
//
static void end_pattern_at_row(pattern *p, int row)
{
  for (int t = 0; t < MAX_TRACKS; t++) {
    event *e = p->tracks[t]->get_event(row);
    if (e && e->effect == 'C') return;
  }
  for (int t = 0; t < MAX_TRACKS; t++) {
    event *e = p->tracks[t]->get_event(row);
    if (!e || e->effect == 0xFF) {
      p->tracks[t]->update_event_safe(row, -1, -1, -1, -1, 'C', 0);
      return;
    }
  }
}

int ZTImportExport::ImportIT(const char *fn, zt_module* zt)
{
  int note, ins, volpan, cmnd, cmndvalue, deflength ;
  int i ;

  sprintf(szStatmsg,"Loading IT %s",fn);
  statusmsg = szStatmsg;
  status_change = 1;
  
  ITModule mod;
  
  if (!mod.LoadModule(fn, UNPACK_PATTERNS)) {
    return 0;
  }
  
  SDL_Delay(10);
  zt->de_init();
  SDL_Delay(10);
  zt->init();
  
  //  int e = strlen(fn)-1;
  //  while(fn[e]!='\\' && e>=0) e--;
  //  if (fn[e]=='\\') e++;
  strcpy((char *)zt->filename,&fn[0]);
  strcat((char *)zt->filename,".zt");
  
  // import pattern sequences
  for (i = 0; i < mod.patterns->numPatterns; i++) {
    
    const int rows = mod.Patterns(i).numRows;
    zt->patterns[i]->resize(rows < ZTM_PATTERN_MIN_LEN ? ZTM_PATTERN_MIN_LEN : rows);
    
    for (int row = 0; row < rows; row++) {
      for (int chan = 0; chan < mod.Patterns(i).numChannels; chan++) {
  
        note = mod.Patterns(i).Note(chan, row) ;
        if (note == 0) note = -1;

        ins = mod.Patterns(i).Instrument(chan, row) ;
        volpan = mod.Patterns(i).VolPan(chan, row) ;
        cmnd = mod.Patterns(i).Command(chan, row) ;
        cmndvalue = mod.Patterns(i).CommandValue(chan, row) ;
        
        if (ins == 0) ins = -1;
        else --ins ;           // cm 24-07-2001 - Fixed bug #430645    // cm 24-07-2001 - Fixed bug #430645

        // only process command C

        if (cmnd == 3) cmnd = 'C';
        else {
          cmnd = -1;
          cmndvalue = -1;
        }

        deflength = -1 ;

        if (note==0xFF) note = 0x81;
        if (note==0xFE) note = 0x82;
        zt->patterns[i]->tracks[chan]->update_event_safe(row, note, ins, volpan, deflength, cmnd, cmndvalue);               
      }
    }

    if (rows > 0 && rows < ZTM_PATTERN_MIN_LEN) end_pattern_at_row(zt->patterns[i], rows - 1);
  }
  
  // set order list

  for (i = 0; i < mod.header.numOrders; i++) {

    if (i < 255) zt->orderlist[i] = mod.header.orders[i];
  }
  
  // import instruments

  if (mod.header.numIns > 0) {

    for (int i = 0; i < mod.header.numIns; i++) {
      
      const int bank = mod.Instruments(i).midiBank;
      zt->instruments[i]->bank = static_cast<signed short>((bank <= 0x3FFF) ? bank : -1);
      
      const int channel = static_cast<int>(mod.Instruments(i).midiChannel) - 1;
      zt->instruments[i]->channel = static_cast<unsigned char>((channel >= 0 && channel <= 0xF) ? channel : 0);
      
      const int patch = mod.Instruments(i).midiProgram;
      zt->instruments[i]->patch = static_cast<signed char>((patch <= 0x7F) ? patch : -1);
      
      //strcpy((char *)&zt->instruments[i]->title[0], mod.Instruments(i).name);
      memcpy(&zt->instruments[i]->title[0], mod.Instruments(i).name, 24);
      
      for (int z=0;z<23;z++) {
        
        if (zt->instruments[i]->title[z] == 0) zt->instruments[i]->title[z] = ' ' ;
      }
        
      zt->instruments[i]->title[24]=0;
      
      // this->instruments[i]->default_length
      
      zt->instruments[i]->default_volume = mod.Instruments(i).globalVolume;
      
      if (zt->instruments[i]->default_volume>0x7F) zt->instruments[i]->default_volume = 0x7F;
      
      zt->instruments[i]->flags |= INSTFLAGS_TRACKERMODE;
    }
  } 
  else {

    // import samples
    
    for (int i = 0; i < mod.header.numSamples; i++) {
      
      memcpy(&zt->instruments[i]->title[0], mod.Samples(i).name, 24);
      
      zt->instruments[i]->default_volume = mod.Samples(i).defaultVolume*2;
      
      if (zt->instruments[i]->default_volume>0x7f) zt->instruments[i]->default_volume=0x7f;
      
      zt->instruments[i]->flags |= INSTFLAGS_TRACKERMODE;
    }
  }
  
  
  // set misc
  //ztPlayer->tpb = zt->tpb = mod.header.initialSpeed;

  ztPlayer->bpm = zt->bpm = mod.header.initialTempo;
  ztPlayer->set_speed();
  strcpy((char *)&zt->title[0], mod.header.songName);
  
  //todo:
  // calculate default length and use c5speed
  // scale volumes
  load_lock = 0;
  
  return(1) ;
}

