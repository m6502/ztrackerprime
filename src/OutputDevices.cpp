#include "zt.h"



void AddPlugin(midiOut *mout, OutputDevice *o)
{
    if (mout->numOuputDevices<MAX_MIDI_OUTS-1) {
        mout->outputDevices[mout->numOuputDevices] = o;
        mout->numOuputDevices++;
    }
}



void InitializePlugins(midiOut *mout)
{
    // MIDI devices are registered FIRST so their indices (which instruments
    // persist in .zt as midi_device) are stable regardless of whether audio
    // is enabled. Audio output devices, when enabled, come AFTER.
    unsigned int i = mout->numOuputDevices;
    unsigned int devs = midiOutGetNumDevs();
    unsigned int total = i+devs;
    mout->numOuputDevices += devs;
    for (;i<total;i++) {
        mout->outputDevices[i] = new MidiOutputDevice(i);
    }

#ifdef _ENABLE_AUDIO
    // Opt-in (zt.conf audio_enabled / --load-sample). The sample voice
    // mixer is opened immediately so the audio backend can pump it.
    if (zt_config_globals.audio_enabled) {
        SampleOutputDevice *smp = new SampleOutputDevice();
        smp->open();
        AddPlugin(mout, smp);
    }
#endif
}




///////
//
// MidiOutput plugin - Midioutput plugin
//


MidiOutputDevice::MidiOutputDevice(int deviceIndex) {
    devNum = deviceIndex;
    m_runningStatus = reverse_bank_select = 0;
    handle = NULL;
    type = OUTPUTDEVICE_TYPE_MIDI;
    if (!midiOutGetDevCaps(deviceIndex, &caps, sizeof(MIDIOUTCAPS)))
        strcpy(szName, caps.szPname);
}
MidiOutputDevice::~MidiOutputDevice() {
    close();
}
int MidiOutputDevice::open(void) {
    int error;
    if (opened) {
        if (close())
            return -1;
    }
    if (!(error = midiOutOpen(&handle, devNum, 0, 0, CALLBACK_NULL))) {
        opened = 1;
        return 0;
    }
    return error;
}
int MidiOutputDevice::close(void) {
    if (opened) {
        if (!midiOutClose(handle)) {
            handle = NULL;
            opened = 0;
            return 0;
        }
    }
    return -1;
}


void MidiOutputDevice::reset(void) 
{
  int i, j ;

  if (this->opened) {
    
    for(i=0;i<128;i++) {

      for (j=0;j<16;j++) {

        if (this->notestates[i][j] & NB_ON) midiOutMsg( 0x80 + j, i, 0);
      }
    }
    
    for (i=0;i<16;i++) {

      midiOutMsg(0xE0 + i, 0x0, 0x40);
      //            midiOutShortMsg(this->handle,(0xE0+i) + (0x40<<16)); // reset pitchwheel
      /*
      midiOutShortMsg(this->handle,(0xB0+i) + (0x79<<8) + (0<<16));     // Reset ctrl
      midiOutShortMsg(this->handle,(0xB0+i) + (0x40<<8) + (0<<16));     // Sustain
      midiOutShortMsg(this->handle,(0xB0+i) + (0x01<<8) + (0<<16));     // Modulation
      */
    }
  }
  
  m_runningStatus = 0;
  OutputDevice::reset(); // dont forget this
}


