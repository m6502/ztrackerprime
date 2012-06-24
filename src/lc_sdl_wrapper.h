#ifndef _LC_SDL_WRAPPER_H
#define _LC_SDL_WRAPPER_H

#include "zt.h"

#ifndef MAXINT
   #define MAXINT     0x7FFFFFFF
#endif

typedef Uint32 TColor;


class Drawable
{
public:

  Drawable(int w = 640, int h = 480, bool free_surface = true);

  Drawable(SDL_Surface *target_surface, bool free_surface = false);
  ~Drawable();

  TColor* getLine(int y);
  TColor getColor (int Red, int Green, int Blue);
  long unlock ();
  long lock ();
  long fillRect (int x1, int y1, int x2, int y2, TColor color);
  void release(void);
  void Release(void);
  long copy(Drawable* Source, int x, int y, int x1, int y1, int x2, int y2);

  long copy(Drawable* Source, int x, int y);
  void flip(void);
  void drawHLine(int y, int x, int x2, TColor c);
  void drawVLine(int x, int y, int y2, TColor c);

public:

  SDL_Surface *surface;
  bool free_surface_on_delete;
  int width,height;
};

typedef Drawable Bitmap;
typedef Drawable Screen;


Bitmap* newBitmap(int Width, int Height, int Flags=0);

