/*************************************************************************
 *
 * FILE  CDataBuf.cpp
 * $Id: CDataBuf.cpp,v 1.8 2001/10/16 05:45:06 cmicali Exp $
 *
 * DESCRIPTION 
 *   Generic data buffer.
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

#include "zt.h"


CDataBuf::CDataBuf() {
    this->buffer = NULL;
    this->buffsize = 0;
    this->allocsize = 0;
    this->read_cursor = 0;
}

CDataBuf::~CDataBuf() {
    this->flush();
}

int CDataBuf::isempty() {
    return (this->getsize() > 0);
}

void CDataBuf::write(const char *buf, int size) {
    if (size>DATABUF_CHUNK_SIZE)
        SDL_Delay(100);
    if (this->buffsize+size > this->allocsize) {
        while(this->allocsize < this->buffsize+size)
            this->allocsize+=DATABUF_CHUNK_SIZE;
        this->buffer = (char *)realloc(this->buffer, this->allocsize);
    }
    for(int i=0;i<size;i++) {
        this->buffer[this->buffsize] = buf[i];
        ++this->buffsize;
    }
}
void CDataBuf::pushuc(const unsigned char c) {
    if (this->buffsize+sizeof(char) > this->allocsize) {
        while(this->allocsize < this->buffsize+sizeof(char))
            this->allocsize+=DATABUF_CHUNK_SIZE;
        this->buffer = (char *)realloc(this->buffer, this->allocsize);
    }
    this->buffer[this->buffsize] = c;
    ++this->buffsize;
}

void CDataBuf::pushc(const char c) {
    if (this->buffsize+sizeof(char) > this->allocsize) {
        while(this->allocsize < this->buffsize+sizeof(char))
            this->allocsize+=DATABUF_CHUNK_SIZE;
        this->buffer = (char *)realloc(this->buffer, this->allocsize);
    }
    this->buffer[this->buffsize] = c;
    ++this->buffsize;
}

void CDataBuf::popc(void) {
    if (this->buffsize-sizeof(char) < this->allocsize-DATABUF_CHUNK_SIZE-1) {
        while(this->buffsize-sizeof(char) < this->allocsize-DATABUF_CHUNK_SIZE-1)
            this->allocsize -= DATABUF_CHUNK_SIZE;
        this->buffer = (char *)realloc(this->buffer, this->allocsize);
    }
    if (this->buffsize>0) {
        --this->buffsize;
    }
}

void CDataBuf::pushstr(const char *str) {
    int i = 0;
    while(str[i])
        this->pushc(str[i++]);
}

void CDataBuf::pushc(const int c) {
    this->pushc((const char) c);
}

void CDataBuf::pushsi(const short int si) {
    this->write((const char *)&si,sizeof(short int));
}

void CDataBuf::pushusi(const unsigned short int usi) {
    this->write((const char *)&usi,sizeof(unsigned short int));
}

void CDataBuf::pushi(const int i) {
    this->write((const char *)&i,sizeof(int));
}

void CDataBuf::pushui(const unsigned int ui) {
    this->write((const char *)&ui,sizeof(unsigned int));
}


void CDataBuf::flush(void) {
    if (this->buffer)
        free(this->buffer);
    this->buffer = NULL;
    this->buffsize = 0;
    this->allocsize = 0;
    this->read_cursor = 0;
}

char *CDataBuf::getbuffer(void) {
    return this->buffer;
}

int CDataBuf::getsize(void) {
    return this->buffsize;
}

void CDataBuf::setbufsize(int size) {
    this->allocsize = size;
    this->buffsize = size;
    this->read_cursor = 0;
    this->buffer = (char *)realloc(this->buffer, this->allocsize);
}

char CDataBuf::getch(void) {
    char c;
    if (read_cursor <= buffsize-sizeof(char) ) {
        c = this->buffer[read_cursor];
        read_cursor+=sizeof(char);
        return c;
    }
    return NULL;
}

unsigned char CDataBuf::getuch(void) {
    unsigned char c;
    if (read_cursor <= buffsize-sizeof(unsigned char) ) {
        c = this->buffer[read_cursor];
        read_cursor+=sizeof(unsigned char);
        return c;
    }
    return NULL;
}


short int CDataBuf::getsi(void) {
    short int c;
    if (read_cursor <= buffsize-sizeof(short int) ) {
        memcpy(&c,&this->buffer[read_cursor],sizeof(short int));
        read_cursor+=sizeof(short int);
        return c;
    }
    return NULL;
}
unsigned short int CDataBuf::getusi(void) {
    unsigned short int c;
    if (read_cursor <= buffsize-sizeof(unsigned short int) ) {
        memcpy(&c,&this->buffer[read_cursor],sizeof(short int));
        read_cursor+=sizeof(unsigned short int);
        return c;
    }
    return NULL;
}
int CDataBuf::geti(void) {
    int c;
    if (read_cursor <= buffsize-sizeof(int) ) {
        memcpy(&c,&this->buffer[read_cursor],sizeof(int));
        read_cursor+=sizeof(int);
        return c;
    }
    return NULL;
}
unsigned int CDataBuf::getui(void) {
    unsigned int c;
    if (read_cursor <= buffsize-sizeof(unsigned int) ) {
        memcpy(&c,&this->buffer[read_cursor],sizeof(unsigned int));
        read_cursor+=sizeof(unsigned int);
        return c;
    }
    return NULL;
}

void CDataBuf::reset_read(void) {
    read_cursor = 0;
}

int CDataBuf::eob(void) {
    if (read_cursor>=buffsize)
        return 1;
    else
        return 0;
}

void CDataBuf::seek(int size, int mode) {
    switch(mode) {
        case CDB_SEEK_FROMSTART:
            this->read_cursor = size;
            break;
        case CDB_SEEK_FROMEND:
            this->read_cursor = this->buffsize - size;
            break;
        case CDB_SEEK_FROMCUR:
            this->read_cursor += size;
            break;
    }
    if (this->read_cursor<0) this->read_cursor = 0;
    if (this->read_cursor>=this->buffsize) this->read_cursor = this->buffsize;
}