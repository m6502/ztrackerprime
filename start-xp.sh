#!/bin/bash
# Cross-compile zTracker for Windows XP 32-bit from macOS or Linux.
# Produces: build-win32/src/zt.exe + SDL.dll (runs on XP SP2 through Win11)
# Prerequisites (macOS): brew install mingw-w64 ninja cmake
# Prerequisites (Linux): sudo apt install mingw-w64 ninja-build cmake
set -e
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

if ! command -v cmake &>/dev/null; then echo "Install cmake"; exit 1; fi
if ! command -v ninja &>/dev/null; then echo "Install ninja"; exit 1; fi
if ! command -v i686-w64-mingw32-g++ &>/dev/null; then echo "Install mingw-w64: brew install mingw-w64"; exit 1; fi

cmake -S . -B build-win32 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-xp-toolchain.cmake \
  2>&1 | grep -E "error|Configuring done" | tail -1
cmake --build build-win32 --target zt 2>&1 | tail -3
echo "--- built: build-win32/src/zt.exe (XP 32-bit) ---"
ls -lh build-win32/src/zt.exe
file build-win32/src/zt.exe
