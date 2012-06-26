#ifndef _FONT_H
#define _FONT_H

#include "zt.h"


// <Manu>
#define DEFAULT_FONT_SIZE_X           8
#define DEFAULT_FONT_SIZE_Y           8



extern unsigned char font[256*8];

#define row(x) ((int)((int)x)<<3)
#define col(x) ((int)((int)x)<<3)

int font_load(char *filename);
int font_load(std::istream *is);

int textcenter(char *str, int local=-1);

void print(int x, int y, char *str, TColor col, Drawable *S);
void printBG(int x, int y, char *str, TColor col, TColor bg, Drawable *S);
void printBGu(int x, int y, unsigned char *str, TColor col, TColor bg, Drawable *S);
void printshadow(int x, int y, char *str, TColor col, Drawable *S);
void printline(int x, int y, unsigned char ch, int len, TColor col, Drawable *S);
void printchar(int x, int y, unsigned char ch, TColor col, Drawable *S);
void printcharBG(int x, int y, unsigned char ch, TColor col, TColor bg, Drawable *S);

int printtitle(int y, char *str, TColor col,TColor bg,Drawable *S);
void fillline(int y, char c, TColor col, TColor bg, Drawable *S);
void printlineBG(int xi, int y, unsigned char ch, int len, TColor col, TColor bg, Drawable *S);

void printLCD(int x,int y,char *str, Drawable *S);

int hex2dec(char c);
void printBGCC(int x, int y, char *str, TColor col, TColor bg, Drawable *S);

#endif