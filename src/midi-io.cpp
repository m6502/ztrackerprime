/*************************************************************************
 *
 * FILE  midi-io.cpp
 * $Id: midi-io.cpp,v 1.19 2002/04/15 20:48:54 zonaj Exp $
 *
 * DESCRIPTION 
 *   Midi queue and midi I/O functions.
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

/*

midiOutDevice::midiOutDevice(int i) {
    devNum = i; szName = 0; handle = NULL; opened = 0;
    if (!midiOutGetDevCaps(i, &caps, sizeof(MIDIOUTCAPS)))
        szName = caps.szPname;
    this->clear_notestates();
//    delay_ms = delay_ticks = 30;
 //   delay_ms = 500;
    delay_ticks = 0;
}
midiOutDevice::~midiOutDevice(void) {
    close();
}
int midiOutDevice::open(void) {
    int error;
    if (opened)
        if (close())
            return -1;
    if (!(error = midiOutOpen(&handle, devNum, 0, 0, CALLBACK_NULL))) {
        opened=1;
        return 0;
    }
    return error;
}
int midiOutDevice::close(void) {
    if (opened)
        if (!midiOutClose(handle)) {
            handle = NULL; opened = 0;
            return 0;
        } 
    return -1;
}

void midiOutDevice::clear_notestates(void) {
    for(int i=0;i<128;i++)
        for (int j=0;j<16;j++)
            this->notestates[i][j] = NB_OFF;    
}

void midiOutDevice::reset(void) {
    if (this->opened) {
        for(int i=0;i<128;i++)
            for (int j=0;j<16;j++)
                if (this->notestates[i][j] & NB_ON)
                    midiOutShortMsg(this->handle,(0x80+(j)) + ((i)<<8));
    }               
    for (int i=0;i<16;i++) {
        
        //midiOutShortMsg(this->handle,(0xB0+i) + (0x07<<8) + (0x7F<<16)); // Reset chan volume
        midiOutShortMsg(this->handle,(0xE0+i) + (0x40<<16)); // reset pitchwheel
        midiOutShortMsg(this->handle,(0xB0+i) + (0x79<<8) + (0<<16));     // Reset ctrl
        midiOutShortMsg(this->handle,(0xB0+i) + (0x40<<8) + (0<<16));     // Sustain
        midiOutShortMsg(this->handle,(0xB0+i) + (0x01<<8) + (0<<16));     // Modulation
    }
    this->clear_notestates();
}
*/
extern void InitializePlugins(midiOut *mout);

midiOut::midiOut() {
    numOuputDevices = 0;
    InitializePlugins(this);
    devlist_head = NULL;
}

midiOut::~midiOut() {
    intlist *t=devlist_head,*tm;
    while(t) {
        tm = t->next;
        outputDevices[t->key]->close();
        delete t;
        t = tm;
    }
    for (unsigned int i=0;i<numOuputDevices;i++)
        delete outputDevices[i];
}
int midiOut::get_delay_ticks(int dev) {
    if ((unsigned int)dev>=numOuputDevices) return 0;
    return outputDevices[dev]->delay_ticks;
}
void midiOut::set_delay_ticks(int dev, int ticks) {
    if ((unsigned int)dev>=numOuputDevices) return;
    outputDevices[dev]->delay_ticks = ticks;;
}
int midiOut::get_bank_select(int dev) {
    if ((unsigned int)dev>=numOuputDevices) return 0;
    if (outputDevices[dev]->type == OUTPUTDEVICE_TYPE_MIDI) {
        MidiOutputDevice *mod = (MidiOutputDevice *)outputDevices[dev];
        return mod->reverse_bank_select ;
    }
    else
        return 0;
}
void midiOut::set_bank_select(int dev, int bank_select) {
    if ((unsigned int)dev>=numOuputDevices) return;
    if (outputDevices[dev]->type == OUTPUTDEVICE_TYPE_MIDI) {
        MidiOutputDevice *mod = (MidiOutputDevice *)outputDevices[dev];
        mod->reverse_bank_select = bank_select;
    }
}

char *midiOut::get_alias(int dev) {
    if ((unsigned int)dev>=numOuputDevices) return NULL;
    return (outputDevices[dev]->alias != NULL && strlen(outputDevices[dev]->alias))?outputDevices[dev]->alias:"";
}
void midiOut::set_alias(int dev, char *n) {
    if ((unsigned int)dev>=numOuputDevices) return;
    if(n != NULL && strlen(n) < 1023)
         strcpy(outputDevices[dev]->alias,n);
}

UINT midiOut::AddDevice(int dev) {
    intlist *t;
    UINT error;
    error = outputDevices[dev]->open();
    if (error)
        return error;
    t = devlist_head;
    devlist_head = new intlist(dev,t);
    return 0;
}

