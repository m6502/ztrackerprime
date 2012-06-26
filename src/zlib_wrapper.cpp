/*************************************************************************
 *
 * FILE  zlib_wrapper.cpp
 * $Id: zlib_wrapper.cpp,v 1.5 2001/07/29 18:44:14 tlr Exp $
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
#include "zt.h"


/*************************************************************************
 *
 * NAME  InflateStream::InflateStream()
 *
 * SYNOPSIS
 *   InflateStream::InflateStream (ifs)
 *   InflateStream::InflateStream (ifstream &)
 *
 * DESCRIPTION
 *   Constructor for InflateStream, assign the ifstream ifs to be inflated
 *
 * INPUTS
 *   ifs                    - an ifstream
 *           
 * RESULT    
 *   none
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
InflateStream::InflateStream(std::ifstream *ifs)
{
    int status;
    this->errornum=0;
    this->eofflag=0;
    this->ifs=ifs;

    this->zstrm.next_in=Z_NULL;
    this->zstrm.avail_in=0;
    this->zstrm.zalloc=Z_NULL;
    this->zstrm.zfree=Z_NULL;
    this->zstrm.opaque=Z_NULL;

    status=inflateInit(&this->zstrm);
    if (status!=Z_OK)
        this->errornum=1;

}

/*************************************************************************
 *
 * NAME  InflateStream::~InflateStream()
 *
 * SYNOPSIS
 *   InflateStream::~InflateStream ()
 *   InflateStream::~InflateStream ()
 *
 * DESCRIPTION
 *   Destructor for InflateStream, free up resources associated with
 *   inflating.
 *
 * INPUTS
 *   none
 *           
 * RESULT    
 *   none
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
InflateStream::~InflateStream()
{
    inflateEnd(&this->zstrm);
}

/*************************************************************************
 *
 * NAME  InflateStream::read()
 *
 * SYNOPSIS
 *   ret = InflateStream::read (data, size)
 *   int InflateStream::read (char *, int)
 *
 * DESCRIPTION
 *   Read the skin file 'skinname' and explode it into its parts.
 *
 * INPUTS
 *   data                   - pointer to a buffer of atleast the size 'size'.
 *   size                   - number of bytes to read
 *           
 * RESULT    
 *   ret                    - status
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
void InflateStream::read(char *data, int size)
{
    int status = 0, pos ;

    this->zstrm.next_out = (unsigned char *)data;
    this->zstrm.avail_out = size;
    
    while (!( this->zstrm.avail_out == 0)) {

        if ( !this->ifs->eof() && this->zstrm.avail_in == 0 ) {

            this->zstrm.next_in = this->input_buffer;
            pos = this->ifs->tellg();
            this->ifs->read( (char *)this->input_buffer, BUFFER_SIZE );
            

            // this->zstrm.avail_in = this->ifs.tellg() - pos; // Pre-VS2008

            int current_pos = this->ifs->tellg() ;
            this->zstrm.avail_in = current_pos - pos;


        }   
        status = inflate( &this->zstrm, Z_SYNC_FLUSH );
        if (status!=Z_OK )
            break;
    }    
    if (status==Z_STREAM_END) {
        this->eofflag=1;
    } else if (status!=Z_OK) {
        this->errornum=1;
    }

}

/*************************************************************************
 *
 * NAME  InflateStream::eof()
 *
 * SYNOPSIS
 *   ret = InflateStream::eof ()
 *   int InflateStream::eof (void)
 *
 * DESCRIPTION
 *   Return the end of file flag for the stream.
 *
 * INPUTS
 *   none
 *           
 * RESULT    
 *   ret                    - TRUE=eof
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
int InflateStream::eof(void)
{
    return this->eofflag;
}

/*************************************************************************
 *
 * NAME  InflateStream::fail()
 *
 * SYNOPSIS
 *   ret = InflateStream::fail ()
 *   int InflateStream::fail (void)
 *
 * DESCRIPTION
 *   Return the error code for the stream
 *
 * INPUTS
 *   none
 *           
 * RESULT    
 *   ret                    - 0 = no error
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
int InflateStream::fail()
{
    return this->errornum;
}



/*************************************************************************
 *
 * NAME  DeflateStream::DeflateStream()
 *
 * SYNOPSIS
 *   DeflateStream::DeflateStream (ofs)
 *   DeflateStream::DeflateStream (ofstream &)
 *
 * DESCRIPTION
 *   Constructor for DeflateStream, assign the ofstream ofs to be deflated
 *
 * INPUTS
 *   ofs                    - an ofstream
 *           
 * RESULT    
 *   none
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
DeflateStream::DeflateStream(std::ofstream *ofs)
{
    int status;
    this->errornum=0;
    this->ofs=ofs;

    this->zstrm.next_out=this->output_buffer;
    this->zstrm.avail_out=BUFFER_SIZE;
    this->zstrm.opaque=Z_NULL;
    this->zstrm.zalloc=Z_NULL;
    this->zstrm.zfree=Z_NULL;
    this->zstrm.opaque=Z_NULL;

    status=deflateInit(&this->zstrm,Z_DEFAULT_COMPRESSION);
    if (status!=Z_OK)
        this->errornum=1;

}

/*************************************************************************
 *
 * NAME  DeflateStream::~DeflateStream()
 *
 * SYNOPSIS
 *   DeflateStream::~DeflateStream ()
 *   DeflateStream::~DeflateStream ()
 *
 * DESCRIPTION
 *   Destructor for DeflateStream, free up resources associated with
 *   deflating.
 *
 * INPUTS
 *   none
 *           
 * RESULT    
 *   none
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
DeflateStream::~DeflateStream()
{
    this->write(NULL,0);
    deflateEnd(&this->zstrm);
}

void DeflateStream::write(char *data, int size)
{
    int status=0,flush;

    flush=(size)?Z_NO_FLUSH:Z_FINISH;
 
    this->zstrm.next_in = (unsigned char *)data;
    this->zstrm.avail_in = size;
    while (this->zstrm.avail_in != 0 || flush==Z_FINISH) {
        if ( this->zstrm.avail_out == 0 ) {
            this->ofs->write( (char *)this->output_buffer, BUFFER_SIZE );
            this->zstrm.next_out=this->output_buffer;
            this->zstrm.avail_out=BUFFER_SIZE;
        }   
        status = deflate( &this->zstrm, flush );
        if (status!=Z_OK )
            break;
    }    
    if (status==Z_STREAM_END) {
        this->ofs->write( (char *)this->output_buffer,
                         BUFFER_SIZE - this->zstrm.avail_out ); // does this work with 0?
    } else if (status!=Z_OK) {
        this->errornum=1;
    }
}

/*************************************************************************
 *
 * NAME  DeflateStream::fail()
 *
 * SYNOPSIS
 *   ret = DeflateStream::fail ()
 *   int DeflateStream::fail (void)
 *
 * DESCRIPTION
 *   Return the error code for the stream
 *
 * INPUTS
 *   none
 *           
 * RESULT    
 *   ret                    - 0 = no error
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
int DeflateStream::fail()
{
    return this->errornum;
}

/* eof */
