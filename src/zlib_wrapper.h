/*************************************************************************
 *
 * FILE  zlib_wrapper.h
 * $Id: zlib_wrapper.h,v 1.4 2001/07/29 18:44:14 tlr Exp $
 *
 * DESCRIPTION 
 *   wrapper for using zlib with streams.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
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
#ifndef _ZLIB_WRAPPER_H
#define _ZLIB_WRAPPER_H

#include <zlib.h>
#include <fstream>

/* buffer size */
#define BUFFER_SIZE 1024


class InflateStream
{
    public:
      InflateStream(std::ifstream *ifs);
        ~InflateStream();
        void read(char *data, int size);
        int eof(void);
        int fail(void);
    private:
        std::ifstream *ifs;
        int errornum;
        int eofflag;
        z_stream zstrm;
        unsigned char input_buffer[BUFFER_SIZE];
};

class DeflateStream
{
    public:
        DeflateStream(std::ofstream *ofs);
        ~DeflateStream();
        void write(char *data, int size);
        int fail(void);
    private:
        std::ofstream *ofs;
        int errornum;
        z_stream zstrm;
        unsigned char output_buffer[BUFFER_SIZE];
};

#endif /* _ZLIB_WRAPPER_H */
