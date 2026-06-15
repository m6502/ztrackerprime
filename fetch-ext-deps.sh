#!/usr/bin/env bash

# fetch any additional large or optional dependencies, like ableton link

set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
extlibs="$root/extlibs"

scripts=(
  fetch-ableton-link.sh
)

for s in "${scripts[@]}"; do
  echo "=== Running $s ==="
  (cd "$extlibs" && "./$s")
done
exit