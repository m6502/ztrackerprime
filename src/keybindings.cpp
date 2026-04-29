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
#include <SDL.h>                // SDLK_* constants
#include "keybuffer.h"          // KS_ALT, KS_CTRL, KS_SHIFT

#include <string.h>
#include <stdio.h>
#include <ctype.h>

// KS_META is not defined in this SDL3 port's keybuffer.h — define a
// compatible alias so the match() special-case compiles, but map it
// to 0 so it never fires in practice (no meta-key path on SDL3 yet).
#ifndef KS_META
#  define KS_META 0
#endif

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
    "note_audition",        // ZT_ACTION_NOTE_AUDITION
    "row_audition",         // ZT_ACTION_ROW_AUDITION
};

// Same length as s_actionNames; rendered in the Shortcuts & MIDI
// Mappings page so the user never sees snake_case keys. Keep both
// arrays in lock-step with the enum -- adding an action means
// appending here too. Kept short enough to fit a single column at
// 640px (under ~38 chars).
static const char* const s_actionDescriptions[ZT_ACTION_COUNT] = {
    "(none)",                              // ZT_ACTION_NONE
    "Quit zTracker",                       // ZT_ACTION_QUIT
    "Play Song from start",                // ZT_ACTION_PLAY_SONG
    "Play Pattern from start",             // ZT_ACTION_PLAY_PAT
    "Play Pattern from current row",       // ZT_ACTION_PLAY_PAT_LINE
    "Stop playback",                       // ZT_ACTION_STOP
    "Panic (note-off all channels)",       // ZT_ACTION_PANIC
    "Hard panic (controller reset)",       // ZT_ACTION_HARD_PANIC
    "Switch to Help (F1)",                 // ZT_ACTION_SWITCH_HELP
    "Switch to Pattern Editor (F2)",       // ZT_ACTION_SWITCH_PEDIT
    "Switch to Instrument Editor (F3)",    // ZT_ACTION_SWITCH_IEDIT
    "Switch to Song Configuration (F11)",  // ZT_ACTION_SWITCH_SONGCONF
    "Switch to System Configuration (F12)",// ZT_ACTION_SWITCH_SYSCONF
    "Switch to About (Alt+F12)",           // ZT_ACTION_SWITCH_ABOUT
    "Open Load Song dialog",               // ZT_ACTION_LOAD
    "Save Song",                           // ZT_ACTION_SAVE
    "Save Song As...",                     // ZT_ACTION_SAVE_AS
    "New Song",                            // ZT_ACTION_NEW_SONG
    "Export to .mid file",                 // ZT_ACTION_MIDI_EXPORT
    "Undo",                                // ZT_ACTION_UNDO
    "Replicate selection at cursor",       // ZT_ACTION_REPLICATE_AT_CURSOR
    "Double pattern length",               // ZT_ACTION_DOUBLE_PATTERN
    "Clone pattern to next slot",          // ZT_ACTION_CLONE_PATTERN
    "Octave down",                         // ZT_ACTION_OCTAVE_DOWN
    "Octave up",                           // ZT_ACTION_OCTAVE_UP
    "Edit step = 0",                       // ZT_ACTION_STEP_0
    "Edit step = 1",                       // ZT_ACTION_STEP_1
    "Edit step = 2",                       // ZT_ACTION_STEP_2
    "Edit step = 3",                       // ZT_ACTION_STEP_3
    "Edit step = 4",                       // ZT_ACTION_STEP_4
    "Edit step = 5",                       // ZT_ACTION_STEP_5
    "Edit step = 6",                       // ZT_ACTION_STEP_6
    "Edit step = 7",                       // ZT_ACTION_STEP_7
    "Edit step = 8",                       // ZT_ACTION_STEP_8
    "Edit step = 9",                       // ZT_ACTION_STEP_9
    "Switch to Lua Console",               // ZT_ACTION_SWITCH_LUA_CONSOLE
    "Halve pattern length",                // ZT_ACTION_HALVE_PATTERN
    "Interpolate selection",               // ZT_ACTION_INTERPOLATE_SELECTION
    "Follow playback",                     // ZT_ACTION_FOLLOW_PLAYBACK
    "Track solo",                          // ZT_ACTION_TRACK_SOLO
    "Reverse selection",                   // ZT_ACTION_REVERSE_SELECTION
    "Rotate selection down",               // ZT_ACTION_ROTATE_DOWN
    "Rotate selection up",                 // ZT_ACTION_ROTATE_UP
    "Audition current note (4)",           // ZT_ACTION_NOTE_AUDITION
    "Audition current row (8)",            // ZT_ACTION_ROW_AUDITION
};

