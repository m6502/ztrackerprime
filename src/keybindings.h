#ifndef _KEYBINDINGS_H
#define _KEYBINDINGS_H

// Forward-declare conf so we don't pull in all of zt.h here.
class conf;

// -----------------------------------------------------------------------
// Action IDs for all remappable shortcuts
// -----------------------------------------------------------------------
enum ZtAction {
    ZT_ACTION_NONE = 0,

    // Global shortcuts (main.cpp global_keys)
    ZT_ACTION_QUIT,
    ZT_ACTION_PLAY_SONG,
    ZT_ACTION_PLAY_PAT,
    ZT_ACTION_PLAY_PAT_LINE,
    ZT_ACTION_STOP,
    ZT_ACTION_PANIC,
    ZT_ACTION_HARD_PANIC,
    ZT_ACTION_SWITCH_HELP,
    ZT_ACTION_SWITCH_PEDIT,
    ZT_ACTION_SWITCH_IEDIT,
    ZT_ACTION_SWITCH_SONGCONF,
    ZT_ACTION_SWITCH_SYSCONF,
    ZT_ACTION_SWITCH_ABOUT,
    ZT_ACTION_LOAD,
    ZT_ACTION_SAVE,
    ZT_ACTION_SAVE_AS,
    ZT_ACTION_NEW_SONG,
    ZT_ACTION_MIDI_EXPORT,

    // Pattern editor shortcuts (CUI_Patterneditor.cpp)
    ZT_ACTION_UNDO,
    ZT_ACTION_REPLICATE_AT_CURSOR,
    ZT_ACTION_DOUBLE_PATTERN,
    ZT_ACTION_CLONE_PATTERN,
    ZT_ACTION_OCTAVE_DOWN,
    ZT_ACTION_OCTAVE_UP,
    ZT_ACTION_STEP_0,
    ZT_ACTION_STEP_1,
    ZT_ACTION_STEP_2,
    ZT_ACTION_STEP_3,
    ZT_ACTION_STEP_4,
    ZT_ACTION_STEP_5,
    ZT_ACTION_STEP_6,
    ZT_ACTION_STEP_7,
    ZT_ACTION_STEP_8,
    ZT_ACTION_STEP_9,

    ZT_ACTION_SWITCH_LUA_CONSOLE,

    // New Paketti/IT features
    ZT_ACTION_HALVE_PATTERN,
    ZT_ACTION_INTERPOLATE_SELECTION,
    ZT_ACTION_FOLLOW_PLAYBACK,
    ZT_ACTION_TRACK_SOLO,
    ZT_ACTION_REVERSE_SELECTION,
    ZT_ACTION_ROTATE_DOWN,
    ZT_ACTION_ROTATE_UP,

    // Note column auditioning -- the keyboard path is hard-wired to
    // SDL_SCANCODE_4 / _8 in CUI_Patterneditor.cpp, but listing the
    // actions here lets them appear in the unified Shortcuts & MIDI
    // page and gives the MIDI mapping system a stable index to fire.
    ZT_ACTION_NOTE_AUDITION,
    ZT_ACTION_ROW_AUDITION,

    ZT_ACTION_COUNT
};

// -----------------------------------------------------------------------
// A single key + modifier binding
// -----------------------------------------------------------------------
struct KeyBinding {
    int key;        // SDLK_* key code  (0 = unbound)
    int modifiers;  // KS_ALT | KS_CTRL | KS_SHIFT bitmask
};

// -----------------------------------------------------------------------
// The full keybinding table
// -----------------------------------------------------------------------
class KeyBindings {
public:
    KeyBinding bindings[ZT_ACTION_COUNT];

    // Set all bindings to the current hard-coded defaults.
    void setDefaults();

    // Load overrides from zt.conf (entries like "key_quit: ctrl+alt+q").
    // Missing entries keep the default value.
    void load(conf *Config);

    // Save all bindings to zt.conf.
    void save(conf *Config);

    // Return the action that matches key+kstate, or ZT_ACTION_NONE.
    ZtAction match(int key, int kstate) const;

    // --- Helpers ---

    // Human-readable name used in zt.conf, e.g. "quit", "play_song".
    static const char* actionName(ZtAction action);

    // English description shown in the Shortcuts & MIDI Mappings UI,
    // e.g. "Quit zTracker", "Play song from start". The conf-key form
    // is kept because zt.conf is hand-editable, but the UI never has
    // to expose snake_case to the user.
    static const char* actionDescription(ZtAction action);

    // Conf key for an action, e.g. "key_quit".
    static void actionConfKey(ZtAction action, char* buf, int bufsize);

    // Parse "ctrl+alt+q" -> key + modifiers.  Returns false on failure.
    static bool parseKeyCombo(const char* str, int &key, int &modifiers);

    // Format key + modifiers back to e.g. "ctrl+alt+q".
    static void formatKeyCombo(int key, int modifiers, char* buf, int bufsize);
};

// Global instance — defined in keybindings.cpp.
extern KeyBindings g_keybindings;

#endif // _KEYBINDINGS_H
