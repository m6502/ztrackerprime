
// Impulse Tracker File Format Loading/Saving/Editing code
// version 0.11
// Copyright 1997-2000 Austin Luminais
// e-mail: lipid@chromosome57.com
// http://www.chromosome57.com/luminais

// This code is free for non-commercial use. Donations appreciated for commercial use.

// This code is incomplete, undocumented and may have bugs. Use at your own risk.
// If you make additions or fixes, please send them to me.

// Things NOT supported:
//      any 2.14 specific file format features
//      compressed samples (IT 2.14 format) - please contact me if you have source for this!
//      old instrument format
//      user message (shift-F9 message in impulse tracker)
//
//  0.10 - initial release
//  0.11 - memory leak fixes (thanks to christopher micali)
//         fixed sample loading bug


#ifndef IT_H
#define IT_H

#include <stdio.h>
#include <string.h>

// 32-bit ints, 16-bit words

#ifndef WORD
#define WORD short
#endif

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef TRUE
#define TRUE true
#endif

#ifndef FALSE
#define FALSE false
#endif

#define MAX_PATTERN_CHANNELS 64
#define MAX_PACKED_PATTERN_DATA 100000

#define PACK_PATTERNS 1
#define UNPACK_PATTERNS 2

#define PATTERN_NULL_VALUE 255


//TODO: message loading and saving.
//      old instrument format.
//      ITChannelData::SetNumRows, verification of rows allocated for unpacking.
//      endianess portability.
//      documentation.
//      won't load compressed samples


//////////////////////////////////////////////////////////////////////////////////////////

class ITHeader {
public:
    char headerID[4];
    char songName[26];
    WORD extra;
    WORD numOrders;
    WORD numIns;
    WORD numSamples;
    WORD numPatterns;

    WORD trackerVersion;
    WORD formatVersion;
    WORD flags;
    WORD special; // if bit 0 is set, song message is attached.

    BYTE globalVolume;
    BYTE mixVolume;
    BYTE initialSpeed;
    BYTE initialTempo;
    BYTE separation;        // stereo seperation

    BYTE nullByte;

    WORD messageLength;
    int  messageOffset;

    int reserved;

    BYTE channelPan[64];
    BYTE channelVolume[64];

    BYTE* orders;             // length is numOrders

    // arrays of long offsets
    int*  instrumentOffset;   // length is numIns * sizeof(int)
    int*  sampleOffset;       // length is numSamples * sizeof(int)
    int*  patternOffset;      // length is numPatterns * sizeof(int)

public:

    ITHeader() { Init(); }
    ~ITHeader() {
        delete[] orders;
        delete[] instrumentOffset;
        delete[] sampleOffset;
        delete[] patternOffset;
    }

    void Init(void) { // init with default info.

        int i ;

        strcpy(headerID, "IMPM");
        
        for (i = 0; i < 26; i++) {
          
          songName[i] = 0;
        }

        extra = 0;
        numOrders = numIns = numSamples = numPatterns = 0;

        trackerVersion = 533;
        formatVersion = 532;

        flags = 0;
        special = 0;

        globalVolume = 128;
        mixVolume = 48;
        initialSpeed = 6;
        initialTempo = 125;
        separation = 128;

        nullByte = 0;

        messageLength = messageOffset = 0;

        reserved = 0;

        for (i = 0; i < 64; i++) {
          
          channelPan[i] = 32;
        }

        for (i = 0; i < 64; i++) {
          
          channelVolume[i] = 64;
        }

        orders = NULL;
        instrumentOffset = sampleOffset = patternOffset = NULL;
    }


    bool Load(FILE* infile, long offset = 0);
    bool Save(FILE* outfile, long offset = 0);
    bool SaveOffsets(FILE* outfile, long offset = -1);

    void Print(void);
        
}; // class ITHeader



///////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////


