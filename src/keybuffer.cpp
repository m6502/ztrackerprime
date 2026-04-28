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

// keybuffer.cpp only needs the KeyBuffer class declaration plus the
// SDL_KMOD_* / SDLK_* macros surfaced through keybuffer.h. zt.h was a
// cargo-cult include from the rest of the tree; pulling it in made it
// impossible to compile keybuffer.cpp into the unit-test executables
// without also dragging in SDL3, the Lua engine, and the world.
#include "keybuffer.h"

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
    KBKey ret = 0;
    // Use cursize, not head!=tail: when the ring is exactly full,
    // head wraps back to tail and the head!=tail check incorrectly
    // reports "empty" with 200 keys still pending. Found by the
    // tests/test_keybuffer.cpp overflow drain test.
    if (cursize > 0) {
        a=tail+1;
        if (a>=maxsize) a = 0;
        ret = buffer[a].key;
        this->cur_state = buffer[a].state;
        this->actual_char = buffer[a].actual_char;
        this->actual_code = buffer[a].code;
    }
    return ret;
}
KBKey KeyBuffer::getkey(void) {
    KBKey ret=0;
    if (cursize > 0) {
        ++tail;
        --cursize;
        if (tail>=maxsize) tail = 0;
        ret = buffer[tail].key;
        this->cur_state = buffer[tail].state;
        this->actual_char = buffer[tail].actual_char;
        this->actual_code = buffer[tail].code;
    }
    return ret;
}
KBMod KeyBuffer::getstate(void) {
    return this->cur_state;
}
unsigned char KeyBuffer::getactualchar(void) {
    return this->actual_char;
}

unsigned int KeyBuffer::getcode() {
    return this->actual_code;
}

unsigned char KeyBuffer::size(void) {
    return cursize;
/*  if (head<tail)
        return ((head+maxsize)-tail);
    else
        return head-tail;
*/
}
void KeyBuffer::insert(KBKey key, KBMod state, unsigned char actual_char, unsigned int code) {
    unsigned char ls;
    switch(key) {
        case SDLK_KP_0: key=SDLK_0; break;
        case SDLK_KP_1: key=SDLK_1; break;
        case SDLK_KP_2: key=SDLK_2; break;
        case SDLK_KP_3: key=SDLK_3; break;
        case SDLK_KP_4: key=SDLK_4; break;
        case SDLK_KP_5: key=SDLK_5; break;
        case SDLK_KP_6: key=SDLK_6; break;
        case SDLK_KP_7: key=SDLK_7; break;
        case SDLK_KP_8: key=SDLK_8; break;
        case SDLK_KP_9: key=SDLK_9; break;
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
    buffer[head].code = code;
    KBMod c = 0;
    if (state & SDL_KMOD_ALT)
        c |= KS_ALT;
    if (state & SDL_KMOD_CTRL)
        c |= KS_CTRL;
    if (state & SDL_KMOD_SHIFT)
        c |= KS_SHIFT;
#ifdef __APPLE__
    // On macOS, map Cmd (SDL_KMOD_GUI) to KS_META AND KS_ALT so that
    // Cmd+key works everywhere ALT+key does (Cmd is the primary modifier on Mac).
    if (state & SDL_KMOD_GUI)
        c |= KS_META | KS_ALT;
#else
    if (state & SDL_KMOD_GUI)
        c |= KS_META;
#endif
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
    if (K[SDLK_LALT] || K[SDLK_RALT])
        buffer[head].state |= KS_ALT;
    if (K[SDLK_LCTRL] || K[SDLK_RCTRL])
        buffer[head].state |= KS_CTRL;
    if (K[SDLK_LSHIFT] || K[SDLK_RSHIFT])
        buffer[head].state |= KS_SHIFT;
}
*/
