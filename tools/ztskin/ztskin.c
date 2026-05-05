/*************************************************************************
 *
 * FILE  ztskin.c
 *
 * DESCRIPTION
 *   The skin compiler/decompiler. CLI utility that packs a set of files
 *   into a zT skin archive (with per-entry zlib compression), or
 *   "explodes" a skin archive back into the constituent files.
 *
 *   File format (all 32-bit fields little-endian):
 *     repeat for each entry:
 *       u32   namelen
 *       u8[]  name (namelen bytes, no terminator)
 *       u32   dataoffset       absolute offset of the payload
 *       u32   realsize         uncompressed length
 *       u32   compressedsize   bytes between dataoffset and next entry
 *       u32   compressedflag   1 = zlib-compressed, 0 = raw
 *       u8[compressedsize] payload
 *
 * This file is part of zTracker Prime - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2001, Daniel Kahlin <tlr@users.sourceforge.net>
 * Multiplatform port (c) 2026, Manuel Montoto / zTracker Prime contributors.
 * BSD-style license. See repository LICENSE.txt.
 *
 ******/
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#define BUFFER_SIZE  1024
#define NAMEBUFSIZE  256
#define PROGRAM      "ztskin"

static int make_skin(const char *skinfile, int num, char *filenames[]);
static int explode_skin(const char *skinname);

static int copyfile(FILE *fin, int len, FILE *fout);
static int packfile(FILE *fin, FILE *fout);
static int unpackfile(FILE *fin, FILE *fout);

static unsigned fgetl(FILE *infp);
static int      fputl(unsigned data, FILE *outfp);

static void panic(const char *str, ...);


int main(int argc, char *argv[])
{
    int n;
    int explode = 0;
    const char *skinname;

    if (argc < 2)
        panic("too few arguments (use '-h' for help)");

    /* scan for options, stop at the first non-option */
    for (n = 1; n < argc; n++) {
        if (argv[n][0] == '-') {
            switch (argv[n][1]) {
            case '?':
            case 'h':
                fprintf(stdout,
                    "usage: " PROGRAM " [OPTION]... <skinfile> [<infile>]...\n"
                    "\n"
                    "Pack <infile>s into <skinfile>, or with -e explode\n"
                    "<skinfile> back into its constituent files (written\n"
                    "to the current working directory).\n"
                    "\n"
                    "Valid options:\n"
                    "    -e              explode skin file\n"
                    "    -h              displays this help text\n");
                return 0;
            case 'e':
                explode = 1;
                break;
            default:
                panic("unknown option '%s'", argv[n]);
            }
        } else {
            break;
        }
    }

    if (n >= argc)
        panic("too few arguments");
    skinname = argv[n];
    n++;

    if (explode)
        return explode_skin(skinname);

    return make_skin(skinname, argc - n, &argv[n]);
}


/* Pack each file in `filenames` into a new skin at `skinname`. */
static int make_skin(const char *skinname, int num, char *filenames[])
{
    FILE *fin, *fout;
    int i;
    int len;
    long headpos, datapos, nextpos;
    long realsize, compressedsize;
    const char *filename;

    printf("creating skin file '%s'...\n", skinname);
    if (!(fout = fopen(skinname, "wb")))
        panic("can't open file '%s'", skinname);

    for (i = 0; i < num; i++) {
        filename = filenames[i];

        printf("'%s'\n", filename);
        len = (int)strlen(filename);
        fputl((unsigned)len, fout);
        fwrite(filename, 1, (size_t)len, fout);
        headpos = ftell(fout);
        fputl(0, fout);
        fputl(0, fout);
        fputl(0, fout);
        fputl(0, fout);
        datapos = ftell(fout);

        if (!(fin = fopen(filename, "rb")))
            panic("can't open file '%s'", filename);

        packfile(fin, fout);
        nextpos = ftell(fout);
        realsize = ftell(fin);
        compressedsize = nextpos - datapos;

        fseek(fout, headpos, SEEK_SET);
        fputl((unsigned)datapos, fout);
        fputl((unsigned)realsize, fout);
        fputl((unsigned)compressedsize, fout);
        fputl(1, fout);                 /* compressed flag */
        fseek(fout, nextpos, SEEK_SET);

        if (ferror(fin))
            panic("read error");
        if (ferror(fout))
            panic("write error");

        fclose(fin);
    }

    if (ferror(fout))
        panic("write error");

    fclose(fout);
    return 0;
}


