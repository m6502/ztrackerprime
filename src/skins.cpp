/*************************************************************************
 *
 * FILE  Skins.cpp
 * $Id: skins.cpp,v 1.10 2002/04/15 15:27:05 zonaj Exp $
 *
 * DESCRIPTION 
 *   Skin functions.
 *
 * This file is part of ztracker - a tracker-style MIDI sequencer.
 *
 * Copyright (c) 2000-2001, Christopher Micali <micali@concentric.net>
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

// ------------------------------------------------------------------------------------------------
//
//
Skin::Skin() 
{
  reset() ;
}

// ------------------------------------------------------------------------------------------------
//
//
Skin::Skin(const char *name)
{
  reset();
  load(name);
}


// ------------------------------------------------------------------------------------------------
//
//
Skin::~Skin() 
{
  unload();
  reset();
}


// ------------------------------------------------------------------------------------------------
//
//
void Skin::makepath(char *dst, const char *filename)
{
  strcpy(&dst[0], strSkinPath); 
  strcat(&dst[0], filename);
}


#define bmpLoad(f) makepath(d,f); s = SDL_LoadBMP(d); if (!s) { sprintf(d, "Skin[%s]: Cannot load %s", name, f); if (!quiet) zt_show_error("Error", d); return 0; }
#define pngLoad(f) makepath(d,f); s = SDL_LoadPNG(d); if (!s) { sprintf(d, "Skin[%s]: Cannot load %s", name, f); if (!quiet) zt_show_error("Error", d); return 0; }


// ------------------------------------------------------------------------------------------------
//
//
void Skin::setpath(char *name) 
{
  static char str[MAX_PATH + 1];

  strcpy(strSkinName, name);
  strcpy(&str[0], cur_dir);
#if defined(_WIN32)
  strcat(&str[0], "\\skins\\");
  strcat(&str[0], name);
  strcat(&str[0], "\\");
#else
  strcat(&str[0], "/skins/");
  strcat(&str[0], name);
  strcat(&str[0], "/");
#endif
  strcpy(strSkinPath, str);
}




// ------------------------------------------------------------------------------------------------
//
//
int Skin::load(const char *name, bool quiet)
{
  char d[512];
  SDL_Surface *s;
  
  setpath(name);
  makepath(d,"colors.conf");

  if (!Colors.load(d)) {

    sprintf(d, "Skin[%s]: Cannot load colors.conf", name) ;
    if (!quiet) zt_show_error("Error", d) ;
    return(0) ;
  }
  // Snapshot the as-shipped palette so recolor_to_palette can anchor its
  // gradient to the skin's actual authoring colors.
  origColors = Colors;

  pngLoad("toolbar.png");
  bmToolbar = new Bitmap( s , true );
  // Cache an untouched copy of the toolbar PNG so the Palette Editor can
  // re-recolor on every palette change without losing source pixels.
  if (bmToolbar->surface) {
    origToolbar = SDL_CreateSurface(bmToolbar->surface->w,
                                    bmToolbar->surface->h,
                                    bmToolbar->surface->format);
    if (origToolbar && SDL_LockSurface(origToolbar)) {
      if (SDL_LockSurface(bmToolbar->surface)) {
        for (int y = 0; y < bmToolbar->surface->h; ++y) {
          memcpy((Uint8 *)origToolbar->pixels + y * origToolbar->pitch,
                 (Uint8 *)bmToolbar->surface->pixels + y * bmToolbar->surface->pitch,
                 bmToolbar->surface->w * 4);
        }
        SDL_UnlockSurface(bmToolbar->surface);
      }
      SDL_UnlockSurface(origToolbar);
    }
  }

#ifdef _ENABLE_LOAD_SAVE_DECORATION
  pngLoad("load.png");
  bmLoad = new Bitmap( s , true );

  pngLoad("save.png");
  bmSave = new Bitmap( s , true );
#endif

  pngLoad("buttons.png");
  bmButtons = new Bitmap( s , true );
  if (bmButtons->surface) {
    origButtons = SDL_CreateSurface(bmButtons->surface->w,
                                    bmButtons->surface->h,
                                    bmButtons->surface->format);
    if (origButtons && SDL_LockSurface(origButtons)) {
      if (SDL_LockSurface(bmButtons->surface)) {
        for (int y = 0; y < bmButtons->surface->h; ++y) {
          memcpy((Uint8 *)origButtons->pixels + y * origButtons->pitch,
                 (Uint8 *)bmButtons->surface->pixels + y * bmButtons->surface->pitch,
                 bmButtons->surface->w * 4);
        }
        SDL_UnlockSurface(bmButtons->surface);
      }
      SDL_UnlockSurface(origButtons);
    }
  }
  
  pngLoad("about.png");
  bmAbout = new Bitmap( s , true );

  if (640 != INTERNAL_RESOLUTION_X && 480 != INTERNAL_RESOLUTION_Y) {
    
    double xscale = (double)INTERNAL_RESOLUTION_X / 640;
    double yscale = (double)INTERNAL_RESOLUTION_Y / 480;
    Drawable ss( zoomSurface(bmAbout->surface, xscale, yscale ,SMOOTHING_ON) , false );
    //        S->copy(&ss,5,row(12));

    zt_destroy_surface(bmAbout->surface);
    bmAbout->surface = ss.surface;
  }    

  pngLoad("logo.png");

  zt_destroy_surface(s);
  bmLogo = NULL;
  
  makepath(d,"font.fnt");
  //makepath(d,"cour.ttf");

  if (font_load(d)) {
    sprintf(d, "Skin[%s]: Cannot load font.fnt", name); 
    if (!quiet) zt_show_error("Error", d);
    return 0;
  }
  
  return(1) ;
}




// ------------------------------------------------------------------------------------------------
//
//
void Skin::getLogo(void) 
{
  char d[512] ;

  makepath(d,"logo.png"); 
  SDL_Surface *s = SDL_LoadPNG(d) ;
  bmLogo = new Bitmap( s , true ) ;
}




// ------------------------------------------------------------------------------------------------
//
//
void Skin::freeLogo(void) 
{
  delete bmLogo;
  bmLogo = NULL;
}




// ------------------------------------------------------------------------------------------------
//
//
void Skin::unload()
{
  if (bmToolbar) { delete bmToolbar; bmToolbar = NULL; }

#ifdef _ENABLE_LOAD_SAVE_DECORATION
  if (bmLoad)    { delete bmLoad;    bmLoad    = NULL; }
  if (bmSave)    { delete bmSave;    bmSave    = NULL; }
#endif

  if (bmButtons) { delete bmButtons; bmButtons = NULL; }
  if (bmAbout)   { delete bmAbout;   bmAbout   = NULL; }
  if (bmLogo)    { delete bmLogo;    bmLogo    = NULL; }
  if (origToolbar) { zt_destroy_surface(origToolbar); origToolbar = NULL; }
  if (origButtons) { zt_destroy_surface(origButtons); origButtons = NULL; }
  reset();

}



// ------------------------------------------------------------------------------------------------
//
//
void Skin::reset(void) {
    bmButtons = bmAbout = bmLogo = bmLoad = bmSave = bmToolbar = NULL;
    origToolbar = NULL;
    origButtons = NULL;
}

// ---------------------------------------------------------------------------
// Recolor cached PNGs into bmToolbar / bmButtons by remapping each pixel's
// luminance through a 3-stop gradient (lo -> mid -> hi).
//
// This is the "skin follows the palette" trick used by FT2/IT and friends:
// instead of vector graphics or per-skin colors.conf substitutions, we treat
// the toolbar/buttons PNG as a luminance template and tint it to whatever
// colors the user picked in the Palette Editor. Antialiased edges, button
// shadows and embossing all survive because we only touch chroma, not luma.
// ---------------------------------------------------------------------------

static inline void unpack_color(TColor c, int *r, int *g, int *b) {
    *r = (c >> 16) & 0xFF;
    *g = (c >>  8) & 0xFF;
    *b = (c      ) & 0xFF;
}

static inline void lerp_rgb(int t, int *r0, int *g0, int *b0,
                            int r1, int g1, int b1) {
    // t in [0..255]
    int inv = 255 - t;
    *r0 = (*r0 * inv + r1 * t) / 255;
    *g0 = (*g0 * inv + g1 * t) / 255;
    *b0 = (*b0 * inv + b1 * t) / 255;
}

static inline int clamp_byte(int v) {
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return v;
}

// (No LUT. The new shift-based remap doesn't need one — each output pixel
//  is just the source pixel plus a constant per-channel signed delta, so the
//  PNG's gradient structure is preserved perfectly with no banding.)

// Recolor by per-pixel signed delta: out = src + (target_bg - orig_bg).
// Pixels at the skin's authored Background land EXACTLY at the target
// Background. All other pixels shift by the same vector so the toolbar's
// internal contrast and gradients are preserved with zero banding.
static void remap_surface(SDL_Surface *src, SDL_Surface *dst,
                          int dr, int dg, int db) {
    if (!src || !dst) return;
    if (src->w != dst->w || src->h != dst->h) return;

    if (!SDL_LockSurface(src)) return;
    if (!SDL_LockSurface(dst)) { SDL_UnlockSurface(src); return; }

    const SDL_PixelFormatDetails *sf = SDL_GetPixelFormatDetails(src->format);
    const SDL_PixelFormatDetails *df = SDL_GetPixelFormatDetails(dst->format);
    SDL_Palette *spal = SDL_GetSurfacePalette(src);
    SDL_Palette *dpal = SDL_GetSurfacePalette(dst);

    // Fast path: src/dst share the same 32-bit format. Walk pixels by raw
    // byte offsets derived from the format masks.
    bool fast = sf == df
                && sf->bytes_per_pixel == 4
                && sf->Rmask && sf->Gmask && sf->Bmask;
    if (fast) {
        auto mask_to_byte = [](Uint32 m) -> int {
            if (m == 0x000000FFu) return 0;
            if (m == 0x0000FF00u) return 1;
            if (m == 0x00FF0000u) return 2;
            if (m == 0xFF000000u) return 3;
            return -1;
        };
        int rb = mask_to_byte(sf->Rmask);
        int gb = mask_to_byte(sf->Gmask);
        int bb_ = mask_to_byte(sf->Bmask);
        int ab = sf->Amask ? mask_to_byte(sf->Amask) : -1;
        if (rb >= 0 && gb >= 0 && bb_ >= 0) {
            for (int y = 0; y < src->h; ++y) {
                Uint8 *srow = (Uint8 *)src->pixels + y * src->pitch;
                Uint8 *drow = (Uint8 *)dst->pixels + y * dst->pitch;
                for (int x = 0; x < src->w; ++x) {
                    Uint8 *sp = srow + x * 4;
                    Uint8 *dp = drow + x * 4;
                    dp[rb] = (Uint8)clamp_byte((int)sp[rb] + dr);
                    dp[gb] = (Uint8)clamp_byte((int)sp[gb] + dg);
                    dp[bb_] = (Uint8)clamp_byte((int)sp[bb_] + db);
                    if (ab >= 0) dp[ab] = sp[ab];
                }
            }
            SDL_UnlockSurface(dst);
            SDL_UnlockSurface(src);
            return;
        }
    }

    // Fallback: format-agnostic.
    for (int y = 0; y < src->h; ++y) {
        Uint8 *srow = (Uint8 *)src->pixels + y * src->pitch;
        Uint8 *drow = (Uint8 *)dst->pixels + y * dst->pitch;
        for (int x = 0; x < src->w; ++x) {
            Uint8 sr, sg, sbv, sa;
            Uint32 px;
            int bpp = sf->bytes_per_pixel;
            if (bpp == 4) {
                px = ((Uint32 *)srow)[x];
            } else {
                px = 0;
                for (int k = 0; k < bpp; ++k) px |= srow[x * bpp + k] << (8 * k);
            }
            SDL_GetRGBA(px, sf, spal, &sr, &sg, &sbv, &sa);
            Uint32 outpx = SDL_MapRGBA(df, dpal,
                                       (Uint8)clamp_byte((int)sr + dr),
                                       (Uint8)clamp_byte((int)sg + dg),
                                       (Uint8)clamp_byte((int)sbv + db),
                                       sa);
            int dbpp = df->bytes_per_pixel;
            if (dbpp == 4) {
                ((Uint32 *)drow)[x] = outpx;
            } else {
                for (int k = 0; k < dbpp; ++k) drow[x * dbpp + k] = (outpx >> (8 * k)) & 0xFF;
            }
        }
    }
    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

void Skin::recolor_to_palette(TColor /*lo*/, TColor mid, TColor /*hi*/) {
    // Compute the per-channel signed shift between the skin's authored
    // Background and the user's target Background. Apply it to every cached
    // source pixel — fast, banding-free, and the toolbar's main field
    // matches COLORS.Background exactly because that's the anchor.
    int omR, omG, omB, tmR, tmG, tmB;
    unpack_color(origColors.Background, &omR, &omG, &omB);
    unpack_color(mid,                   &tmR, &tmG, &tmB);
    int dr = tmR - omR;
    int dg = tmG - omG;
    int db = tmB - omB;

    if (origToolbar && bmToolbar && bmToolbar->surface) {
        remap_surface(origToolbar, bmToolbar->surface, dr, dg, db);
    }
    if (origButtons && bmButtons && bmButtons->surface) {
        remap_surface(origButtons, bmButtons->surface, dr, dg, db);
    }
}



extern void make_toolbar(void); // This is defined in main.cpp

// ------------------------------------------------------------------------------------------------
//
//
Skin *Skin::switchskin(const char *newskintitle)
{
  Skin *newskin ;
  
  newskin = new Skin();

  if (!newskin->load(newskintitle,true)) {

    //delete newskin;
    sprintf(szStatmsg, "Error loading '%s' skin (files missing?)",newskintitle);
    statusmsg = szStatmsg;
    status_change = 1;  
    return newskin;
  }

  doredraw++;
  need_refresh++;
  CurrentSkin = newskin;
  make_toolbar();
  //    delete this;
  sprintf(szStatmsg, "%s skin loaded",newskintitle);
  statusmsg = szStatmsg;
  status_change = 1;  
  return this;
}


