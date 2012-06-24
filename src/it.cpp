/*************************************************************************
 *
 * FILE  it.cpp
 * $Id: it.cpp,v 1.4 2001/08/21 13:12:12 cmicali Exp $
 *
 * DESCRIPTION 
 *   IT module loading code.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2000-2001, Christopher Micali <micali@concentric.net>
 * Copyright (c) 2001, Daniel Kahlin <tlr@users.sourceforge.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their
 *    contributors may be used to endorse or promote products derived 
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS´´ AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******/

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




#include <stdio.h>
#include "it.h"

//  #define DEBUG
//  #define DEBUG_PATTERNS

/////////////////////////////////////////////////////////////////////////////////////


void ConvertITNameString(const char * name, char * newName, int length = 25) {

    for (int i = 0; i < length; i++) {
        if (name[i] == '\0')
            newName[i] = ' ';
        else
            newName[i] = name[i];
    }

    newName[length] = '\0';
}


void PrintBool(bool b) {
    if (b)
        printf("TRUE\n");
    else
        printf("FALSE\n");
}



///////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////

bool ITHeader::Load(FILE* infile, long offset) {

    // returns FALSE if any part did not load correctly.

    bool success = TRUE;

    if (fseek(infile, offset, 0))
        return FALSE;

    // read non-dynamic part of the header, first 192 bytes.
    if (!fread(this, 192, 1, infile))
        return FALSE;

    orders = new BYTE[numOrders];

    instrumentOffset = new int[numIns];
    sampleOffset = new int[numSamples];
    patternOffset = new int[numPatterns];

    success = (orders != NULL) && (instrumentOffset != NULL) &&
                (sampleOffset != NULL) && (patternOffset != NULL);

    if ((orders != NULL) && (numOrders > 0)) {
        if (!fread(orders, sizeof(BYTE), numOrders, infile))
            success = FALSE;
    }

    if ((instrumentOffset != NULL) && (numIns > 0)) {
        if (!fread(instrumentOffset, sizeof(int), numIns, infile))
            success = FALSE;
    }

    if ((sampleOffset != NULL) && (numSamples > 0)) {
        if (!fread(sampleOffset, sizeof(int), numSamples, infile))
            success = FALSE;
    }

    if ((patternOffset != NULL) && (numPatterns > 0)) {
        if (!fread(patternOffset, sizeof(int), numPatterns, infile))
            success = FALSE;
    }

    return success;
}



bool ITHeader::Save(FILE* outfile, long offset) {

    if (fseek(outfile, offset, 0))
        return FALSE;

    if (!fwrite(this, 192, 1, outfile))
        return FALSE;

    return SaveOffsets(outfile, ftell(outfile));

}



bool ITHeader::SaveOffsets(FILE* outfile, long offset) {

    if (offset >= 0) {
        if (fseek(outfile, offset, 0))
            return FALSE;
    } 
    else {
        // seek to position where offsets (starting with orders) are stored
        // in the header.
        if (fseek(outfile, 192, 0))
            return FALSE;
    }

    if (fwrite(orders, sizeof(BYTE), numOrders, outfile) < (unsigned)numOrders)
        return FALSE;
    
    if (fwrite(instrumentOffset, sizeof(int), numIns, outfile) < (unsigned)numIns)
        return FALSE;
    
    if (fwrite(sampleOffset, sizeof(int), numSamples, outfile) < (unsigned)numSamples)
        return FALSE;
    
    if (fwrite(patternOffset, sizeof(int), numPatterns, outfile) < (unsigned)numPatterns)
        return FALSE;

    return TRUE;
}



    
void ITHeader::Print(void) 
{
  int i ;

    printf("header ID: %s\n", headerID);
    printf("songName: %s\n", songName);
    printf("extra: %d\n", extra);
    printf("numOrders: %d\n numIns: %d\nnumSamples: %d\nnumPatterns: %d\n",
        numOrders, numIns, numSamples, numPatterns);
    
    printf("\ntrackerVersion: %d   formatVersion: %d\n", trackerVersion, formatVersion);
    printf("flags: %d  special: %d\n", flags, special);
    printf("\n");

    printf("globalVolume: %d     mixVolume: %d\n", globalVolume, mixVolume);
    printf("initialspeed: %d     initalTempo: %d\n", initialSpeed, initialTempo);
    printf("separation: %d\n", separation);
    printf("nullByte: %d\n\n", nullByte);

    printf("messageLength: %d   messageOffset: %d\n", messageLength, messageOffset);
    printf("reserved: %d\n", reserved);
    printf("\n");

    printf("channelPan: ");
    
    for (i = 0; i < 64; i++) printf("%d ", channelPan[i]);
    
    printf("\n\n");

    printf("channelVolume: ");
    
    for (i = 0; i < 64; i++) printf("%d ", channelVolume[i]);
    
    printf("\n\n");

    printf("orders: ");
    
    for (i = 0; i < numOrders; i++) printf("%d ", orders[i]);
    
    printf("\n\n");

    printf("instrumentOffset: ");
    
    for (i = 0; i < numIns; i++) printf("%d ", instrumentOffset[i]);
    
    printf("\n\n");

    printf("sampleOffset: ");
    
    for (i = 0; i < numSamples; i++) printf("%d ", sampleOffset[i]);
    
    printf("\n\n");

    printf("patternOffset: ");
    
    for (i = 0; i < numPatterns; i++) printf("%d ", patternOffset[i]);
    
    printf("\n\n");
}

