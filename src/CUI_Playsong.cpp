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

    // <Manu> ¿Por qué 20?
    //p->y = 20 ;
    pattern_display->y = 14 ;
    
    // <Manu> Control del número de líneas que se muestran mientras se reproduce una canción (30 por defecto)
    //p->ysize = 30 ;

    pattern_display->ysize = (INTERNAL_RESOLUTION_Y / FONT_SIZE_Y) - pattern_display->y - 7 ; // 7 es la parte del fondo (meter en algún #define)
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
CUI_Playsong::~CUI_Playsong(void) 
{
 //   if (UI) delete UI; UI = NULL;
  if (UI_PatternDisplay) delete UI_PatternDisplay; UI_PatternDisplay = NULL;
  if (UI_VUMeters) delete UI_VUMeters; UI_VUMeters = NULL;
  UI = NULL;
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

    case DIK_RIGHT: 
    case DIK_ADD:
      
      if ((key==DIK_RIGHT && kstate == KS_CTRL) || key!=DIK_RIGHT) {
       
        if (ztPlayer->playing) ztPlayer->ffwd();
      }
        
      break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    case DIK_LEFT:
    case DIK_SUBTRACT:        // <Manu> Was DIK_MINUS
      
      if ((key==DIK_LEFT && kstate == KS_CTRL) || key!=DIK_RIGHT)
        if (ztPlayer->playing)
          ztPlayer->rewind();
        break;


      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

        
    case DIK_S:
      
      Keys.insert(SDLK_F10, KMOD_ALT);
      act++;
      
      break;

      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

    case DIK_G:
      
      cur_edit_pattern = ztPlayer->playing_cur_pattern;
      switch_page(UIP_Patterneditor);
      cur_state = STATE_PEDIT;
      UIP_Patterneditor->clear = 1;
      
      break;
      
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------
      // ----------------------------------------------------------------------

#ifndef DISABLE_VUMETERS

    case DIK_PGUP:
    case DIK_PGDN:
      
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
  pattern_display->ysize = (INTERNAL_RESOLUTION_Y / FONT_SIZE_Y) - pattern_display->y - 7 ; // 7 es la parte del fondo (meter en algún #define)

  //  char str[256];
  if (S->lock()==0) {
    
    if (clear > 0) {
      
      // <Manu> cambio res
      //S->fillRect(0,row(15),INTERNAL_RESOLUTION_X,row(50)/*410*/,COLORS.Background);
      S->fillRect(0, row(15), INTERNAL_RESOLUTION_X, INTERNAL_RESOLUTION_Y - (640 - row(50))/*410*/,COLORS.Background); // <Manu> Necesario?

      clear = 0 ;
    }
    
    printtitle(PAGE_TITLE_ROW_Y,"Play Song",COLORS.Text,COLORS.Background,S);
    need_refresh = 0;
    updated=2;
    
    UI->draw(S);
    
    S->unlock();
    
  }
}




