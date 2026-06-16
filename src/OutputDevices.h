#ifndef _OUTPUT_DEVICES_H
#define _OUTPUT_DEVICES_H

#include "midi-io.h"


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
        virtual int  sendSysEx(const unsigned char *bytes, int len);


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



#ifdef _ENABLE_AUDIO

// Fun-sounds generators (Ctrl+Alt+F easter egg). Kept alongside the real
// sampler below: TestTone is the deliberately warbly 8-bit-into-S16 tone the
// fun button plays; Noise is its sibling static generator.
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

#include <mutex>
#include "sample.h"

// Output rate of the SDL audio backend (see main.cpp zt_backend_audio_start).
#define ZT_SAMPLE_OUT_RATE   44100
#define ZT_SAMPLE_MAX_VOICES 32

// Software sampler output device. Plays PCM from the song's sample pool
// (song->samples[]) polyphonically, pitched from the played note relative
// to each sample's root note. Routed to like any other output device: an
// instrument whose midi_device points here plays a sample instead of
// sending MIDI. progChange(sample_index, _, chan) selects which pool slot
// a channel plays (playback sends it before noteOn for sample instruments).
//
// Thread model: noteOn/noteOff/progChange/reset run on the UI + playback
// threads; work() runs on the SDL audio callback thread. A single mutex
// guards the voice array + per-channel sample map. Each voice captures a
// raw `const zt_sample *` at noteOn; whoever frees a pool sample MUST stop
// this device's voices first (the sample editor in the follow-up PR locks
// + clears before mutating the pool).
class SampleOutputDevice : public AudioOutputDevice {
    public:
        SampleOutputDevice();
        ~SampleOutputDevice();
        virtual int  open(void);
        virtual int  close(void);
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

    private:
        struct voice {
            const zt_sample *smp;
            double           pos;     // fractional read position, in frames
            double           step;    // frames advanced per output frame
            unsigned char    note;
            unsigned char    chan;
            unsigned char    vel;
            bool             active;
        };
        voice      voices[ZT_SAMPLE_MAX_VOICES];
        int        sample_for_chan[16];   // pool index per channel, -1 = none
        std::mutex mtx;

        void all_voices_off_locked();
};

#endif

#endif