////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////

bool ITInstrumentHeader::Load(FILE* infile, ITHeader* header, int headIndex,
                                long offset) {

    // if no header or offset specified, read from current position.
    // if no offset specified, use header and headIndex.
    // otherwise, read from offset.
    //
    // returns FALSE if does not load correctly.

    long position = ftell(infile);

    if (header == NULL) {
        if (offset >= 0)
            position = offset;
    }
    else {
        position = header->instrumentOffset[headIndex];
    }

    if (fseek(infile, position, 0))
        return FALSE;

    int itemsRead =
        fread(this, sizeof(ITInstrumentHeader), 1, infile); 

    return (itemsRead > 0);
}


bool ITInstrumentHeader::Save(FILE* outfile, ITHeader* header, int headIndex,
                                long offset) {

    // writes to current filepos, or offset if specified.
    // if a header and headIndex is specified, the offset is updated to equal
    // the position where this header was written.

    if (offset >= 0)
        if (fseek(outfile, offset, 0))
            return FALSE;

    if (header != NULL)
        header->instrumentOffset[headIndex] = ftell(outfile);

    return (fwrite(this, sizeof(ITInstrumentHeader), 1, outfile) > 0);
}


void ITInstrumentHeader::Print(void) {

    char insName[26];
    ConvertITNameString(name, insName);

    printf("ins. name: %s\n", insName);
    printf("Header ID: %s\n", headerID);
    printf("DOS filename: %s\n", dosFileName);
    printf("nullbyte: %d\n", nullByte);
    printf("NNA: %d  DCT: %d  DCA: %d\n", newNoteAction, duplicateCheckType,
                duplicateCheckAction);

    printf("fadeOut: %d\n", fadeOut);
    printf("pitchPanSeperation: %d   pitchPanCenter: %d\n", 
                pitchPanSeperation, pitchPanCenter);
    printf("globalVolume: %d   defaultPan: %d\n", globalVolume, defaultPan);
    printf("randomVolVariation: %d   randomPanVariation: %d\n",
                randomVolumeVariation, randomPanningVariation);
    printf("trackerVersion: %d\n", trackerVersion);
    printf("numSamples assoc: %d\n", numSamples);
    printf("extraByte: %d  extraWord: %d\n", extraByte, extraWord);
    printf("midiChannel: %d   midiProgram: %d   midiBank: %d\n",
                midiChannel, midiProgram, midiBank);
    printf("\n");

    printf("noteTable: ");
    for (int i = 0; i < 120; i++) {
        printf("%d-%d  ", noteTable[i].note, noteTable[i].sample);
    }
    printf("\n\n");

    printf("volumeEnvelope - ");
    volumeEnvelope.Print();
    printf("panningEnvelope - ");
    panningEnvelope.Print();
    printf("pitchEnvelope - ");
    pitchEnvelope.Print();

    printf("\n");
}

//////////////////////////////////////////////////////////////////////////////////////  

/////////////////////////////////////////////////////////////////////////////////////


int ITSampleHeader::SampleSize(void) {
    // returns size of one sample, in bytes

    int size = 1;
    if (flags & 2) size = 2;
    if (flags & 4) size *= 2;
    return size;
}


