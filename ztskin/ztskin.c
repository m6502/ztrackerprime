/*************************************************************************
 *
 * FILE  skin.c
 * $Id: ztskin.c,v 1.1 2001/07/22 19:55:01 tlr Exp $
 *
 * DESCRIPTION 
 *   The skin compiler/decompiler.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* these must be present to get the win32 dll linkage working */
#define ZLIB_DLL
#define _WINDOWS
#include <zlib.h>

/* read/write buffer size */
#define BUFFER_SIZE 1024

/* max length of a name inside the skin file */
#define NAMEBUFSIZE 256

#define PROGRAM "ztskin"

int make_skin(char *skinfile, int num, char *filenames[]);
int explode_skin(char *skinname);

int copyfile (FILE *fin, int len, FILE *fout);
int packfile(FILE *fin, int len, FILE *fout);
int unpackfile(FILE *fin, int len, FILE *fout);

unsigned fgetl(FILE *infp);
int fputl(unsigned data, FILE *outfp);

void warning(const char *str, ...);
void panic(const char *str, ...);


/*************************************************************************
 *
 * NAME  main()
 *
 * SYNOPSIS
 *   ret = main(argc, argv)
 *   int main(int, char **)
 *
 * DESCRIPTION
 *   The entry point of the C-code.
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
int main(int argc, char *argv[])
{
    int n;
    int explode=0;
    char *skinname;

    if (argc < 2)
        panic("too few arguments (use '-h' for help)");

    /* scan for options, stop at the first non option */
    for (n=1; n<argc; n++) {
        if (argv[n][0]=='-') {
            switch (argv[n][1]) {
            case '?':
            case 'h':
                fprintf (stdout,
"usage: " PROGRAM " [OPTION]... <skinfile> [<infile>] ...\n"
"\n"
"Valid options:\n"
"    -e              explode skin file\n"
"    -h              displays this help text\n");
    	        exit (0);
                break;
            case 'e':
                explode=1;
                break;
            }
        } else {
            break;
        }
    }

    if (n >= argc)
        panic("too few arguments");
    skinname=argv[n];
    n++;

    if (explode) {
        explode_skin(skinname);
        exit(0);
    }

    make_skin(skinname,argc-n,&argv[n]);

    exit(0);
}

/*************************************************************************
 *
 * NAME  make_skin()
 *
 * SYNOPSIS
 *   ret = make_skin (skinname, num, filenames)
 *   int explode_skin (char *, int, char **)
 *
 * DESCRIPTION
 *   Read the skin file 'name' and explode it into its parts.
 *
 * INPUTS
 *   skinname               - output file name
 *   num                    - number of files to input
 *   filenames              - names of files to input
 *           
 * RESULT    
 *   ret                    - status
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
int make_skin(char *skinname, int num, char *filenames[])
{
    FILE *fin,*fout;
    int i;
    int len;
    int headpos,datapos,realsize,compressedsize,nextpos;
    char *filename;

    printf("creating skin file '%s'...\n",skinname);
    if (!(fout=fopen(skinname,"wb")))
        panic("can't open file '%s'",skinname);


    for (i=0; i<num; i++) {
        filename=filenames[i];

        printf("'%s'\n",
            filename);
        len=strlen(filename);
        fputl(len,fout);
        fwrite(filename,1,len,fout);
        headpos=ftell(fout);
        fputl(0,fout);
        fputl(0,fout);
        fputl(0,fout);
        fputl(0,fout);
        datapos=ftell(fout);

        /* pack the payload */
        if (!(fin=fopen(filename,"rb")))
            panic("can't open file '%s'",filename);

        packfile(fin,0,fout);
        nextpos=ftell(fout);
        realsize=ftell(fin);
        compressedsize=nextpos-datapos;

        fseek(fout, headpos, SEEK_SET);
        fputl(datapos,fout);
        fputl(realsize,fout);
        fputl(compressedsize,fout);  
        fputl(1,fout);              /* compressed flag */
        fseek(fout, nextpos, SEEK_SET);

        if (ferror(fin))
            panic("read error");
        if (ferror(fout))
            panic("write error");

        fclose(fin);
    }

    /* done, check if there where errors */
    if (ferror(fout))
        panic("write error");

    /* nope, all ok, close and return */
    fclose(fout);
    return 0;
}


/*************************************************************************
 *
 * NAME  explode_skin()
 *
 * SYNOPSIS
 *   ret = explode_skin (skinname)
 *   int explode_skin (char *)
 *
 * DESCRIPTION
 *   Read the skin file 'skinname' and explode it into its parts.
 *
 * INPUTS
 *   skinname               - input file name
 *           
 * RESULT    
 *   ret                    - status
 *           
 * KNOWN BUGS
 *   none
 *           
 ******/     
