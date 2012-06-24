#ifndef _MIDI_DEVICE_H
#define _MIDI_DEVICE_H

#define NB_OFF    0x000000
#define NB_ON     0x010000
#define NB_MUTED  0x020000
#define NB_RETRIG 0x040000

#define MIDI_MSG( cmd, chan, data1, data2) ( (cmd+chan) + (data1<<8) + (data2<<16)) 

extern int g_midi_in_clocks_received;

class miq {
    public:
        miq();
        ~miq();
        void insert(unsigned int msg);
        unsigned int pop(void);
        unsigned int check(void);
        int size(void);
        void clear(void);
    private:
        unsigned int *q;
        int qsize, qhead, qtail, qelems;
};

extern miq midiInQueue;

class intlist {
    public:
    int key;
    intlist *next;
    intlist(int k, intlist *n);
    ~intlist();
};

#define OUTPUTDEVICE_TYPE_NONE     0
#define OUTPUTDEVICE_TYPE_AUDIO    1
#define OUTPUTDEVICE_TYPE_MIDI     2
#define OUTPUTDEVICE_TYPE_OTHER    3

#define MAX_DEVICE_NAME 256

class OutputDevice {
    public:
        char szName[MAX_DEVICE_NAME];
        int opened;
        int delay_ticks;
        char*alias;
        int type;

        OutputDevice() {
            strcpy(szName,"no name");
            type = OUTPUTDEVICE_TYPE_NONE;
            delay_ticks = 0;
            alias = NULL;
            opened = 0;
            this->clear_notestates();
        }
        virtual ~OutputDevice() {
        }

        virtual int open(void)=0;
        virtual int close(void)=0;
        virtual void reset(void) {
            this->clear_notestates();
        }
        virtual void panic(void) {
            reset();
        }
        virtual void hardpanic(void) {
        }
        
        unsigned int notestates[128][16];

        void clear_notestates(void) {
            for(int i=0;i<128;i++)
                for (int j=0;j<16;j++)
                    this->notestates[i][j] = NB_OFF; 
        }


        virtual void send(unsigned int msg)=0;
        virtual void noteOn(unsigned char note, unsigned char chan, unsigned char vol)=0;
        virtual void noteOff(unsigned char note, unsigned char chan, unsigned char vol)=0;
        virtual void afterTouch(unsigned char note, unsigned char chan, unsigned char vol)=0;
        virtual void pitchWheel(unsigned char chan, unsigned short int value)=0;
        virtual void progChange(int program, int bank, unsigned char chan)=0;
        virtual void sendCC(unsigned char cc, unsigned char value,unsigned char chan)=0;

};

class AudioOutputDevice : public OutputDevice {
    public:
        AudioOutputDevice() {
        }
        virtual ~AudioOutputDevice() {
        }
        virtual void work( void *udata, Uint8 *stream, int len)=0;
};

class midiOut {
    public:
        OutputDevice *outputDevices[MAX_MIDI_OUTS];
        unsigned int numOuputDevices;
        intlist *devlist_head;

        midiOut();
        ~midiOut();

        void set_delay_ticks(int dev, int ticks);
        int get_delay_ticks(int dev);

        void set_bank_select(int dev, int bank_slecect);
        int get_bank_select(int dev);


        void set_alias(int dev, char *alias);
        char* get_alias(int dev);

        UINT AddDevice(int dev);
        int RemDevice(int dev);
        int QueryDevice(int dev); // 1 active 0 not active
        int GetNumOpenDevs(void);
        int GetDevID(int which);
        

        void MixAudio(void *udata, Uint8 *stream, int len) {
            AudioOutputDevice *aod;
            intlist *t = devlist_head;
            while(t) {
                if (outputDevices[t->key]->type == OUTPUTDEVICE_TYPE_AUDIO) {
                    aod = (AudioOutputDevice *)outputDevices[t->key];
                    if (aod->opened)
                        aod->work(udata,stream,len);
                }
                t = t->next;
            }
        }

