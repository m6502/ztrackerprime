#!/bin/bash
# Cross-compile zTracker for Windows (64-bit) from macOS or Linux.
# Produces: build-win64/src/zt.exe + SDL.dll
# Prerequisites (macOS): brew install mingw-w64 ninja cmake
# Prerequisites (Linux): sudo apt install mingw-w64 ninja-build cmake
set -e
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

if ! command -v cmake &>/dev/null; then echo "Install cmake"; exit 1; fi
if ! command -v ninja &>/dev/null; then echo "Install ninja"; exit 1; fi
if ! command -v x86_64-w64-mingw32-g++ &>/dev/null; then echo "Install mingw-w64: brew install mingw-w64"; exit 1; fi

cmake -S . -B build-win64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-xp-toolchain.cmake \
  -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  2>&1 | grep -E "error|Configuring done" | tail -1
cmake --build build-win64 --target zt 2>&1 | tail -3
echo "--- built: build-win64/src/zt.exe ---"
ls -lh build-win64/src/zt.exe
file build-win64/src/zt.exe