int explode_skin(char *skinname)
{
    FILE *fin,*fout;
    unsigned namelen,dataoffset,realsize,compressedsize,compressedflag;
    char namebuf[NAMEBUFSIZE];

    printf("exploding skin file '%s'...\n",skinname);
    if (!(fin=fopen(skinname,"rb")))
        panic("can't open file '%s'",skinname);

    while (!(feof(fin) || ferror(fin))) {
        /* get the header */
        namelen=fgetl(fin);
        if (feof(fin) || ferror(fin))
            break;
        if (namelen > (NAMEBUFSIZE-1))
            panic("namelen too big");
           
        fread(namebuf,1,namelen,fin);
        namebuf[namelen]=0;
        dataoffset=fgetl(fin);
        realsize=fgetl(fin);
        compressedsize=fgetl(fin);
        compressedflag=fgetl(fin);

        /* seek the payload */
        fseek(fin,dataoffset,SEEK_SET);

        /* bailout if anything went wrong */
        if (feof(fin) || ferror(fin))
            break;

        /* print header info */
        if (compressedflag)
            printf("'%s', %d bytes (%d bytes compressed)\n",
                   namebuf,realsize,compressedsize);
        else
            printf("'%s', %d bytes\n",namebuf,realsize);

        /* output the payload */
        if (!(fout=fopen(namebuf,"wb")))
            panic("can't open file '%s'",namebuf);

        if (compressedflag) {
        /* decompress the block */
            unpackfile(fin,compressedsize,fout);
        } else {
        /* otherwise just copy the data */
            copyfile(fin,compressedsize,fout);
        }
        fclose(fout);
        if (ferror(fin))
            panic("read error");
        if (ferror(fout))
            panic("write error");

        /* seek next block */
        fseek(fin, compressedsize+dataoffset, SEEK_SET);
    }

    /* done, check if there where errors */
    if (ferror(fin))
        panic("read error");

    /* nope, all ok, close and return */
    fclose(fin);
    return 0;
}

/*************************************************************************
 *
 * NAME  copyfile()
 *
 * SYNOPSIS
 *   ret = copyfile (fin, len, fout)
 *   int copyfile (FILE *, int, FILE *)
 *
 * DESCRIPTION
 *   copy 'len' bytes from fin to fout.
 *
 * INPUTS
 *   fin                    - input file
 *   len                    - number of bytes to input (0 means until EOF)
 *   fout                   - output file
 *           
 * RESULT    
 *   ret                    - status (0=OK)
 *           
 * KNOWN BUGS
 *   len==0 does not produce the correct result
 *           
 ******/     
int copyfile (FILE *fin, int len, FILE *fout)
{
    char copy_buffer[BUFFER_SIZE];
    int count;
    
    while (len || !feof(fin)) {
        count = fread( copy_buffer,
                       1,
                       (len<BUFFER_SIZE)?len:BUFFER_SIZE,
                       fin );
        fwrite( copy_buffer, 1, count, fout );
        len-=count;
    }
    return 0;
}

/*************************************************************************
 *
 * NAME  packfile()
 *
 * SYNOPSIS
 *   ret = packfile (fin, len, fout)
 *   int packfile (FILE *, int, FILE *)
 *
 * DESCRIPTION
 *   Pack 'len' bytes from fin to fout.
 *
 * INPUTS
 *   fin                    - input file
 *   len                    - number of bytes to input (0 means until EOF)
 *   fout                   - output file
 *           
 * RESULT    
 *   ret                    - status (0=OK)
 *           
 * KNOWN BUGS
 *   len==0 does not produce the correct result
 *           
 ******/     
int packfile(FILE *fin, int len, FILE *fout)
{
    z_stream zstrm;
    char input_buffer[BUFFER_SIZE];
    char output_buffer[BUFFER_SIZE];
    int count,status;

    zstrm.zalloc=Z_NULL;
    zstrm.zfree=Z_NULL;
    zstrm.opaque=Z_NULL;

    status=deflateInit(&zstrm,Z_DEFAULT_COMPRESSION);
    if (status!=Z_OK)
	return status;

    zstrm.avail_in = 0;
    zstrm.next_out = output_buffer;
    zstrm.avail_out = BUFFER_SIZE;
    while (!(ferror(fin) || ferror(fout))) {
        if ( zstrm.avail_in == 0 ) {
            zstrm.next_in = input_buffer;
            zstrm.avail_in = fread( input_buffer, 1, BUFFER_SIZE, fin );
        }
        if ( zstrm.avail_in != 0 ) {
	    status = deflate( &zstrm, Z_NO_FLUSH );
	} else {
	    status = deflate( &zstrm, Z_FINISH );
	}
        count = BUFFER_SIZE - zstrm.avail_out;
        if ( count )
            fwrite( output_buffer, 1, count, fout );
        zstrm.next_out = output_buffer;
        zstrm.avail_out = BUFFER_SIZE;
	if (status!=Z_OK)
	    break;
    }    
    deflateEnd(&zstrm);

    if (status==Z_STREAM_END)
	return Z_OK;
    return status;
}