int midiOut::RemDevice(int dev) {
    intlist *t = devlist_head;
    intlist *prev = devlist_head;
    while(t) {
        if (t->key == dev) {
            outputDevices[dev]->close();
            if (t == devlist_head) {
                devlist_head = t->next;
            } else {
                prev->next = t->next;
            }
            delete t;
            return 0;
        }
        prev = t;
        t = t->next;
    }
    return -1;
}

int midiOut::QueryDevice(int dev) {
    intlist *t = devlist_head;
    while(t) {
        if (t->key == dev)
            return 1;
        t = t->next;
    }
    return 0;
}

int midiOut::GetNumOpenDevs(void) {
    intlist *t = devlist_head;
    int c=0;
    while(t) {
        c++;
        t = t->next;
    }
    return c;
}
int midiOut::GetDevID(int which) {
    intlist *t = devlist_head;
    int c=0;
    while(t) {
        if (c==which)
            return t->key;
        c++;
        t = t->next;
    }
    return -1;
}


















miq midiInQueue;;

miq::miq() { 
    qsize = 1024;
    q = new unsigned int[qsize];
    qhead = qtail = 0;
    qelems = 0;
}

miq::~miq() {
    delete[] q;
}

void miq::insert(unsigned int msg) {
    if (qelems<qsize) {
        if (qhead>=qsize)
            qhead = 0;
        q[qhead] = msg;
        qhead++;
        qelems++;
    }
}

unsigned int miq::pop(void) {
    unsigned int a=0;
    if (qhead!=qtail) {
        a = q[qtail];
        qtail++;
        if (qtail>=qsize)
            qtail=0;
        qelems--;
    }
    return a;
}

unsigned int miq::check(void) {
    unsigned int a=0;
    if (qhead!=qtail) {
        a = q[qtail];
    }
    return a;
}

int miq::size(void) {
    return qelems;
}

void miq::clear(void) {
    qhead = qtail = 0;
    qelems = 0;
}
int g_midi_in_clocks_received = 0;

void CALLBACK midiInCallback(HMIDIIN handle, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {

    unsigned char msg;
    int song_pos_ptr;
    static int queued_row, queued_order;
//	LPMIDIHDR		lpMIDIHeader;
//	unsigned char *	ptr;

    midiInDevice *dev = MidiIn->GetDevice( handle );
    
    switch ( uMsg ) {

        case MIM_OPEN:
            SDL_Delay(0);
            break;
        case MIM_CLOSE:
            break;
/*
		case MIM_LONGDATA:

			lpMIDIHeader = (LPMIDIHDR)dwParam1;
            if (dev) { 
			    ptr = (unsigned char *)(lpMIDIHeader->lpData);
			    if (!dev->SysXFlag)
			    {
				    dev->SysXFlag = 1;
			    }

			    if (*(ptr + (lpMIDIHeader->dwBytesRecorded - 1)) == 0xF7)
			    {
				    dev->SysXFlag = 0;
			    }
            }
			midiInAddBuffer(handle, lpMIDIHeader, sizeof(MIDIHDR));

			break;
*/        
        
        
        
        case MIM_DATA:
          msg = (unsigned char)(dwParam1 & 0x000000FF);

          switch(msg) {
              case 0x80: // Note off
                  midiInQueue.insert(dwParam1);
                  break;
              case 0x90: // Note on
                  //midiInQueue.insert(((dwParam1&0xFF00)>>8));
                  if ( ((dwParam1&0xFF0000)>>16) == 0x0) {
                      dwParam1 = 0x80 + (dwParam1&0xFFFF00);
                  }
                  midiInQueue.insert(dwParam1);
                  break;
              default:
                  if (msg < 0xF0) {
                      midiInQueue.insert(dwParam1);
                  }
                  break;
          }

          if (zt_config_globals.midi_in_sync)
              switch(msg) {
                case 0xF8: // MIDI CLOCK
                    g_midi_in_clocks_received++;
/*                  sprintf(szStatmsg, "Midi CLOCK");
                    statusmsg = szStatmsg;
                    status_change = 1;
                    need_refresh++; 
*/                  break;

                case 0xF2: // MIDI SONG POSITION POINTER
                    song_pos_ptr = (dwParam1&0x7F0000)>>9 |
                                   (dwParam1&0x7F00)>>8;
                    if (ztPlayer->calc_pos(song_pos_ptr,&queued_row,&queued_order)) {
                        queued_row=0;
                        queued_order=0;
                    }
#if 0
                    sprintf(szStatmsg, "SONG POS 0x%04x --> row %d order %d",song_pos_ptr,queued_row,queued_order);
                    statusmsg = szStatmsg;
                    status_change = 1;
                    need_refresh++; 
#endif
                    break;
                case 0xFA: // MIDI START
                    queued_row=0;
                    queued_order=0;
                    /* fall thru */
                case 0xFB: // MIDI CONTINUE
                    if (ztPlayer->playing)
                        ztPlayer->stop();
                    g_midi_in_clocks_received = 0;
                    if (queued_row || queued_order) {
                        // we have a queued row and order, play from that position.
                        ztPlayer->play(queued_row,queued_order,3);
                    } else {
                        if (song->orderlist[0] < 0x100) {
                            // if there are patterns in the orderlist,
                            // play song from the start.
                            ztPlayer->play(0,cur_edit_pattern,1); 
                        } else {
                            // otherwise loop the current pattern
                            ztPlayer->play(0,cur_edit_pattern,0);
                        }
                    }
                    break;
                case 0xFC: // MIDI STOP
                    g_midi_in_clocks_received=0;
                    ztPlayer->stop();
                    break;
                default:
                    break;
              }

          break;
        case MIM_MOREDATA:
            break;
        case MIM_ERROR:
            break;
        case MIM_LONGERROR:
            break;
        default:
            break;

    }
}


intlist::intlist(int k, intlist *n) {
    key = k;
    next = n;
}

intlist::~intlist() {
}

midiInDevice::midiInDevice(int i) {
    devNum = i;
    szName = NULL;
    handle = NULL;
    opened = 0;
    if (!midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)))
        szName = caps.szPname;
}

