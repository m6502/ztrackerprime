#ifndef _KEYBUFFER_H
#define _KEYBUFFER_H

// Production builds include real SDL3. Test executables build with
// -DZT_TEST_NO_SDL plus tests/sdl_stub.h on the include path; we pull
// the stub in here so any cpp that #includes keybuffer.h gets the
// SDL_KMOD_* / SDLK_* macros without each test source repeating the
// include.
#ifdef ZT_TEST_NO_SDL
#include "sdl_stub.h"
#else
#include <SDL.h>
#endif

#define KS_NO_SHIFT_KEYS 0
#define KS_ALT   1
#define KS_CTRL  2
#define KS_SHIFT 4
#define KS_META  8   // Cmd key on macOS, Win key on Windows
#define KS_LAST_STATE 0xFF

// Helper: true if ALT is active (matches ALT alone, META alone, or META+ALT)
// Used so that on macOS, Cmd+key works everywhere ALT+key does.
#define KS_HAS_ALT(ks) (((ks) & (KS_ALT|KS_META)) && !((ks) & (KS_CTRL|KS_SHIFT)))

typedef unsigned int KBKey ;
typedef unsigned int KBMod;

class KeyBuffer {


    public:
        
        KeyBuffer(void);
        ~KeyBuffer(void) = default ;

        KBKey getkey(void);
        unsigned int getcode();
        KBMod getstate(void);
        unsigned char getactualchar(void);
        unsigned char size(void);
        void insert(KBKey key, KBMod state = SDL_KMOD_NONE, unsigned char actual_char = 0, unsigned int code = 0);
//        void insert(unsigned char key, Keyboard &K);
        void flush(void);

        KBKey checkkey(void);

        struct c {
            KBKey key;
            unsigned int code;
            unsigned char  state;
            unsigned char actual_char;
        } buffer[256];
        
        unsigned char cur_state;
        unsigned char head;
        unsigned char tail;
        unsigned char maxsize,cursize;
        unsigned char last_actual_char,actual_char;
        unsigned int actual_code;
};

#endif
