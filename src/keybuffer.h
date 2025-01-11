#ifndef _KEYBUFFER_H
#define _KEYBUFFER_H

#include <SDL.h>

#define KS_NO_SHIFT_KEYS 0
#define KS_ALT   1
#define KS_CTRL  2
#define KS_SHIFT 4
#define KS_LAST_STATE 0xFF

typedef unsigned int KBKey ;
typedef unsigned int KBMod;

class KeyBuffer {


    public:
        
        KeyBuffer(void);
        ~KeyBuffer(void) = default ;

        KBKey getkey(void);
        KBMod getstate(void);
        unsigned char getactualchar(void);
        unsigned char size(void);
        void insert(KBKey key, KBMod state = KMOD_NONE, unsigned char actual_char=0x0 );
//        void insert(unsigned char key, Keyboard &K);
        void flush(void);

        KBKey checkkey(void);

        //unsigned char buffer[256];
        struct c {
            KBKey key;
            unsigned char  state;
            unsigned char actual_char;
        } buffer[256];
        
        unsigned char cur_state;
        unsigned char head;
        unsigned char tail;
        unsigned char maxsize,cursize;
        unsigned char last_actual_char,actual_char;

        
};

#endif