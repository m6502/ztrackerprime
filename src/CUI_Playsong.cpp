#include "zt.h"
#include "PatternDisplay.h"


#define DISABLE_VUMETERS           // <Manu> It's been broken forever, and I don't use it - Will reenable when it's fixed


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
    pattern_display->y = 14 ;
    
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
    vu->cur_order = vu->cur_pattern = vu->cur_row = 0;
    vu->num_channels = 32;
    UI_VUMeters->add_element(vu,0);

    UI = UI_PatternDisplay;
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

#ifndef DISABLE_VUMETERS

    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN:
      
      if (UI == UI_PatternDisplay) UI = UI_VUMeters;
      else UI = UI_PatternDisplay;
      
      clear++;
      act++;
      
      break;

#endif

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
      
      // <Manu> cambio res [EN: resolution change]
      //S->fillRect(0,row(15),INTERNAL_RESOLUTION_X,row(50)/*410*/,COLORS.Background);
      S->fillRect(0, row(15), INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y - (640 - row(50))/*410*/,COLORS.Background); // <Manu> Necesario? [EN: needed?]

      clear = 0 ;
    }
    
    printtitle(PAGE_TITLE_ROW_Y,"Play Song (F5)",COLORS.Text,COLORS.Background,S);
    need_refresh = 0;
    updated=2;
    
    UI->draw(S);
    
    S->unlock();
  }
}