class ITInstrumentEnvelope {
public:
    BYTE flag;
    BYTE numNodePoints;
    BYTE loopBeginning;
    BYTE loopEnd;
    BYTE sustainLoopBeginning;
    BYTE sustainLoopEnd;
    
    struct {
        BYTE yValue;
        WORD tickNumber;
    } nodePoints[25];


public:
    bool Load(FILE* infile) {
        // reads from current file position
        return (fread(this, sizeof(ITInstrumentEnvelope), 1, infile) > 0);
    }

    void Print(void) {
        printf("numNodePoints: %d, loopBeg: %d, loopEnd: %d, susBeg: %d, susEnd: %d\n",
                    numNodePoints, loopBeginning, loopEnd, sustainLoopBeginning,
                    sustainLoopEnd);
    }
};



///////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////




class ITInstrumentHeader {
public:

    char headerID[4];
    char dosFileName[12];

    BYTE nullByte;

    BYTE newNoteAction; // new note action
    BYTE duplicateCheckType; // duplicate check type;
    BYTE duplicateCheckAction; // duplicate check action;

    WORD fadeOut;

    BYTE pitchPanSeperation;
    BYTE pitchPanCenter;

    BYTE globalVolume;
    BYTE defaultPan;
    BYTE randomVolumeVariation;   // percentage
    BYTE randomPanningVariation;

    WORD trackerVersion;
    BYTE numSamples;        // number of samples associated w/ ins.  only used in ins. files.

    BYTE extraByte;

    char name[26];

    WORD extraWord;

    BYTE midiChannel;
    BYTE midiProgram;
    WORD midiBank;

    struct {
        BYTE note;
        BYTE sample;
    } noteTable[120];

    ITInstrumentEnvelope volumeEnvelope;
    ITInstrumentEnvelope panningEnvelope;
    ITInstrumentEnvelope pitchEnvelope;

public:

    bool Save(FILE* outfile, ITHeader* header = NULL, int headIndex = 0, long offset = -1);
    bool Load(FILE* infile, ITHeader* header = NULL, int headIndex = 0, long offset = -1);

    void Print(void);
};



///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITSampleHeader {
public:

    char headerID[4];
    char dosFileName[12];

    BYTE nullByte;

    BYTE globalVolume;
    BYTE flags;
    BYTE defaultVolume;

    char name[26];
    
    BYTE convert;
    BYTE defaultPan;

    int length;  // length in number of samples
    int loopBegin;
    int loopEnd;

    int C5speed;  // number of bytes a second for C-5

    int sustainLoopBegin;
    int sustainLoopEnd;

    int sampleOffset;  // long offset of sample data in file.

    BYTE vibratoSpeed;
    BYTE vibratoDepth;
    BYTE vibratoRate;
    BYTE vibratoType;

    BYTE* sampleData;

public:

    ITSampleHeader() {
        strcpy(headerID, "");
        strcpy(dosFileName, "");
        nullByte = globalVolume = flags = defaultVolume = 0;
        strcpy(name, "");
        convert = defaultPan = length = loopBegin = loopEnd = C5speed = 0;
        sustainLoopBegin = sustainLoopEnd = sampleOffset = 0;
        vibratoSpeed = vibratoDepth = vibratoRate = vibratoType = 0;

        sampleData = NULL;
    }
    ~ITSampleHeader() {
        if (sampleData != NULL)
            delete[] sampleData;
    }

    int SampleSize(void);  // returns size of one sample, in bytes

    int LoopLength(void);  // returns length of the sample loop, 0 if no loop.

    bool Save(FILE* outfile, ITHeader* header = NULL, int headIndex = 0, long offset = -1);
    bool SaveSampleData(FILE* outfile, ITHeader* header = NULL, int headIndex = 0,
                bool updateSampleOffset = TRUE, bool saveSampleOffset = TRUE,
                long offset = -1);

    bool Load(FILE* infile, ITHeader* header = NULL, int headIndex = 0, long offset = -1);
    bool LoadSampleData(FILE* infile);

    void Print(void);
};


