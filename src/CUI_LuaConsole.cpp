// CUI_LuaConsole.cpp — Lua REPL console page for zTracker
//
// Layout:
//   - Scrollback area fills most of the screen (TRACKS_ROW_Y .. CHARS_Y-3)
//   - A single-line text input at the bottom row
//   - Title bar at PAGE_TITLE_ROW_Y
//
// Keys:
//   Enter      — execute current input line
//   Escape/F2  — return to pattern editor
//   Up/Down    — browse input history / scroll scrollback
//   PgUp/PgDn  — scroll scrollback by page
//   Backspace  — delete character to left
//   Delete     — clear input line

#include "zt.h"
#include "lua_engine.h"
#include "lc_sdl_wrapper.h"
#include "keybuffer.h"
#include "font.h"

// ---------------------------------------------------------------------------
// Local state (kept outside the class to avoid header pollution)
// ---------------------------------------------------------------------------
static char   s_input[256]    = "";  // current input buffer
static int    s_cursor        = 0;   // cursor position in s_input

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Number of visible rows in the scrollback area
static int console_rows()
{
    return CHARS_Y - TRACKS_ROW_Y - 3; // leave 2 rows for input + border
}

// Draw a single text row at character position (cx, cy) with given colors
static void draw_text_row(const char *text, int cx, int cy,
                           TColor fg, TColor bg, Drawable *S)
{
    // Clear the row first
    int px = col(cx);
    int py = row(cy);
    int pw = (CHARS_X - 2) * CHARACTER_SIZE_X;
    S->fillRect(px, py, pw, CHARACTER_SIZE_Y, bg);
    if (text && text[0])
        printBG(col(cx), row(cy), text, fg, bg, S);
}

// ---------------------------------------------------------------------------
// CUI_LuaConsole constructor / destructor
// ---------------------------------------------------------------------------

CUI_LuaConsole::CUI_LuaConsole()
{
    UI = nullptr;  // No widget system needed
}

CUI_LuaConsole::~CUI_LuaConsole()
{
}

// ---------------------------------------------------------------------------
// enter / leave
// ---------------------------------------------------------------------------

void CUI_LuaConsole::enter()
{
    s_input[0] = '\0';
    s_cursor = 0;
    g_lua.scroll_off = 0;
    need_refresh++;
    cur_state = STATE_LUA_CONSOLE;
}

void CUI_LuaConsole::leave()
{
}

// ---------------------------------------------------------------------------
// update — handle keyboard input
// ---------------------------------------------------------------------------

