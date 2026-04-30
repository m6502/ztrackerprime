/*************************************************************************
 *
 * FILE  zt_headless.cpp
 *
 * Scripted, windowless operation. See zt_headless.h for the CLI surface.
 *
 ******/

#include "zt_headless.h"

#include <SDL.h>
#include <png.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "keybuffer.h"

// Pulled from main.cpp -- the global key ring and the "user requested
// quit" flag. We don't pull main.cpp's full surface in here; just the
// two symbols the script driver needs to talk to.
extern KeyBuffer Keys;
extern int       do_exit;

// ---------------------------------------------------------------------------
// Module-local state.

static bool        g_active           = false;
static FILE       *g_script_fp        = NULL;
static char        g_script_path[1024] = {0};
static int         g_script_lineno    = 0;
static bool        g_script_done      = false;

// `wait` defers the next command by N milliseconds. We measure with
// SDL_GetTicks so it tracks wall time, not frame count -- robust against
// frame-rate changes.
static Uint64      g_wait_until_ms    = 0;

// ---------------------------------------------------------------------------
// CLI plumbing.

void zt_headless_init(bool active, const char *script_path) {
    g_active = active;
    if (script_path && *script_path) {
        snprintf(g_script_path, sizeof(g_script_path), "%s", script_path);
        g_script_fp = fopen(g_script_path, "r");
        if (!g_script_fp) {
            fprintf(stderr, "zt: --script: failed to open '%s': %s\n",
                    g_script_path, strerror(errno));
            // No script => behave as if `quit` was the first command, so
            // a misconfigured CI run exits fast instead of hanging on a
            // black window.
            g_script_done = true;
            do_exit = 1;
        }
    }
}

bool zt_headless_active(void) {
    return g_active;
}

// ---------------------------------------------------------------------------
// Key-name lookup. Maps the textual names we accept in `key <name>`
// commands to SDL keysym constants. Modifier prefixes (`shift+`, `ctrl+`,
// `alt+`, `meta+`) are stripped by the caller before we look up the bare
// key name.

struct KeyName { const char *name; KBKey key; };

static const KeyName g_key_names[] = {
    // Function keys.
    {"f1", SDLK_F1}, {"f2", SDLK_F2}, {"f3", SDLK_F3}, {"f4", SDLK_F4},
    {"f5", SDLK_F5}, {"f6", SDLK_F6}, {"f7", SDLK_F7}, {"f8", SDLK_F8},
    {"f9", SDLK_F9}, {"f10", SDLK_F10}, {"f11", SDLK_F11}, {"f12", SDLK_F12},
    // Navigation.
    {"up", SDLK_UP}, {"down", SDLK_DOWN}, {"left", SDLK_LEFT}, {"right", SDLK_RIGHT},
    {"home", SDLK_HOME}, {"end", SDLK_END},
    {"pageup", SDLK_PAGEUP}, {"pagedown", SDLK_PAGEDOWN},
    // Editing.
    {"tab", SDLK_TAB}, {"return", SDLK_RETURN}, {"enter", SDLK_RETURN},
    {"space", SDLK_SPACE}, {"esc", SDLK_ESCAPE}, {"escape", SDLK_ESCAPE},
    {"backspace", SDLK_BACKSPACE}, {"delete", SDLK_DELETE},
    // Punctuation that crops up in keybindings.
    {"grave", SDLK_GRAVE},          // The § key on Finnish ISO.
    {"leftbracket", SDLK_LEFTBRACKET}, {"rightbracket", SDLK_RIGHTBRACKET},
    // Letters and digits resolved by direct lowercasing below.
};

static KBKey resolve_key_name(const char *name) {
    if (!name || !*name) return 0;
    // Single character: digit or letter -> SDLK_a .. SDLK_z / SDLK_0 ..
    // SDLK_9. The SDLK_* values for ASCII letters/digits coincide with
    // the lowercase ASCII codepoint, so we can return the byte directly.
    if (name[1] == '\0') {
        char c = (char)tolower((unsigned char)name[0]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            return (KBKey)c;
        }
    }
    char buf[32];
    size_t i = 0;
    for (; name[i] && i + 1 < sizeof(buf); i++) {
        buf[i] = (char)tolower((unsigned char)name[i]);
    }
    buf[i] = '\0';
    for (size_t j = 0; j < sizeof(g_key_names)/sizeof(g_key_names[0]); j++) {
        if (strcmp(buf, g_key_names[j].name) == 0) return g_key_names[j].key;
    }
    return 0;
}

