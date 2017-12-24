/*************************************************************************
 *
 * FILE  CUI_Savescreen.cpp
 * $Id: CUI_Savescreen.cpp,v 1.10 2001/09/01 16:56:59 tlr Exp $
 *
 * DESCRIPTION 
 *   The save file selector page.
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
#include "Button.h"
#include "FileList.h"
#include "CUI_FileLists_Common.hpp"




char save_filename[256];
extern int is_saving;


// ------------------------------------------------------------------------------------------------
//
//
int scmpi(char *s1, char *s2)
{
  if (!s1 || !s2) return 0;
  if (strlen(s1)!=strlen(s2)) return 0;

  int i = 0 ;

  while (s1[i]!=0 && s2[i]!=0) {
    
    if (tolower(s1[i]) != tolower(s2[i])) return 0;
    i++ ;
  }

  return -1 ;
}



// ------------------------------------------------------------------------------------------------
//
//
void do_save(void)
{
  strcpy((char *)song->filename,save_filename);
  popup_window(UIP_SaveMsg);
}




// ------------------------------------------------------------------------------------------------
//
//
int file_exists(const char *fn)
{
  if (fn == NULL) return 0 ;
  if (fn[0] == 0) return 0 ;

  FILE *fp = fopen(fn,"rb") ;

  if (!fp) return 0 ;
  
  fclose(fp) ;
  return 1 ;
}



// ------------------------------------------------------------------------------------------------
//
//
void BTNCLK_ToggleZTMID(UserInterfaceElement *b)
{
  Button *btn;
  btn = (Button *)b;
  need_refresh++; 

  if (btn->updown == 0) {

    btn->updown = !btn->updown;

    if (btn->ID == 4) {

      ((Button *)UIP_Savescreen->UI->get_element(5))->updown = 0;
      ((Button *)UIP_Savescreen->UI->get_element(5))->state = 0;
      ((Button *)UIP_Savescreen->UI->get_element(5))->need_redraw = 1;
    }
    else {

      if (btn->ID == 5) {

        ((Button *)UIP_Savescreen->UI->get_element(4))->updown = 0;
        ((Button *)UIP_Savescreen->UI->get_element(4))->state = 0;
        ((Button *)UIP_Savescreen->UI->get_element(4))->need_redraw = 1;
      }
    }
  }
}



// ------------------------------------------------------------------------------------------------
//
//
void fl_enter(UserInterfaceElement *uie)
{
  char *f;
  FileList *fl = (FileList *)uie;
  f = fl->getCurrentItem();
  strcpy(&save_filename[0], f);
  begin_save();   
}



// ------------------------------------------------------------------------------------------------
//
//
CUI_Savescreen::CUI_Savescreen(void)
{
  FileList *fl;
  DirList *dl;
  TextInput *ti;
  DriveList *dr;
  Button *b;

  UI = new UserInterface;

  fl = new FileList;
  UI->add_element(fl,0);

  fl->x     = FILE_LIST_POS_X_CHARS ;
  fl->y     = FILE_LIST_POS_Y_CHARS ;

  fl->xsize = FILE_LIST_SIZE_X_CHARS ;
  fl->ysize = SAVE_FILE_LIST_SIZE_Y_CHARS ;

  //  fl->str = &save_filename[0]; fl->refresh();
  fl->onEnter = (ActFunc)fl_enter;
  //    strcpy(fl->filepattern,"*.zt");

  dl = new DirList;
  UI->add_element(dl,1);

  dl->x     = DIRECTORY_LIST_POS_X ;
  dl->y     = DIRECTORY_LIST_POS_Y ;

  dl->xsize = DIRECTORY_LIST_SIZE_X_CHARS ;
  dl->ysize = DIRECTORY_LIST_SIZE_Y_CHARS ;


  dr = new DriveList;
  UI->add_element(dr,2);

  dr->x     = DRIVE_LIST_POS_X ;
  dr->y     = DRIVE_LIST_POS_Y;

  dr->xsize = DRIVE_LIST_SIZE_X ;
  dr->ysize = DRIVE_LIST_SIZE_Y ;

  ti = new TextInput;
  UI->add_element(ti,3);
  
  ti->x      = SAVE_TEXTINPUT_POS_X ;
  ti->y      = SAVE_TEXTINPUT_POS_Y ;
  ti->ysize  = 1 ;
  ti->xsize  = SAVE_TEXTINPUT_SIZE_X ;
  ti->length = SAVE_TEXTINPUT_SIZE_X ;

  ti->str = (unsigned char *)&save_filename[0];

  b = new Button;
  UI->add_element(b, 4); 
  b->x = SAVE_ZT_BUTTON_POS_X ;
  b->y = SAVE_ZT_BUTTON_POS_Y ;
  b->caption = " Save as .ZT ";
  b->ysize = 1;
  b->xsize = strlen(b->caption)+1;
  b->state = b->updown = 1;
  b->OnClick = (ActFunc)BTNCLK_ToggleZTMID;

  b = new Button;
  UI->add_element(b, 5); 
  b->x = SAVE_MID_BUTTON_POS_X ;
  b->y = SAVE_MID_BUTTON_POS_Y ;
  b->caption = " Save as .MID";
  b->ysize = 1;
  b->xsize = strlen(b->caption)+1;
  b->OnClick = (ActFunc)BTNCLK_ToggleZTMID;
}




// ------------------------------------------------------------------------------------------------
//
//
CUI_Savescreen::~CUI_Savescreen(void)
{
  if (UI) delete UI ;
  UI = NULL;
}




// ------------------------------------------------------------------------------------------------
//
//
void CUI_Savescreen::enter(void)
{
  need_refresh = 1;
  is_saving = 0;
  FileList *fl;
  DirList *dl;
  fl = (FileList *)UI->get_element(0);
  dl = (DirList *)UI->get_element(1);
  dl->OnChange();
  fl->OnChange();
  if (song->filename[0] && song->filename[0]!=' ')
    strcpy(save_filename, (const char *)song->filename);
  //    fl->set_cursor(save_filename);
  fl->setCursor(fl->findItem(save_filename));
  cur_state = STATE_SAVE;
}




// ------------------------------------------------------------------------------------------------
//
//
void CUI_Savescreen::leave(void) {

}



// ------------------------------------------------------------------------------------------------
//
//
void begin_save(void)
{
  //int i = strlen(ls_filename)-3; if (i<0) i=0;
  if (((Button *)UIP_Savescreen->UI->get_element(4))->state) {

    UIP_SaveMsg->filetype = 1;

    int i=0;

    while(i<255 && save_filename[i] != 0) {

      i++;
    }

    while (i>=0 && save_filename[i]==' ') {

      i--;
    }

    if (save_filename[i] == ' ' || i == 0) save_filename[i] =0;

    if (i>3) {

      if (zcmpi(&save_filename[i-3],".zt")==0) memcpy(&save_filename[i],".zt",3);
    }
    else {

      if (i>0) memcpy(&save_filename[i+3],".zt",3);
    }

    if (file_exists(save_filename)) {

      UIP_RUSure->str = " File exists, sure?";
      UIP_RUSure->OnYes = (VFunc)do_save;
      popup_window(UIP_RUSure);
    }
    else {

      do_save();
    }
  }

  if (((Button *)UIP_Savescreen->UI->get_element(5))->state) {

    UIP_SaveMsg->filetype = 2;
    int i=0;

    while(i<255 && save_filename[i] != 0) {

      i++;
    }

    while (i>=0 && save_filename[i]==' ') {

      i--;
    }

    if (save_filename[i] == ' ' || i == 0) save_filename[i] =0;

    if (i>4) {
      if (zcmpi(&save_filename[i-4],".mid")==0)
        memcpy(&save_filename[i],".mid",4);
    }
    else {

      if (i>0) memcpy(&save_filename[i+4],".mid",4);
    }

    if (file_exists(save_filename)) {

      UIP_RUSure->str = " File exists, sure?";
      UIP_RUSure->OnYes = (VFunc)do_save;
      popup_window(UIP_RUSure);
    }
    else {

      do_save();
    }
  }
}




// ------------------------------------------------------------------------------------------------
//
//
void CUI_Savescreen::update()
{
  FileList *fl;
  DirList *dl;
  DriveList *dr;
  int key=0;

  UI->update();

  fl = (FileList *)UI->get_element(0);
  dl = (DirList *)UI->get_element(1);
  dr = (DriveList *)UI->get_element(2);

  if (dl->updated || dr->updated) {

    dl->OnChange();
    fl->OnChange();
  }

  if (Keys.size()) {

    key = Keys.getkey();

    switch(key)
    {
      case DIK_RETURN:

        begin_save();
        need_refresh++;

      break;
    } ;
  }
}




// ------------------------------------------------------------------------------------------------
//
//
void CUI_Savescreen::draw(Drawable *S)
{
  if (clear) {

    if (S->lock() == 0) {

      S->fillRect(col(1),row(12),INTERNAL_RESOLUTION_X,424,COLORS.Background);
      S->unlock();
      clear=0;
    }
  }

#ifdef _ENABLE_LOAD_SAVE_DECORATION
  S->copy(CurrentSkin->bmSave, LOADORSAVE_IMAGE_X, LOADORSAVE_IMAGE_Y);
#endif

  if (S->lock() == 0) {

    UI->draw(S);

    if (!is_saving) draw_status(S);

    printtitle(11, "File Save", COLORS. Text, COLORS.Background, S) ;

    need_refresh = 0;
    updated=2;
    S->unlock();
  }
}

/* eof */
