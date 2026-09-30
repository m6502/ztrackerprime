#!/usr/bin/env python3
"""Run production text/grid rendering against reference output, without MIDI.

Requires SDL3 development files and pkg-config. --sanitize enables ASan/UBSan;
--benchmark prints timings after the pixel/string assertions pass.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import ROOT, function

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--benchmark', action='store_true')
args = parser.parse_args()
prefix = '#include "zt.h"\n#include "pattern_note_format.h"\n'
prefix += '#define Screen_Pitch ((int)(S->surface->pitch / (int)sizeof(TColor)))\n'
runtime = prefix
for path, signatures in {
    'src/module.cpp': ['event::event(void)', 'event::~event(void)',
                       'track::track(short int len)', 'track::~track(void)',
                       'void track::reset(void)', 'event* track::get_event(',
                       'pattern::pattern(void)', 'pattern::pattern(int len)',
                       'pattern::~pattern(void)'],
    'src/font.cpp': ['void printBG(', 'void printchar(', 'void printline('],
    'src/main.cpp': ['char *hex2note('],
    'src/CUI_Patterneditor.cpp': ['char *printNote('],
}.items():
    runtime += ''.join(function(path, s) for s in signatures)
runtime += function('src/PatternDisplay.cpp', 'char *PatternDisplay::printNote(').replace(
    'char *PatternDisplay::printNote(char *str, event *r)',
    'char *playback_note(char *str, event *r, int cur_pat_view)', 1)
drawable = prefix + ''.join(function('src/lc_sdl_wrapper.cpp', s) for s in [
    'Drawable::Drawable(int w,', 'Drawable::~Drawable()', 'TColor* Drawable::getLine(',
    'long Drawable::fillRect ('])
grid = prefix + '#include "pattern_draw_cache.h"\n#include "track_color.h"\n#include "ccizer.h"\n'
grid += '#define shade_color zt_track_shade\n#define text_for_bg zt_track_text_for_bg\n'
grid += 'extern PatternDrawCache g_pattern_draw_cache;\nextern int PATTERN_EDIT_ROWS;\n'
grid += 'extern int g_posx_tracks, max_displayable_rows;\n'
grid += 'char *printNote(char *, event *, int);\n'
source = (ROOT / 'src/CUI_Patterneditor.cpp').read_text()
grid += '\n'.join(re.findall(
    r'^#define (?:LEFT_MARGIN|TRACKS_POS_Y|TRACKS_FIRST_NOTE_POS_Y)\s+[^\n]+', source, re.M)) + '\n'
for signature in ['static int pattern_center_off(', 'static int track_header_toggle_column(',
                  'void draw_track_markers(', 'void disp_pattern(']:
    grid += function('src/CUI_Patterneditor.cpp', signature)

with tempfile.TemporaryDirectory(prefix='zt-drawing-test-') as tmp:
    tmp = Path(tmp)
    for name, contents in [('runtime', runtime), ('drawable', drawable), ('grid', grid)]:
        (tmp / (name + '.cpp')).write_text(contents)
    sdl = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'sdl3'], text=True))
    include = subprocess.check_output(['pkg-config', '--variable=includedir', 'sdl3'], text=True).strip()
    flags = ['-O3']
    if args.sanitize:
        # No polymorphic UI objects are instantiated by this harness; their
        # vtables/typeinfo live in UI functions deliberately not linked here.
        flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize=vptr',
                 '-fno-omit-frame-pointer']
    command = shlex.split(os.environ.get('CXX', 'c++')) + ['-std=c++17'] + flags + [
        '-I' + str(ROOT / 'src'), '-I' + str(Path(include) / 'SDL3'),
        str(tmp / 'runtime.cpp'), str(tmp / 'drawable.cpp'), str(tmp / 'grid.cpp'),
        str(ROOT / 'src/edit_cols.cpp'), str(ROOT / 'tests/test_drawing.cpp'),
        str(ROOT / 'tests/fixtures/drawing_reference.cpp'), '-o', str(tmp / 'test')] + sdl
    subprocess.run(command, check=True)
    subprocess.run([str(tmp / 'test'), str(ROOT / 'skins/default/font.fnt')] +
                   (['--benchmark'] if args.benchmark else []), check=True)
