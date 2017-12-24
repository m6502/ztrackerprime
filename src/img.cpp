#include <png.h>
#include "zt.h"

// This was lifted from the SDL_Image package
// www.libsdl.org


// ------------------------------------------------------------------------------------------------
//
//
static void png_read_data(png_structp ctx, png_bytep area, png_size_t size)
{
	SDL_RWops *src;
	src = (SDL_RWops *)png_get_io_ptr(ctx);
	SDL_RWread(src, area, size, 1);
}



// ------------------------------------------------------------------------------------------------
//
//
SDL_Surface *IMG_LoadPNG_RW(SDL_RWops *src)
{
  SDL_Surface *volatile surface;
  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  Uint32 Rmask;
  Uint32 Gmask;
  Uint32 Bmask;
  Uint32 Amask;
  png_bytep *volatile row_pointers;
  int row ;
  volatile int ckey ;
  png_color_16 *transv = NULL ;

  //<Manu> Variables sin usar
  //SDL_Palette *palette;
  //int i;

  
  ckey = -1;
  
  /* Initialize the data we will clean up when we're done */
  png_ptr = NULL; 
  info_ptr = NULL; 
  row_pointers = NULL; 
  surface = NULL;
  
  /* Check to make sure we have something to do */
  if ( ! src ) goto done;
  
  /* Create the PNG loading context structure */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,NULL,NULL) ;
  
  if (png_ptr == NULL){
    //		IMG_SetError("Couldn't allocate memory for PNG file");
    goto done;
  }
  
  /* Allocate/initialize the memory for image information.  REQUIRED. */
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    //		IMG_SetError("Couldn't create image information for PNG file");
    goto done;
  }
  
//NEWPNG  /* Set error handling if you are using setjmp/longjmp method (this is
//NEWPNG  * the normal method of doing things with libpng).  REQUIRED unless you
//NEWPNG  * set up your own error handlers in png_create_read_struct() earlier.
//NEWPNG  */
//NEWPNG  if ( setjmp(png_ptr->jmpbuf) ) {
//NEWPNG    //		IMG_SetError("Error reading the PNG file.");
//NEWPNG    goto done;
//NEWPNG  }
  
  /* Set up the input control */
  png_set_read_fn(png_ptr, src, png_read_data);
  
  /* Read PNG header info */
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
               &color_type, &interlace_type, NULL, NULL);
  
  /* tell libpng to strip 16 bit/color files down to 8 bits/color */
  png_set_strip_16(png_ptr) ;
  
  /* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
  * byte into separate bytes (useful for paletted and grayscale images).
  */
  png_set_packing(png_ptr);
  
  /* scale greyscale values to the range 0..255 */
  if(color_type == PNG_COLOR_TYPE_GRAY)
    png_set_expand(png_ptr);
  
    /* For images with a single "transparent colour", set colour key;
  if more than one index has transparency, use full alpha channel */
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    int num_trans;
    //		Uint8 *trans;         // <MANU> No compilaba, asi que he tenido que cambiarle el tipo ¿?
    unsigned char *trans;
    
    num_trans=sizeof(Uint8) ;
    
    png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &transv);
    
    if(color_type == PNG_COLOR_TYPE_PALETTE) {
      
      if(num_trans == 1) ckey = trans[0] ;          // exactly one transparent value: set colour key
      else png_set_expand(png_ptr);
      
    } 
    else ckey = 0; // actual value will be set later
  }
  
  if ( color_type == PNG_COLOR_TYPE_GRAY_ALPHA ) png_set_gray_to_rgb(png_ptr) ;
  
  png_read_update_info(png_ptr, info_ptr);
  
  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
    &color_type, &interlace_type, NULL, NULL);
  
  /* Allocate the SDL surface to hold the image */
  
  Rmask = Gmask = Bmask = Amask = 0 ;

  if ( color_type != PNG_COLOR_TYPE_PALETTE ) {
    
    if ( SDL_BYTEORDER == SDL_LIL_ENDIAN ) {
      
      Rmask = 0x000000FF;
      Gmask = 0x0000FF00;
      Bmask = 0x00FF0000;
      Amask = (png_get_channels(png_ptr, info_ptr) == 4) ? 0xFF000000 : 0;
    } 
    else {
      
      int s = (png_get_channels(png_ptr, info_ptr) == 4) ? 0 : 8;

      Rmask = 0xFF000000 >> s;
      Gmask = 0x00FF0000 >> s;
      Bmask = 0x0000FF00 >> s;
      Amask = 0x000000FF >> s;
    }
  }
  
  surface = SDL_AllocSurface(SDL_SWSURFACE, width, height,
    bit_depth*png_get_channels(png_ptr, info_ptr), Rmask,Gmask,Bmask,Amask);
  
  if ( surface == NULL ) {
    //		IMG_SetError("Out of memory");
    goto done;
  }
  
  if(ckey != -1) {
    
    if(color_type != PNG_COLOR_TYPE_PALETTE) {

      _ASSERT(transv) ;

      /* FIXME: Should these be truncated or shifted down? */
      ckey = SDL_MapRGB(surface->format, (Uint8)transv->red, (Uint8)transv->green, (Uint8)transv->blue) ;
      SDL_SetColorKey(surface, SDL_SRCCOLORKEY, ckey);
    }
  }
  
  /* Create the array of pointers to image data */

  row_pointers = (png_bytep*) malloc(sizeof(png_bytep)*height);
  
  if ( (row_pointers == NULL) ) {

    //		IMG_SetError("Out of memory");
    SDL_FreeSurface(surface);
    surface = NULL;
    goto done;
  }

  for (row = 0; row < (int)height; row++) {

    // <Manu> 
    //row_pointers[row] = (png_bytep) (Uint8 *)surface->pixels + row*surface->pitch;
    row_pointers[row] = (png_bytep) (((Uint8 *)surface->pixels) + (row * surface->pitch));
  }
  
  /* Read the entire image in one go */
  png_read_image(png_ptr, row_pointers);
  
  /* read rest of file, get additional chunks in info_ptr - REQUIRED */
  png_read_end(png_ptr, info_ptr);

