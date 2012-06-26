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
Skin::Skin(char *name) 
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
void Skin::makepath(char *dst, char *filename) 
{
  strcpy(&dst[0], strSkinPath); 
  strcat(&dst[0], filename);
}


#define bmpLoad(f) makepath(d,f); s = SDL_LoadBMP(d); if (!s) { sprintf(d, "Skin[%s]: Cannot load %s", name, f); if (!quiet) MessageBox(NULL,d,"Error",MB_ICONERROR | MB_OK); return 0; }
#define pngLoad(f) makepath(d,f); s = SDL_LoadPNG(d); if (!s) { sprintf(d, "Skin[%s]: Cannot load %s", name, f); if (!quiet) MessageBox(NULL,d,"Error",MB_ICONERROR | MB_OK); return 0; }


// ------------------------------------------------------------------------------------------------
//
//
void Skin::setpath(char *name) 
{
  char str[256];

  strcpy(strSkinName, name);
  strcpy(&str[0],cur_dir);
  strcat(&str[0], "\\skins\\");
  strcat(&str[0],name);
  strcat(&str[0],"\\");
  strcpy(strSkinPath, str);
}




// ------------------------------------------------------------------------------------------------
//
//
int Skin::load(char *name, bool quiet) 
{
  char d[512];
  SDL_Surface *s;
  
  setpath(name);
  makepath(d,"colors.conf");

  if (!Colors.load(d)) {

    sprintf(d, "Skin[%s]: Cannot load colors.conf", name) ;
    if (!quiet) MessageBox(NULL,d,"Error",MB_ICONERROR | MB_OK) ;
    return(0) ;
  }

  pngLoad("toolbar.png");
  bmToolbar = new Bitmap( s , true );

  pngLoad("load.png");
  bmLoad = new Bitmap( s , true );

  pngLoad("save.png");
  bmSave = new Bitmap( s , true );

  pngLoad("buttons.png");
  bmButtons = new Bitmap( s , true );
  
  pngLoad("about.png");
  bmAbout = new Bitmap( s , true );

  if (640 != RESOLUTION_X && 480 != RESOLUTION_Y) {
    
    double xscale = (double)RESOLUTION_X / 640;
    double yscale = (double)RESOLUTION_Y / 480;
    Drawable ss( zoomSurface(bmAbout->surface, xscale, yscale ,SMOOTHING_ON) , false );
    //        S->copy(&ss,5,row(12));
    SDL_FreeSurface( bmAbout->surface );
    bmAbout->surface = ss.surface;
  }    

  pngLoad("logo.png");
  SDL_FreeSurface(s);
  bmLogo = NULL;
  
  makepath(d,"font.fnt");

  if (font_load(d)) {
    sprintf(d, "Skin[%s]: Cannot load font.fnt", name); 
    if (!quiet) MessageBox(NULL,d,"Error",MB_ICONERROR | MB_OK);
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
  delete bmToolbar;
  delete bmLoad;
  delete bmSave;
  delete bmButtons;
  delete bmAbout;
  delete bmLogo;
  reset();

}



// ------------------------------------------------------------------------------------------------
//
//
void Skin::reset(void) {
    bmButtons = bmAbout = bmLogo = bmLoad = bmSave = bmToolbar = NULL;
}



extern void make_toolbar(void); // This is defined in main.cpp

// ------------------------------------------------------------------------------------------------
//
//
Skin * Skin::switchskin(char *newskintitle) 
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


