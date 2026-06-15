#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Clean build directories

build_dir_mac="${BUILD_DIR:-"$script_dir/build-mac"}"
build_dir_linux="${BUILD_DIR:-"$script_dir/build-linux"}"
build_dir_win32="${BUILD_DIR:-"$script_dir/build-win32"}"
build_dir_windows="${BUILD_DIR:-"$script_dir/build-windows"}"
build_dir_dist="${BUILD_DIR:-"$script_dir/dist"}"

rm -rf "$build_dir_mac"
rm -rf "$build_dir_linux"
rm -rf "$build_dir_win32"
rm -rf "$build_dir_windows"
rm -rf "$build_dir_dist"

# Clean CMake artifacts

cmake_build_dirs="${BUILD_DIR:-"$script_dir/cmake-build"}"
rm -rf "$cmake_build_dirs-*"

rm -rf "$script_dir/Makefile"
rm -rf "$script_dir/cmake_install.cmake"
rm -rf "$script_dir/CTestTestfile.cmake"
rm -rf "$script_dir/compile_commands.json"