//#define DIK_ESCAPE          0x01
//#define DIK_1               0x02
//#define DIK_2               0x03
//#define DIK_3               0x04
//#define DIK_4               0x05
//#define DIK_5               0x06
//#define DIK_6               0x07
//#define DIK_7               0x08
//#define DIK_8               0x09
//#define DIK_9               0x0A
//#define DIK_0               0x0B
//#define DIK_MINUS           0x0C    /* - on main keyboard */
//#define DIK_EQUALS          0x0D
//#define DIK_BACK            0x0E    /* backspace */
//#define DIK_TAB             0x0F
//#define DIK_Q               0x10
//#define DIK_W               0x11
//#define DIK_E               0x12
//#define DIK_R               0x13
//#define DIK_T               0x14
//#define DIK_Y               0x15
//#define DIK_U               0x16
//#define DIK_I               0x17
//#define DIK_O               0x18
//#define DIK_P               0x19
//#define DIK_LBRACKET        0x1A
//#define DIK_RBRACKET        0x1B
//#define DIK_RETURN          0x1C    /* Enter on main keyboard */
//#define DIK_LCONTROL        0x1D
//#define DIK_A               0x1E
//#define DIK_S               0x1F
//#define DIK_D               0x20
//#define DIK_F               0x21
//#define DIK_G               0x22
//#define DIK_H               0x23
//#define DIK_J               0x24
//#define DIK_K               0x25
//#define DIK_L               0x26
//#define DIK_SEMICOLON       0x27
//#define DIK_APOSTROPHE      0x28
//#define DIK_GRAVE           0x29    /* accent grave */
//#define DIK_LSHIFT          0x2A
//#define DIK_BACKSLASH       0x2B
//#define DIK_Z               0x2C
//#define DIK_X               0x2D
//#define DIK_C               0x2E
//#define DIK_V               0x2F
//#define DIK_B               0x30
//#define DIK_N               0x31
//#define DIK_M               0x32
//#define DIK_COMMA           0x33
//#define DIK_PERIOD          0x34    /* . on main keyboard */
//#define DIK_SLASH           0x35    /* / on main keyboard */
//#define DIK_RSHIFT          0x36
//#define DIK_MULTIPLY        0x37    /* * on numeric keypad */
//#define DIK_LMENU           0x38    /* left Alt */
//#define DIK_SPACE           0x39
//#define DIK_CAPITAL         0x3A
//#define DIK_F1              0x3B
//#define DIK_F2              0x3C
//#define DIK_F3              0x3D
//#define DIK_F4              0x3E
//#define DIK_F5              0x3F
//#define DIK_F6              0x40
//#define DIK_F7              0x41
//#define DIK_F8              0x42
//#define DIK_F9              0x43
//#define DIK_F10             0x44
//#define DIK_NUMLOCK         0x45
//#define DIK_SCROLL          0x46    /* Scroll Lock */
//#define DIK_NUMPAD7         0x47
//#define DIK_NUMPAD8         0x48
//#define DIK_NUMPAD9         0x49
//#define DIK_SUBTRACT        0x4A    /* - on numeric keypad */
//#define DIK_NUMPAD4         0x4B
//#define DIK_NUMPAD5         0x4C
//#define DIK_NUMPAD6         0x4D
//#define DIK_ADD             0x4E    /* + on numeric keypad */
//#define DIK_NUMPAD1         0x4F
//#define DIK_NUMPAD2         0x50
//#define DIK_NUMPAD3         0x51
//#define DIK_NUMPAD0         0x52
//#define DIK_DECIMAL         0x53    /* . on numeric keypad */
//#define DIK_F11             0x57
//#define DIK_F12             0x58
//
//#define DIK_F13             0x64    /*                     (NEC PC98) */
//#define DIK_F14             0x65    /*                     (NEC PC98) */
//#define DIK_F15             0x66    /*                     (NEC PC98) */
//
//#define DIK_KANA            0x70    /* (Japanese keyboard)            */
//#define DIK_CONVERT         0x79    /* (Japanese keyboard)            */
//#define DIK_NOCONVERT       0x7B    /* (Japanese keyboard)            */
//#define DIK_YEN             0x7D    /* (Japanese keyboard)            */
//#define DIK_NUMPADEQUALS    0x8D    /* = on numeric keypad (NEC PC98) */
//#define DIK_CIRCUMFLEX      0x90    /* (Japanese keyboard)            */
//#define DIK_AT              0x91    /*                     (NEC PC98) */
//#define DIK_COLON           0x92    /*                     (NEC PC98) */
//#define DIK_UNDERLINE       0x93    /*                     (NEC PC98) */
//#define DIK_KANJI           0x94    /* (Japanese keyboard)            */
//#define DIK_STOP            0x95    /*                     (NEC PC98) */
//#define DIK_AX              0x96    /*                     (Japan AX) */
//#define DIK_UNLABELED       0x97    /*                        (J3100) */
//#define DIK_NUMPADENTER     0x9C    /* Enter on numeric keypad */
//#define DIK_RCONTROL        0x9D
//#define DIK_NUMPADCOMMA     0xB3    /* , on numeric keypad (NEC PC98) */
//#define DIK_DIVIDE          0xB5    /* / on numeric keypad */
//#define DIK_SYSRQ           0xB7
//#define DIK_RMENU           0xB8    /* right Alt */
//#define DIK_HOME            0xC7    /* Home on arrow keypad */
//#define DIK_UP              0xC8    /* UpArrow on arrow keypad */
//#define DIK_PRIOR           0xC9    /* PgUp on arrow keypad */
//#define DIK_LEFT            0xCB    /* LeftArrow on arrow keypad */
//#define DIK_RIGHT           0xCD    /* RightArrow on arrow keypad */
//#define DIK_END             0xCF    /* End on arrow keypad */
//#define DIK_DOWN            0xD0    /* DownArrow on arrow keypad */
//#define DIK_NEXT            0xD1    /* PgDn on arrow keypad */
//#define DIK_INSERT          0xD2    /* Insert on arrow keypad */
//#define DIK_DELETE          0xD3    /* Delete on arrow keypad */
//#define DIK_LWIN            0xDB    /* Left Windows key */
//#define DIK_RWIN            0xDC    /* Right Windows key */
//#define DIK_APPS            0xDD    /* AppMenu key */
//
//#define DIK_BACKSPACE       DIK_BACK            /* backspace */
//#define DIK_NUMPADSTAR      DIK_MULTIPLY        /* * on numeric keypad */
//#define DIK_LALT            DIK_LMENU           /* left Alt */
//#define DIK_CAPSLOCK        DIK_CAPITAL         /* CapsLock */
//#define DIK_NUMPADMINUS     DIK_SUBTRACT        /* - on numeric keypad */
//#define DIK_NUMPADPLUS      DIK_ADD             /* + on numeric keypad */
//#define DIK_NUMPADPERIOD    DIK_DECIMAL         /* . on numeric keypad */
//#define DIK_NUMPADSLASH     DIK_DIVIDE          /* / on numeric keypad */
//#define DIK_RALT            DIK_RMENU           /* right Alt */
//#define DIK_UPARROW         DIK_UP              /* UpArrow on arrow keypad */
//#define DIK_PGUP            DIK_PRIOR           /* PgUp on arrow keypad */
//#define DIK_LEFTARROW       DIK_LEFT            /* LeftArrow on arrow keypad */
//#define DIK_RIGHTARROW      DIK_RIGHT           /* RightArrow on arrow keypad */
//#define DIK_DOWNARROW       DIK_DOWN            /* DownArrow on arrow keypad */
//#define DIK_PGDN            DIK_NEXT            /* PgDn on arrow keypad */
//

