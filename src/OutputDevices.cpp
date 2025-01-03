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
    
#ifdef _ENABLE_AUDIO    
    AddPlugin(mout, new NoiseOutputDevice());
    AddPlugin(mout, new TestToneOutputDevice());
#endif
    
    unsigned int i = mout->numOuputDevices;
    unsigned int devs = midiOutGetNumDevs();
    unsigned int total = i+devs;
    mout->numOuputDevices += devs;
    for (;i<total;i++) {
        mout->outputDevices[i] = new MidiOutputDevice(i);
    }
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
        unsigned short int b;
        unsigned char hb,lb;
        if (bank>=0) {
            bank &= 0x3fff;
            lb = bank&0x007F;
            hb = bank>>7;
            b = bank;
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



#ifdef _ENABLE_AUDIO    


///////
//
// TestTone plugin - Plays a square wave at fixed freq
//

TestToneOutputDevice::TestToneOutputDevice()
{
  int i ;

    type = OUTPUTDEVICE_TYPE_AUDIO;
    strcpy(szName, "TestTone Audio Plugin");
    reset();
    wavec = 0;
    for(i=0;i<128;i++)
        wave[i]=0;
    for(i=128;i<256;i++)
        wave[i]=0x7F;
    //smp = NULL;
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
    OutputDevice::reset(); // dont forget this
}
void TestToneOutputDevice::hardpanic(void) {
    panic();
}
void TestToneOutputDevice::send(unsigned int msg) {
}
void TestToneOutputDevice::noteOn(unsigned char note, unsigned char chan, unsigned char vol) {
    makenoise=1;
    //Mix_PlayChannel(-1,smp,0);
}
void TestToneOutputDevice::noteOff(unsigned char note, unsigned char chan, unsigned char vol) {
    makenoise=0;
}
void TestToneOutputDevice::afterTouch(unsigned char note, unsigned char chan, unsigned char vol) {
}
void TestToneOutputDevice::pitchWheel(unsigned char chan, unsigned short int value) {
}
void TestToneOutputDevice::progChange(int program, int bank, unsigned char chan) {
}
void TestToneOutputDevice::sendCC(unsigned char cc, unsigned char value,unsigned char chan) {
}
void TestToneOutputDevice::work( void *udata, Uint8 *stream, int len) {
    //Mix_PlayChannelTimed()
    
        if (makenoise) {
        for(int i=0;i<len;i++) {
            stream[i] = wave[wavec&0xFF];
            wavec++; 
        }
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
void NoiseOutputDevice::send(unsigned int msg) {
}
void NoiseOutputDevice::noteOn(unsigned char note, unsigned char chan, unsigned char vol) {
    makenoise=1;
}
void NoiseOutputDevice::noteOff(unsigned char note, unsigned char chan, unsigned char vol) {
    makenoise=0;
}
void NoiseOutputDevice::afterTouch(unsigned char note, unsigned char chan, unsigned char vol) {
}
void NoiseOutputDevice::pitchWheel(unsigned char chan, unsigned short int value) {
}
void NoiseOutputDevice::progChange(int program, int bank, unsigned char chan) {
}
void NoiseOutputDevice::sendCC(unsigned char cc, unsigned char value,unsigned char chan) {
}
void NoiseOutputDevice::work( void *udata, Uint8 *stream, int len) {
    if (makenoise) {
        for(int i=0;i<len;i++) {
            stream[i] = rand()&0x3F;
        }
    }
}

#endif
