// CUI_LuaConsole.cpp — Lua REPL console page for zTracker
//
// Layout (top to bottom):
//   row PAGE_TITLE_ROW_Y              title bar ("Lua Console")
//   row TRACKS_ROW_Y .. sep_y-1       scrollback (history of output)
//   row sep_y                         horizontal separator
//   row inp_y                         input line with ">" prompt + cursor
//   (rows below inp_y are covered by the skin's toolbar strip)
//
// Drawable::fillRect takes (x1, y1, x2, y2, color) — two corner points,
// inclusive on the low side, inclusive on the high side (see
// lc_sdl_wrapper.cpp:93). NOT (x, y, w, h).

#include "zt.h"
#include "lua_engine.h"
#include "lc_sdl_wrapper.h"
#include "keybuffer.h"
#include "font.h"

// ---------------------------------------------------------------------------
// Local state
// ---------------------------------------------------------------------------
static char s_input[256] = "";
static int  s_cursor     = 0;
static int  s_welcomed   = 0;

// ---------------------------------------------------------------------------
// Layout (all values are character rows/cols; convert with row()/col())
// ---------------------------------------------------------------------------
static int frame_x1() { return 1; }
static int frame_x2() { return CHARS_X - 2; }

// Reserve 8 rows at the bottom for the skin toolbar strip (55 px / 8 ≈ 7).
static int bottom_margin_rows() { return 8; }

static int sep_row()   { return CHARS_Y - bottom_margin_rows() - 1; }
static int input_row() { return sep_row() + 1; }

static int scrollback_top()    { return TRACKS_ROW_Y; }
static int scrollback_bottom() { return sep_row() - 1; }
static int scrollback_rows()
{
    int n = scrollback_bottom() - scrollback_top() + 1;
    return (n > 0) ? n : 1;
}

// Pixel corners of the console frame (inclusive)
static int frame_px1() { return col(frame_x1()); }
static int frame_px2() { return col(frame_x2() + 1) - 1; }

// ---------------------------------------------------------------------------
// CUI_LuaConsole constructor / destructor
// ---------------------------------------------------------------------------
CUI_LuaConsole::CUI_LuaConsole()  { UI = nullptr; }
CUI_LuaConsole::~CUI_LuaConsole() {}

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

    if (!s_welcomed) {
        g_lua.print_line("zTracker Lua Console — Lua 5.4");
        g_lua.print_line("Type Lua code, press Enter to execute.");
        g_lua.print_line("Up/Down = history   PgUp/PgDn = scroll   Esc = exit");
        g_lua.print_line("Try:  print('hello')   or   for i=1,3 do print(i) end");
        g_lua.print_line("");
        s_welcomed = 1;
    }

    zt_text_input_start();
}

void CUI_LuaConsole::leave()
{
    zt_text_input_stop();
}