#ifdef ___DESACTIVADO_

  /* Load the palette, if any */
  palette = surface->format->palette;
  if ( palette ) {
    if(color_type == PNG_COLOR_TYPE_GRAY) {
      palette->ncolors = 256;
      
      for(i = 0; i < 256; i++) {
        palette->colors[i].r = i;
        palette->colors[i].g = i;
        palette->colors[i].b = i;
      }
      
    } 
    else {
      
      if (info_ptr->num_palette > 0 ) {

        palette->ncolors = info_ptr->num_palette; 
      
        for( i=0; i<info_ptr->num_palette; ++i ) {
        
          palette->colors[i].b = info_ptr->palette[i].blue;
          palette->colors[i].g = info_ptr->palette[i].green;
          palette->colors[i].r = info_ptr->palette[i].red;
        }
      }      
    }
  }

#endif

done:	/* Clean up and return */

  png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : (png_infopp)0, (png_infopp)0) ;
  
  if ( row_pointers ) {
    
    free(row_pointers);
  }
  
  return(surface); 
}




// ------------------------------------------------------------------------------------------------
//
//
SDL_Surface *IMG_DoLoad(SDL_RWops *src, int freesrc, char *type)
{
	int start;
  SDL_Surface *image;

	/* Make sure there is something to do.. */
	if ( src == NULL ) return(NULL);

  /* See whether or not this data source can handle seeking */
	
  if ( SDL_RWseek(src, 0, SEEK_CUR) < 0 ) {
//		IMG_SetError("Can't seek in this data source");
		return(NULL);
	}

  /* Detect the type of image being loaded */
	start = SDL_RWtell(src);
	image = NULL;
  
  SDL_RWseek(src, start, SEEK_SET);
  image = IMG_LoadPNG_RW(src);

	/* Clean up, check for errors, and return */
	if ( freesrc ) SDL_RWclose(src);

  return(image);
}




