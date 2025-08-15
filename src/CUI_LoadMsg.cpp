/*************************************************************************
 *
 * FILE  CUI_LoadMsg.cpp
 * $Id: CUI_LoadMsg.cpp,v 1.11 2001/09/01 17:12:56 tlr Exp $
 *
 * DESCRIPTION 
 *   The load file progress page.  This is where the loading takes place.
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
#include "FileList.h"


int load_finished = 0;
int load_lock = 0;
int is_loading = 0;

void load_thread(void) {
    char *sstr;
    int ret;

    is_loading = 1;
    load_finished = 0;
    load_lock = 1;
    ret=song->load(load_filename);

    /* if there was a status message, display it */
    if (sstr=song->getstatusstr()) {
        /* display if ret=0, this should do a popup on ret!=0 */
        statusmsg = sstr;
        status_change = 1;
    }

    load_finished = 1;
    is_loading = 0;
    return;
}

void TP_Load_Inc_Str(void) {
    UIP_LoadMsg->strselect++;
    if (UIP_LoadMsg->strselect>3) UIP_LoadMsg->strselect = 0;
    need_popup_refresh++;
    if (load_finished) {
        close_popup_window();
        UIP_Loadscreen->clear = 1;
        fixmouse++;
        need_refresh++;     
    }
}

CUI_LoadMsg::CUI_LoadMsg(void) {
    UI = NULL;
}

CUI_LoadMsg::~CUI_LoadMsg(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_LoadMsg::enter(void) {
    need_refresh = 1;
    load_finished = 0;
//    setWindowTitle("zt - loading file...");
    SDL_WM_SetCaption("zt - [loading file]","zt - [loading file]");
    OldPriority = GetThreadPriority(GetCurrentThread());
    strtimer = strselect = 0;
    strtimer = SetTimer(NULL,strtimer,500,(TIMERPROC)TP_Load_Inc_Str);
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)load_thread,NULL,0,&iID); 
    while(load_lock) {
        SDL_Delay(1);
    }
}

void CUI_LoadMsg::leave(void) {
    FileList *fl;
    SetThreadPriority(GetCurrentThread(),OldPriority);
    KillTimer(NULL,strtimer);
    CloseHandle(hThread);
//    setWindowTitle("zt");
    SDL_WM_SetCaption("zt","zt");
    fl = (FileList *)UIP_Savescreen->UI->get_element(0);
//    fl->set_cursor(load_filename);
    fl->setCursor(fl->findItem(load_filename));
    need_refresh++;
}

void CUI_LoadMsg::update() {
    int key=0;
    if (Keys.size()) {
        key = Keys.getkey();
    }   

}

void CUI_LoadMsg::draw(Drawable *S)
{
  const char *str[] = { 

      "Please wait, loading... |",
      "Please wait, loading... /",
      "Please wait, loading... -",
      "Please wait, loading... \\"
  };

  int sizex = 50 ;
  int sizey = 5 ;
  int x = (CHARS_X / 2) - (sizex / 2) ;
  int y = (CHARS_Y / 2) - (sizey / 2) ;

  if (S->lock() == 0) {

    S->fillRect(col(x), row(y), col(x + sizex) - 1, row(y + sizey) - 1, COLORS.Background);
    printline(col(x),row(y + 4),148,50,COLORS.Lowlight,S);

    for (int i=y;i<y+5;i++) {

      printchar(col(x),     row(i),145,COLORS.Highlight,S);
      printchar(col(x + 49),row(i),146,COLORS.Lowlight, S);
    }

    printline(col(x),row(y),143,50,COLORS.Highlight,S);
    print(col(textcenter(str[strselect],CHARS_X / 2)),row(y + 2),str[strselect],COLORS.Text,S); // <Manu> strselect is not updated while loading anyway... Better to simplify and remove the four strings?

    S->unlock();

    need_refresh = 0;
    updated++;
  }
}

/* eof */
