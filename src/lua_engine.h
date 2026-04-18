// lua_engine.h — Lua 5.4 scripting engine for zTracker
#ifndef ZT_LUA_ENGINE_H
#define ZT_LUA_ENGINE_H

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

// Maximum lines kept in the console scrollback
#define LUA_CONSOLE_MAX_LINES 256
// Maximum characters per scrollback line
#define LUA_CONSOLE_LINE_LEN  256
// Maximum history entries for the input line
#define LUA_CONSOLE_HISTORY   64

class ZtLuaEngine {
public:
    lua_State *L;

    // Scrollback buffer — ring buffer of text lines
    char lines[LUA_CONSOLE_MAX_LINES][LUA_CONSOLE_LINE_LEN];
    int  line_count;   // total lines ever appended (mod gives ring index)
    int  scroll_off;   // lines scrolled up from bottom (0 = at bottom)

    // Input history
    char history[LUA_CONSOLE_HISTORY][256];
    int  history_count;
    int  history_pos;  // -1 = not browsing

    ZtLuaEngine();
    ~ZtLuaEngine();

    bool init();
    void shutdown();

    // Execute a string of Lua code; output/errors go to scrollback
    void execute(const char *code);

    // Append a line of text to the scrollback
    void print_line(const char *text);

    // Print the full API (all zt.* bindings + selected globals) to scrollback.
    void print_api_list();

    // Tab completion.
    //
    // Given `input` of length `len` with the cursor at byte `cursor`, try to
    // complete the identifier ending at the cursor. If a unique completion is
    // found, `input` and `*cursor` are rewritten in place (caller guarantees
    // at least `cap` bytes in `input`). If multiple candidates exist, the
    // common prefix is applied and the candidate list is printed to the
    // scrollback. Returns true if `input`/`*cursor` were modified.
    bool tab_complete(char *input, int cap, int *cursor);

private:
    void registerBindings();
};

extern ZtLuaEngine g_lua;

#endif // ZT_LUA_ENGINE_H
