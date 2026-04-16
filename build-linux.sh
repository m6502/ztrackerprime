#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-"$script_dir/build-linux"}"
config="${CONFIG:-RelWithDebInfo}"
run_after_build=0
cmake_args=()
app_args=()

usage() {
  cat <<'USAGE'
Usage: ./build-linux.sh [--run] [--clean] [--debug|--release] [--] [app args...]

Build zTracker for Linux with CMake.
Requires CMake, a C/C++ compiler, pkg-config, SDL3 development files, and ALSA development files.
USAGE
}

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
    -D*|-G*|-C*|-U*)
      cmake_args+=("$1")
      shift
      ;;
    *)
      app_args+=("$1")
      shift
      ;;
  esac
done

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "build-linux.sh must be run on Linux." >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is required." >&2
  exit 1
fi

if ! pkg-config --exists sdl3 2>/dev/null; then
  echo "SDL3 was not found by pkg-config. Install SDL3 development files or pass SDL3_INCLUDE_DIR/SDL3_LIBRARY." >&2
  exit 1
fi

configure_args=(
  -S "$script_dir"
  -B "$build_dir"
  -DCMAKE_BUILD_TYPE="$config"
)

if ((${#cmake_args[@]})); then
  configure_args+=("${cmake_args[@]}")
fi

cmake "${configure_args[@]}"

cmake --build "$build_dir" --config "$config" --parallel

binary_path="$build_dir/Program/zt"
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