// Parse a `[mods+]key` token. `mods` is one or more of shift/ctrl/alt/meta
// joined by `+`. Returns 1 on success and writes *key_out / *mod_out.
static int parse_keychord(const char *tok, KBKey *key_out, KBMod *mod_out) {
    KBMod mods = SDL_KMOD_NONE;
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", tok);
    char *p = buf;
    for (;;) {
        char *plus = strchr(p, '+');
        if (!plus) break;
        *plus = '\0';
        // Lowercase the modifier word in-place.
        for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);
        if      (!strcmp(p, "shift")) mods |= SDL_KMOD_SHIFT;
        else if (!strcmp(p, "ctrl"))  mods |= SDL_KMOD_CTRL;
        else if (!strcmp(p, "alt"))   mods |= SDL_KMOD_ALT;
        else if (!strcmp(p, "meta") || !strcmp(p, "cmd")) mods |= SDL_KMOD_GUI;
        else {
            fprintf(stderr, "zt: --script: unknown modifier '%s' on line %d\n",
                    p, g_script_lineno);
            return 0;
        }
        p = plus + 1;
    }
    KBKey k = resolve_key_name(p);
    if (!k) {
        fprintf(stderr, "zt: --script: unknown key '%s' on line %d\n",
                p, g_script_lineno);
        return 0;
    }
    *key_out = k;
    *mod_out = mods;
    return 1;
}

// ---------------------------------------------------------------------------
// PNG dump. SDL_PIXELFORMAT_ARGB8888 on little-endian is laid out in memory
// as B,G,R,A bytes -- libpng's PNG_FORMAT_BGRA matches that exactly, so we
// don't need a per-pixel byte-swap.

int zt_headless_dump_png(SDL_Surface *surface, const char *path) {
    if (!surface || !path || !*path) return 0;

    SDL_LockSurface(surface);
    int w = surface->w;
    int h = surface->h;
    int pitch = surface->pitch;
    void *pixels = surface->pixels;

    png_image img;
    memset(&img, 0, sizeof(img));
    img.version = PNG_IMAGE_VERSION;
    img.width   = (png_uint_32)w;
    img.height  = (png_uint_32)h;
    img.format  = PNG_FORMAT_BGRA;     // matches little-endian ARGB8888.

    int rc = png_image_write_to_file(&img, path,
                                     0 /* convert_to_8bit */,
                                     pixels, pitch,
                                     NULL /* colormap */);
    if (!rc) {
        fprintf(stderr, "zt: --script: png write '%s' failed: %s\n",
                path, img.message);
    }
    SDL_UnlockSurface(surface);
    return rc;
}

// ---------------------------------------------------------------------------
// Script driver.

static void exec_command(SDL_Surface *frame_surface, const char *cmd, char *args) {
    // `key <chord>`
    if (!strcmp(cmd, "key")) {
        // Trim leading whitespace.
        while (*args && isspace((unsigned char)*args)) args++;
        char *end = args + strlen(args);
        while (end > args && isspace((unsigned char)end[-1])) *--end = '\0';
        if (!*args) {
            fprintf(stderr, "zt: --script: 'key' needs an argument on line %d\n",
                    g_script_lineno);
            return;
        }
        KBKey k = 0;
        KBMod m = 0;
        if (!parse_keychord(args, &k, &m)) return;
        Keys.insert(k, m, 0, 0);
        return;
    }
    // `wait <ms>`
    if (!strcmp(cmd, "wait")) {
        long ms = strtol(args, NULL, 10);
        if (ms < 0) ms = 0;
        if (ms > 60000) ms = 60000;     // 1 min hard cap (CI safety).
        g_wait_until_ms = SDL_GetTicks() + (Uint64)ms;
        return;
    }
    // `shot <path>`
    if (!strcmp(cmd, "shot")) {
        while (*args && isspace((unsigned char)*args)) args++;
        char *end = args + strlen(args);
        while (end > args && isspace((unsigned char)end[-1])) *--end = '\0';
        if (!*args) {
            fprintf(stderr, "zt: --script: 'shot' needs a path on line %d\n",
                    g_script_lineno);
            return;
        }
        zt_headless_dump_png(frame_surface, args);
        return;
    }
    // `quit`
    if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit")) {
        do_exit = 1;
        g_script_done = true;
        return;
    }
    fprintf(stderr, "zt: --script: unknown command '%s' on line %d\n",
            cmd, g_script_lineno);
}

void zt_headless_pump(SDL_Surface *frame_surface) {
    if (!g_active || g_script_done || !g_script_fp) return;
    if (g_wait_until_ms && SDL_GetTicks() < g_wait_until_ms) return;
    g_wait_until_ms = 0;

    // Run commands until we either hit a `wait` (which sets g_wait_until_ms
    // and returns next frame), the script ends, or a `quit` fires. This
    // means a script of N immediate `key` commands fires them all in one
    // pump call -- which is fine, the page sees them in queue order.
    char line[1024];
    while (fgets(line, sizeof(line), g_script_fp)) {
        g_script_lineno++;
        // Strip trailing newline + CR.
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        // Skip blanks and comments.
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;
        // Split into command + args.
        char *args = p;
        while (*args && !isspace((unsigned char)*args)) args++;
        if (*args) { *args = '\0'; args++; }
        // Lowercase the command word.
        for (char *q = p; *q; q++) *q = (char)tolower((unsigned char)*q);

        exec_command(frame_surface, p, args);

        if (g_wait_until_ms || g_script_done) return;
    }

    // EOF: stop driving the loop. The main loop continues to render
    // until something else (do_exit / SDL quit) ends it. CI scripts
    // should always end with `quit`.
    g_script_done = true;
    fclose(g_script_fp);
    g_script_fp = NULL;
}
