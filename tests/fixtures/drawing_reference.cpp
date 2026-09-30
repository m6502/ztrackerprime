// Pre-optimization reference functions retained for byte/pixel regression checks.
// Extracted verbatim from zTracker on 2026-09-26; only names/signatures changed.
#include "zt.h"

char *reference_printNote(char *str, event *r, int cur_edit_mode) 
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
char *reference_playback_note(char *str, event *r, int view) 
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
  
  switch(view) 
  {
  case 0:
    //sprintf(str,"%.3s",note,vol); // 2 cols
    sprintf(str,"%.3s %.2s",note,vol); // 2 cols
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
void reference_printchar(int x, int y, unsigned char ch, TColor col, Drawable *S) {
    TColor *buf;
    unsigned char byte;
    int i,j;
    for(i=0;i<8;i++) {


        if((y + i) >= S->surface->h) continue ;
        if((x + 7) >= S->surface->w) continue ;


        byte = font[(((int)ch)<<3)+i];
        buf = S->getLine(y+i) + x + 7;




        for(j=0;j<8;j++) {
            if (byte & 1) 

                *buf = col;
            buf--;
            byte >>= 1;
        }
    }
}