const char* KeyBindings::actionName(ZtAction action)
{
    if (action < 0 || action >= ZT_ACTION_COUNT)
        return "unknown";
    return s_actionNames[action];
}

const char* KeyBindings::actionDescription(ZtAction action)
{
    if (action < 0 || action >= ZT_ACTION_COUNT)
        return "(unknown action)";
    return s_actionDescriptions[action];
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
    bindings[ZT_ACTION_QUIT]            = { SDLK_Q,   KS_ALT | KS_CTRL };
    bindings[ZT_ACTION_PLAY_SONG]       = { SDLK_F5,  0 };
    bindings[ZT_ACTION_PLAY_PAT]        = { SDLK_F6,  0 };
    bindings[ZT_ACTION_PLAY_PAT_LINE]   = { SDLK_F7,  0 };
    bindings[ZT_ACTION_STOP]            = { SDLK_F8,  0 };
    bindings[ZT_ACTION_PANIC]           = { SDLK_F9,  0 };
    bindings[ZT_ACTION_HARD_PANIC]      = { SDLK_F9,  KS_SHIFT };
    bindings[ZT_ACTION_SWITCH_HELP]     = { SDLK_F1,  0 };
    bindings[ZT_ACTION_SWITCH_PEDIT]    = { SDLK_F2,  0 };
    bindings[ZT_ACTION_SWITCH_IEDIT]    = { SDLK_F3,  0 };
    bindings[ZT_ACTION_SWITCH_SONGCONF] = { SDLK_F11, 0 };
    bindings[ZT_ACTION_SWITCH_SYSCONF]  = { SDLK_F12, 0 };
    bindings[ZT_ACTION_SWITCH_ABOUT]    = { SDLK_F12, KS_ALT };
    bindings[ZT_ACTION_LOAD]            = { SDLK_L,   KS_CTRL };
    bindings[ZT_ACTION_SAVE]            = { SDLK_S,   KS_CTRL };
    bindings[ZT_ACTION_SAVE_AS]         = { SDLK_S,   KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_NEW_SONG]        = { SDLK_N,   KS_ALT };
    bindings[ZT_ACTION_MIDI_EXPORT]     = { SDLK_M,   KS_CTRL };

    // Pattern editor shortcuts — matches CUI_Patterneditor.cpp
    bindings[ZT_ACTION_UNDO]               = { SDLK_Z,  KS_ALT };   // also KS_META, handled in match()
    bindings[ZT_ACTION_REPLICATE_AT_CURSOR]= { SDLK_R,  KS_ALT };   // ALT+R
    bindings[ZT_ACTION_DOUBLE_PATTERN]     = { SDLK_G,  KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_CLONE_PATTERN]      = { SDLK_D,  KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_OCTAVE_DOWN]        = { SDLK_KP_DIVIDE,   0 };
    bindings[ZT_ACTION_OCTAVE_UP]          = { SDLK_KP_MULTIPLY, 0 };
    bindings[ZT_ACTION_STEP_0]             = { SDLK_0,  KS_ALT };
    bindings[ZT_ACTION_STEP_1]             = { SDLK_1,  KS_ALT };
    bindings[ZT_ACTION_STEP_2]             = { SDLK_2,  KS_ALT };
    bindings[ZT_ACTION_STEP_3]             = { SDLK_3,  KS_ALT };
    bindings[ZT_ACTION_STEP_4]             = { SDLK_4,  KS_ALT };
    bindings[ZT_ACTION_STEP_5]             = { SDLK_5,  KS_ALT };
    bindings[ZT_ACTION_STEP_6]             = { SDLK_6,  KS_ALT };
    bindings[ZT_ACTION_STEP_7]             = { SDLK_7,  KS_ALT };
    bindings[ZT_ACTION_STEP_8]             = { SDLK_8,  KS_ALT };
    bindings[ZT_ACTION_STEP_9]             = { SDLK_9,  KS_ALT };

    // Lua console — Ctrl+Alt+L (avoids collision with Ctrl+L load shortcut)
    bindings[ZT_ACTION_SWITCH_LUA_CONSOLE] = { SDLK_L,  KS_CTRL | KS_ALT };

    // New Paketti/IT features
    bindings[ZT_ACTION_HALVE_PATTERN]        = { SDLK_H,          KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_INTERPOLATE_SELECTION]= { SDLK_I,          KS_CTRL };
    bindings[ZT_ACTION_FOLLOW_PLAYBACK]      = { SDLK_SCROLLLOCK,  0 };
    bindings[ZT_ACTION_TRACK_SOLO]           = { SDLK_F9,          KS_CTRL };
    bindings[ZT_ACTION_REVERSE_SELECTION]    = { SDLK_R,           KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_ROTATE_DOWN]          = { SDLK_DOWN,        KS_CTRL | KS_SHIFT };
    bindings[ZT_ACTION_ROTATE_UP]            = { SDLK_UP,          KS_CTRL | KS_SHIFT };
    // Note column auditioning. The keyboard scancode path in
    // CUI_Patterneditor.cpp still drives the 4 / 8 keys for layout
    // independence, so the entries here are display-only -- they
    // ensure the action shows up in the unified UI with a sensible
    // default key. Manually rebinding here does not currently change
    // the in-pattern behaviour; rebinding is meaningful only via the
    // MIDI mapping column.
    bindings[ZT_ACTION_NOTE_AUDITION]        = { SDLK_4,           0 };
    bindings[ZT_ACTION_ROW_AUDITION]         = { SDLK_8,           0 };
}

