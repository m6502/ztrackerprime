/*************************************************************************
 *
 * FILE  edit_cols.cpp
 * $Id: edit_cols.cpp,v 1.7 2001/08/17 18:14:43 zonaj Exp $
 *
 * DESCRIPTION 
 *   Defines editing columns.
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

// this is such a kludge, oh well, it works right?


void init_short_cols(void) {
    edit_cols[2].startx = 4;
    edit_cols[2].type   = T_VOL;
    edit_cols[2].place  = 1;
    edit_cols[3].startx = 5;
    edit_cols[3].type   = T_VOL;
    edit_cols[3].place  = 0;
    col_desc[2] = col_desc[3] = "Volume (0-7F)";
}
void fix_short_cols(void) {
    edit_cols[2].startx = 4;
    edit_cols[2].type   = T_INST;
    edit_cols[2].place  = 1;
    edit_cols[3].startx = 5;
    edit_cols[3].type   = T_INST;
    edit_cols[3].place  = 0;
    col_desc[2] = col_desc[3] = "Instrument (0-7F)";
}

void init_fx_cols(void)
{
	init_short_cols();
    edit_cols[4].startx = 7;
    edit_cols[4].type   = T_FX;
    edit_cols[4].place  = 0;
    col_desc[4] = "Effect";
    edit_cols[5].startx = 8;
    edit_cols[5].type   = T_FXP;
    edit_cols[5].place  = 3;
    edit_cols[6].startx = 9;
    edit_cols[6].type   = T_FXP;
    edit_cols[6].place  = 2;
    edit_cols[7].startx = 10;
    edit_cols[7].type   = T_FXP;
    edit_cols[7].place  = 1;
    edit_cols[8].startx = 11;
    edit_cols[8].type   = T_FXP;
    edit_cols[8].place  = 0;
    col_desc[5] = col_desc[6] = col_desc[7] = col_desc[8] = "Effect Parameter";
}

void fix_cols(void) {
    init_edit_cols();
}

void init_edit_cols(void) {
    int i,o=0;

enum E_col_type { T_NOTE, T_OCTAVE, T_INST, T_VOL, T_CHAN, T_LEN, 
                  T_FX1, T_FX1P, T_FX2, T_FX2P, T_FX3, T_FX3P, T_FX4, T_FX4P, T_FX5, T_FX5P };
    
    edit_cols[0].startx = 0;
    edit_cols[0].type   = T_NOTE;
    edit_cols[0].place  = 0;
    col_desc[0] = "Note (All keys)";
    
    edit_cols[1].startx = 2;
    edit_cols[1].type   = T_OCTAVE;
    edit_cols[1].place  = 0;
    col_desc[1] = "Octave (0-9)";
    
    edit_cols[2].startx = 4;
    edit_cols[2].type   = T_INST;
    edit_cols[2].place  = 1;
    edit_cols[3].startx = 5;
    edit_cols[3].type   = T_INST;
    edit_cols[3].place  = 0;
    col_desc[2] = col_desc[3] = "Instrument (0-7F)";
    
    edit_cols[4].startx = 7;
    edit_cols[4].type   = T_VOL;
    edit_cols[4].place  = 1;
    edit_cols[5].startx = 8;
    edit_cols[5].type   = T_VOL;
    edit_cols[5].place  = 0;
    col_desc[4] = col_desc[5] = "Volume (0-7F)";
    
    edit_cols[6].startx = 10;
    edit_cols[6].type   = T_LEN;
    edit_cols[6].place  = 2;
    edit_cols[7].startx = 11;
    edit_cols[7].type   = T_LEN;
    edit_cols[7].place  = 1;
    edit_cols[8].startx = 12;
    edit_cols[8].type   = T_LEN;
    edit_cols[8].place  = 0;
    col_desc[6] = col_desc[7] = col_desc[8] = "Length (0-999)";

    for(i=9;i<41;i++)
        col_desc[i] = NULL;

    col_desc[9] = "Effect";
    col_desc[11] = col_desc[12] = col_desc[13] = col_desc[10] =  "Effect Parameter";

        edit_cols[9].startx = 14;
        edit_cols[9].type   = T_FX;
        edit_cols[9].place = 0;

        edit_cols[10].startx = 15;
        edit_cols[10].type   = T_FXP;
        edit_cols[10].place  = 3;
        edit_cols[11].startx = 16;
        edit_cols[11].type   = T_FXP;
        edit_cols[11].place  = 2;
        edit_cols[12].startx = 17;
        edit_cols[12].type   = T_FXP;
        edit_cols[12].place  = 1;
        edit_cols[13].startx = 18;
        edit_cols[13].type   = T_FXP;
        edit_cols[13].place  = 0;
}