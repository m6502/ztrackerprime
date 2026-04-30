#!/bin/bash
# macOS-ONLY: Launches zt.app, presses navigation keys, screenshots zt's
# window only. Uses `screencapture -l <window-id>` (queried via Quartz)
# rather than AppleScript's `position/size of window 1`, because SDL3
# windows on macOS don't always show up in System Events' window list
# -- the original AppleScript path returned "Invalid index. (-1719)"
# in some configurations and silently fell back to fullscreen.
#
# Audit P17: visual verification of the CCizer / SysEx pages was the
# gap I left across PRs #78-#84. Solving the window-detection issue
# unblocks layout-bug catching at PR-time instead of review-time.

if [ "$(uname -s)" != "Darwin" ]; then
  echo "zt-screenshot.sh requires macOS (uses CoreGraphics + screencapture)."
  echo "For Linux try xdotool + scrot; for Windows try AutoHotkey + nircmd."
  exit 1
fi

set -e

# Default to the in-tree build path; override with arg 1.
APP="${1:-$(cd "$(dirname "$0")/.." && pwd)/build/Program/zt.app}"
OUT="${2:-/tmp/zt-shots}"

if [ ! -e "$APP" ]; then
  echo "App not found: $APP"
  echo "Usage: $0 [path/to/zt.app] [output_dir]"
  exit 1
fi

mkdir -p "$OUT"
rm -f "$OUT"/*.png

pkill -9 -f "zt.app/Contents/MacOS/zt" 2>/dev/null || true
sleep 0.5
open "$APP"
sleep 2

# Wait up to 5 s for the SDL window to appear. We poll Quartz via a
# tiny Swift snippet (`/usr/bin/swift` ships with the Xcode Command
# Line Tools, present on every dev machine). The previous AppleScript
# `position/size of window 1` path returned "Invalid index. (-1719)"
# for SDL3 windows because they don't always show up in System Events'
# AppKit-only window list -- but they ARE visible to CGWindowList* via
# Quartz, which is what `screencapture -l` consumes anyway.
#
# The window's owner name is "zTracker" (the .app bundle's display
# name from CFBundleName / Info.plist), NOT "zt" (the executable
# name). Using the right value is half the battle.
get_zt_window_id() {
swift - 2>/dev/null <<'SWIFT'
import Cocoa
let opts: CGWindowListOption = [.optionOnScreenOnly]
guard let infos = CGWindowListCopyWindowInfo(opts, kCGNullWindowID) as? [[String: Any]] else {
    exit(1)
}
for w in infos {
    let owner = w[kCGWindowOwnerName as String] as? String ?? ""
    if owner == "zTracker" || owner == "zt" {
        let num = w[kCGWindowNumber as String] as? Int ?? 0
        let b = w[kCGWindowBounds as String] as? [String: Any] ?? [:]
        let width = (b["Width"] as? Double) ?? 0
        let height = (b["Height"] as? Double) ?? 0
        if width > 100 && height > 100 {
            print(num)
            exit(0)
        }
    }
}
exit(1)
SWIFT
}

WIN_ID=""
for _ in 1 2 3 4 5 6 7 8 9 10; do
  WIN_ID=$(get_zt_window_id || true)
  [ -n "$WIN_ID" ] && break
  sleep 0.5
done

if [ -z "$WIN_ID" ]; then
  echo "Could not locate zt SDL window via Quartz."
  echo "Fallback: full-screen capture."
fi

press_key() {
  # System Events still works for keystrokes (it's only the window-list
  # API that's flaky for SDL windows).
  osascript -e 'tell application "zTracker" to activate' >/dev/null 2>&1 || true
  sleep 0.2
  osascript -e "tell application \"System Events\" to key code $1" >/dev/null 2>&1
  sleep 0.5
}

press_key_mod() {
  osascript -e 'tell application "zTracker" to activate' >/dev/null 2>&1 || true
  sleep 0.2
  osascript -e "tell application \"System Events\" to key code $1 using $2" >/dev/null 2>&1
  sleep 0.5
}

capture_zt() {
  local outfile="$1"
  osascript -e 'tell application "zTracker" to activate' >/dev/null 2>&1 || true
  sleep 0.2
  if [ -n "$WIN_ID" ]; then
    screencapture -x -l "$WIN_ID" "$outfile" 2>/dev/null
    echo "  window-id=$WIN_ID"
  else
    screencapture -x "$outfile"
    echo "  fallback fullscreen"
  fi
}

# F-key codes (same as before): F1=122, F2=120, F3=99, F5=96, F11=103, F12=111
for entry in "122:f1" "120:f2" "99:f3" "103:f11" "111:f12"; do
  code="${entry%%:*}"
  name="${entry##*:}"
  echo "$name (key $code):"
  press_key "$code"
  capture_zt "$OUT/$name.png"
done

# Ctrl+F12 = Global Config
echo "ctrl-f12:"
press_key_mod 111 "control down"
capture_zt "$OUT/ctrl-f12.png"

# Shift+F2 = Shortcuts & MIDI Mappings (post-PR-#77 reshuffle)
echo "shift-f2 (Shortcuts & MIDI Mappings):"
press_key_mod 120 "shift down"
capture_zt "$OUT/shift-f2.png"

# Shift+F3 = CC Console (PR #78)
echo "shift-f3 (CC Console):"
press_key_mod 99 "shift down"
capture_zt "$OUT/shift-f3.png"

# Shift+F5 = SysEx Librarian (PR #83)
echo "shift-f5 (SysEx Librarian):"
press_key_mod 96 "shift down"
capture_zt "$OUT/shift-f5.png"

pkill -9 -f "zt.app/Contents/MacOS/zt" 2>/dev/null || true

echo
ls -la "$OUT"