// ------------------------------------------------------------------------------------------------
//
//
SDL_Surface *SDL_LoadPNG(const char *file)
{
  SDL_RWops *src =SDL_RWFromFile(file, "rb") ;
  char *ext = (char *)strrchr(file, '.') ;

  if(ext) ext++ ;

  return(IMG_DoLoad(src, 1, ext)) ;
}




///////////////////////////////////


// This was lifted from the SDL_rotozoomer package
// http://www.ferzkopp.net/Software/SDL_rotozoom/


#define MAX(a,b)    (((a) > (b)) ? (a) : (b))


#define VALUE_LIMIT	0.001

// ------------------------------------------------------------------------------------------------
//
//
int zoomSurfaceRGBA (SDL_Surface *src, SDL_Surface *dst, int smooth)
{
 int x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy, ex, ey, t1, t2, sstep;
 tColorRGBA *c00, *c01, *c10, *c11;
 tColorRGBA *sp, *csp, *dp;
 int sgap, dgap, orderRGBA;
 
 /* Variable setup */
 if (smooth) {
  /* For interpolation: assume source dimension is one pixel */
  /* smaller to avoid overflow on right and bottom edge.     */
  sx=(int)(65536.0*(float)(src->w-1)/(float)dst->w);
  sy=(int)(65536.0*(float)(src->h-1)/(float)dst->h);
 } else {
  sx=(int)(65536.0*(float)src->w/(float)dst->w);
  sy=(int)(65536.0*(float)src->h/(float)dst->h);
 }

 /* Allocate memory for row increments */
 if ((sax=(int *)malloc((dst->w+1)*sizeof(Uint32)))==NULL) {
  return(-1);
 } 
 if ((say=(int *)malloc((dst->h+1)*sizeof(Uint32)))==NULL) {
  free(sax);
  return(-1);
 }

 /* Precalculate row increments */
 csx=0;
 csax=sax;
 for (x=0; x<=dst->w; x++) {
  *csax=csx;
  csax++;
  csx &= 0xffff;
  csx += sx;
 }
 csy=0;
 csay=say;
 for (y=0; y<=dst->h; y++) {
  *csay=csy;
  csay++;
  csy &= 0xffff;
  csy += sy;
 }

 /* Pointer setup */
 sp=csp=(tColorRGBA *)src->pixels;
 dp=(tColorRGBA *)dst->pixels;
 sgap=src->pitch - src->w*4;
 dgap=dst->pitch - dst->w*4;
 orderRGBA=(src->format->Rmask==0x000000ff);

 /* Switch between interpolating and non-interpolating code */
 if (smooth) {
 
  /* Interpolating Zoom */

  /* Scan destination */
  csay=say;
  for (y=0; y<dst->h; y++) {
   /* Setup color source pointers */
   c00=csp;
   c01=csp; c01++;
   c10=(tColorRGBA *)((Uint8 *)csp+src->pitch);
   c11=c10; c11++;
   csax=sax;
   for (x=0; x<dst->w; x++) {
     /* Switch between RGBA and ABGR ordering */
     if (orderRGBA) {
      /* RGBA ordering */
      /* Copy Alpha */
      dp->a=c00->a;
      /* Is pixel visible? */
      if (c00->a>0) {
       /* Interpolate colors */
       ex=(*csax & 0xffff);
       ey=(*csay & 0xffff);
       t1=((((c01->b-c00->b)*ex) >> 16) + c00->b) & 0xff;   
       t2=((((c11->b-c10->b)*ex) >> 16) + c10->b) & 0xff;
       dp->b=(((t2-t1)*ey) >> 16) + t1;
       t1=((((c01->g-c00->g)*ex) >> 16) + c00->g) & 0xff;   
       t2=((((c11->g-c10->g)*ex) >> 16) + c10->g) & 0xff;
       dp->g=(((t2-t1)*ey) >> 16) + t1;
       t1=((((c01->r-c00->r)*ex) >> 16) + c00->r) & 0xff;   
       t2=((((c11->r-c10->r)*ex) >> 16) + c10->r) & 0xff;
       dp->r=(((t2-t1)*ey) >> 16) + t1;
      }
     } else {
      /* ABGR ordering */
      /* Copy Alpha */
      dp->r=c00->r;
      /* Is pixel visible? */
      if (c00->r>0) {
       /* Interpolate colors */
       ex=(*csax & 0xffff);
       ey=(*csay & 0xffff);
       t1=((((c01->g-c00->g)*ex) >> 16) + c00->g) & 0xff;   
       t2=((((c11->g-c10->g)*ex) >> 16) + c10->g) & 0xff;
       dp->g=(((t2-t1)*ey) >> 16) + t1;
       t1=((((c01->b-c00->b)*ex) >> 16) + c00->b) & 0xff;   
       t2=((((c11->b-c10->b)*ex) >> 16) + c10->b) & 0xff;
       dp->b=(((t2-t1)*ey) >> 16) + t1;
       t1=((((c01->a-c00->a)*ex) >> 16) + c00->a) & 0xff;   
       t2=((((c11->a-c10->a)*ex) >> 16) + c10->a) & 0xff;
       dp->a=(((t2-t1)*ey) >> 16) + t1;
      }
    }   
    /* Advance source pointers */
    csax++;
    sstep=(*csax >> 16);
    c00 += sstep;
    c01 += sstep;
    c10 += sstep;
    c11 += sstep;
    /* Advance destination pointer */
    dp++;
   }
   /* Advance source pointer */
   csay++;
   csp = (tColorRGBA *)((Uint8 *)csp+(*csay >> 16)*src->pitch);
   /* Advance destination pointers */
   dp = (tColorRGBA *)((Uint8 *)dp+dgap);
  }

 } else {

  /* Non-Interpolating Zoom */

  csay=say;
  for (y=0; y<dst->h; y++) {
   sp=csp;
   csax=sax;
   for (x=0; x<dst->w; x++) {
    /* Draw */
    *dp=*sp;
    /* Advance source pointers */
    csax++;
    sp += (*csax >> 16);
    /* Advance destination pointer */
    dp++;
   }
   /* Advance source pointer */
   csay++;
   csp = (tColorRGBA *)((Uint8 *)csp+(*csay >> 16)*src->pitch);
   /* Advance destination pointers */
   dp = (tColorRGBA *)((Uint8 *)dp+dgap);
  }

 }

 /* Remove temp arrays */
 free (sax);
 free (say);

 return(0);
}