///////////////////////////////////////////////////////////////////////////////////////


class ITEventData {   // holds data for one event on one channel
public:
    BYTE note;
    BYTE instrument;
    BYTE volPan;
    BYTE command, commandValue;

public:
    ITEventData() {
        note = instrument = 0;
        volPan = command = commandValue = PATTERN_NULL_VALUE;
    }

    void Print(void) {
        printf("note: %d  ins: %d  volPan: %d  command: %d - %d\n", 
                    note, instrument, volPan, command, commandValue);
    }
};


///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITChannelData {  // holds events for one channel
public:
    ITEventData* events;
    int numRows;
public:
    ITChannelData(int rows) {
        numRows = rows;
        events = new ITEventData[numRows];
        if (events == NULL)
            numRows = 0;
    }
        
    ~ITChannelData() {
        delete[] events;
    }

    void SetNumRows(int num);
    void SetNote(BYTE frame, BYTE value) {
        if (frame < numRows) events[frame].note = value;
    }
    void SetInstrument(BYTE frame, BYTE value) {
        if (frame < numRows) events[frame].instrument = value;
    }
    void SetVolPan(BYTE frame, BYTE value) {
        if (frame < numRows) events[frame].volPan = value;
    }
    void SetCommand(BYTE frame, BYTE command, BYTE value) {
        if (frame < numRows) {
            events[frame].command = command;
            events[frame].commandValue = value;
        }
    }

    void Print(void) {
        printf("channel numRows: %d\n", numRows);
        for (int i = 0; i < numRows; i++)
            events[i].Print();
    }


};



///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITRowData {
    // holds one event at a time for up to 64 channels
public:
    ITEventData channel[MAX_PATTERN_CHANNELS];

public:

ITEventData& operator[](int position) {
    return channel[position];
}


};



///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////

class ITPackedPattern;



class ITPattern { // unpacked Pattern
public:
    WORD numRows;  // number of rows in pattern, from 32 - 200;
    int numChannels;        // number of channels currently allocated for this pattern
    ITChannelData* channels[MAX_PATTERN_CHANNELS];
public:
    ITPattern() { numRows = 0;
                numChannels = 0;
                for (int i = 0; i < MAX_PATTERN_CHANNELS; i++)
                    channels[i] = NULL;
                }
    ~ITPattern() { RemoveAllChannels();}

    bool AddChannel(int rows = 64, int num = 1);
    bool AddChannelsUpTo(int num, int rows = 64);
    void RemoveChannel(void);
    void RemoveAllChannels(void);

    BYTE Note(int channel, int frame);
    BYTE Instrument(int channel, int frame);
    BYTE VolPan(int channel, int frame);
    BYTE Command(int channel, int frame);
    BYTE CommandValue(int channel, int frame);

    void SetChannelNote(BYTE channel, BYTE frame, BYTE value);
    void SetChannelInstrument(BYTE channel, BYTE frame, BYTE value);
    void SetChannelVolPan(BYTE channel, BYTE frame, BYTE value);
    void SetChannelCommand(BYTE channel, BYTE frame, BYTE command, BYTE value);


    void PackTo(ITPackedPattern& packed);

    void Print(void);
};

inline void ITPattern::RemoveAllChannels(void) {
    while (numChannels > 0) 
        RemoveChannel();
}

inline void ITPattern::RemoveChannel(void) {
    numChannels--;
    delete channels[numChannels];
}

inline BYTE ITPattern::Note(int channel, int frame) {
    return channels[channel]->events[frame].note;
}

inline BYTE ITPattern::Instrument(int channel, int frame) {
    return channels[channel]->events[frame].instrument;
}

inline BYTE ITPattern::VolPan(int channel, int frame) {
    return channels[channel]->events[frame].volPan;
}

inline BYTE ITPattern::Command(int channel, int frame) {
    return channels[channel]->events[frame].command;
}