// -----------------------------------------------------------------------
// Key name <-> SDLK_* lookup table
// -----------------------------------------------------------------------
struct KeyNameEntry { const char* name; int sdlk; };

static const KeyNameEntry s_keyNames[] = {
    { "a", SDLK_A }, { "b", SDLK_B }, { "c", SDLK_C }, { "d", SDLK_D },
    { "e", SDLK_E }, { "f", SDLK_F }, { "g", SDLK_G }, { "h", SDLK_H },
    { "i", SDLK_I }, { "j", SDLK_J }, { "k", SDLK_K }, { "l", SDLK_L },
    { "m", SDLK_M }, { "n", SDLK_N }, { "o", SDLK_O }, { "p", SDLK_P },
    { "q", SDLK_Q }, { "r", SDLK_R }, { "s", SDLK_S }, { "t", SDLK_T },
    { "u", SDLK_U }, { "v", SDLK_V }, { "w", SDLK_W }, { "x", SDLK_X },
    { "y", SDLK_Y }, { "z", SDLK_Z },
    { "0", SDLK_0 }, { "1", SDLK_1 }, { "2", SDLK_2 }, { "3", SDLK_3 },
    { "4", SDLK_4 }, { "5", SDLK_5 }, { "6", SDLK_6 }, { "7", SDLK_7 },
    { "8", SDLK_8 }, { "9", SDLK_9 },
    { "f1",  SDLK_F1  }, { "f2",  SDLK_F2  }, { "f3",  SDLK_F3  }, { "f4",  SDLK_F4  },
    { "f5",  SDLK_F5  }, { "f6",  SDLK_F6  }, { "f7",  SDLK_F7  }, { "f8",  SDLK_F8  },
    { "f9",  SDLK_F9  }, { "f10", SDLK_F10 }, { "f11", SDLK_F11 }, { "f12", SDLK_F12 },
    { "escape",    SDLK_ESCAPE    },
    { "return",    SDLK_RETURN    }, { "enter",     SDLK_RETURN    },
    { "space",     SDLK_SPACE     },
    { "tab",       SDLK_TAB       },
    { "back",      SDLK_BACKSPACE }, { "backspace", SDLK_BACKSPACE },
    { "delete",    SDLK_DELETE    },
    { "insert",    SDLK_INSERT    },
    { "home",      SDLK_HOME      },
    { "end",       SDLK_END       },
    { "pgup",      SDLK_PAGEUP    }, { "prior",     SDLK_PAGEUP    },
    { "pgdn",      SDLK_PAGEDOWN  }, { "next",      SDLK_PAGEDOWN  },
    { "up",        SDLK_UP        },
    { "down",      SDLK_DOWN      },
    { "left",      SDLK_LEFT      },
    { "right",     SDLK_RIGHT     },
    { "minus",     SDLK_MINUS     },
    { "equals",    SDLK_EQUALS    },
    { "comma",     SDLK_COMMA     },
    { "period",    SDLK_PERIOD    },
    { "slash",     SDLK_SLASH     },
    { "divide",    SDLK_KP_DIVIDE   },   // numpad /
    { "multiply",  SDLK_KP_MULTIPLY },   // numpad *
    { "subtract",  SDLK_KP_MINUS    },   // numpad -
    { "add",       SDLK_KP_PLUS     },   // numpad +
    { "grave",     SDLK_GRAVE       },
    { "scroll",    SDLK_SCROLLLOCK  },
    { NULL, 0 }
};