midiInDevice::~midiInDevice(void) {
    this->close();
}

int midiInDevice::open(void) {
    int error;
    if (opened)
        if (close())
            return -1;
    if (!(error = midiInOpen(&handle, devNum, (DWORD)midiInCallback, devNum, CALLBACK_FUNCTION))) {
        opened=1;
        midiHdr.lpData = (LPSTR)&SysXBuffer[0];
        midiHdr.dwBufferLength = sizeof(SysXBuffer);
        midiHdr.dwFlags = 0;
        midiInPrepareHeader(handle, &midiHdr, sizeof(MIDIHDR));
        midiInAddBuffer(handle, &midiHdr, sizeof(MIDIHDR));
        return midiInStart(handle);
    }
    return error;
}

int midiInDevice::close(void) {
//    char buffer[30000];
    unsigned long err;
    if (opened) {
        //midiInStop(handle);
        midiInReset(handle);
        midiInUnprepareHeader(handle, &midiHdr, sizeof(MIDIHDR));
        err = midiInClose(handle);
        //err = midiInGetErrorText(err, &buffer[0], 30000);
        handle = NULL; 
        opened = 0;
        return 0;
         
    }
/*  if (!(err = midiInGetErrorText(err, &buffer[0], 30000)))
    {
        printf("%s\r\n", &buffer[0]);
    }
*/  return -1;
}

MMRESULT midiInDevice::reset(void) {
    return midiInReset(this->handle);
}




midiIn::midiIn() {
    unsigned int i;
    numMidiDevs = midiInGetNumDevs();
    if (!(numMidiDevs<MAX_MIDI_INS)) 
        return;
    for (i=0;i<numMidiDevs;i++) {
        midiInDev[i] = new midiInDevice(i);
    }
    devlist_head = NULL;
}

midiIn::~midiIn() {
    intlist *t=devlist_head,*tm;
    while(t) {
        tm = t->next;
        midiInDev[t->key]->close();
   //     delete t;
        t = tm;
    }
    for (unsigned int i=0;i<numMidiDevs;i++)
        delete midiInDev[i];
}

UINT midiIn::AddDevice(int dev) {
    intlist *t;
    UINT error;
    error = midiInDev[dev]->open();
    if (error != MMSYSERR_NOERROR)
        return error;
    t = devlist_head;
    devlist_head = new intlist(dev,t);
    return 0;
}

int midiIn::RemDevice(int dev) {
    intlist *t = devlist_head;
    intlist *prev = devlist_head;
    while(t) {
        if (t->key == dev) {
            midiInDev[dev]->close();
            if (t == devlist_head) {
                devlist_head = t->next;
            } else {
                prev->next = t->next;
            }
            delete t;
            return 0;
        }
        prev = t;
        t = t->next;
    }
    return -1;
}

int midiIn::QueryDevice(int dev) {
    intlist *t = devlist_head;
    while(t) {
        if (t->key == dev)
            return 1;
        t = t->next;
    }
    return 0;
}

int midiIn::GetNumOpenDevs(void) {
    intlist *t = devlist_head;
    int c=0;
    while(t) {
        c++;
        t = t->next;
    }
    return c;
}
int midiIn::GetDevID(int which) {
    intlist *t = devlist_head;
    int c=0;
    while(t) {
        if (c==which)
            return t->key;
        c++;
        t = t->next;
    }
    return -1;
}

midiInDevice *midiIn::GetDevice(HMIDIIN hmi) {
    intlist *t = devlist_head;
    unsigned int c = 0;
    while(t) {
        c = t->key;
        if ( c < this->numMidiDevs && this->midiInDev[c]->handle == hmi )
            return this->midiInDev[c];
        c++;
        t = t->next;
    }
    return NULL;
}    


/* eof */
