/*************************************************************************
 *
 * FILE  midi-io.cpp
 *
 * DESCRIPTION
 *   Midi queue and midi I/O functions.
 *   Ported from Win32 midiIn/midiOut to RtMidi for cross-platform support.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2000-2001, Christopher Micali
 * Copyright (c) 2001, Daniel Kahlin
 * All rights reserved. (BSD-3-Clause -- see original header)
 *
 ******/
#include "zt.h"
#include "RtMidi.h"
#include <cstring>


extern void InitializePlugins(midiOut *mout);

midiOut::midiOut() {
    numOuputDevices = 0;
    InitializePlugins(this);
    devlist_head = NULL;
}

midiOut::~midiOut() {
    intlist *t = devlist_head;
    while(t) {
        intlist *tm = t->next;
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
    outputDevices[dev]->delay_ticks = ticks;
}
int midiOut::get_bank_select(int dev) {
    if ((unsigned int)dev>=numOuputDevices) return 0;
    if (outputDevices[dev]->type == OUTPUTDEVICE_TYPE_MIDI) {
        MidiOutputDevice *mod = (MidiOutputDevice *)outputDevices[dev];
        return mod->reverse_bank_select;
    }
    return 0;
}
void midiOut::set_bank_select(int dev, int bank_select) {
    if ((unsigned int)dev>=numOuputDevices) return;
    if (outputDevices[dev]->type == OUTPUTDEVICE_TYPE_MIDI) {
        MidiOutputDevice *mod = (MidiOutputDevice *)outputDevices[dev];
        mod->reverse_bank_select = bank_select;
    }
}

const char *midiOut::get_alias(int dev) {
    if ((unsigned int)dev>=numOuputDevices) return NULL;
    return (outputDevices[dev]->alias != NULL && strlen(outputDevices[dev]->alias))?outputDevices[dev]->alias:"";
}
void midiOut::set_alias(int dev, char *n) {
    if ((unsigned int)dev>=numOuputDevices) return;
    if(n != NULL && strlen(n) < 1023)
         strcpy(outputDevices[dev]->alias,n);
}

unsigned int midiOut::AddDevice(int dev) {
    intlist *t;
    unsigned int error;
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


// ------------------------------------------------------------------------------------------------
// MIDI input queue (platform-independent)
// ------------------------------------------------------------------------------------------------

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

int mim_moredata = 0;
int mim_error = 0;
int mim_longerror = 0;


// ------------------------------------------------------------------------------------------------
// RtMidi input callback — replaces the Win32 midiInCallback
// ------------------------------------------------------------------------------------------------

static void rtMidiInCallback(double /*deltatime*/, std::vector<unsigned char> *message, void * /*userData*/)
{
    if (!message || message->size() == 0) return;

    unsigned char msg = (*message)[0];
    unsigned int dwParam1 = 0;

    // Pack into the same 32-bit format the rest of zTracker expects:
    // byte0 = status, byte1 = data1, byte2 = data2
    dwParam1 = msg;
    if (message->size() > 1) dwParam1 |= ((*message)[1] << 8);
    if (message->size() > 2) dwParam1 |= ((*message)[2] << 16);

    unsigned char status = msg & 0xF0;

    switch(status) {
        case 0x80: // Note off
            midiInQueue.insert(dwParam1);
            break;
        case 0x90: // Note on
            if ( ((dwParam1&0xFF0000)>>16) == 0x0) {
                // velocity 0 → note off
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

    // MIDI sync messages
    if (zt_config_globals.midi_in_sync) {
        static int queued_row = 0, queued_order = 0;

        switch(msg) {
            case 0xF8: // MIDI CLOCK
                g_midi_in_clocks_received++;
                break;
            case 0xF2: { // MIDI SONG POSITION POINTER
                int song_pos_ptr = (dwParam1&0x7F0000)>>9 |
                                   (dwParam1&0x7F00)>>8;
                if (ztPlayer->calc_pos(song_pos_ptr,&queued_row,&queued_order)) {
                    queued_row=0;
                    queued_order=0;
                }
                break;
            }
            case 0xFA: // MIDI START
                queued_row=0;
                queued_order=0;
                /* fall thru */
            case 0xFB: // MIDI CONTINUE
                if (ztPlayer->playing)
                    ztPlayer->stop();
                g_midi_in_clocks_received = 0;
                if (queued_row || queued_order) {
                    ztPlayer->play(queued_row,queued_order,3);
                } else {
                    if (song->orderlist[0] < 0x100) {
                        ztPlayer->play(0,cur_edit_pattern,1);
                    } else {
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
    }
}


// ------------------------------------------------------------------------------------------------
// intlist
// ------------------------------------------------------------------------------------------------

intlist::intlist(int k, intlist *n) {
    key = k;
    next = n;
}

intlist::~intlist() {
}


// ------------------------------------------------------------------------------------------------
// midiInDevice — backed by RtMidiIn
// ------------------------------------------------------------------------------------------------

midiInDevice::midiInDevice(int i) {
    devNum = i;
    szName = NULL;
    rtMidiIn = nullptr;
    opened = 0;

    try {
        RtMidiIn probe;
        if ((unsigned)i < probe.getPortCount()) {
            std::string name = probe.getPortName(i);
            szName = strdup(name.c_str());
        }
    } catch (...) {}
}

midiInDevice::~midiInDevice(void) {
    this->close();
    if (szName) free(szName);
}

int midiInDevice::open(void) {
    if (opened)
        if (close())
            return -1;
    try {
        rtMidiIn = new RtMidiIn();
        rtMidiIn->openPort(devNum);
        rtMidiIn->setCallback(rtMidiInCallback, nullptr);
        // Don't ignore sysex, timing, or active sensing for now
        rtMidiIn->ignoreTypes(true, false, true);
        opened = 1;
        return 0;
    } catch (...) {
        delete rtMidiIn;
        rtMidiIn = nullptr;
        return -1;
    }
}

int midiInDevice::close(void) {
    if (opened) {
        try {
            if (rtMidiIn) {
                rtMidiIn->cancelCallback();
                rtMidiIn->closePort();
                delete rtMidiIn;
                rtMidiIn = nullptr;
            }
            opened = 0;
            return 0;
        } catch (...) {
            return -1;
        }
    }
    return -1;
}

int midiInDevice::reset(void) {
    // RtMidi doesn't have a direct reset equivalent.
    // Close and reopen achieves the same effect.
    if (opened) {
        int dev = devNum;
        close();
        return open();
    }
    return 0;
}


// ------------------------------------------------------------------------------------------------
// midiIn — manages midiInDevice instances, backed by RtMidi port enumeration
// ------------------------------------------------------------------------------------------------

midiIn::midiIn() {
    unsigned int i;
    try {
        RtMidiIn probe;
        numMidiDevs = probe.getPortCount();
    } catch (...) {
        numMidiDevs = 0;
    }
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
        t = tm;
    }
    for (unsigned int i=0;i<numMidiDevs;i++)
        delete midiInDev[i];
}

unsigned int midiIn::AddDevice(int dev) {
    intlist *t;
    unsigned int error;
    error = midiInDev[dev]->open();
    if (error != 0)
        return error;
    t = devlist_head;

    if(devlist_head != NULL) {
        delete(devlist_head);
    }

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

midiInDevice *midiIn::GetDeviceByIndex(int idx) {
    if (idx >= 0 && (unsigned)idx < numMidiDevs)
        return midiInDev[idx];
    return NULL;
}

/* eof */
