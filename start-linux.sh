#!/bin/bash
# Build and run zTracker on Linux.
# Prerequisites: sudo apt install cmake ninja-build libsdl1.2-dev libasound2-dev
#            or: sudo dnf install cmake ninja-build SDL-devel alsa-lib-devel
set -e
cd "$(dirname "$0")"

if ! command -v cmake &>/dev/null; then echo "Install cmake: sudo apt install cmake"; exit 1; fi
if ! command -v ninja &>/dev/null; then echo "Install ninja: sudo apt install ninja-build"; exit 1; fi
if ! pkg-config --exists sdl 2>/dev/null; then echo "Install SDL 1.2: sudo apt install libsdl1.2-dev"; exit 1; fi

cmake -S . -B build-linux -G Ninja 2>&1 | grep -E "error|Configuring done" | tail -1
cmake --build build-linux --target zt 2>&1 | tail -3
echo "--- launching zTracker ---"
cd build-linux/src && ./zt
