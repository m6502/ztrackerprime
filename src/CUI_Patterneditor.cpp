#include "zt.h"
#include "platform/undo.h"
#include "ccizer.h"

// KS_META stub for Cmd key support (SDL3 maps macOS Cmd to SDL_KMOD_GUI).
// Currently keybuffer does not translate GUI -> KS_META, so KS_META is a
// compile-time no-op here. PR-C (configurable keybindings) defines the real
// bits; keeping this stub lets the same source compile on both branches.
#ifndef KS_META
#define KS_META 0
#endif

static PatternUndo g_undo;

// Save undo snapshot before destructive pattern operations
#define UNDO_SAVE() g_undo.save(song, cur_edit_pattern)

#define LEFT_MARGIN                     40
#define RIGHT_MARGIN                    2

#define TRACKS_POS_Y                    row(TRACKS_ROW_Y)
#define TRACKS_FIRST_NOTE_POS_Y         row(TRACKS_ROW_Y + 1)

// Rows reserved below the pattern so it can never overlap the lower frame.
// The toolbar bitmap is 55px tall (TOOLBAR_SIZE_Y in main.cpp) and is drawn at
// INTERNAL_RESOLUTION_Y - 55. At 8px per character row that's ceil(55/8) = 7
// rows; we reserve one extra row so the pattern's bottom border never touches
// the toolbar. Matches the legacy layout (40 rows = 0..039 at 480px height).
#define SPACE_AT_BOTTOM                 7


int PATTERN_EDIT_ROWS = 100;
int m_upd = 0;
event *upd_e = NULL;

int max_displayable_rows = 0 ;
int g_posx_tracks = 0 ;

int last_unused_key = 0 ;

