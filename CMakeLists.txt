cmake_minimum_required(VERSION 3.20)

set(MINGW_ROOT "W:/MINGW64" CACHE PATH "MinGW root directory (used with MinGW Makefiles generator)")

if(CMAKE_GENERATOR MATCHES "MinGW Makefiles")
  if(NOT DEFINED CMAKE_C_COMPILER AND EXISTS "${MINGW_ROOT}/bin/gcc.exe")
    set(CMAKE_C_COMPILER "${MINGW_ROOT}/bin/gcc.exe" CACHE FILEPATH "C compiler" FORCE)
  endif()
  if(NOT DEFINED CMAKE_CXX_COMPILER AND EXISTS "${MINGW_ROOT}/bin/g++.exe")
    set(CMAKE_CXX_COMPILER "${MINGW_ROOT}/bin/g++.exe" CACHE FILEPATH "C++ compiler" FORCE)
  endif()
  if(NOT DEFINED CMAKE_RC_COMPILER AND EXISTS "${MINGW_ROOT}/bin/windres.exe")
    set(CMAKE_RC_COMPILER "${MINGW_ROOT}/bin/windres.exe" CACHE FILEPATH "Resource compiler" FORCE)
  endif()
  if(NOT DEFINED CMAKE_MAKE_PROGRAM AND EXISTS "${MINGW_ROOT}/bin/mingw32-make.exe")
    set(CMAKE_MAKE_PROGRAM "${MINGW_ROOT}/bin/mingw32-make.exe" CACHE FILEPATH "Make program" FORCE)
  endif()
endif()

if(WIN32)
  project(ztracker LANGUAGES C CXX RC)
else()
  project(ztracker LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so clangd/IDE tooling can resolve SDL headers
# and project include paths without guessing.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ZT_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

# Sync tempo/transport with Ableton Link peers on the LAN. When ON and the
# vendored Link source tree is present (extlibs/link/, fetched by
# .github/scripts/fetch-ableton-link.sh), zt links Ableton::Link and compiles
# the real wrapper. When OFF or the tree is absent, the wrapper builds as a
# safe stub so the binary still works without Link.
option(ZT_ENABLE_ABLETON_LINK "Sync tempo/transport with Ableton Link peers on the LAN" ON)

set(ZT_WINDOWS_LIB_DIR "${CMAKE_SOURCE_DIR}/extlibs" CACHE PATH "Directory containing Windows SDL libraries (.lib/.dll)")
set(SDL3_INCLUDE_DIR "" CACHE PATH "Path containing SDL3/SDL.h (or SDL.h with SDL3 include root)")
set(SDL3_LIBRARY "" CACHE FILEPATH "Full path to SDL3 library (libSDL3*.a/.so/.dylib or SDL3.lib)")
set(SDL3MAIN_LIBRARY "" CACHE FILEPATH "Full path to SDL3main library (Windows/MinGW only)")

# --- Lua 5.4 static library ---------------------------------------------------
file(GLOB LUA_SOURCES "${CMAKE_SOURCE_DIR}/extlibs/lua/*.c")
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC "${CMAKE_SOURCE_DIR}/extlibs/lua")
target_compile_definitions(lua_static PRIVATE _CRT_SECURE_NO_WARNINGS)
if(UNIX)
  target_link_libraries(lua_static PRIVATE m)
endif()
# ------------------------------------------------------------------------------

# --- zlib static library (shared by zt and the ztskin tool) ------------------
add_library(zt_zlib_static STATIC
  extlibs/zlib/adler32.c
  extlibs/zlib/compress.c
  extlibs/zlib/crc32.c
  extlibs/zlib/deflate.c
  extlibs/zlib/infback.c
  extlibs/zlib/inffast.c
  extlibs/zlib/inflate.c
  extlibs/zlib/inftrees.c
  extlibs/zlib/trees.c
  extlibs/zlib/zutil.c
)
target_include_directories(zt_zlib_static PUBLIC "${CMAKE_SOURCE_DIR}/extlibs/zlib")
target_compile_definitions(zt_zlib_static PRIVATE _CRT_SECURE_NO_WARNINGS)
# zlib's own sources trip a few -Wpedantic / sign-conversion warnings; quiet
# them on GCC/Clang so the rest of the project keeps -Wall -Wextra -Wpedantic.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(zt_zlib_static PRIVATE -w)
endif()
# ------------------------------------------------------------------------------