/*************************************************************************
 *
 * NAME  unpackfile()
 *
 * SYNOPSIS
 *   ret = unpackfile (fin, len, fout)
 *   int unpackfile (FILE *, int, FILE *)
 *
 * DESCRIPTION
 *   Unpack 'len' bytes from fin to fout.
 *
 * INPUTS
 *   fin                    - input file
 *   len                    - number of bytes to input (0 means until EOF)
 *   fout                   - output file
 *           
 * RESULT    
 *   ret                    - status (0=OK)
 *           
 * KNOWN BUGS
 *   len==0 does not produce the correct result
 *           
 ******/     
int unpackfile(FILE *fin, int len, FILE *fout)
{
    z_stream zstrm;
    char input_buffer[BUFFER_SIZE];
    char output_buffer[BUFFER_SIZE];
    int count,status;

    zstrm.next_in=Z_NULL;
    zstrm.avail_in=0;
    zstrm.zalloc=Z_NULL;
    zstrm.zfree=Z_NULL;
    zstrm.opaque=Z_NULL;

    status=inflateInit(&zstrm);
    if (status!=Z_OK)
	return status;

    zstrm.avail_in = 0;
    zstrm.next_out = output_buffer;
    zstrm.avail_out = BUFFER_SIZE;
    while (!(ferror(fin) || ferror(fout))) {
        if ( zstrm.avail_in == 0 ) {
            zstrm.next_in = input_buffer;
            zstrm.avail_in = fread( input_buffer, 1, BUFFER_SIZE, fin );
        }
        if ( zstrm.avail_in != 0 ) {
	    status = inflate( &zstrm, Z_NO_FLUSH );
	} else {
	    status = inflate( &zstrm, Z_FINISH );
	}
        count = BUFFER_SIZE - zstrm.avail_out;
        if ( count )
            fwrite( output_buffer, 1, count, fout );
        zstrm.next_out = output_buffer;
        zstrm.avail_out = BUFFER_SIZE;
	if (status!=Z_OK)
	    break;
    }    
    inflateEnd(&zstrm);
    if (status==Z_STREAM_END)
	return Z_OK;
    return status;
}


/*************************************************************************
 *
 * NAME  fgetl()
 *
 * SYNOPSIS
 *   data = fgetl (fin)
 *   unsigned fgetl (FILE *)
 *
 * DESCRIPTION
 *   Get a 32-bit (little endian) word from fin.
 *   Errors must be determined by checking feof() & ferror()
 *
 * INPUTS
 *   fin                    - input file
 *           
 * RESULT    
 *   data                   - the read data
 *           
 * KNOWN BUGS      
 *   none
 *           
 ******/     
unsigned fgetl(FILE *fin)
{
    unsigned tmp;
    tmp=fgetc(fin);
    tmp|=(fgetc(fin))<<8;
    tmp|=(fgetc(fin))<<16;
    tmp|=(fgetc(fin))<<24;
    return tmp;
}

/*************************************************************************
 *
 * NAME  fputl()
 *
 * SYNOPSIS
 *   ret = fputl (data, fout)
 *   unsigned fgetl (FILE *)
 *
 * DESCRIPTION
 *   Put a 32-bit (little endian) word to fout.
 *   Errors must be determined by checking feof() & ferror()
 *
 * INPUTS
 *   data                   - data to write
 *   fin                    - input file
 *           
 * RESULT    
 *   ret                    - always 0
 *           
 * KNOWN BUGS      
 *   none
 *           
 ******/     
int fputl(unsigned data, FILE *fout)
{
    fputc((data & 0xff),fout);
    fputc(((data>>8) & 0xff),fout);
    fputc(((data>>16) & 0xff),fout);
    fputc(((data>>24) & 0xff),fout);
    return 0;
}

/*************************************************************************
 *
 * NAME  warning()
 *
 * SYNOPSIS
 *   warning (str, ...)
 *   void warning (const char *, ...)
 *
 * DESCRIPTION
 *   output warning message prepended with 'Warning: ' and appended
 *   with '\n'.
 *
 * INPUTS
 *   str              - format string
 *   ...              - vararg parameters
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
void warning (const char *str, ...)
{
    va_list args;

    fprintf (stderr, "Warning: ");
    va_start (args, str);
    vfprintf (stderr, str, args);
    va_end (args);
    fputc ('\n', stderr);
}

/*************************************************************************
 *
 * NAME  panic()
 *
 * SYNOPSIS
 *   panic (str, ...)
 *   void panic (const char *, ...)
 *
 * DESCRIPTION
 *   output error message prepended with 'PROGRAM: ' and appended
 *   with '\n'.
 *
 * INPUTS
 *   str              - format string
 *   ...              - vararg parameters
 *
 * RESULT
 *   none
 *
 * KNOWN BUGS
 *   none
 *
 ******/
void panic (const char *str, ...)
{
    va_list args;

    fprintf (stderr, PROGRAM ": ");
    va_start (args, str);
    vfprintf (stderr, str, args);
    va_end (args);
    fputc ('\n', stderr);
    exit (1);
}
/* eof */