static int clamp_int(int value, int min_value, int max_value)
{
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

static void adjust_pattern_scroll_after_row_count_change(int old_edit_rows, int new_edit_rows)
{
  if (old_edit_rows == new_edit_rows) return;
  if (new_edit_rows < 1) return;
  if (!song || !song->patterns[cur_edit_pattern]) return;

  int pattern_len = song->patterns[cur_edit_pattern]->length;
  if (pattern_len < 1) return;

  int old_visible_rows = old_edit_rows;
  if (old_visible_rows > pattern_len) old_visible_rows = pattern_len;
  if (old_visible_rows < 1) old_visible_rows = 1;

  int new_visible_rows = new_edit_rows;
  if (new_visible_rows > pattern_len) new_visible_rows = pattern_len;
  if (new_visible_rows < 1) new_visible_rows = 1;

  int old_max_scroll = pattern_len - old_visible_rows;
  if (old_max_scroll < 0) old_max_scroll = 0;

  int old_top = clamp_int(cur_edit_row_disp, 0, old_max_scroll);
  int cursor_row = clamp_int(cur_edit_row, 0, pattern_len - 1);
  int cursor_offset = cursor_row - old_top;

  if (cursor_offset < 0) cursor_offset = 0;
  if (cursor_offset > old_visible_rows - 1) cursor_offset = old_visible_rows - 1;

  int new_top;

  if (cursor_offset >= old_visible_rows / 2) {
    int rows_below_cursor = old_visible_rows - 1 - cursor_offset;
    new_top = cursor_row - (new_visible_rows - 1 - rows_below_cursor);
  }
  else {
    new_top = cursor_row - cursor_offset;
  }

  int new_max_scroll = pattern_len - new_visible_rows;
  if (new_max_scroll < 0) new_max_scroll = 0;

  cur_edit_row_disp = clamp_int(new_top, 0, new_max_scroll);
}

// ------------------------------------------------------------------------------------------------
//
//
void set_effect_data_msg(char *str, unsigned char effect, unsigned short int effect_data)
{
  int temp = effect_data;

  switch(effect) 
  {

    case 'T':
      if (effect_data<60) effect_data = 60;
      if (effect_data>240) effect_data = 240;
      sprintf(str,"Effect: Set Tempo to %d bpm ",effect_data);
      break;


    case 'A':
      if (effect_data == 2  || 
          effect_data == 4  ||
          effect_data == 6  ||
          effect_data == 8  ||
          effect_data == 12 ||
          effect_data == 24 ||
          effect_data == 48 
          )            
        sprintf(str,"Effect: Set TPB to %d",effect_data);
      else
        sprintf(str,"Effect: Set TPB (invalid TPB: %d) ",effect_data);
      break;


  case 'B':

    // <Manu> Nuevo comando B para hacer loops en la GBA; pintamos la informacion [EN: new B command for GBA loops; we draw the info]
    sprintf(str,"Command: Write GBA loop marker to pattern %d when exporting MIDI", effect_data);

    break ;

    
  case 'D':
    sprintf(str,"Effect: Delay note by %d ticks",effect_data);
    break;
  case 'E':
    sprintf(str,"Effect: Pitchwheel slide down  %.4X=%.5d ",temp,temp);
    break;
  case 'F':
    sprintf(str,"Effect: Pitchwheel slide up  %.4X=%.5d ",temp,temp);
    break;
  case 'W':
    sprintf(str,"Effect: Pitchwheel set: %.4X (2000 is reset)",temp);
    break;
  case 'C':
    sprintf(str,"Effect: Jump to row %d of next pattern",temp);
    break;
  case 'Q':
    sprintf(str,"Effect: Retrigger note every %d subticks",temp);
    break;
  case 'P':
    sprintf(str,"Effect: Send program change for current instrument");
    break;
  case 'S':
    sprintf(str,"Effect: Send CC: Controller: %d (0x%.2X) value: %d (0x%.2X)",GET_HIGH_BYTE(effect_data),GET_HIGH_BYTE(effect_data), GET_LOW_BYTE(effect_data), GET_LOW_BYTE(effect_data));
    break;
  case 'X':
    effect_data = GET_LOW_BYTE(effect_data);
    if (effect_data<0x40) {
      temp = 100-((100*effect_data)/0x39);
      //if (temp<-100)
      sprintf(str,"Effect: Set pan %.3d%% left ",temp);
    }
    else if (effect_data>0x40) {
      temp = ((100*(effect_data-0x41))/0x40);
      if (effect_data>=0x7f) temp = 100;
      sprintf(str,"Effect: Set pan %.3d%% right ",temp);
    }
    else sprintf(str,"Effect: Set pan to center");
    break;
#ifdef USE_ARPEGGIOS
  case 'R':
    if (temp)
      sprintf(str,"Effect: Start arpeggio %d",temp);
    else
      sprintf(str,"Effect: Continue arpeggio");
    break;
#endif /* USE_ARPEGGIOS */
#ifdef USE_MIDIMACROS
  case 'Z':
    if (GET_HIGH_BYTE(temp))
      sprintf(str,"Effect: Send midimacro %d, with param %d",GET_HIGH_BYTE(temp),GET_LOW_BYTE(temp));
    else
      sprintf(str,"Effect: Send last used midimacro with param %d",GET_LOW_BYTE(temp));
    break;
#endif /* USE_MIDIMACROS */
  default:
    sprintf(str,"Effect Data:  %.4X=%.5d  %.2X=%.3d  %.2X=%.3d",temp,temp,(temp&0xFF00)>>8,(temp&0xFF00)>>8,temp&0x00FF,temp&0x00FF);
    break;
  }
}






// ------------------------------------------------------------------------------------------------
//
//
char *printNote(char *str, event *r, int cur_edit_mode) 
{
  char note[4],ins[3],vol[3],len[4],fx[3],fxd[5];

  hex2note(note,r->note);
  
  if (r->vol < 0x80) {

    sprintf(vol,"%.2x",r->vol);
    vol[0] = toupper(vol[0]);
    vol[1] = toupper(vol[1]);
  } 
  else strcpy(vol,"..");

  
  if (r->inst<MAX_INSTS) {

    sprintf(ins,"%.2d",r->inst);
    ins[0] = toupper(ins[0]);
    ins[1] = toupper(ins[1]);
  } 
  else strcpy(ins,"..");
  
  
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

  switch(cur_edit_mode)
  {
  case VIEW_SQUISH:
    sprintf(str,"%.3s %.2s",note,vol); // 2 cols
    break;
  case VIEW_REGULAR:
    sprintf(str,"%.3s %.2s %.2s %.3s",note,ins,vol,len); // 4 cols
    break;
  case VIEW_BIG:
    sprintf(str,"%.3s %.2s %.2s %.3s %s%.4s",note,ins,vol,len,fx,fxd); // 7 cols
    // NOT IN VL CH LEN fx PARM 
    break;
  case VIEW_FX:
    sprintf(str,"%.3s %.2s %s%.4s",note,vol,fx,fxd);
    break;
    /*
    case VIEW_EXTEND:
    sprintf(str,"%.3s %.2s %.2s %.2s ... .. .... .. .... .. .... .. .... .. ....",note,ins,vol,ch); // 15 cols
    // NOT IN VL CH LEN fx PARM*5 
    break;
    */
  }
  
  return str;
  
  // 4  .!. .. 
  // 8  .!. .. .. ..
  // 17 .!. .. .. .. ... .. ....
  // 40 .!. .. .. .. ... .. .... .. .... .. .... .. .... .. ....
}





#ifdef __AAAA5251785AAAA55AAA

// ------------------------------------------------------------------------------------------------
// <Manu> Debería mirar si puedo hacer esto de una manera un poco más limpia [EN: should look into doing this in a slightly cleaner way]
//
char *printBlankNote(char *str, int cur_edit_mode) 
{
  switch(cur_edit_mode) {
  case VIEW_SQUISH:
    sprintf(str,"%.3s %.2s","",""); // 2 cols
    break;
  case VIEW_REGULAR:
    sprintf(str,"%.3s %.2s %.2s %.3s","","","",""); // 4 cols
    break;
  case VIEW_BIG:
    sprintf(str,"%.3s %.2s %.2s %.3s %s%.4s","","","","","",""); // 7 cols
    // NOT IN VL CH LEN fx PARM 
    break;
  case VIEW_FX:
    sprintf(str,"%.3s %.2s %s%.4s","","","","");
    break;
    /*
    case VIEW_EXTEND:
    sprintf(str,"%.3s %.2s %.2s %.2s ... .. .... .. .... .. .... .. .... .. ....",note,in,vol,ch); // 15 cols
    // NOT IN VL CH LEN fx PARM*5 
    break;
    */
  }
  
  return str;
  
  // 4  .!. .. 
  // 8  .!. .. .. ..
  // 17 .!. .. .. .. ... .. ....
  // 40 .!. .. .. .. ... .. .... .. .... .. .... .. .... .. ....
}


#endif



// ------------------------------------------------------------------------------------------------
//
//
void draw_track_markers(int tracks_shown, int field_size, Drawable *S) 
{
  static char str[256];
  int j;
  int p=0;

  for(j=cur_edit_track_disp;j<cur_edit_track_disp+tracks_shown;j++) {

    if (zt_config_globals.cur_edit_mode != VIEW_SQUISH) sprintf(str," Track %.2d ",j+1);
    else sprintf(str," %.2d ",j+1);

    if (song->track_mute[j]) printBG(col(g_posx_tracks + (p*(field_size+1))),row(TRACKS_ROW_Y),str,COLORS.Text,COLORS.Lowlight,S);
    else printBG(col(g_posx_tracks + (p*(field_size+1))),row(TRACKS_ROW_Y),str,COLORS.Brighttext,COLORS.Lowlight,S);
    
    p++;
  }
}








// ------------------------------------------------------------------------------------------------
//
//
void draw_bar(int x, int y, int xsize, int ysize, int value, int maxval, TColor bg, Drawable *S) 
{
  int howfar;

  howfar = value*(xsize-1)/maxval;
  
  for (int cy=y;cy<y+ysize;cy++) {

    S->drawHLine(cy,x,x+xsize,bg);
    if (value>0) S->drawHLine(cy,x,x+howfar,COLORS.LCDMid);
  }
  
  if (value>0) {

    S->drawHLine(y,x,x+howfar,COLORS.LCDHigh);
    S->drawVLine(x,y,y+ysize-1,COLORS.LCDHigh);
    S->drawHLine(y+ysize-1,x,x+howfar,COLORS.LCDLow);
    S->drawVLine(x+howfar,y,y+ysize-1,COLORS.LCDLow);
  }
}






// ------------------------------------------------------------------------------------------------
//
//
void draw_bar_rev(int x, int y, int xsize, int ysize, int value, int maxval, TColor bg, Drawable *S) 
{
  int howfar;
  int w=x+xsize,s;
  howfar = (maxval-value)*(xsize-1)/maxval;
  s = w-howfar;
  S->fillRect(x,y,w,y+ysize-1,bg);
  S->fillRect(s,y,w,y+ysize-1,COLORS.LCDMid);
  /*
  for (int cy=y;cy<y+ysize;cy++) {
  S->drawHLine(cy,x,w,bg);
  //      if (value>0)
  S->drawHLine(cy,s,w,COLORS.LCDMid);
  }
  */
  //  if (value>0) {
  S->drawHLine(y,s,w,COLORS.LCDHigh);
  S->drawVLine(s,y,y+ysize-1,COLORS.LCDHigh);
  S->drawHLine(y+ysize-1,s,w,COLORS.LCDLow);
  S->drawVLine(w,y,y+ysize-1,COLORS.LCDLow);
  //  }
}








// ------------------------------------------------------------------------------------------------
//
//
void draw_signed_bar(int x, int y, int xsize, int ysize, int value, int maxval, TColor bg, Drawable *S) {
  //int howfar;
  /*
  for (int cy=y;cy<y+ysize;cy++) {
  S->drawHLine(cy,x,x+xsize-1,bg);
  }
  */  S->fillRect(x,y,x+xsize-1,y+ysize-1,bg);
  
  if (value>=maxval/2)
    draw_bar(x+(xsize/2),y,xsize/2,ysize,value-(maxval/2),maxval/2,bg,S);
  else {
    draw_bar_rev(x,y,xsize/2,ysize,value,maxval/2,bg,S);
  }
}









// ------------------------------------------------------------------------------------------------
//
//
void disp_gfxeffect_pattern(int tracks_shown, int field_size, int cols_shown, Drawable *S, int upd_one=0,event *which_e=NULL) 
{
  int num_displayed_tracks,num_displayed_rows,data,datamax;
  TColor bg;
  event *e;
  char str[64];

  (void)cols_shown;

  printline(col(5),row(TRACKS_ROW_Y),148,(tracks_shown * (field_size+1))-1,COLORS.Lowlight,S); // 0x89
  printline(col(5),row(TRACKS_ROW_Y+(PATTERN_EDIT_ROWS+1)),143,(tracks_shown * (field_size+1))-1,COLORS.Highlight,S); // 0x89
  
  draw_track_markers(tracks_shown, field_size,S);
  
  printchar(col(4),row(TRACKS_ROW_Y),139,COLORS.Lowlight,S);


  num_displayed_rows = 0 ;

  for(int var_row=cur_edit_row_disp;var_row<(cur_edit_row_disp+PATTERN_EDIT_ROWS);var_row++) {
  
    if (!upd_one) {

      sprintf(str,"%.3d",var_row);
    
      if (ztPlayer->playing) {
      
        if (var_row==ztPlayer->playing_cur_row && ztPlayer->playing_cur_pattern==cur_edit_pattern) {
          
          printBG(col(1),row(TRACKS_ROW_Y + 1 + num_displayed_rows),str,COLORS.Highlight,COLORS.Background,S);
        }
        else {
          
          printBG(col(1),row(TRACKS_ROW_Y + 1 + num_displayed_rows),str,COLORS.Text,COLORS.Background,S);
        }
      } 
      else printBG(col(1),row(TRACKS_ROW_Y + 1 + num_displayed_rows),str,COLORS.Text,COLORS.Background,S);
      
      printchar(col(4),row(TRACKS_ROW_Y + 1 + num_displayed_rows),146,COLORS.Lowlight,S);
      printchar(col(4+(tracks_shown*(field_size+1))),row(TRACKS_ROW_Y + 1 + num_displayed_rows),145,COLORS.Highlight,S);
    }
    
    
    num_displayed_tracks=0;
    
    for(int var_track=cur_edit_track_disp;var_track <cur_edit_track_disp+tracks_shown; var_track++) {
    
      bg = COLORS.EditBG;
      
      if (!(var_row%zt_config_globals.lowlight_increment)) bg = COLORS.EditBGlow;
      if (!(var_row%zt_config_globals.highlight_increment)) bg = COLORS.EditBGhigh;
      
      if (!(e = song->patterns[cur_edit_pattern]->tracks[var_track]->get_event(var_row))) e = &blank_event;
      
      //          printNote(str,e,cur_edit_mode);
      //          printBG(col(5+(num_displayed_tracks*(field_size+1))),row(TRACKS_ROW_Y + 1 + num_displayed_rows),str,fg,bg,S);
      
      if (upd_one && e != which_e && upd_one==1) goto please_skip; // ...

      switch(UIP_Patterneditor->md_mode) 
      {
      
      case MD_VOL:
      
        data = e->vol;
        
        if (e->vol>0x7F) {
        
          if (e->inst < MAX_INSTS) data = song->instruments[e->inst]->default_volume;
          else data = 0x0;
        }

        datamax = 0x7F;
        
        break;

      case MD_FX:
      case MD_FX_SIGNED:

        data = e->effect_data & 0x7F;
        datamax = 0x7F;

        break;
      } 


      switch(UIP_Patterneditor->md_mode)
      {

      case MD_FX_SIGNED:
        
        draw_signed_bar(col(5+(num_displayed_tracks*(field_size+1))),row(TRACKS_ROW_Y + 1 + num_displayed_rows),col(field_size)-1,row(1),data,datamax,bg,S);
        
        break;

      default:
        
        draw_bar(col(5+(num_displayed_tracks*(field_size+1))),row(TRACKS_ROW_Y + 1 + num_displayed_rows),col(field_size)-1,row(1),data,datamax,bg,S);
        
        break;
      } ;
      
      
      if (var_track<(cur_edit_track_disp+tracks_shown-1)) {
        
        printchar(col(5+((num_displayed_tracks+1)*(field_size+1))-1),row(TRACKS_ROW_Y + 1 + num_displayed_rows),168,COLORS.Lowlight,S) ;
      }

please_skip:

      num_displayed_tracks++;
    }
    
    num_displayed_rows++;
  }

  printchar(col(4),                               row(TRACKS_ROW_Y),                   153, COLORS.Lowlight,  S) ;
  printchar(col(4+(tracks_shown*(field_size+1))), row(TRACKS_ROW_Y),                   152, COLORS.Lowlight,  S) ;
  printchar(col(4),                               row(TRACKS_ROW_Y + 1 + PATTERN_EDIT_ROWS), 151, COLORS.Lowlight,  S) ;
  printchar(col(4+(tracks_shown*(field_size+1))), row(TRACKS_ROW_Y + PATTERN_EDIT_ROWS), 150, COLORS.Highlight, S) ;
}





// ------------------------------------------------------------------------------------------------
//
//
void disp_pattern(int tracks_shown, int field_size, int cols_shown, Drawable *S) 
{
  int num_displayed_rows, num_displayed_tracks, special ;
  TColor bg,fg;
  static char str[2048] ;    // <Manu> Aumento esto de 512 a 2048 por si se utiliza una resolución muy grande. 512 debería servir para w=4096 pero... [EN: bumping this from 512 to 2048 in case a very high resolution is used. 512 should be enough for w=4096 but...]
  
  int posx_current_track ;
  int posy_current_row ;
  //  char note[4];

  (void)cols_shown;

  max_displayable_rows = PATTERN_EDIT_ROWS ;
  if(max_displayable_rows > song->patterns[cur_edit_pattern]->length) max_displayable_rows = song->patterns[cur_edit_pattern]->length ;

  int first_row = cur_edit_row_disp ;

  int last_row = first_row + max_displayable_rows ;
  if(last_row > song->patterns[cur_edit_pattern]->length) last_row = song->patterns[cur_edit_pattern]->length ;

  int blank_rows = PATTERN_EDIT_ROWS - max_displayable_rows /*- 1*/ ;

  int first_blank_row = first_row + max_displayable_rows ;



  //printBG(col(5),row(15),
 
  /*
  num_displayed_tracks=0;
  for(var_track=cur_edit_track_disp;var_track<cur_edit_track_disp+tracks_shown;var_track++) {
  if (cur_edit_mode != VIEW_SQUISH)
  sprintf(str," Track %.2d ",var_track+1);
  else
  sprintf(str," %.2d ",var_track+1);
  if (song->track_mute[var_track])
  printBG(col(5+(num_displayed_tracks*(field_size+1))),row(TRACKS_ROW_Y),str,COLORS.Text,COLORS.Lowlight,S);
  else
  printBG(col(5+(num_displayed_tracks*(field_size+1))),row(TRACKS_ROW_Y),str,COLORS.Brighttext,COLORS.Lowlight,S);
  num_displayed_tracks++;
} */



  // <Manu> Cumple alguna función dibujar este carácter? [EN: does drawing this character serve any purpose?]
  //printchar(col(4), row(TRACKS_ROW_Y), 139, COLORS.Lowlight,S);

  
  int total_pixels = INTERNAL_RESOLUTION_X /*- LEFT_MARGIN - RIGHT_MARGIN*/ ;
  int track_pixels = ((tracks_shown * (field_size+1)) * FONT_SIZE_X) + LEFT_MARGIN  ;
  int extra_pixels = total_pixels - track_pixels ;
  
  // Left-align the track grid: no centering, just LEFT_MARGIN.
  int poscharx_tracks = LEFT_MARGIN / FONT_SIZE_X ;
  (void)extra_pixels;
  g_posx_tracks = poscharx_tracks ;


  num_displayed_rows = 0 ; // <Manu> Esto va de 0 al número máximo de rows a dibujar - 1 [EN: this goes from 0 to (max rows to draw) - 1]
  
  
  for(int var_row = first_row; var_row < (last_row + blank_rows); var_row++) {

    posy_current_row = TRACKS_FIRST_NOTE_POS_Y + row(num_displayed_rows) ;

    // Determine if this row needs special treatment

    if(var_row == cur_edit_row) special = 1;
    else special = 0;


    // Is this row number inside the allowed number?

    if(var_row >= first_blank_row) {

      sprintf(str,"   ") ;
    }
    else {

      if(var_row >= 0) { // <Manu> ¿Necesario? [EN: needed?]
        
        sprintf(str,"%.3d",var_row);
      
        // Línea vertical izquierda del marco de los tracks
        printchar(col(poscharx_tracks - 1),                               posy_current_row, 146, COLORS.Lowlight,S) ;

        // Línea vertical derecha del marco de los tracks
        printchar(col((poscharx_tracks - 1) + (tracks_shown*(field_size+1))), posy_current_row, 145, COLORS.Highlight,S) ;
      }
      
    }


    // Dibujar el número de línea
    if (ztPlayer->playing && 
        ztPlayer->playing_cur_pattern==cur_edit_pattern &&
        var_row == ztPlayer->playing_cur_row) {
        
        // Actual en play
        printBG(col(poscharx_tracks - 4),posy_current_row,str,COLORS.Highlight,COLORS.Background,S);
    } 
    else {
      
      // Normal
      printBG(col(poscharx_tracks - 4),posy_current_row,str,/*0xFF0000*/COLORS.Text,COLORS.Background,S);
    }


    {
      unsigned long Background, Highlight ;

      if(song->patterns[cur_edit_pattern]->custom_colors) {

          Background = song->patterns[cur_edit_pattern]->Background ;
          Highlight = song->patterns[cur_edit_pattern]->Highlight ;
      }
      else {

          Background = COLORS.Background ;
          Highlight = COLORS.Highlight ;
      }


      if(var_row >= 0) {       // <Manu> ¿Necesario?
    
            // Contenido de los tracks

            num_displayed_tracks = 0 ;

            for(int var_track = cur_edit_track_disp; var_track < (cur_edit_track_disp + tracks_shown); var_track++) {

              unsigned long EditText, EditBG, EditBGlow, EditBGhigh ;

              if(song->patterns[cur_edit_pattern]->tracks[var_track]->custom_colors) {

                Background = song->patterns[cur_edit_pattern]->Background ;
                Highlight = song->patterns[cur_edit_pattern]->Highlight ;
              }
              else {

                EditText = song->patterns[cur_edit_pattern]->tracks[var_track]->EditText ;
                EditBG = song->patterns[cur_edit_pattern]->tracks[var_track]->EditBG ;
                EditBGlow = song->patterns[cur_edit_pattern]->tracks[var_track]->EditBGlow ;
                EditBGhigh = song->patterns[cur_edit_pattern]->tracks[var_track]->EditBGhigh ;
              }

              //if(var_track == 3)
              //{
              //  //EditText = song->patterns[cur_edit_pattern]->tracks[var_track]->EditText ;
              //  EditBG = 0xFFFFFF ;
              //  //EditBGlow = song->patterns[cur_edit_pattern]->tracks[var_track]->EditBGlow ;
              //  //EditBGhigh = song->patterns[cur_edit_pattern]->tracks[var_track]->EditBGhigh ;
              //}



              posx_current_track = col(poscharx_tracks) + col(num_displayed_tracks * (field_size + 1)) ;

              // <Manu> Si esta línea hay que dibujarla en blanco dibujamos las columnas de los tracks con el color del fondo [EN: if this line should be drawn blank, draw the track columns with the background color]

              if(var_row >= first_blank_row) {

                //fg = COLORS.EditText; // <Manu> Innecesario ? [EN: unnecessary?]
                //bg = COLORS.EditBG;

                //printBlankNote(str, zt_config_globals.cur_edit_mode) ;
                //printBG(col(poscharx_tracks+(num_displayed_tracks*(field_size+1))), row(TRACKS_ROW_Y + 1 + num_displayed_rows), str, fg, bg, S) ;

                // Lo dibujamos un píxel a la izquierda y un píxel más largo para comernos la posible línea del marco de los tracks

                int x1 = posx_current_track - 2  ;
                int x2 = x1 + col(field_size + 1) + 1  ;

                int y1 = posy_current_row ;
                int y2 = y1 + FONT_SIZE_Y ;

                S->fillRect(x1, y1, x2, y2, Background) ;
              }
              else {

                if(special) {
                    
                    bg = COLORS.CursorRowLow ;
                    if (!(var_row % zt_config_globals.lowlight_increment)) {
                
                        bg = COLORS.CursorRowHigh ;
                    }

                    if (!(var_row % zt_config_globals.highlight_increment)) {
                    
                        bg = COLORS.CursorRowHigh ;
                    }
                }
                else {
                    
                    bg = EditBG ;

                    if (!(var_row % zt_config_globals.lowlight_increment)) {
                
                        bg = EditBGlow ;
                    }

                    if (!(var_row % zt_config_globals.highlight_increment)) {
                    
                        bg = EditBGhigh ;
                    }

                }



                fg = EditText;

                if (selected) {

                  if (var_track>=select_track_start && var_track<=select_track_end) {

                    if (var_row>=select_row_start && var_row<=select_row_end) {

                      fg = Highlight;

                      if (bg == EditBGlow || bg==EditBGhigh) bg = COLORS.SelectedBGHigh;
                      else {

                        if(!special) bg = COLORS.SelectedBGLow;
                        else bg = COLORS.SelectedBGHigh;
                      }
                    }
                  }
                }

                event *e = song->patterns[cur_edit_pattern]->tracks[var_track]->get_event(var_row) ;
                if (e == NULL) e = &blank_event;

                printNote(str, e, zt_config_globals.cur_edit_mode) ;
                printBG(col(poscharx_tracks + (num_displayed_tracks*(field_size+1))), posy_current_row, str, fg, bg, S) ;

                str[edit_cols[cur_edit_col].startx + 1] = 0 ;


                // Dibujar el caret

                if (var_row == cur_edit_row && var_track == cur_edit_track) {

                  printBG(
                          col(poscharx_tracks + edit_cols[cur_edit_col].startx + (num_displayed_tracks * (field_size + 1))),
                          posy_current_row,
                          &str[ edit_cols[cur_edit_col].startx ],
                          EditBG,
                          Highlight,
                          S
                         );
                }

                
                
                
                //

                if (var_track < (cur_edit_track_disp + (tracks_shown - 1)) ) {
                  
                  printchar(
                            col( poscharx_tracks + ((num_displayed_tracks + 1) * (field_size + 1)) - 1), 
                            posy_current_row, 
                            168, 
                            COLORS.Lowlight, 
                            S
                           ) ;



                }
              
              }

              num_displayed_tracks++;
            

            } // for(;;)

      } // if(var_row >= 0)


    }

    
    num_displayed_rows++;
  }


  //static int old_num_displayed_rows = 0 ;
  //
  //if(num_displayed_rows != old_num_displayed_rows) {
  //
  //  old_num_displayed_rows = num_displayed_rows ;
  //
  //  need_refresh++ ;
  //  clear++ ;
  //}




/*
  if(blank_rows > 0) {

    int x1 = col(1) ;
    int x2 = INTERNAL_RESOLUTION_X - 1 ;

    int y1 = TRACKS_POS_Y + row(first_blank_row + 1) ;
    int y2 = TRACKS_POS_Y + row(PATTERN_EDIT_ROWS + 1) ;

    
    
    //y2 = y1 + 10 ;

    S->fillRect(x1, y1, x2, y2, COLORS.Highlight) ;
//    screenmanager.Update(x1, y1, x2, y2);
  }
*/




  // Línea superior de los tracks
  printline(col(poscharx_tracks),TRACKS_POS_Y, 148,(tracks_shown * (field_size+1))-1,COLORS.Lowlight,S); // 0x89

  // Línea inferior de los tracks
  printline(col(poscharx_tracks),TRACKS_POS_Y + row((PATTERN_EDIT_ROWS - blank_rows) + 1), 143,(tracks_shown * (field_size+1))-1,COLORS.Highlight,S); // 0x89
  
  // Etiquetas de los tracks
  draw_track_markers(tracks_shown, field_size, S);

  // Puntos que cierran la parte alta del marco de los tracks

  printchar(col((poscharx_tracks - 1)),                               TRACKS_POS_Y,                                 153, COLORS.Lowlight, S) ;
  printchar(col((poscharx_tracks - 1)+(tracks_shown*(field_size+1))), TRACKS_POS_Y,                                 152, COLORS.Lowlight, S) ;

  // Puntos que cierran la parte baja del marco de los tracks

  printchar(col((poscharx_tracks - 1)),                               TRACKS_POS_Y + row(max_displayable_rows + 1), 151, COLORS.Lowlight,  S) ;
  printchar(col((poscharx_tracks - 1)+(tracks_shown*(field_size+1))), TRACKS_POS_Y + row(max_displayable_rows + 1), 150, COLORS.Highlight, S) ;

  
  
  //printchar(col(poscharx_tracks), row(80), 'P', COLORS.Highlight, S) ;
}




// ------------------------------------------------------------------------------------------------
//
//
CUI_Patterneditor::CUI_Patterneditor(void)
{
  mode = PEM_REGULARKEYS;
  md_mode = MD_VOL;
  last_cur_pattern = -1 ;
  last_pattern_size = -1 ;
}

// ------------------------------------------------------------------------------------------------
//
//
void CUI_Patterneditor::enter(void)
{
  need_refresh = 1;
  clear = 1;
  cur_state = STATE_PEDIT;
  mousedrawing=0;
  //PATTERN_EDIT_ROWS = 32 + 4 + (INTERNAL_RESOLUTION_Y - 480) / 8;
  
  // Dynamic row count based on current window height - see issue #15.

  PATTERN_EDIT_ROWS = CHARS_Y - TRACKS_ROW_Y - 1 - SPACE_AT_BOTTOM ;

  //  this->mode = PEM_REGULARKEYS;
}








// ------------------------------------------------------------------------------------------------
//
//
void CUI_Patterneditor::leave(void)
{
  
}






// ------------------------------------------------------------------------------------------------
//
//
void CUI_Patterneditor::update()
{
  int old_pattern_edit_rows = PATTERN_EDIT_ROWS;
  PATTERN_EDIT_ROWS = CHARS_Y - TRACKS_ROW_Y - 1 - SPACE_AT_BOTTOM ;
  if (PATTERN_EDIT_ROWS < 1) PATTERN_EDIT_ROWS = 1;
  int pattern_edit_rows_changed = (old_pattern_edit_rows != PATTERN_EDIT_ROWS);

  int i,j,p=0,step=0,noplay=0,bump=0;
  unsigned char set_note=0xff,kstate;
  int key;
  unsigned int scancode;
  int temp,oktogo=0/*,dontc=0*/;
  short int p1,p2,p3;
  //  char str[256];
  event *e;
  //char note[4];
  unsigned char val,pval;
  float istart,iend,istep,iadd;   
  int amount, con;
  int note_row, prev_note_row, x, prev_row;
  
  
  clear = 0;
  keypress = 0;
  if (pattern_edit_rows_changed) {
    adjust_pattern_scroll_after_row_count_change(old_pattern_edit_rows, PATTERN_EDIT_ROWS);
    need_refresh++;
    clear++;
  }
  //memset(note,0,4);

//  int pattern_visible ;
//
//  if(ztPlayer->playing) {
//  
//    pattern_visible = ztPlayer->cur_pattern ;
//  }



  // <Manu> Esto arregla el bug del último número de línea destacado cuando el play pasa por el pattern actual y sigue por otro. [EN: this fixes the highlighted last-line-number bug when playback passes through the current pattern and continues in another one]
  //        No es 100% perfecto por culpa del prebuffer, pero bueno...

  if(!ztPlayer->playing) last_cur_pattern = -1 ;
  else {

    if(ztPlayer->playing_cur_row == zt_config_globals.prebuffer_rows) {

      if(ztPlayer->cur_pattern != last_cur_pattern) {
    
        last_cur_pattern = ztPlayer->cur_pattern ;
        need_refresh++ ;
      }
    }
  }


  // <Manu> Esto arreglaba el bug de que no se refresque la pantalla al cambiar [EN: this used to fix the bug where the screen wasn't refreshed on change]
  //        el número de filas del pattern actual, pero ya no es necesario :-)
  //
  //if(ztPlayer->song->patterns[cur_edit_pattern]->length != last_pattern_size) {
  //
  //  last_pattern_size = ztPlayer->song->patterns[cur_edit_pattern]->length ;
  //  need_refresh++ ;
  //  clear++ ;
  //}

//#define _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS

#ifndef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
  zt_config_globals.cur_edit_mode = VIEW_BIG ;//VIEW_SQUISH ;
#else

  if (Keys.size()) {

    key = Keys.checkkey();
    kstate = Keys.getstate();
    keyed++;

    if((kstate & KS_CTRL) && (key == SDLK_TAB)) {

      if(kstate & KS_SHIFT) {
        
        zt_config_globals.cur_edit_mode-- ;
        if(zt_config_globals.cur_edit_mode < VIEW_SQUISH) zt_config_globals.cur_edit_mode = VIEW_BIG ;
      }
      else {
        
        zt_config_globals.cur_edit_mode++ ;
        if (zt_config_globals.cur_edit_mode > VIEW_BIG) zt_config_globals.cur_edit_mode = VIEW_SQUISH ;
      }
      

      

      need_refresh++;
      
      clear++;
      key=Keys.getkey();
      scancode=Keys.getcode();
      kstate = Keys.getstate();
    }
  }

#endif



  
  static char view_mode_status[64];
  // Only post the "View mode: …" status message when the mode or zoom
  // actually changes. Previously it was rewritten on every refresh,
  // overwriting any other status message and feeling spammy on entry.
  static int  prev_view_mode = -1;
  static float prev_zoom     = -1.0f;
  bool view_mode_changed = (zt_config_globals.cur_edit_mode != prev_view_mode)
                       || (ZOOM != prev_zoom);

  switch(zt_config_globals.cur_edit_mode)
  {

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_SQUISH:

    if (view_mode_changed) {
      sprintf(view_mode_status, "View mode: Squish (%.1fx)", ZOOM);
      statusmsg = view_mode_status;
      status_change = 1;
    }

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / (FONT_SIZE_X * 6) ;
    field_size = 6;
    cols_shown = 4 ;
    init_short_cols();

    break;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_REGULAR:

    if (view_mode_changed) {
      sprintf(view_mode_status, "View mode: Regular (%.1fx)", ZOOM);
      statusmsg = view_mode_status;
      status_change = 1;
    }

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / (FONT_SIZE_X * 14) ;
    field_size = 13;
    cols_shown = 9;
    fix_cols();

    break;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_BIG:

    if (view_mode_changed) {
      sprintf(view_mode_status, "View mode: Big (%.1fx)", ZOOM);
      statusmsg = view_mode_status;
      status_change = 1;
    }

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / 160 ;    // 160 es lo que mide cada columna; dejamos 80 pixels extras de margen
    field_size = 19;
    cols_shown = 14;
    fix_cols();

    break;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_FX:

    if (view_mode_changed) {
      sprintf(view_mode_status, "View mode: FX (%.1fx)", ZOOM);
      statusmsg = view_mode_status;
      status_change = 1;
    }

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / (FONT_SIZE_X * 13) ;

//    if((((INTERNAL_RESOLUTION_X-640)/8) % 13) > .5) tracks_shown++;

    field_size = 12;
    cols_shown = 9;
    init_fx_cols();

    break;
  }
  prev_view_mode = zt_config_globals.cur_edit_mode;
  prev_zoom = ZOOM;

  
  if (cur_edit_track_disp+tracks_shown >= MAX_TRACKS) {    // Fix for bug #454764 -cm
    
    cur_edit_track_disp = MAX_TRACKS - tracks_shown; // 
  }
  
  if (cur_edit_track>(cur_edit_track_disp+tracks_shown-1)) {
    
    cur_edit_track = cur_edit_track_disp+tracks_shown-1;
  }

  if (cur_edit_col > cols_shown-1) {
    
    cur_edit_col = cols_shown -1;
  }
  
  if (ztPlayer->playing && bScrollLock) { /* NIC fixes this so it scrolls in the center */

    int ocer=cur_edit_row,ocerd=cur_edit_row_disp,op=cur_edit_pattern;
    
    if (ztPlayer->playmode)
      cur_edit_pattern = ztPlayer->playing_cur_pattern;
    
    cur_edit_row = ztPlayer->playing_cur_row;
    
    if (cur_edit_row < 0) cur_edit_row = 0 ;

    if (cur_edit_row < cur_edit_row_disp) cur_edit_row_disp = cur_edit_row ;

    if(ztPlayer->playing_cur_row >= (PATTERN_EDIT_ROWS / 2) ) {
      
      cur_edit_row_disp = ztPlayer->playing_cur_row - (PATTERN_EDIT_ROWS / 2) + 1 ;
    }
    
    //    if (ztPlayer->playing_cur_row>song->patterns[cur_edit_pattern]->length-1)
    //        cur_edit_row = song->patterns[cur_edit_pattern]->length-1;
    //      if (ztPlayer->playing_cur_row>cur_edit_row_disp+(PATTERN_EDIT_ROWS-1)-16)
    //        cur_edit_row_disp=ztPlayer->playing_cur_row-(PATTERN_EDIT_ROWS-1)-16;
    
    if (ocer!=cur_edit_row || ocerd!=cur_edit_row_disp || op!=cur_edit_pattern) {
      
      need_refresh++;
    }
  }
  
  //static char test[256] ;
  //statusmsg = test ;
  //extern int last_unused_key ;
  //sprintf(test, "Last pressed key code: %d", last_unused_key) ;
  //need_refresh++ ;
  
  
  if (Keys.size() || midiInQueue.size() > 0) {
    
    key=Keys.getkey();
    scancode=Keys.getcode();
    kstate = Keys.getstate();
    
    set_note = 0xff;
    
    /* COMMON KEYS */
    if (kstate == KS_SHIFT) {

      switch(key)
      {

      case SDLK_GRAVE:
        if (mode==PEM_MOUSEDRAW) {

        /* Here we flush the MIDIin queue so as to
          not  plug them  in the  pattern  editor */
          midiInQueue.clear();
          key = 0;

          mode = PEM_REGULARKEYS;
          statusmsg = "Editing: Pattern";
        }
        else {
          mode = PEM_MOUSEDRAW;
          switch(md_mode) {
          case MD_VOL:       statusmsg = "Editing: Volume"; break;
          case MD_FX:        statusmsg = "Editing: Effect low-byte (0-0x7F)"; break;
          case MD_FX_SIGNED: statusmsg = "Editing: Effect low-byte signed (0-0x7F)"; break;
          default:           statusmsg = "Editing: Volume"; break;
          }
        }
        status_change = 1;
        need_refresh++;
        break;


      case SDLK_KP_PLUS:
        if(cur_edit_order < (ZTM_ORDERLIST_LEN - 1) && song->orderlist[cur_edit_order + 1] != 0x100)
        {
          cur_edit_order++;
          cur_edit_pattern = song->orderlist[cur_edit_order];
          need_refresh++;
        }
        break;

      case SDLK_KP_MINUS:

        if(cur_edit_order > 0 && song->orderlist[cur_edit_order -1] != 0x100)
        {
          cur_edit_order--;
          cur_edit_pattern = song->orderlist[cur_edit_order];
          need_refresh++;
        }
        break;


      } ;
    }



    if (kstate == KS_NO_SHIFT_KEYS) {

      switch(key)
      {
      case SDLK_APOSTROPHE:
      //case SDLK_LEFTBRACKET:
      case SDLK_KP_MINUS:

        if (cur_edit_pattern>0) {

          cur_edit_pattern--;
          need_refresh++; 
        }

        break;

      
      //case 92:
      case SDLK_BACKSLASH: // <ManU> 92 en español, ¿qué pasa con teclados internacionales?
      //case SDLK_RIGHTBRACKET:
      case SDLK_KP_PLUS:

        if (cur_edit_pattern<255) {
          cur_edit_pattern++;
          need_refresh++; 
        }

        break;

      default:

          last_unused_key = key ;

          break ;

      }           
    }
    
    
    /* START KEYS */
    
    switch(mode) {
      
    case PEM_MOUSEDRAW:
      
/*
      if (kstate == KS_SHIFT) {
      }
      if (KS_HAS_ALT(kstate)) {
      }
      if (kstate == KS_CTRL) {
      }
*/

      if (kstate == KS_NO_SHIFT_KEYS) {
        switch(key) {
          
        case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_LEFT)):
        case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_UP << 8) | SDL_BUTTON_RIGHT)): 
          mousedrawing=0;
          break;
          
        case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_LEFT)):
          if (    MousePressX >= col(5) 
            && MousePressX <  col(5+(tracks_shown*(field_size+1)))
            && MousePressY >= row(TRACKS_ROW_Y + 1)
            && MousePressY <= row(TRACKS_ROW_Y + 1 + PATTERN_EDIT_ROWS)
            ) {
            //int x;
            //x = (MousePressX - col(5)) / 8;
            //x -= (field_size+1)*(x/(field_size+1));
            //if (x<field_size)
            mousedrawing=1;
            LastX = MousePressX;
            LastY = MousePressY;
          }
          break;
        case ((unsigned int)((SDL_EVENT_MOUSE_BUTTON_DOWN << 8) | SDL_BUTTON_RIGHT)):
          if (    MousePressX >= col(5) 
                && MousePressX <  col(5+(tracks_shown*(field_size+1)))
                && MousePressY >= row(TRACKS_ROW_Y + 1)
                && MousePressY <= row(TRACKS_ROW_Y + 1 + PATTERN_EDIT_ROWS)
                ) {
            //int x;
            //x = (MousePressX - col(5)) / 8;
            //x -= (field_size+1)*(x/(field_size+1));
            //if (x<field_size)
            mousedrawing=2;
            LastX = MousePressX;
            LastY = MousePressY;
            m_upd=2;
          }
          break;
          
        case SDLK_TAB:
          md_mode++;
          if (md_mode>=MD_END)
            md_mode = 0;
          need_refresh++;
          break;

        case SDLK_DOWN:
          cur_edit_row_disp++;
          need_refresh++;
          break;

        case SDLK_UP:
          cur_edit_row_disp--;
          need_refresh++;
          break;

        case SDLK_LEFT:
          cur_edit_track_disp--;
          need_refresh++;
          break;

        case SDLK_RIGHT:
          cur_edit_track_disp++;
          need_refresh++;
          break;

        case SDLK_PAGEDOWN:
          cur_edit_row_disp+= zt_config_globals.highlight_increment * 4 ;
          need_refresh++;
          break;

        case SDLK_PAGEUP:
          cur_edit_row_disp-= zt_config_globals.highlight_increment * 4 ;    // <Manu> ¿Esto no debería ser un múltiplo del step? [EN: shouldn't this be a multiple of step?]
          need_refresh++;
          break;

        case SDLK_END:
          cur_edit_row_disp=0xFFF;
          need_refresh++;
          break;

        case SDLK_HOME:
          cur_edit_row_disp = 0;
          need_refresh++;
          break;
        } ;
      }
      
      
      
      
      
      
      if (cur_edit_row_disp >= song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS) {
        
        cur_edit_row_disp = song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS;
      }
      
      if (cur_edit_track_disp >= MAX_TRACKS-tracks_shown) {
        
        cur_edit_track_disp =  MAX_TRACKS - 1 - tracks_shown;
      }
      
      if (cur_edit_row_disp<0)   cur_edit_row_disp = 0;
      
      if (cur_edit_track_disp<0) cur_edit_track_disp = 0;
      
      if (need_refresh/* && !dontc*/) {

        switch(md_mode)
        {
        case MD_VOL:
          statusmsg = "Editing: Volume";
          break;

        case MD_FX:
          statusmsg = "Editing: Effect low-byte (0-0x7F)";
          break;
      
        case MD_FX_SIGNED:
          statusmsg = "Editing: Effect low-byte signed (0-0x7F)";
          break;
        } ;
      }
      
      break;
      
            case PEM_REGULARKEYS:
              
              if (kstate == KS_SHIFT) {
                int effective=0;
                switch(key) {
                case SDLK_PERIOD:
                  cur_inst++;
                  if (cur_inst>=MAX_INSTS)
                    cur_inst = MAX_INSTS-1;
                  need_refresh++;
                  break;
                case SDLK_COMMA:
                  cur_inst--;
                  if (cur_inst<0)
                    cur_inst=0;
                  need_refresh++;
                  break;
                  
                  
                  
                  ////////////////////////////////////////////////////////////////////////
                  // NEW SHIFT SELECTION
                  
                case SDLK_LEFT:
                  if(!selected)
                  {
                    selected=1;
                    select_row_start = cur_edit_row;
                    select_row_end = cur_edit_row;
                    select_track_end = cur_edit_track;
                    select_track_start = cur_edit_track;
                  }
                  if(cur_edit_track > 0){
                    cur_edit_track--;
                    if(select_track_start>cur_edit_track)
                      select_track_start = cur_edit_track;
                    else
                      select_track_end = cur_edit_track;
                  }
                  if (cur_edit_track_disp>cur_edit_track)
                    cur_edit_track_disp--;
                  need_refresh++;
                  break;
                case SDLK_RIGHT:
                  if(cur_edit_track >= MAX_TRACKS-1)
                    break;
                  if(!selected)
                  {
                    selected=1;
                    select_row_start = cur_edit_row;
                    select_row_end = cur_edit_row;
                    select_track_start = cur_edit_track;
                    select_track_end = cur_edit_track;
                  }
                  if(cur_edit_track < MAX_TRACKS){
                    cur_edit_track++;
                    if(select_track_end < cur_edit_track)
                      select_track_end = cur_edit_track;
                    else
                      select_track_start = cur_edit_track;
                  }
                  
                  if ((cur_edit_track - cur_edit_track_disp) == (tracks_shown)) {
                    if (cur_edit_track_disp<(MAX_TRACKS-tracks_shown)) {
                      cur_edit_track_disp++;
                    }
                  }
                  need_refresh++;
                  break;
                  
                 
                // <Manu> Clarifico esto un poco [EN: clarifying this a bit]
                  
                case SDLK_PAGEDOWN:
                  
                  effective=zt_config_globals.highlight_increment * 4;
                  // fall through
                
                case SDLK_DOWN:
                
                  if(!effective) effective+=cur_step;
                  if (!effective) effective = 1;
                  
                  if(cur_edit_row < song->patterns[cur_edit_pattern]->length-1) {

                    if(!selected) {

                      selected=1;
                      select_track_start = cur_edit_track;
                      select_track_end = cur_edit_track;
                      select_row_start = cur_edit_row;
                      select_row_end = cur_edit_row+effective;
                    }
                    else{

                      if(select_row_end < cur_edit_row+effective) {

                        if(cur_edit_row+effective < song->patterns[cur_edit_pattern]->length-1) {
                          
                          select_row_end = cur_edit_row+effective;
                        }
                        else {
                          
                          select_row_end = song->patterns[cur_edit_pattern]->length-1;
                        }
                      }
                      else {
                          
                        if(cur_edit_row+effective < song->patterns[cur_edit_pattern]->length-1) {
                            
                          select_row_start = cur_edit_row+effective;
                        }
                        else{
                            
                          // <Manu> if y else eran iguales ¿? [EN: if and else were identical?]
                          select_row_start = song->patterns[cur_edit_pattern]->length-1;
                        }
                      }
                    }
                  }
                  
                  if ((cur_edit_row-1 - cur_edit_row_disp) == (PATTERN_EDIT_ROWS-1)) {
                    
                    if (cur_edit_row_disp<(song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                      
                      cur_edit_row_disp++;
                    }
                  }

                  need_refresh++;
                  
                  break;
                  
                case SDLK_PAGEUP:

                  effective=zt_config_globals.highlight_increment * 4;
                  
                  if(cur_edit_row-effective < cur_edit_row_disp && cur_edit_row != cur_edit_row_disp) {
                    effective = cur_edit_row - cur_edit_row_disp;
                  }
                  else {
                    
                    if(cur_edit_row - effective <= 0) {
                      
                      effective = cur_edit_row ;
                    }
                  }
                  // fall through

                case SDLK_UP:
                  
                  if(!effective) effective+=cur_step;

                  if(cur_edit_row > 0) {

                    if(!selected) {

                      selected=1;
                      select_track_start = cur_edit_track;
                      select_track_end = cur_edit_track;
                      select_row_start = cur_edit_row-effective;
                      select_row_end = cur_edit_row;
                    }
                    else {

                      if(cur_edit_row-effective < select_row_start){
                        
                        if(cur_edit_row-effective >= 0) select_row_start = cur_edit_row-effective;
                        else select_row_start = 0;
                      }
                      else {
                        
                        if(cur_edit_row-effective >= 0) select_row_end = cur_edit_row-effective;
                        else select_row_end = 0;
                      }
                    }
                  }

                  if ((cur_edit_row+1 - cur_edit_row_disp) == 0) {
                    
                    if (cur_edit_row_disp>0) {
                      
                      cur_edit_row_disp--;
                    }
                  }

                  need_refresh++;
                  break;
                  
                  // END NEW SHIFT SELECTION
                  ////////////////////////////////////////////////////////////////////////////////////////

            }
        }


        // ------------------------------------------------------------------
        // Pattern operations (ported from master: Undo, Double, Halve, Clone,
        // Interpolate Selection, Reverse Selection, Rotate Up/Down, Track Solo).
        // Keys are hardcoded here; PR-C (#8) makes them remappable once merged.
        // ------------------------------------------------------------------

        // Undo: ALT+Z / Cmd+Z (Ctrl+Z is already block-clear)
        if ((kstate & KS_ALT || kstate & KS_META) && !(kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_Z) {
          int restored = g_undo.restore(song);
          if (restored >= 0) {
            cur_edit_pattern = restored;
            statusmsg = "Undo";
          } else {
            statusmsg = "Nothing to undo";
          }
          status_change = 1; need_refresh++; key = 0;
        }

        // Cmd+N (macOS): set length of first note of selection to length of
        // the selection. Mirrors the Ctrl+N binding documented in help.txt;
        // on Mac the global Alt+N → New Song would otherwise win because
        // SDL maps Cmd to KS_META|KS_ALT.
        if ((kstate & KS_META) && !(kstate & KS_CTRL) && !(kstate & KS_SHIFT) && key == SDLK_N) {
          if (selected &&
            (e = song->patterns[cur_edit_pattern]->tracks[select_track_start]->get_event(select_row_start))
            ) {
            j = (select_row_end+1 - select_row_start)*(96/song->tpb);
            for(i=select_track_start;i<=select_track_end;i++)
              song->patterns[cur_edit_pattern]->tracks[i]->update_event(select_row_start,-1,-1,-1,j,-1,-1);
            file_changed++;
            need_refresh++;
          }
          key = 0;
        }

        // CC drawmode toggle: Ctrl+Shift+§ (GRAVE). Plain Shift+§ is the
        // existing PEM_MOUSEDRAW toggle (handled above); we use the
        // Ctrl-extended combo so both gestures coexist. While CC drawmode
        // is on, the MIDI-in handler below intercepts incoming CC (0xB0)
        // and Pitchbend (0xE0) messages and writes Sxxyy / Wxxxx effects
        // at the cursor row.
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_GRAVE) {
          g_cc_drawmode = !g_cc_drawmode;
          midiInQueue.clear();
          sprintf(szStatmsg, "CC drawmode: %s", g_cc_drawmode ? "ON  (incoming CC writes S/W effects)" : "OFF");
          statusmsg = szStatmsg; status_change = 1; need_refresh++; key = 0;
        }

        // Double Pattern: Ctrl+Shift+G
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_G) {
          UNDO_SAVE();
          int pat_len = song->patterns[cur_edit_pattern]->length;
          int new_len = pat_len * 2;
          if (new_len <= 256) {
            song->patterns[cur_edit_pattern]->resize(new_len);
            for (int t = 0; t < MAX_TRACKS; t++) {
              track *trk = song->patterns[cur_edit_pattern]->tracks[t];
              if (!trk) continue;
              for (int r = 0; r < pat_len; r++) {
                event *ev = trk->get_event(r);
                if (ev) trk->update_event(r + pat_len, ev->note, ev->inst, ev->vol, ev->length, ev->effect, ev->effect_data);
              }
            }
            sprintf(szStatmsg, "Pattern doubled: %d -> %d rows", pat_len, new_len);
          } else {
            sprintf(szStatmsg, "Cannot double: max 256 rows");
          }
          statusmsg = szStatmsg; status_change = 1; need_refresh++; key = 0;
        }

        // Halve Pattern: Ctrl+Shift+H
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_H) {
          UNDO_SAVE();
          int pat_len = song->patterns[cur_edit_pattern]->length;
          int new_len = pat_len / 2;
          if (new_len >= 32) {
            for (int t = 0; t < MAX_TRACKS; t++) {
              track *trk = song->patterns[cur_edit_pattern]->tracks[t];
              if (!trk) continue;
              for (int r = 0; r < new_len; r++) {
                event *ev = trk->get_event(r * 2);
                if (ev) trk->update_event(r, ev->note, ev->inst, ev->vol, ev->length, ev->effect, ev->effect_data);
                else    trk->update_event(r, blank_event.note, blank_event.inst, blank_event.vol, blank_event.length, blank_event.effect, blank_event.effect_data);
              }
            }
            song->patterns[cur_edit_pattern]->resize(new_len);
            if (cur_edit_row >= new_len) cur_edit_row = new_len - 1;
            sprintf(szStatmsg, "Pattern halved: %d -> %d rows", pat_len, new_len);
          } else {
            sprintf(szStatmsg, "Cannot halve: minimum pattern length is 32 rows");
          }
          statusmsg = szStatmsg; status_change = 1; need_refresh++; key = 0;
        }

        // Humanize Velocities: Alt+H — randomize volume column within selection
        // by +/- HUMANIZE_RANGE on each row that has a note. Paketti-inspired.
        if ((kstate & KS_ALT) && !(kstate & KS_SHIFT) && !(kstate & KS_CTRL) && key == SDLK_H) {
          if (selected) {
            UNDO_SAVE();
            const int HUMANIZE_RANGE = 15; // +/- steps around current value
            int touched = 0;
            for (int t = select_track_start; t <= select_track_end; t++) {
              track *trk = song->patterns[cur_edit_pattern]->tracks[t];
              if (!trk) continue;
              for (int r = select_row_start; r <= select_row_end; r++) {
                event *ev = trk->get_event(r);
                if (!ev) continue;
                if (ev->note >= 0x80) continue;      // skip rows without a real note
                int v = (ev->vol < 0x80) ? ev->vol : 64; // treat blank vol as 64
                int delta = (rand() % (2 * HUMANIZE_RANGE + 1)) - HUMANIZE_RANGE;
                int nv = v + delta;
                if (nv < 0) nv = 0;
                if (nv > 127) nv = 127;
                trk->update_event(r, -1, -1, nv, -1, -1, -1);
                touched++;
              }
            }
            sprintf(szStatmsg, "Humanized %d note velocit%s (+/- %d)",
                    touched, touched == 1 ? "y" : "ies", HUMANIZE_RANGE);
          } else {
            sprintf(szStatmsg, "Humanize: select a block first (SHIFT+arrow)");
          }
          statusmsg = szStatmsg; status_change = 1; need_refresh++; key = 0;
        }

        // Clone Pattern + jump to it: Ctrl+Shift+D
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_D) {
          int src = cur_edit_pattern;
          int dest = -1;
          for (int p = 0; p < 256; p++) {
            if (p != src && song->patterns[p] && song->patterns[p]->isempty()) { dest = p; break; }
          }
          if (dest < 0) {
            for (int p = 0; p < 256; p++) {
              if (p != src && !song->patterns[p]) {
                song->patterns[p] = new pattern(song->patterns[src]->length);
                dest = p; break;
              }
            }
          }
          if (dest >= 0) {
            int pat_len = song->patterns[src]->length;
            song->patterns[dest]->resize(pat_len);
            for (int t = 0; t < MAX_TRACKS; t++) {
              track *st = song->patterns[src]->tracks[t];
              track *dt = song->patterns[dest]->tracks[t];
              if (!st || !dt) continue;
              for (int r = 0; r < pat_len; r++) {
                event *ev = st->get_event(r);
                if (ev) dt->update_event(r, ev->note, ev->inst, ev->vol, ev->length, ev->effect, ev->effect_data);
              }
            }
            cur_edit_pattern = dest;
            sprintf(szStatmsg, "Cloned pattern %03d -> %03d", src, dest);
          } else {
            sprintf(szStatmsg, "No empty pattern slot");
          }
          statusmsg = szStatmsg; status_change = 1; need_refresh++; key = 0;
        }

        // Interpolate Selection: Ctrl+I
        // (Note: the plain Ctrl+I inside the KS_CTRL switch below is the
        // legacy "Interpolate effect" op; this one is richer — it picks the
        // column type from where the cursor is.)
        if (kstate == KS_CTRL && key == SDLK_I && selected) {
          UNDO_SAVE();
          int rows = select_row_end - select_row_start;
          if (rows >= 1) {
            for (int t = select_track_start; t <= select_track_end; t++) {
              track *trk = song->patterns[cur_edit_pattern]->tracks[t];
              if (!trk) continue;
              event *e_start = trk->get_event(select_row_start);
              event *e_end   = trk->get_event(select_row_end);
              if (!e_start || !e_end) continue;
              int ctype = edit_cols[cur_edit_col].type;
              int v0 = 0, v1 = 0;
              if (ctype == T_VOL) {
                v0 = (e_start->vol < 0x80) ? e_start->vol : 0;
                v1 = (e_end->vol   < 0x80) ? e_end->vol   : 0;
              } else if (ctype == T_FXP) {
                v0 = e_start->effect_data;
                v1 = e_end->effect_data;
              } else if (ctype == T_NOTE) {
                v0 = (e_start->note < 0x80) ? e_start->note : -1;
                v1 = (e_end->note   < 0x80) ? e_end->note   : -1;
              } else {
                continue; // unsupported column type, skip track
              }
              if (v0 < 0 || v1 < 0) continue;
              for (int r = select_row_start + 1; r < select_row_end; r++) {
                int pos = r - select_row_start;
                int val = v0 + (v1 - v0) * pos / rows;
                if (ctype == T_VOL)
                  trk->update_event(r, -1, -1, val, -1, -1, -1);
                else if (ctype == T_FXP)
                  trk->update_event(r, -1, -1, -1, -1, -1, val);
                else if (ctype == T_NOTE)
                  trk->update_event(r, val, -1, -1, -1, -1, -1);
              }
            }
            statusmsg = "Interpolated selection";
            status_change = 1; need_refresh++; key = 0;
          }
        }

        // Track Solo: Ctrl+F9
        if ((kstate & KS_CTRL) && !(kstate & KS_SHIFT) && !(kstate & KS_ALT) && key == SDLK_F9) {
          bool all_others_muted = true;
          for (int t = 0; t < MAX_TRACKS; t++) {
            if (t != cur_edit_track && !song->track_mute[t]) {
              all_others_muted = false;
              break;
            }
          }
          if (all_others_muted) {
            for (int t = 0; t < MAX_TRACKS; t++) { unmutetrack(t); }
            statusmsg = "Unsolo: all tracks unmuted";
          } else {
            for (int t = 0; t < MAX_TRACKS; t++) {
              if (t == cur_edit_track) { unmutetrack(t); }
              else                     { mutetrack(t);   }
            }
            sprintf(szStatmsg, "Solo track %d", cur_edit_track + 1);
            statusmsg = szStatmsg;
          }
          status_change = 1; need_refresh++; key = 0;
        }

        // Reverse Selection: Ctrl+Shift+R
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_R) {
          if (selected) {
            UNDO_SAVE();
            for (int t = select_track_start; t <= select_track_end; t++) {
              track *trk = song->patterns[cur_edit_pattern]->tracks[t];
              if (!trk) continue;
              int lo = select_row_start;
              int hi = select_row_end;
              while (lo < hi) {
                event *ea = trk->get_event(lo);
                event *eb = trk->get_event(hi);
                event tmp_a = ea ? *ea : blank_event;
                event tmp_b = eb ? *eb : blank_event;
                trk->update_event(lo, tmp_b.note, tmp_b.inst, tmp_b.vol, tmp_b.length, tmp_b.effect, tmp_b.effect_data);
                trk->update_event(hi, tmp_a.note, tmp_a.inst, tmp_a.vol, tmp_a.length, tmp_a.effect, tmp_a.effect_data);
                lo++; hi--;
              }
            }
            statusmsg = "Selection reversed";
          } else {
            statusmsg = "No selection";
          }
          status_change = 1; need_refresh++; key = 0;
        }

        // Rotate Track Down: Ctrl+Shift+Down
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_DOWN) {
          UNDO_SAVE();
          int pat_len = song->patterns[cur_edit_pattern]->length;
          track *trk = song->patterns[cur_edit_pattern]->tracks[cur_edit_track];
          if (trk && pat_len > 1) {
            event *last = trk->get_event(pat_len - 1);
            event saved = last ? *last : blank_event;
            for (int r = pat_len - 1; r > 0; r--) {
              event *ep = trk->get_event(r - 1);
              if (ep) trk->update_event(r, ep->note, ep->inst, ep->vol, ep->length, ep->effect, ep->effect_data);
              else    trk->update_event(r, blank_event.note, blank_event.inst, blank_event.vol, blank_event.length, blank_event.effect, blank_event.effect_data);
            }
            trk->update_event(0, saved.note, saved.inst, saved.vol, saved.length, saved.effect, saved.effect_data);
          }
          statusmsg = "Rotated track down"; status_change = 1; need_refresh++; key = 0;
        }

        // Rotate Track Up: Ctrl+Shift+Up
        if ((kstate & KS_CTRL) && (kstate & KS_SHIFT) && key == SDLK_UP) {
          UNDO_SAVE();
          int pat_len = song->patterns[cur_edit_pattern]->length;
          track *trk = song->patterns[cur_edit_pattern]->tracks[cur_edit_track];
          if (trk && pat_len > 1) {
            event *first = trk->get_event(0);
            event saved = first ? *first : blank_event;
            for (int r = 0; r < pat_len - 1; r++) {
              event *ep = trk->get_event(r + 1);
              if (ep) trk->update_event(r, ep->note, ep->inst, ep->vol, ep->length, ep->effect, ep->effect_data);
              else    trk->update_event(r, blank_event.note, blank_event.inst, blank_event.vol, blank_event.length, blank_event.effect, blank_event.effect_data);
            }
            trk->update_event(pat_len - 1, saved.note, saved.inst, saved.vol, saved.length, saved.effect, saved.effect_data);
          }
          statusmsg = "Rotated track up"; status_change = 1; need_refresh++; key = 0;
        }


        if (kstate == KS_CTRL ) {


          switch(key)
          {
          case SDLK_A:
            if(selected) {
              popup_window(UIP_PENature);
              Keys.flush();
              key = Keys.checkkey();
              kstate = Keys.getstate();
              need_refresh++;
            }
            break;
            
          case SDLK_W:
            // Clear unused volumes (only on rows whose note is empty).
            // Triggered by Ctrl-W; Cmd-W on macOS reaches us via the
            // separate KS_HAS_ALT block below.
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++) {
                for(j=select_row_start;j<=select_row_end;j++) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                  if (e)
                    if (e->note > 0x7F)
                      e->vol = 0x80;
                }
              }
              need_refresh++;
            }
            break;
            
          case SDLK_N: /* Set length of first note to length of selection */
            if (selected && 
              (e = song->patterns[cur_edit_pattern]->tracks[select_track_start]->get_event(select_row_start))
              ) {
              j = (select_row_end+1 - select_row_start)*(96/song->tpb);
              for(i=select_track_start;i<=select_track_end;i++)
                song->patterns[cur_edit_pattern]->tracks[i]->update_event(select_row_start,-1,-1,-1,j,-1,-1);
              file_changed++;
              need_refresh++;
            }
            break;



          } ;


        }

        if (KS_HAS_ALT(kstate)) {
        
          switch(key)
          {

          case SDLK_1: cur_step=1;  need_refresh++; statusmsg = "Step set to 1"; break;
          case SDLK_2: cur_step=2;  need_refresh++; statusmsg = "Step set to 2"; break;
          case SDLK_3: cur_step=3;  need_refresh++; statusmsg = "Step set to 3"; break;
          case SDLK_4: cur_step=4;  need_refresh++; statusmsg = "Step set to 4"; break;
          case SDLK_5: cur_step=5;  need_refresh++; statusmsg = "Step set to 5"; break;
          case SDLK_6: cur_step=6;  need_refresh++; statusmsg = "Step set to 6"; break;
          case SDLK_7: cur_step=7;  need_refresh++; statusmsg = "Step set to 7"; break;
          case SDLK_8: cur_step=8;  need_refresh++; statusmsg = "Step set to 8"; break;
          case SDLK_9: cur_step=9;  need_refresh++; statusmsg = "Step set to 9"; break;
          case SDLK_0: cur_step=0;  need_refresh++; statusmsg = "Step set to 0"; break;

          case SDLK_W:
            // Cmd-W on macOS (Alt-W elsewhere) does the same as Ctrl-W:
            // clear unused volumes (rows whose note is empty) in selection.
            // macOS users would otherwise hit the OS "Close Window" handler;
            // see zt_macos_disable_cmd_w() in macos_menu.mm.
            if (selected) {
              for (i = select_track_start; i <= select_track_end; i++) {
                for (j = select_row_start; j <= select_row_end; j++) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                  if (e)
                    if (e->note > 0x7F)
                      e->vol = 0x80;
                }
              }
              need_refresh++;
            }
            break;

          case SDLK_P: // copy to new pattern
            // select all of current pattern
            {
              
              selected = 1;
              select_row_start = 0;
              select_row_end = song->patterns[cur_edit_pattern]->length-1;
              select_track_start = 0;
              select_track_end = MAX_TRACKS-1;
              
              // copy to clipboard
              CClipboard tempcb;
              tempcb.copy(); SDL_Delay(50);
              
              x = cur_edit_pattern+1;
              while(x<256 && !song->patterns[x]->isempty()) x++;
              if (x<256) {
                sprintf(szStatmsg,"Copied pattern %d to %d",cur_edit_pattern,x);
                statusmsg = szStatmsg;
                song->patterns[x]->resize(song->patterns[cur_edit_pattern]->length);
                cur_edit_pattern = x;
                tempcb.paste(0,0,0); // insert
              } else {
                statusmsg = "Could not find empty pattern for paste";
              }
            }
            // paste pattern and unselect
            selected = 0;
            need_refresh++;
            
            break;
            
          case SDLK_GRAVE: 
            // lengthalize selection
            note_row = -1;
            prev_note_row = -1;
            if (selected) {
              for (i = select_track_start; i <= select_track_end; i++) {
                if (select_row_end - select_row_start > 0) {
                  for (j = select_row_start; j <= select_row_end; j++) {
                    e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                    if (e && e->note > 0 && e->note < 0x80) {
                      prev_note_row = note_row;
                      note_row = j;
                      
                      // if a note row has been found and there is a previous note row,
                      // set length of previous note (only if no length is currently specified)
                      if (prev_note_row >= 0) {
                        e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(prev_note_row);
                        if (e && e->length == 0) {
                          x = (note_row+1 - prev_note_row)*(96/song->tpb);
                          song->patterns[cur_edit_pattern]->tracks[i]->update_event(prev_note_row,-1,-1,-1,x,-1,-1);
                        }
                      }
                      
                    }
                  } // selected rows
                }
              } // tracks
            }
            need_refresh++;
            break;
            
          case SDLK_LEFTBRACKET: 
            // Decrease all volumes by 8
            if (selected) {
              int inst = -1;
              for (i = select_track_start; i <= select_track_end; i++) {
                for (j = select_row_start; j <= select_row_end; j++) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                  if (e && e->note < 0x80) {
                    int nvol;
                    if (e->inst < MAX_INSTS)
                      inst = e->inst;
                    if (inst != -1) {
                      nvol = e->vol;
                      if (nvol > 0x7F)
                        nvol = song->instruments[inst]->default_volume;
                      nvol -= 8;
                      if (nvol<0) nvol = 0;
                      song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,-1,-1,nvol,-1,-1,-1);
                    }
                  }
                } // selected rows
              } // tracks
            }
            need_refresh++;
            break;
            
          case SDLK_RIGHTBRACKET: 
            // Increase all volumes by 8
            if (selected) {
              int inst = -1;
              for (i = select_track_start; i <= select_track_end; i++) {
                for (j = select_row_start; j <= select_row_end; j++) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                  if (e && e->note < 0x80) {
                    int nvol;
                    if (e->inst < MAX_INSTS)
                      inst = e->inst;
                    if (inst != -1) {
                      nvol = e->vol;
                      if (nvol > 0x7F)
                        nvol = song->instruments[inst]->default_volume;
                      nvol += 8;
                      if (nvol>0x7F) nvol = 0x7F;
                      song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,-1,-1,nvol,-1,-1,-1);
                    }
                  }
                } // selected rows
              } // tracks
            }
            need_refresh++;
            break;
            
          case SDLK_T: 
            // Set all effects to cur row first row in track
            if (selected) {
              
              for (i = select_track_start; i <= select_track_end; i++) {
                if (select_row_end - select_row_start > 0) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(select_row_start);
                  if (!e) e = &blank_event;
                  unsigned char eff = e->effect;
                  unsigned short int eff_d = e->effect_data;
                  for (j = select_row_start+1; j <= select_row_end; j++) {
                    song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,-1,-1,-1,-1,eff,eff_d);
                  } // selected rows
                }
              } // tracks
            }
            need_refresh++;
            break;
            
            
          case SDLK_I: // Interpolate effect
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++) {
                if (select_row_end - select_row_start > 0) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(select_row_start);
                  if (e) istart = (float)(e->effect_data & 0xFF); else istart = 0;
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(select_row_end);
                  if (e) iend = (float)(e->effect_data & 0xFF); else iend = 0;
                  istep = (iend - istart) / (select_row_end-select_row_start);
                  iadd = istart;
                  for(j=select_row_start;j<=select_row_end;j++) {         
                    e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j);
                    if (e) p1 = e->effect_data; else p1 = 0x0000;
                    p1 = p1 & 0xFF00;
                    p1 = p1 + (unsigned char)iadd;
                    song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,-1,-1,-1,-1,-1,p1);
                    iadd += istep;
                  }
                }
              }
              need_refresh++;
            }
            break;
            
          case SDLK_K: // Interpolate volume
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++) {
                if (select_row_end - select_row_start > 0) {
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(select_row_start);
                  if (e) {
                    istart = e->vol; 
                    if (istart>0x7f) {
                      if (e->inst < MAX_INSTS)
                        istart = song->instruments[e->inst]->default_volume;
                      else
                        istart = 0;
                    }
                  } else {
                    istart = 0;
                  }
                  e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(select_row_end);
                  if (e) {
                    iend = e->vol; 
                    if (iend>0x7f) {
                      if (e->inst < MAX_INSTS)
                        iend = song->instruments[e->inst]->default_volume;
                      else
                        iend = 0;
                    }
                  } else {
                    iend = 0;
                  }
                  istep = (iend - istart) / (select_row_end-select_row_start);
                  iadd = istart;
                  for(j=select_row_start;j<=select_row_end;j++) {         
                    song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,-1,-1,(int)iadd,-1,-1,-1);
                    iadd += istep;
                  }
                }
              }
              need_refresh++;
            }
            break;
            
            
          case SDLK_J:
            if(selected) {
              // display the fader menu
              //cur_volume_percent = 100;
              popup_window(UIP_PEVol);
              Keys.flush();
              key = Keys.checkkey();
              kstate = Keys.getstate();
              need_refresh++;
            }
            break;


          // ------------------------------------------------------------------------
          // <Manu> Copy paste commands
          // ------------------------------------------------------------------------
            
          case SDLK_C:
            if (selected) {
              clipboard->copy(); 
              SDL_Delay(50);
            }
            break;

          case SDLK_X:
            if (!last_cmd_keyjazz && selected) {
              clipboard->copy();
              for(i=select_track_start;i<=select_track_end;i++) {
                for(j=select_row_start;j<=select_row_end;j++)
                  song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,0x80,MAX_INSTS,0x80,0x0,0xff,0x0);
              }
              need_refresh++;
              last_keyjazz = SDLK_X;
              last_cmd_keyjazz = 1;
            }
            break;

          case SDLK_V:

            clipboard->paste(cur_edit_track,cur_edit_row,0); // insert
            need_refresh++;
            
            break;

          case SDLK_O:

            clipboard->paste(cur_edit_track,cur_edit_row,1); // overwrite
            need_refresh++;

            break;

          case SDLK_M:

            clipboard->paste(cur_edit_track,cur_edit_row,2); // merge
            need_refresh++;

            break;

          
          
          case SDLK_L:
            if ( selected &&
              select_row_start == 0 &&
              select_row_end == song->patterns[cur_edit_pattern]->length-1 &&
              select_track_start == cur_edit_track &&
              select_track_end == cur_edit_track) {
              selected = 1;
              select_row_start = 0;
              select_row_end = song->patterns[cur_edit_pattern]->length-1;
              select_track_start = 0;
              select_track_end = MAX_TRACKS-1;
            } else {
              selected = 1;
              select_row_start = 0;
              select_row_end = song->patterns[cur_edit_pattern]->length-1;
              select_track_start = select_track_end = cur_edit_track;
            }
            need_refresh++;
            break;
          case SDLK_Z:
            if (selected) {
              if (zclear_presscount) {


                for(i=select_track_start;i<=select_track_end;i++) {
                  for(j=select_row_start;j<=select_row_end;j++) {
                    song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,0x80,MAX_INSTS,0x80,0x0,0xff,0x0);
                  }
                }

                zclear_presscount=0;
                need_refresh++;

              } else {
                zclear_presscount++;
                zclear_flag++;
              }
            }
            break;
          case SDLK_U: // Unselect block
            selected=0;
            need_refresh++;
            break;
          case SDLK_F: // Double block length
            if (selected) {
              int len = select_row_end - select_row_start + 1;
              len *= 2;
              select_row_end = select_row_start + len - 1; 
              if (select_row_end >= song->patterns[cur_edit_pattern]->length )
                select_row_end = song->patterns[cur_edit_pattern]->length - 1;
              need_refresh++;
            }
            break;
          case SDLK_G: // Halve block length
            if (selected) {
              int len = select_row_end - select_row_start + 1;
              len /= 2;
              if (len > 0) 
                select_row_end = select_row_start + len - 1;
              else
                selected = 0;
              need_refresh++;
            }
            break;
          case SDLK_B:  // Set Select Begin block
            if (!selected) {    
              selected = 1;
              select_row_start = cur_edit_row;
              select_row_end = cur_edit_row;
              select_track_start = cur_edit_track;
              select_track_end = cur_edit_track;
            } else {
              select_row_start = cur_edit_row;
              select_track_start = cur_edit_track;
              if (select_track_start > select_track_end) {
                temp = select_track_start;
                select_track_start = select_track_end;
                select_track_end = temp;
              }
              if (select_row_start > select_row_end){
                temp = select_row_start;
                select_row_start = select_row_end;
                select_row_end = temp;
              }
            }
            need_refresh++;
            break;
          case SDLK_E: // Set Block End
            if (!selected) {    
              selected = 1;
              select_row_start = cur_edit_row;
              select_row_end = cur_edit_row;
              select_track_start = cur_edit_track;
              select_track_end = cur_edit_track;
            } else {
              select_row_end = cur_edit_row;
              select_track_end = cur_edit_track;
              if (select_track_start > select_track_end) {
                temp = select_track_start;
                select_track_start = select_track_end;
                select_track_end = temp;
              }
              if (select_row_start > select_row_end){
                temp = select_row_start;
                select_row_start = select_row_end;
                select_row_end = temp;
              }
            }                   
            need_refresh++;
            break;
            
            
            }
        }       
        if (KS_HAS_ALT(kstate)) {
          switch(key) {
            
          case SDLK_GRAVE:
            // find row of previous note in current edit track
            prev_row = -1;
            x = cur_edit_row;
            while (x >= 0 && prev_row == -1) {
              e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(x);
              if (e && e->note > 0 && e->note < 0x80) {
                prev_row = x;
              }
              x--;
            }

            if (prev_row >= 0) {
              j = (cur_edit_row+1 - prev_row)*(96/song->tpb);
              song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(prev_row,-1,-1,-1,j,-1,-1);
              {
                char szNote[4] = {'.','.','.',0};
                hex2note(szNote, e->note);
                sprintf(szStatmsg, "Changed Note (%s, %02X) length to %d", szNote, e->inst, j);
                statusmsg = szStatmsg;
                status_change = 1;
              }
              need_refresh++;
            }
            break;
            
            
          case SDLK_S: /* Set Instrument */
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++) {
                for(j=select_row_start;j<=select_row_end;j++) {
                  if ((e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j))) {
                    if (e->inst < MAX_INSTS)
                      e->inst = cur_inst;
                  }
                }
              }
              need_refresh++;
            }
            break;
            
          case SDLK_D: /* select */
            if ( selected &&
              select_row_start == cur_edit_row &&
              
              select_track_start == cur_edit_track &&
              select_track_end == cur_edit_track ) {
              
              select_row_start = cur_edit_row;
              
              if (select_row_end < cur_edit_row + zt_config_globals.highlight_increment-1)
                select_row_end = cur_edit_row + zt_config_globals.highlight_increment - 1;
              else  {
                
                float size = (float)(select_row_end - select_row_start) ;

                size /= zt_config_globals.highlight_increment;
                
                // <Manu> Cast
                //select_row_end = cur_edit_row + zt_config_globals.highlight_increment*(size+1);
                select_row_end = (int) (cur_edit_row + zt_config_globals.highlight_increment*(size+1)) ;
              }
              
              select_track_start = cur_edit_track;
              select_track_end = cur_edit_track; 
              selected = 1;
              need_refresh++;
            }
            else {
              select_row_start = cur_edit_row;
              select_row_end = cur_edit_row + zt_config_globals.highlight_increment - 1;
              select_track_start = cur_edit_track;
              select_track_end = cur_edit_track; 
              selected = 1;
              need_refresh++;
              
            }
            
            
            
            break;
          case SDLK_R: /* Replicate at Cursor (from Paketti, ALT+R / Cmd+R) */
          {
            UNDO_SAVE();
            // Take rows 0..cur_edit_row-1 (above cursor, not including cursor)
            // as the source chunk on the current track, then repeat that chunk
            // from cur_edit_row to end of pattern.
            int pat_len = song->patterns[cur_edit_pattern]->length;
            int chunk_len = cur_edit_row; // rows 0 through cur_edit_row-1
            if (chunk_len > 0 && cur_edit_row < pat_len) {
              for (j = cur_edit_row; j < pat_len; j++) {
                int src_row = (j - cur_edit_row) % chunk_len;
                event *src = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(src_row);
                if (src) {
                  song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(
                    j, src->note, src->inst, src->vol, src->length, src->effect, src->effect_data);
                } else {
                  // Clear destination row if source is empty
                  song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(
                    j, 0x80, MAX_INSTS, 0x80, 0, 0xFF, 0);
                }
              }
              need_refresh++;
              sprintf(szStatmsg, "Replicated rows 0-%d from cursor (%d rows)", cur_edit_row - 1, pat_len);
              statusmsg = szStatmsg;
              status_change = 1;
            }
          }
          break;

          case SDLK_Q: /* Transpose up */
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++) {
                for(j=select_row_start;j<=select_row_end;j++) {
                  if ((e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j))) {
                    if (e->note < 0x7F)
                      e->note++;
                  }
                }
              }
              need_refresh++;
            }
            break;
          case SDLK_A: /* Transpose down */
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++) {
                for(j=select_row_start;j<=select_row_end;j++) {
                  if ((e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j))) {
                    if (e->note > 0 && e->note < 0x80)
                      e->note--;
                  }
                }
              }
              need_refresh++;
            }
            break;
            
            
          case SDLK_LEFT:
            if (cur_edit_track>cur_edit_track_disp) {
              cur_edit_track--;
            } else {
              cur_edit_track--; cur_edit_track_disp--;
              if (cur_edit_track<0) cur_edit_track=0;
              if (cur_edit_track_disp<0) cur_edit_track_disp=0;
            }
            need_refresh++;
            break;
          case SDLK_RIGHT:
            key = SDLK_TAB; oktogo=1;
            break;
            
          case SDLK_F1: toggle_track_mute(0); need_refresh++; break;
          case SDLK_F2: toggle_track_mute(1); need_refresh++; break;
          case SDLK_F3: toggle_track_mute(2); need_refresh++; break;
          case SDLK_F4: toggle_track_mute(3); need_refresh++; break;
          case SDLK_F5: toggle_track_mute(4); need_refresh++; break;
          case SDLK_F6: toggle_track_mute(5); need_refresh++; break;
          case SDLK_F7: toggle_track_mute(6); need_refresh++; break;
          case SDLK_F8: toggle_track_mute(7); need_refresh++; break;
          case SDLK_F9: toggle_track_mute(cur_edit_track); need_refresh++; break;
          case SDLK_F10: 
            
            val = 0;

            for(i=0;i<MAX_TRACKS;i++) {

              if (i==cur_edit_track) {

                if (song->track_mute[i]) val = 1;
              } 
              else {

                if (!song->track_mute[i]) val = 1;
              }   
            }
            
            if (val) {

              for(i=0;i<MAX_TRACKS;i++) {

                if (i==cur_edit_track) {
                  unmutetrack(i);
                }
                else { mutetrack(i); }
              }
            } 
            
            else {
              
              for(i=0;i<MAX_TRACKS;i++) {
                unmutetrack(i);
              }
            }
            
            need_refresh++;
            break;
            
          }
        }
        
        //boom
        
        switch(key) {

        // -------------------------------------------
        case SDLK_LEFTBRACKET:
        case SDLK_KP_DIVIDE:

          if (base_octave>0) {
            base_octave--;
            need_refresh++; 
          }

          break;

        // -------------------------------------------
        case SDLK_RIGHTBRACKET:
        case SDLK_KP_MULTIPLY:

          if (base_octave<9) {
            base_octave++;
            need_refresh++; 
          }

          break;
        

        // -------------------------------------------
        case SDLK_RETURN:
          if ((e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row))) {
            if (e->inst < MAX_INSTS) {
              cur_inst = e->inst;
              need_refresh++;
            }
          }
          break;
        
        // -------------------------------------------
                case SDLK_BACKSPACE:   // Mac "delete" key (backspace) - same as Delete
