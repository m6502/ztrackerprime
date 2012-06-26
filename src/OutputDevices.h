#ifndef _OUTPUT_DEVICES_H
#define _OUTPUT_DEVICES_H

#include "zt.h"
       

class TestToneOutputDevice : public AudioOutputDevice {
    public:
        int makenoise;
        int wavec;
         unsigned char wave[256];
         TestToneOutputDevice();
        ~TestToneOutputDevice();
        virtual int open(void);
        virtual int close(void);
        virtual void reset(void);
        virtual void hardpanic(void);
        virtual void send(unsigned int msg);
        virtual void noteOn(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void noteOff(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void afterTouch(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void pitchWheel(unsigned char chan, unsigned short int value);
        virtual void progChange(int program, int bank, unsigned char chan);
        virtual void sendCC(unsigned char cc, unsigned char value,unsigned char chan);
        virtual void work( void *udata, Uint8 *stream, int len);
};

class NoiseOutputDevice : public AudioOutputDevice {
    public:
        int makenoise;
         NoiseOutputDevice();
        ~NoiseOutputDevice();
        virtual int open(void);
        virtual int close(void);
        virtual void reset(void);
        virtual void hardpanic(void);
        virtual void send(unsigned int msg);
        virtual void noteOn(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void noteOff(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void afterTouch(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void pitchWheel(unsigned char chan, unsigned short int value);
        virtual void progChange(int program, int bank, unsigned char chan);
        virtual void sendCC(unsigned char cc, unsigned char value,unsigned char chan);
        virtual void work( void *udata, Uint8 *stream, int len);
};

class MidiOutputDevice : public OutputDevice {
    public:
        HMIDIOUT handle;
        MIDIOUTCAPS caps;
        int devNum;
        int reverse_bank_select;
         MidiOutputDevice(int devIndex);
        ~MidiOutputDevice();
        virtual int open(void);
        virtual int close(void);
        virtual void reset(void);
        virtual void hardpanic(void);
        virtual void send(unsigned int msg);
        virtual void noteOn(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void noteOff(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void afterTouch(unsigned char note, unsigned char chan, unsigned char vol);
        virtual void pitchWheel(unsigned char chan, unsigned short int value);
        virtual void progChange(int program, int bank, unsigned char chan);
        virtual void sendCC(unsigned char cc, unsigned char value,unsigned char chan);


	private:

		virtual void midiOutMsg(unsigned char status, unsigned char data1, unsigned char data2) {
			if (this->opened)
//				if (status == m_runningStatus) {
//					::midiOutShortMsg( handle, (data1 + (data2<<8)));
//				} else {
					::midiOutShortMsg( handle, (status + (data1<<8) + (data2<<16)) );
//					this->m_runningStatus = status;
//				}
		}

		unsigned char m_runningStatus;

};


#endif