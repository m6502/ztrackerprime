#!/usr/bin/env bash
# fetch-ableton-link.sh
#
# Populate extlibs/link/ with the Ableton Link source tree that CMakeLists.txt
# links against when ZT_ENABLE_ABLETON_LINK is ON. Like libpng/zlib (see
# fetch-extlibs.sh), the Link sources are NOT committed to this repo -- CI (and
# a local developer who wants Link) fetches them on demand.
#
# Link bundles asio as a git submodule (modules/asio-standalone), so this MUST
# be a recursive clone, not a flat tarball -- the tarball does not include
# submodule contents and the build would fail with missing <asio.hpp>.
#
# Pinned to a release tag so builds are reproducible. Bump LINK_VERSION to
# move to a newer Link.
#
# Env vars (with sane defaults):
#   LINK_VERSION   default Link-4.0   (a tag in github.com/Ableton/link)
#
# Layout produced:
#   link/AbletonLinkConfig.cmake
#   link/include/ableton/Link.hpp
#   link/modules/asio-standalone/asio/include/asio.hpp

set -euo pipefail

: "${LINK_VERSION:=Link-4.0}"

root="$(pwd)"
dest="$root/link"
repo="https://github.com/Ableton/link.git"

# If a valid tree is already present (developer already fetched, or a cached
# CI checkout), don't re-clone.
if [ -f "$dest/AbletonLinkConfig.cmake" ] \
   && [ -f "$dest/include/ableton/Link.hpp" ] \
   && [ -f "$dest/modules/asio-standalone/asio/include/asio.hpp" ]; then
  echo "=== Ableton Link already present ($LINK_VERSION); skipping fetch ==="
  exit 0
fi

echo "=== Fetching Ableton Link $LINK_VERSION (with submodules) ==="
rm -rf "$dest"

ok=0
for attempt in 1 2 3; do
  echo "  clone attempt $attempt ..."
  if git clone --depth 1 --branch "$LINK_VERSION" \
        --recurse-submodules --shallow-submodules \
        "$repo" "$dest"; then
    ok=1
    break
  fi
  rm -rf "$dest"
  sleep 5
done

if [ "$ok" -ne 1 ]; then
  echo "ERROR: failed to clone Ableton Link after 3 attempts" >&2
  exit 1
fi

required=(
  link/AbletonLinkConfig.cmake
  link/include/ableton/Link.hpp
  link/modules/asio-standalone/asio/include/asio.hpp
)
missing=0
for f in "${required[@]}"; do
  if [ ! -f "$root/$f" ]; then
    echo "  MISSING: $f" >&2
    missing=1
  fi
done
if [ "$missing" -ne 0 ]; then
  echo "ERROR: required Ableton Link files missing after clone" >&2
  exit 1
fi

echo "=== Ableton Link OK ($LINK_VERSION) ==="