// ---------------------------------------------------------------------------
// update — keyboard input
// ---------------------------------------------------------------------------
void CUI_LuaConsole::update()
{
    if (!Keys.size()) return;

    KBKey key    = Keys.getkey();
    KBKey kstate = Keys.getstate();
    (void)kstate;

    int page = scrollback_rows();

    switch (key) {
    case SDLK_RETURN:
        if (s_input[0]) {
            g_lua.execute(s_input);
            s_input[0] = '\0';
            s_cursor   = 0;
            g_lua.scroll_off = 0;
        }
        break;

    case SDLK_ESCAPE:
    case SDLK_F2:
        switch_page(UIP_Patterneditor);
        break;

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

    case SDLK_PAGEUP:
        g_lua.scroll_off += page;
        if (g_lua.scroll_off > g_lua.line_count - 1)
            g_lua.scroll_off = (g_lua.line_count > 0) ? g_lua.line_count - 1 : 0;
        break;

    case SDLK_PAGEDOWN:
        g_lua.scroll_off -= page;
        if (g_lua.scroll_off < 0) g_lua.scroll_off = 0;
        break;

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

    const TColor page_bg    = COLORS.Background;
    const TColor console_bg = COLORS.EditBG;
    const TColor console_fg = COLORS.EditText;
    const TColor prompt_fg  = COLORS.Data;
    const TColor input_bg   = COLORS.EditBGhigh;
    const TColor title_fg   = COLORS.Text;
    const TColor accent     = COLORS.Highlight;

    const int fx1 = frame_px1();
    const int fx2 = frame_px2();

    // --- Clear the whole console area first ---
    S->fillRect(fx1, row(PAGE_TITLE_ROW_Y),
                fx2, row(input_row() + 1) - 1, page_bg);

    // --- Title ---
    printtitle(PAGE_TITLE_ROW_Y, "Lua Console (Esc/F2 to close)",
               title_fg, page_bg, S);

    // --- Scrollback background (distinct shade, so the console area is
    //     visually separate from the empty page background) ---
    const int sb_top = scrollback_top();
    const int sb_rows = scrollback_rows();
    S->fillRect(fx1, row(sb_top),
                fx2, row(sb_top + sb_rows) - 1, console_bg);

    // --- Scrollback lines (newest at bottom) ---
    const int bottom_idx = g_lua.line_count - g_lua.scroll_off;
    const int top_idx    = bottom_idx - sb_rows;
    for (int i = 0; i < sb_rows; i++) {
        int line_idx = top_idx + i;
        if (line_idx >= 0 && line_idx < g_lua.line_count) {
            int ring = line_idx % LUA_CONSOLE_MAX_LINES;
            const char *text = g_lua.lines[ring];
            if (text && text[0]) {
                printBG(col(frame_x1() + 1), row(sb_top + i),
                        text, console_fg, console_bg, S);
            }
        }
    }

    // --- Scroll indicator ---
    if (g_lua.scroll_off > 0) {
        char ind[32];
        snprintf(ind, sizeof(ind), " [-%d] ", g_lua.scroll_off);
        int ind_len = (int)strlen(ind);
        printBG(col(frame_x2() - ind_len), row(sb_top),
                ind, prompt_fg, console_bg, S);
    }

    // --- Separator (just above input line) ---
    printlineBG(col(frame_x1()), row(sep_row()), 0x81,
                frame_x2() - frame_x1() + 1, accent, page_bg, S);

    // --- Input line ---
    const int iy = input_row();
    S->fillRect(fx1, row(iy), fx2, row(iy + 1) - 1, input_bg);

    printcharBG(col(frame_x1()    ), row(iy), '>', prompt_fg, input_bg, S);
    printcharBG(col(frame_x1() + 1), row(iy), ' ', prompt_fg, input_bg, S);

    const int text_x1   = frame_x1() + 2;
    const int max_chars = frame_x2() - text_x1;
    const int len       = (int)strlen(s_input);
    int view_off = 0;
    if (s_cursor >= max_chars) view_off = s_cursor - max_chars + 1;
    if (view_off < 0)   view_off = 0;
    if (view_off > len) view_off = len;
    char view[512];
    {
        int n = len - view_off;
        if (n > max_chars) n = max_chars;
        if (n > (int)sizeof(view) - 1) n = (int)sizeof(view) - 1;
        memcpy(view, s_input + view_off, (size_t)n);
        view[n] = '\0';
    }
    printBG(col(text_x1), row(iy), view, console_fg, input_bg, S);

    // Cursor block (inverse)
    int cur_col = text_x1 + (s_cursor - view_off);
    if (cur_col < text_x1)      cur_col = text_x1;
    if (cur_col > frame_x2())   cur_col = frame_x2();
    unsigned char cur_ch = (s_cursor < len) ? (unsigned char)s_input[s_cursor] : ' ';
    printcharBG(col(cur_col), row(iy), cur_ch, input_bg, prompt_fg, S);

    // Top-of-screen info (Song Name etc.) last so it wins.
    draw_status(S);

    need_refresh = 0;
    updated = 2;

    S->unlock();

    // Tell screenmanager the frame is dirty so the backbuffer actually
    // gets blitted to the SDL texture.
    screenmanager.UpdateAll();
}
