#!/bin/bash
# Build and run zTracker on macOS (Apple Silicon + Intel).
# Prerequisites: brew install sdl12-compat ninja cmake
set -e
cd "$(dirname "$0")"
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

if ! command -v cmake &>/dev/null; then echo "Install cmake: brew install cmake"; exit 1; fi
if ! command -v ninja &>/dev/null; then echo "Install ninja: brew install ninja"; exit 1; fi
if ! pkg-config --exists sdl 2>/dev/null; then echo "Install SDL: brew install sdl12-compat"; exit 1; fi

cmake -S . -B build-macos -G Ninja 2>&1 | grep -E "error|Configuring done" | tail -1
cmake --build build-macos --target zt 2>&1 | tail -3
echo "--- launching zTracker ---"
cd build-macos/src && ./zt