int ITSampleHeader::LoopLength(void) {  // returns length of the sample loop, 0 if no loop.

    int result = loopEnd - loopBegin;
    if (result < 0)
        return 0;
    else
        return result;
}


bool ITSampleHeader::Load(FILE* infile, ITHeader* header, int headIndex,
                            long offset) {

    // if no header or offset specified, read from current position.
    // if no offset specified, use header and headIndex.
    // otherwise, read from offset.
    //
    // returns FALSE if does not load correctly.

    long position = ftell(infile);

    if (header == NULL) {
        if (offset >= 0)
            position = offset;
    }
    else {
        position = header->sampleOffset[headIndex];
    }

    if (fseek(infile, position, 0))
        return FALSE;

    int itemsRead =
        fread(this, sizeof(ITSampleHeader)-sizeof(BYTE*), 1, infile); 

    if ((length > 0) && (itemsRead > 0) && (sampleOffset > 0))
        return LoadSampleData(infile);
    else
        return (itemsRead > 0);
}


bool ITSampleHeader::LoadSampleData(FILE* infile) {

    if (length <= 0) // empty sample
        return TRUE;

    if (fseek(infile, sampleOffset, 0))
        return FALSE;

    delete[] sampleData;
    sampleData = NULL;


/*  
    // This is commented out for zt 
    // We dont use the sample data, so why bother allocating/loading it?

    sampleData = new BYTE[length * SampleSize()];
    if (sampleData == NULL)
        return FALSE;

    int itemsRead =
        fread(sampleData, length * SampleSize(), 1, infile);

    if (itemsRead == 0) {
        delete[] sampleData;
        sampleData = NULL;
    }
*/
    return 0;//(itemsRead > 0);
}


bool ITSampleHeader::Save(FILE* outfile, ITHeader* header, int headIndex,
                            long offset) {

    // writes to current filepos, or offset if specified.
    // if a header and headIndex is specified, the offset is updated to equal
    // the position where this header was written.

    if (offset >= 0)
        if (fseek(outfile, offset, 0))
            return FALSE;

    if (header != NULL)
        header->sampleOffset[headIndex] = ftell(outfile);

    return (fwrite(this, sizeof(ITSampleHeader), 1, outfile) > 0);
        
}


bool ITSampleHeader::SaveSampleData(FILE* outfile, ITHeader* header, int headIndex,
            bool updateSampleOffset, bool saveSampleOffset,
            long offset) {

    // writes to current filepos, or offset if specified.
    //
    // if updateSampleOffset, the sampleOffset of this SampleHeader is updated to
    // where the sample data was written.
    //
    // if saveSampleOffset, then if a header and headIndex is specified,
    // the sample header is written again to its current position (to save
    // updated sampleOffset value)
        
    if (offset >= 0)
        if (fseek(outfile, offset, 0))
            return FALSE;

    if (updateSampleOffset)
        sampleOffset = ftell(outfile);

    if ((saveSampleOffset) && (header != NULL)) {

        long curPosition = ftell(outfile);

        Save(outfile, NULL, 0, header->sampleOffset[headIndex]);

        if (fseek(outfile, curPosition, 0))
            return FALSE;

    }

    if (length < 1) 
        return TRUE;
    else

        return (fwrite(sampleData, SampleSize() * length, 1, outfile) > 0);
    
}


void ITSampleHeader::Print(void) {
        
    char sampleName[26];
    ConvertITNameString(name, sampleName);
    // file format uses /0 instead of spaces

    printf("name: %s\n", sampleName);
    printf("headerID: %s\n", headerID);
    printf("dosFileName: %s\n", dosFileName);
    printf("globalVolume: %d   defaultVolume: %d\n", globalVolume, defaultVolume);
    printf("flags: %d   convert: %d\n", flags, convert);
    printf("defaultPan: %d\n", defaultPan);
    printf("length: %d", length);
    printf("loopBegin: %d   loopEnd: %d   susLoopBegin: %d   susLoopEnd: %d\n",
                loopBegin, loopEnd, sustainLoopBegin, sustainLoopEnd);
    printf("C5speed: %d\n", C5speed);
    printf("sample data offset: %d\n", sampleOffset);
    printf("vibSpeed: %d   vibDepth: %d   vibRate: %d   vibType: %d\n\n",
                vibratoSpeed, vibratoDepth, vibratoRate, vibratoType);
}



///////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////


