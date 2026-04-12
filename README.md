zTracker'
=========

zTracker is a cross-platform MIDI tracker / sequencer modeled after Impulse Tracker / Scream Tracker. 

zTracker' is a fork of zTracker by [@m6502](https://github.com/m6502) (Manuel Montoto), now ported to build on **Windows (XP 32-bit through Win11)** and **macOS (Apple Silicon + Intel)** from a single source tree.

![ztscreenshot](https://github.com/user-attachments/assets/4ea296d7-0b72-49e2-a246-a064474e8c75)


Build instructions
------------------

Requires CMake 3.20+, Ninja, and a C++17 compiler.

### macOS

```bash
brew install sdl12-compat ninja cmake
cmake -S . -B build-macos -G Ninja
cmake --build build-macos
# -> build-macos/src/zt
```

### Windows (cross-compile from macOS via MinGW-w64)

```bash
brew install mingw-w64
cmake -S . -B build-win32 -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-xp-toolchain.cmake
cmake --build build-win32
# -> build-win32/src/zt.exe (PE32 i386, runs on Windows XP SP2 through Windows 11)
```

### Windows (native MSVC)

Open the included `zt.sln` in Visual Studio 2015+ or use CMake with the MSVC generator.


Features
--------

- 1:1 copy of Impulse Tracker interface
- 64 track sequencer with variable 32-256 rows/pattern, 256 total patterns
- Multiple MIDI devices across multiple interfaces (via RtMidi: WinMM / CoreMIDI / ALSA / JACK)
- Rock solid timing (3/496ppqn accuracy)
- Load/save compressed .zt files
- Volume/effect curve drawing in pattern editor
- IT file importing
- Auto sync via MIDI clock
- .mid file export
- Intelligent MIDI-in with slave to external sync
- Zoom mode (2x, 3x, 4x...) for high resolution screens
- Resizeable window
- BPM/TPB changes applied in real-time during playback
- 7 bundled skins (blue-c64, default, professional, reaktor, tekstyle, x.seed, xt-g01)

### macOS keyboard

On macOS, the **Cmd key** (Command) works as the primary modifier in place of ALT. All ALT-based shortcuts (transpose, selections, etc.) respond to Cmd on Mac. For example:
- **Cmd+Q** = Transpose selection up
- **Cmd+A** = Transpose selection down
- **Cmd+D** = Select block
- **ALT+CTRL+Q** = Quit (same on all platforms)


What's different from original zTracker (2002)
----------------------------------------------

Changes by [@m6502](https://github.com/m6502) (Manuel Montoto), who has been using zTracker daily for 24+ years:

**Program behavior:**
- Laptop keyboard support (octave up/down remapped to keys after I/O/P)
- 1bpp PNG skin font support
- Zoom mode for high-DPI screens
- Resizeable window
- ALT-based shortcuts (instead of CTRL)
- F-key channel enable/disable (instead of number row)
- First 16 instruments auto-assigned to MIDI channels
- Real-time BPM/TPB changes during playback
- Starts in Tracker Mode by default
- Used instruments shown with dot in instrument editor after playback
- Page Up/Down moves by highlighted-row increments (works in non-4/4 time)
- Default pattern size 64 (was 128)
- Minimum resolution 1024x700
- Play view fits channels to window width
- Load/Save screens use full available space

**Bug fixes:**
- Many crash fixes, memory bug fixes (uninitialized variables etc.)
- File requester shows .mid files
- Ctrl+L/Ctrl+S load/save work again
- F6 plays current pattern even if not in order list
- Scroll moves when playing individual notes/rows
- Pattern row numbers display correctly for non-default lengths
- Highlight clears when playback exits current pattern
- Numpad +/- both work for song position

**Removed:**
- Initial splash screen
- "Are you crazy" quit dialog (just stops and loads)

**Known limitations:**
- zt.conf must be edited by hand (in-app config editor planned)
- Fullscreen toggle may not work reliably on all platforms


Cross-platform port (2026)
--------------------------

This fork adds:

| Component | Implementation |
|---|---|
| **Build system** | CMake (replaces VS 2008/2015 .sln) |
| **C++ standard** | C++17 with const correctness |
| **MIDI I/O** | RtMidi 6.0 (cross-platform: WinMM, CoreMIDI, ALSA, JACK) |
| **Playback timer** | `std::chrono::steady_clock` (replaces Win32 `timeSetEvent`) |
| **Threading** | `std::thread` (replaces Win32 `CreateThread`) |
| **Mutex** | `std::timed_mutex` (replaces Win32 `CreateMutex`) |
| **Platform shim** | `src/platform/platform.h` for remaining Win32 type stubs |
| **Dependencies** | zlib 1.3.1 + libpng 1.6.44 via FetchContent; SDL 1.2 (sdl12-compat on macOS) |
| **Win32 target** | MinGW-w64 cross-compile, PE subsystem 5.1 (XP SP2 compatible) |
| **macOS target** | Native arm64/x86_64 via Clang |

### Acknowledgments

- **Chris Micali** ([@cmicali](https://github.com/cmicali)) — original zTracker author (2000-2002)
- **Manuel Montoto** ([@m6502](https://github.com/m6502)) — zTracker' maintainer, 24 years of daily use, bug fixes, and feature improvements
- **Austin Luminais** (lipid), **Nic Soudee** (zonaj), **Daniel Kahlin** (tlr) — original zTracker contributors
- **Robert Bruinier** ([@rbruinier](https://github.com/rbruinier)) — created the macOS abstraction layer in [ztracker_mac](https://github.com/esaruoho/ztracker_mac) (2015), identifying the Win32 API boundaries
- **Johan Halin** ([@superjohan](https://github.com/superjohan)) — compile fixes for macOS Xcode build (2017)
- **Nicolas Leveille** ([@uucidl](https://github.com/uucidl)) — const correctness guidance for the char* chain reaction fix

### Skin authors

- **Daxx909** (Rob Smolenaars) — tekstyle, xt-g01 skins
- **Demmas** — x.seed skin


Links
-----

- [Original zTracker on SourceForge](http://ztracker.sourceforge.net)
- [zTracker' upstream by m6502](https://github.com/m6502/ztrackerprime)
- [Original zTracker source by cmicali](https://github.com/cmicali/ztracker)
