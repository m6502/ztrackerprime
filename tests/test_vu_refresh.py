#!/usr/bin/env python3
"""Exercise the actual VU constructor/update with deterministic key/player fixtures.

No MIDI device, SDL event loop, timing sleeps, or copied update implementation.
Run with python3 tests/test_vu_refresh.py.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import function

fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdio>
// Windows headers expose a function-like max macro unless NOMINMAX is set.
#define max(a, b) windows_max_macro_must_not_expand(a, b)
using KBKey = int;
constexpr int SDLK_DOWN = 1, SDLK_UP = 2, MAX_TRACKS = 64;
int need_refresh = 0;
struct { int key = 0; int checkkey() { return key; } void getstate() {} } Keys;
struct Player { bool playing = false; int playing_cur_row = 0;
    int playing_cur_pattern = 0, playing_cur_order = 0; } player;
Player *ztPlayer = &player;
struct event { int note = 0; };
class Drawable {};
class UserInterfaceElement { public: int need_redraw = 1; };
'''
fixture += function('src/UserInterface.h', 'struct playedinfo') + ';\n'
fixture += function('src/UserInterface.h', 'class VUPlay :') + ';\n'
fixture += function('src/UserInterface.cpp', 'VUPlay::VUPlay()')
fixture += function('src/UserInterface.cpp', 'int VUPlay::update()')
fixture += r'''
int main() {
    VUPlay vu;
    auto clean = [&] { need_refresh = vu.need_redraw = 0; };
    auto dirty = [&] { assert(need_refresh == 1 && vu.need_redraw == 1); };
    auto quiet = [&] { assert(!need_refresh && !vu.need_redraw); };
    clean();
    for (int i = 0; i < 10000; ++i) vu.update();
    quiet();
    player.playing = true;
    vu.latency[0].longevity = 10;
    vu.update(); dirty(); assert(vu.latency[0].longevity == 8);
    clean();
    for (int i = 0; i < 10000; ++i) vu.update();
    quiet(); assert(vu.latency[0].longevity == 8);
    ++player.playing_cur_row;
    vu.update(); dirty(); assert(vu.latency[0].longevity == 6);
    clean(); ++player.playing_cur_pattern;
    vu.update(); dirty(); assert(vu.latency[0].longevity == 4);
    clean(); ++player.playing_cur_order; // same row, repeated pattern
    vu.update(); dirty(); assert(vu.latency[0].longevity == 2);
    clean(); player.playing_cur_row = 0; // loop backwards
    vu.update(); dirty(); assert(vu.latency[0].longevity == 0);
    clean(); ++player.playing_cur_row;
    vu.update(); dirty(); assert(vu.latency[0].longevity == 0);
    clean(); player.playing = false;
    vu.update(); dirty();
    clean(); vu.update(); quiet();
    player.playing = true; // resume at identical position
    vu.update(); dirty();
    clean(); Keys.key = SDLK_DOWN;
    vu.update(); dirty(); assert(vu.starttrack == 1);
    clean(); Keys.key = SDLK_UP;
    vu.update(); dirty(); assert(vu.starttrack == 0);
    clean(); vu.update(); quiet(); // already at first track
    vu.starttrack = 32; Keys.key = SDLK_DOWN;
    vu.update(); quiet(); // already at last page
    Keys.key = 0;
    ztPlayer = nullptr;
    vu.update(); dirty();
    clean(); vu.update(); quiet();
    puts("VU refresh: stopped/unchanged polling, transport, row/pattern/order, loop, decay, track scroll passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='zt-vu-test-') as tmp:
    source = Path(tmp) / 'vu.cpp'
    source.write_text(fixture)
    binary = Path(tmp) / 'vu-test'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                   ['-std=c++17', '-O2', '-Wall', '-Wextra', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