        void panic(void) {
            intlist *t = devlist_head;
            while(t) {
                outputDevices[t->key]->panic();
                t = t->next;
            }
        }
        void hardpanic(void) {
            intlist *t = devlist_head;
            while(t) {
                outputDevices[t->key]->panic();
                t = t->next;
            }
        }

        inline void midiOut::send(unsigned int dev,unsigned  int msg) {
            if (dev>=numOuputDevices) 
                return;
            outputDevices[dev]->send(msg);
        }
        inline void midiOut::sendGlobal(unsigned int msg) {
            intlist *t = devlist_head;
            while(t) {
                outputDevices[t->key]->send(msg);
                t = t->next;
            }
        } 
        
        inline void midiOut::noteOn(unsigned int dev, unsigned char note, unsigned char chan, unsigned char vol, unsigned char track, unsigned char muted, unsigned char flags = 0) {
            if (dev>=numOuputDevices) return;
            if (outputDevices[dev]->opened && !muted)
                outputDevices[dev]->noteOn(note,chan,vol);
            outputDevices[dev]->notestates[note][chan] = track | (vol<<8) | NB_ON;
            if (muted)
                outputDevices[dev]->notestates[note][chan] |= NB_MUTED;
            if (flags & INSTFLAGS_REGRIGGER_NOTE_ON_UNMUTE)
                outputDevices[dev]->notestates[note][chan] |= NB_RETRIG;
        }

        inline void midiOut::noteOff(unsigned int dev, unsigned char note, unsigned char chan, unsigned char vol,unsigned char muted) {
            if (dev>=numOuputDevices) return;
            if (outputDevices[dev]->opened && !muted)
                outputDevices[dev]->noteOff(note,chan,vol);
            outputDevices[dev]->notestates[note][chan] = NB_OFF;
        }

        inline void midiOut::afterTouch(unsigned int dev, unsigned char note, unsigned char chan, unsigned char vol) {
            if (dev>=numOuputDevices) return;
            outputDevices[dev]->afterTouch(note,chan,vol);
        }

        inline void midiOut::pitchWheel(unsigned int dev, unsigned char chan, unsigned short int value) {
            if (dev>=numOuputDevices) return;
             outputDevices[dev]->pitchWheel(chan,value);
        }

        inline void midiOut::progChange(unsigned int dev, int program, int bank, unsigned char chan) {
            if (dev>=numOuputDevices) return;
            outputDevices[dev]->progChange(program,bank,chan);
        }
        inline void midiOut::sendCC(unsigned int dev,unsigned char cc, unsigned char value,unsigned char chan) {
            if (dev>=numOuputDevices) return;
            outputDevices[dev]->sendCC(cc,value,chan);
        }
        inline void midiOut::clock(void) {
            sendGlobal(0xF8);
        }
        

        inline void midiOut::mute_track(unsigned char track) {
            unsigned char vol;
            int i,j, val;
            int flags = 0;
            intlist *t = devlist_head;
            while(t) { 
                for (i=0;i<128;i++) {
                    for (j=0;j<16;j++) {
                        val = outputDevices[t->key]->notestates[i][j];
                        flags = val & 0xFF0000;
                        if ( ( (val & 0x0000FF) == track ) &&
                             ( (flags) & NB_ON )
                            ) {
                            vol = (val & 0x00FF00)>>8;
                            outputDevices[t->key]->noteOff(i,j,0);
                            flags |= NB_MUTED;
                            outputDevices[t->key]->notestates[i][j] = (vol<<8) + track + flags;
                        }
                    }
                }
                t = t->next;
            }
        }

