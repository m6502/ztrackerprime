#ifndef _IMG_H
#define _IMG_H

#include "zt.h"

SDL_Surface *SDL_LoadPNG(const char *file);
SDL_Surface * zoomSurface (SDL_Surface *src, double zoomx, double zoomy, int smooth);
/* ---- Defines */

#define SMOOTHING_OFF		0
#define SMOOTHING_ON		1

/* ---- Structures */

typedef struct tColorRGBA {
	Uint8 r;
	Uint8 g;
	Uint8 b;
	Uint8 a;
} tColorRGBA;

typedef struct tColorY {
	Uint8 y;
} tColorY;

#endif