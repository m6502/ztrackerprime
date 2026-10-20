#!/usr/bin/env python3
"""Drive the real F11 editor, then inspect its saved compressed/plain chunks."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib
from app_fixture import create_test_app


def chunks(path):
    data = path.read_bytes()
    end = 8 + struct.unpack_from('=I', data, 4)[0]
    body = data[end:]
    if data[11] & 4:
        body = zlib.decompress(body)
    result = {}
    at = 0
    while at + 8 <= len(body):
        tag, size = struct.unpack_from('=4sI', body, at)
        at += 8
        assert at + size <= len(body), 'truncated saved chunk'
        result[tag] = body[at:at+size]
        at += size
    return result



def check_order_number(path, expected, font_path):
    """Check rendered order digits so a stale Order List cannot pass."""
    data = path.read_bytes()
    at, packed = 8, bytearray()
    while at < len(data):
        size = struct.unpack_from('>I', data, at)[0]
        kind, chunk = data[at+4:at+8], data[at+8:at+8+size]
        if kind == b'IHDR':
            width, height, depth, color, _, _, interlace = struct.unpack('>IIBBBBB', chunk)
            assert depth == 8 and color in (2, 6) and interlace == 0
        elif kind == b'IDAT': packed.extend(chunk)
        at += size + 12
    bpp = 4 if color == 6 else 3
    stride = width * bpp
    raw = zlib.decompress(packed)
    previous = bytearray(stride)
    rows = []
    for y in range(height):
        at = y * (stride + 1)
        kind = raw[at]
        row = bytearray(raw[at+1:at+1+stride])
        for i in range(stride):
            a, b, c = row[i-bpp] if i >= bpp else 0, previous[i], previous[i-bpp] if i >= bpp else 0
            if kind == 4:
                p = a + b - c
                distances = (abs(p-a), abs(p-b), abs(p-c))
                prediction = (a, b, c)[distances.index(min(distances))]
            else: prediction = (0, a, b, (a+b)//2)[kind]
            row[i] = (row[i] + prediction) & 255
        rows.append(row)
        previous = row
    font = font_path.read_bytes()
    background = rows[13*8][62*8*bpp:62*8*bpp+3]
    for index, char in enumerate(f'{expected:03d}'):
        for y in range(8):
            for x in range(8):
                at = ((59+index)*8+x)*bpp
                actual = rows[13*8+y][at:at+3] != background
                wanted = bool(font[ord(char)*8+y] & (128 >> x))
                assert actual == wanted, f'Order List did not scroll to {expected}'


binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-position-notes-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    (app / 'zt.conf').write_text('skin: default\ndefault_directory: \n')

    def run(commands):
        script = app / 'notes.txt'
        script.write_text('\n'.join(['wait 80'] + commands + ['quit']) + '\n')
        result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                                cwd=app.parent, capture_output=True, text=True, timeout=40)
        assert result.returncode == 0, result.stderr

    commands = []

    def key(chord):
        commands.extend([f'key {chord}', 'wait 15'])

    def text(value):
        commands.extend([f'text {value}', 'wait 30'])

    def save(name, compressed=False):
        commands.append(f'lua assert(zt.save("{app}/{name}.zt", {str(compressed).lower()}))')
        commands.append('wait 30')

    def load(name):
        commands.extend([f'lua assert(zt.load("{app}/{name}.zt"))', 'wait 50'])

    save('blank')
    key('f11'); key('tab')
    text('Intro'); key('return'); text('Verse')
    key('home'); key('backspace'); key('delete'); text('X')
    key('down'); key('home'); key('backspace'); text('Bridge')
    commands.append(f'shot {app}/notes.png')
    # PageDown clamps at 255 and scrolls both panes together.
    for _ in range(30): key('pagedown')
    text('Ending'); key('return'); text('!'); key('delete')
    save('plain'); save('compressed', True)
    commands.append(f'shot {app}/last.png')
    # Rebind safely when loading without leaving F11.
    load('plain'); key('end'); key('backspace'); save('edited')
    load('compressed'); save('roundtrip')
    load('blank'); save('reset')
    # Mouse placement uses the same line grid as the order list.
    for _ in range(30): key('pageup')
    commands.extend([f'click {71*8} {18*8+2}', 'wait 30'])
    text('Clicked')
    key('down'); key('home'); text('L' * 100)
    key('backspace'); key('home'); text('X'); key('end'); text('!')
    save('clicked')
    # F10 retains ordinary multiline insertion and deletion.
    key('f10'); text('A'); key('return'); text('B'); save('message')
    run(commands)

    expected = [''] * 256
    expected[0:3] = ['Intro', 'Xerse', 'Bridge']
    expected[255] = 'Ending!'
    payload = '\n'.join(expected).encode()
    # Forty order rows fit in the default 480px-high test window.
    check_order_number(app / 'last.png', 216, app / 'skins/default/font.fnt')
    for name in ('plain', 'compressed', 'roundtrip'):
        assert chunks(app / f'{name}.zt')[b'PNTS'] == payload, name
    expected[255] = 'Ending'
    assert chunks(app / 'edited.zt')[b'PNTS'] == '\n'.join(expected).encode()
    assert b'PNTS' not in chunks(app / 'reset.zt'), 'old songs inherited notes'
    clicked = chunks(app / 'clicked.zt')[b'PNTS'].split(b'\n')
    assert len(clicked) == 256 and clicked[5] == b'Clicked', clicked[:8]
    assert clicked[6] == b'X' + b'L' * 99 + b'!', 'horizontal editing lost text'
    assert chunks(app / 'message.zt')[b'SMSG'].rstrip(b'\0').endswith(b'A\nB')

    # Invalid optional notes must not break loading or violate the line grid.
    base = (app / 'blank.zt').read_bytes()
    end = 8 + struct.unpack_from('=I', base, 4)[0]
    for index, bad in enumerate((b'\n'*254, b'\n'*256,
                                b'\0' + b'\n'*255,
                                b'a'*1025 + b'\n'*255)):
        (app / f'bad{index}.zt').write_bytes(
            base[:end] + struct.pack('=4sI', b'PNTS', len(bad)) + bad + base[end:])
    commands.clear()
    key('f11')
    for index in range(4):
        load(f'bad{index}'); save(f'validated{index}')
    run(commands)
    for index in range(4):
        assert b'PNTS' not in chunks(app / f'validated{index}.zt')


    # Each pane keeps its absolute caret position and order digit even when
    # the other pane scrolls it completely out of view.
    commands.clear()
    key('f11'); key('down'); key('down'); key('right')
    key('tab')
    for _ in range(5): key('down')
    text('Memo')
    key('shift+tab'); key('7')  # order 002, tens digit
    key('tab'); text('!')
    for _ in range(30): key('pagedown')
    text('End')
    commands.append(f'shot {app}/independent-bottom.png')
    key('shift+tab'); key('3')  # order 002, units digit; advances to 003
    key('tab'); text('!')      # notes resume at 255
    for _ in range(30): key('pageup')
    text('Start')
    key('shift+tab'); key('1')  # order 003, hundreds digit
    key('end'); key('end')     # orders now at 255
    key('tab'); text('!')      # notes resume at 000
    key('shift+tab'); key('right'); key('9')  # order 255, units digit
    save('independent')
    run(commands)
    stored = chunks(app / 'independent.zt')
    notes = stored[b'PNTS'].split(b'\n')
    assert notes[0] == b'Start!' and notes[5] == b'Memo!' and notes[255] == b'End!', \
        'Order navigation moved the notes caret'
    orders = struct.unpack('=256H', stored[b'ZTol'])
    assert orders[2] == 73 and orders[3] == 100 and orders[255] == 9, \
        'Notes navigation moved the order caret or its digit'
    check_order_number(app / 'independent-bottom.png', 216, app / 'skins/default/font.fnt')

print('Position notes: editing, 256-line boundaries, mouse placement, compressed/plain persistence, reload, malformed chunks, independent cursors and F10 passed.')
