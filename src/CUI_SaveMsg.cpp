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

std::atomic<int> save_finished(0);
std::atomic<int> is_saving(0);

unsigned long ZT_THREAD_CALL save_thread(void *) {
    char *sstr;

    is_saving = 1;
    save_finished= 0;
    switch(UIP_SaveMsg->filetype) {
        case 1:
            song->save((char *)song->filename);
            /* if there was a status message, display it */
            if ((sstr = song->getstatusstr()) != NULL) {
                /* display if ret=0, this should do a popup on ret!=0 */
                statusmsg = sstr;
                status_change = 1;
            }
            break;
        case 2: {
            ZTImportExport zie;
            zie.ExportMID((char *)song->filename,0);
            break;
        }
        case 3: {
            ZTImportExport zie;
            if (zie.ExportMultichannelMID((char *)song->filename)) {
                sprintf(szStatmsg, "Exported multichannel MIDI to %s",
                        (char *)song->filename);
            } else {
                sprintf(szStatmsg,
                        "Multichannel MIDI export failed (no tracks with events)");
            }
            statusmsg = szStatmsg;
            status_change = 1;
            break;
        }
        case 4: {
            ZTImportExport zie;
            int n = zie.ExportPerTrackMID((char *)song->filename);
            if (n > 0) {
                sprintf(szStatmsg,
                        "Exported %d per-track .MID files (e.g. *_track01.mid)",
                        n);
            } else {
                sprintf(szStatmsg,
                        "Per-track MIDI export failed (no tracks with events)");
            }
            statusmsg = szStatmsg;
            status_change = 1;
            break;
        }
    }
    save_finished = 1;
    is_saving = 0;
    return 0;
}

void TP_Save_Inc_Str(void) {
    UIP_SaveMsg->strselect++;
    if (UIP_SaveMsg->strselect>3) UIP_SaveMsg->strselect = 0;
    need_popup_refresh++;
}

CUI_SaveMsg::CUI_SaveMsg(void) {
    UI = NULL;
    hThread = NULL;
    strtimer = 0;
    filetype = 1; // .zt default
}

CUI_SaveMsg::~CUI_SaveMsg(void) {
    if (hThread) {
        zt_thread_join(hThread);
        zt_thread_close(hThread);
        hThread = NULL;
    }
    if (UI) { delete UI; }
    UI = NULL;
}

void CUI_SaveMsg::enter(void) {
    need_refresh = 1;
    save_finished= 0;
//    setWindowTitle("zt - saving...");
    zt_set_window_title("zt - [saving file]");
    OldPriority = zt_thread_get_current_priority();
    strselect = 0;
    strtimer = zt_timer_start(strtimer, 500, TP_Save_Inc_Str);
    hThread = zt_thread_create(save_thread, NULL, &iID);
    zt_thread_set_current_priority(ZT_THREAD_PRIORITY_BELOW_NORMAL);
    if (hThread) {
        zt_thread_set_priority(hThread, ZT_THREAD_PRIORITY_ABOVE_NORMAL);
    } else {
        save_finished = 1;
        statusmsg = (char *)"Error: unable to create save thread";
        status_change = 1;
    }
}
void CUI_SaveMsg::leave(void) {
    FileList *fl;
    DirList *dl;

    zt_thread_set_current_priority(OldPriority);
    zt_timer_stop(strtimer);
    strtimer = 0;
    if (hThread) {
        zt_thread_join(hThread);
        zt_thread_close(hThread);
        hThread = NULL;
    }

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
    zt_set_window_title("zt");
}

void CUI_SaveMsg::update() {
    if (Keys.size()) {
        (void)Keys.getkey();
    }
    if (save_finished) {
        close_popup_window();
        UIP_Savescreen->clear = 1;
        fixmouse++;
        need_refresh++;
        save_finished = 0;
    }
}

void CUI_SaveMsg::draw(Drawable *S)
{
  const char *str[] = { 

      "Please wait, saving... |",
      "Please wait, saving... /",
      "Please wait, saving... -",
      "Please wait, saving... \\"
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