set(ZT_SOURCES
  extlibs/libpng/png.c
  extlibs/libpng/pngerror.c
  extlibs/libpng/pngget.c
  extlibs/libpng/pngmem.c
  extlibs/libpng/pngpread.c
  extlibs/libpng/pngread.c
  extlibs/libpng/pngrio.c
  extlibs/libpng/pngrtran.c
  extlibs/libpng/pngrutil.c
  extlibs/libpng/pngset.c
  extlibs/libpng/pngtrans.c
  extlibs/libpng/pngwio.c
  extlibs/libpng/pngwrite.c
  extlibs/libpng/pngwtran.c
  extlibs/libpng/pngwutil.c
  src/CDataBuf.cpp
  src/zt_crash.cpp
  src/conf.cpp
  src/edit_cols.cpp
  src/font.cpp
  src/img_sdl3.cpp
  src/keybindings.cpp
  src/keybuffer.cpp
  src/lc_sdl_wrapper.cpp
  src/list.cpp
  src/main.cpp
  src/midi-io.cpp
  src/module.cpp
  src/OutputDevices.cpp
  src/platform.cpp
  src/playback.cpp
  src/skins.cpp
  src/zlib_wrapper.cpp
  src/ztfile-io.cpp
  src/CUI_About.cpp
  src/CUI_Arpeggioeditor.cpp
  src/CUI_Config.cpp
  src/CUI_Help.cpp
  src/CUI_InstEditor.cpp
  src/CUI_LoadMsg.cpp
  src/CUI_Loadscreen.cpp
  src/CUI_Logoscreen.cpp
  src/CUI_Midimacroeditor.cpp
  src/CUI_NewSong.cpp
  src/CUI_Ordereditor.cpp
  src/CUI_Page.cpp
  src/CUI_Patterneditor.cpp
  src/CUI_PEFindReplace.cpp
  src/CUI_PENature.cpp
  src/CUI_PEParms.cpp
  src/CUI_TrackOptions.cpp
  src/CUI_IEParms.cpp
  src/CUI_PEVol.cpp
  src/CUI_Playsong.cpp
  src/CUI_RUSure.cpp
  src/CUI_SaveMsg.cpp
  src/CUI_Savescreen.cpp
  src/CUI_SliderInput.cpp
  src/CUI_Songconfig.cpp
  src/CUI_SongDuration.cpp
  src/CUI_SongMessage.cpp
  src/CUI_PaletteEditor.cpp
  src/CUI_Sysconfig.cpp
  src/import_export.cpp
  src/pattern_display_layout.cpp
  src/it.cpp
  src/Button.cpp
  src/FileList.cpp
  src/InstEditor.cpp
  src/OrderEditor.cpp
  src/PatternDisplay.cpp
  src/Sliders.cpp
  src/UserInterface.cpp
  src/lua_engine.cpp
  src/CUI_LuaConsole.cpp
  src/CUI_KeyBindings.cpp
  src/CUI_MainMenu.cpp
  src/midi_mappings.cpp
  src/ccizer.cpp
  src/CUI_CcConsole.cpp
  src/sysex_inq.cpp
  src/CUI_SysExLibrarian.cpp
  src/sysex_macro.cpp
  src/ccenv_io.cpp
  src/CUI_CCEnvelopeEditor.cpp
  src/ableton_link.cpp
  src/midi_clock_sync.cpp
  src/zt_headless.cpp
)

if(WIN32)
  list(APPEND ZT_SOURCES zt.rc)
  # XP-compat shims (GetTickCount64 polyfill etc.) -- only needed on the
  # 32-bit Windows XP target; harmless to include on other Windows builds
  # because the file is fully #if'd to defined(_WIN32) && !defined(_WIN64).
  list(APPEND ZT_SOURCES src/xp_compat.c)
endif()

