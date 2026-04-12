// keybindings.cpp — Configurable keyboard shortcuts for zTracker.
//
// Users can remap any action by adding entries to zt.conf:
//   key_quit: ctrl+alt+q
//   key_play_song: f5
//   key_undo: alt+z
//
// Entries that are absent from zt.conf use the built-in defaults,
// which reproduce the original hard-coded behaviour exactly.

#include "keybindings.h"
#include "conf.h"
#include "lc_sdl_wrapper.h"   // DIK_* constants
#include "keybuffer.h"        // KS_ALT, KS_CTRL, KS_SHIFT, KS_META

#include <string.h>
#include <stdio.h>
#include <ctype.h>

// -----------------------------------------------------------------------
// Global instance
// -----------------------------------------------------------------------
KeyBindings g_keybindings;

// -----------------------------------------------------------------------
// Action name table
// -----------------------------------------------------------------------
static const char* const s_actionNames[ZT_ACTION_COUNT] = {
    "none",                 // ZT_ACTION_NONE
    "quit",                 // ZT_ACTION_QUIT
    "play_song",            // ZT_ACTION_PLAY_SONG
    "play_pat",             // ZT_ACTION_PLAY_PAT
    "play_pat_line",        // ZT_ACTION_PLAY_PAT_LINE
    "stop",                 // ZT_ACTION_STOP
    "panic",                // ZT_ACTION_PANIC
    "hard_panic",           // ZT_ACTION_HARD_PANIC
    "switch_help",          // ZT_ACTION_SWITCH_HELP
    "switch_pedit",         // ZT_ACTION_SWITCH_PEDIT
    "switch_iedit",         // ZT_ACTION_SWITCH_IEDIT
    "switch_songconf",      // ZT_ACTION_SWITCH_SONGCONF
    "switch_sysconf",       // ZT_ACTION_SWITCH_SYSCONF
    "switch_about",         // ZT_ACTION_SWITCH_ABOUT
    "load",                 // ZT_ACTION_LOAD
    "save",                 // ZT_ACTION_SAVE
    "save_as",              // ZT_ACTION_SAVE_AS
    "new_song",             // ZT_ACTION_NEW_SONG
    "midi_export",          // ZT_ACTION_MIDI_EXPORT
    "undo",                 // ZT_ACTION_UNDO
    "replicate_at_cursor",  // ZT_ACTION_REPLICATE_AT_CURSOR
    "double_pattern",       // ZT_ACTION_DOUBLE_PATTERN
    "clone_pattern",        // ZT_ACTION_CLONE_PATTERN
    "octave_down",          // ZT_ACTION_OCTAVE_DOWN
    "octave_up",            // ZT_ACTION_OCTAVE_UP
    "step_0",               // ZT_ACTION_STEP_0
    "step_1",               // ZT_ACTION_STEP_1
    "step_2",               // ZT_ACTION_STEP_2
    "step_3",               // ZT_ACTION_STEP_3
    "step_4",               // ZT_ACTION_STEP_4
    "step_5",               // ZT_ACTION_STEP_5
    "step_6",               // ZT_ACTION_STEP_6
    "step_7",               // ZT_ACTION_STEP_7
    "step_8",               // ZT_ACTION_STEP_8
    "step_9",               // ZT_ACTION_STEP_9
    "switch_lua_console",   // ZT_ACTION_SWITCH_LUA_CONSOLE
    "halve_pattern",        // ZT_ACTION_HALVE_PATTERN
    "interpolate_selection",// ZT_ACTION_INTERPOLATE_SELECTION
    "follow_playback",      // ZT_ACTION_FOLLOW_PLAYBACK
    "track_solo",           // ZT_ACTION_TRACK_SOLO
    "reverse_selection",    // ZT_ACTION_REVERSE_SELECTION
    "rotate_down",          // ZT_ACTION_ROTATE_DOWN
    "rotate_up",            // ZT_ACTION_ROTATE_UP
};

const char* KeyBindings::actionName(ZtAction action)
{
    if (action < 0 || action >= ZT_ACTION_COUNT)
        return "unknown";
    return s_actionNames[action];
}

void KeyBindings::actionConfKey(ZtAction action, char* buf, int bufsize)
{
    snprintf(buf, bufsize, "key_%s", actionName(action));
}