static int sdlkFromName(const char* name)
{
    for (int i = 0; s_keyNames[i].name; i++) {
        if (strcmp(s_keyNames[i].name, name) == 0)
            return s_keyNames[i].sdlk;
    }
    return 0;
}

static const char* nameFromSdlk(int sdlk)
{
    for (int i = 0; s_keyNames[i].name; i++) {
        if (s_keyNames[i].sdlk == sdlk)
            return s_keyNames[i].name;
    }
    return NULL;
}

// -----------------------------------------------------------------------
// parseKeyCombo — "ctrl+alt+q" -> {key=SDLK_Q, modifiers=KS_CTRL|KS_ALT}
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
    key = sdlkFromName(keystr);
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
// formatKeyCombo — {key=SDLK_Q, modifiers=KS_CTRL|KS_ALT} -> "ctrl+alt+q"
// -----------------------------------------------------------------------
void KeyBindings::formatKeyCombo(int key, int modifiers, char* buf, int bufsize)
{
    buf[0] = '\0';
    if (key == 0) { snprintf(buf, bufsize, "none"); return; }

    char tmp[64] = "";
    if (modifiers & KS_CTRL)  { strncat(tmp, "ctrl+",  sizeof(tmp)-strlen(tmp)-1); }
    if (modifiers & KS_ALT)   { strncat(tmp, "alt+",   sizeof(tmp)-strlen(tmp)-1); }
    if (modifiers & KS_SHIFT) { strncat(tmp, "shift+", sizeof(tmp)-strlen(tmp)-1); }
    if (KS_META && (modifiers & KS_META)) { strncat(tmp, "meta+",  sizeof(tmp)-strlen(tmp)-1); }

    const char* kname = nameFromSdlk(key);
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
        // (KS_META may be 0 in SDL3 builds where the meta path is unused)
        if (a == ZT_ACTION_UNDO && KS_META != 0) {
            if ((b.modifiers & KS_ALT) &&
                ((kstate & KS_ALT) || (kstate & KS_META)) &&
                !(kstate & ~(KS_ALT | KS_META))) {
                return ZT_ACTION_UNDO;
            }
        }
    }
    return ZT_ACTION_NONE;
}
