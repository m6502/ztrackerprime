/*************************************************************************
 *
 * FILE  Skins.h
 * $Id: Skins.h,v 1.6 2001/08/16 15:18:04 cmicali Exp $
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
 
#ifndef _SKIN_H
#define _SKIN_H

#include "zt.h"

class Skin {
        
    public:

        Bitmap *bmToolbar;
        Bitmap *bmLoad, *bmSave;
        Bitmap *bmButtons;
        Bitmap *bmAbout;
        Bitmap *bmLogo;
        
        colorset Colors;
        char strSkinPath[256];
        char strSkinName[64];

        Skin();
        Skin(char *name);
        ~Skin();
        int load( char *name, bool quiet = false);
        void setpath(char *name);
        void unload(void);
        void reset(void);
        void makepath(char *dst,char *filename);
        void getLogo(void);
        void freeLogo(void);
        Skin * switchskin(char *newskintitle);
};

#endif