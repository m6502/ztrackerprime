/*************************************************************************
 *
 * FILE  CUI_SaveMsg.cpp
 * $Id: CUI_SaveMsg.cpp,v 1.10 2001/09/01 17:12:56 tlr Exp $
 *
 * DESCRIPTION 
 *   The save file progress page.  This is where the saving takes place.
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

int save_finished = 0;
int is_saving = 0;

void save_thread(void) {
    char *sstr;
    int ret;

    is_saving = 1;
    save_finished= 0;
    switch(UIP_SaveMsg->filetype) {
        case 1:
            ret = song->save((char *)song->filename);
            /* if there was a status message, display it */
            if (sstr=song->getstatusstr()) {
                /* display if ret=0, this should do a popup on ret!=0 */
                statusmsg = sstr;
                status_change = 1;
            }
            break;
        case 2:
            ZTImportExport zie;
            zie.ExportMID((char *)song->filename,0);
            break;          
    }
    save_finished = 1;
    is_saving = 0;
    return;
}

void TP_Save_Inc_Str(void) {
    UIP_SaveMsg->strselect++;
    if (UIP_SaveMsg->strselect>3) UIP_SaveMsg->strselect = 0;
    need_popup_refresh++;
}

CUI_SaveMsg::CUI_SaveMsg(void) {
    UI = NULL;
    filetype = 1; // .zt default
}

CUI_SaveMsg::~CUI_SaveMsg(void) {
    if (UI) delete UI; UI = NULL;
}

void CUI_SaveMsg::enter(void) {
    need_refresh = 1;
    save_finished= 0;
//    setWindowTitle("zt - saving...");
    SDL_WM_SetCaption("zt - [saving file]","zt - [saving file]");
    OldPriority = GetThreadPriority(GetCurrentThread());
    strselect = 0;
    strtimer = SetTimer(NULL,strtimer,500,(TIMERPROC)TP_Save_Inc_Str);
    hThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)save_thread,NULL,0,&iID); 
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    SetThreadPriority(hThread,THREAD_PRIORITY_ABOVE_NORMAL);
}

void CUI_SaveMsg::leave(void) {
    FileList *fl;
    DirList *dl;

    SetThreadPriority(GetCurrentThread(),OldPriority);
    KillTimer(NULL,strtimer);
    CloseHandle(hThread);

    need_refresh++;

    if (cur_state == STATE_SAVE) {
        fl = (FileList *)UIP_Savescreen->UI->get_element(0);
        dl = (DirList *)UIP_Savescreen->UI->get_element(1);
        
        fl->OnChange();
        dl->OnChange();
        
//        fl->set_cursor(save_filename);
        fl->setCursor(fl->findItem((char*)song->filename));
        Keys.flush();
    }
//    setWindowTitle("zt");
    SDL_WM_SetCaption("zt","zt");
}

void CUI_SaveMsg::update() {
    int key=0;
    if (Keys.size()) {
        key = Keys.getkey();
    }   
    if (save_finished) {
            close_popup_window();
            UIP_Savescreen->clear = 1;
            fixmouse++;
            need_refresh++;     
    }
}

void CUI_SaveMsg::draw(Drawable *S) {
    int i;

    char *str[] = { "Please wait, saving... |",
                    "Please wait, saving... /",
                    "Please wait, saving... -",
                    "Please wait, saving... \\"
    };

    if (S->lock()==0) {
        S->fillRect(col(15),row(20),640-col(15)-1,row(25)-1,COLORS.Background);
        printline(col(15),row(24),148,50,COLORS.Lowlight,S);
        for (i=20;i<25;i++) {
            printchar(col(15),row(i),145,COLORS.Highlight,S);
            printchar(col(64),row(i),146,COLORS.Lowlight,S);
        }
        printline(col(15),row(20),143,50,COLORS.Highlight,S);
        print(col(textcenter(str[strselect],40)),row(22),str[strselect],COLORS.Text,S);
        S->unlock();
        need_refresh = 0;
        updated++;
    }
}
/* eof */
