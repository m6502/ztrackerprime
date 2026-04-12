# MinGW-w64 i686 cross-compile toolchain producing XP-compatible 32-bit Win32 binaries.
# Used from macOS via: cmake -S . -B build-win32 -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-xp-toolchain.cmake -G Ninja

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Homebrew installs the cross-compilers with this exact prefix.
set(MINGW_TRIPLE i686-w64-mingw32)

set(CMAKE_C_COMPILER   ${MINGW_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_TRIPLE}-g++)
set(CMAKE_RC_COMPILER  ${MINGW_TRIPLE}-windres)
set(CMAKE_AR           ${MINGW_TRIPLE}-ar)
set(CMAKE_RANLIB       ${MINGW_TRIPLE}-ranlib)

# Root for find_library / find_path — target system libs live under the triple sysroot.
# We intentionally don't pin a specific Cellar version; rely on the Homebrew-shimmed wrappers in $PATH.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# XP SP2 target — _WIN32_WINNT / WINVER 0x0501.
# Set on all targets in this build.
add_compile_definitions(_WIN32_WINNT=0x0501 WINVER=0x0501 NTDDI_VERSION=0x05010200)

# Produce self-contained .exe — don't depend on libgcc_s_dw2-1.dll / libstdc++-6.dll / libwinpthread-1.dll at runtime.
# Note: we do NOT pass -static globally because that breaks SDLmain's WinMain linkage.
# Instead we statically link just the GCC/C++ runtimes and winpthread.
add_link_options(-static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)

# PE header subsystem version 5.1 so XP's loader doesn't reject the binary.
add_link_options(-Wl,--major-subsystem-version=5 -Wl,--minor-subsystem-version=1
                 -Wl,--major-os-version=5 -Wl,--minor-os-version=1)
