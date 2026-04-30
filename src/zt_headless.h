#ifndef _ZT_HEADLESS_H_
#define _ZT_HEADLESS_H_

// Headless / scripted operation. Two CLI flags drive this module:
//
//   --headless       Sets SDL_HINT_VIDEO_DRIVER=dummy before SDL_Init so no
//                    window opens. The renderer + texture pipeline still
//                    runs against the in-memory ARGB8888 surface, which we
//                    can dump to PNG via libpng's simplified API.
//
//   --script <file>  Drives the running app from a script. Each line is
//                    one command. Comments (#...) and blanks are ignored.
//                    Commands:
//                       key <name>           inject a synthetic keystroke
//                                            (e.g.  key F2,
//                                                   key shift+F3,
//                                                   key ctrl+s,
//                                                   key down, key tab,
//                                                   key space, key return)
//                       wait <ms>            sleep this thread for <ms>
//                                            milliseconds (lets the page's
//                                            update() / draw() run a few
//                                            frames before the next key)
//                       shot <path>          dump the current screen surface
//                                            to a PNG at <path>
//                       quit                 sets do_exit so the main loop
//                                            tears down cleanly
//
// Together they let the dev (or CI) drive zt to any state, capture the
// frame, diff it, and exit -- without ever opening a window.
//
// Designed to be called from main.cpp's render loop and from main(),
// nothing more. No SDL types in this header so the keybuffer / module
// unit tests don't have to drag SDL in just to compile against zt.h.

struct SDL_Surface;

// CLI flag plumbing. Set once from zt_parse_cli's caller in main().
// `script_path` ownership stays with the caller (typically argv).
void  zt_headless_init(bool active, const char *script_path);
bool  zt_headless_active(void);

// Pump the script forward by one frame: inject any keystrokes whose
// scheduled time has elapsed, dump any pending PNG, advance until we hit
// a `wait` or run out of commands. Called once per main-loop iteration
// from main.cpp BEFORE the SDL_PollEvent block.
//
// `frame_surface` is the current screen surface (zt's `screen_buffer_surface`)
// -- used by `shot` commands to dump the current frame.
void  zt_headless_pump(SDL_Surface *frame_surface);

// Dump an SDL ARGB8888 surface to a PNG. Returns 1 on success, 0 on failure.
// Exposed so internal `shot` commands and any future test harness can share
// the same writer.
int   zt_headless_dump_png(SDL_Surface *frame_surface, const char *path);

#endif
