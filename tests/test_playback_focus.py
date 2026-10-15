#!/usr/bin/env python3
"""Keep the stopped playback grid across actual SDL focus/expose repaints."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib
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
        # PatternDisplay starts at character row 12. Exclude status/toolbar.
        if 96 <= y < height - 64:
            pixels.extend(row[8 * bpp:(width - 8) * bpp])
        previous = row
    return bytes(pixels)


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
    names = {name for pair in comparisons for name in pair}
    grids = {name: grid_pixels(app / (name + '.png')) for name in names}
    assert grids['song-stopped'] != grids['initial-unplayed'], 'Playback never painted a pattern'
    for before, after in comparisons:
        assert grids[before] == grids[after], \
            f'Stopped playback grid changed: {before} -> {after}'
print(f'Playback focus: {len(comparisons)} stopped song/loop grid comparisons passed.')
