cmake_minimum_required(VERSION 3.20)
project(ztracker
    VERSION 2025.8.15
    DESCRIPTION "zTracker' — Impulse-Tracker-inspired MIDI tracker, cross-platform port"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Out-of-source builds only.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are forbidden. Use: cmake -S . -B build-win32 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-xp-toolchain.cmake")
endif()

# On MinGW cross-compile from macOS, expose the vendored third_party root for find_path/find_library.
if(CMAKE_CROSSCOMPILING AND MINGW)
    list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_SOURCE_DIR}/third_party")
    list(APPEND CMAKE_PREFIX_PATH    "${CMAKE_SOURCE_DIR}/third_party/SDL-1.2.15")
endif()

include(FetchContent)

# ------------------------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------------------------

# --- SDL 1.2 (vendored dev pack for MinGW cross; Homebrew libsdl12-compat for native macOS)
if(CMAKE_CROSSCOMPILING AND MINGW)
    set(SDL12_ROOT "${CMAKE_SOURCE_DIR}/third_party/SDL-1.2.15")
    set(SDL12_INCLUDE_DIR "${SDL12_ROOT}/include/SDL")
    set(SDL12_LIB_DIR     "${SDL12_ROOT}/lib")
    set(SDL12_LIBRARIES
        "${SDL12_LIB_DIR}/libSDLmain.a"
        "${SDL12_LIB_DIR}/libSDL.dll.a"
    )
    set(SDL12_RUNTIME_DLL "${SDL12_ROOT}/bin/SDL.dll")
    if(NOT EXISTS "${SDL12_INCLUDE_DIR}/SDL.h")
        message(FATAL_ERROR "SDL 1.2 dev pack not found at ${SDL12_ROOT}. Run: cd third_party && curl -LO https://www.libsdl.org/release/SDL-devel-1.2.15-mingw32.tar.gz && tar xf SDL-devel-1.2.15-mingw32.tar.gz")
    endif()
else()
    # Native build (macOS / Linux) — find SDL 1.2 via pkg-config or Homebrew.
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL12 sdl)
    endif()
    if(NOT SDL12_FOUND)
        message(WARNING "SDL 1.2 not found via pkg-config. On macOS: brew install sdl12-compat (or libsdl).")
    endif()
    set(SDL12_INCLUDE_DIR ${SDL12_INCLUDE_DIRS})
    set(SDL12_LIBRARIES   ${SDL12_LIBRARIES})
endif()

# --- zlib: FetchContent from upstream. The vendored MinGW dev pack would be cleaner but
# cross-compiling it via FetchContent keeps the build reproducible.
FetchContent_Declare(zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
    GIT_SHALLOW    TRUE
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zlib)
# zlib's CMakeLists builds both shared and static. We want static for a self-contained .exe.
# It exports target `zlibstatic`.

# --- libpng: depends on zlib. We pre-create a ZLIB::ZLIB imported alias so
# libpng's find_package(ZLIB) picks up our in-tree zlibstatic instead of trying
# to locate a system zlib (which doesn't exist for the MinGW cross target).
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()
set(ZLIB_FOUND TRUE)
set(ZLIB_INCLUDE_DIR  "${zlib_SOURCE_DIR};${zlib_BINARY_DIR}")
set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}")
set(ZLIB_LIBRARY      ZLIB::ZLIB)
set(ZLIB_LIBRARIES    ZLIB::ZLIB)
set(ZLIB_VERSION_STRING "1.3.1")
set(ZLIB_VERSION        "1.3.1")

set(PNG_BUILD_ZLIB   OFF CACHE BOOL "" FORCE)
set(PNG_SHARED       OFF CACHE BOOL "" FORCE)
set(PNG_STATIC       ON  CACHE BOOL "" FORCE)
set(PNG_EXECUTABLES  OFF CACHE BOOL "" FORCE)
set(PNG_TESTS        OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS        OFF CACHE BOOL "" FORCE)
# We never install libpng; skip install rules to avoid export-set complaints.
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(libpng
    GIT_REPOSITORY https://github.com/pnggroup/libpng.git
    GIT_TAG        v1.6.44
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(libpng)

# ------------------------------------------------------------------------------------------------
# Main target
# ------------------------------------------------------------------------------------------------

add_subdirectory(src)
