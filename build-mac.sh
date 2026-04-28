#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-"$script_dir/build-mac"}"
config="${CONFIG:-RelWithDebInfo}"
run_after_build=0
cmake_args=()
app_args=()

usage() {
  cat <<'USAGE'
Usage: ./build-mac.sh [--run] [--clean] [--debug|--release] [--] [app args...]

Build zTracker for macOS with CMake.

Options:
  --run       Launch build-mac/Program/zt.app after a successful build.
  --clean     Remove build-mac before configuring.
  --debug     Build with CMAKE_BUILD_TYPE=Debug.
  --release   Build with CMAKE_BUILD_TYPE=Release.

Environment:
  BUILD_DIR   Override the build directory.
  CONFIG      Override the CMake build type/configuration.
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

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build-mac.sh must be run on macOS." >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is required. Install it with: brew install cmake" >&2
  exit 1
fi

if ! pkg-config --exists sdl3 2>/dev/null; then
  cat >&2 <<'EOF'
SDL3 was not found by pkg-config.

Install it with:
  brew install sdl3 pkg-config

Or pass explicit CMake paths, for example:
  ./build-mac.sh -DSDL3_INCLUDE_DIR=/opt/homebrew/include -DSDL3_LIBRARY=/opt/homebrew/lib/libSDL3.dylib
EOF
  exit 1
fi

# zlib/libpng are vendored under extlibs/, so we don't need (and don't want)
# any inherited LDFLAGS/CPPFLAGS pointing at /usr/local/opt/* (Intel-Homebrew
# leftovers on Apple Silicon machines). Clearing them avoids the
#   ld: warning: search path '/usr/local/opt/zlib/lib' not found
# warning during link.
unset LDFLAGS CPPFLAGS

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

app_path="$build_dir/Program/zt.app"
binary_path="$app_path/Contents/MacOS/zt"

if [[ ! -x "$binary_path" ]]; then
  echo "Build completed, but expected executable was not found at: $binary_path" >&2
  exit 1
fi

echo "Built: $app_path"

if ((run_after_build)); then
  log_file="${ZT_RUN_LOG:-/tmp/zt-run.log}"
  : > "$log_file"
  echo "Logging stdout/stderr to: $log_file"
  if ((${#app_args[@]})); then
    "$binary_path" "${app_args[@]}" 2>&1 | tee "$log_file" &
  else
    "$binary_path" 2>&1 | tee "$log_file" &
  fi
  echo "Launched: $binary_path"
fi
