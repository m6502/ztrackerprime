#include "zt.h"


int PATTERN_EDIT_ROWS = 100;
int m_upd = 0;
event *upd_e = NULL;

int max_displayable_rows = 0 ;
int g_posx_tracks = 0 ;

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

    // <Manu> Nuevo comando B para hacer loops; pintamos la informacion

    sprintf(str,"Command: Loop to pattern %d", effect_data);

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
// <Manu> Debería mirar si puedo hacer esto de una manera un poco más limpia
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
  char str[256];
  int j;
  int p=0;

  for(j=cur_edit_track_disp;j<cur_edit_track_disp+tracks_shown;j++) {

    if (zt_config_globals.cur_edit_mode != VIEW_SQUISH) sprintf(str," Track %.2d ",j+1);
    else sprintf(str," %.2d ",j+1);

    if (song->track_mute[j]) printBG(col(g_posx_tracks + (p*(field_size+1))),row(14),str,COLORS.Text,COLORS.Lowlight,S);
    else printBG(col(g_posx_tracks + (p*(field_size+1))),row(14),str,COLORS.Brighttext,COLORS.Lowlight,S);
    
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

  int var_row,var_track,num_displayed_tracks,num_displayed_rows,data,datamax;
  TColor bg;
  event *e;
  char str[64];
  printline(col(5),row(14),148,(tracks_shown * (field_size+1))-1,COLORS.Lowlight,S); // 0x89
  printline(col(5),row(14+(PATTERN_EDIT_ROWS+1)),143,(tracks_shown * (field_size+1))-1,COLORS.Highlight,S); // 0x89
  draw_track_markers(tracks_shown, field_size,S);
  printchar(col(4),row(14),139,COLORS.Lowlight,S);


  num_displayed_rows = 0 ;

  for(var_row=cur_edit_row_disp;var_row<(cur_edit_row_disp+PATTERN_EDIT_ROWS);var_row++) {
  
    if (!upd_one) {

      sprintf(str,"%.3d",var_row);
    
      if (ztPlayer->playing) {
      
        if (var_row==ztPlayer->playing_cur_row && ztPlayer->playing_cur_pattern==cur_edit_pattern) {
          
          printBG(col(1),row(15+num_displayed_rows),str,COLORS.Highlight,COLORS.Background,S);
        }
        else {
          
          printBG(col(1),row(15+num_displayed_rows),str,COLORS.Text,COLORS.Background,S);
        }
      } 
      else printBG(col(1),row(15+num_displayed_rows),str,COLORS.Text,COLORS.Background,S);
      
      printchar(col(4),row(15+num_displayed_rows),146,COLORS.Lowlight,S);
      printchar(col(4+(tracks_shown*(field_size+1))),row(15+num_displayed_rows),145,COLORS.Highlight,S);
    }
    
    
    num_displayed_tracks=0;
    
    for(var_track=cur_edit_track_disp;var_track <cur_edit_track_disp+tracks_shown; var_track++) {
    
      bg = COLORS.EditBG;
      
      if (!(var_row%zt_config_globals.lowlight_increment)) bg = COLORS.EditBGlow;
      if (!(var_row%zt_config_globals.highlight_increment)) bg = COLORS.EditBGhigh;
      
      if (!(e = song->patterns[cur_edit_pattern]->tracks[var_track]->get_event(var_row))) e = &blank_event;
      
      //          printNote(str,e,cur_edit_mode);
      //          printBG(col(5+(num_displayed_tracks*(field_size+1))),row(15+num_displayed_rows),str,fg,bg,S);
      
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
        
        draw_signed_bar(col(5+(num_displayed_tracks*(field_size+1))),row(15+num_displayed_rows),col(field_size)-1,row(1),data,datamax,bg,S);
        
        break;

      default:
        
        draw_bar(col(5+(num_displayed_tracks*(field_size+1))),row(15+num_displayed_rows),col(field_size)-1,row(1),data,datamax,bg,S);
        
        break;
      } ;
      
      
      if (var_track<(cur_edit_track_disp+tracks_shown-1)) {
        
        printchar(col(5+((num_displayed_tracks+1)*(field_size+1))-1),row(15+num_displayed_rows),168,COLORS.Lowlight,S) ;
      }

please_skip:

      num_displayed_tracks++;
    }
    
    num_displayed_rows++;
  }

  printchar(col(4),                               row(14),                   153, COLORS.Lowlight,  S) ;
  printchar(col(4+(tracks_shown*(field_size+1))), row(14),                   152, COLORS.Lowlight,  S) ;
  printchar(col(4),                               row(15+PATTERN_EDIT_ROWS), 151, COLORS.Lowlight,  S) ;
  printchar(col(4+(tracks_shown*(field_size+1))), row(15+PATTERN_EDIT_ROWS), 150, COLORS.Highlight, S) ;
}