// -----------------------------------------------------------------------
// setDefaults — must exactly match the original hard-coded bindings
// -----------------------------------------------------------------------
void KeyBindings::setDefaults()
{
    // Clear everything first
    for (int i = 0; i < ZT_ACTION_COUNT; i++) {
        bindings[i].key       = 0;
        bindings[i].modifiers = 0;
    }

    // Global shortcuts — matches global_keys() in main.cpp
    bindings[ZT_ACTION_QUIT]            = { DIK_Q,   KS_ALT | KS_CTRL };
    bindings[ZT_ACTION_PLAY_SONG]       = { DIK_F5,  0 };
    bindings[ZT_ACTION_PLAY_PAT]        = { DIK_F6,  0 };
    bindings[ZT_ACTION_PLAY_PAT_LINE]   = { DIK_F7,  0 };
    bindings[ZT_ACTION_STOP]            = { DIK_F8,  0 };
    bindings[ZT_ACTION_PANIC]           = { DIK_F9,  0 };
    bindings[ZT_ACTION_HARD_PANIC]      = { DIK_F9,  KS_SHIFT };
    bindings[ZT_ACTION_SWITCH_HELP]     = { DIK_F1,  0 };
    bindings[ZT_ACTION_SWITCH_PEDIT]    = { DIK_F2,  0 };
    bindings[ZT_ACTION_SWITCH_IEDIT]    = { DIK_F3,  0 };
    bindings[ZT_ACTION_SWITCH_SONGCONF] = { DIK_F11, 0 };
    bindings[ZT_ACTION_SWITCH_SYSCONF]  = { DIK_F12, 0 };
    bindings[ZT_ACTION_SWITCH_ABOUT]    = { DIK_F12, KS_ALT };
    bindings[ZT_ACTION_LOAD]            = { DIK_L,   KS_CTRL };
    bindings[ZT_ACTION_SAVE]            = { DIK_S,   KS_CTRL };
    bindings[ZT_ACTION_SAVE_AS]         = { DIK_S,   KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_NEW_SONG]        = { DIK_N,   KS_ALT };
    bindings[ZT_ACTION_MIDI_EXPORT]     = { DIK_M,   KS_CTRL };

    // Pattern editor shortcuts — matches CUI_Patterneditor.cpp
    bindings[ZT_ACTION_UNDO]               = { DIK_Z,  KS_ALT };   // also KS_META, handled in match()
    bindings[ZT_ACTION_REPLICATE_AT_CURSOR]= { DIK_R,  KS_ALT };   // ALT+R
    bindings[ZT_ACTION_DOUBLE_PATTERN]     = { DIK_G,  KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_CLONE_PATTERN]      = { DIK_D,  KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_OCTAVE_DOWN]        = { DIK_DIVIDE,   0 };
    bindings[ZT_ACTION_OCTAVE_UP]          = { DIK_MULTIPLY, 0 };
    bindings[ZT_ACTION_STEP_0]             = { DIK_0,  KS_ALT };
    bindings[ZT_ACTION_STEP_1]             = { DIK_1,  KS_ALT };
    bindings[ZT_ACTION_STEP_2]             = { DIK_2,  KS_ALT };
    bindings[ZT_ACTION_STEP_3]             = { DIK_3,  KS_ALT };
    bindings[ZT_ACTION_STEP_4]             = { DIK_4,  KS_ALT };
    bindings[ZT_ACTION_STEP_5]             = { DIK_5,  KS_ALT };
    bindings[ZT_ACTION_STEP_6]             = { DIK_6,  KS_ALT };
    bindings[ZT_ACTION_STEP_7]             = { DIK_7,  KS_ALT };
    bindings[ZT_ACTION_STEP_8]             = { DIK_8,  KS_ALT };
    bindings[ZT_ACTION_STEP_9]             = { DIK_9,  KS_ALT };

    // Lua console — Ctrl+Alt+L (avoids collision with Ctrl+L load shortcut)
    bindings[ZT_ACTION_SWITCH_LUA_CONSOLE] = { DIK_L,  KS_CTRL | KS_ALT };

    // New Paketti/IT features
    bindings[ZT_ACTION_HALVE_PATTERN]        = { DIK_H,        KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_INTERPOLATE_SELECTION]= { DIK_I,        KS_CTRL };
    bindings[ZT_ACTION_FOLLOW_PLAYBACK]      = { DIK_SCROLL,   0 };
    bindings[ZT_ACTION_TRACK_SOLO]           = { DIK_F9,       KS_CTRL };
    bindings[ZT_ACTION_REVERSE_SELECTION]    = { DIK_R,        KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_ROTATE_DOWN]          = { DIK_DOWN,     KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_ROTATE_UP]            = { DIK_UP,       KS_CTRL | KS_SHIFT };
}

