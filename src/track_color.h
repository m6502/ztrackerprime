#ifndef ZT_TRACK_COLOR_H
#define ZT_TRACK_COLOR_H

#include "img.h"

// ------------------------------------------------------------------------------------------------
// Shift each RGB channel of a packed TColor by delta (clamped 0..255),
// preserving the alpha byte. Used to derive the low/high background shades
// of a custom track color from a single base color.
//
static inline TColor zt_track_shade(TColor c, int delta)
{
    int r = (int)((c >> 16) & 0xFF) + delta;
    int g = (int)((c >>  8) & 0xFF) + delta;
    int b = (int)((c      ) & 0xFF) + delta;
    
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    
    if (b < 0) b = 0;
    if (b > 255) b = 255;
             
    TColor color = (c & 0xFF000000u) | ((TColor)r << 16) | ((TColor)g << 8) | (TColor)b ;
    return color ;
}

// ------------------------------------------------------------------------------------------------
// Perceptual luminance (0..255) of a packed color
//
static inline int zt_track_luma(TColor c)
{
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    return (r * 77 + g * 150 + b * 29) >> 8;
}

// ------------------------------------------------------------------------------------------------
//
//
static inline TColor zt_track_text_for_bg(TColor bg)
{
    // Pick a legible foreground (black or bright white) for a given background
    // using a perceptual luminance threshold, so custom track colors stay
    // readable whether the user picks a light or dark hue
    return (zt_track_luma(bg) > 140) ? 0xFF000000u : 0xFFFFFFFFu;
}

// ------------------------------------------------------------------------------------------------
// Background for the current row of a colored track
//
static inline TColor zt_track_cursor_bg(TColor base, int beat) {
    int d = (zt_track_luma(base) > 140) ? -48 : 56;
    if (beat) d += (d > 0) ? 22 : -22;
    return zt_track_shade(base, d);
}

#endif // ZT_TRACK_COLOR_H