// ------------------------------------------------------------------------------------------------
//
//
void disp_pattern(int tracks_shown, int field_size, int cols_shown, Drawable *S) 
{
  int var_row, var_track ;
  int num_displayed_rows, num_displayed_tracks, clear=0, step=0, noplay=0, special ;
  TColor bg,fg;
  char str[512] ;    // <Manu> ¿Seguro que esto no dará problemas con resoluciones muy grandes?
  event *e;
  
  int posx_current_track ;
  int posy_current_row ;
  //  char note[4];

  max_displayable_rows = PATTERN_EDIT_ROWS ;
  if(max_displayable_rows > song->patterns[cur_edit_pattern]->length) max_displayable_rows = song->patterns[cur_edit_pattern]->length ;

  int first_row = cur_edit_row_disp ;

  int last_row = first_row + max_displayable_rows ;
  if(last_row > song->patterns[cur_edit_pattern]->length) last_row = song->patterns[cur_edit_pattern]->length ;

  int blank_rows = PATTERN_EDIT_ROWS - max_displayable_rows /*- 1*/ ;
  int first_blank_row = first_row + max_displayable_rows ;


#define TRACKS_POS_Y                 row(14)
#define TRACKS_FIRST_NOTE_POS_Y      (TRACKS_POS_Y + row(1))

  //printBG(col(5),row(15),
 
  /*
  num_displayed_tracks=0;
  for(var_track=cur_edit_track_disp;var_track<cur_edit_track_disp+tracks_shown;var_track++) {
  if (cur_edit_mode != VIEW_SQUISH)
  sprintf(str," Track %.2d ",var_track+1);
  else
  sprintf(str," %.2d ",var_track+1);
  if (song->track_mute[var_track])
  printBG(col(5+(num_displayed_tracks*(field_size+1))),row(14),str,COLORS.Text,COLORS.Lowlight,S);
  else
  printBG(col(5+(num_displayed_tracks*(field_size+1))),row(14),str,COLORS.Brighttext,COLORS.Lowlight,S);
  num_displayed_tracks++;
} */



  // <Manu> Cumple alguna función dibujar este carácter?
  //printchar(col(4), row(14), 139, COLORS.Lowlight,S);



  int poscharx_tracks = ((INTERNAL_RESOLUTION_X / FONT_SIZE_X) - (tracks_shown*(field_size+1))) >> 1 ;
  g_posx_tracks = poscharx_tracks ;

  num_displayed_rows = 0 ; // <Manu> Esto va de 0 al número máximo de rows a dibujar - 1
  
  
  for(var_row = first_row; var_row < (last_row + blank_rows); var_row++) {

    posy_current_row = TRACKS_FIRST_NOTE_POS_Y + row(num_displayed_rows) ;

    // Determine if this row needs special treatment

    if(var_row == cur_edit_row) special = 1;
    else special = 0;


    // Is this row number inside the allowed number?

    if(var_row >= first_blank_row) {

      sprintf(str,"   ") ;
    }
    else {

      if(var_row >= 0) { // <Manu> ¿Necesario?
        
        sprintf(str,"%.3d",var_row);
      
        // Línea vertical izquierda del marco de los tracks
        printchar(col(poscharx_tracks - 1),                               posy_current_row, 146, COLORS.Lowlight,S) ;

        // Línea vertical derecha del marco de los tracks
        printchar(col((poscharx_tracks - 1) + (tracks_shown*(field_size+1))), posy_current_row, 145, COLORS.Highlight,S) ;
      }
      
    }


    if (ztPlayer->playing) {

      if (var_row==ztPlayer->playing_cur_row && ztPlayer->playing_cur_pattern==cur_edit_pattern) {
        
        printBG(col(poscharx_tracks - 4),posy_current_row,str,COLORS.Highlight,COLORS.Background,S);
      }
      else {
        
        printBG(col(poscharx_tracks - 4),posy_current_row,str,COLORS.Text,COLORS.Background,S);
      }
    } 
    else {
      
      printBG(col(poscharx_tracks - 4),posy_current_row,str,COLORS.Text,COLORS.Background,S);
    }






    



    
    {
      if(var_row >= 0) {       // <Manu> ¿Necesario?
    
    // Contenido de los tracks

            num_displayed_tracks = 0 ;

            for(var_track = cur_edit_track_disp; var_track < (cur_edit_track_disp + tracks_shown); var_track++) {

              posx_current_track = col(poscharx_tracks) + col(num_displayed_tracks * (field_size + 1)) ;

              // <Manu> Si esta línea hay que dibujarla en blanco dibujamos las columnas de los tracks con el color del fondo

              if(var_row >= first_blank_row) {

                //fg = COLORS.EditText; // <Manu> Innecesario ?
                //bg = COLORS.EditBG;

                //printBlankNote(str, zt_config_globals.cur_edit_mode) ;
                //printBG(col(poscharx_tracks+(num_displayed_tracks*(field_size+1))), row(15+num_displayed_rows), str, fg, bg, S) ;

                // Lo dibujamos un píxel a la izquierda y un píxel más largo para comernos la posible línea del marco de los tracks

                int x1 = posx_current_track - 2  ;
                int x2 = x1 + col(field_size + 1) + 1  ;

                int y1 = posy_current_row ;
                int y2 = y1 + FONT_SIZE_Y ;

                S->fillRect(x1, y1, x2, y2, /*COLORS.Highlight*/COLORS.Background) ;
              }
              else {

                // bg = special?COLORS.CursorRowLow:COLORS.EditBG;

                if(special) bg = COLORS.CursorRowLow ;
                else bg = COLORS.EditBG ;


                if (!(var_row % zt_config_globals.lowlight_increment))  bg = special ? COLORS.CursorRowHigh : COLORS.EditBGlow ;
                if (!(var_row % zt_config_globals.highlight_increment)) bg = special ? COLORS.CursorRowHigh : COLORS.EditBGhigh ;

                if (!(e = song->patterns[cur_edit_pattern]->tracks[var_track]->get_event(var_row))) e = &blank_event;

                fg = COLORS.EditText;

                if (selected) {

                  if (var_track>=select_track_start && var_track<=select_track_end) {

                    if (var_row>=select_row_start && var_row<=select_row_end) {

                      fg = COLORS.Highlight;

                      if (bg == COLORS.EditBGlow || bg==COLORS.EditBGhigh) bg = COLORS.SelectedBGHigh;
                      else {

                        if(!special) bg = COLORS.SelectedBGLow;
                        else bg = COLORS.SelectedBGHigh;
                      }
                    }
                  }
                }


                //if(var_track >= 0 && var_track < song->patterns[cur_edit_pattern]->length) {


                printNote(str, e, zt_config_globals.cur_edit_mode) ;
                printBG(col(poscharx_tracks + (num_displayed_tracks*(field_size+1))), posy_current_row, str, fg, bg, S) ;

                str[ edit_cols[cur_edit_col].startx + 1 ] = 0 ;

                
                // Dibujar el caret

                if (var_row == cur_edit_row && var_track == cur_edit_track) {

                  printBG(
                          col(poscharx_tracks + edit_cols[cur_edit_col].startx + (num_displayed_tracks * (field_size + 1))),
                          posy_current_row,
                          &str[ edit_cols[cur_edit_col].startx ],
                          COLORS.EditBG,
                          COLORS.Highlight,
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
CUI_Patterneditor::~CUI_Patterneditor(void)
{
  
}




// ------------------------------------------------------------------------------------------------
//
//
void CUI_Patterneditor::enter(void)
{
  need_refresh = 1;
  cur_state = STATE_PEDIT;
  mousedrawing=0;
  //PATTERN_EDIT_ROWS = 32 + 4 + (INTERNAL_RESOLUTION_Y - 480) / 8;
  
  // <Manu> 180 es el espacio que queda más o menos para las notas descontando dónde empiezan los tracks, la toolbar, etc.
  //        No sería mala idea calcular esto de una manera un poco más clara.

  PATTERN_EDIT_ROWS = (INTERNAL_RESOLUTION_Y - 180) / 8 ;
  

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
  int i,j,o=0,p=0,step=0,noplay=0,bump=0;
  unsigned char set_note=0xff,kstate;
  int key;
  int temp,oktogo=0,keyed=0/*,dontc=0*/;
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
  //memset(note,0,4);

//  int pattern_visible ;
//
//  if(ztPlayer->playing) {
//  
//    pattern_visible = ztPlayer->cur_pattern ;
//  }



  // <Manu> Esto arregla el bug del último número de línea destacado cuando el play pasa por el pattern actual y sigue por otro.
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


  // <Manu> Esto arreglaba el bug de que no se refresque la pantalla al cambiar
  //        el número de filas del pattern actual, pero ya no es necesario :-)
  //
  //if(ztPlayer->song->patterns[cur_edit_pattern]->length != last_pattern_size) {
  //
  //  last_pattern_size = ztPlayer->song->patterns[cur_edit_pattern]->length ;
  //  need_refresh++ ;
  //}

//#define _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS

#ifndef _ACTIVAR_CAMBIO_TAMANYO_COLUMNAS
  zt_config_globals.cur_edit_mode = VIEW_BIG ;
#else

  if (Keys.size()) {

    key = Keys.checkkey();
    kstate = Keys.getstate();
    keyed++;

    if((kstate & KS_CTRL) && (key == DIK_TAB)) {

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
      kstate = Keys.getstate();
    }
  }

#endif


#define LEFT_MARGIN       40
#define RIGHT_MARGIN      4

  
  switch(zt_config_globals.cur_edit_mode) 
  {

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_SQUISH:

    if(need_refresh) statusmsg = "View mode: Squish";

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / (FONT_SIZE_X * 6) ;
    field_size = 6;
    cols_shown = 4 ;
    init_short_cols();

    break;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_REGULAR:

    if (need_refresh) statusmsg = "View mode: Regular";

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / (FONT_SIZE_X * 14) ;
    field_size = 13;
    cols_shown = 9;
    fix_cols();

    break;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_BIG:

    if (need_refresh) statusmsg = "View mode: Big";

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / 160 ;    // 160 es lo que mide cada columna; dejamos 80 pixels extras de margen
    field_size = 19;
    cols_shown = 14;
    fix_cols();

    break;

  // --------------------------------------------------------------------------
  // --------------------------------------------------------------------------

  case VIEW_FX:

    if (need_refresh) statusmsg = "View mode: FX";

    tracks_shown = (INTERNAL_RESOLUTION_X - LEFT_MARGIN - RIGHT_MARGIN) / (FONT_SIZE_X * 13) ;
    
//    if((((INTERNAL_RESOLUTION_X-640)/8) % 13) > .5) tracks_shown++;

    field_size = 12;
    cols_shown = 9;
    init_fx_cols();

    break;
  }
  
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
  
  
  
  if (Keys.size() || midiInQueue.size() > 0) {
    
    key=Keys.getkey();
    kstate = Keys.getstate();
    
    set_note = 0xff;
    
    /* COMMON KEYS */
    if (kstate == KS_SHIFT) {

      switch(key)
      {

      case DIK_GRAVE:
        if (mode==PEM_MOUSEDRAW) {
          
        /* Here we flush the MIDIin queue so as to
          not  plug them  in the  pattern  editor */
          midiInQueue.clear();
          key = 0;
          
          mode = PEM_REGULARKEYS;
        }
        else mode = PEM_MOUSEDRAW;
        need_refresh++;
        break;
      case DIK_ADD:
        if(cur_edit_order < 255 && song->orderlist[cur_edit_order + 1] != 0x100)
        {
          cur_edit_order++;
          cur_edit_pattern = song->orderlist[cur_edit_order];
          need_refresh++;
        }
        break;
      case DIK_SUBTRACT:
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
      case DIK_ADD:

        if (cur_edit_pattern<255) {
          cur_edit_pattern++;
          need_refresh++; 
        }

        break;

      case DIK_SUBTRACT:

        if (cur_edit_pattern>0) {

          cur_edit_pattern--;
          need_refresh++; 
        }

        break;
      }           
    }
    
    
    /* START KEYS */
    
    switch(mode) {
      
    case PEM_MOUSEDRAW:
      
/*
      if (kstate == KS_SHIFT) {
      }
      if (kstate == KS_ALT) {
      }
      if (kstate == KS_CTRL) {
      }
*/

      if (kstate == KS_NO_SHIFT_KEYS) {
        switch(key) {
          
        case DIK_MOUSE_1_OFF:
        case DIK_MOUSE_2_OFF: 
          mousedrawing=0;
          break;
          
        case DIK_MOUSE_1_ON:
          if (    MousePressX >= col(5) 
            && MousePressX <  col(5+(tracks_shown*(field_size+1)))
            && MousePressY >= row(15)
            && MousePressY <= row(15+PATTERN_EDIT_ROWS)
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
        case DIK_MOUSE_2_ON:
          if (    MousePressX >= col(5) 
                && MousePressX <  col(5+(tracks_shown*(field_size+1)))
                && MousePressY >= row(15)
                && MousePressY <= row(15+PATTERN_EDIT_ROWS)
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
          
        case DIK_TAB:
          md_mode++;
          if (md_mode>=MD_END)
            md_mode = 0;
          need_refresh++;
          break;

        case DIK_DOWN:
          cur_edit_row_disp++;
          need_refresh++;
          break;

        case DIK_UP:
          cur_edit_row_disp--;
          need_refresh++;
          break;

        case DIK_LEFT:
          cur_edit_track_disp--;
          need_refresh++;
          break;

        case DIK_RIGHT:
          cur_edit_track_disp++;
          need_refresh++;
          break;

        case DIK_PGDN:
          cur_edit_row_disp+= zt_config_globals.highlight_increment * 4 ;
          need_refresh++;
          break;

        case DIK_PGUP:
          cur_edit_row_disp-= zt_config_globals.highlight_increment * 4 ;    // <Manu> ¿Esto no debería ser un múltiplo del step?
          need_refresh++;
          break;

        case DIK_END:
          cur_edit_row_disp=0xFFF;
          need_refresh++;
          break;

        case DIK_HOME:
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
                case DIK_PERIOD:
                  cur_inst++;
                  if (cur_inst>=MAX_INSTS)
                    cur_inst = MAX_INSTS-1;
                  need_refresh++;
                  break;
                case DIK_COMMA:
                  cur_inst--;
                  if (cur_inst<0)
                    cur_inst=0;
                  need_refresh++;
                  break;
                  
                  
                  
                  ////////////////////////////////////////////////////////////////////////
                  // NEW SHIFT SELECTION
                  
                case DIK_LEFT:
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
                case DIK_RIGHT:
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
                  
                 
                // <Manu> Clarifico esto un poco
                  
                case DIK_PGDN:
                  
                  effective=zt_config_globals.highlight_increment * 4;
                
                case DIK_DOWN:
                
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
                            
                          // <Manu> if y else eran iguales ¿?
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
                  
                case DIK_PGUP:

                  effective=zt_config_globals.highlight_increment * 4;
                  
                  if(cur_edit_row-effective < cur_edit_row_disp && cur_edit_row != cur_edit_row_disp) {
                    effective = cur_edit_row - cur_edit_row_disp;
                  }
                  else {
                    
                    if(cur_edit_row - effective <= 0) {
                      
                      effective = cur_edit_row ;
                    }
                  }

                case DIK_UP:
                  
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




        if (kstate == KS_CTRL ) {


          switch(key)
          {
          case DIK_A:
            if(selected) {
              popup_window(UIP_PENature);
              Keys.flush();
              key = Keys.checkkey();
              kstate = Keys.getstate();
              need_refresh++;
            }
            break;
            
          case DIK_W:
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
            
          case DIK_N: /* Set length of first note to length of selection */
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

        if (kstate == KS_ALT ) {
        
          switch(key)
          {

          case DIK_1: cur_step=1;  need_refresh++; statusmsg = "Step set to 1"; break;
          case DIK_2: cur_step=2;  need_refresh++; statusmsg = "Step set to 2"; break;
          case DIK_3: cur_step=3;  need_refresh++; statusmsg = "Step set to 3"; break;
          case DIK_4: cur_step=4;  need_refresh++; statusmsg = "Step set to 4"; break;
          case DIK_5: cur_step=5;  need_refresh++; statusmsg = "Step set to 5"; break;
          case DIK_6: cur_step=6;  need_refresh++; statusmsg = "Step set to 6"; break;
          case DIK_7: cur_step=7;  need_refresh++; statusmsg = "Step set to 7"; break;
          case DIK_8: cur_step=8;  need_refresh++; statusmsg = "Step set to 8"; break;
          case DIK_9: cur_step=9;  need_refresh++; statusmsg = "Step set to 9"; break;
          case DIK_0: cur_step=0;  need_refresh++; statusmsg = "Step set to 0"; break;

            
          case DIK_P: // copy to new pattern
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
            
          case DIK_GRAVE: 
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
            
          case DIK_T: 
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
            
            
          case DIK_I: // Interpolate effect
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
            
          case DIK_K: // Interpolate volume
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
            
            
          case DIK_J:
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
            
          case DIK_C:
            if (selected) {
              clipboard->copy(); 
              SDL_Delay(50);
            }
            break;

          case DIK_X:
            if (!last_cmd_keyjazz && selected) {
              clipboard->copy();
              for(i=select_track_start;i<=select_track_end;i++)
                for(j=select_row_start;j<=select_row_end;j++)
                  song->patterns[cur_edit_pattern]->tracks[i]->update_event(j,0x80,MAX_INSTS,0x80,0x0,0xff,0x0);
                need_refresh++;
                last_keyjazz = DIK_X;
                last_cmd_keyjazz = 1;
            }
            break;

          case DIK_V:

            clipboard->paste(cur_edit_track,cur_edit_row,0); // insert
            need_refresh++;
            
            break;

          case DIK_O:

            clipboard->paste(cur_edit_track,cur_edit_row,1); // overwrite
            need_refresh++;

            break;

          case DIK_M:

            clipboard->paste(cur_edit_track,cur_edit_row,2); // merge
            need_refresh++;

            break;

          
          
          case DIK_L:
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
          case DIK_Z:
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
          case DIK_U: // Unselect block
            selected=0;
            need_refresh++;
            break;
          case DIK_F: // Double block length
            if (selected) {
              int len = select_row_end - select_row_start + 1;
              len *= 2;
              select_row_end = select_row_start + len - 1; 
              if (select_row_end >= song->patterns[cur_edit_pattern]->length )
                select_row_end = song->patterns[cur_edit_pattern]->length - 1;
              need_refresh++;
            }
            break;
          case DIK_G: // Halve block length
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
          case DIK_B:  // Set Select Begin block
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
          case DIK_E: // Set Block End
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
        if (kstate == KS_ALT) {
          switch(key) {
            
          case DIK_GRAVE: 
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
              need_refresh++;
            }
            break;
            
            
          case DIK_S: /* Set Instrument */
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++)
                for(j=select_row_start;j<=select_row_end;j++) {
                  if (e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j)) {
                    if (e->inst < MAX_INSTS)
                      e->inst = cur_inst;
                  }
                }
                need_refresh++;
            }
            break;
            
          case DIK_D: /* select */
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
          case DIK_Q: /* Transpose up */
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++)
                for(j=select_row_start;j<=select_row_end;j++) {
                  if (e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j)) {
                    if (e->note < 0x7F)
                      e->note++;
                  }
                }
                need_refresh++;
            }
            break;
          case DIK_A: /* Transpose up */
            if (selected) {
              for(i=select_track_start;i<=select_track_end;i++)
                for(j=select_row_start;j<=select_row_end;j++) {
                  if (e = song->patterns[cur_edit_pattern]->tracks[i]->get_event(j)) {
                    if (e->note > 0 && e->note < 0x80)
                      e->note--;
                  }
                }
                need_refresh++;
            }
            break;
            
            
          case DIK_LEFT:
            if (cur_edit_track>cur_edit_track_disp) {
              cur_edit_track--;
            } else {
              cur_edit_track--; cur_edit_track_disp--;
              if (cur_edit_track<0) cur_edit_track=0;
              if (cur_edit_track_disp<0) cur_edit_track_disp=0;
            }
            need_refresh++;
            break;
          case DIK_RIGHT:
            key = DIK_TAB; oktogo=1;
            break;
            
            
          case DIK_F1: toggle_track_mute(0); 
            need_refresh++; 
            break;
          case DIK_F2: toggle_track_mute(1); need_refresh++; break;
          case DIK_F3: toggle_track_mute(2); need_refresh++; break;
          case DIK_F4: toggle_track_mute(3); need_refresh++; break;
          case DIK_F5: toggle_track_mute(4); need_refresh++; break;
          case DIK_F6: toggle_track_mute(5); need_refresh++; break;
          case DIK_F7: toggle_track_mute(6); need_refresh++; break;
          case DIK_F8: toggle_track_mute(7); need_refresh++; break;
          case DIK_F9: toggle_track_mute(cur_edit_track); need_refresh++; break;
          case DIK_F10: 
            
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
                else mutetrack(i);
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

        case DIK_MULTIPLY:

          if (base_octave<9) {
            base_octave++;
            need_refresh++; 
          }

          break;

        case DIK_DIVIDE:

          if (base_octave>0) {
            base_octave--;
            need_refresh++; 
          }

          break;

        case DIK_RETURN:
          if (e = song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row)) {
            if (e->inst < MAX_INSTS) {
              cur_inst = e->inst;
              need_refresh++;
            }
          }
          break;
        
        case DIK_DELETE:

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
        case DIK_INSERT:
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
        
        if ((kstate == KS_CTRL && ( key == DIK_LEFT || key == DIK_RIGHT || key == DIK_UP || key == DIK_DOWN )) || kstate == KS_NO_SHIFT_KEYS || kstate == KS_SHIFT || midiInQueue.size()>0) {
          
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
            case DIK_0: val = 0; pval++; break;
            case DIK_1: val = 1; pval++; break;
            case DIK_2: val = 2; pval++; break;
            case DIK_3: val = 3; pval++; break;
            case DIK_4: val = 4; pval++; break;
            case DIK_5: val = 5; pval++; break;
            case DIK_6: val = 6; pval++; break;
            case DIK_7: val = 7; pval++; break;
            case DIK_8: val = 8; pval++; break;
            case DIK_9: val = 9; pval++; break;
            case DIK_PERIOD: 
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
                if (p>=MAX_INSTS) p=MAX_INSTS-1; if (p<0) p=0;
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
              case DIK_0: val = 0; pval++; break;
              case DIK_1: val = 1; pval++; break;
              case DIK_2: val = 2; pval++; break;
              case DIK_3: val = 3; pval++; break;
              case DIK_4: val = 4; pval++; break;
              case DIK_5: val = 5; pval++; break;
              case DIK_6: val = 6; pval++; break;
              case DIK_7: val = 7; pval++; break;
              case DIK_8: val = 8; pval++; break;
              case DIK_9: val = 9; pval++; break;
              case DIK_A: val = 0xA; pval++; break;
              case DIK_B: val = 0xB; pval++; break;
              case DIK_C: val = 0xC; pval++; break;
              case DIK_D: val = 0xD; pval++; break;
              case DIK_E: val = 0xE; pval++; break;
              case DIK_F: val = 0xF; pval++; break;
              case DIK_PERIOD: 
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

                case DIK_A: val = 'A'; pval++; break;
                case DIK_B: val = 'B'; pval++; break;
                case DIK_C: val = 'C'; pval++; break;
                case DIK_D: val = 'D'; pval++; break;
                case DIK_E: val = 'E'; pval++; break;
                case DIK_F: val = 'F'; pval++; break;
                case DIK_G: val = 'G'; pval++; break;
                case DIK_H: val = 'H'; pval++; break;
                case DIK_I: val = 'I'; pval++; break;
                case DIK_J: val = 'J'; pval++; break;
                case DIK_K: val = 'K'; pval++; break;
                case DIK_L: val = 'L'; pval++; break;
                case DIK_M: val = 'M'; pval++; break;
                case DIK_N: val = 'N'; pval++; break;
                case DIK_O: val = 'O'; pval++; break;
                case DIK_P: val = 'P'; pval++; break;
                case DIK_Q: val = 'Q'; pval++; break;
                case DIK_R: val = 'R'; pval++; break;
                case DIK_S: val = 'S'; pval++; break;
                case DIK_T: val = 'T'; pval++; break;
                case DIK_U: val = 'U'; pval++; break;
                case DIK_V: val = 'V'; pval++; break;
                case DIK_W: val = 'W'; pval++; break;
                case DIK_X: val = 'X'; pval++; break;
                case DIK_Y: val = 'W'; pval++; break;
                case DIK_Z: val = 'Z'; pval++; break;
                case DIK_PERIOD: 
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
                  case DIK_0: val = 0; pval++; break;
                  case DIK_1: val = 1; pval++; break;
                  case DIK_2: val = 2; pval++; break;
                  case DIK_3: val = 3; pval++; break;
                  case DIK_4: val = 4; pval++; break;
                  case DIK_5: val = 5; pval++; break;
                  case DIK_6: val = 6; pval++; break;
                  case DIK_7: val = 7; pval++; break;
                  case DIK_8: val = 8; pval++; break;
                  case DIK_9: val = 9; pval++; break;
                  case DIK_A: val = 0xA; pval++; break;
                  case DIK_B: val = 0xB; pval++; break;
                  case DIK_C: val = 0xC; pval++; break;
                  case DIK_D: val = 0xD; pval++; break;
                  case DIK_E: val = 0xE; pval++; break;
                  case DIK_F: val = 0xF; pval++; break;
                  case DIK_PERIOD: 
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
                    case DIK_1: set_note = root_note + (12*1); noplay=1; break;
                    case DIK_2: set_note = root_note + (12*2); noplay=1; break;
                    case DIK_3: set_note = root_note + (12*3); noplay=1; break;
                    case DIK_4: set_note = root_note + (12*4); noplay=1; break;
                    case DIK_5: set_note = root_note + (12*5); noplay=1; break;
                    case DIK_6: set_note = root_note + (12*6); noplay=1; break;
                    case DIK_7: set_note = root_note + (12*7); noplay=1; break;
                    case DIK_8: set_note = root_note + (12*8); noplay=1; break;
                    case DIK_9: set_note = root_note + (12*9); noplay=1; break;
                    case DIK_0: set_note = root_note ; noplay=1; break;
                    default: break;
                    }
                    break;
                    case T_NOTE: 
                      switch(key) {
                        /* TOP ROW */
                      case DIK_Q: set_note = 12*base_octave;         break;
                      case DIK_2: set_note = (12*base_octave)+1;     break;
                      case DIK_W: set_note = (12*base_octave)+2;     break;
                      case DIK_3: set_note = (12*base_octave)+3;     break;
                      case DIK_E: set_note = (12*base_octave)+4;     break;
                      case DIK_R: set_note = (12*base_octave)+5;     break;
                      case DIK_5: set_note = (12*base_octave)+6;     break;
                      case DIK_T: set_note = (12*base_octave)+7;     break;
                      case DIK_6: set_note = (12*base_octave)+8;     break;
                      case DIK_Y: set_note = (12*base_octave)+9;     break;
                      case DIK_7: set_note = (12*base_octave)+10;    break;
                      case DIK_U: set_note = (12*base_octave)+11;    break;
                      case DIK_I: set_note = (12*base_octave)+12;    break;
                      case DIK_9: set_note = (12*base_octave)+1+12;  break;
                      case DIK_O: set_note = (12*base_octave)+2+12;  break;
                      case DIK_0: set_note = (12*base_octave)+3+12;  break;
                      case DIK_P: set_note = (12*base_octave)+4+12;  break;
                      case SDLK_LEFTBRACKET: set_note = (12*base_octave)+5+12;  break;
                      case SDLK_RIGHTBRACKET: set_note = (12*base_octave)+6+12;  break;
                        /* BOTTOM ROW */
                      case DIK_Z: set_note = 12*(base_octave-1);     break;
                      case DIK_S: set_note = (12*(base_octave-1))+1; break;
                      case DIK_X: set_note = (12*(base_octave-1))+2; break;
                      case DIK_D: set_note =(12*(base_octave-1))+3;  break;
                      case DIK_C: set_note =(12*(base_octave-1))+4;  break;
                      case DIK_V: set_note = (12*(base_octave-1))+5; break;
                      case DIK_G: set_note = (12*(base_octave-1))+6; break;
                      case DIK_B: set_note = (12*(base_octave-1))+7; break;
                      case DIK_H: set_note = (12*(base_octave-1))+8; break;
                      case DIK_N: set_note = (12*(base_octave-1))+9; break;
                      case DIK_J: set_note = (12*(base_octave-1))+10;break; 
                      case DIK_M: set_note = (12*(base_octave-1))+11;break;
                        /* EDITING KEYS */
                      case DIK_1: set_note = 0x81; break;      
                      case DIK_GRAVE: set_note = 0x82; break;  
                      case DIK_PERIOD: 
                        if (kstate != KS_SHIFT) {
                          set_note = 0x80; 
                        }
                        break;
                        /* ROW/NOTE PEEK PLAY */
                      case DIK_8:
                        
                        ztPlayer->play_current_row();
                        
                        // <Manu> Hacemos avanzar el cursor
                        
                        cur_edit_row+=cur_step;
            
                // <MANU> 2 Feb 2005 - Arreglado el bug que impedia avanzar el scroll
                //        mientras se tocaba la nota actual o la linea actual y llegabamos abajo

                  if ((cur_edit_row-1 - cur_edit_row_disp) >= (PATTERN_EDIT_ROWS-1)) {
                    
                    if (cur_edit_row_disp<(song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                      
                      cur_edit_row_disp+=cur_step;
                    }
                  }

                        
                        need_refresh++; 
                        
                        // ------------------
                      
                        break;
                      
                      case DIK_4:
                        
                        ztPlayer->play_current_note();

                        // <Manu> Hacemos avanzar el cursor
/*                        
                        cur_edit_row++;
                        cur_edit_row_disp++;
*/
                        cur_edit_row+=cur_step;

                // <MANU> 2 Feb 2005 - Arreglado el bug que impedia avanzar el scroll
                //        mientras se tocaba la nota actual o la linea actual y llegabamos abajo

                  if ((cur_edit_row-1 - cur_edit_row_disp) >= (PATTERN_EDIT_ROWS-1)) {
                    
                    if (cur_edit_row_disp<(song->patterns[cur_edit_pattern]->length - PATTERN_EDIT_ROWS)) {
                      
                      cur_edit_row_disp+=cur_step;
                    }
                  }

                  need_refresh++; 
                        
                        // ------------------

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
                        case DIK_0: val = 0; pval++; break;
                        case DIK_1: val = 1; pval++; break;
                        case DIK_2: val = 2; pval++; break;
                        case DIK_3: val = 3; pval++; break;
                        case DIK_4: val = 4; pval++; break;
                        case DIK_5: val = 5; pval++; break;
                        case DIK_6: val = 6; pval++; break;
                        case DIK_7: val = 7; pval++; break;
                        case DIK_8: val = 8; pval++; break;
                        case DIK_9: val = 9; pval++; break;
                        case DIK_I: val = 0xA0; pval++; break;
                        case DIK_PERIOD: 
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
              switch( dw&0xFF ) {
              case 0x80: // Note off
                key = (dw&0xFF00)>>8 ;
                key+=0xFF;
                MidiOut->noteOff(song->instruments[cur_inst]->midi_device,jazz[key].note,jazz[key].chan,0x0,0);
                jazz[key].note = 0x80;
                break;
              case 0x90: // Note on
                set_note = key = (dw&0xFF00)>>8 ;
                key+=0xFF;
                p1 = (dw&0xFF0000)>>16;
                break;
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
                  if (e=song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row))
                    song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->update_event(cur_edit_row,set_note,-1,zt_config_globals.record_velocity?p1:(-1),-1,-1,-1);
                }
                
                step++;
              }
              if (set_note < 0x80 && !noplay) {
                if ( /*(!ztPlayer->playing || key_jazz) && */jazz[key].note>=0x80 ) {
                  if (p1==-1) {
                    p1 = song->instruments[cur_inst]->default_volume;
                    if (song->instruments[cur_inst]->global_volume != 0x7F && p1>0) {
                      p1 *= song->instruments[cur_inst]->global_volume;
                      p1 /= 0x7f;
                    }
                    if (e=song->patterns[cur_edit_pattern]->tracks[cur_edit_track]->get_event(cur_edit_row))
                      if (e->vol<0x80)
                        p1 = e->vol;
                  }
                  
                  set_note += song->instruments[cur_inst]->transpose; 
                  if (set_note>0x7f) set_note = 0x7f;

                  MidiOut->noteOn(song->instruments[cur_inst]->midi_device,set_note,song->instruments[cur_inst]->channel,(unsigned char)p1,MAX_TRACKS,0);

                  jazz[key].note = set_note;
                  jazz[key].chan = song->instruments[cur_inst]->channel;
                }
              }
            }

            if (key == DIK_SPACE) {

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

                  key = DIK_RIGHT;
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
            if (key==DIK_RIGHT) {
              for(con=0;con < amount;con++) {
                cur_edit_col++;
                if (cur_edit_col>=cols_shown) {
                  cur_edit_col = 0;
                  key=DIK_TAB;
                  kstate = 0;
                }
                need_refresh++;
                sprintf(szStatmsg,"%s",col_desc[cur_edit_col]);
                statusmsg=szStatmsg;
              }
            }
            if (key==DIK_LEFT) {
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
        
        if (key == DIK_PGUP) {
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
        if (key == DIK_PGDN) {
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
        if (key == DIK_DOWN || step) {
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
        if (key==DIK_UP) {
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
        if (key==DIK_TAB && ((!(kstate & KS_ALT)) || oktogo) ) {
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
        if (key==DIK_HOME) {
          
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
        if (key==DIK_END) {
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
          && LastY >= row(15)
          && LastY <= row(15+PATTERN_EDIT_ROWS)
          ) {
          int x,y,data,track;
          x = (LastX - col(5)) / 8;
          y = (LastY - row(15)) / 8;
          track = (x/(field_size+1));
          FU_x = LastX-((field_size+1)*8);//col(5) + ((field_size+1)*track*8);
          FU_y = LastY-16;// row(15);
          FU_dx = LastX+((field_size+1)*8);//FU_x + (field_size+1)*8;
          FU_dy = LastY+16;//row(15+PATTERN_EDIT_ROWS);
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
  int o=0;
  bool m_Fullupd = true;
  
  if (S->lock()==0) {
    
    if (clear) {

      S->fillRect(col(1),row(12),INTERNAL_RESOLUTION_X,INTERNAL_RESOLUTION_Y - (480-424),COLORS.Background);
      clear=0;
    }

    o=0;
    
    draw_status_vars(S);
    
    printtitle(11,"Pattern Editor",COLORS.Text,COLORS.Background,S);
    
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

      
      
      
      
      
      
      
      // <Manu> Esto nunca puede funcionar porque pe_buf es NULL


      
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

      // <Manu> Clarificar un poco esta fórmula, porque con row(52 se comía la última línea 

      screenmanager.Update(0, row(11), INTERNAL_RESOLUTION_X, row(53+((INTERNAL_RESOLUTION_Y-480)/8))) ;
    }
  }
}