// -----------------------------------------------------------------------
// Key name <-> DIK_* lookup table
// -----------------------------------------------------------------------
struct KeyNameEntry { const char* name; int dik; };

static const KeyNameEntry s_keyNames[] = {
    { "a", DIK_A }, { "b", DIK_B }, { "c", DIK_C }, { "d", DIK_D },
    { "e", DIK_E }, { "f", DIK_F }, { "g", DIK_G }, { "h", DIK_H },
    { "i", DIK_I }, { "j", DIK_J }, { "k", DIK_K }, { "l", DIK_L },
    { "m", DIK_M }, { "n", DIK_N }, { "o", DIK_O }, { "p", DIK_P },
    { "q", DIK_Q }, { "r", DIK_R }, { "s", DIK_S }, { "t", DIK_T },
    { "u", DIK_U }, { "v", DIK_V }, { "w", DIK_W }, { "x", DIK_X },
    { "y", DIK_Y }, { "z", DIK_Z },
    { "0", DIK_0 }, { "1", DIK_1 }, { "2", DIK_2 }, { "3", DIK_3 },
    { "4", DIK_4 }, { "5", DIK_5 }, { "6", DIK_6 }, { "7", DIK_7 },
    { "8", DIK_8 }, { "9", DIK_9 },
    { "f1",  DIK_F1  }, { "f2",  DIK_F2  }, { "f3",  DIK_F3  }, { "f4",  DIK_F4  },
    { "f5",  DIK_F5  }, { "f6",  DIK_F6  }, { "f7",  DIK_F7  }, { "f8",  DIK_F8  },
    { "f9",  DIK_F9  }, { "f10", DIK_F10 }, { "f11", DIK_F11 }, { "f12", DIK_F12 },
    { "escape", DIK_ESCAPE },
    { "return", DIK_RETURN }, { "enter", DIK_RETURN },
    { "space",  DIK_SPACE  },
    { "tab",    DIK_TAB    },
    { "back",   DIK_BACK   }, { "backspace", DIK_BACK },
    { "delete", DIK_DELETE },
    { "insert", DIK_INSERT },
    { "home",   DIK_HOME   },
    { "end",    DIK_END    },
    { "pgup",   DIK_PRIOR  }, { "prior", DIK_PRIOR },
    { "pgdn",   DIK_NEXT   }, { "next",  DIK_NEXT  },
    { "up",    DIK_UP    },
    { "down",  DIK_DOWN  },
    { "left",  DIK_LEFT  },
    { "right", DIK_RIGHT },
    { "minus",    DIK_MINUS    },
    { "equals",   DIK_EQUALS   },
    { "comma",    DIK_COMMA    },
    { "period",   DIK_PERIOD   },
    { "slash",    DIK_SLASH    },
    { "divide",   DIK_DIVIDE   },   // numpad /
    { "multiply", DIK_MULTIPLY },   // numpad *
    { "subtract", DIK_SUBTRACT },   // numpad -
    { "add",      DIK_ADD      },   // numpad +
    { "grave",    DIK_GRAVE    },
    { "scroll",   DIK_SCROLL   },
    { NULL, 0 }
};

static int dikFromName(const char* name)
{
    for (int i = 0; s_keyNames[i].name; i++) {
        if (strcmp(s_keyNames[i].name, name) == 0)
            return s_keyNames[i].dik;
    }
    return 0;
}

static const char* nameFromDik(int dik)
{
    for (int i = 0; s_keyNames[i].name; i++) {
        if (s_keyNames[i].dik == dik)
            return s_keyNames[i].name;
    }
    return NULL;
}

// -----------------------------------------------------------------------
// parseKeyCombo — "ctrl+alt+q" -> {key=DIK_Q, modifiers=KS_CTRL|KS_ALT}
// -----------------------------------------------------------------------
bool KeyBindings::parseKeyCombo(const char* str, int &key, int &modifiers)
{
    if (!str || !str[0]) return false;

    key       = 0;
    modifiers = 0;

    // Work on a lowercase copy
    char buf[64];
    int n = (int)strlen(str);
    if (n >= (int)sizeof(buf)) return false;
    for (int i = 0; i <= n; i++)
        buf[i] = (char)tolower((unsigned char)str[i]);

    // Split by '+', last token is the key, earlier ones are modifiers
    char *tokens[8];
    int  ntok = 0;
    char *p = buf;
    while (*p && ntok < 8) {
        tokens[ntok++] = p;
        while (*p && *p != '+') p++;
        if (*p == '+') { *p = '\0'; p++; }
    }

    if (ntok == 0) return false;

    // Last token = key
    const char *keystr = tokens[ntok - 1];
    key = dikFromName(keystr);
    if (key == 0) return false;

    // Earlier tokens = modifiers
    for (int i = 0; i < ntok - 1; i++) {
        if (strcmp(tokens[i], "ctrl")  == 0) modifiers |= KS_CTRL;
        else if (strcmp(tokens[i], "alt")   == 0) modifiers |= KS_ALT;
        else if (strcmp(tokens[i], "shift") == 0) modifiers |= KS_SHIFT;
        else if (strcmp(tokens[i], "meta")  == 0) modifiers |= KS_META;
        else if (strcmp(tokens[i], "cmd")   == 0) modifiers |= KS_META;
        else if (strcmp(tokens[i], "win")   == 0) modifiers |= KS_META;
        else return false; // unknown modifier
    }

    return true;
}