        inline void midiOut::unmute_track(unsigned char track) {
            unsigned char vol;
            int i,j;
            intlist *t = devlist_head;
            while(t) { 
                for (i=0;i<128;i++) {
                    for (j=0;j<16;j++) {
                        unsigned int ns = outputDevices[t->key]->notestates[i][j] & 0xFF00FF;
                        unsigned int fl = (track | NB_MUTED | NB_ON | NB_RETRIG);
                        if (ns == fl) {
                            vol = (outputDevices[t->key]->notestates[i][j] & 0xFF00)>>8;
                            outputDevices[t->key]->noteOn(i,j,vol);
                            outputDevices[t->key]->notestates[i][j] = (vol<<8) | track | NB_ON | NB_RETRIG;
                        }
                    }
                }
                t = t->next;
            }
        }



};
/*
class midiOutDevice {
    public:
        HMIDIOUT handle;
        char *szName;
        int devNum;
        MIDIOUTCAPS caps;
        int opened;     
        int delay_ticks;
        //int delay_ms;
        
        midiOutDevice(int i);
        ~midiOutDevice(void);
        
        int open(void);
        int close(void);
        void clear_notestates(void);
        void reset(void);
        
        
        unsigned int notestates[128][16];

};

class midiOut {
    public:
        midiOutDevice *midiOutDev[MAX_MIDI_OUTS];
        unsigned int numMidiDevs;
        intlist *devlist_head;
        midiOut();
        ~midiOut();
        UINT AddDevice(int dev);
        int RemDevice(int dev);
        int QueryDevice(int dev); // 1 active 0 not active
        int GetNumOpenDevs(void);
        int GetDevID(int which);

        void panic(void);
        void hardpanic(void);
        void set_delay_ticks(int dev, int ticks);
        int get_delay_ticks(int dev);
        
        inline void midiOut::send(unsigned int dev, int msg) {
            if (dev>numMidiDevs) return;
            if (midiOutDev[dev]->opened)
                midiOutShortMsg(midiOutDev[dev]->handle,msg);
        }
        inline void midiOut::sendGlobal(int msg) {
            intlist *t = devlist_head;
            while(t) {
                midiOutShortMsg(midiOutDev[t->key]->handle,msg);
                t = t->next;
            }
        }

        inline void midiOut::mute_track(unsigned char track) {
            unsigned char vol;
            int i,j, val;
            int flags = 0;
            intlist *t = devlist_head;
            while(t) { 
                for (i=0;i<128;i++) {
                    for (j=0;j<16;j++) {
                        val = midiOutDev[t->key]->notestates[i][j];
                        flags = val & 0xFF0000;
                        if ( ( (val & 0x0000FF) == track ) &&
                             ( (flags) & NB_ON )
                            ) {
                            vol = (val & 0x00FF00)>>8;
                            this->noteOff(t->key,i,j,0,0);
                            flags |= NB_MUTED;
                            midiOutDev[t->key]->notestates[i][j] = (vol<<8) + track + flags;
                        }
                    }
                }
                t = t->next;
            }
        }

        inline void midiOut::unmute_track(unsigned char track) {
            unsigned char vol;
            int i,j;
            intlist *t = devlist_head;
            while(t) { 
                for (i=0;i<128;i++) {
                    for (j=0;j<16;j++) {
                        unsigned int ns = midiOutDev[t->key]->notestates[i][j] & 0xFF00FF;
                        unsigned int fl = (track | NB_MUTED | NB_ON | NB_RETRIG);
                        if (ns == fl) {
                            vol = (midiOutDev[t->key]->notestates[i][j] & 0xFF00)>>8;
                            this->noteOn(t->key,i,j,vol,track,0);
                            midiOutDev[t->key]->notestates[i][j] = (vol<<8) | track | NB_ON | NB_RETRIG;
                        }
                    }
                }
                t = t->next;
            }
        }

        inline void midiOut::noteOn(unsigned int dev, unsigned char note, unsigned char chan, unsigned char vol, unsigned char track, unsigned char muted, unsigned char flags = 0) {
            if (dev>=numMidiDevs) return;
            if (midiOutDev[dev]->opened && !muted)
                midiOutShortMsg(midiOutDev[dev]->handle,(0x90+(chan)) + ((note)<<8) + ((vol)<<16));
            midiOutDev[dev]->notestates[note][chan] = track | (vol<<8) | NB_ON;
            if (muted)
                midiOutDev[dev]->notestates[note][chan] |= NB_MUTED;
            if (flags & INSTFLAGS_REGRIGGER_NOTE_ON_UNMUTE)
                midiOutDev[dev]->notestates[note][chan] |= NB_RETRIG;
        }

        inline void midiOut::noteOff(unsigned int dev, unsigned char note, unsigned char chan, unsigned char vol,unsigned char muted) {
            if (dev>=numMidiDevs) return;
            if (midiOutDev[dev]->opened && !muted)
                midiOutShortMsg(midiOutDev[dev]->handle,(0x80+(chan)) + ((note)<<8) + ((vol)<<16));
            midiOutDev[dev]->notestates[note][chan] = NB_OFF;
        }

        inline void midiOut::afterTouch(unsigned int dev, unsigned char note, unsigned char chan, unsigned char vol) {
            if (dev>=numMidiDevs) return;
            if (midiOutDev[dev]->opened)
                midiOutShortMsg(midiOutDev[dev]->handle,(0xD0+(chan)) + ((note)<<8) + ((vol)<<16));
        }

        inline void midiOut::pitchWheel(unsigned int dev, unsigned char chan, unsigned short int value) {
            unsigned char d1,d2;
            if (dev>=numMidiDevs) return;
            value&=0x3FFF;
            d1 = (value & 0x007F);
            d2 = value>>7;
            if (midiOutDev[dev]->opened)
                midiOutShortMsg(midiOutDev[dev]->handle,(0xE0+(chan)) + (d1<<8) + (d2<<16));
        }

        inline void midiOut::progChange(unsigned int dev, int program, int bank, unsigned char chan) {
            unsigned short int b;
            unsigned char hb,lb;
            bank &= 0x3fff;
            lb = bank&0x007F;
            hb = bank>>7;
            b = bank;
            if (dev>=numMidiDevs) return;
            if (midiOutDev[dev]->opened) {
                if (bank >= 0) {
                    midiOutShortMsg(midiOutDev[dev]->handle,(0xB0+(chan)) + (0x00 << 8) + (hb<<16)); // MSB Bank select (High 7 bits)
                    midiOutShortMsg(midiOutDev[dev]->handle,(0xB0+(chan)) + (0x20 << 8) + (lb<<16)); // LSB Bank select (Low 7 bits)
                }
                if (program >= 0)
                    midiOutShortMsg(midiOutDev[dev]->handle,(0xC0+(chan)) + (((unsigned char)program)<<8));                  // Program change
            }           
        }
        inline void midiOut::sendCC(unsigned int dev,unsigned char cc, unsigned char value,unsigned char chan) {
            if (dev>=numMidiDevs) return;
            if (midiOutDev[dev]->opened)
                midiOutShortMsg(midiOutDev[dev]->handle,(0xB0+chan) + (cc<<8) + (value<<16));
        }
        inline void midiOut::clock(void) {
            sendGlobal(0xF8);
        }

};



*/
class midiInDevice {
    public:
        HMIDIIN handle;
        char *szName;
        int devNum;
        MIDIINCAPS caps;
        int opened;     

        MIDIHDR         midiHdr;
        unsigned char SysXBuffer[256];
        unsigned char SysXFlag;

        midiInDevice(int i);
        ~midiInDevice(void);
        
        int open(void);
        int close(void);
        MMRESULT reset(void);
        
};


class midiIn {
    public:
        midiInDevice *midiInDev[MAX_MIDI_INS];
        unsigned int numMidiDevs;
        intlist *devlist_head;
        midiIn();
        ~midiIn();
        UINT AddDevice(int dev);
        int RemDevice(int dev);
        int QueryDevice(int dev); // 1 active 0 not active
        int GetNumOpenDevs(void);
        int GetDevID(int which);
        midiInDevice *GetDevice(HMIDIIN hmi);
};


#endif