if(APPLE)
  # App icon. zt.icon (Icon Composer) is the source of truth; zt.icns is a
  # pre-Tahoe fallback rendered from it. zt.icns is always embedded so every
  # macOS / toolchain shows the icon; on Xcode 26+ actool additionally compiles
  # zt.icon into an Assets.car (Liquid Glass icon used on macOS 26+) — see the
  # gated POST_BUILD step below.
  set(ZT_MACOSX_ICON "${CMAKE_SOURCE_DIR}/zt.icns")
  list(APPEND ZT_SOURCES ${ZT_MACOSX_ICON})
  set_source_files_properties(${ZT_MACOSX_ICON} PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")

  # Probe whether this toolchain's actool understands the .icon format
  # (Xcode 26+). Older actool emits no Assets.car without erroring, so we key
  # off the artifact's existence rather than the exit code.
  set(ZT_ICON_COMPOSER OFF)
  execute_process(COMMAND xcrun --find actool
    OUTPUT_VARIABLE ZT_ACTOOL OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE ZT_ACTOOL_RC ERROR_QUIET)
  if(ZT_ACTOOL_RC EQUAL 0 AND ZT_ACTOOL)
    set(ZT_ICON_PROBE "${CMAKE_BINARY_DIR}/icon-probe")
    file(MAKE_DIRECTORY "${ZT_ICON_PROBE}")
    execute_process(
      COMMAND "${ZT_ACTOOL}" "${CMAKE_SOURCE_DIR}/assets/icon/zt.icon"
        --app-icon zt --compile "${ZT_ICON_PROBE}" --platform macosx
        --minimum-deployment-target 26.0
        --output-partial-info-plist "${ZT_ICON_PROBE}/partial.plist"
      OUTPUT_QUIET ERROR_QUIET)
    if(EXISTS "${ZT_ICON_PROBE}/Assets.car")
      set(ZT_ICON_COMPOSER ON)
    endif()
  endif()
  if(ZT_ICON_COMPOSER)
    message(STATUS "zTracker: Icon Composer (.icon) supported — bundling Assets.car")
  else()
    message(STATUS "zTracker: Icon Composer (.icon) unsupported by this toolchain — using zt.icns only")
  endif()

  # Objective-C++ helper that strips Cmd-Q from the auto-created NSApp menu
  # so the Pattern Editor can use Cmd-Q as Transpose-Up.
  list(APPEND ZT_SOURCES src/macos_menu.mm)
  set_source_files_properties(src/macos_menu.mm PROPERTIES
    COMPILE_FLAGS "-fobjc-arc")
endif()

if(WIN32)
  add_executable(zt WIN32 ${ZT_SOURCES})
else()
  add_executable(zt ${ZT_SOURCES})
endif()
target_include_directories(zt PRIVATE src extlibs/libpng extlibs/zlib extlibs/lua)
target_link_libraries(zt PRIVATE lua_static zt_zlib_static)

# --- Ableton Link ------------------------------------------------------------
# Link ships an Ableton::Link IMPORTED INTERFACE target via AbletonLinkConfig
# .cmake that carries include dirs, per-platform compile defs, and link libs.
# We link it only when ZT_ENABLE_ABLETON_LINK is ON AND the source tree is
# present. ZT_HAS_ABLETON_LINK then flips src/ableton_link.cpp from its
# stub to the real implementation. MinGW is excluded: the XP target pins
# _WIN32_WINNT=0x0501 and asio's Windows backend needs Vista+ APIs.
set(ZT_ABLETON_LINK_AVAILABLE OFF)
if(ZT_ENABLE_ABLETON_LINK AND NOT MINGW
   AND EXISTS "${CMAKE_SOURCE_DIR}/extlibs/link/AbletonLinkConfig.cmake")
  include("${CMAKE_SOURCE_DIR}/extlibs/link/AbletonLinkConfig.cmake")
  target_link_libraries(zt PRIVATE Ableton::Link)
  target_compile_definitions(zt PRIVATE ZT_HAS_ABLETON_LINK)
  set(ZT_ABLETON_LINK_AVAILABLE ON)
endif()
message(STATUS "Ableton Link: ${ZT_ABLETON_LINK_AVAILABLE}")
# -----------------------------------------------------------------------------

# Linux release artifacts bundle libSDL3.so.0 next to the binary (Ubuntu 24.04
# apt has no libsdl3-dev yet). Without $ORIGIN in the rpath, the dynamic linker
# won't look beside the binary and `./zt` fails with
#   "error while loading shared libraries: libSDL3.so.0: cannot open shared
#    object file: No such file or directory"
# even though the .so is sitting right there. macOS uses .app bundles; Windows
# searches the exe directory for DLLs natively. Linux is the only platform
# that needs this.
if(UNIX AND NOT APPLE)
  set_target_properties(zt PROPERTIES
    BUILD_RPATH   "$ORIGIN"
    INSTALL_RPATH "$ORIGIN")
endif()

if(MSVC)
  target_compile_options(zt PRIVATE /W3 /MP /wd4103)
else()
  target_compile_options(zt PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(zt PRIVATE -Wno-pragma-pack)
  # Apple's SDK marks sprintf and friends as deprecated in favour of the
  # snprintf family. The codebase uses sprintf extensively with known-safe
  # buffer sizes; silence the noise project-wide on Clang rather than
  # touching 200+ call sites. MSVC's _CRT_SECURE_NO_WARNINGS above does
  # the equivalent on Windows.
  target_compile_options(zt PRIVATE -Wno-deprecated-declarations)
endif()

if(ZT_ENABLE_SANITIZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zt PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g3)
    target_link_options(zt PRIVATE -fsanitize=address,undefined)
  else()
    message(WARNING "ZT_ENABLE_SANITIZERS is ON, but compiler does not support GCC/Clang sanitizer flags.")
  endif()
endif()

target_compile_definitions(zt PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
target_compile_definitions(zt PRIVATE PNG_ARM_NEON_OPT=0)

# Crash handler uses backtrace_symbols which needs symbol names to be
# exported. -rdynamic is the GNU ld flag (also accepted by lld); the
# Apple linker exports symbols from executables by default so this is
# a no-op there. MSVC has its own debug-info pipeline.
if(NOT WIN32 AND NOT APPLE)
  target_link_options(zt PRIVATE "-rdynamic")
endif()

# Inject build date as ZT_BUILD_DATE so the version string in the UI reflects
# when the binary was actually built, instead of a stale hardcoded value.
string(TIMESTAMP ZT_BUILD_DATE "%Y_%m_%d" UTC)
target_compile_definitions(zt PRIVATE ZT_BUILD_DATE="${ZT_BUILD_DATE}")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL3PC QUIET IMPORTED_TARGET sdl3)
endif()

if(TARGET PkgConfig::SDL3PC)
  target_link_libraries(zt PRIVATE PkgConfig::SDL3PC)
  find_path(SDL3_COMPAT_INCLUDE_DIR
    NAMES SDL.h
    HINTS ${SDL3PC_INCLUDE_DIRS}
    PATH_SUFFIXES SDL3
  )
  if(SDL3_COMPAT_INCLUDE_DIR)
    target_include_directories(zt SYSTEM PRIVATE "${SDL3_COMPAT_INCLUDE_DIR}")
  endif()
else()
  if(WIN32 AND (NOT DEFINED SDL3_INCLUDE_DIR OR SDL3_INCLUDE_DIR STREQUAL ""))
    if(EXISTS "${ZT_WINDOWS_LIB_DIR}/SDL3/include/SDL3/SDL.h")
      set(SDL3_INCLUDE_DIR "${ZT_WINDOWS_LIB_DIR}/SDL3/include")
    endif()
  endif()

  if(NOT DEFINED SDL3_INCLUDE_DIR OR SDL3_INCLUDE_DIR STREQUAL "")
    find_path(SDL3_INCLUDE_DIR
      NAMES SDL3/SDL.h SDL.h
      HINTS
        "${ZT_WINDOWS_LIB_DIR}"
        "${ZT_WINDOWS_LIB_DIR}/include"
        "${ZT_WINDOWS_LIB_DIR}/SDL3"
        "${ZT_WINDOWS_LIB_DIR}/SDL3/include"
        "/usr/include"
        "/usr/local/include"
      PATH_SUFFIXES SDL3 include
    )
  endif()

  if(WIN32 AND (NOT DEFINED SDL3_LIBRARY OR SDL3_LIBRARY STREQUAL "") AND EXISTS "${ZT_WINDOWS_LIB_DIR}/SDL3.lib")
    set(SDL3_LIBRARY "${ZT_WINDOWS_LIB_DIR}/SDL3.lib")
  endif()

  if(NOT DEFINED SDL3_LIBRARY OR SDL3_LIBRARY STREQUAL "")
    find_library(SDL3_LIBRARY
      NAMES SDL3 SDL3-static libSDL3.dll.a libSDL3.a
      HINTS
        "${ZT_WINDOWS_LIB_DIR}"
        "${ZT_WINDOWS_LIB_DIR}/lib"
        "${MINGW_ROOT}/lib"
        "/usr/lib"
        "/usr/lib64"
        "/usr/local/lib"
        "/usr/local/lib64"
    )
  endif()

  if(WIN32 AND (NOT DEFINED SDL3MAIN_LIBRARY OR SDL3MAIN_LIBRARY STREQUAL "") AND EXISTS "${ZT_WINDOWS_LIB_DIR}/SDL3main.lib")
    set(SDL3MAIN_LIBRARY "${ZT_WINDOWS_LIB_DIR}/SDL3main.lib")
  endif()

  if(NOT DEFINED SDL3MAIN_LIBRARY OR SDL3MAIN_LIBRARY STREQUAL "")
    find_library(SDL3MAIN_LIBRARY
      NAMES SDL3main libSDL3main.a
      HINTS
        "${ZT_WINDOWS_LIB_DIR}"
        "${ZT_WINDOWS_LIB_DIR}/lib"
        "${MINGW_ROOT}/lib"
    )
  endif()

  if(WIN32)
    if(NOT DEFINED SDL3_INCLUDE_DIR OR SDL3_INCLUDE_DIR STREQUAL "" OR
       NOT DEFINED SDL3_LIBRARY OR SDL3_LIBRARY STREQUAL "")
      message(FATAL_ERROR
        "SDL3 not found. Set SDL3_INCLUDE_DIR and SDL3_LIBRARY (and SDL3MAIN_LIBRARY on MinGW/Windows when needed)."
      )
    endif()
  else()
    if(NOT DEFINED SDL3_INCLUDE_DIR OR SDL3_INCLUDE_DIR STREQUAL "" OR
       NOT DEFINED SDL3_LIBRARY OR SDL3_LIBRARY STREQUAL "")
      message(FATAL_ERROR
        "SDL3 not found. Install pkg-config sdl3, or set SDL3_INCLUDE_DIR and SDL3_LIBRARY."
      )
    endif()
  endif()

  target_include_directories(zt SYSTEM PRIVATE "${SDL3_INCLUDE_DIR}")
  if(EXISTS "${SDL3_INCLUDE_DIR}/SDL3/SDL.h")
    target_include_directories(zt SYSTEM PRIVATE "${SDL3_INCLUDE_DIR}/SDL3")
  endif()
  if(MINGW)
    target_link_options(zt PRIVATE -mwindows)
    if(DEFINED SDL3MAIN_LIBRARY AND NOT SDL3MAIN_LIBRARY STREQUAL "")
      target_link_libraries(zt PRIVATE mingw32 ${SDL3MAIN_LIBRARY} ${SDL3_LIBRARY})
    else()
      target_link_libraries(zt PRIVATE mingw32 ${SDL3_LIBRARY})
    endif()
  else()
    if(WIN32)
      if(DEFINED SDL3MAIN_LIBRARY AND NOT SDL3MAIN_LIBRARY STREQUAL "")
        target_link_libraries(zt PRIVATE ${SDL3MAIN_LIBRARY} ${SDL3_LIBRARY})
      else()
        target_link_libraries(zt PRIVATE ${SDL3_LIBRARY})
      endif()
    else()
      target_link_libraries(zt PRIVATE ${SDL3_LIBRARY})
    endif()
  endif()
endif()

if(WIN32)
  target_link_libraries(zt PRIVATE odbc32 odbccp32 winmm)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(zt PRIVATE asound)
endif()

if(APPLE)
  string(TIMESTAMP ZT_BUILD_VERSION "%Y.%m.%d" UTC)
  target_link_libraries(zt PRIVATE "-framework CoreMIDI" "-framework CoreFoundation" "-framework Cocoa")
  # Turn the macOS build into a proper .app bundle with icon + Info.plist.
  set_target_properties(zt PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "zTracker"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.m6502.ztrackerprime"
    MACOSX_BUNDLE_BUNDLE_VERSION "${ZT_BUILD_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${ZT_BUILD_VERSION}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "zTracker ${ZT_BUILD_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "(c) 2000-2026 zTracker contributors"
    MACOSX_BUNDLE_ICON_FILE "zt.icns"
    MACOSX_BUNDLE_INFO_STRING "zTracker - tracker-style music composition"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
  )
endif()

set_target_properties(zt PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Program"
)

if(APPLE)
  # Copy runtime resources into the .app bundle so a double-clicked app
  # finds skins/, palettes/, doc/, and zt.conf.
  add_custom_command(TARGET zt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/skins"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/skins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/palettes"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/palettes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/doc"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/doc"
    COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_SOURCE_DIR}/zt.conf"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/zt.conf"
    COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_SOURCE_DIR}/zt_icon.png"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/zt_icon.png"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/assets/ccizer"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/ccizer"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/assets/syx"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/syx"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/assets/lua"
      "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/lua"
    VERBATIM
  )
  if(ZT_ICON_COMPOSER)
    # Compile the Icon Composer source into the bundle: Assets.car is the
    # Liquid Glass icon used on macOS 26+. actool ALSO drops a zt.icns, but its
    # version is sparse (only ic04/ic07/ic11/ic13), so afterwards we restore the
    # committed full-size zt.icns (adds ic05/ic08/ic09/ic10/ic12/ic14) for the
    # pre-macOS-26 fallback path -- otherwise older systems render only a few
    # sizes and fall back to a generic icon at the rest.
    add_custom_command(TARGET zt POST_BUILD
      COMMAND "${ZT_ACTOOL}" "${CMAKE_SOURCE_DIR}/assets/icon/zt.icon"
        --app-icon zt
        --compile "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources"
        --platform macosx
        --minimum-deployment-target 26.0
        --output-partial-info-plist "${CMAKE_BINARY_DIR}/zt-icon-partial.plist"
      COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/zt.icns"
        "$<TARGET_BUNDLE_CONTENT_DIR:zt>/Resources/zt.icns"
      VERBATIM
    )
  endif()
else()
  # Copy runtime data next to the binary so it works out of the box on Windows/Linux.
  add_custom_command(TARGET zt POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/zt.conf" "$<TARGET_FILE_DIR:zt>/zt.conf"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/skins" "$<TARGET_FILE_DIR:zt>/skins"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/palettes" "$<TARGET_FILE_DIR:zt>/palettes"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/doc" "$<TARGET_FILE_DIR:zt>/doc"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/ccizer" "$<TARGET_FILE_DIR:zt>/ccizer"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/syx" "$<TARGET_FILE_DIR:zt>/syx"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/lua" "$<TARGET_FILE_DIR:zt>/lua"
  )
endif()


# --- ztskin (skin compiler/decompiler CLI) -------------------------------
option(ZT_BUILD_ZTSKIN "Build the ztskin skin compiler/decompiler CLI tool" ON)
if(ZT_BUILD_ZTSKIN)
    add_executable(ztskin tools/ztskin/ztskin.c)
    target_link_libraries(ztskin PRIVATE zt_zlib_static)
    target_compile_definitions(ztskin PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(ztskin PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Program")
    if(MSVC)
        target_compile_options(ztskin PRIVATE /W3)
    else()
        target_compile_options(ztskin PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# --- Tests ---------------------------------------------------------------
option(ZT_BUILD_TESTS "Build the standalone unit-test harness (run with ctest)" ON)
if(ZT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