inline BYTE ITPattern::CommandValue(int channel, int frame) {
    return channels[channel]->events[frame].commandValue;
}




class ITPackedPatternList;

class ITPatternList {
public:

    ITPattern* patterns;
    int numPatterns;

public:

    ITPatternList() { patterns = NULL; numPatterns = 0; }

    ITPatternList(int num) {
        patterns = new ITPattern[num];
        if (patterns != NULL)
            numPatterns = num;
    }

    ~ITPatternList() {
        delete[] patterns;
    }

    //  void Print(void);

    void PackTo(ITPackedPatternList& packed);

    ITPattern& operator[](int position) {
        return patterns[position];
    }

};



///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITPackedPattern {
public:

    WORD length;   // length of packed pattern, not including 8-byte header.
    WORD numRows;  // number of rows in pattern, from 32 - 200.
    int extra;

    BYTE* data;

public:

    ~ITPackedPattern() {
        delete[] data;
    }

    bool Load(FILE* infile, ITHeader* header = NULL, int headIndex = 0, long offset = -1);
    bool Save(FILE* outfile, ITHeader* header = NULL, int headIndex = 0, long offset = -1);

    void SetNewData(int dataLength, BYTE* newData);

    void Print(void);

    void UnPackTo(ITPattern& pattern);
};


///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITInstrumentList {
public:

    ITInstrumentHeader* instruments;
    int numInstruments;

public:

    ITInstrumentList() { instruments = NULL; numInstruments = 0; }
    ~ITInstrumentList() {
        delete[] instruments;
    }

    int Load(FILE* infile, ITHeader* header);
    int Save(FILE* outfile, ITHeader* header = NULL, long offset = -1);

    void Print(void);

    ITInstrumentHeader& operator[](int position) {
        return instruments[position];
    }
};


///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////

class ITSampleList {
public:

    ITSampleHeader* samples;
    int numSamples;

public:

    ITSampleList() { samples = NULL; numSamples = 0; }
    ~ITSampleList() {
        delete[] samples;
    }

    int Load(FILE* infile, ITHeader* header);
    int Save(FILE* outfile, ITHeader* header = NULL, long offset = -1);
    int SaveSampleData(FILE* outfile, ITHeader* header = NULL, long offset = -1);

    void Print(void);

    ITSampleHeader& operator[](int position) {
        return samples[position];
    }

};

///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITPackedPatternList {
public:

    ITPackedPattern* packedPatterns;
    int numPatterns;

public:

    ITPackedPatternList() { packedPatterns = NULL; numPatterns = 0; }
    ~ITPackedPatternList() {
        delete[] packedPatterns;
    }

    int Load(FILE* infile, ITHeader* header);
    int Save(FILE* outfile, ITHeader* header = NULL, long offset = -1);

    void Print(void);

    void UnPackTo(ITPatternList& unpacked);

    ITPackedPattern& operator[](int position) {
        return packedPatterns[position];
    }

};


///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


class ITModule {
public:

    ITHeader header;
    ITInstrumentList instruments;
    ITSampleList samples;
    ITPackedPatternList packedPatterns;
    ITPatternList* patterns;

public:
    ITModule();
    ITModule(int numPatterns);
    ~ITModule();

    ITPattern& Patterns(int index) {
        return patterns->patterns[index];
    }

    ITInstrumentHeader& Instruments(int index) {
        return instruments.instruments[index];
    }

    ITSampleHeader& Samples(int index) {
        return samples.samples[index];
    }

    int NumPatterns(void) { return patterns->numPatterns; }

    void AllocPatterns(int num);

    bool LoadModule(const char* fileName, int unpack = 0);
    bool SaveModule(const char* fileName, int pack = 0);

    void PackPatterns(void);
    void UnpackPatterns(void);

    void Print(void);

    bool GrowIfNeeded(int pattern, int channel, int row);

};



#endif