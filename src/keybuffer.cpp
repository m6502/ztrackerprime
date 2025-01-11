/*************************************************************************
 *
 * FILE  keybuffer.cpp
 * $Id: keybuffer.cpp,v 1.6 2001/10/16 05:45:06 cmicali Exp $
 *
 * DESCRIPTION 
 *   Buffer for holding keystrokes and keystates.
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

/*
class KeyBuffer {

    public:
        
        keybuffer();
        ~keybuffer();

        unsigned char getkey();
        unsigned char size();
        void insert(unsigned char key);

    private:
        
        unsigned char buffer[256];
        unsigned char head;
        unsigned char tail;

}
*/


KeyBuffer::KeyBuffer(void) {
    tail=head=0;
    maxsize = 200;
    cursize=0;
}

void KeyBuffer::flush(void) {
    tail=head;
    cursize = 0;
}

KBKey KeyBuffer::checkkey(void) {
    unsigned char a=0;
    KBKey ret = SDLK_UNKNOWN;
    if (head != tail) {
        a=tail+1;
        if (a>=maxsize) a = 0;
        ret = buffer[a].key;
        this->cur_state = buffer[a].state;
        this->actual_char = buffer[tail].actual_char;
    }
    return ret;
}
KBKey KeyBuffer::getkey(void) {
    unsigned char a=0; 
    KBKey ret=SDLK_UNKNOWN;
    if (head != tail) {
        ++tail;
        --cursize;
        if (tail>=maxsize) tail = 0;
        ret = buffer[tail].key;
        this->cur_state = buffer[tail].state;
        this->actual_char = buffer[tail].actual_char;
    }
    return ret;
}
KBMod KeyBuffer::getstate(void) {
    return this->cur_state;
}
unsigned char KeyBuffer::getactualchar(void) {
    return this->actual_char;
}
unsigned char KeyBuffer::size(void) {
    return cursize;
/*  if (head<tail)
        return ((head+maxsize)-tail);
    else
        return head-tail;
*/
}
void KeyBuffer::insert(KBKey key, KBMod state, unsigned char actual_char) {
    unsigned char ls;
    switch(key) {
        case DIK_NUMPAD0: key=DIK_0; break;
        case DIK_NUMPAD1: key=DIK_1; break;
        case DIK_NUMPAD2: key=DIK_2; break;
        case DIK_NUMPAD3: key=DIK_3; break;
        case DIK_NUMPAD4: key=DIK_4; break;
        case DIK_NUMPAD5: key=DIK_5; break;
        case DIK_NUMPAD6: key=DIK_6; break;
        case DIK_NUMPAD7: key=DIK_7; break;
        case DIK_NUMPAD8: key=DIK_8; break;
        case DIK_NUMPAD9: key=DIK_9; break;
        default: break;
    }
    if (cursize>=maxsize) 
        return;
    ls = buffer[head].state;
    if ((++head)>=maxsize)
        head = 0;
    ++cursize;

#ifdef DEBUG
    keybuff_bg->setvalue(cursize);
#endif

    buffer[head].key = key;
    KBMod c = 0;
    if (state & KMOD_ALT)
        c |= KS_ALT;
    if (state & KMOD_CTRL)
        c |= KS_CTRL;
    if (state & KMOD_SHIFT)
        c |= KS_SHIFT;
    if (state == KS_LAST_STATE) {
        buffer[head].actual_char = last_actual_char;
        buffer[head].state = ls;
    } else {
        buffer[head].actual_char = actual_char;
        buffer[head].state = c;
        last_actual_char = actual_char;
    }
}
/*
void KeyBuffer::insert(unsigned char key) {
    if (cursize>=maxsize) 
        return;
    if ((++head)>=maxsize)
        head = 0;
    ++cursize;
#ifdef DEBUG
    keybuff_bg->setvalue(cursize);
#endif
    buffer[head].key = key;
    buffer[head].state = KS_NO_SHIFT_KEYS;
    if (K[DIK_LALT] || K[DIK_RALT])
        buffer[head].state |= KS_ALT;
    if (K[DIK_LCONTROL] || K[DIK_RCONTROL])
        buffer[head].state |= KS_CTRL;
    if (K[DIK_LSHIFT] || K[DIK_RSHIFT])
        buffer[head].state |= KS_SHIFT;
}
*/
