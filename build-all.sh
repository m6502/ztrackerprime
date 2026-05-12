#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir"

pull=0
forward_args=()
targets=()

usage() {
  cat <<'USAGE'
Usage: ./build-all.sh [--pull] [--clean] [--debug|--release] [--run]
                     [--mac] [--linux] [--win32] [--windows]
                     [-- app args...]

Build zTracker for every target.

  --pull              git pull --ff-only before building.
  --clean/--debug/    Forwarded verbatim to each underlying build-*.sh.
    --release/--run

  --mac --linux       Restrict to specific targets. If none are passed,
    --win32 --windows ALL four are attempted (mac + linux + win32 + windows).
                      Targets whose toolchain is unavailable on this host
                      will fail loudly; the script continues with the rest
                      and reports the failing set at the end.

Anything after `--` is forwarded to each build script (mainly useful with --run).
USAGE
}

while (($#)); do
  case "$1" in
    --pull)    pull=1; shift ;;
    --mac)     targets+=("mac"); shift ;;
    --linux)   targets+=("linux"); shift ;;
    --win32)   targets+=("win32"); shift ;;
    --windows) targets+=("windows"); shift ;;
    --help|-h) usage; exit 0 ;;
    --)        shift; forward_args+=("--" "$@"); break ;;
    *)         forward_args+=("$1"); shift ;;
  esac
done

if ((${#targets[@]} == 0)); then
  targets=("mac" "linux" "win32" "windows")
fi

if ((pull)); then
  echo "==> git pull --ff-only"
  git pull --ff-only
fi

failed=()
for t in "${targets[@]}"; do
  script="./build-$t.sh"
  if [[ ! -x "$script" ]]; then
    echo "==> SKIP $t: $script not found or not executable" >&2
    failed+=("$t")
    continue
  fi
  echo "==> $script ${forward_args[*]:-}"
  if ! "$script" "${forward_args[@]}"; then
    echo "==> FAIL $t" >&2
    failed+=("$t")
  fi
done

if ((${#failed[@]})); then
  echo "build-all.sh: failed targets: ${failed[*]}" >&2
  exit 1
fi

echo "build-all.sh: built ${targets[*]}"
