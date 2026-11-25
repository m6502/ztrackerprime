#!/usr/bin/env python3
"""Jnnnn MIDI export: one straight pass, "[nnnn" at the target order's first
row, "]nnnn" at the START OF THE ROW AFTER the J (where the tracker jumps),
so the J row plays in full and its note-offs come before the marker."""
import subprocess
import sys
import tempfile
from pathlib import Path

from app_fixture import create_test_app

# A scratch copy of the app: zt saves zt.conf beside itself on exit, which
# inside the real macOS bundle breaks its code signature.
_app_dir = tempfile.TemporaryDirectory(prefix='zt-app-')
app, zt = create_test_app(Path(sys.argv[1]), Path(_app_dir.name))


def varlen(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def tracks(d):
    """Yields each MTrk as a list of (abs_tick, kind, payload)."""
    i = 14
    while i < len(d):
        assert d[i:i + 4] == b'MTrk'
        end = i + 8 + int.from_bytes(d[i + 4:i + 8], 'big')
        j, t, run, evs = i + 8, 0, 0, []
        while j < end:
            dt, j = varlen(d, j)
            t += dt
            if d[j] == 0xFF:
                n, k = varlen(d, j + 2)
                evs.append((t, 'meta%02X' % d[j + 1], d[k:k + n]))
                j = k + n
                continue
            if d[j] & 0x80:
                run = d[j]
                j += 1
            n = 1 if run & 0xF0 in (0xC0, 0xD0) else 2
            evs.append((t, '%02X' % (run & 0xF0), bytes(d[j:j + n])))
            j += n
        yield evs
        i = end


with tempfile.TemporaryDirectory(prefix='zt-jump-') as tmp:
    tmp = Path(tmp)
    song, mid = tmp / 'jump.zt', tmp / 'jump.mid'
    # Orders 0,1,2 -> patterns 0,1,2 (64 rows each). Pattern 1: J0001 on
    # row 15 (loop orders 1..1), with a note on that same row. Pattern 2:
    # J0000 on its last row, the last row of the song. Pattern 0: J0003 on row 5,
# an order position past the end of the song.
    (tmp / 'mk.script').write_text(f'''\
lua for p=0,2 do zt.song.cur_pattern = p; local c = zt.cell(0, 0); c.note = 60 + p; c.instrument = 0 end
lua zt.song.cur_pattern = 1; local c = zt.cell(1, 15); c.effect = 0x4A; c.effect_data = 1; c = zt.cell(2, 15); c.note = 72; c.instrument = 0; c.length = 1
lua zt.song.cur_pattern = 2; local c = zt.cell(1, 63); c.effect = 0x4A; c.effect_data = 0
lua zt.song.cur_pattern = 0; local c = zt.cell(3, 5); c.effect = 0x4A; c.effect_data = 3
lua zt.orders[0] = 0; zt.orders[1] = 1; zt.orders[2] = 2; zt.orders[3] = zt.BREAK
lua assert(zt.save("{song}"))
quit
''')
    run = dict(cwd=app, timeout=60, check=True,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([str(zt), '--headless', '--script', str(tmp / 'mk.script')], **run)
    assert song.exists(), 'song was not saved'
    run.update(stderr=subprocess.PIPE, text=True)
    stderr = subprocess.run([str(zt), str(song), '--export-multitrack-midi', str(mid)], **run).stderr

    # J0003 on order 0 row 5 jumps past the end of the song: the tracker does
    # not jump, so no "]0003" is written, and the export says why.
    assert 'J0003 at order 0 row 5 jumps to an order position with no pattern' in stderr, stderr

    all_tracks = list(tracks(mid.read_bytes()))
    markers = [(t, p.decode()) for t, k, p in all_tracks[0] if k == 'meta06']
    notes = [e for tr in all_tracks[1:] for e in tr if e[1] == '90']

    pattern_ticks = next(t for t, k, p in notes if p[0] == 61 and p[1])  # order 1 starts
    row = pattern_ticks // 64
    song_end = 3 * pattern_ticks

    expected = [
        (0, '[0000'),                          # J0000 targets order 0
        (pattern_ticks, '[0001'),              # J0001 targets order 1
        (pattern_ticks + 16 * row, ']0001'),   # row AFTER the J on row 15
        (song_end, ']0000'),                   # J on the last row of the song
    ]
    assert markers == expected, f'markers {markers} != {expected}'

    # The note started on the J row is released no later than the "]".
    off = next(t for t, k, p in notes if p[0] == 72 and p[1] == 0)
    assert off <= pattern_ticks + 16 * row, f'J-row note-off at {off}, after the ]'

    # One straight pass: the export did not follow either jump.
    assert max(t for tr in all_tracks for t, _, _ in tr) <= song_end + 1

print('jump export: markers placed at the jump points, single pass, J-row note released, '
      'J to nowhere reported and not exported')
