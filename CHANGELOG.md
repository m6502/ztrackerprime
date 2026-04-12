# Changelog — esaruoho/ztrackerprime cross-platform fork

All changes on top of m6502/ztrackerprime (last upstream commit: `dba2320`, Aug 2025).

## Recent fixes (2026-04-12, post-initial-port)

- **All ALT shortcuts work with Cmd on macOS** — KS_HAS_ALT macro matches ALT, META, or META|ALT. Cmd+B/E/L (block), Cmd+1-0 (edit step), Cmd+Q/A (transpose), Cmd+D (select) all work with Mac Cmd key
- **macOS Backspace works as Delete** — Mac "delete" key (DIK_BACK) now falls through to DIK_DELETE handler in Pattern Editor
- **All pages aligned to TRACKS_ROW_Y** — System Config, Song Config, Instrument Editor content now starts at the same row as Pattern Editor
- **Version string moved up** — HEADER_ROW changed from 2 to 1, centers properly in top bar
- **SDL key repeat enabled** — holding cursor keys now repeats on all platforms (SDL_EnableKeyRepeat)
- **F11/F12 title labels corrected** — Song Config says (F11), System Config says (F12)
- **Screen bleed fixed** — switch_page() fills background, Pattern Editor clears on enter, Play Song triggers refresh on leave (#17)
- **Help ESC goes to Pattern Editor** — no more garbled System Config view when pressing ESC from Help
- **Pattern Editor uses full window height** — PATTERN_EDIT_ROWS computed dynamically from CHARS_Y, shows ~6 more rows (#15)
- **ListBox cursor clamped on resize** — cursor stays visible when window shrinks (#13)
- **macOS .app bundle** with icon (zt.icns), Info.plist, bundled zt.conf/skins/doc in Resources (#10)
- **macOS startup crash fixed** — cur_dir was NULL when GetCurrentDirectory called during init
- **Skin switching crash fixed** — guard against double-free of SDL surfaces via sdl12-compat
- **MIDI device names now display correctly** — RtMidi port index offset was wrong when audio plugins present. "Opened[out]: IAC Driver Bus 1" instead of "Opened[out]: "
- **help.txt loads on macOS** — tries multiple paths (zt_directory/doc/, doc/, help.txt) as fallback
- **F1 toggles Help** — pressing F1 while Help is open closes it (was one-way)
- **Help page aligned** with Pattern Editor — starts at TRACKS_ROW_Y, fills full window
- **F3 Instrument Editor aligned** — starts at TRACKS_ROW_Y like F2 (was hardcoded row 13)
- **All page titles show shortcut keys** — "Pattern Editor (F2)", "Help (F1)", "System Configuration (F11)", etc.
- **macOS Cmd key support** — Cmd maps to ALT so Cmd+Q=transpose up, Cmd+A=transpose down, etc.
- **VU meters re-enabled** — removed DISABLE_VUMETERS, fixed display row variable (#4)
- **Version bumped** to v2026_04_12
- **Start scripts** — start-mac.sh, start-linux.sh, start-windows.sh, start-xp.sh
- **5 community skins added** — blue-c64, reaktor, tekstyle, x.seed, xt-g01 + old 0.85 skins

### Open issues being tracked

- #18 System Configuration UX: broken tab order, missing keyboard navigation
- #17 Play Song view bleeds through when switching to Pattern Editor
- #16 Show playback activity under Pattern Editor during playback
- #15 Pattern Editor doesn't use full window height
- #14 Some skins include obsolete shortcuts
- #13 Resize may leave cursor out of text list
- #12 Resize window may move it
- #11 Configurable keyboard shortcuts saved to zt.conf
- #6 In-app zt.conf editor + document .conf format
- #1 Add Lua scripting support

## Cross-platform build system

- **CMake replaces Visual Studio .sln/.vcproj** — one build system for all platforms
- **Win32 cross-compile from macOS/Linux** via MinGW-w64 (i686 for XP 32-bit, x86_64 for Win10/11)
- **macOS native build** via Clang + sdl12-compat (Apple Silicon + Intel)
- **Linux build** supported (ALSA via RtMidi, untested)
- **PE subsystem version 5.1** — produced zt.exe runs on Windows XP SP2 through Windows 11
- **FetchContent** for zlib 1.3.1 + libpng 1.6.44 (no manual dependency wrangling)
- **Vendored SDL 1.2.15** MinGW dev pack in third_party/ for reproducible Win32 builds
- **Custom sdl_win32_main.c** replaces pre-built libSDLmain.a (old MSVC _imp___iob incompatible with GCC 15)
- **Start scripts**: `start-mac.sh`, `start-linux.sh`, `start-windows.sh`, `start-xp.sh` — check deps, build, run
- **Runtime data copied to build dir** at build time (zt.conf, skins/, doc/) so binary works out of the box

## MIDI — now cross-platform via RtMidi

- **RtMidi 6.0.0 vendored** into src/third_party/rtmidi/
- **MidiOutputDevice** rewritten: uses RtMidiOut instead of HMIDIOUT/midiOutShortMsg
- **midiInDevice** rewritten: uses RtMidiIn instead of HMIDIIN/midiInCallback
- All MIDI I/O works on **WinMM** (Windows), **CoreMIDI** (macOS), **ALSA/JACK** (Linux)
- Same RtMidi callback feeds the existing midiInQueue — downstream code unchanged

## Playback timer — now portable

- **std::chrono::steady_clock** replaces Win32 timeSetEvent/timeBeginPeriod
- Dedicated timer thread (`player::timerThreadFunc`) fires player_callback at 1ms intervals
- Counter thread uses **std::thread** instead of CreateThread

## Threading and synchronization — now portable

- **std::thread** replaces CreateThread (playback, load, save — 3 call sites)
- **std::timed_mutex** replaces CreateMutex/WaitForSingleObject/ReleaseMutex (hEditMutex)
- lock_mutex/unlock_mutex rewritten to use std::timed_mutex::try_lock_for/unlock

## Platform abstraction layer

- **src/platform/platform.h** — portable stubs for Win32 types and functions on macOS/Linux
- **MessageBox** → fprintf(stderr) on non-Windows
- **GetCurrentDirectory/SetCurrentDirectory** → getcwd/chdir on POSIX
- **GetLogicalDrives/GetDriveType** → stubs (no drive letters on macOS/Linux)
- **LPSTR/LPCTSTR/DWORD/UINT/BOOL** → standard C++ types on non-Windows
- **MIDI error codes** (MIDIERR_NODEVICE, MMSYSERR_NOMEM, etc.) defined portably
- **`<windows.h>` / `<mmsystem.h>`** now only included on `#ifdef _WIN32` (via zt.h)
- **MidiStream.h** gutted (unused class)

## macOS-specific fixes

- **Path separators**: backslash → forward slash for skins, argv[0] parsing, cwd (skins.cpp, main.cpp)
- **Cmd key support**: macOS Cmd (Meta) maps to KS_ALT so all ALT-based shortcuts work with Cmd
  - Cmd+Q = transpose up, Cmd+A = transpose down, etc.
- **postAction() null guard**: no more crash when initialization fails (zt.conf not found)
- **Skin switching crash fixed**: guard against double-free of SDL surfaces via sdl12-compat
- **zt.conf + skins copied to build dir** by CMake so the binary finds them

## Code quality

- **const char\* cleanup** across 19 files — zero `-Wwrite-strings` warnings (the chain reaction fix described by @uucidl in 2017)
- **_ASSERT → assert** (FileList.cpp, img.cpp) — standard C instead of MSVC debug macro
- **min/max → std::min/std::max** (playback.cpp) — standard C++ instead of Windows macros
- **__int64 → int64_t** (playback.cpp) — standard C++ integer type
- **_strdup → strdup** (CUI_Config.cpp) — POSIX instead of MSVC-ism
- **C++17 standard** enforced via CMake

## Bug fixes

- **Default to windowed mode** — full_screen changed from 1 to 0 in conf.cpp constructor
- **8-bit PNG support restored** — added png_set_palette_to_rgb() for paletted PNGs (was disabled behind #ifdef)
- **VU meters re-enabled** — removed DISABLE_VUMETERS define, fixed display row variable (ztPlayer->playing_cur_row instead of ztPlayer->cur_row)
- **F1 toggles Help** — pressing F1 while Help is open now closes it (was one-way only)
- **Help page aligned** with Pattern Editor — starts at same row, fills full window

## Content

- **5 community skins added**: blue-c64, reaktor, tekstyle (Daxx909), x.seed (Demmas), xt-g01 (Daxx909)
- **Old 0.85 skins** preserved: platinum, blue-c64, PatriciaArquette
- **Skin README** included
- **Version bumped** to v2026_04_12
- **README rewritten** with CMake build instructions, cross-platform docs, contributor credits

## Credits

- [@rbruinier](https://github.com/rbruinier) (Robert Bruinier) and [@superjohan](https://github.com/superjohan) (Johan Halin) — 2017 ztracker_mac work that demonstrated cross-platform feasibility
- [@uucidl](https://github.com/uucidl) (Nicolas Leveille) — const correctness guidance
- [@m6502](https://github.com/m6502) (Manuel Montoto) — zTracker' maintainer, all upstream improvements
