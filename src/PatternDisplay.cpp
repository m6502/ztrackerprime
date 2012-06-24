#include "zt.h"
#include "PatternDisplay.h"



// ------------------------------------------------------------------------------------------------
//
//
PatternDisplay::PatternDisplay(void) 
{
    frm = new Frame;
    
    // <Manu> Por defecto estaba a 0, pero yo quiero cur_pat_view a 1
    
    cur_pat_view = 1;

    starttrack = 0; 
    cur_track = 0;
    disp_row = 0;
}




// ------------------------------------------------------------------------------------------------
//
//
PatternDisplay::~PatternDisplay(void) 
{
  if (frm) delete frm;
}




// ------------------------------------------------------------------------------------------------
//
//
int PatternDisplay::update() 
{
  KBKey key,act;
  int ret,kstate,i,val;

  act=0 ;
  ret=0 ;
  key = Keys.checkkey() ;
  kstate = Keys.getstate() ;

  // If play is on
  
  if (ztPlayer->playing) {
    
    // Has the row being displayed as current changed?

    if (this->disp_row != ztPlayer->playing_cur_row) need_refresh++;
  }

  need_redraw++;
    
  if (key) {
    
    if (kstate == KS_NO_SHIFT_KEYS) {

      switch(key) 
      {
      
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case DIK_LEFT:

        cur_track--; act++;
        break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case DIK_RIGHT:

        cur_track++; act++;
        break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case DIK_SPACE:

        toggle_track_mute(this->cur_track); 
        cur_track++; 
        act++;
        need_refresh++;
        need_redraw++;
        
        break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case DIK_M: // feature 447390 cm

        toggle_track_mute(this->cur_track) ;
        act++;
        need_refresh++;
        need_redraw++;
        
        break;
      }
    }

    if (kstate == KS_ALT) {

      switch(key) 
      {
      case DIK_1: toggle_track_mute(0); need_refresh++; act++; break;
      case DIK_2: toggle_track_mute(1); need_refresh++; act++; break;
      case DIK_3: toggle_track_mute(2); need_refresh++; act++; break;
      case DIK_4: toggle_track_mute(3); need_refresh++; act++; break;
      case DIK_5: toggle_track_mute(4); need_refresh++; act++; break;
      case DIK_6: toggle_track_mute(5); need_refresh++; act++; break;
      case DIK_7: toggle_track_mute(6); need_refresh++; act++; break;
      case DIK_8: toggle_track_mute(7); need_refresh++; act++; break;
      case DIK_9: toggle_track_mute(8); need_refresh++; act++; break;
      case DIK_0: toggle_track_mute(9); need_refresh++; act++; break;
        
      case DIK_F9: 
        toggle_track_mute(this->cur_track); 
        need_refresh++;
        need_redraw++;
        act++;
        break;
        
      case DIK_F10: 
        val = 0;
        for(i=0;i<MAX_TRACKS;i++) {

          if (i==this->cur_track) {

            if (song->track_mute[i]) val = 1;
          } 
          else {

            if (!song->track_mute[i]) val = 1;
          }   
        }



        if (val) {
          
          for(i=0;i<MAX_TRACKS;i++) {
            
            if(i==this->cur_track) {
          
              unmutetrack(i);
            }
            else mutetrack(i);
          }
        } 
        else {

          for(i=0;i<MAX_TRACKS;i++) unmutetrack(i);
        }


        need_refresh++ ;
        need_redraw++ ;
        act++ ;

        break;
      }
    }
    
    if(act) {

      Keys.getkey() ;
      need_refresh++ ;
      need_redraw++ ;
      
      if(cur_track>this->tracks+this->starttrack-1) {
        
        this->starttrack++;
        
        if (this->starttrack > MAX_TRACKS - this->tracks) {

          this->starttrack = MAX_TRACKS - this->tracks ;
          cur_track = MAX_TRACKS - 1;
        }
      }
      
      if(cur_track<this->starttrack) {
        
        if (this->starttrack>0) this->starttrack--;
        else cur_track = 0;
      }
    }
  }

  this->update_frame();
  return ret;
}



//int a55 = 0 ;

// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::draw(Drawable *S, int active) 
{
  if (ztPlayer->playing) this->disp_playing_pattern(S);
  else this->disp_track_headers(S);
  
  frm->draw(S,0);
  
  screenmanager.UpdateWH(col(frm->x), row(frm->y-1), col(frm->xsize+1), row(frm->ysize+1));
  
  changed = 0;
}



// ------------------------------------------------------------------------------------------------
//
//
int PatternDisplay::next_order(void) 
{
  int pass=0,cur_order;

  cur_order = ztPlayer->playing_cur_order+1;

  while(song->orderlist[cur_order] > 0xFF && pass<3) {

    if (song->orderlist[cur_order] == 0x100) cur_order = 0;
    
    while(song->orderlist[cur_order] == 0x101 && cur_order < 256) cur_order++;
    
    pass++;
  }

  return song->orderlist[cur_order];
}



// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::disp_playing_row(int x, int y, int pattern, int row, Drawable *S, TColor bg) 
{
  char str[40];
  int ctrack;
  TColor color;
  event *e;
////  //  int i;
  
  sprintf(str, "%.3d",row);
  printBG(col(x),row(y),str,COLORS.Text,COLORS.Background,S);

  for (ctrack = 0; ctrack<tracks; ctrack++) {
  
    e = song->patterns[pattern]->tracks[ctrack+starttrack]->get_event(row);
    
////    if (e) ctrack = ctrack;

    this->printNote(str,e);
////    //      switch(i) {
    color = COLORS.EditText;
////    //              break;
////    //      }
    printBG(col(x + 4 + (ctrack*tracksize)),row(y),str,color,bg,S);
  }
}




// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::update_frame(void) 
{
  switch(this->cur_pat_view) 
  {
    
  case 0:

    tracks = 24;
    tracksize = 3;

    break;

  case 1:


    tracksize = 7;
    
    // <Manu> Control del número de tracks que se muestran mientras se reproduce una canción (10 por defecto)
    //tracks = 10;
    tracks = (RESOLUTION_X / (tracksize * DEFAULT_FONT_SIZE_X)) - 1 ;

    break ;

  default:

    break ;
  } ;
  
  frm->x = x+4;
  frm->y = y;
  frm->ysize=ysize;
  frm->xsize=tracks*tracksize;
  
}



// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::disp_track_headers(Drawable *S) 
{
  TColor bg;
  char str[50];
  int num_track,p=0;
  
  for(num_track = this->starttrack; num_track < (this->starttrack + this->tracks); num_track++) {    

//    if (this->cur_pat_view >1) sprintf(str," Track %.2d ",num_track+1);
//    else sprintf(str,"%.2d",num_track+1);
    
    // <Manu> Cambio el codigo comentado por este switch

    switch(this->cur_pat_view)
    {
    case 0:

      sprintf(str,"%.2d",num_track+1);

      break ;

    case 1:
    
      sprintf(str,"Trk %.2d",num_track+1);

      break ;

    default:

      sprintf(str," Track %.2d ",num_track+1);

      break ;
    };
    
    
    if (song->track_mute[num_track]) {
      
      if (num_track == cur_track) bg = COLORS.EditText;
      else bg = COLORS.Lowlight;
    } 
    else {
      
      if (num_track == cur_track) bg = COLORS.Brighttext;
      else bg = COLORS.Black;
    }
    
    printBG(col(4+x+(p*(this->tracksize))),row(y-1),str,bg,COLORS.Lowlight,S);
    p++;
  }
}





// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::disp_playing_pattern(Drawable *S) 
{
  int cy, rows, cur_row, pat, last ;
  int next ;
  TColor bg;
  

  //int dontchangeitasshole = 0
  
  last = 0;
  next = next_order();

  this->disp_row = cur_row = ztPlayer->playing_cur_row;

  rows = ysize / 2 ;
  cur_row -= rows;
  pat = ztPlayer->playing_cur_pattern;
  
  for (cy = y; cy<y+ysize; cy++) {
    
    if (cur_row < 0) {
      
      pat = ztPlayer->last_pattern;
      last = 1;
      
      if (pat<0) pat = ztPlayer->playing_cur_pattern;
      cur_row = song->patterns[pat]->length + cur_row ;
      }
     /* else
       if (!dontchangeitasshole)
       pat = ztPlayer->playing_cur_pattern;
    */

    if (cur_row >= song->patterns[pat]->length) {

      if (ztPlayer->playmode) {

        //dontchangeitasshole = 1;

        if (last) {

          pat = ztPlayer->playing_cur_pattern;
          last = 0;
        } 
        else {

          pat = next;
        }
      }
      
      cur_row = 0;
    }

    if (cur_row == this->disp_row) bg = COLORS.EditBGhigh;
    else bg = COLORS.EditBG;

    this->disp_playing_row(x,cy,pat,cur_row++,S,bg);
  }
  
  this->disp_track_headers(S);    
}



// ------------------------------------------------------------------------------------------------
//
//
char *PatternDisplay::printNote(char *str, event *r) 
{
  char note[4],in[3],vol[3],len[4],fx[3],fxd[5];

  if (!r) r = &blank_event;
  
  hex2note(note,r->note);
  
  if (r->vol < 0x80) {
  
    sprintf(vol,"%.2x",r->vol);
    vol[0] = toupper(vol[0]);
    vol[1] = toupper(vol[1]);
  } 
  else strcpy(vol,"..");

  if (r->inst<MAX_INSTS) {

    sprintf(in,"%.2d",r->inst);
    in[0] = toupper(in[0]);
    in[1] = toupper(in[1]);
  } 
  else {
    
   // <MANU> Mientras no expanda en X esto, cambio la linea comentada por esta
   //        para que no se vea raro

    // strcpy(in,"..");
     strcpy(in,"  ");
  }

  if (r->length>0x0) {
  
    if (r->length>999) sprintf(len,"INF");
    else sprintf(len,"%.3d",r->length);
  } 
  else strcpy(len,"...");

  if (r->effect<0xFF) {
    
    sprintf(fx,"%c",r->effect);
    fx[0] = toupper(fx[0]);
  } 
  else strcpy(fx,".");

  sprintf(fxd,"%.4x",r->effect_data);

  fxd[0] = toupper(fxd[0]);
  fxd[1] = toupper(fxd[1]);
  fxd[2] = toupper(fxd[2]);
  fxd[3] = toupper(fxd[3]);
  
  switch(this->cur_pat_view) 
  {
  case 0:
    sprintf(str,"%.3s",note,vol); // 2 cols
    break;
  case 1:
  
    // <MANU> Cambio la linea comentada por esta
    
    //sprintf(str,"%.3s %.2s",note,vol); // 2 cols
    sprintf(str,"%.3s %.2s%.1s",note,in,fx); // 2 cols
    break;
  case 2:
    sprintf(str,"%.3s %.2s %.2s %.3s",note,in,vol,len); // 4 cols
    break;
  case 3:
    sprintf(str,"%.3s %.2s %.2s %.3s %s%.4s",note,in,vol,len,fx,fxd); // 7 cols
    // NOT IN VL CH LEN fx PARM 
    break;
  }

  return str;
}