void MidiOutputDevice::hardpanic(void) {
    panic();
    if (this->opened)
        midiOutReset(handle);
    m_runningStatus = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
void MidiOutputDevice::send(unsigned int msg)
{
  if (this->opened) {
    
    midiOutShortMsg(handle, msg);

    if ((msg & 0xFF) >= 0xF0 && 
        (msg & 0xFF) < 0xF8) {
      
      m_runningStatus = 0; // Clear running status if not system realtime or data bytes
    }
  } 
}




void MidiOutputDevice::noteOn(unsigned char note, unsigned char chan, unsigned char vol) {
	midiOutMsg( 0x90 + chan, note, vol);
}
void MidiOutputDevice::noteOff(unsigned char note, unsigned char chan, unsigned char vol) {
	midiOutMsg( 0x80 + chan, note, vol);
}
void MidiOutputDevice::afterTouch(unsigned char note, unsigned char chan, unsigned char vol) {
	midiOutMsg( 0xD0 + chan, note, vol);
}
void MidiOutputDevice::pitchWheel(unsigned char chan, unsigned short int value) {
    unsigned char d1,d2;
    value&=0x3FFF;
    d1 = (value & 0x007F);
    d2 = value>>7;
	midiOutMsg( 0xE0 + chan, d1, d2);

}
void MidiOutputDevice::progChange(int program, int bank, unsigned char chan) {
    if (this->opened) {
        unsigned char hb,lb;
        if (bank>=0) {
            bank &= 0x3fff;
            lb = bank&0x007F;
            hb = bank>>7;
            if (this->reverse_bank_select) {
                // reverse
				midiOutMsg( 0xB0 + chan, 0x00, lb);
				midiOutMsg( 0xB0 + chan, 0x20, hb);

            } else {
                // regular
				midiOutMsg( 0xB0 + chan, 0x00, hb);
				midiOutMsg( 0xB0 + chan, 0x20, lb);
            }
        }
        if (program >= 0) 
			midiOutMsg( 0xC0 + chan, program, 0x00);
    }
}

void MidiOutputDevice::sendCC(unsigned char cc, unsigned char value,unsigned char chan) {
	midiOutMsg( 0xB0 + chan, cc, value);
}

int MidiOutputDevice::sendSysEx(const unsigned char *bytes, int len) {
    if (!this->opened || !bytes || len <= 0) return 0;
    // Caller passes the full SysEx including 0xF0..0xF7 framing. Defensive
    // check: at minimum we want F0 ... F7. If the caller forgot to frame
    // we refuse rather than spit out a malformed byte stream that some
    // synths interpret as random short messages.
    if (bytes[0] != 0xF0 || bytes[len - 1] != 0xF7) return 0;
    return zt_midi_out_sysex(handle, bytes, len) == MMSYSERR_NOERROR ? 1 : 0;
}



#ifdef _ENABLE_AUDIO

#include "sample_voice.h"

///////
//
// SampleOutputDevice - software sampler. Plays PCM from song->samples[]
// polyphonically, pitched from the note relative to each sample's root.
//

SampleOutputDevice::SampleOutputDevice() {
    type = OUTPUTDEVICE_TYPE_AUDIO;
    strcpy(szName, "Sample Player");
    for (int v = 0; v < ZT_SAMPLE_MAX_VOICES; v++) voices[v].active = false;
    for (int c = 0; c < 16; c++) sample_for_chan[c] = -1;
}

SampleOutputDevice::~SampleOutputDevice() {
}

int SampleOutputDevice::open(void)  { opened = 1; return 0; }
int SampleOutputDevice::close(void) { opened = 0; return 0; }

void SampleOutputDevice::all_voices_off_locked() {
    for (int v = 0; v < ZT_SAMPLE_MAX_VOICES; v++) voices[v].active = false;
}

void SampleOutputDevice::reset(void) {
    {
        std::lock_guard<std::mutex> lk(mtx);
        all_voices_off_locked();
        for (int c = 0; c < 16; c++) sample_for_chan[c] = -1;
    }
    OutputDevice::reset(); // clears notestates
}

void SampleOutputDevice::hardpanic(void) {
    panic();
}

void SampleOutputDevice::send(unsigned int /*msg*/) {}

// program = sample-pool index for this channel (-1 / out-of-range clears it).
void SampleOutputDevice::progChange(int program, int /*bank*/, unsigned char chan) {
    std::lock_guard<std::mutex> lk(mtx);
    sample_for_chan[chan & 15] = (program >= 0 && program < ZTM_MAX_SAMPLES) ? program : -1;
}

void SampleOutputDevice::noteOn(unsigned char note, unsigned char chan, unsigned char vol) {
    std::lock_guard<std::mutex> lk(mtx);
    int idx = sample_for_chan[chan & 15];
    if (idx < 0 || !song || !song->samples[idx] || song->samples[idx]->isempty())
        return;
    const zt_sample *s = song->samples[idx];

    // find a free voice, else steal the one nearest its end (largest pos)
    int slot = -1; double worst = -1.0;
    for (int v = 0; v < ZT_SAMPLE_MAX_VOICES; v++) {
        if (!voices[v].active) { slot = v; break; }
        if (voices[v].pos > worst) { worst = voices[v].pos; slot = v; }
    }
    voice &vo = voices[slot];
    vo.smp    = s;
    vo.pos    = 0.0;
    vo.step   = zt_voice_step(s, (int)note, ZT_SAMPLE_OUT_RATE);
    vo.note   = note;
    vo.chan   = chan;
    vo.vel    = vol;
    vo.active = true;
}

void SampleOutputDevice::noteOff(unsigned char note, unsigned char chan, unsigned char /*vol*/) {
    std::lock_guard<std::mutex> lk(mtx);
    // One-shot/looped: a note-off stops matching voices. (No release tail
    // in this first cut; the editor PR can add loop-release behaviour.)
    for (int v = 0; v < ZT_SAMPLE_MAX_VOICES; v++) {
        if (voices[v].active && voices[v].note == note && voices[v].chan == chan)
            voices[v].active = false;
    }
}

void SampleOutputDevice::afterTouch(unsigned char, unsigned char, unsigned char) {}

// Live pitch-bend: scale every active voice on the channel. value is the
// raw 14-bit wheel (0x2000 = center); map to +/- 2 semitones.
void SampleOutputDevice::pitchWheel(unsigned char chan, unsigned short int value) {
    std::lock_guard<std::mutex> lk(mtx);
    double semis = ((double)value - 8192.0) / 8192.0 * 2.0;
    double bend  = pow(2.0, semis / 12.0);
    for (int v = 0; v < ZT_SAMPLE_MAX_VOICES; v++) {
        if (voices[v].active && voices[v].chan == chan && voices[v].smp) {
            double base = zt_voice_step(voices[v].smp, (int)voices[v].note, ZT_SAMPLE_OUT_RATE);
            voices[v].step = base * bend;
        }
    }
}

void SampleOutputDevice::sendCC(unsigned char, unsigned char, unsigned char) {}

// Audio callback thread. Writes interleaved S16 stereo, `len` bytes total
// (= len/4 frames). Always writes every frame (silence when no voice), so
// the uninitialised mix buffer never leaks garbage.
void SampleOutputDevice::work(void * /*udata*/, Uint8 *stream, int len) {
    std::lock_guard<std::mutex> lk(mtx);
    short int *out = (short int *)stream;
    int frames = len / (int)(2 * sizeof(short int));

    for (int f = 0; f < frames; f++) {
        double accL = 0.0, accR = 0.0;
        for (int v = 0; v < ZT_SAMPLE_MAX_VOICES; v++) {
            voice &vo = voices[v];
            if (!vo.active || !vo.smp) continue;
            double g = (double)vo.vel / 127.0;
            accL += g * (double)zt_voice_read(vo.smp, vo.pos, 0);
            accR += g * (double)zt_voice_read(vo.smp, vo.pos, 1);
            bool active = true;
            vo.pos = zt_voice_advance(vo.smp, vo.pos, vo.step, &active);
            vo.active = active;
        }
        if (accL >  32767.0) accL =  32767.0; if (accL < -32768.0) accL = -32768.0;
        if (accR >  32767.0) accR =  32767.0; if (accR < -32768.0) accR = -32768.0;
        out[f * 2 + 0] = (short int)accL;
        out[f * 2 + 1] = (short int)accR;
    }
}

#endif
