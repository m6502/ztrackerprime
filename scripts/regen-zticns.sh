#!/usr/bin/env bash
#
# Regenerate the full-size zt.icns from the Icon Composer source
# (assets/icon/zt.icon). Run this after adjusting the icon, then commit zt.icns.
#
# Why this exists: actool's own .icon -> .icns output is hardcoded to a sparse
# set (ic04/ic07/ic11/ic13 only) regardless of --minimum-deployment-target, so
# older macOS / non-actool builds (e.g. CI's Mojave universal artifact) render
# only those sizes and fall back to a generic icon at the rest. This takes
# actool's largest render (256px) and builds a complete classic iconset
# (16..1024, all ic* types). The 512/1024 slices are upscaled from the 256px
# the source authors at -- to make those crisp, give the .icon higher-res
# source art (the leaf, Path.png, is only 66px).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ICON="$ROOT/assets/icon/zt.icon"
ACTOOL="$(xcrun --find actool)"
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT

"$ACTOOL" "$ICON" --app-icon zt --compile "$TMP" --platform macosx \
  --minimum-deployment-target 10.13 \
  --output-partial-info-plist "$TMP/p.plist" >/dev/null

iconutil -c iconset "$TMP/zt.icns" -o "$TMP/cur.iconset"
M="$TMP/cur.iconset/icon_128x128@2x.png"   # actool's largest render: 256px
IS="$TMP/full.iconset"; mkdir -p "$IS"
gen() { sips -z "$2" "$2" "$M" --out "$IS/$1" >/dev/null; }
gen icon_16x16.png 16;        gen icon_16x16@2x.png 32
gen icon_32x32.png 32;        gen icon_32x32@2x.png 64
gen icon_128x128.png 128;     gen icon_128x128@2x.png 256
gen icon_256x256.png 256;     gen icon_256x256@2x.png 512
gen icon_512x512.png 512;     gen icon_512x512@2x.png 1024
iconutil -c icns "$IS" -o "$ROOT/zt.icns"
echo "Regenerated $ROOT/zt.icns ($(stat -f%z "$ROOT/zt.icns") bytes)"
