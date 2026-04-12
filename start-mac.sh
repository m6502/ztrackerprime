#!/bin/bash
# Build and run zTracker on macOS.
cd "$(dirname "$0")"
PATH="/opt/homebrew/bin:$PATH"
cmake -S . -B build-macos -G Ninja 2>&1 | tail -1
cmake --build build-macos --target zt 2>&1 | tail -3
cd build-macos/src && ./zt
