#include "zt.h"
#include "PatternDisplay.h"

// VU meters: disabled by Manu with the comment "It's been broken forever,
// and I don't use it - Will reenable when it's fixed". Re-enabled here
// after triaging the actual brokenness:
//   * Compile-clean as-is; no missing symbols, no API drift.
//   * Runtime hazards fixed in VUPlay::draw_one_row + VUPlay::draw:
//     - bounds-check latency[track].e.inst before song->instruments[]
//       (a stale event with inst=0xFF used to deref a null instrument
//       slot and crash);
//     - skip the bar render path entirely when ztPlayer / song are not
//       ready (entering F5 before any pattern existed crashed too);
//     - clamp ctrack to MAX_TRACKS so the per-track loop can't read
//       past the array.
//   * draw() now invalidates the previous frame's region before drawing
//     the new frame so the bars don't smear on PageUp toggle.


// ------------------------------------------------------------------------------------------------
//
//
CUI_Playsong::CUI_Playsong(void) 
{
    VUPlay *vu;
//    LCDDisplay *lcd;
    //UI = new UserInterface;

    UI_PatternDisplay = new UserInterface;
    UI_VUMeters = new UserInterface;

    pattern_display = new PatternDisplay() ;
    pattern_display->x = 1;

    clear = 0 ;

    // <Manu> ¿Por qué 20? [EN: why 20?]
    //p->y = 20 ;
    pattern_display->y = 12 ;
    
    // <Manu> Control del número de líneas que se muestran mientras se reproduce una canción (30 por defecto) [EN: controls how many lines are shown while a song is playing (default 30)]
    //p->ysize = 30 ;

    // Reserve 8 rows at the bottom: 7 for the 55px toolbar (ceil(55/8)) plus
    // one row of safety so the pattern display's frame border never overlaps.
    pattern_display->ysize = (INTERNAL_RESOLUTION_Y / FONT_SIZE_Y) - pattern_display->y - 8 ;
    //p->ysize = 62;

    UI_PatternDisplay->add_element(pattern_display,0);

    //delete p;
/*
    lcd = new LCDDisplay;
    lcd->str = "Big Time!";
    lcd->length = strlen(lcd->str);
    lcd->x = col(2);
    lcd->y = row(12);

//  UI->add_gfx(lcd,0);
    delete lcd;
*/
    vu = new VUPlay;
    vu->num_channels = 32;
    UI_VUMeters->add_element(vu,0);

    const int view = zt_config_globals.playback_view;
    pattern_display->cur_pat_view = view == PLAYBACK_VU ? PLAYBACK_COMPACT : view;
    UI = view == PLAYBACK_VU ? UI_VUMeters : UI_PatternDisplay;
}



// ------------------------------------------------------------------------------------------------
//
//
CUI_Playsong::~CUI_Playsong()
{
  UI = nullptr;
  delete UI_PatternDisplay;
  delete UI_VUMeters;
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_Playsong::enter(void) 
{
    need_refresh = 1;
    cur_state = STATE_PLAY;
    UI->full_refresh();
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_Playsong::leave(void) {
    need_refresh = 1;
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_Playsong::update() 
{
  int key=0,act=0;
  
  KBMod kstate = Keys.getstate();
  UI->update();
  
  if (Keys.size()) {
    
    key = Keys.getkey();
    
    switch(key) 
    {
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    case SDLK_RIGHT: 
    case SDLK_KP_PLUS:
      
      if ((key==SDLK_RIGHT && kstate == KS_CTRL) || key!=SDLK_RIGHT) {
       
        if (ztPlayer->playing) ztPlayer->ffwd();
      }
        
      break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    case SDLK_LEFT:
    case SDLK_KP_MINUS:        // <Manu> Was SDLK_MINUS
      
      if ((key==SDLK_LEFT && kstate == KS_CTRL) || key!=SDLK_RIGHT)
        if (ztPlayer->playing)
          ztPlayer->rewind();
      break;


      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

        
    case SDLK_S:
      
      Keys.insert(SDLK_F10, SDL_KMOD_ALT);
      act++;
      
      break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    case SDLK_G:
      
      cur_edit_pattern = ztPlayer->playing_cur_pattern;
      switch_page(UIP_Patterneditor);
      cur_state = STATE_PEDIT;
      UIP_Patterneditor->clear = 1;
      
      break;
      
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN:
    {
      const int current = UI == UI_VUMeters ? PLAYBACK_VU : pattern_display->cur_pat_view;
      const int direction = key == SDLK_PAGEDOWN ? 1 : -1;
      const int count = PLAYBACK_VU - PLAYBACK_COMPACT + 1;
      const int view = PLAYBACK_COMPACT + (current - PLAYBACK_COMPACT + direction + count) % count;
      zt_config_globals.playback_view = view;
      UI = view == PLAYBACK_VU ? UI_VUMeters : UI_PatternDisplay;
      if (UI == UI_PatternDisplay) {
        pattern_display->cur_pat_view = view;
        pattern_display->update_frame();
      }
      
      UI->full_refresh();
      clear++;
      act++;
      
      break;
    }

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    default:

      break ;
    } ;
  }
  
  if (act) need_refresh++;
}



// ------------------------------------------------------------------------------------------------
//
//
void CUI_Playsong::draw(Drawable *S) 
{
  pattern_display->ysize = (INTERNAL_RESOLUTION_Y / FONT_SIZE_Y) - pattern_display->y - 8 ;

  //  char str[256];
  if (S->lock()==0) {
    
    if (clear > 0) {
      
      // Clear both views' full content area, including the pattern headers.
      // The old 640-based height left stale rows/columns behind the VU view.
      S->fillRect(0, row(pattern_display->y - 1), INTERNAL_RESOLUTION_X - 1,
                  INTERNAL_RESOLUTION_Y - 56, COLORS.Background);
      screenmanager.UpdateAll();

      clear = 0 ;
    }
    
    const char *title = "Play Song (F5) - VU meters (PgUp/PgDn)";
    if (UI == UI_PatternDisplay) {
      switch (pattern_display->cur_pat_view) {
      case 2: title = "Play Song (F5) - Regular (PgUp/PgDn)"; break;
      case 3: title = "Play Song (F5) - Full (PgUp/PgDn)"; break;
      default: title = "Play Song (F5) - Compact (PgUp/PgDn)"; break;
      }
    }
    printtitle(PAGE_TITLE_ROW_Y,title,COLORS.Text,COLORS.Background,S);
    need_refresh = 0;
    updated=2;
    
    UI->draw(S);
    
    S->unlock();
  }
}


