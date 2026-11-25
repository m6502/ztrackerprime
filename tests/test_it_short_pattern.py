#!/usr/bin/env python3
"""IT import: a pattern shorter than zTracker's 32-row minimum is padded to 32
rows and ends with a pattern break on its last real row, so it still plays for
its original length (it used to stay at the default 64 rows)."""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from app_fixture import create_test_app

# A scratch copy of the app: zt saves zt.conf beside itself on exit, which
# inside the real macOS bundle breaks its code signature.
_app_dir = tempfile.TemporaryDirectory(prefix='zt-app-')
app, zt = create_test_app(Path(sys.argv[1]), Path(_app_dir.name))


def it_pattern(rows, note):
    # Channel 0, mask "note + instrument follow", on row 0; every row ends with a 0 byte.
    data = bytes([0x81, 0x03, note, 1, 0]) + bytes(rows - 1)
    return struct.pack('<HH4x', len(data), rows) + data


def it_file(patterns, orders):
    header = bytearray(192)
    header[0:4] = b'IMPM'
    struct.pack_into('<HHHH', header, 0x20, len(orders), 0, 0, len(patterns))
    struct.pack_into('<HH', header, 0x28, 0x0214, 0x0214)
    header[0x30:0x36] = bytes([128, 48, 6, 125, 128, 0])
    body = bytes(orders)
    first = 192 + len(body) + 4 * len(patterns)
    offsets, blobs = [], b''
    for p in patterns:
        offsets.append(first + len(blobs))
        blobs += p
    return bytes(header) + body + struct.pack('<%dI' % len(patterns), *offsets) + blobs


def note_ons(d):
    """Returns ([(tick, note)], song_end_tick)."""
    i, out, song_end = 14, [], 0
    while i < len(d):
        end = i + 8 + int.from_bytes(d[i + 4:i + 8], 'big')
        j, t, run = i + 8, 0, 0
        while j < end:
            v = 0
            while True:
                b = d[j]
                j += 1
                v = (v << 7) | (b & 0x7F)
                if not b & 0x80:
                    break
            t += v
            if d[j] == 0xFF:
                n, k = d[j + 2], j + 3
                j = k + n
                song_end = max(song_end, t)
                continue
            if d[j] & 0x80:
                run = d[j]
                j += 1
            n = 1 if run & 0xF0 in (0xC0, 0xD0) else 2
            if run & 0xF0 == 0x90 and d[j + 1]:
                out.append((t, d[j]))
            j += n
        i = end
    return sorted(out), song_end


with tempfile.TemporaryDirectory(prefix='zt-it-') as tmp:
    tmp = Path(tmp)
    it, mid, report = tmp / 'short.it', tmp / 'short.mid', tmp / 'report.txt'
    it.write_bytes(it_file([it_pattern(16, 60), it_pattern(64, 62)], [0, 1]))

    (tmp / 'check.script').write_text(f'''\
lua assert(zt.load("{it}")); local c = zt.pattern(0):cell(0, 15); local f = io.open("{report}", "w"); f:write(zt.get_pattern_length(0), " ", zt.get_pattern_length(1), " ", c.effect, " ", c.effect_data); f:close()
quit
''')
    run = dict(cwd=app, timeout=60, check=True,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run([str(zt), '--headless', '--script', str(tmp / 'check.script')], **run)
    len0, len1, effect, data = map(int, report.read_text().split())
    assert (len0, len1) == (32, 64), f'pattern lengths {len0}, {len1}'
    assert (effect, data) == (ord('C'), 0), f'row 15 effect {effect:#x} {data:#x}'

    subprocess.run([str(zt), str(it), '--export-multitrack-midi', str(mid)], **run)
    ons, song_end = note_ons(mid.read_bytes())
    assert [n for _, n in ons] == [60, 62], ons
    # Pattern 1 starts after 16 rows; the song is 16 + 64 = 80 rows long.
    assert ons[1][0] > 0 and abs(ons[1][0] * 80 - song_end * 16) <= 16, (ons, song_end)

print('IT import: 16-row pattern padded to 32 with C0000 on row 15, plays 16 rows')