bool ITPattern::AddChannel(int rows, int num) {
    for (int i = 0; i < num; i++) {
        if (numChannels >= MAX_PATTERN_CHANNELS)
            return false;

        if (channels[numChannels] != NULL)
            delete channels[numChannels];
        channels[numChannels] = new ITChannelData(rows);
        
        numChannels++;
    }

    return true;
}


bool ITPattern::AddChannelsUpTo(int num, int rows) {

    // if the channel specified hasn't been allocated, allocate as many channels as needed

    // channels are zero-based, so numChannels must be at least one greater than the
    // required channel number

    while (numChannels <= num) {
        if (!AddChannel(rows))
            return false;
    }

    return true;
}





inline void ITPattern::SetChannelNote(BYTE channel, BYTE frame, BYTE value) {
    AddChannelsUpTo(channel, numRows);
    channels[channel]->SetNote(frame, value);
}

inline void ITPattern::SetChannelInstrument(BYTE channel, BYTE frame, BYTE value) {
    AddChannelsUpTo(channel, numRows);
    channels[channel]->SetInstrument(frame, value);
}

inline void ITPattern::SetChannelVolPan(BYTE channel, BYTE frame, BYTE value) {
    AddChannelsUpTo(channel, numRows);
    channels[channel]->SetVolPan(frame, value);
}

inline void ITPattern::SetChannelCommand(BYTE channel, BYTE frame, BYTE command, BYTE value) {
    AddChannelsUpTo(channel, numRows);
    channels[channel]->SetCommand(frame, command, value);
}


void ITPattern::PackTo(ITPackedPattern& packed) {

    BYTE channelVariable = 0;
    BYTE maskVariable = 0;

    int curPos = 0;

    ITRowData row;  // used to keep track of last value in channels
    BYTE lastMask[MAX_PATTERN_CHANNELS];

    for (int i = 0; i < MAX_PATTERN_CHANNELS; i++)
        lastMask[i] = 0;

    packed.numRows = numRows;

    BYTE data[MAX_PACKED_PATTERN_DATA];

    // according to ITTECH.DOC, the packed pattern data plus 8 byte header will always
    // be less than 64k.

    for (int frame = 0; frame < numRows; frame++) {

        for (int channel = 0; channel < numChannels; channel++) {

            channelVariable = channel + 1;
            maskVariable = 0;

            if (Note(channel, frame) > 0) {
                
                // does note equal last note for this channel?
                if (Note(channel, frame) == row[channel].note)
                    maskVariable = maskVariable | 16;
                else
                    maskVariable = maskVariable | 1;
            }
                                                                

            if (Instrument(channel, frame) > 0) {

                if (Instrument(channel, frame) == row[channel].instrument)
                    maskVariable = maskVariable | 32;
                else
                    maskVariable = maskVariable | 2;
            }

            
            if (VolPan(channel, frame) != PATTERN_NULL_VALUE) {

                if (VolPan(channel, frame) == row[channel].volPan)
                    maskVariable = maskVariable | 64;
                else
                    maskVariable = maskVariable | 4;
            }

            if ((Command(channel, frame) != PATTERN_NULL_VALUE) ||
                (CommandValue(channel, frame) != PATTERN_NULL_VALUE)) {

                if ((Command(channel, frame) == row[channel].command) &&
                    (CommandValue(channel, frame) == row[channel].commandValue))
                    maskVariable = maskVariable | 128;
                else
                    maskVariable = maskVariable | 8;
            }


            if (maskVariable != lastMask[channel])
                channelVariable = channelVariable | 128;

            // update "last" values for this channel
            if (maskVariable != 0)
                lastMask[channel] = maskVariable;
            if (Note(channel, frame) != 0)
                row[channel].note = Note(channel, frame);
            if (Instrument(channel, frame) != 0)
                row[channel].instrument = Instrument(channel, frame);
            if (VolPan(channel, frame) != PATTERN_NULL_VALUE)
                row[channel].volPan = VolPan(channel, frame);
            if (Command(channel, frame) != PATTERN_NULL_VALUE)
                row[channel].command = Command(channel, frame);
            if (CommandValue(channel, frame) != PATTERN_NULL_VALUE)
                row[channel].commandValue = CommandValue(channel, frame);

            if (maskVariable == 0)
                continue;  // no info needed.
            
            data[curPos] = channelVariable;
            curPos++;

            if (channelVariable & 128) {
                data[curPos] = maskVariable;
                curPos++;
            }


            if (maskVariable & 1) {
                data[curPos] = Note(channel, frame);
                curPos++;
            }

            if (maskVariable & 2) {
                data[curPos] = Instrument(channel, frame);
                curPos++;
            }

            if (maskVariable & 4) {
                data[curPos] = VolPan(channel, frame);
                curPos++;
            }

            if (maskVariable & 8) {
                data[curPos] = Command(channel, frame);
                curPos++;
                data[curPos] = CommandValue(channel, frame);
                curPos++;
            }

        } // end of row

        data[curPos] = 0;
        curPos++;

    } // end of pattern

    packed.SetNewData(curPos, data);

}

                


