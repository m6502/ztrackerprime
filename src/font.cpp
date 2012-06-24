/*************************************************************************
 *
 * FILE  font.cpp
 * $Id: font.cpp,v 1.7 2001/08/14 14:52:48 cmicali Exp $
 *
 * DESCRIPTION 
 *   IT DOS-font load and display routines.
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

unsigned char font[256*8];

int textcenter(char *str, int local) {
    if (local != -1) {
        return ((local) -(strlen((char *)str)/2));
    } else
    return ((RESOLUTION_X/16) -(strlen((char *)str)/2));
}

int printtitle(int y, char *str, TColor col,TColor bg,Drawable *S) {
    int x;
    char str2[256];
    str2[0] = ' '; str2[1] = 0; strcat(str2,str); strcat(str2," ");
    x = textcenter(str2);
    printBG(col(x),row(y),str2,col,bg,S);
    printlineBG(col(1),row(y),154,x-1,col,bg,S);
    printlineBG(col(x+strlen(str2)),row(y),154, (RESOLUTION_X/8)-x-strlen(str2)-2  ,col,bg,S);
    return 0;
}

int font_load(char *filename)
{
    FILE *fp;
    char c;
    int i;

    if (!(fp = fopen(filename,"rb"))) return 1;

    for (i=0;i<256*8;i++) {

        c = fgetc(fp);
        font[i] = c;
    }

    fclose(fp);
    
    return 0;
}

int font_load(std::istream *is) {
    char c;
    int i;
    if (!is)
        return 1;
    for (i=0;i<256*8;i++) {
        is->read((char *)&c,1);
        font[i] = c;
    }
    return 0;
}


void print(int x, int y, char *str, TColor col, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int c=0,i,j;
    while(str[c]) {
        for(i=0;i<8;i++) {
            byte = font[((int)str[c]<<3)+i];
            buf = S->getLine(y+i) + x + (c<<3) + 7;
            for(j=0;j<8;j++) {
                if (byte & 1)
                    *buf = col;
                buf--;
                byte >>= 1;
            }
        }
        c++;
    }
}

void printshadow(int x, int y, char *str, TColor col, Drawable *S) {
    print((x<<3) +1,(y<<3) +1,str,0,S);
    print(col(x),row(y),str,col,S);
}

void fillline(int y, char c, TColor col, TColor bg, Drawable *S) {
    char str[81];
    memset(str,c,80);
    str[80] = 0;
    printBG(0,y,str,col,bg,S);
}





#define Screen_Pitch (RESOLUTION_X*1)


// ------------------------------------------------------------------------------------------------
//
//
void printBG(int x, int y, char *str,TColor col, TColor bg, Drawable *S)
{
  TColor *buf,*start;
  unsigned char byte;
  int c,i,j,fontptr;

  c = 0 ;

  //adjust = S->getLine(y) + x;

  start = S->getLine(y) + x + 7;

  while(str[c]) {

    fontptr = str[c]<<3;
    buf = start + (c<<3);

    for(i=0; i<8; i++) {

      byte = font[fontptr++];

      for(j=0;j<8;j++) {

        if (byte & 1) *buf = col;              // <Manu> Este if se puede sacar del for(;;) y hacer dos bucles en su lugar
        else *buf = bg;
        
        buf--;
        byte >>= 1;
      }

      buf += Screen_Pitch + 8 ; // - 8;
    }

    c++ ;
  }

}





int hex2dec(char c) {
    if (toupper(c) >= 'A' && toupper(c)<='F')
        return (toupper(c)-'A'+10);
    else
        return (toupper(c)-'0');
}


void printBGCC(int x, int y, char *str, TColor col, TColor bg, Drawable *S) {
    TColor *buf,use;
    unsigned char byte;
    int c=0,i,j,pos=0;
    use = col;
    while(str[c]) {
        if (str[c] == '|' && str[c+1] != '|') {
            c++;
            switch(toupper(str[c])) {
                case 'H':
                    use = COLORS.Highlight;
                    break;
                case 'T':
                    use = COLORS.Text;
                    break;
                case 'U':
                    use = col;
                    break;
            }
            c+=2;
        } else if (str[c] == '|' && str[c+1] == '|') c++;
        
        for(i=0;i<8;i++) {
            byte = font[((int)str[c]<<3)+i];
            buf = S->getLine(y+i) + x + (pos<<3) + 7;
            for(j=0;j<8;j++) {
                if (byte & 1)
                    *buf = use;
                else
                    *buf = bg;
                buf--;
                byte >>= 1;
            }
        }
        c++;
        pos++;
    }
}

void printBGu(int x, int y, unsigned char *str, TColor col, TColor bg, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int c=0,i,j;
    while(str[c]) {
        for(i=0;i<8;i++) {
            byte = font[((int)str[c]<<3)+i];
            buf = S->getLine(y+i) + x + (c<<3) + 7;
            for(j=0;j<8;j++) {
                if (byte & 1)
                    *buf = col;
                else
                    *buf = bg;
                buf--;
                byte >>= 1;
            }
        }
        c++;
    }
}
void printchar(int x, int y, unsigned char ch, TColor col, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int i,j;
    for(i=0;i<8;i++) {
        byte = font[(((int)ch)<<3)+i];
        buf = S->getLine(y+i) + x + 7;
        for(j=0;j<8;j++) {
            if (byte & 1) 

                *buf = col;
            buf--;
            byte >>= 1;
        }
    }
}
void printcharBG(int x, int y, unsigned char ch, TColor col, TColor bg, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int i,j;
    for(i=0;i<8;i++) {
        byte = font[(((int)ch)<<3)+i];
        buf = S->getLine(y+i) + x + 7;
        for(j=0;j<8;j++) {
            if (byte & 1) 
                *buf = col;
            else
                *buf = bg;
            buf--;
            byte >>= 1;
        }
    }
}
void printline(int xi, int y, unsigned char ch, int len, TColor col, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int i,j;
    for(int x=xi;x<(xi+(8*len));x+=8)
        for(i=0;i<8;i++) {
            byte = font[(((int)ch)<<3)+i];
            buf = S->getLine(y+i) + x + 7;
            for(j=0;j<8;j++) {
                if (byte & 1)
                    *buf = col;
                buf--;
                byte >>= 1;
            }
        }
}

void printlineBG(int xi, int y, unsigned char ch, int len, TColor col, TColor bg, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int i,j;
    for(int x=xi;x<(xi+(8*len));x+=8)
        for(i=0;i<8;i++) {
            byte = font[(((int)ch)<<3)+i];
            buf = S->getLine(y+i) + x + 7;
            for(j=0;j<8;j++) {
                if (byte & 1)
                    *buf = col;
                else
                    *buf = bg;
                buf--;
                byte >>= 1;
            }
        }
}

void printLCD(int x,int y, char *str, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int c=0,i,j;
    while(str[c]) {
        for(i=0;i<8;i++) {
            byte = font[((int)str[c]<<3)+i];
            buf = S->getLine(y+(i*3)) + x + ((c<<3)*3)-2;
            for(j=0;j<24;j++) {
                *buf = COLORS.LCDLow;
                buf++;
            }
            buf = S->getLine(y+(i*3)+2) + x + ((c<<3)*3)-2;
            for(j=0;j<24;j++) {
                *buf = COLORS.LCDLow;
                buf++;
            }
            buf = S->getLine(y+(i*3)+1) + x + ((c<<3)*3) + 7*3;
            for(j=0;j<8;j++) {
                *buf = COLORS.LCDLow; buf--;
                buf--;
                *buf = COLORS.LCDLow; buf++;
                if (byte & 1) {
                    buf--;  *buf = COLORS.LCDMid; buf++;
                    buf++;  *buf = COLORS.LCDMid; buf--;
                    *buf = COLORS.LCDHigh;
                } else {
                    *buf = COLORS.LCDLow;
                }
                byte >>= 1;
                buf-=2;
            }
        }
        c++;
    }
}



