// -----------------------------------------------------------------------
// formatKeyCombo — {key=DIK_Q, modifiers=KS_CTRL|KS_ALT} -> "ctrl+alt+q"
// -----------------------------------------------------------------------
void KeyBindings::formatKeyCombo(int key, int modifiers, char* buf, int bufsize)
{
    buf[0] = '\0';
    if (key == 0) { snprintf(buf, bufsize, "none"); return; }

    char tmp[64] = "";
    if (modifiers & KS_CTRL)  { strncat(tmp, "ctrl+",  sizeof(tmp)-strlen(tmp)-1); }
    if (modifiers & KS_ALT)   { strncat(tmp, "alt+",   sizeof(tmp)-strlen(tmp)-1); }
    if (modifiers & KS_SHIFT) { strncat(tmp, "shift+", sizeof(tmp)-strlen(tmp)-1); }
    if (modifiers & KS_META)  { strncat(tmp, "meta+",  sizeof(tmp)-strlen(tmp)-1); }

    const char* kname = nameFromDik(key);
    if (kname) {
        strncat(tmp, kname, sizeof(tmp)-strlen(tmp)-1);
    } else {
        char num[16];
        snprintf(num, sizeof(num), "key%d", key);
        strncat(tmp, num, sizeof(tmp)-strlen(tmp)-1);
    }

    snprintf(buf, bufsize, "%s", tmp);
}

// -----------------------------------------------------------------------
// load — read overrides from zt.conf
// -----------------------------------------------------------------------
void KeyBindings::load(conf *Config)
{
    if (!Config) return;

    for (int a = ZT_ACTION_NONE + 1; a < ZT_ACTION_COUNT; a++) {
        char confkey[64];
        actionConfKey((ZtAction)a, confkey, sizeof(confkey));
        const char *val = Config->get(confkey);
        if (!val || !val[0]) continue;

        int k = 0, m = 0;
        if (parseKeyCombo(val, k, m)) {
            bindings[a].key       = k;
            bindings[a].modifiers = m;
        }
    }
}

// -----------------------------------------------------------------------
// save — write all bindings to zt.conf
// -----------------------------------------------------------------------
void KeyBindings::save(conf *Config)
{
    if (!Config) return;

    for (int a = ZT_ACTION_NONE + 1; a < ZT_ACTION_COUNT; a++) {
        char confkey[64];
        actionConfKey((ZtAction)a, confkey, sizeof(confkey));

        char val[64];
        formatKeyCombo(bindings[a].key, bindings[a].modifiers, val, sizeof(val));
        Config->set(confkey, val);
    }
}

// -----------------------------------------------------------------------
// match — return the action whose binding exactly matches key+kstate.
//
// Special case for undo: accept KS_META in place of (or in addition to)
// KS_ALT, matching the original "KS_ALT || KS_META" logic.
// -----------------------------------------------------------------------
ZtAction KeyBindings::match(int key, int kstate) const
{
    if (key == 0) return ZT_ACTION_NONE;

    for (int a = ZT_ACTION_NONE + 1; a < ZT_ACTION_COUNT; a++) {
        const KeyBinding &b = bindings[a];
        if (b.key == 0) continue;
        if (b.key != key) continue;

        // Exact modifier match
        if (b.modifiers == kstate) return (ZtAction)a;

        // Special undo: binding uses KS_ALT, but KS_META is also acceptable
        if (a == ZT_ACTION_UNDO) {
            if ((b.modifiers & KS_ALT) &&
                ((kstate & KS_ALT) || (kstate & KS_META)) &&
                !(kstate & ~(KS_ALT | KS_META))) {
                return ZT_ACTION_UNDO;
            }
        }
    }
    return ZT_ACTION_NONE;
}