case SDLK_DELETE:

          if (kstate == KS_CTRL) {

            for(need_refresh=0;need_refresh<MAX_TRACKS;need_refresh++) {
              
              song->patterns[cur_edit_pattern]->tracks[need_refresh]->del_row(cur_edit_row);
            }
          } 
          else {
            song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->del_row(cur_edit_row);
          }           
          need_refresh++;
          break;
        case SDLK_INSERT:
          if (kstate == KS_CTRL) {
            for(need_refresh=0;need_refresh<MAX_TRACKS;need_refresh++)
              song->patterns[cur_edit_pattern]->tracks[need_refresh]->ins_row(cur_edit_row);
          } else {
            song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->ins_row(cur_edit_row);
          }           
          need_refresh++;
          break;
        }
        
        /* END SPECIAL KEYS */
        
        if ((kstate == KS_CTRL && ( key == SDLK_LEFT || key == SDLK_RIGHT || key == SDLK_UP || key == SDLK_DOWN )) || kstate == KS_NO_SHIFT_KEYS || kstate == KS_SHIFT || midiInQueue.size()>0) {
          
          pval=0; val=0x80;
          unsigned char leftright;
          if (zt_config_globals.step_editing)
            leftright = KS_SHIFT;
          else
            leftright = KS_NO_SHIFT_KEYS;
          
          switch(edit_cols[cur_edit_col].type) {
          case T_INST:
            
            e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);
            if (e==NULL)
              e = &blank_event;
            switch(key) {
            case SDLK_0: val = 0; pval++; break;
            case SDLK_1: val = 1; pval++; break;
            case SDLK_2: val = 2; pval++; break;
            case SDLK_3: val = 3; pval++; break;
            case SDLK_4: val = 4; pval++; break;
            case SDLK_5: val = 5; pval++; break;
            case SDLK_6: val = 6; pval++; break;
            case SDLK_7: val = 7; pval++; break;
            case SDLK_8: val = 8; pval++; break;
            case SDLK_9: val = 9; pval++; break;
            case SDLK_PERIOD: 
              if (kstate != KS_SHIFT) {
                val=0xFF; 
                pval++; 
              }
              break;
            default: break;
            }
            if (pval) {
              if (val<0xFF) {
                p = e->inst;   // XYZ   p1 = X   p2 = Y  p3 = Z
                if (p==0xFF) p=0;
                p2 = p / 10;
                p3 = p % 10;    // p3 = (54 % 10)  (=4)
                switch(edit_cols[cur_edit_col].place) {
                case 0: p3=val; break;
                case 1: p2=val; break;
                }
                p = p3 + (p2*10);
                if (p<0) p=0;
                if (p>=MAX_INSTS) p=MAX_INSTS-1;
              } else {
                p = 0xff;
              }
              if (p<MAX_INSTS)
                cur_inst = p;
              song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,-1,p,-1,-1,-1,-1);
              need_refresh++; 
              if (kstate != leftright)
                step++;
              else
                bump++;
            }
            break;
            case T_VOL:
              e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);
              if (e==NULL)
                e = &blank_event;
              switch(key) {
              case SDLK_0: val = 0; pval++; break;
              case SDLK_1: val = 1; pval++; break;
              case SDLK_2: val = 2; pval++; break;
              case SDLK_3: val = 3; pval++; break;
              case SDLK_4: val = 4; pval++; break;
              case SDLK_5: val = 5; pval++; break;
              case SDLK_6: val = 6; pval++; break;
              case SDLK_7: val = 7; pval++; break;
              case SDLK_8: val = 8; pval++; break;
              case SDLK_9: val = 9; pval++; break;
              case SDLK_A: val = 0xA; pval++; break;
              case SDLK_B: val = 0xB; pval++; break;
              case SDLK_C: val = 0xC; pval++; break;
              case SDLK_D: val = 0xD; pval++; break;
              case SDLK_E: val = 0xE; pval++; break;
              case SDLK_F: val = 0xF; pval++; break;
              case SDLK_PERIOD: 
                if (kstate!=KS_SHIFT) {
                  val=0x80; pval++; 
                }
                break;
              default: break;
              }
              if (pval) {
                if (val<0x80) {
                  pval = e->vol; if (pval>0x7f) pval=0;
                  if(edit_cols[cur_edit_col].place) {
                    if (val>7)
                      pval = 0x7f;
                    else {
                      pval = pval & 0x0F;
                      pval = pval + (val << 4);
                    }
                  } else {
                    pval = pval & 0xF0;
                    pval = pval + val;
                  }
                } else {
                  pval = val;
                }
                song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,-1,-1,pval,-1,-1,-1);
                need_refresh++; 
                if (kstate != leftright)
                  step++;
                else
                  bump++;
              }
              break;
              case T_FX:
                e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);
                if (e==NULL)
                  e = &blank_event;

                switch(key) {

                case SDLK_A: val = 'A'; pval++; break;
                case SDLK_B: val = 'B'; pval++; break;
                case SDLK_C: val = 'C'; pval++; break;
                case SDLK_D: val = 'D'; pval++; break;
                case SDLK_E: val = 'E'; pval++; break;
                case SDLK_F: val = 'F'; pval++; break;
                case SDLK_G: val = 'G'; pval++; break;
                case SDLK_H: val = 'H'; pval++; break;
                case SDLK_I: val = 'I'; pval++; break;
                case SDLK_J: val = 'J'; pval++; break;
                case SDLK_K: val = 'K'; pval++; break;
                case SDLK_L: val = 'L'; pval++; break;
                case SDLK_M: val = 'M'; pval++; break;
                case SDLK_N: val = 'N'; pval++; break;
                case SDLK_O: val = 'O'; pval++; break;
                case SDLK_P: val = 'P'; pval++; break;
                case SDLK_Q: val = 'Q'; pval++; break;
                case SDLK_R: val = 'R'; pval++; break;
                case SDLK_S: val = 'S'; pval++; break;
                case SDLK_T: val = 'T'; pval++; break;
                case SDLK_U: val = 'U'; pval++; break;
                case SDLK_V: val = 'V'; pval++; break;
                case SDLK_W: val = 'W'; pval++; break;
                case SDLK_X: val = 'X'; pval++; break;
                case SDLK_Y: val = 'W'; pval++; break;
                case SDLK_Z: val = 'Z'; pval++; break;
                case SDLK_PERIOD: 
                  if (kstate!=KS_SHIFT) {
                    val=0xFF; pval++; 
                  }
                  break;
                default: break;
                }
                if (pval) {
                  pval = val;
                  song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,-1,-1,-1,-1,pval,-1);
                  need_refresh++; 
                  if (kstate != leftright)
                    step++;
                  else
                    bump++;
                }
                break;
                
                case T_FXP:
                  e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);
                  if (e==NULL)
                    e = &blank_event;
                  switch(key) {
                  case SDLK_0: val = 0; pval++; break;
                  case SDLK_1: val = 1; pval++; break;
                  case SDLK_2: val = 2; pval++; break;
                  case SDLK_3: val = 3; pval++; break;
                  case SDLK_4: val = 4; pval++; break;
                  case SDLK_5: val = 5; pval++; break;
                  case SDLK_6: val = 6; pval++; break;
                  case SDLK_7: val = 7; pval++; break;
                  case SDLK_8: val = 8; pval++; break;
                  case SDLK_9: val = 9; pval++; break;
                  case SDLK_A: val = 0xA; pval++; break;
                  case SDLK_B: val = 0xB; pval++; break;
                  case SDLK_C: val = 0xC; pval++; break;
                  case SDLK_D: val = 0xD; pval++; break;
                  case SDLK_E: val = 0xE; pval++; break;
                  case SDLK_F: val = 0xF; pval++; break;
                  case SDLK_PERIOD: 
                    if (kstate!=KS_SHIFT) {
                      val=0x80; pval++; 
                    }
                    break;
                  default: break;
                  }
                  if (pval) {
                    temp = e->effect_data;
                    if (val == 0x80) {
                      temp = 0x0;
                    } else {
                      switch(edit_cols[cur_edit_col].place) {
                      case 0:
                        temp &= 0xFFF0;
                        temp += (val);
                        break;
                      case 1:
                        temp &= 0xFF0F;
                        temp += ((int)val)<<4;
                        break;
                      case 2:
                        temp &= 0xF0FF;
                        temp += ((int)val)<<8;
                        break;
                      case 3:
                        temp &= 0x0FFF;
                        temp += ((int)val)<<12;
                        break;
                        
                      }
                    }
                    song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,-1,-1,-1,-1,-1,temp);
                    need_refresh++; 
                    if (kstate != leftright)
                      step++;
                    else
                      bump++;
                  }
                  break;
                  
                  case T_OCTAVE:
                    unsigned char root_note;
                    e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);
                    if (e)
                      root_note = e->note % 12;
                    else
                      root_note = 0;
                    switch(key) {
                    case SDLK_1: set_note = root_note + (12*1); noplay=1; break;
                    case SDLK_2: set_note = root_note + (12*2); noplay=1; break;
                    case SDLK_3: set_note = root_note + (12*3); noplay=1; break;
                    case SDLK_4: set_note = root_note + (12*4); noplay=1; break;
                    case SDLK_5: set_note = root_note + (12*5); noplay=1; break;
                    case SDLK_6: set_note = root_note + (12*6); noplay=1; break;
                    case SDLK_7: set_note = root_note + (12*7); noplay=1; break;
                    case SDLK_8: set_note = root_note + (12*8); noplay=1; break;
                    case SDLK_9: set_note = root_note + (12*9); noplay=1; break;
                    case SDLK_0: set_note = root_note ; noplay=1; break;
                    default: break;
                    }
                    break;
                    case T_NOTE: 
                      switch(scancode) {
                        /* TOP ROW */
                      case SDL_SCANCODE_Q: set_note = 12*base_octave;         break;
                      case SDL_SCANCODE_2: set_note = (12*base_octave)+1;     break;
                      case SDL_SCANCODE_W: set_note = (12*base_octave)+2;     break;
                      case SDL_SCANCODE_3: set_note = (12*base_octave)+3;     break;
                      case SDL_SCANCODE_E: set_note = (12*base_octave)+4;     break;
                      case SDL_SCANCODE_R: set_note = (12*base_octave)+5;     break;
                      case SDL_SCANCODE_5: set_note = (12*base_octave)+6;     break;
                      case SDL_SCANCODE_T: set_note = (12*base_octave)+7;     break;
                      case SDL_SCANCODE_6: set_note = (12*base_octave)+8;     break;
                      case SDL_SCANCODE_Y: set_note = (12*base_octave)+9;     break;
                      case SDL_SCANCODE_7: set_note = (12*base_octave)+10;    break;
                      case SDL_SCANCODE_U: set_note = (12*base_octave)+11;    break;
                      case SDL_SCANCODE_I: set_note = (12*base_octave)+12;    break;
                      case SDL_SCANCODE_9: set_note = (12*base_octave)+1+12;  break;
                      case SDL_SCANCODE_O: set_note = (12*base_octave)+2+12;  break;
                      case SDL_SCANCODE_0: set_note = (12*base_octave)+3+12;  break;
                      case SDL_SCANCODE_P: set_note = (12*base_octave)+4+12;  break;
                      
                      
                      // <Manu> Repurpose these keys
                      //case SDLK_LEFTBRACKET: set_note = (12*base_octave)+5+12;  break;
                      //case SDLK_RIGHTBRACKET: set_note = (12*base_octave)+6+12;  break;


                        /* BOTTOM ROW */
                      case SDL_SCANCODE_Z: set_note = 12*(base_octave-1);     break;
                      case SDL_SCANCODE_S: set_note = (12*(base_octave-1))+1; break;
                      case SDL_SCANCODE_X: set_note = (12*(base_octave-1))+2; break;
                      case SDL_SCANCODE_D: set_note =(12*(base_octave-1))+3;  break;
                      case SDL_SCANCODE_C: set_note =(12*(base_octave-1))+4;  break;
                      case SDL_SCANCODE_V: set_note = (12*(base_octave-1))+5; break;
                      case SDL_SCANCODE_G: set_note = (12*(base_octave-1))+6; break;
                      case SDL_SCANCODE_B: set_note = (12*(base_octave-1))+7; break;
                      case SDL_SCANCODE_H: set_note = (12*(base_octave-1))+8; break;
                      case SDL_SCANCODE_N: set_note = (12*(base_octave-1))+9; break;
                      case SDL_SCANCODE_J: set_note = (12*(base_octave-1))+10;break;
                      case SDL_SCANCODE_M: set_note = (12*(base_octave-1))+11;break;
                        /* EDITING KEYS */
                        // Switch is on scancode (see line 2920); use
                        // SDL_SCANCODE_* here too. Mixing SDLK_* into a
                        // scancode switch silently makes the cases
                        // dead — that broke 4/8 audition + step in the
                        // 2026-04-19 layout-independent-keys refactor.
                      case SDL_SCANCODE_1: set_note = 0x81; break;
                      case SDL_SCANCODE_GRAVE: set_note = 0x82; break;
                      case SDL_SCANCODE_PERIOD:
                        if (kstate != KS_SHIFT) {
                          set_note = 0x80;
                        }
                        break;
                        /* ROW/NOTE PEEK PLAY */
                        // 4 / 8 advance distance is configurable via
                        // zt_config_globals.note_audition_step_mode:
                        //   ZT_NAS_NONE     -> stay on the same row
                        //   ZT_NAS_ONE      -> always advance by 1 row (default)
                        //   ZT_NAS_EDITSTEP -> advance by cur_step (legacy)
                      case SDL_SCANCODE_8:

                        ztPlayer->play_current_row();

                        {
                          int advance =
                              (zt_config_globals.note_audition_step_mode == ZT_NAS_NONE)     ? 0 :
                              (zt_config_globals.note_audition_step_mode == ZT_NAS_EDITSTEP) ? cur_step :
                              1;
                          cur_edit_row += advance;

                // <MANU> 2 Feb 2005 - Arreglado el bug que impedia avanzar el scroll
                //        mientras se tocaba la nota actual o la linea actual y llegabamos abajo
                          if (advance > 0 &&
                              (cur_edit_row-1 - cur_edit_row_disp) >= (PATTERN_EDIT_ROWS-1)) {
                            if (cur_edit_row_disp <
                                (song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                              cur_edit_row_disp += advance;
                            }
                          }
                        }

                        need_refresh++;

                        break;

                      case SDL_SCANCODE_4:

                        ztPlayer->play_current_note();

                        {
                          int advance =
                              (zt_config_globals.note_audition_step_mode == ZT_NAS_NONE)     ? 0 :
                              (zt_config_globals.note_audition_step_mode == ZT_NAS_EDITSTEP) ? cur_step :
                              1;
                          cur_edit_row += advance;

                          if (advance > 0 &&
                              (cur_edit_row-1 - cur_edit_row_disp) >= (PATTERN_EDIT_ROWS-1)) {
                            if (cur_edit_row_disp <
                                (song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                              cur_edit_row_disp += advance;
                            }
                          }
                        }

                        need_refresh++;

                        break;
                        /* EVERYTHING ELSE */
                      default:
                        break;
                      }
                      break;
                      
                      case T_LEN:
                        
                        e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);
                        if (e==NULL)
                          e = &blank_event;
                        switch(key) {
                        case SDLK_0: val = 0; pval++; break;
                        case SDLK_1: val = 1; pval++; break;
                        case SDLK_2: val = 2; pval++; break;
                        case SDLK_3: val = 3; pval++; break;
                        case SDLK_4: val = 4; pval++; break;
                        case SDLK_5: val = 5; pval++; break;
                        case SDLK_6: val = 6; pval++; break;
                        case SDLK_7: val = 7; pval++; break;
                        case SDLK_8: val = 8; pval++; break;
                        case SDLK_9: val = 9; pval++; break;
                        case SDLK_I: val = 0xA0; pval++; break;
                        case SDLK_PERIOD: 
                          if (kstate!=KS_SHIFT) {
                            val=0xFF; pval++; 
                          }
                          break;
                        default: break;
                        }
                        if (pval) {
                          if (val<0xA0) {
                            p = e->length;   // XYZ   p1 = X   p2 = Y  p3 = Z
                            p1 = p / 100;   // p1
                            p2 = (p-(p1*100)) / 10;
                            p3 = p % 10;    // p3 = (54 % 10)  (=4)
                            switch(edit_cols[cur_edit_col].place) {
                            case 0: p3=val; break;
                            case 1: p2=val; break;
                            case 2: p1=val; break;
                            }
                            p = p3 + (p2*10) + (p1*100);
                            if (p<1) p=1;
                            if (p>999) p=999;
                          } else {
                            if (val==0xff)
                              p = 0;
                            if (val==0xA0)
                              p = 1000;
                          }
                          song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,-1,-1,-1,p,-1,-1);
                          need_refresh++; 
                          if (kstate != leftright)
                            step++;
                          else
                            bump++;
                        }
                        break;
            } // End select(key)
            
            p1 = -1;
            //if (set_note > 0x82) set_note = 0x7F;
            if (midiInQueue.size()>0) {
              int dw;
              dw = midiInQueue.pop();
              unsigned char status = (unsigned char)(dw & 0xFF);
              unsigned char hi     = status & 0xF0;

              // CC drawmode: capture CC (0xB0..0xBF) / PB (0xE0..0xEF) and
              // write the matching effect (`Sxxyy` for CC, `Wxxxx` for PB)
              // at the cursor row in the current edit track. Cursor advances
              // by EditStep (step-follow). When drawmode is OFF the message
              // falls through to the legacy 0x80/0x90 jazz-recording path.
              if (g_cc_drawmode && (hi == 0xB0 || hi == 0xE0)) {
                int data1 = (dw >> 8)  & 0x7F;
                int data2 = (dw >> 16) & 0x7F;
                // Look up the friendly slot name from the currently-loaded
                // CCizer file (if any) — matches by learn binding so we
                // get "Cutoff" instead of "CC 74" in the status line.
                const char *slot_name = NULL;
                ZtCcizerFile *cf = zt_ccizer_current_file();
                if (cf) {
                  int m = zt_ccizer_find_learn_match(
                      cf, status, (hi == 0xE0) ? 0 : (unsigned char)data1);
                  if (m >= 0) slot_name = cf->slots[m].name;
                }
                if (hi == 0xB0) {
                  // S<cc>:<val>  ->  effect 'S', effect_data = (cc<<8) | val
                  unsigned short eff_data = (unsigned short)((data1 << 8) | data2);
                  song->patterns[cur_edit_pattern]
                       ->tracks[cur_edit_track]
                       ->update_event(cur_edit_row, -1, -1, -1, -1, 'S', eff_data);
                  if (slot_name) {
                    sprintf(szStatmsg, "Drew S%02X%02X [%s = %d] at row %02X",
                            data1, data2, slot_name, data2, cur_edit_row);
                  } else {
                    sprintf(szStatmsg, "Drew S%02X%02X (CC %d = %d) at row %02X",
                            data1, data2, data1, data2, cur_edit_row);
                  }
                } else {
                  // W<14bit pitchbend>  ->  effect 'W', effect_data = lsb|(msb<<7)
                  unsigned short pb14 = (unsigned short)(data1 | (data2 << 7));
                  song->patterns[cur_edit_pattern]
                       ->tracks[cur_edit_track]
                       ->update_event(cur_edit_row, -1, -1, -1, -1, 'W', pb14);
                  if (slot_name) {
                    sprintf(szStatmsg, "Drew W%04X [%s = %d] at row %02X",
                            pb14, slot_name, pb14, cur_edit_row);
                  } else {
                    sprintf(szStatmsg, "Drew W%04X (PB %d) at row %02X",
                            pb14, pb14, cur_edit_row);
                  }
                }
                statusmsg = szStatmsg; status_change = 1;
                file_changed++;
                // Step-follow: advance cursor by EditStep so the next CC
                // message lands on the next row down.
                int advance = cur_step;
                if (advance < 1) advance = 1;
                cur_edit_row += advance;
                if (cur_edit_row >= song->patterns[cur_edit_pattern]->length)
                  cur_edit_row = song->patterns[cur_edit_pattern]->length - 1;
                need_refresh++;
                // Don't fall through to note-jazz handling.
                set_note = 0xFF;
                p1 = -1;
              } else {
                switch( status ) {
                case 0x80: // Note off (channel 0 — legacy)
                  key = (dw&0xFF00)>>8 ;
                  key+=0xFF;
                  if (jazz_note_is_active(key)) {
                    const mbuf st = jazz_get_state(key);
                    MidiOut->noteOff(song->instruments[cur_inst]->midi_device,st.note,st.chan,0x0,0);
                    jazz_clear_state(key);
                  }
                  break;
                case 0x90: // Note on (channel 0 — legacy)
                  set_note = key = (dw&0xFF00)>>8 ;
                  key+=0xFF;
                  p1 = (dw&0xFF0000)>>16;
                  break;
                }
              }
            }
            
            if (set_note != 0xFF) {
              if (key_jazz==0) {
                if (noplay==0 ) {
                  if (set_note<0x80)
                    song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,set_note,cur_inst,zt_config_globals.record_velocity?p1:(-1),-1,-1,-1);
                  else
                    song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,set_note,MAX_INSTS,zt_config_globals.record_velocity?p1:(-1),-1,-1,-1);
                } else {
                  if ((e=song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row)))
                    song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,set_note,-1,zt_config_globals.record_velocity?p1:(-1),-1,-1,-1);
                }
                
                step++;
              }
              if (set_note < 0x80 && !noplay) {
                if (!jazz_note_is_active(key)) {
                  if (p1==-1) {
                    p1 = song->instruments[cur_inst]->default_volume;
                    if (song->instruments[cur_inst]->global_volume != 0x7F && p1>0) {
                      p1 *= song->instruments[cur_inst]->global_volume;
                      p1 /= 0x7f;
                    }
                    if ((e=song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row)))
                      if (e->vol<0x80)
                        p1 = e->vol;
                  }
                  
                  set_note += song->instruments[cur_inst]->transpose; 
                  if (set_note>0x7f) set_note = 0x7f;

                  MidiOut->noteOn(song->instruments[cur_inst]->midi_device,set_note,song->instruments[cur_inst]->channel,(unsigned char)p1,MAX_TRACKS,0);

                  jazz_set_state(key, set_note, song->instruments[cur_inst]->channel);
                }
              }
            }

            if (key == SDLK_SPACE) {

              key_jazz = ! key_jazz;

              if (key_jazz) statusmsg = "KeyJazz ON";
              else statusmsg = "KeyJazz OFF";

              //clear++; 
              need_refresh++;
            }

            if (bump) {

              if (cur_edit_col+1<cols_shown) {

                if (edit_cols[cur_edit_col+1].type != edit_cols[cur_edit_col].type) {

                  goto wrap;                
                } 
                else {

                  key = SDLK_RIGHT;
                }

              } 
              else {
wrap:;
     while(cur_edit_col>0) {
       if (edit_cols[cur_edit_col-1].type == edit_cols[cur_edit_col].type)
         cur_edit_col--;
       else
         break;
     }
     step++;
              }
            }
            if(kstate == KS_CTRL)
              amount = zt_config_globals.control_navigation_amount;
            else
              amount = 1;
            if (key==SDLK_RIGHT) {
              for(con=0;con < amount;con++) {
                cur_edit_col++;
                if (cur_edit_col>=cols_shown) {
                  cur_edit_col = 0;
                  key=SDLK_TAB;
                  kstate = 0;
                }
                need_refresh++;
                sprintf(szStatmsg,"%s",col_desc[cur_edit_col]);
                statusmsg=szStatmsg;
              }
            }
            if (key==SDLK_LEFT) {
              for(con=0;con < amount;con++) {
                if (cur_edit_col>0) {
                  cur_edit_col--;
                  need_refresh++;
                } else {
                  if ((cur_edit_track - cur_edit_track_disp) == 0) {
                    if (cur_edit_track_disp>0) {
                      cur_edit_track--;
                      cur_edit_track_disp--;
                      need_refresh++;
                      cur_edit_col = cols_shown-1;
                    }
                  } else
                    if (cur_edit_track>0) {
                      cur_edit_track--;
                      need_refresh++;
                      cur_edit_col = cols_shown-1;
                    }
                }
                sprintf(szStatmsg,"%s",col_desc[cur_edit_col]);
                statusmsg=szStatmsg;
              }
            }
            
        } // End No CTRL or ALT block
        
        if (key == SDLK_PAGEUP) {
          if(zt_config_globals.centered_editing) {
            if ((cur_edit_row-cur_edit_row_disp) > (PATTERN_EDIT_ROWS / 2) || (cur_edit_row == cur_edit_row_disp))
              cur_edit_row-=zt_config_globals.highlight_increment * 4;
            else
              cur_edit_row=cur_edit_row_disp;
            
            if (cur_edit_row < 0)
              cur_edit_row = 0;
            if (cur_edit_row < cur_edit_row_disp)
              cur_edit_row_disp = cur_edit_row;
            if(cur_edit_row > (PATTERN_EDIT_ROWS / 2) - (zt_config_globals.highlight_increment * 4)) {
              cur_edit_row_disp = cur_edit_row - (PATTERN_EDIT_ROWS / 2) + 2;
              if(cur_edit_row_disp < 0) cur_edit_row_disp = 0;
            }
            if(cur_edit_row <= (zt_config_globals.highlight_increment * 4))
              cur_edit_row_disp = 0;
            need_refresh++;
          }
          else {
            if ((cur_edit_row-cur_edit_row_disp)>(PATTERN_EDIT_ROWS/2) || (cur_edit_row==cur_edit_row_disp))
              cur_edit_row-=(zt_config_globals.highlight_increment * 4);
            else
              cur_edit_row=cur_edit_row_disp;
            if (cur_edit_row<0)
              cur_edit_row=0;
            if (cur_edit_row<cur_edit_row_disp)
              cur_edit_row_disp=cur_edit_row;
            need_refresh++;
          }
        }
        if (key == SDLK_PAGEDOWN) {
          if(zt_config_globals.centered_editing) {
            if (cur_edit_row < (cur_edit_row_disp+PATTERN_EDIT_ROWS) ||
              cur_edit_row == (cur_edit_row_disp + (PATTERN_EDIT_ROWS - 1)))
              cur_edit_row += (zt_config_globals.highlight_increment * 4);
            else
              cur_edit_row = cur_edit_row_disp+PATTERN_EDIT_ROWS + 1;
            if (cur_edit_row > song->patterns[cur_edit_pattern]->length - 1)
              cur_edit_row = song->patterns[cur_edit_pattern]->length - 1;
            if (cur_edit_row > cur_edit_row_disp + (PATTERN_EDIT_ROWS - 1))
              cur_edit_row_disp = cur_edit_row - (PATTERN_EDIT_ROWS - 1);
            if(cur_edit_row < (song->patterns[cur_edit_pattern]->length-1 - (PATTERN_EDIT_ROWS / 2) + (zt_config_globals.highlight_increment * 4)))
            {
              cur_edit_row_disp = cur_edit_row - (PATTERN_EDIT_ROWS / 2) + 2;
              if(cur_edit_row_disp < 0)
                cur_edit_row_disp = 0;
              else
                if(cur_edit_row_disp > song->patterns[cur_edit_pattern]->length - 1)
                  cur_edit_row_disp = song->patterns[cur_edit_pattern]->length - 1;
            }
            need_refresh++;
          }
          else {
            if (cur_edit_row<(cur_edit_row_disp+PATTERN_EDIT_ROWS) || cur_edit_row==(cur_edit_row_disp+(PATTERN_EDIT_ROWS-1)))
              cur_edit_row+=(zt_config_globals.highlight_increment * 4);
            else
              cur_edit_row=cur_edit_row_disp+PATTERN_EDIT_ROWS+1;
            if (cur_edit_row>song->patterns[cur_edit_pattern]->length-1)
              cur_edit_row = song->patterns[cur_edit_pattern]->length-1;
            if (cur_edit_row>cur_edit_row_disp+(PATTERN_EDIT_ROWS-1))
              cur_edit_row_disp=cur_edit_row-(PATTERN_EDIT_ROWS-1);
            need_refresh++;
          }
        } 
        if (key == SDLK_DOWN || step) {
          if (cur_step < (song->patterns[cur_edit_pattern]->length - cur_edit_row )) {
            for(con=0;con < amount;con++) {
              if(zt_config_globals.centered_editing) {
                step=cur_step-1; 
                need_refresh++;
                for(int e=0;e<=step;e++) {
                  if ((cur_edit_row - cur_edit_row_disp) >= (PATTERN_EDIT_ROWS-1) / 2 - 1) {
                    if (cur_edit_row_disp<(song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                      cur_edit_row++;
                      cur_edit_row_disp++;
                      need_refresh++;
                    }
                    else{
                      cur_edit_row++;
                      need_refresh++;
                    }
                  }
                  else if((cur_edit_row - cur_edit_row_disp) > (PATTERN_EDIT_ROWS-1) / 2 - 1) {
                    cur_edit_row++;
                    need_refresh++;
                  }
                  else  if ((cur_edit_row - cur_edit_row_disp) < (PATTERN_EDIT_ROWS-1)) {
                    cur_edit_row++;
                    need_refresh++;
                  }
                }
              }
              else {
                step=cur_step-1; 
                need_refresh++;
                for(int e=0;e<=step;e++) {
                  if ((cur_edit_row - cur_edit_row_disp) == (PATTERN_EDIT_ROWS-1)) {
                    if (cur_edit_row_disp<(song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                      cur_edit_row++;
                      cur_edit_row_disp++;
                      need_refresh++;
                    }
                  } else 	if ((cur_edit_row - cur_edit_row_disp) < (PATTERN_EDIT_ROWS-1)) {
                    cur_edit_row++;
                    need_refresh++;
                  } 
                }
              }
            }
          } else {
            if (step) 
              need_refresh++;
          }
          
        }
        if (key==SDLK_UP) {
          for(con=0;con < amount;con++) {
            if(zt_config_globals.centered_editing) {
              step=cur_step-1; 
              need_refresh++;
              for(int e=0;e<=step;e++) {
                if ((cur_edit_row - cur_edit_row_disp) < (PATTERN_EDIT_ROWS-1) /2){
                  if (cur_edit_row_disp>0) {
                    cur_edit_row--;
                    cur_edit_row_disp--;
                  }
                  else if (cur_edit_row>0) {
                    cur_edit_row--;
                    need_refresh++;
                  }
                  
                } else
                  if ((cur_edit_row - cur_edit_row_disp) == 0) {
                    if (cur_edit_row_disp>0) {
                      cur_edit_row--;
                      cur_edit_row_disp--;
                    }
                  }
                  else if (cur_edit_row>0) {
                    cur_edit_row--;
                    need_refresh++;
                  }
              }
            }
            else {
              step=cur_step-1; 
              need_refresh++;
              for(int e=0;e<=step;e++) {
                if ((cur_edit_row - cur_edit_row_disp) == 0) {
                  if (cur_edit_row_disp>0) {
                    cur_edit_row--;
                    cur_edit_row_disp--;
                  }
                } else
                  if (cur_edit_row>0) {
                    cur_edit_row--;
                  } 
              }
            }
          }
        }
        if (key==SDLK_TAB && ((!(kstate & KS_ALT)) || oktogo) ) {
          if (kstate & KS_SHIFT) {
            if (cur_edit_track)
              cur_edit_track--;
            if (cur_edit_track_disp>cur_edit_track)
              cur_edit_track_disp--;
            need_refresh++;
          } else {
            if ((cur_edit_track - cur_edit_track_disp) == (tracks_shown-1)) {
              if (cur_edit_track_disp<(MAX_TRACKS-tracks_shown)) {
                cur_edit_track++;
                cur_edit_track_disp++;
                need_refresh++;
              }
            } else
              if ((cur_edit_track - cur_edit_track_disp) <(tracks_shown-1)) {
                cur_edit_track++;
                need_refresh++;
              } 
          }
        }
        if (key==SDLK_HOME) {
          
          if (cur_edit_col>0) {
            cur_edit_col = 0;
          } else {
            if (cur_edit_track == 0) {
              cur_edit_row=0;
              cur_edit_row_disp=0;
            } else {
              if ((cur_edit_track - cur_edit_track_disp) == 0) {
                if (cur_edit_track_disp>0) {
                  cur_edit_track=0;
                  cur_edit_track_disp=0;
                  need_refresh++;
                }
              } else {
                cur_edit_track = cur_edit_track_disp;
              }   
            }
          }   
          need_refresh++;
        }
        if (key==SDLK_END) {
          if (cur_edit_col<cols_shown-1) {
            cur_edit_col=cols_shown-1;
          } else {
            if ((cur_edit_track - cur_edit_track_disp) == (tracks_shown-1)) {
              if (cur_edit_track == MAX_TRACKS-1) {
                cur_edit_row = song->patterns[cur_edit_pattern]->length-1;
                cur_edit_row_disp=cur_edit_row-(PATTERN_EDIT_ROWS-1);
              } else {
                cur_edit_track_disp = MAX_TRACKS-tracks_shown;
                cur_edit_track = MAX_TRACKS-1;
              }
            } else {
              cur_edit_track = cur_edit_track_disp + tracks_shown-1;
            }
          }
          need_refresh++;
        }
        
        
        
        break ;

        }  // End of mode switch
        
        
    } // End of if (keys.size()) 
    
    /* END KEYS */
    
    /* MOUSE UPDATES */
    
    switch(mode) {
    case PEM_MOUSEDRAW:
      if (mousedrawing) {
        
        if (   LastX >= col(5) 
          && LastX <  col(5+(tracks_shown*(field_size+1)))
          && LastY >= row(TRACKS_ROW_Y + 1)
          && LastY <= row(TRACKS_ROW_Y + 1 + PATTERN_EDIT_ROWS)
          ) {
          int x,y,data,track;
          x = (LastX - col(5)) / 8;
          y = (LastY - row(TRACKS_ROW_Y + 1)) / 8;
          track = (x/(field_size+1));
          FU_x = LastX-((field_size+1)*8);//col(5) + ((field_size+1)*track*8);
          FU_y = LastY-16;// row(15);
          FU_dx = LastX+((field_size+1)*8);//FU_x + (field_size+1)*8;
          FU_dy = LastY+16;//row(TRACKS_ROW_Y + 1 + PATTERN_EDIT_ROWS);
          if (FU_x<0) FU_x = 0;
          if (FU_dx > INTERNAL_RESOLUTION_X) FU_dx = INTERNAL_RESOLUTION_X-1;
          //x -= (field_size+1)*track;
          if (mousedrawing==1) {
            //  if (x<field_size) {
            data = (LastX-col(5)) - ((field_size+1)*track*8);
            data = (data*0x7f) / (field_size*8);
            //} else {
            if (data>0x7F)
              data = 0x7F;
            //} 
          } else {
            switch(md_mode) {
            case MD_VOL:
              data = 0x80;
              break;
            case MD_FX:
            case MD_FX_SIGNED:
              data = 0x0;
              break;
            }
            m_upd = 2;
          }
          //sprintf(szStatmsg,"Ok we got a press at x:%d track:%d row:%d data:%d",x,track,y,data);
          //statusmsg = szStatmsg;
          track += cur_edit_track_disp;   
          e = song->patterns[cur_edit_pattern]->tracks[track]->get_event(y);
          if (!e) e=&blank_event;
          y+=cur_edit_row_disp;
          switch(md_mode) {
          case MD_VOL:
            if (e->vol == data && mousedrawing==1) 
              goto nevermind;
            song->patterns[cur_edit_pattern]->tracks[track]->update_event(y,-1,-1,data,-1,-1,-1);
            break;
          case MD_FX:
            data += (e->effect_data&0xFF00);
            if (e->effect_data == data && mousedrawing==1) 
              goto nevermind;
            song->patterns[cur_edit_pattern]->tracks[track]->update_event(y,-1,-1,-1,-1,-1,data);
            break;
          case MD_FX_SIGNED:
            data += (e->effect_data&0xFF00);
            if (e->effect_data == data && mousedrawing==1) 
              goto nevermind;
            song->patterns[cur_edit_pattern]->tracks[track]->update_event(y,-1,-1,-1,-1,-1,data);
            break;
          }
          upd_e = song->patterns[cur_edit_pattern]->tracks[track]->get_event(y);
          m_upd++; 
          screenmanager.Update(FU_x,FU_y,FU_dx,FU_dy);
          need_refresh++;
nevermind:;
          //fast_update = 1;
          
        }
        
      }
      break;
    }
    
    
    if (cur_edit_row >= song->patterns[cur_edit_pattern]->length) {
      cur_edit_row = song->patterns[cur_edit_pattern]->length - 1;
      need_refresh++;
    }
    if (song->patterns[cur_edit_pattern]->length > PATTERN_EDIT_ROWS) {
      if (cur_edit_row_disp > song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS ) {
        cur_edit_row_disp = song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS;
        need_refresh++;
      }
    } else {
      if (cur_edit_row_disp != 0) {
        cur_edit_row_disp = 0;
        need_refresh++;
      }
    }
    
    need_update=0;
}








