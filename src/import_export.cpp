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

#include "import_export.h"


ZTImportExport::ZTImportExport() {
}

ZTImportExport::~ZTImportExport() {
}


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
  
  while(TRUE) {

    buf->pushc((unsigned char)(buffer&0xFF)) ;
    
    if (buffer & 0x80) buffer >>= 8;
    else break;
  }
}




// ------------------------------------------------------------------------------------------------
// <Manu> Creo esta funcion
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
//
//
int ZTImportExport::ExportMID(char *fn, int format) 
{
  CDataBuf buffer, mtrk[MAX_INSTS+1], *mp;
  midi_buf *buf;
  int bpm, i, rstatus[MAX_INSTS+1], stat;
  FILE *fp;
  midi_event *e;
  int dtime[MAX_INSTS+1], total_mtrks=1, trk, chn;
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

        if(song->instruments[i]->midi_device < MAX_MIDI_OUTS) {
        
          if((MidiOut->outputDevices[song->instruments[i]->midi_device]->alias != NULL &&
             strlen(MidiOut->outputDevices[song->instruments[i]->midi_device]->alias)>1)) {
            
            midiOutName=MidiOut->outputDevices[song->instruments[i]->midi_device]->alias ;
          }
          else {
          
            midiOutName=MidiOut->outputDevices[song->instruments[i]->midi_device]->szName ;
          }
        }
        else midiOutName = NULL;

        
        sprintf(str,"%s (%s)",song->instruments[i]->title, midiOutName) ;
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

//  <Manu> Elimino estos eventos que no sirven para nada
  
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
  
  ztPlayer->prepare_play(0,0,1,0); // start row 1, playmode: song, loop: off
  buf = new midi_buf;
  
  while(ztPlayer->pinit) {
    
    buf->reset();
    ztPlayer->playback(buf,1);
    
    //        buf->insert(0, ET_LOOP, 1, 0 , 0, 0, 0, 0, 0) ;
    
    while(e = buf->get_next_event()) {
      

      if(0 && e->type == ET_LOOP) {     // <Manu> Por ahora lo dejo asi para evitar problemas
      
        // ¿Esto no puede estar abajo porque no tiene nota?
      
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          
//        case ET_LOOP: // <Manu> Exportamos el evento de loop ------------------
          
          push_varlen(&mtrk[0], dtime[0]) ;     // Delta time
          dtime[0] = 0 ;

          mtrk[0].pushuc(0xFF) ;                // Es un meta-evento
          mtrk[0].pushuc(0x06) ;                // Marker

          push_varlen(&mtrk[0], 1) ;            // Tiene un dato de 1 byte

          if(e->data1==0) {
            
            mtrk[0].pushuc('[') ; // Dato
          }
          else {
            
            mtrk[0].pushuc(']') ;
          }

//          break ; 
      }
      else {
          
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
          // ------------------------------------------------------------------
      
      if (e->inst < MAX_INSTS) {
        
        trk = mtrk_imap[e->inst+1];
        mp = &mtrk[trk];
        chn = e->device;
        
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
          
          // <Manu> Añado llaves para que compile al poner el case de abajo ¿?
          
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

    }
        
    for (trk=0;trk<MAX_INSTS+1;trk++) {
          
      dtime[trk]++ ;
    }
  }
    
  delete buf;
    
  // END MTrk         
  for (i=0;i<total_mtrks;i++) {
    
    push_varlen(&mtrk[i], 0) ;
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





/*

// <Manu> BACKUP DE LA FUNCION DE ARRIBA






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
                midiOutName = NULL;
                // song->instruments[i]->midi_device will be set to 255 if it is not set
                // to a valid midi device.
                if (song->instruments[i]->midi_device < MAX_MIDI_OUTS) {
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
                      { // <Manu> Añado llaves para que compile al poner el case de abajo ¿?
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
                      } // <Manu> Cierro la llave de arriba
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
        push_varlen(&mtrk[i], 0);
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
int ZTImportExport::ImportIT(char *fn, zt_module* zt) 
{
  int note, ins, volpan, cmnd, cmndvalue, deflength ;
  int i ;

  sprintf(szStatmsg,"Loading IT ",fn);
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
    
    zt->patterns[i]->resize(mod.Patterns(i).numRows);
    
    for (int row = 0; row < mod.Patterns(i).numRows; row++) {
      zt->patterns[i]->resize(mod.Patterns(i).numRows);
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
  }
  
  // set order list

  for (i = 0; i < mod.header.numOrders; i++) {

    if (i<0xFF) zt->orderlist[i] = mod.header.orders[i];
  }
  
  // import instruments

  if (mod.header.numIns > 0) {

    for (int i = 0; i < mod.header.numIns; i++) {
      
      zt->instruments[i]->bank = mod.Instruments(i).midiBank;
      if (zt->instruments[i]->bank <-1 || zt->instruments[i]->bank >0x3FFF) zt->instruments[i]->bank = -1;
      
      zt->instruments[i]->channel = mod.Instruments(i).midiChannel-1;
      if (zt->instruments[i]->channel > 0xF || zt->instruments[i]->channel<0) zt->instruments[i]->channel = 0x0;
      
      zt->instruments[i]->patch = mod.Instruments(i).midiProgram;
      if (zt->instruments[i]->patch <-1 || zt->instruments[i]->patch >0x7F) zt->instruments[i]->patch = -1;
      
      //strcpy((char *)&zt->instruments[i]->title[0], mod.Instruments(i).name);
      if (mod.Instruments(i).name) memcpy(&zt->instruments[i]->title[0], mod.Instruments(i).name,24);
      else memset(&zt->instruments[i]->title[0], 0, 24);
      
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
      
      if (mod.Samples(i).name) memcpy(&zt->instruments[i]->title[0], mod.Samples(i).name,24);
      else memset(&zt->instruments[i]->title[0], 0, 24);
      
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