/* Keys mapping .. couldnt use SDLK_ because they are enums :[ */

#define DIK_A               97  
#define DIK_B               98  
#define DIK_C               99  
#define DIK_D               100 
#define DIK_E               101 
#define DIK_F               102 
#define DIK_G               103 
#define DIK_H               104 
#define DIK_I               105 
#define DIK_J               106 
#define DIK_K               107 
#define DIK_L               108 
#define DIK_M               109 
#define DIK_N               110 
#define DIK_O               111 
#define DIK_P               112 
#define DIK_Q               113 
#define DIK_R               114 
#define DIK_S               115 
#define DIK_T               116 
#define DIK_U               117 
#define DIK_V               118 
#define DIK_W               119 
#define DIK_X               120 
#define DIK_Y               121 
#define DIK_Z               122 


#define DIK_ESCAPE          27

#define DIK_0               48  
#define DIK_1               49  
#define DIK_2               50  
#define DIK_3               51  
#define DIK_4               52  
#define DIK_5               53  
#define DIK_6               54  
#define DIK_7               55  
#define DIK_8               56  
#define DIK_9               57  

#define DIK_MINUS           45        // - on main keyboard 
#define DIK_EQUALS          61    
#define DIK_BACK            8      // backspace 
#define DIK_TAB             9  

#define DIK_LBRACKET        91
#define DIK_RBRACKET        92
#define DIK_RETURN          13   // Enter on main keyboard 
#define DIK_LCONTROL        306

#define DIK_SEMICOLON       59
#define DIK_APOSTROPHE      34
#define DIK_GRAVE           96    // accent grave 
#define DIK_LSHIFT          304
#define DIK_BACKSLASH       92

#define DIK_COMMA           44
#define DIK_PERIOD          46    // . on main keyboard 
#define DIK_SLASH           47    // / on main keyboard 
#define DIK_RSHIFT          303
#define DIK_MULTIPLY        268    // * on numeric keypad 
#define DIK_LMENU           308    // left Alt 
#define DIK_SPACE           32
#define DIK_CAPITAL         400
#define DIK_NUMLOCK         300
#define DIK_SCROLL          302    // Scroll Lock 
#define DIK_NUMPAD0         256
#define DIK_NUMPAD1         257
#define DIK_NUMPAD2         258
#define DIK_NUMPAD3         259
#define DIK_NUMPAD4         260
#define DIK_NUMPAD5         261
#define DIK_NUMPAD6         262
#define DIK_NUMPAD7         263
#define DIK_NUMPAD8         264
#define DIK_NUMPAD9         265
#define DIK_SUBTRACT        269    // - on numeric keypad 
#define DIK_ADD             270    // + on numeric keypad 
#define DIK_DECIMAL         266    // . on numeric keypad 

