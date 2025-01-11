#ifndef _MIDI_STREAM_H
#define _MIDI_STREAM_H

#include <mmsystem.h>

#define CMS_BufferLength 10*1024

class CMidiStream
{
    public:
        MIDIHDR midihdr;
        
        LPSTR buffer;
        int buffer_size;


        CMidiStream();
        ~CMidiStream();
        void clear(void);
        MIDIHDR *get_midihdr(void);
};

#endif