void CUI_LuaConsole::update()
{
    if (!Keys.size()) return;

    KBKey key   = Keys.getkey();
    KBKey kstate= Keys.getstate();
    (void)kstate;

    int rows = console_rows();

    switch (key) {
    // -- Execute ----------------------------------------------------------------
    case SDLK_RETURN:
        if (s_input[0]) {
            g_lua.execute(s_input);
            s_input[0] = '\0';
            s_cursor   = 0;
            g_lua.scroll_off = 0;  // snap back to bottom
        }
        break;

    // -- Leave page -------------------------------------------------------------
    case SDLK_ESCAPE:
    case SDLK_F2:
        switch_page(UIP_Patterneditor);
        break;

    // -- History navigation (Up/Down) -------------------------------------------
    case SDLK_UP:
        if (g_lua.history_count > 0) {
            if (g_lua.history_pos < 0)
                g_lua.history_pos = g_lua.history_count - 1;
            else if (g_lua.history_pos > 0)
                g_lua.history_pos--;
            int hi = g_lua.history_pos % LUA_CONSOLE_HISTORY;
            strncpy(s_input, g_lua.history[hi], 255);
            s_input[255] = '\0';
            s_cursor = (int)strlen(s_input);
        }
        break;

    case SDLK_DOWN:
        if (g_lua.history_pos >= 0) {
            g_lua.history_pos++;
            if (g_lua.history_pos >= g_lua.history_count) {
                g_lua.history_pos = -1;
                s_input[0] = '\0';
                s_cursor   = 0;
            } else {
                int hi = g_lua.history_pos % LUA_CONSOLE_HISTORY;
                strncpy(s_input, g_lua.history[hi], 255);
                s_input[255] = '\0';
                s_cursor = (int)strlen(s_input);
            }
        }
        break;

    // -- Scroll scrollback ------------------------------------------------------
    case SDLK_PAGEUP:
        g_lua.scroll_off += rows;
        if (g_lua.scroll_off > g_lua.line_count - 1)
            g_lua.scroll_off = (g_lua.line_count > 0) ? g_lua.line_count - 1 : 0;
        break;

    case SDLK_PAGEDOWN:
        g_lua.scroll_off -= rows;
        if (g_lua.scroll_off < 0) g_lua.scroll_off = 0;
        break;

    // -- Text editing -----------------------------------------------------------
    case SDLK_BACKSPACE:
        if (s_cursor > 0) {
            int len = (int)strlen(s_input);
            memmove(&s_input[s_cursor - 1], &s_input[s_cursor],
                    (size_t)(len - s_cursor + 1));
            s_cursor--;
        }
        break;

    case SDLK_DELETE:
        s_input[0] = '\0';
        s_cursor   = 0;
        break;

    case SDLK_LEFT:
        if (s_cursor > 0) s_cursor--;
        break;

    case SDLK_RIGHT: {
        int len = (int)strlen(s_input);
        if (s_cursor < len) s_cursor++;
        break;
    }

    case SDLK_HOME:
        s_cursor = 0;
        break;

    case SDLK_END:
        s_cursor = (int)strlen(s_input);
        break;

    default: {
        // Use the actual_char field set by the SDL event handler
        unsigned char ch = Keys.getactualchar();
        if (ch >= 0x20 && ch < 0x7F) {
            int len = (int)strlen(s_input);
            if (len < 254) {
                memmove(&s_input[s_cursor + 1], &s_input[s_cursor],
                        (size_t)(len - s_cursor + 1));
                s_input[s_cursor] = (char)ch;
                s_cursor++;
            }
        }
        break;
    }
    }

    need_refresh++;
    updated = 2;
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------

void CUI_LuaConsole::draw(Drawable *S)
{
    if (S->lock() != 0) return;

    TColor fg   = COLORS.EditText;
    TColor bg   = COLORS.EditBG;
    TColor hfg  = COLORS.Data;      // highlight (prompt text)
    TColor ibg  = COLORS.EditBGhigh; // input row bg

    // Title
    printtitle(PAGE_TITLE_ROW_Y, "Lua Console (Esc/F2 to close)", COLORS.Text, COLORS.Background, S);

    int rows    = console_rows();
    int start_y = TRACKS_ROW_Y;

    // --- Scrollback area ---
    // We render from (line_count - scroll_off - rows) upward
    int bottom = g_lua.line_count - g_lua.scroll_off;  // index of first line below view
    int top    = bottom - rows;

    for (int i = 0; i < rows; i++) {
        int line_idx = top + i;
        const char *text = "";
        if (line_idx >= 0 && line_idx < g_lua.line_count) {
            int ring = line_idx % LUA_CONSOLE_MAX_LINES;
            text = g_lua.lines[ring];
        }
        int cy = start_y + i;
        draw_text_row(text, 1, cy, fg, bg, S);
    }

    // --- Separator line ---
    int sep_y = start_y + rows;
    S->fillRect(col(1), row(sep_y), (CHARS_X - 2) * CHARACTER_SIZE_X, CHARACTER_SIZE_Y, COLORS.Lowlight);

    // --- Input line ---
    int inp_y = sep_y + 1;
    S->fillRect(col(1), row(inp_y), (CHARS_X - 2) * CHARACTER_SIZE_X, CHARACTER_SIZE_Y, ibg);

    // Draw prompt ">"
    printcharBG(col(1), row(inp_y), '>', hfg, ibg, S);
    // Draw input text
    printBG(col(2), row(inp_y), s_input, fg, ibg, S);

    // Draw cursor block
    int cur_x = 2 + s_cursor;
    unsigned char cur_ch = s_input[s_cursor] ? (unsigned char)s_input[s_cursor] : ' ';
    printcharBG(col(cur_x), row(inp_y), cur_ch, ibg, hfg, S);

    draw_status(S);

    need_refresh = 0;
    updated = 2;

    S->unlock();
}
