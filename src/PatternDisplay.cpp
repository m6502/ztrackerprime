#include "zt.h"
#include "pattern_note_format.h"
#include "PatternDisplay.h"
#include "pattern_display_layout.h"
#include "track_color.h"
#include <algorithm>



// ------------------------------------------------------------------------------------------------
//
//
PatternDisplay::PatternDisplay(void) 
{
    frame = new Frame;
    
    // <Manu> Por defecto estaba a 0, pero yo quiero cur_pat_view a 1 [EN: default was 0, but I want cur_pat_view = 1]
    
    cur_pat_view = 1;

    starttrack = 0; 
    cur_track = 0;
    disp_row = 0;
    disp_pattern = -1;
    disp_order = 0;
    disp_playmode = 0;
    disp_song_generation = -1;
}




// ------------------------------------------------------------------------------------------------
//
//
PatternDisplay::~PatternDisplay(void) 
{
  if (frame) delete frame;
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

      case SDLK_LEFT:

        cur_track--; act++;
        break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case SDLK_RIGHT:

        cur_track++; act++;
        break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case SDLK_SPACE:

        toggle_track_mute(this->cur_track); 
        cur_track++; 
        act++;
        need_refresh++;
        need_redraw++;
        
        break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

      case SDLK_M: // feature 447390 cm

        toggle_track_mute(this->cur_track) ;
        act++;
        need_refresh++;
        need_redraw++;
        
        break;
      }
    }

    if (KS_HAS_ALT(kstate)) {

      switch(key) 
      {
      case SDLK_1: toggle_track_mute(0); need_refresh++; act++; break;
      case SDLK_2: toggle_track_mute(1); need_refresh++; act++; break;
      case SDLK_3: toggle_track_mute(2); need_refresh++; act++; break;
      case SDLK_4: toggle_track_mute(3); need_refresh++; act++; break;
      case SDLK_5: toggle_track_mute(4); need_refresh++; act++; break;
      case SDLK_6: toggle_track_mute(5); need_refresh++; act++; break;
      case SDLK_7: toggle_track_mute(6); need_refresh++; act++; break;
      case SDLK_8: toggle_track_mute(7); need_refresh++; act++; break;
      case SDLK_9: toggle_track_mute(8); need_refresh++; act++; break;
      case SDLK_0: toggle_track_mute(9); need_refresh++; act++; break;
        
      case SDLK_F9: 
        toggle_track_mute(this->cur_track); 
        need_refresh++;
        need_redraw++;
        act++;
        break;
        
      case SDLK_F10: 
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
            else {
              mutetrack(i);
            }
          }
        } 
        else {

          for(i=0;i<MAX_TRACKS;i++) {
            unmutetrack(i);
          }
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
    }
  }

  this->update_frame();
  return ret;
}



//int a55 = 0 ;

// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::draw(Drawable *S, int)
{
  if (disp_song_generation != zt_song_generation) {
    disp_pattern = -1;
    disp_song_generation = zt_song_generation;
    S->fillRect(col(x), row(y), col(frame->x + frame->xsize) - 1,
                row(y + ysize) - 1, COLORS.Background);
  }

  // A stopped view still needs to rebuild its notes after focus recovery,
  // window exposure, a popup, or switching back from the VU meters.
  if (ztPlayer && (ztPlayer->playing || disp_pattern >= 0))
    this->disp_playing_pattern(S);
  else
    this->disp_track_headers(S);

  frame->ysize = ysize ;
  frame->draw(S,0);
  
  screenmanager.UpdateWH(col(frame->x), row(frame->y-1), col(frame->xsize+1), row(frame->ysize+1));
  
  changed = 0;
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
    // Paint the content separately from the editor-style track separator.
    // This also clears the last digit when a NUL effect shortens the text.
    const int length = (int)strlen(str);
    memset(str + length, ' ', tracksize - 1 - length);
    str[tracksize - 1] = '\0';

    color = COLORS.EditText;
    TColor cellbg = bg;
    TColor tc = song->track_color[ctrack+starttrack];
    if (tc) {
      // <Manu> current row gets an adaptive cursor shade so it reads
      // clearly while staying in the track's hue; other rows use the base.
      cellbg = (bg == COLORS.EditBGhigh) ? zt_track_cursor_bg(tc, 1) : tc;
      color  = zt_track_text_for_bg(cellbg);
    }
    printBG(col(x + 4 + (ctrack*tracksize)),row(y),str,color,cellbg,S);
    const int separator_x = col(x + 4 + (ctrack + 1) * tracksize - 1);
    printBG(separator_x,row(y)," ",COLORS.Text,COLORS.Background,S);
    if (ctrack < tracks - 1)
      printchar(separator_x,row(y),168,COLORS.Lowlight,S);
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

    tracksize = 7;

    break;

  case 1:


    tracksize = 8; // note, instrument, effect letter, separator
    
    break ;

  case 2:
    tracksize = 14; // note, instrument, volume, length, separator
    break;

  case 3:
    tracksize = 20; // regular fields, effect + parameter, separator
    break;

  default:
    cur_pat_view = 1;
    tracksize = 8;
    break ;
  } ;
  
  // Leave room for row numbers, the left border and the right border.
  const int available = INTERNAL_RESOLUTION_X / FONT_SIZE_X - (x + 4) - 1;
  tracks = (std::max)(1, (std::min)(MAX_TRACKS, available / tracksize));

  // Reconcile the viewport after navigation, format changes and resizing.
  cur_track = (std::max)(0, (std::min)(MAX_TRACKS - 1, cur_track));
  starttrack = (std::max)(0, (std::min)(MAX_TRACKS - tracks, starttrack));
  if (cur_track < starttrack) starttrack = cur_track;
  if (cur_track >= starttrack + tracks) starttrack = cur_track - tracks + 1;

  frame->x = x+4;
  frame->y = y;
  frame->ysize=ysize;
  frame->xsize=tracks*tracksize;
  
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

    const char *custom = song->track_name[num_track];
    if (custom[0]) {
      // Keep the last cell free for the gap, even with a maximum-length name.
      snprintf(str, tracksize, "%.*s", ZTM_TRACKNAME_MAXLEN - 1, custom);
    } else switch(this->cur_pat_view)
    {
    case 0:

      snprintf(str,sizeof(str),"%.2d",num_track+1);

      break ;

    case 1:
    
      snprintf(str,sizeof(str),"Trk %.2d",num_track+1);

      break ;

    default:

      snprintf(str,sizeof(str)," Track %.2d ",num_track+1);

      break ;
    };

    const int length = (int)strlen(str);
    const int left = (tracksize - 1 - length) / 2;
    memmove(str + left, str, length);
    memset(str, ' ', left);
    memset(str + left + length, ' ', tracksize - 1 - left - length);
    str[tracksize - 1] = '\0';

    if (song->track_mute[num_track]) {

      if (num_track == cur_track) bg = COLORS.EditText;
      else bg = COLORS.Lowlight;
    }
    else {

      if (num_track == cur_track) bg = COLORS.Brighttext;
      else bg = COLORS.Black;
    }


    TColor hbg = COLORS.Lowlight;
    TColor tc = song->track_color[num_track];
    
    if (tc) {
        
      hbg = (num_track == cur_track) ? zt_track_shade(tc, 24) : zt_track_shade(tc, -16);
      if (!song->track_mute[num_track]) bg = zt_track_text_for_bg(hbg);
    }

    printBG(col(4+x+(p*(this->tracksize))),row(y-1),str,bg,hbg,S);
    // Keep a visible one-character gap between header backgrounds.
    printBG(col(4+x+p*tracksize+tracksize-1),row(y-1)," ",
            COLORS.Text,COLORS.Background,S);
    p++;
  }
}





// ------------------------------------------------------------------------------------------------
//
//
void PatternDisplay::disp_playing_pattern(Drawable *S) 
{
  int cy, rows;
  int playing_row, playing_pattern, playing_order;
  int lengths[ZTM_MAX_PATTERNS];
  TColor bg;

  //int dontchangeitasshole = 0

  // Playback runs on another thread.  Copy its display state once so a
  // pattern boundary cannot leave the top and bottom halves of this frame
  // using different orders/patterns.
  if (ztPlayer->playing) {
    disp_row = ztPlayer->playing_cur_row;
    disp_pattern = ztPlayer->playing_cur_pattern;
    disp_order = ztPlayer->playing_cur_order;
    disp_playmode = ztPlayer->playmode;
  }
  if (disp_pattern < 0 || disp_pattern >= ZTM_MAX_PATTERNS ||
      !song->patterns[disp_pattern]) {
    disp_pattern = -1;
    this->disp_track_headers(S);
    return;
  }
  playing_row = disp_row;
  playing_pattern = disp_pattern;
  playing_order = disp_order;

  for (int i = 0; i < ZTM_MAX_PATTERNS; ++i)
    lengths[i] = song->patterns[i] ? song->patterns[i]->length : 0;

  rows = ysize / 2 ;
  
  for (cy = y; cy<y+ysize; cy++) {
    PatternDisplayPosition pos = pattern_display_position_at_offset(
        song->orderlist, lengths, disp_playmode,
        playing_order, playing_pattern, playing_row, cy - (y + rows));

    // The playhead is the fixed centre screen row.  Comparing only the row
    // number also highlighted the same numbered row in the previous/next
    // patterns (especially visible when each pattern reached row 00).
    if (cy == y + rows) bg = COLORS.EditBGhigh;
    else bg = COLORS.EditBG;

    this->disp_playing_row(x,cy,pos.pattern,pos.row,S,bg);
  }
  
  this->disp_track_headers(S);    
}



// ------------------------------------------------------------------------------------------------
//
//
char *PatternDisplay::printNote(char *str, event *r)
{
  if (!r) r = &blank_event;
  switch (cur_pat_view) {
  case 0: return format_pattern_note(str, *r, PatternNoteView::Volume, ' ');
  case 1: return format_pattern_note(str, *r, PatternNoteView::InstrumentEffect, ' ');
  case 2: return format_pattern_note(str, *r, PatternNoteView::Regular, ' ');
  case 3: return format_pattern_note(str, *r, PatternNoteView::Full, ' ');
  }
  return str;
}
