#ifndef _LC_SDL_WRAPPER_H
#define _LC_SDL_WRAPPER_H

#include <SDL.h>

#ifndef MAXINT
   #define MAXINT     0x7FFFFFFF
#endif

typedef Uint32 TColor;

class Drawable
{
public:

  Drawable(int w = 640, int h = 480, bool free_surface = true);

  Drawable(SDL_Surface *target_surface, bool free_surface = false);
  ~Drawable();

  TColor* getLine(int y);
  TColor getColor (int Red, int Green, int Blue);
  long unlock ();
  long lock ();
  long fillRect (int x1, int y1, int x2, int y2, TColor color);
  void release(void);
  void Release(void);
  long copy(Drawable* Source,
            int dest_x, int dest_y,
            int source_x1, int source_y1, int source_x2, int source_y2) ;

  long copy(Drawable* Source, int x, int y);
  void flip(void);
  void drawHLine(int y, int x, int x2, TColor c);
  void drawVLine(int x, int y, int y2, TColor c);

public:

  SDL_Surface *surface;
  bool free_surface_on_delete;
  int width,height;
};

typedef Drawable Bitmap;
typedef Drawable Screen;

Bitmap* newBitmap(int Width, int Height, int Flags=0);

#endif // _LC_SDL_WRAPPER_H