void ITPattern::Print(void) {
    printf("numRows: %d\n", numRows);
    printf("numChannels: %d\n\n", numChannels);
    for (int i = 0; i < numChannels; i++) {
        channels[i]->Print();
    }
}

////////////////////////////////////////////////////////////////

void ITPatternList::PackTo(ITPackedPatternList& packed) {
    for (int i = 0; i < numPatterns; i++) {
        patterns[i].PackTo(packed[i]);
    }
}


///////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////


void ITPackedPattern::SetNewData(int dataLength, BYTE* newData) {
    delete[] data;
    length = 0;

    data = new BYTE[dataLength];
    if (data != NULL) {
        for (int i = 0; i < dataLength; i++) {  //TODO: faster memory transfer
            data[i] = newData[i];
        }

        length = dataLength;
    }
}


bool ITPackedPattern::Load(FILE* infile, ITHeader* header, int headIndex,
                           long offset) {

    // if no header or offset specified, read from current position.
    // if no offset specified, use header and headIndex.
    // otherwise, read from offset.
    //
    // returns FALSE if does not load correctly.

    long position = ftell(infile);

    if (header == NULL) {
        if (offset >= 0)
            position = offset;
    }
    else {
        position = header->patternOffset[headIndex];
    }

    if (fseek(infile, position, 0))
        return FALSE;

    if (!(fread(this, 8, 1, infile)))
        return FALSE;

    data = new BYTE[length];
    if (data == NULL)
        return FALSE;

    int itemsRead =
        fread(data, length, 1, infile); 

    return (itemsRead > 0);
}


bool ITPackedPattern::Save(FILE* outfile, ITHeader* header, int headIndex,
                           long offset) {

    // writes to current filepos, or offset if specified.
    // if a header and headIndex is specified, the offset is updated to equal
    // the position where this header was written.

    if (offset >= 0)
        if (fseek(outfile, offset, 0))
            return FALSE;

    if (header != NULL)
        header->patternOffset[headIndex] = ftell(outfile);

    if (fwrite(this, 8, 1, outfile) < 1)
        return FALSE;

    return (fwrite(data, length, 1, outfile) > 0);
    
}



void ITPackedPattern::Print(void) {
    printf("length: %d\n", length);
    printf("numRows: %d\n", numRows);
    int cnt = 0;
    for (int i = 0; i < length; i++) {
        printf("%.3d ", data[i]);
        cnt++;
        if (cnt >= 19) {
            printf("\n");
            cnt = 0;
        }
    }
    printf("\n");
}


