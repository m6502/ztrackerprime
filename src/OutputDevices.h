#ifndef _OUTPUT_DEVICES_H
#define _OUTPUT_DEVICES_H

#include "midi-io.h"

class RtMidiOut; // forward declaration — avoids pulling RtMidi.h into every TU

class MidiOutputDevice : public OutputDevice {
    public:
        RtMidiOut *rtMidiOut;
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
        void midiOutMsg(unsigned char status, unsigned char data1, unsigned char data2);
        unsigned char m_runningStatus;
};


#ifdef _ENABLE_AUDIO

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

#endif

#endif
