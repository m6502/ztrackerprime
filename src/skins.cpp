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

static int luminance_of(int r, int g, int b) {
    int L = (r * 299 + g * 587 + b * 114) / 1000;
    if (L < 0) L = 0;
    if (L > 255) L = 255;
    return L;
}

// Build a 256-entry "luminance -> output color" lookup using the ORIGINAL
// skin's color luminances as gradient anchor points. This is the key trick:
// pixels in the source PNG that match the skin's authored Background end up
// exactly at the user's target Background (not somewhere between Background
// and Highlight, which is what a fixed 0/128/255 anchor produces).
//
// Anchor points are sorted by source luminance into [a0, a1, a2, a3, a4],
// each with a corresponding target color, so the LUT is a piecewise linear
// remap with up to 4 segments.
static void build_lut_anchored(Uint8 lut_r[256], Uint8 lut_g[256], Uint8 lut_b[256],
                               int origLoL, int origMidL, int origHiL,
                               int targLoR, int targLoG, int targLoB,
                               int targMidR, int targMidG, int targMidB,
                               int targHiR, int targHiG, int targHiB) {
    // Force valid ordering: pure black anchor at L=0, pure white anchor at
    // L=255, and the skin's three authoring colors in between. If two
    // anchors collapse, drop the duplicate so the lerp stays well-defined.
    struct Anchor { int L, r, g, b; };
    Anchor stops[5] = {
        {   0,                0,        0,        0        },
        { origLoL,            targLoR,  targLoG,  targLoB  },
        { origMidL,           targMidR, targMidG, targMidB },
        { origHiL,            targHiR,  targHiG,  targHiB  },
        { 255,                255,      255,      255      },
    };
    // Edge stops blend toward the matching authored color at full saturation
    // so very dark / very bright source pixels don't flatten to pure black or
    // pure white — they stay tinted.
    stops[0].r = targLoR / 2; stops[0].g = targLoG / 2; stops[0].b = targLoB / 2;
    stops[4].r = (targHiR + 255) / 2;
    stops[4].g = (targHiG + 255) / 2;
    stops[4].b = (targHiB + 255) / 2;

    // Sort + dedupe by L.
    for (int i = 0; i < 5; ++i)
      for (int j = i + 1; j < 5; ++j)
        if (stops[j].L < stops[i].L) { Anchor t = stops[i]; stops[i] = stops[j]; stops[j] = t; }
    int n = 0;
    Anchor uniq[5];
    for (int i = 0; i < 5; ++i) {
        if (n == 0 || stops[i].L > uniq[n-1].L) uniq[n++] = stops[i];
        else uniq[n-1] = stops[i];   // same L -> later anchor wins
    }

    int seg = 0;
    for (int L = 0; L < 256; ++L) {
        while (seg < n - 1 && L > uniq[seg + 1].L) seg++;
        if (seg >= n - 1) {
            lut_r[L] = (Uint8)uniq[n-1].r;
            lut_g[L] = (Uint8)uniq[n-1].g;
            lut_b[L] = (Uint8)uniq[n-1].b;
            continue;
        }
        const Anchor &a = uniq[seg];
        const Anchor &b = uniq[seg + 1];
        int span = b.L - a.L;
        if (span <= 0) { lut_r[L] = (Uint8)a.r; lut_g[L] = (Uint8)a.g; lut_b[L] = (Uint8)a.b; continue; }
        int t = ((L - a.L) * 255) / span;
        int inv = 255 - t;
        lut_r[L] = (Uint8)((a.r * inv + b.r * t) / 255);
        lut_g[L] = (Uint8)((a.g * inv + b.g * t) / 255);
        lut_b[L] = (Uint8)((a.b * inv + b.b * t) / 255);
    }
}