void ITPackedPattern::UnPackTo(ITPattern& pattern) {

    BYTE channelVariable = 0;
    BYTE channel = 0;
    BYTE maskVariable = 0;

    BYTE note = 0;

    int curPos = 0;
    bool done = FALSE;

    ITRowData currentRow;
    BYTE lastMask[MAX_PATTERN_CHANNELS];

    int frame = 0;

    pattern.numRows = numRows;
    //TODO: make sure channels are allocated with enough rows.

    while (!done) {

        channelVariable = 1;

        // unpack a row
        while ((channelVariable > 0) && (curPos <= length)) { 

            channelVariable = data[curPos];
            curPos++;

            if (channelVariable == 0)
                break;

            channel = (channelVariable - 1) & 63;

            if (channelVariable & 128) {
                maskVariable = data[curPos];
                curPos++;

                lastMask[channel] = maskVariable;
            }
            else {
                maskVariable = lastMask[channel];
            }

            if (maskVariable & 1) {
                currentRow.channel[channel].note = data[curPos];
                curPos++;

                pattern.SetChannelNote(channel, frame, currentRow.channel[channel].note);
            }

            if (maskVariable & 2) {
                currentRow.channel[channel].instrument = data[curPos];
                curPos++;

                pattern.SetChannelInstrument(channel, frame,
                                currentRow.channel[channel].instrument);
            }

            if (maskVariable & 4) {
                currentRow.channel[channel].volPan = data[curPos];
                curPos++;

                pattern.SetChannelVolPan(channel, frame,
                                currentRow.channel[channel].volPan);
            }

            if (maskVariable & 8) {
                currentRow.channel[channel].command = data[curPos];
                curPos++;
                currentRow.channel[channel].commandValue = data[curPos];
                curPos++;

                pattern.SetChannelCommand(channel, frame,
                                currentRow.channel[channel].command,
                                currentRow.channel[channel].commandValue);
            }

            if (maskVariable & 16) {
                pattern.SetChannelNote(channel, frame,
                                currentRow.channel[channel].note);
            }

            if (maskVariable & 32) {
                pattern.SetChannelInstrument(channel, frame,
                                currentRow.channel[channel].instrument);
            }

            if (maskVariable & 64) {
                pattern.SetChannelVolPan(channel, frame,
                                currentRow.channel[channel].volPan);
            }

            if (maskVariable & 128) {
                pattern.SetChannelCommand(channel, frame,
                                currentRow.channel[channel].command,
                                currentRow.channel[channel].commandValue);
            }

        }

        frame++;

        if ((frame >= numRows) || (curPos >= length))
            done = true;

    }       

}

///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


int ITInstrumentList::Save(FILE* outfile, ITHeader* header, long offset) {

    // if header is given, its offsets are updated to reflect the positions
    // where things are written.

    if ((outfile == NULL) || (instruments == NULL))
        return 0;

    if (offset >= 0) 
        if (fseek(outfile, offset, 0))
            return 0;

    int numSaved = 0;

    for (int i = 0; i < numInstruments; i++) {
        if (instruments[i].Save(outfile, header, i))
            numSaved++;
    }

    return numSaved;
}


int ITInstrumentList::Load(FILE* infile, ITHeader* header) {

    // returns number of instruments successfully loaded
    int numLoaded = 0;

    if ((header == NULL) || (infile == NULL) || (header->numIns <= 0))
        return 0;

    delete [] instruments;
    instruments = new ITInstrumentHeader[header->numIns];
    if (instruments == NULL)
        return numLoaded;

    for (int i = 0; i < header->numIns; i++) {

        if ((instruments[i].Load(infile, header, i))) {
            numLoaded++;
            numInstruments++;
        }
    }

    return numLoaded;
}
            


void ITInstrumentList::Print(void) {
    for (int i = 0; i < numInstruments; i++) {
        printf("instrument %d\n", i);
        instruments[i].Print();
    }
}


///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


int ITSampleList::Save(FILE* outfile, ITHeader* header, long offset) {

    // if header is given, its offsets are updated to reflect the positions
    // where things are written.

    if ((outfile == NULL) || (samples == NULL))
        return 0;

    if (offset >= 0) 
        if (fseek(outfile, offset, 0))
            return 0;

    int numSaved = 0;

    for (int i = 0; i < numSamples; i++) {
        if (samples[i].Save(outfile, header, i))
            numSaved++;
    }

    return numSaved;
}


int ITSampleList::SaveSampleData(FILE* outfile, ITHeader* header, long offset) {

    // if header is given, this updates the sampleOffset member of the associated
    // sampleHeaders, and resaves them in their current position, obtained from
    // the specified header.

    if ((outfile == NULL) || (samples == NULL))
        return 0;

    if (offset >= 0) 
        if (fseek(outfile, offset, 0))
            return 0;

    int numSaved = 0;

    for (int i = 0; i < numSamples; i++) {
        if (samples[i].SaveSampleData(outfile, header, i))
            numSaved++;
    }

    return numSaved;
}


int ITSampleList::Load(FILE* infile, ITHeader* header) {

    // returns number of samples successfully loaded
    int numLoaded = 0;

    if ((header == NULL) || (infile == NULL) || (header->numSamples <= 0))
        return 0;

    delete [] samples;
    samples = NULL;

    samples = new ITSampleHeader[header->numSamples+1];

    if (samples == NULL)
        return numLoaded;

    for (int i = 0; i < header->numSamples; i++) {

        if (samples[i].Load(infile, header, i)) {
            numLoaded++;
            numSamples++;
        }

    }

    return numLoaded;
}
            


