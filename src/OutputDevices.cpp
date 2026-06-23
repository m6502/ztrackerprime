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
    // MIDI devices first, so their indices match midiOutGetNumDevs() order --
    // the device picker and saved selections index by position. Audio plugins
    // are appended AFTER, so enabling _ENABLE_AUDIO never shifts a MIDI device
    // index (the selection-breaking bug the enable-audio spike found when the
    // audio plugins sat at indices 0,1 ahead of MIDI).
    unsigned int i = mout->numOuputDevices;
    unsigned int devs = midiOutGetNumDevs();
    unsigned int total = i+devs;
    mout->numOuputDevices += devs;
    for (;i<total;i++) {
        mout->outputDevices[i] = new MidiOutputDevice(i);
    }

#ifdef _ENABLE_AUDIO
    AddPlugin(mout, new NoiseOutputDevice());
    AddPlugin(mout, new TestToneOutputDevice());
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
    if (!midiOutGetDevCaps(deviceIndex, &caps, sizeof(MIDIOUTCAPS))) {
        zt_sanitize_device_name(caps.szPname);
        strcpy(szName, caps.szPname);
    }
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


///////
//
// TestTone plugin - Plays a square wave at fixed freq
//

TestToneOutputDevice::TestToneOutputDevice()
{
    type = OUTPUTDEVICE_TYPE_AUDIO;
    strcpy(szName, "TestTone Audio Plugin");
    reset();
}


TestToneOutputDevice::~TestToneOutputDevice() {
}


int TestToneOutputDevice::open(void) {
    //smp = Mix_LoadWAV("sound.wav");
    opened = 1;
    return 0;
}


int TestToneOutputDevice::close(void) {
    //Mix_FreeChunk(smp);
    opened = 0;
    return 0;
}


void TestToneOutputDevice::reset(void) {
    makenoise = 0;
    phase = 0.0;
    lfo   = 0.0;
    OutputDevice::reset(); // dont forget this
}
void TestToneOutputDevice::hardpanic(void) {
    panic();
}
void TestToneOutputDevice::send(unsigned int) {
}
void TestToneOutputDevice::noteOn(unsigned char, unsigned char, unsigned char) {
    makenoise=1;
}
void TestToneOutputDevice::noteOff(unsigned char, unsigned char, unsigned char) {
    makenoise=0;
}
void TestToneOutputDevice::afterTouch(unsigned char, unsigned char, unsigned char) {
}
void TestToneOutputDevice::pitchWheel(unsigned char, unsigned short int) {
}
void TestToneOutputDevice::progChange(int, int, unsigned char) {
}
void TestToneOutputDevice::sendCC(unsigned char, unsigned char, unsigned char) {
}
void TestToneOutputDevice::work( void *udata, Uint8 *stream, int len) {
    (void)udata;
    if (!makenoise) return;   // leave the (mixer-zeroed) buffer silent

    // A sine carrier whose pitch is swept up and down by a slow LFO -- the
    // "weee-ooo" warble. Proper S16 stereo samples (the old version wrote
    // 8-bit bytes into the 16-bit buffer, which just buzzed at a fixed pitch).
    const double SR  = 44100.0;
    const double TAU = 6.283185307179586;
    Sint16 *out = (Sint16 *)stream;
    int frames = len / (int)(2 * sizeof(Sint16));   // 2 channels, S16
    for (int f = 0; f < frames; f++) {
        lfo += TAU * 7.0 / SR;                       // ~7 Hz wobble
        if (lfo >= TAU) lfo -= TAU;
        double freq = 440.0 + 260.0 * sin(lfo);      // sweeps ~180..700 Hz
        phase += TAU * freq / SR;
        if (phase >= TAU) phase -= TAU;
        Sint16 s = (Sint16)(7000.0 * sin(phase));    // ~21% full scale
        *out++ = s;   // left
        *out++ = s;   // right
    }
}



///////
//
// NoiseMaker plugin - Makes random static
//


NoiseOutputDevice::NoiseOutputDevice() {
    type = OUTPUTDEVICE_TYPE_AUDIO;
    strcpy(szName, "NoiseMaker Audio Plugin");
    reset();
}
NoiseOutputDevice::~NoiseOutputDevice() {

}
int NoiseOutputDevice::open(void) {
    opened = 1;
    return 0;
}
int NoiseOutputDevice::close(void) {
    opened = 0;
    return 0;
}
void NoiseOutputDevice::reset(void) {
    makenoise = 0;
    OutputDevice::reset(); // dont forget this
}
void NoiseOutputDevice::hardpanic(void) {
    panic();
}
void NoiseOutputDevice::send(unsigned int) {
}
void NoiseOutputDevice::noteOn(unsigned char, unsigned char, unsigned char) {
    makenoise=1;
}
void NoiseOutputDevice::noteOff(unsigned char, unsigned char, unsigned char) {
    makenoise=0;
}
void NoiseOutputDevice::afterTouch(unsigned char, unsigned char, unsigned char) {
}
void NoiseOutputDevice::pitchWheel(unsigned char, unsigned short int) {
}
void NoiseOutputDevice::progChange(int, int, unsigned char) {
}
void NoiseOutputDevice::sendCC(unsigned char, unsigned char, unsigned char) {
}
void NoiseOutputDevice::work( void *udata, Uint8 *stream, int len) {
    (void)udata;
    if (makenoise) {
        for(int i=0;i<len;i++) {
            stream[i] = rand()&0x3F;
        }
    }
}

#endif
