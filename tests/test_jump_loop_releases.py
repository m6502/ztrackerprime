#!/usr/bin/env python3
"""J loops in MIDI export: a note still held at "]" goes on sounding into the
loop body in a game that loops on the markers, so the export must release it
there, where the tracker releases it after the jump. Also: every note still
held at the end of the song gets a NOTE_OFF, and a loop release that would
also cut a note sounding on the first pass is reported."""
import subprocess
import sys
import tempfile
from pathlib import Path

zt = Path(sys.argv[1]).resolve()


def varlen(d, i):
    v = 0
    while True:
        b = d[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i


def tracks(d):
    """Each MTrk as a list of (tick, what): 'on36c0', 'off36c0', '[0001', 'END'."""
    out, i = [], 14
    while i < len(d):
        end = i + 8 + int.from_bytes(d[i + 4:i + 8], 'big')
        j, t, run, evs = i + 8, 0, 0, []
        while j < end:
            dt, j = varlen(d, j)
            t += dt
            if d[j] == 0xFF:
                kind, (n, k) = d[j + 1], varlen(d, j + 2)
                if kind == 0x06:
                    evs.append((t, d[k:k + n].decode()))
                elif kind == 0x2F:
                    evs.append((t, 'END'))
                j = k + n
                continue
            if d[j] & 0x80:
                run = d[j]
                j += 1
            n = 1 if run & 0xF0 in (0xC0, 0xD0) else 2
            if run & 0xF0 == 0x90:
                evs.append((t, '%s%dc%d' % ('on' if d[j + 1] else 'off', d[j], run & 15)))
            j += n
        out.append(evs)
        i = end
    return out


def export(tmp, name, lua):
    song, mid = tmp / (name + '.zt'), tmp / (name + '.mid')
    (tmp / (name + '.script')).write_text(
        '\n'.join('lua ' + line for line in lua) +
        f'\nlua assert(zt.save("{song}"))\nquit\n')
    run = dict(cwd=zt.parent, timeout=60, check=True, capture_output=True, text=True)
    subprocess.run([str(zt), '--headless', '--script', str(tmp / (name + '.script'))], **run)
    assert song.exists(), f'{name}: song was not saved'
    result = subprocess.run([str(zt), str(song), '--export-multitrack-midi', str(mid)], **run)
    return tracks(mid.read_bytes()), result.stderr


# Instruments 0-2: tracker mode (a note lasts until the track's next note),
# channels 0-2. Instrument 3: plain, channel 3, lengths in ticks.
# Orders 0, 1, 2 = patterns 0, 1, 2 (64 rows); J0001 on the last row of
# pattern 1 loops order 1.
SETUP = [
    'for i=0,3 do local ins = zt.instrument(i); ins.channel = i; ins.flags = (i == 3) and 0 or 4 end',
    'function put(p, t, r, n, i, len) zt.song.cur_pattern = p; local c = zt.cell(t, r); '
    'c.note = n; c.instrument = i; if len then c.length = len end end',
    'zt.song.cur_pattern = 1; local c = zt.cell(9, 63); c.effect = 0x4A; c.effect_data = 1',
    'zt.orders[0] = 0; zt.orders[1] = 1; zt.orders[2] = 2; zt.orders[3] = zt.BREAK',
]

with tempfile.TemporaryDirectory(prefix='zt-jump-loop-') as tmp:
    tmp = Path(tmp)

    trk, stderr = export(tmp, 'loop', SETUP + [
        'put(1, 0, 47, 36, 0)',          # held across the jump, cut by itself at row 47
        'put(1, 1, 0, 72, 1)',           # on the "[" row, held across the jump
        'put(1, 2, 8, 70, 2)',           # cuts ...
        'put(1, 2, 63, 68, 2)',          # ... this last-row note after the jump
        'put(1, 3, 62, 50, 3, 48)',      # 48 ticks from row 62: spans the jump
        'put(2, 0, 0, 40, 0)',           # exit section; held to the end
        'put(0, 4, 0, 30, 0)',           # held the whole song, no note in the loop
    ])
    assert 'also cuts' not in stderr, stderr

    loop_start = next(t for t, w in trk[0] if w == '[0001')
    loop_end = next(t for t, w in trk[0] if w == ']0001')
    row = loop_start // 64
    assert loop_end == loop_start + 64 * row, (loop_start, loop_end)

    def released_before(track, tick, off, on=None):
        evs = trk[track]
        i = evs.index((tick, off))
        if on:
            assert evs[i + 1] == (tick, on), f'track {track}: {off} not just before {on}: {evs}'

    released_before(1, loop_start + 47 * row, 'off36c0', 'on36c0')
    released_before(2, loop_start, 'off72c1', 'on72c1')
    released_before(3, loop_start + 8 * row, 'off68c2', 'on70c2')
    released_before(4, loop_start + 48 - 2 * row, 'off50c3')

    # Track 5's note 30 is held across the loop but nothing in the loop
    # releases it: no release inside the loop body.
    assert [w for t, w in trk[5] if loop_start <= t < loop_end] == [], trk[5]

    # Played straight through, the file leaves nothing sounding at the end.
    song_end = trk[0][-1][0]
    held = {}
    for evs in trk[1:]:
        for t, w in evs:
            if w.startswith('on'):
                held[w[2:]] = held.get(w[2:], 0) + 1
            elif w.startswith('off') and held.get(w[3:], 0) > 0:
                held[w[3:]] -= 1
        assert evs[-1] == (song_end, 'END'), evs
    assert not any(held.values()), held

    # A song that ends on the J: the file ends exactly at the "]", and the
    # notes held there get no end-of-song NOTE_OFF on that tick (it would cut
    # them right before the jump); the loop releases them instead.
    trk, stderr = export(tmp, 'ends_on_loop', SETUP + [
        'zt.orders[2] = zt.BREAK',
        'put(1, 0, 47, 36, 0)',
        'put(1, 1, 0, 72, 1)',
    ])
    loop_start = next(t for t, w in trk[0] if w == '[0001')
    loop_end = next(t for t, w in trk[0] if w == ']0001')
    row = loop_start // 64
    assert trk[0][-1] == (loop_end, 'END'), trk[0]
    for evs in trk[1:]:
        assert [w for t, w in evs if t == loop_end] == ['END'], evs
    released_before(1, loop_start + 47 * row, 'off36c0', 'on36c0')
    released_before(2, loop_start, 'off72c1', 'on72c1')

    # Another track holding note 36 on channel 0 when the loop release of
    # track 0's note 36 is written: on the first pass that release cuts it.
    _, stderr = export(tmp, 'conflict', SETUP + [
        'put(1, 0, 47, 36, 0)',
        'put(1, 5, 40, 36, 0)',
    ])
    assert 'also cuts that note on the first pass' in stderr, stderr

print('jump loop releases: held notes released where the tracker releases them; '
      'nothing left sounding; no cut on a final "]"; first-pass conflict reported')