void ITSampleList::Print(void) {
    for (int i = 0; i < numSamples; i++) {
        printf("sample %d\n", i);
        samples[i].Print();
    }
}


///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////


int ITPackedPatternList::Save(FILE* outfile, ITHeader* header, long offset) {

    // if header is given, its offsets are updated to reflect the positions
    // where things are written.

    if ((outfile == NULL) || (packedPatterns == NULL))
        return 0;

    if (offset >= 0) 
        if (fseek(outfile, offset, 0))
            return 0;

    int numSaved = 0;

    for (int i = 0; i < numPatterns; i++) {
        if (packedPatterns[i].Save(outfile, header, i))
            numSaved++;
    }

    return numSaved;
}


int ITPackedPatternList::Load(FILE* infile, ITHeader* header) {

    // returns number of packed patterns successfully loaded
    int numLoaded = 0;

    if ((header == NULL) || (infile == NULL) || (header-numPatterns <= 0))
        return 0;

    delete [] packedPatterns;
    packedPatterns = new ITPackedPattern[header->numPatterns];
    if (packedPatterns == NULL)
        return numLoaded;

    for (int i = 0; i < header->numPatterns; i++) {

        if (packedPatterns[i].Load(infile, header, i)) {
            numLoaded++;
            numPatterns++;
        }
    }

    return numLoaded;
}
            

void ITPackedPatternList::Print(void) {
    for (int i = 0; i < numPatterns; i++) {
        printf("pattern %d\n", i);
        packedPatterns[i].Print();
    }
}


void ITPackedPatternList::UnPackTo(ITPatternList& unpacked) {
    for (int i = 0; i < numPatterns; i++) {
        packedPatterns[i].UnPackTo(unpacked[i]);
    }
}



///////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////

ITModule::ITModule() {
    patterns = NULL;
}

ITModule::ITModule(int numPatterns) {
    patterns = new ITPatternList(numPatterns);
}

ITModule::~ITModule() {
    delete patterns;
}

void ITModule::AllocPatterns(int num) {
    delete patterns;
    patterns = new ITPatternList(num);
}


bool ITModule::LoadModule(const char* fileName, int unpack) {

    FILE* infile = fopen(fileName, "rb");
    if (infile == NULL) return FALSE;

    bool success = TRUE;

    if (!header.Load(infile))
        success = FALSE;

    if (success && instruments.Load(infile, &header) < header.numIns)
        success = FALSE;

    if (success && samples.Load(infile, &header) < header.numSamples)
        success = FALSE;

    // if (success && packedPatterns.Load(infile, &header) < header.numPatterns)
    success = TRUE;
    if (packedPatterns.Load(infile, &header) < header.numPatterns)
        success = FALSE;

    fclose(infile);

    if (success) {
        //TODO: AllocPatterns even if not unpacking now?
        AllocPatterns(header.numPatterns);

        if (unpack == UNPACK_PATTERNS)
            UnpackPatterns();
    }

    return success;
}


bool ITModule::SaveModule(const char* fileName, int pack) {

    FILE* outfile = fopen(fileName, "wb");
    if (outfile == NULL) return FALSE;

    bool success = TRUE;

    if (pack == PACK_PATTERNS)
        PackPatterns();

    if (!header.Save(outfile))
        success = FALSE;

    if (!instruments.Save(outfile, &header))
        success = FALSE;

    if (!samples.Save(outfile, &header))
        success = FALSE;

    if (!packedPatterns.Save(outfile, &header))
        success = FALSE;

    if (!samples.SaveSampleData(outfile, &header))
        success = FALSE;

    if (!header.SaveOffsets(outfile))
        success = FALSE;

    fclose(outfile);

    return success;
}


void ITModule::Print(void) {
    header.Print();
    instruments.Print();
    samples.Print();
    packedPatterns.Print();
}


inline void ITModule::UnpackPatterns(void) {
    packedPatterns.UnPackTo(*patterns);
}

inline void ITModule::PackPatterns(void) {
    patterns->PackTo(packedPatterns);
}


bool ITModule::GrowIfNeeded(int pattern, int channel, int row) {
    // increase number of patterns, channels, or rows as necessary
    // to have an event on specified row of specified channel of specified pattern.
    // returns true if successful, false if unable to grow to specs.

    return 0;
}


