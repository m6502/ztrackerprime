#!/usr/bin/env python3
"""Reproducible CPU microbenchmark; does not open MIDI devices or a window.

Requires a C++17 compiler and SDL3 development headers/library. Run from any
directory. Compares production text routines with the saved legacy implementation. Reports median
and min/max microseconds per synthetic 8-track x 64-row grid, seven samples.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from drawing_harness import ROOT, function

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--optimization', choices=['O0', 'O2', 'O3'], default='O3')
args = parser.parse_args()
groups = {
    'src/module.cpp': ['event::event(void)', 'event::~event(void)',
                       'track::track(short int len)', 'track::~track(void)',
                       'void track::reset(void)', 'event* track::get_event('],
    'src/main.cpp': ['char *hex2note('],
    'src/CUI_Patterneditor.cpp': ['char *printNote('],
    'src/font.cpp': ['void printBG(', 'void printchar('],
}
baseline = '#include "zt.h"\n#include "pattern_note_format.h"\nunsigned char font[256 * 8];\nevent blank_event;\n'
baseline += '#define Screen_Pitch ((int)(S->surface->pitch / (int)sizeof(TColor)))\n'
baseline += ''.join(function(p, s) for p, signatures in groups.items() for s in signatures)
# Keep getLine out of the font translation unit, just as in the application.
drawable = '#include "zt.h"\n' + ''.join(function('src/lc_sdl_wrapper.cpp', s) for s in [
    'Drawable::Drawable(int w,', 'Drawable::~Drawable()', 'TColor* Drawable::getLine('])

with tempfile.TemporaryDirectory(prefix='zt-drawing-bench-') as tmp:
    tmp = Path(tmp)
    (tmp / 'baseline.cpp').write_text(baseline)
    (tmp / 'drawable.cpp').write_text(drawable)
    cxx = shlex.split(os.environ.get('CXX', 'c++'))
    sdl = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'sdl3'], text=True))
    sdl_include = Path(subprocess.check_output(
        ['pkg-config', '--variable=includedir', 'sdl3'], text=True).strip())
    command = cxx + ['-std=c++17', '-' + args.optimization,
                     '-I' + str(ROOT / 'src'), '-I' + str(sdl_include / 'SDL3'),
                     str(tmp / 'baseline.cpp'), str(tmp / 'drawable.cpp'),
                     str(ROOT / 'tools/benchmark_drawing.cpp'),
                     str(ROOT / 'tools/benchmark_drawing_candidates.cpp'),
                     str(ROOT / 'tests/fixtures/drawing_reference.cpp'),
                     '-o', str(tmp / 'bench')] + sdl
    print('Compile:', shlex.join(command), flush=True)
    subprocess.run(command, check=True)
    subprocess.run([str(tmp / 'bench'), str(ROOT / 'skins/default/font.fnt')], check=True)
