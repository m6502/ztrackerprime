# Coding conventions

The codebase predates modern C++ style. Follow the existing idioms rather than imposing new ones.

## File layout

- Headers in `src/*.h`, implementations in `src/*.cpp`. A handful of header-only modules use `.hpp` (`CUI_FileLists_Common.hpp`).
- Pure-data / pure-decision modules added since PR #64 are header-only `.h` files (`preset_data.h`, `preset_selector.h`, `page_sync.h`, `save_key_dispatch.h`, `editor_layout.h`).
- Pages live in `src/CUI_<Pagename>.{h,cpp}` and inherit from `CUI_Page`.

## Header guards

Style varies. New headers use `#ifndef _<NAME>_H_ / #define _<NAME>_H_ / #endif`. Don't switch existing files to `#pragma once`.

## Include order

Implementation files typically include local headers first, then SDL/system headers, then `<stdio.h>`/`<string.h>` etc. Don't reorder unless the change is the point of the patch.

## Naming

- Classes: `PascalCase` (`CheckBox`, `ValueSlider`, `CUI_Patterneditor`).
- Functions: mixed — older code is `lower_snake`, newer code is sometimes `camelCase`. Match the surrounding file.
- Constants / macros: `UPPER_SNAKE` (`KS_HAS_ALT`, `INITIAL_ROW`, `STATE_PEDIT`).
- Page globals: `UIP_<Pagename>` for the `CUI_Page*` instance, `STATE_<NAME>` for the state enum.

## Status-bar diagnostic idiom

To trace which code path fired in a UI bug, set `status_change=1; statusmsg="...";` in the suspected branch. The status bar then displays the message until the next change. Useful when the user can't observe internal state otherwise.

Don't ship leftover diagnostic messages — strip them once the bug is fixed.

## SDL3, not SDL2

`#include <SDL.h>` resolves to SDL3. Keys are `SDL_SCANCODE_*` and `SDLK_*`; events are `SDL_EVENT_*`. Use the SDL3 API; don't paste SDL2 patterns.

## Lua

Lua is statically linked from `extlibs/lua/`. The Lua console (F-key bound) lives in `src/CUI_LuaConsole.cpp`. Don't add new Lua bindings without checking the existing `lua_*` glue and CMake setup.

## "Don't add what isn't asked for"

zTracker is a 25-year-old codebase with a tight contract. Avoid speculative refactors, premature abstractions, or bringing modern-C++ features into legacy modules just because they're available. Match the surrounding style; touch one thing at a time.