/* 
 
 8bit Zoomer without smoothing.

 Zoomes 8bit palette/Y 'src' surface to 'dst' surface.
 
*/  

// ------------------------------------------------------------------------------------------------
//
//
int zoomSurfaceY (SDL_Surface *src, SDL_Surface *dst)
{
 Uint32 x, y, sx, sy, *sax, *say, *csax, *csay, csx, csy;
 Uint8 *sp, *dp, *csp;
 int dgap;

 /* Variable setup */
 sx=(Uint32)(65536.0*(float)src->w/(float)dst->w);
 sy=(Uint32)(65536.0*(float)src->h/(float)dst->h);

 /* Allocate memory for row increments */
 if ((sax=(Uint32 *)malloc(dst->w*sizeof(Uint32)))==NULL) {
  return(-1);
 } 
 if ((say=(Uint32 *)malloc(dst->h*sizeof(Uint32)))==NULL) {
  if (sax!=NULL) {
   free(sax);
  }
  return(-1);
 }

 /* Precalculate row increments */
 csx=0;
 csax=sax;
 for (x=0; x<(unsigned int)dst->w; x++) {
  csx += sx;
  *csax=(csx >> 16);
  csx &= 0xffff;
  csax++;
 }
 csy=0;
 csay=say;
 for (y=0; y<(unsigned int)dst->h; y++) {
  csy += sy;
  *csay=(csy >> 16);
  csy &= 0xffff;
  csay++;
 }

 csx=0;
 csax=sax;
 for (x=0; x<(unsigned int)dst->w; x++) {
  csx += (*csax);
  csax++;
 }
 csy=0;
 csay=say;
 for (y=0; y<(unsigned int)dst->h; y++) {
  csy += (*csay);
  csay++;
 }

 /* Pointer setup */
 sp=csp=(Uint8 *)src->pixels;
 dp=(Uint8 *)dst->pixels;
 dgap=dst->pitch - dst->w;

 /* Draw */
 csay=say;
 for (y=0; y<(unsigned int)dst->h; y++) {
  csax=sax;
  sp=csp;
  for (x=0; x<(unsigned int)dst->w; x++) {
   /* Draw */
   *dp=*sp;
   /* Advance source pointers */
   sp += (*csax);
   csax++;
   /* Advance destination pointer */
   dp++;
  }
  /* Advance source pointer (for row) */
  csp += ((*csay)*src->pitch);
  csay++;
  /* Advance destination pointers */
  dp += dgap;
 }

 /* Remove temp arrays */
 free(sax);
 free(say);

 return(0);
}



