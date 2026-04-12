#include "zt.h"
#include "RtMidi.h"
#include <vector>


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

    // Use RtMidi to enumerate MIDI output ports.
    RtMidiOut probe;
    unsigned int devs = probe.getPortCount();

    unsigned int total = i+devs;
    mout->numOuputDevices += devs;
    for (;i<total;i++) {
        mout->outputDevices[i] = new MidiOutputDevice(i);
    }
}


///////
//
// MidiOutput plugin — backed by RtMidi
//

MidiOutputDevice::MidiOutputDevice(int deviceIndex) {
    devNum = deviceIndex;
    m_runningStatus = reverse_bank_select = 0;
    rtMidiOut = nullptr;
    type = OUTPUTDEVICE_TYPE_MIDI;

    // Get port name for display.
    try {
        RtMidiOut probe;
        if ((unsigned)deviceIndex < probe.getPortCount())
            strncpy(szName, probe.getPortName(deviceIndex).c_str(), MAX_DEVICE_NAME-1);
        else
            strcpy(szName, "Unknown MIDI Out");
    } catch (...) {
        strcpy(szName, "Unknown MIDI Out");
    }
    szName[MAX_DEVICE_NAME-1] = '\0';
}

MidiOutputDevice::~MidiOutputDevice() {
    close();
}

int MidiOutputDevice::open(void) {
    if (opened) {
        if (close())
            return -1;
    }
    try {
        rtMidiOut = new RtMidiOut();
        rtMidiOut->openPort(devNum);
        opened = 1;
        return 0;
    } catch (...) {
        delete rtMidiOut;
        rtMidiOut = nullptr;
        return -1;
    }
}

int MidiOutputDevice::close(void) {
    if (opened) {
        try {
            if (rtMidiOut) {
                rtMidiOut->closePort();
                delete rtMidiOut;
                rtMidiOut = nullptr;
            }
            opened = 0;
            return 0;
        } catch (...) {
            return -1;
        }
    }
    return -1;
}


void MidiOutputDevice::reset(void)
{
    int i, j;

    if (this->opened) {
        for(i=0;i<128;i++) {
            for (j=0;j<16;j++) {
                if (this->notestates[i][j] & NB_ON) midiOutMsg( 0x80 + j, i, 0);
            }
        }

        for (i=0;i<16;i++) {
            midiOutMsg(0xE0 + i, 0x0, 0x40);
        }
    }

    m_runningStatus = 0;
    OutputDevice::reset();
}


void MidiOutputDevice::hardpanic(void) {
    panic();
    // Send "All Notes Off" and "All Sound Off" on all channels.
    if (this->opened) {
        for (int ch = 0; ch < 16; ch++) {
            midiOutMsg(0xB0 + ch, 0x7B, 0x00); // All Notes Off
            midiOutMsg(0xB0 + ch, 0x78, 0x00); // All Sound Off
        }
    }
    m_runningStatus = 0;
}


// ------------------------------------------------------------------------------------------------
//
//
void MidiOutputDevice::send(unsigned int msg)
{
    if (this->opened && rtMidiOut) {
        std::vector<unsigned char> message;
        message.push_back(msg & 0xFF);
        message.push_back((msg >> 8) & 0xFF);
        message.push_back((msg >> 16) & 0xFF);
        try {
            rtMidiOut->sendMessage(&message);
        } catch (...) {}

        if ((msg & 0xFF) >= 0xF0 &&
            (msg & 0xFF) < 0xF8) {
            m_runningStatus = 0;
        }
    }
}


void MidiOutputDevice::midiOutMsg(unsigned char status, unsigned char data1, unsigned char data2) {
    if (this->opened && rtMidiOut) {
        std::vector<unsigned char> message;
        message.push_back(status);
        message.push_back(data1);
        message.push_back(data2);
        try {
            rtMidiOut->sendMessage(&message);
        } catch (...) {}
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
                midiOutMsg( 0xB0 + chan, 0x00, lb);
                midiOutMsg( 0xB0 + chan, 0x20, hb);
            } else {
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
    int i;
    type = OUTPUTDEVICE_TYPE_AUDIO;
    strcpy(szName, "TestTone Audio Plugin");
    reset();
    makenoise = 0;
    wavec = 0;
    for (i=0;i<128;i++)
        wave[i] = 128;
    for (;i<256;i++)
        wave[i] = -128;
}

TestToneOutputDevice::~TestToneOutputDevice() {
    close();
}

int TestToneOutputDevice::open(void) {
    opened = 1;
    return 0;
}
int TestToneOutputDevice::close(void) {
    opened = 0;
    return 0;
}
void TestToneOutputDevice::reset(void) {
    makenoise = 0;
    OutputDevice::reset();
}
void TestToneOutputDevice::hardpanic(void) {
    reset();
}
void TestToneOutputDevice::send(unsigned int msg) {}
void TestToneOutputDevice::noteOn(unsigned char note, unsigned char chan, unsigned char vol) {
    makenoise = 1;
}
void TestToneOutputDevice::noteOff(unsigned char note, unsigned char chan, unsigned char vol) {
    makenoise = 0;
}
void TestToneOutputDevice::afterTouch(unsigned char note, unsigned char chan, unsigned char vol) {}
void TestToneOutputDevice::pitchWheel(unsigned char chan, unsigned short int value) {}
void TestToneOutputDevice::progChange(int program, int bank, unsigned char chan) {}
void TestToneOutputDevice::sendCC(unsigned char cc, unsigned char value,unsigned char chan) {}
void TestToneOutputDevice::work( void *udata, Uint8 *stream, int len) {
    if (makenoise) {
        for (int i=0;i<len;i++) {
            stream[i] += wave[wavec++];
            wavec &= 0xFF;
        }
    }
}

///////
//
// Noise plugin - Generates white noise
//

NoiseOutputDevice::NoiseOutputDevice() {
    type = OUTPUTDEVICE_TYPE_AUDIO;
    strcpy(szName, "Noise Audio Plugin");
    makenoise = 0;
}
NoiseOutputDevice::~NoiseOutputDevice() {
    close();
}
int NoiseOutputDevice::open(void) { opened = 1; return 0; }
int NoiseOutputDevice::close(void) { opened = 0; return 0; }
void NoiseOutputDevice::reset(void) { makenoise = 0; OutputDevice::reset(); }
void NoiseOutputDevice::hardpanic(void) { reset(); }
void NoiseOutputDevice::send(unsigned int msg) {}
void NoiseOutputDevice::noteOn(unsigned char note, unsigned char chan, unsigned char vol) { makenoise = 1; }
void NoiseOutputDevice::noteOff(unsigned char note, unsigned char chan, unsigned char vol) { makenoise = 0; }
void NoiseOutputDevice::afterTouch(unsigned char note, unsigned char chan, unsigned char vol) {}
void NoiseOutputDevice::pitchWheel(unsigned char chan, unsigned short int value) {}
void NoiseOutputDevice::progChange(int program, int bank, unsigned char chan) {}
void NoiseOutputDevice::sendCC(unsigned char cc, unsigned char value,unsigned char chan) {}
void NoiseOutputDevice::work( void *udata, Uint8 *stream, int len) {
    if (makenoise) {
        for (int i=0;i<len;i++) {
            stream[i] += (rand() & 0xFF);
        }
    }
}

#endif
