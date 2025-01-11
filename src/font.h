#ifndef _FONT_H
#define _FONT_H

#include "lc_sdl_wrapper.h"


// <Manu>
#define FONT_SIZE_X                  8
#define FONT_SIZE_Y                  8


extern unsigned char font[256*8];

#define row(_y) ((int)((_y) * FONT_SIZE_Y))
#define col(_x) ((int)((_x) * FONT_SIZE_X))

int font_load(char *filename);

// <Manu> Not used
//int font_load(std::istream *is);

int textcenter(const char *str, int local=-1);

void print(int x, int y, const char *str, TColor col, Drawable *S);
void printBG(int x, int y, const char *str, TColor col, TColor bg, Drawable *S);
void printBGu(int x, int y, unsigned char *str, TColor col, TColor bg, Drawable *S);



// <Manu> Not used
//void printshadow(int x, int y, char *str, TColor col, Drawable *S);
//void fillline(int y, char c, TColor col, TColor bg, Drawable *S);
//int hex2dec(char c);

void printline(int x, int y, unsigned char ch, int len, TColor col, Drawable *S);
void printchar(int x, int y, unsigned char ch, TColor col, Drawable *S);
void printcharBG(int x, int y, unsigned char ch, TColor col, TColor bg, Drawable *S);

int printtitle(int y, const char *str, TColor col,TColor bg,Drawable *S);
void printlineBG(int xi, int y, unsigned char ch, int len, TColor col, TColor bg, Drawable *S);

void printLCD(int x,int y,char *str, Drawable *S);

void printBGCC(int x, int y, char *str, TColor col, TColor bg, Drawable *S);

#endif