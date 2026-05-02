#!/usr/bin/env python3
"""
Convert Schism Tracker's palettes.c into zTracker' palette .conf files.

Schism stores palettes as 16 entries of 6-bit RGB (0-63 range, classic
VGA). zTracker' palettes have 18 named slots (Background, Highlight,
Lowlight, Text, Data, Black, EditText, EditBG, EditBGlow, EditBGhigh,
Brighttext, SelectedBGLow, SelectedBGHigh, LCDHigh, LCDMid, LCDLow,
CursorRowHigh, CursorRowLow) so the two formats don't 1:1 convert.

This script picks reasonable defaults for the slot mapping based on
how Schism's color indices are used across pages (text 0/3, frame
1/2, status 4/5/6, edit grid 7-11, selection / cursor 12-15). The
output is a starting point that may need hand-tuning per palette --
LCDHigh in particular tends to want a distinct highlight color that
the source 16-slot palette doesn't always provide.

Usage:
    python3 scripts/import-schism-palettes.py \\
        ~/work/schismtracker/schism/palettes.c \\
        palettes/

Existing files in the destination are NOT overwritten -- the seven
hand-tuned palettes that already ship (camouflage, gold, light_blue,
midnight_tracking, pine_colours, soundtracker, volcanic) are left
alone. To force a re-import, pass --overwrite.
"""

import argparse
import os
import re
import sys


# zT slot order in the .conf file, anchored to one or two Schism indices.
# Where two are listed, we average them. The mapping is empirical -- it
# produces output that's visually consistent across the bundled Schism
# palettes; tweak as needed once you eyeball the result.
SLOT_MAP = [
    ("Background",     [1]),
    ("Highlight",      [3]),
    ("Lowlight",       [2]),
    ("Text",           [0]),
    ("Data",           [5]),
    ("Black",          [0]),
    ("EditText",       [11]),
    ("EditBG",         [0]),
    ("EditBGlow",      [9]),
    ("EditBGhigh",     [10]),
    ("Brighttext",     [11]),
    ("SelectedBGLow",  [12]),
    ("SelectedBGHigh", [13]),
    ("LCDHigh",        [5]),
    ("LCDMid",         [9, 10]),
    ("LCDLow",         [8]),
    ("CursorRowHigh",  [14]),
    ("CursorRowLow",   [15]),
]


def parse_palettes(path):
    """
    Returns [(name, [(r,g,b) ...16]), ...] from Schism's palettes.c.
    Each RGB triplet stays in 0-63 range.
    """
    with open(path) as f:
        text = f.read()

    # Strip C comments first so inline /* ... */ inside palette bodies
    # (e.g. `{"Gold", { /* hey, this is the ST3 palette! */`) doesn't
    # break the structural pattern.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)

    # `{"Name", { {r,g,b}, ... 16x ..., }},` -- name capture, then any
    # body up to the matching outer braces. We re-parse the body for
    # triplets to count them.
    name_pat = re.compile(r'\{\s*"([^"]+)"\s*,\s*\{')
    triplet_pat = re.compile(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}')

    palettes = []
    for m in name_pat.finditer(text):
        name = m.group(1)
        # Walk forward from the opening brace of the inner array,
        # collecting matching braces until we close it.
        i = m.end()
        depth = 1
        while i < len(text) and depth > 0:
            c = text[i]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            i += 1
        body = text[m.end():i - 1]
        triplets = triplet_pat.findall(body)
        if len(triplets) != 16:
            continue
        rgb = [(int(r), int(g), int(b)) for r, g, b in triplets]
        palettes.append((name, rgb))
    return palettes


def to_8bit(c6):
    """6-bit to 8-bit: schism stores 0-63, scale to 0-255 with rounding."""
    return min(255, round(c6 * 255 / 63))


def hex_color(rgb6):
    r, g, b = (to_8bit(c) for c in rgb6)
    return f"#{r:02X}{g:02X}{b:02X}"


def average(rgbs):
    """Average a list of (r,g,b) tuples in 6-bit space."""
    n = len(rgbs)
    return tuple(round(sum(c[i] for c in rgbs) / n) for i in range(3))


def filename_for(name):
    """
    'Camouflage (default)' -> 'camouflage.conf'
    'Why Colors?'          -> 'why_colors.conf'
    'FX 2.0'               -> 'fx_2_0.conf'
    'Gold (Vintage)'       -> 'gold_vintage.conf'
    """
    base = name.lower()
    base = re.sub(r"\s*\(default\)\s*", "", base)
    base = re.sub(r"\s*\(([^)]+)\)\s*", r"_\1", base)
    base = re.sub(r"[^a-z0-9]+", "_", base)
    base = base.strip("_")
    return base + ".conf"


def render_conf(rgb16):
    """Produce a zT-format .conf body from a 16-entry Schism palette."""
    lines = []
    for slot, indices in SLOT_MAP:
        if len(indices) == 1:
            color = rgb16[indices[0]]
        else:
            color = average([rgb16[i] for i in indices])
        # zT' palette parser keys on the trailing ':' so the slot
        # name is matched as a whole word -- without it, load() falls
        # back to defaults silently and the file looks empty.
        lines.append(f"{slot + ':':<16} {hex_color(color)}")
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help="Path to schism's palettes.c")
    ap.add_argument("dest", help="zTracker palettes/ directory")
    ap.add_argument("--overwrite", action="store_true",
                    help="Overwrite existing .conf files (default: skip)")
    ap.add_argument("--skip", action="append", default=["User Defined"],
                    help="Schism palette name to skip (repeatable)")
    args = ap.parse_args()

    palettes = parse_palettes(args.source)
    if not palettes:
        print(f"no palettes parsed from {args.source}", file=sys.stderr)
        return 1

    os.makedirs(args.dest, exist_ok=True)

    written = []
    skipped_existing = []
    skipped_explicit = []
    for name, rgb16 in palettes:
        if name in args.skip:
            skipped_explicit.append(name)
            continue
        fname = filename_for(name)
        path = os.path.join(args.dest, fname)
        if os.path.exists(path) and not args.overwrite:
            skipped_existing.append((name, fname))
            continue
        with open(path, "w") as f:
            f.write(f"# {name} -- imported from Schism Tracker palettes.c\n")
            f.write(render_conf(rgb16))
        written.append((name, fname))

    print(f"wrote {len(written)} palette(s) to {args.dest}:")
    for name, fname in written:
        print(f"  {name:<24}  {fname}")
    if skipped_existing:
        print(f"\nskipped {len(skipped_existing)} existing file(s) "
              f"(use --overwrite to force):")
        for name, fname in skipped_existing:
            print(f"  {name:<24}  {fname}")
    if skipped_explicit:
        print(f"\nexplicitly skipped: {', '.join(skipped_explicit)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