#define DIK_F1              282
#define DIK_F2              283 
#define DIK_F3              284
#define DIK_F4              285
#define DIK_F5              286
#define DIK_F6              287
#define DIK_F7              288
#define DIK_F8              289
#define DIK_F9              290
#define DIK_F10             291
#define DIK_F11             292 
#define DIK_F12             293 
#define DIK_F13             294     //                     (NEC PC98) 
#define DIK_F14             295     //                     (NEC PC98) 
#define DIK_F15             296     //                     (NEC PC98) 

//#define DIK_KANA            0x70    // (Japanese keyboard)            
//#define DIK_CONVERT         0x79    // (Japanese keyboard)            
//#define DIK_NOCONVERT       0x7B    // (Japanese keyboard)            
//#define DIK_YEN             0x7D    // (Japanese keyboard)            
#define DIK_NUMPADEQUALS    272    // = on numeric keypad (NEC PC98) 
//#define DIK_CIRCUMFLEX      0x90    // (Japanese keyboard)            
//#define DIK_AT              0x91    //                     (NEC PC98) 
#define DIK_COLON           58    //                     (NEC PC98) 
//#define DIK_UNDERLINE       0x93    //                     (NEC PC98) 
//#define DIK_KANJI           0x94    // (Japanese keyboard)            
//#define DIK_STOP            0x95    //                     (NEC PC98) 
//#define DIK_AX              0x96    //                     (Japan AX) 
//#define DIK_UNLABELED       0x97    //                        (J3100) 
#define DIK_NUMPADENTER     271    // Enter on numeric keypad 
#define DIK_RCONTROL        0x9D
//#define DIK_NUMPADCOMMA     0xB3    // , on numeric keypad (NEC PC98) 
#define DIK_DIVIDE          267    // / on numeric keypad 
//#define DIK_SYSRQ           0xB7
#define DIK_RMENU           0xB8    // right Alt 
#define DIK_HOME            278    // Home on arrow keypad 
#define DIK_UP              273    // UpArrow on arrow keypad 
#define DIK_PRIOR           280    // PgUp on arrow keypad 
#define DIK_LEFT            276    // LeftArrow on arrow keypad 
#define DIK_RIGHT           275    // RightArrow on arrow keypad 
#define DIK_END             279    // End on arrow keypad 
#define DIK_DOWN            274    // DownArrow on arrow keypad 
#define DIK_NEXT            281    // PgDn on arrow keypad 
#define DIK_INSERT          277    // Insert on arrow keypad 
#define DIK_DELETE          127  // Delete on arrow keypad 
#define DIK_LWIN            311    // Left Windows key 
#define DIK_RWIN            312    // Right Windows key 
#define DIK_APPS            314    // AppMenu key 


//  Alternate names for keys, to facilitate transition from DOS.
 
#define DIK_BACKSPACE       DIK_BACK            // backspace 
#define DIK_NUMPADSTAR      DIK_MULTIPLY        // * on numeric keypad 
#define DIK_LALT            DIK_LMENU           // left Alt 
#define DIK_CAPSLOCK        DIK_CAPITAL         // CapsLock 
#define DIK_NUMPADMINUS     DIK_SUBTRACT        // - on numeric keypad 
#define DIK_NUMPADPLUS      DIK_ADD             // + on numeric keypad 
#define DIK_NUMPADPERIOD    DIK_DECIMAL         // . on numeric keypad 
#define DIK_NUMPADSLASH     DIK_DIVIDE          // / on numeric keypad 
#define DIK_RALT            DIK_RMENU           // right Alt 
#define DIK_UPARROW         DIK_UP              // UpArrow on arrow keypad 
#define DIK_PGUP            DIK_PRIOR           // PgUp on arrow keypad 
#define DIK_LEFTARROW       DIK_LEFT            // LeftArrow on arrow keypad 
#define DIK_RIGHTARROW      DIK_RIGHT           // RightArrow on arrow keypad 
#define DIK_DOWNARROW       DIK_DOWN            // DownArrow on arrow keypad 
#define DIK_PGDN            DIK_NEXT            // PgDn on arrow keypad 
















#endif // _LC_SDL_WRAPPER_H