#include "zt.h"

#define LEFT_MARGIN                     40
#define RIGHT_MARGIN                    2

#define TRACKS_POS_Y                    row(TRACKS_ROW_Y)
#define TRACKS_FIRST_NOTE_POS_Y         row(TRACKS_ROW_Y + 1)

#define SPACE_IN_CHARACTERS_AT_BOTTOM   20


int PATTERN_EDIT_ROWS = 100;
int m_upd = 0;
event *upd_e = NULL;

int max_displayable_rows = 0 ;
int g_posx_tracks = 0 ;

int last_unused_key = 0 ;

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

    // <Manu> Nuevo comando B para hacer loops en la GBA; pintamos la informacion
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
