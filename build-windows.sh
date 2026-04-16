#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-"$script_dir/build-windows"}"
config="${CONFIG:-RelWithDebInfo}"
run_after_build=0
cmake_args=()
app_args=()

usage() {
  cat <<'USAGE'
Usage: ./build-windows.sh [--run] [--clean] [--debug|--release] [--] [app args...]

Build zTracker for 64-bit Windows with CMake.

This script is intended for Git Bash/MSYS/MinGW64 on Windows. By default it
uses CMake's "MinGW Makefiles" generator unless another generator is supplied
with -G.

Environment:
  BUILD_DIR   Override the build directory.
  CONFIG      Override the CMake build type/configuration.
  MINGW_ROOT  Override the 64-bit MinGW root passed to CMake.
USAGE
}

generator_supplied=0

while (($#)); do
  case "$1" in
    --run)
      run_after_build=1
      shift
      ;;
    --clean)
      rm -rf "$build_dir"
      shift
      ;;
    --debug)
      config="Debug"
      shift
      ;;
    --release)
      config="Release"
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      app_args=("$@")
      break
      ;;
    -G*)
      generator_supplied=1
      cmake_args+=("$1")
      shift
      ;;
    -D*|-C*|-U*|-A*)
      cmake_args+=("$1")
      shift
      ;;
    *)
      app_args+=("$1")
      shift
      ;;
  esac
done

case "$(uname -s)" in
  MINGW64*|MSYS*|CYGWIN*)
    ;;
  MINGW32*)
    echo "build-windows.sh needs a 64-bit Windows shell/toolchain. Use MinGW64/MSYS2 MinGW64, not MinGW32." >&2
    exit 1
    ;;
  *)
    echo "build-windows.sh is intended to run from Git Bash/MSYS/MinGW64 on Windows." >&2
    exit 1
    ;;
esac

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is required." >&2
  exit 1
fi

configure_args=(
  -S "$script_dir"
  -B "$build_dir"
)

if ! ((generator_supplied)); then
  configure_args+=(-G "MinGW Makefiles")
fi

configure_args+=(-DCMAKE_BUILD_TYPE="$config")

if [[ -n "${MINGW_ROOT:-}" ]]; then
  configure_args+=("-DMINGW_ROOT=$MINGW_ROOT")
else
  configure_args+=("-DMINGW_ROOT=W:/MINGW64")
fi

if ((${#cmake_args[@]})); then
  configure_args+=("${cmake_args[@]}")
fi

cmake "${configure_args[@]}"
cmake --build "$build_dir" --config "$config" --parallel

binary_path="$build_dir/Program/zt.exe"
if [[ ! -x "$binary_path" ]]; then
  echo "Build completed, but expected executable was not found at: $binary_path" >&2
  exit 1
fi

echo "Built: $binary_path"

if ((run_after_build)); then
  if ((${#app_args[@]})); then
    "$binary_path" "${app_args[@]}"
  else
    "$binary_path"
  fi
fi
