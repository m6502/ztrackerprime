#!/usr/bin/env python3
"""Keep the stopped playback grid across actual SDL focus/expose repaints."""
from pathlib import Path
import struct
import re
import subprocess
import sys
import tempfile
import zlib
from collections import Counter
from app_fixture import create_test_app


def grid_pixels(path):
    """Decode libpng's 8-bit RGB(A) screenshot, returning just the grid pixels."""
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    offset, packed = 8, bytearray()
    while offset < len(data):
        size = struct.unpack_from('>I', data, offset)[0]
        kind, chunk = data[offset + 4:offset + 8], data[offset + 8:offset + 8 + size]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
            assert depth == 8 and color in (2, 6) and interlace == 0
        elif kind == b'IDAT':
            packed.extend(chunk)
        offset += size + 12
    bpp = 4 if color == 6 else 3
    stride = width * bpp
    raw = zlib.decompress(packed)
    previous = bytearray(stride)
    pixels = bytearray()
    for y in range(height):
        start = y * (stride + 1)
        filter_type = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            a = row[i - bpp] if i >= bpp else 0
            b = previous[i]
            c = previous[i - bpp] if i >= bpp else 0
            if filter_type == 1:
                predict = a
            elif filter_type == 2:
                predict = b
            elif filter_type == 3:
                predict = (a + b) // 2
            elif filter_type == 4:
                p = a + b - c
                distances = (abs(p - a), abs(p - b), abs(p - c))
                predict = (a, b, c)[distances.index(min(distances))]
            else:
                assert filter_type == 0
                predict = 0
            row[i] = (row[i] + predict) & 255
        # Include the headers at row 11, excluding status and toolbar.
        if 88 <= y < height - 64:
            pixels.extend(row[8 * bpp:(width - 8) * bpp])
        previous = row
    return bytes(pixels)


def check_cell(path, column, text, pixels):
    """Check actual on-screen glyphs, including spacing and all effect digits."""
    data = path.read_bytes()
    width = struct.unpack_from('>I', data, 16)[0]
    bpp = 4 if data[25] == 6 else 3
    stride = (width - 16) * bpp
    font = (Path(__file__).resolve().parents[1] / 'skins/default/font.fnt').read_bytes()
    # The second painted row avoids the frame and is stable in this fixture.
    for n, char in enumerate(text):
        x = (column + n) * 8 - 8
        colors, mask = [], []
        for y in range(8):
            glyph = font[ord(char) * 8 + y]
            for bit in range(8):
                offset = (16 + y) * stride + (x + bit) * bpp
                colors.append(pixels[offset:offset + bpp])
                mask.append(bool(glyph & (128 >> bit)))
        background = Counter(c for c, bit in zip(colors, mask) if not bit).most_common(1)[0][0]
        assert all((c != background) == bit for c, bit in zip(colors, mask)), \
            f'{path.name}: character {n} should be {char!r}'