static void remap_surface(SDL_Surface *src, SDL_Surface *dst,
                          int origLoL, int origMidL, int origHiL,
                          int lor, int log_, int lob,
                          int midr, int midg, int midb,
                          int hir, int hig, int hib) {
    if (!src || !dst) return;
    if (src->w != dst->w || src->h != dst->h) return;

    Uint8 lut_r[256], lut_g[256], lut_b[256];
    build_lut_anchored(lut_r, lut_g, lut_b,
                       origLoL, origMidL, origHiL,
                       lor, log_, lob,
                       midr, midg, midb,
                       hir, hig, hib);

    if (!SDL_LockSurface(src)) return;
    if (!SDL_LockSurface(dst)) { SDL_UnlockSurface(src); return; }

    const SDL_PixelFormatDetails *sf = SDL_GetPixelFormatDetails(src->format);
    const SDL_PixelFormatDetails *df = SDL_GetPixelFormatDetails(dst->format);
    SDL_Palette *spal = SDL_GetSurfacePalette(src);
    SDL_Palette *dpal = SDL_GetSurfacePalette(dst);

    // Fast path: src/dst share the same 32-bit format and the byte offsets
    // for R/G/B/A can be derived from the format masks. Covers the typical
    // case where Skin::load cached origToolbar with the same pixel format
    // as bmToolbar->surface.
    bool fast = sf == df
                && sf->bytes_per_pixel == 4
                && sf->Rmask && sf->Gmask && sf->Bmask;
    if (fast) {
        // Convert mask -> byte offset in the 32-bit pixel.
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
                    Uint8 sr = sp[rb], sg = sp[gb], sbv = sp[bb_];
                    int lum = (sr * 299 + sg * 587 + sbv * 114) / 1000;
                    if (lum > 255) lum = 255;
                    dp[rb] = lut_r[lum];
                    dp[gb] = lut_g[lum];
                    dp[bb_] = lut_b[lum];
                    if (ab >= 0) dp[ab] = sp[ab];
                }
            }
            SDL_UnlockSurface(dst);
            SDL_UnlockSurface(src);
            return;
        }
    }

    // Fallback: format-agnostic via SDL_GetRGBA / SDL_MapRGBA. Slower but
    // safe regardless of pixel layout.
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
            int lum = (sr * 299 + sg * 587 + sbv * 114) / 1000;
            if (lum > 255) lum = 255;
            Uint32 outpx = SDL_MapRGBA(df, dpal,
                                       lut_r[lum], lut_g[lum], lut_b[lum], sa);
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

void Skin::recolor_to_palette(TColor lo, TColor mid, TColor hi) {
    int lor, log_, lob, midr, midg, midb, hir, hig, hib;
    unpack_color(lo,  &lor,  &log_, &lob);
    unpack_color(mid, &midr, &midg, &midb);
    unpack_color(hi,  &hir,  &hig,  &hib);

    // Read the original skin's authoring colors so we know which source
    // luminances correspond to "this is meant to be the background color".
    int oLoR, oLoG, oLoB, oMidR, oMidG, oMidB, oHiR, oHiG, oHiB;
    unpack_color(origColors.Lowlight,   &oLoR,  &oLoG,  &oLoB);
    unpack_color(origColors.Background, &oMidR, &oMidG, &oMidB);
    unpack_color(origColors.Highlight,  &oHiR,  &oHiG,  &oHiB);
    int origLoL  = luminance_of(oLoR,  oLoG,  oLoB);
    int origMidL = luminance_of(oMidR, oMidG, oMidB);
    int origHiL  = luminance_of(oHiR,  oHiG,  oHiB);

    if (origToolbar && bmToolbar && bmToolbar->surface) {
        remap_surface(origToolbar, bmToolbar->surface,
                      origLoL, origMidL, origHiL,
                      lor, log_, lob, midr, midg, midb, hir, hig, hib);
    }
    if (origButtons && bmButtons && bmButtons->surface) {
        remap_surface(origButtons, bmButtons->surface,
                      origLoL, origMidL, origHiL,
                      lor, log_, lob, midr, midg, midb, hir, hig, hib);
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


