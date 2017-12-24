/*************************************************************************
 *
 * FILE  lc_sdl_wrapper.cpp
 * $Id: lc_sdl_wrapper.cpp,v 1.5 2001/07/31 16:10:39 cmicali Exp $
 *
 * DESCRIPTION 
 *   Wrapper classes to work with old libCON code using SDL.
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
#include "png.h"

    Bitmap* newBitmap(int Width, int Height, int Flags) {
        return (Bitmap*)new Drawable(Width, Height);
    }


    Drawable::Drawable(int w, int h, bool free_surface) {

        width = w;
        height = h;
        Uint32 rmask, gmask, bmask, amask;
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000;
        gmask = 0x00ff0000;
        bmask = 0x0000ff00;
        amask = 0x000000ff;
    #else
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
    #endif
        surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32,
                                       rmask, gmask, bmask, amask);
        free_surface_on_delete = free_surface;
    }

    Drawable::Drawable(SDL_Surface *target_surface, 
             bool free_surface )  {

        surface = target_surface;
        width = surface->w;
        height = surface->h;
        free_surface_on_delete = free_surface;      
    }
    Drawable::~Drawable() {
        if (surface && free_surface_on_delete)
            SDL_FreeSurface(surface);
    }

    TColor* Drawable::getLine(int y) {

        TColor *bufp;

        bufp = (TColor *)surface->pixels + ((y*surface->pitch)>>2) ;
        
        return bufp;
    }
    TColor Drawable::getColor (int Red, int Green, int Blue) {
        return SDL_MapRGB(surface->format, Red, Green, Blue);           
    }
    long Drawable::unlock () {
        SDL_UnlockSurface(surface);
        return 0;
    }
    long Drawable::lock () {
        return SDL_LockSurface(surface);
    }
    long Drawable::fillRect (int x1, int y1, int x2, int y2, TColor color) {
        SDL_Rect r;
        r.x = x1;
        r.y = y1;
        r.w = x2-x1 +1;
        r.h = y2-y1 +1;
        return SDL_FillRect(surface, &r, color);            
    }
    void Drawable::release(void) {

    }
    void Drawable::Release(void) {

    }

    long Drawable::copy(Drawable* Source, int x, int y) {
        SDL_Rect dstrect;
        dstrect.x = x;
        dstrect.y = y;

        // <Manu> ?
        //dstrect.w = ? ;
        //dstrect.h = ? ;

        if (Source)
            return SDL_BlitSurface(Source->surface, NULL, this->surface, &dstrect);
        else
            return -1;
    }
    long Drawable::copy(Drawable* Source, int dest_x, int dest_y,
                      int source_x1, int source_y1, int source_x2, int source_y2
                      ) {
        SDL_Rect srcrect, dstrect;
        srcrect.x = source_x1;
        srcrect.y = source_y1;
        srcrect.w = source_x2 - (source_x1 + 1) ;
        srcrect.h = source_y2 - (source_y1 + 1);
        dstrect.x = dest_x;
        dstrect.y = dest_y;
        if (Source)
            return SDL_BlitSurface(Source->surface, &srcrect, this->surface, &dstrect);
        else
            return -1;
    }
    void Drawable::flip(void) {
    }

    void Drawable::drawHLine(int y, int x, int x2, TColor c) {
        TColor *screen = (TColor *)surface->pixels;
        screen += y*INTERNAL_RESOLUTION_X + x;
        TColor *end;
        end = screen + (x2-x);
        for(;screen<end;screen++)
            *screen = c;
    };
    void Drawable::drawVLine(int x, int y, int y2, TColor c) {
    };