// ------------------------------------------------------------------------------------------------
//
//
SDL_Surface * zoomSurface (SDL_Surface *src, double zoomx, double zoomy, int smooth) 
{
 SDL_Surface *rz_src;
 SDL_Surface *rz_dst;
 int dstwidth, dstheight;
 int is32bit;
 int i,src_converted;

 /* Sanity check */
 if (src==NULL) return(NULL);

 /* Determine if source surface is 32bit or 8bit */
 is32bit=(src->format->BitsPerPixel==32);
 if ( (is32bit) || (src->format->BitsPerPixel==8)) {
  /* Use source surface 'as is' */
  rz_src=src;
  src_converted=0;
 } else {
  /* New source surface is 32bit with a defined RGBA ordering */
  rz_src = SDL_CreateRGBSurface(SDL_SWSURFACE, src->w, src->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
  SDL_BlitSurface(src,NULL,rz_src,NULL);
  src_converted=1;
  is32bit=1;
 }
        
 /* Sanity check zoom factors */
 if (zoomx<VALUE_LIMIT) {
  zoomx=VALUE_LIMIT;
 }  
 if (zoomy<VALUE_LIMIT) {
  zoomy=VALUE_LIMIT;
 }  

 /* Calculate target size and set rect */
 dstwidth=(int)((double)rz_src->w*zoomx);
 dstheight=(int)((double)rz_src->h*zoomy);
 if (dstwidth<1) { 
  dstwidth=1;
 }
 if (dstheight<1) {
  dstheight=1;
 }

 /* Alloc space to completely contain the zoomed surface */
 rz_dst=NULL;
 if (is32bit) {
  /* Target surface is 32bit with source RGBA/ABGR ordering */
  rz_dst = SDL_CreateRGBSurface(SDL_SWSURFACE, dstwidth, dstheight, 32, rz_src->format->Rmask, rz_src->format->Gmask, rz_src->format->Bmask, rz_src->format->Amask);
 } else {
  /* Target surface is 8bit */
  rz_dst = SDL_CreateRGBSurface(SDL_SWSURFACE, dstwidth, dstheight, 8, 0, 0, 0, 0);
 }

 /* Lock source surface */
 SDL_LockSurface(rz_src);
 /* Check which kind of surface we have */
 if (is32bit) {
  /* Call the 32bit transformation routine to do the zooming (using alpha) */
  zoomSurfaceRGBA(rz_src,rz_dst,smooth);
  /* Turn on source-alpha support */
  SDL_SetAlpha(rz_dst, SDL_SRCALPHA , 255);
 } else {
  /* Copy palette and colorkey info */
  for (i=0; i<rz_src->format->palette->ncolors; i++) {
   rz_dst->format->palette->colors[i]=rz_src->format->palette->colors[i];
  }
  rz_dst->format->palette->ncolors=rz_src->format->palette->ncolors;
  /* Call the 8bit transformation routine to do the zooming */
  zoomSurfaceY(rz_src,rz_dst);
  SDL_SetColorKey(rz_dst, SDL_SRCCOLORKEY | SDL_RLEACCEL, rz_src->format->colorkey);
 }			
 /* Unlock source surface */
 SDL_UnlockSurface(rz_src);

 /* Cleanup temp surface */
 if (src_converted) {
  SDL_FreeSurface(rz_src);
 }

 /* Return destination surface */
 return(rz_dst);
}


