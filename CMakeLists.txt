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

option(ZT_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

set(ZT_WINDOWS_LIB_DIR "${CMAKE_SOURCE_DIR}/extlibs" CACHE PATH "Directory containing Windows SDL libraries (.lib/.dll)")
set(SDL3_INCLUDE_DIR "" CACHE PATH "Path containing SDL3/SDL.h (or SDL.h with SDL3 include root)")
set(SDL3_LIBRARY "" CACHE FILEPATH "Full path to SDL3 library (libSDL3*.a/.so/.dylib or SDL3.lib)")
set(SDL3MAIN_LIBRARY "" CACHE FILEPATH "Full path to SDL3main library (Windows/MinGW only)")

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
  src/CDataBuf.cpp
  src/conf.cpp
  src/edit_cols.cpp
  src/font.cpp
  src/img_sdl3.cpp
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
  src/CUI_PENature.cpp
  src/CUI_PEParms.cpp
  src/CUI_PEVol.cpp
  src/CUI_Playsong.cpp
  src/CUI_RUSure.cpp
  src/CUI_SaveMsg.cpp
  src/CUI_Savescreen.cpp
  src/CUI_SliderInput.cpp
  src/CUI_Songconfig.cpp
  src/CUI_SongDuration.cpp
  src/CUI_SongMessage.cpp
  src/CUI_Sysconfig.cpp
  src/import_export.cpp
  src/it.cpp
  src/Button.cpp
  src/FileList.cpp
  src/InstEditor.cpp
  src/OrderEditor.cpp
  src/PatternDisplay.cpp
  src/Sliders.cpp
  src/UserInterface.cpp
)

if(WIN32)
  list(APPEND ZT_SOURCES zt.rc)
endif()

if(WIN32)
  add_executable(zt WIN32 ${ZT_SOURCES})
else()
  add_executable(zt ${ZT_SOURCES})
endif()
target_include_directories(zt PRIVATE src extlibs/libpng extlibs/zlib)

if(MSVC)
  target_compile_options(zt PRIVATE /W3 /MP /wd4103)
else()
  target_compile_options(zt PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(zt PRIVATE -Wno-pragma-pack)
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
  target_link_libraries(zt PRIVATE "-framework CoreMIDI" "-framework CoreFoundation")
endif()

set_target_properties(zt PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Program"
)
