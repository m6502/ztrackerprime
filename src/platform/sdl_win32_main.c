/*
 * Replacement SDLmain for MinGW-w64 cross-compile.
 *
 * The pre-built libSDLmain.a from the SDL 1.2.15 dev pack was compiled with
 * an older toolchain that references _imp___iob (old MSVC stdio). MinGW-w64
 * GCC 15 doesn't provide that symbol. Instead of shimming it, we just provide
 * our own WinMain that does what SDL_win32_main.c does: calls SDL_main().
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

/* SDL_main.h redefines main → SDL_main via a macro. Our main.cpp picks that up.
   Here we declare the symbol we're going to call. */
extern int SDL_main(int argc, char *argv[]);

/* Parse the command line into argc/argv. */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    (void)hInst; (void)hPrev; (void)sw;

    char *cmdline = GetCommandLineA();
    int argc = 0;

    /* Count args (simplistic: split on spaces, no quoting). */
    char *buf = _strdup(cmdline);
    char *p = buf;
    char *argv_buf[256];

    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv_buf[argc++] = p;
        if (argc >= 255) break;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    argv_buf[argc] = NULL;

    int ret = SDL_main(argc, argv_buf);

    free(buf);
    return ret;
}