/* Explode `skinname` into its constituent files in CWD. */
static int explode_skin(const char *skinname)
{
    FILE *fin, *fout;
    unsigned namelen, dataoffset, realsize, compressedsize, compressedflag;
    char namebuf[NAMEBUFSIZE];

    printf("exploding skin file '%s'...\n", skinname);
    if (!(fin = fopen(skinname, "rb")))
        panic("can't open file '%s'", skinname);

    while (!(feof(fin) || ferror(fin))) {
        namelen = fgetl(fin);
        if (feof(fin) || ferror(fin))
            break;
        if (namelen > (NAMEBUFSIZE - 1))
            panic("namelen too big");

        fread(namebuf, 1, namelen, fin);
        namebuf[namelen] = 0;
        dataoffset     = fgetl(fin);
        realsize       = fgetl(fin);
        compressedsize = fgetl(fin);
        compressedflag = fgetl(fin);

        fseek(fin, (long)dataoffset, SEEK_SET);

        if (feof(fin) || ferror(fin))
            break;

        if (compressedflag)
            printf("'%s', %u bytes (%u bytes compressed)\n",
                   namebuf, realsize, compressedsize);
        else
            printf("'%s', %u bytes\n", namebuf, realsize);

        if (!(fout = fopen(namebuf, "wb")))
            panic("can't open file '%s'", namebuf);

        if (compressedflag)
            unpackfile(fin, fout);
        else
            copyfile(fin, (int)compressedsize, fout);

        fclose(fout);
        if (ferror(fin))
            panic("read error");
        /* fout already closed; ferror check above on fin only */

        fseek(fin, (long)(compressedsize + dataoffset), SEEK_SET);
    }

    if (ferror(fin))
        panic("read error");

    fclose(fin);
    return 0;
}


/* Copy exactly `len` bytes from fin to fout. Returns 0 on success. */
static int copyfile(FILE *fin, int len, FILE *fout)
{
    unsigned char buf[BUFFER_SIZE];
    while (len > 0 && !feof(fin) && !ferror(fin)) {
        size_t want = (len < BUFFER_SIZE) ? (size_t)len : (size_t)BUFFER_SIZE;
        size_t got  = fread(buf, 1, want, fin);
        if (got == 0)
            break;
        fwrite(buf, 1, got, fout);
        len -= (int)got;
    }
    return 0;
}


/* Deflate fin to fout until EOF on fin. */
static int packfile(FILE *fin, FILE *fout)
{
    z_stream zs;
    unsigned char inbuf[BUFFER_SIZE];
    unsigned char outbuf[BUFFER_SIZE];
    int status;

    zs.zalloc = Z_NULL;
    zs.zfree  = Z_NULL;
    zs.opaque = Z_NULL;

    status = deflateInit(&zs, Z_DEFAULT_COMPRESSION);
    if (status != Z_OK)
        return status;

    zs.avail_in  = 0;
    zs.next_in   = inbuf;
    zs.next_out  = outbuf;
    zs.avail_out = BUFFER_SIZE;

    while (!ferror(fin) && !ferror(fout)) {
        if (zs.avail_in == 0) {
            zs.next_in  = inbuf;
            zs.avail_in = (uInt)fread(inbuf, 1, BUFFER_SIZE, fin);
        }
        if (zs.avail_in != 0)
            status = deflate(&zs, Z_NO_FLUSH);
        else
            status = deflate(&zs, Z_FINISH);

        {
            unsigned count = (unsigned)(BUFFER_SIZE - zs.avail_out);
            if (count)
                fwrite(outbuf, 1, count, fout);
        }
        zs.next_out  = outbuf;
        zs.avail_out = BUFFER_SIZE;

        if (status != Z_OK)
            break;
    }
    deflateEnd(&zs);

    return (status == Z_STREAM_END) ? Z_OK : status;
}


/* Inflate fin to fout until inflate() reports stream end. */
static int unpackfile(FILE *fin, FILE *fout)
{
    z_stream zs;
    unsigned char inbuf[BUFFER_SIZE];
    unsigned char outbuf[BUFFER_SIZE];
    int status;

    zs.next_in  = Z_NULL;
    zs.avail_in = 0;
    zs.zalloc   = Z_NULL;
    zs.zfree    = Z_NULL;
    zs.opaque   = Z_NULL;

    status = inflateInit(&zs);
    if (status != Z_OK)
        return status;

    zs.avail_in  = 0;
    zs.next_out  = outbuf;
    zs.avail_out = BUFFER_SIZE;

    while (!ferror(fin) && !ferror(fout)) {
        if (zs.avail_in == 0) {
            zs.next_in  = inbuf;
            zs.avail_in = (uInt)fread(inbuf, 1, BUFFER_SIZE, fin);
        }
        if (zs.avail_in != 0)
            status = inflate(&zs, Z_NO_FLUSH);
        else
            status = inflate(&zs, Z_FINISH);

        {
            unsigned count = (unsigned)(BUFFER_SIZE - zs.avail_out);
            if (count)
                fwrite(outbuf, 1, count, fout);
        }
        zs.next_out  = outbuf;
        zs.avail_out = BUFFER_SIZE;

        if (status != Z_OK)
            break;
    }
    inflateEnd(&zs);

    return (status == Z_STREAM_END) ? Z_OK : status;
}


static unsigned fgetl(FILE *fin)
{
    unsigned tmp;
    tmp  = (unsigned)fgetc(fin);
    tmp |= (unsigned)fgetc(fin) <<  8;
    tmp |= (unsigned)fgetc(fin) << 16;
    tmp |= (unsigned)fgetc(fin) << 24;
    return tmp;
}


static int fputl(unsigned data, FILE *fout)
{
    fputc((int)( data        & 0xff), fout);
    fputc((int)((data >>  8) & 0xff), fout);
    fputc((int)((data >> 16) & 0xff), fout);
    fputc((int)((data >> 24) & 0xff), fout);
    return 0;
}


static void panic(const char *str, ...)
{
    va_list args;
    fprintf(stderr, PROGRAM ": ");
    va_start(args, str);
    vfprintf(stderr, str, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}