// ------------------------------------------------------------------------------------------------
//
//
void CUI_Patterneditor::draw(Drawable *S) 
{
  event *e;
  bool m_Fullupd = true;
  
  if (S->lock()==0) {


    if (clear) {
      S->fillRect(col(1),row(12),INTERNAL_RESOLUTION_X,INTERNAL_RESOLUTION_Y - (480-424),COLORS.Background);
      clear=0;
    }
    
    draw_status_vars(S);

    printtitle(PAGE_TITLE_ROW_Y,"Pattern Editor (F2)",COLORS.Text,COLORS.Background,S);
    
    switch(mode) {
      
    case PEM_MOUSEDRAW: 

      if (!ztPlayer->playing) status(S);
      
      disp_gfxeffect_pattern(tracks_shown,field_size,cols_shown,S,m_upd, upd_e);

      if (m_upd) m_Fullupd = false;

      m_upd = 0; 
      upd_e = NULL;

      break;
      
    case PEM_REGULARKEYS:

      e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row);

      if (edit_cols[cur_edit_col].type == T_FXP || edit_cols[cur_edit_col].type == T_FX) {    
        
        if (!e && edit_cols[cur_edit_col].type == T_FX) {

          sprintf(szStatmsg, "Effect");
        }
        else {

          if (!e) e = &blank_event;
      
          set_effect_data_msg(szStatmsg, e->effect, e->effect_data);
        }
        
        statusmsg = szStatmsg;
      }

      if (!ztPlayer->playing)

        status(S);


//      #define __FAST_UPDATE__

#ifdef __FAST_UPDATE__
      
      /////////////////////////////////////////////////////////////////////////////////
      
      // Hello, this is an attempt at speeding up the pattern editor dramatically
      // through the use of an off screen buffer that contains the "image" of the
      // pattern editor (even beyond the current page's scope). When navigating, zt
      // will determine what "coordinates" of this huge image to plaster up on the
      // pattern editor, and then will update as usual the current row (for colors).
      
      // Update buffer if necessary

      // <Manu> Esto nunca puede funcionar porque pe_buf es NULL [EN: this can never work because pe_buf is NULL]

      
      if(pe_modification) // pe_modification is a very very global var which is set to
        // 1 in modifying parts of patterneditor's Update, and it is
        // set to something > 1 when doing any F key (except F2)
      {
        if(pe_modification==1) { // Only a chunk
          disp_pattern(20,field_size,128,pe_buf); // these numbers are wrong of course
        }
        else { // the whole thing (useful at least at the very beginning...)
          disp_pattern(20,field_size,128,pe_buf);
        }
      }
      
      // Now that buffer is up to date, determine exactly which portion to display
      
      // put that portion in S
      
      /////////////////////////////////////////////////////////////////////////////////
#else
      disp_pattern(tracks_shown,field_size,cols_shown,S);
#endif
      break;
    }
    
    need_refresh = 0;
    updated++;
    
    S->unlock();
    
    if (m_Fullupd) {

      // <Manu> Clarificar un poco esta fórmula, porque con row(52 se comía la última línea  [EN: clarify this formula a bit — with row(52) it was eating the last line]

      screenmanager.Update(0, row(11), INTERNAL_RESOLUTION_X, row(53+((INTERNAL_RESOLUTION_Y-480)/8))) ;
    }
  }
}

