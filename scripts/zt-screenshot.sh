#!/bin/bash
# Launches zt.app, presses each navigation key, screenshots zt's window only.
# Uses AppleScript to read window position + size, then screencapture -R.

set -e

APP="${1:-$HOME/work/ztrackerprime-pr-l/build/Program/zt.app}"
OUT="${2:-/tmp/zt-shots}"

mkdir -p "$OUT"
rm -f "$OUT"/*.png

pkill -9 -f "zt.app/Contents/MacOS/zt" 2>/dev/null || true
sleep 0.5
open "$APP"
sleep 2

press_key() {
  osascript -e 'tell application "System Events" to tell process "zt" to set frontmost to true' >/dev/null 2>&1
  sleep 0.3
  osascript -e "tell application \"System Events\" to key code $1" >/dev/null 2>&1
  sleep 0.5
}

press_key_mod() {
  osascript -e 'tell application "System Events" to tell process "zt" to set frontmost to true' >/dev/null 2>&1
  sleep 0.3
  osascript -e "tell application \"System Events\" to key code $1 using $2" >/dev/null 2>&1
  sleep 0.5
}

capture_zt() {
  local outfile="$1"
  osascript -e 'tell application "System Events" to tell process "zt" to set frontmost to true' >/dev/null 2>&1
  sleep 0.3
  local pos=$(osascript -e 'tell application "System Events" to tell process "zt" to get position of window 1' 2>/dev/null)
  local sz=$(osascript -e 'tell application "System Events" to tell process "zt" to get size of window 1' 2>/dev/null)
  if [ -n "$pos" ] && [ -n "$sz" ]; then
    local x=$(echo "$pos" | awk -F', *' '{print $1}')
    local y=$(echo "$pos" | awk -F', *' '{print $2}')
    local w=$(echo "$sz" | awk -F', *' '{print $1}')
    local h=$(echo "$sz" | awk -F', *' '{print $2}')
    screencapture -x -R "${x},${y},${w},${h}" "$outfile" 2>/dev/null
    echo "  rect=${x},${y},${w},${h}"
  else
    screencapture -x "$outfile"
    echo "  fallback fullscreen"
  fi
}

for entry in "122:f1" "120:f2" "99:f3" "103:f11" "111:f12"; do
  code="${entry%%:*}"
  name="${entry##*:}"
  echo "$name (key $code):"
  press_key "$code"
  capture_zt "$OUT/$name.png"
done

echo "ctrl-f12:"
press_key_mod 111 "control down"
capture_zt "$OUT/ctrl-f12.png"

pkill -9 -f "zt.app/Contents/MacOS/zt" 2>/dev/null || true

echo
ls -la "$OUT"