def check_header_gaps(path, size, pixels):
    data = path.read_bytes()
    width = struct.unpack_from('>I', data, 16)[0]
    bpp = 4 if data[25] == 6 else 3
    stride = (width - 16) * bpp
    background = pixels[:bpp]
    for track in range(min(64, (width // 8 - 6) // size)):
        x = (5 + (track + 1) * size - 1) * 8 - 8
        # The bottom pixel row contains the frame's horizontal border.
        for y in range(7):
            for bit in range(8):
                offset = y * stride + (x + bit) * bpp
                assert pixels[offset:offset + bpp] == background, \
                    f'{path.name}: header {track + 1} has no background gap'


def check_track_separators(path, size, pixels):
    data = path.read_bytes()
    width, height = struct.unpack_from('>II', data, 16)
    bpp = 4 if data[25] == 6 else 3
    stride = (width - 16) * bpp
    background = pixels[:bpp]
    # The frame's top horizontal line uses the same Lowlight colour as
    # the pattern editor's separator glyph (168).
    offset = 7 * stride + (5 * 8 - 8) * bpp
    lowlight = pixels[offset:offset + bpp]
    assert lowlight != background
    font = (Path(__file__).resolve().parents[1] / 'skins/default/font.fnt').read_bytes()
    count = min(64, (width // 8 - 6) // size)
    for track in range(count):
        x = (5 + (track + 1) * size - 1) * 8 - 8
        for y in range(8, height - 64 - 88):
            glyph = font[168 * 8 + y % 8] if track < count - 1 else 0
            for bit in range(8):
                offset = y * stride + (x + bit) * bpp
                expected = lowlight if glyph & (128 >> bit) else background
                assert pixels[offset:offset + bpp] == expected, \
                    f'{path.name}: incorrect separator at track {track + 1}, row {y // 8}'


binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-playback-focus-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    (app / 'zt.conf').write_text('skin: default\ndefault_directory: \n')
    commands = ['wait 100', 'key f5', 'wait 70', f'shot {app}/initial-unplayed.png',
                'lua for p=0,1 do for r=0,63 do zt.pattern(p):set_note(0,r,36+p*24+r%12,0,100); zt.pattern(p):set_note(1,r,48+p*12+r%12,1,80) end end; zt.orders[0]=0; zt.orders[1]=1; zt.orders[2]=zt.BREAK']
    comparisons = []

    def key(chord):
        commands.extend([f'key {chord}', 'wait 70'])

    def shot(name):
        commands.append(f'shot {app}/{name}.png')
        return name

    for name, play_key in [('song', 'f5'), ('loop', 'f6')]:
        key(play_key)
        commands.append('wait 200')
        key('f8')
        baseline = shot(name + '-stopped')
        for event in ['focus_gained', 'exposed', 'restored']:
            if event == 'focus_gained':
                commands.extend(['window_event focus_lost', 'wait 30'])
            commands.extend([f'window_event {event}', 'wait 70'])
            comparisons.append((baseline, shot(name + '-' + event)))
        key('pageup'); key('pagedown')
        comparisons.append((baseline, shot(name + '-view-toggle')))
        key('esc'); key('esc')
        comparisons.append((baseline, shot(name + '-popup-close')))

    # Identical rows make field assertions independent of playback timing.
    commands.append('lua for p=0,1 do for r=0,63 do '
                    'for t=0,1 do local c=zt.cell(t,r,p); c.note=60+2*t; '
                    'c.instrument=t; c.volume=100-20*t; c.length=37+5*t; '
                    'c.effect=83; c.effect_data=0x1234 end; '
                    'local c=zt.cell(63,r,p); c.note=69; c.instrument=2; '
                    'c.volume=33; c.length=999; c.effect=78; c.effect_data=0xbeef end end')
    key('f5'); key('f8')
    cells = []
    for mode, size, fields in [(1, 8, ['C-5 00S', 'D-5 01S']),
                               (2, 14, ['C-5 00 64 037', 'D-5 01 50 042']),
                               (3, 20, ['C-5 00 64 037 S1234', 'D-5 01 50 042 S1234'])]:
        baseline = shot(f'mode-{mode}')
        cells.extend((baseline, 5 + t * size, text) for t, text in enumerate(fields))
        commands.extend(['window_event focus_gained', 'wait 70'])
        comparisons.append((baseline, shot(f'mode-{mode}-focus')))
        for _ in range(4 - mode):
            key('pagedown')
        shot(f'mode-{mode}-vu')
        for _ in range(4 - mode):
            key('pageup')
        comparisons.append((baseline, shot(f'mode-{mode}-vu-toggle')))
        # Forward/backward switching must erase the old wider columns.
        key('pagedown'); key('pageup')
        comparisons.append((baseline, shot(f'mode-{mode}-roundtrip')))
        key('pagedown')
    key('pagedown')  # VU -> Compact.
    comparisons.append(('mode-1', shot('compact-wrap')))
    key('pageup')  # Compact -> VU, wrapping backwards.
    comparisons.append(('mode-3-vu', shot('vu-wrap')))
    key('pageup')  # VU -> Full.
    comparisons.append(('mode-3', shot('full-wrap')))
    for _ in range(65):
        key('right')
    cells.append((shot('last-track-full'), 5 + 3 * 20, 'A-5 02 21 999 NBEEF'))
    key('pagedown'); key('pagedown')  # Full -> VU -> Compact.
    cells.append((shot('last-track-compact'), 5 + 11 * 8, 'A-5 02N'))
    key('m')
    commands.append(f'lua local f=assert(io.open("{app}/selected-track.txt","w")); f:write(tostring(zt.track(63).muted)); f:close()')
    for _ in range(65):
        key('left')
    cells.append((shot('first-track-compact'), 5, 'C-5 00S'))
    # Switching formats during playback must work too.
    key('f5'); key('pagedown')
    cells.append((shot('playing-regular'), 5, 'C-5 00 64 037'))
    key('pagedown')
    cells.append((shot('playing-full'), 5, 'C-5 00 64 037 S1234'))
    key('f8')
    # A NUL effect shortens the legacy formatter; clear the previous last digit.
    commands.append('lua for p=0,1 do for r=0,63 do zt.cell(0,r,p).effect=0 end end')
    commands.extend(['window_event exposed', 'wait 70'])
    cells.append((shot('full-without-effect'), 5, 'C-5 00 64 037 1234 '))
    key('pagedown'); key('pagedown')  # Full -> VU -> Compact.
    cells.append((shot('compact-without-effect'), 5, 'C-5 00 '))
    key('alt+n'); key('y')
    comparisons.append(('initial-unplayed', shot('new-song')))
    commands.extend(['window_event focus_gained', 'wait 70'])
    comparisons.append(('new-song', shot('new-song-focus')))
    commands.append('quit')
    script = app / 'playback-focus.txt'
    script.write_text('\n'.join(commands) + '\n')
    result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                            cwd=app.parent, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    assert (app / 'selected-track.txt').read_text() == 'true', \
        'Selected track lost during format change'
    names = {name for pair in comparisons for name in pair} | {name for name, _, _ in cells}
    grids = {name: grid_pixels(app / (name + '.png')) for name in names}
    assert grids['song-stopped'] != grids['initial-unplayed'], 'Playback never painted a pattern'
    for before, after in comparisons:
        assert grids[before] == grids[after], \
            f'Stopped playback grid changed: {before} -> {after}'
    for name, column, text in cells:
        check_cell(app / (name + '.png'), column, text, grids[name])
    for mode, size in [(1, 8), (2, 14), (3, 20)]:
        name = f'mode-{mode}'
        check_header_gaps(app / (name + '.png'), size, grids[name])
        check_track_separators(app / (name + '.png'), size, grids[name])

    def run_session(lines):
        script.write_text('\n'.join(['wait 100', 'key f5', 'wait 70', 'key f8',
                                     'wait 70'] + lines + ['quit']) + '\n')
        result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                                cwd=app.parent, capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stderr

    # Cycle to each view, leave/reenter F5, quit, and restore it in a new process.
    for mode in (2, 3, 4, 1):
        before = app / f'saved-{mode}.png'
        after = app / f'restored-{mode}.png'
        run_session(['key pagedown', 'wait 70', 'key f2', 'wait 70', 'key f5',
                     'wait 70', f'shot {before}'])
        saved = (app / 'zt.conf').read_text()
        assert re.search(rf'^playback_view: {mode}$', saved, re.M), saved
        run_session([f'shot {after}'])
        assert grid_pixels(before) == grid_pixels(after), \
            f'Playback view {mode} was not restored after restart'

    # A malformed setting falls back to Compact.
    config = app / 'zt.conf'
    config.write_text(re.sub(r'^playback_view: .*$', 'playback_view: 99',
                             config.read_text(), flags=re.M))
    run_session([f'shot {app}/invalid-view.png'])
    assert grid_pixels(app / 'invalid-view.png') == grid_pixels(app / 'restored-1.png')
print(f'Playback views: {len(comparisons)} repaint comparisons and {len(cells)} rendered field checks passed.')
