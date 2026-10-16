#!/usr/bin/env python3
"""Run the real app through unplug/replug using a disposable ALSA virtual port."""
import ctypes as C
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import zlib
from app_fixture import create_test_app

alsa = C.CDLL('libasound.so.2')
alsa.snd_seq_open.argtypes = [C.POINTER(C.c_void_p), C.c_char_p, C.c_int, C.c_int]
alsa.snd_seq_set_client_name.argtypes = [C.c_void_p, C.c_char_p]
alsa.snd_seq_create_simple_port.argtypes = [C.c_void_p, C.c_char_p, C.c_uint, C.c_uint]
alsa.snd_seq_delete_simple_port.argtypes = [C.c_void_p, C.c_int]
alsa.snd_seq_get_port_info.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
alsa.snd_seq_port_info_malloc.argtypes = [C.POINTER(C.c_void_p)]
alsa.snd_seq_port_info_free.argtypes = [C.c_void_p]
alsa.snd_seq_port_info_get_read_use.argtypes = [C.c_void_p]
alsa.snd_seq_port_info_get_write_use.argtypes = [C.c_void_p]
alsa.snd_seq_close.argtypes = [C.c_void_p]
seq = C.c_void_p()
if alsa.snd_seq_open(C.byref(seq), b'default', 3, 0) < 0:
    print('SKIP: ALSA sequencer unavailable')
    sys.exit(77)
client_name = f'ZT App Hotplug Test {os.getpid()}'
alsa.snd_seq_set_client_name(seq, client_name.encode())
port_name = f'{client_name}:Virtual MIDI'


def instrument_slot(path):
    data = path.read_bytes()
    header_end = 8 + struct.unpack_from('<I', data, 4)[0]
    if data[11] & 4:
        data = data[:header_end] + zlib.decompress(data[header_end:])
    position = 0
    while position < len(data):
        tag, size = struct.unpack_from('<4sI', data, position)
        payload = data[position + 8:position + 8 + size]
        if tag == b'ZTin' and payload[0] == 0:
            return payload[4]
        position += 8 + size
    raise AssertionError('saved song lost instrument 0')


def subscribers(port):
    info = C.c_void_p()
    assert alsa.snd_seq_port_info_malloc(C.byref(info)) == 0
    try:
        assert alsa.snd_seq_get_port_info(seq, port, info) == 0
        return (alsa.snd_seq_port_info_get_read_use(info),
                alsa.snd_seq_port_info_get_write_use(info))
    finally:
        alsa.snd_seq_port_info_free(info)


process = None
try:
    binary = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='zt-hotplug-ui-') as tmp:
        app, executable = create_test_app(binary, Path(tmp))
        (app / 'zt.conf').write_text('skin: default\nauto_open_midi: yes\ndefault_directory: \n')
        (app / 'devices.conf').write_text(
            f'known_out_device_0: {port_name}\nopen_out_device_0: {port_name}\n'
            f'known_in_device_0: {port_name}\nopen_in_device_0: {port_name}\n'
            f'alias_{"".join(c if c.isascii() and c.isalnum() else "_" for c in port_name)}: Fixture MIDI\n')
        # Generate a valid uncompressed song with the real writer, then attach
        # a named destination that is absent when the normal application starts.
        (app / 'lua').mkdir()
        song_path = app / 'fixture.zt'
        (app / 'lua/selftest.lua').write_text(
            'zt.instrument(0).name = "Hotplug regression"\n'
            f'assert(zt.save({str(song_path)!r}, false))\n')
        result = subprocess.run([str(executable), '--headless', '--lua-test'],
                                cwd=app, capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stderr
        name = port_name.encode()
        metadata = struct.pack('<BH BHH', 1, 1, 0, len(name), 0) + name
        with song_path.open('ab') as output:
            output.write(b'MIDR' + struct.pack('<I', len(metadata)) + metadata)
        script = app / 'run.txt'
        script.write_text('\n'.join([
            'wait 600', 'key f3', 'wait 100', f'shot {app}/missing.png',
            'wait 1800', 'key ctrl+s', 'wait 150', f'shot {app}/connected.png',
            'wait 1800', 'key ctrl+s', 'wait 150', f'shot {app}/disconnected.png',
            'wait 1800', 'key ctrl+s', 'wait 150', f'shot {app}/returned.png',
            'wait 600', 'quit',
        ]) + '\n')
        process = subprocess.Popen([str(executable), '--headless', '--script', str(script), str(song_path)],
                                   cwd=app, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        def stage(name):
            deadline = time.monotonic() + 12
            while not (app / f'{name}.png').exists():
                if process.poll() is not None:
                    out, err = process.communicate()
                    raise AssertionError(f'app exited before {name}: {out}\n{err}')
                assert time.monotonic() < deadline, f'timed out waiting for {name}'
                time.sleep(0.02)

        stage('missing')
        assert instrument_slot(song_path) == 255
        port = alsa.snd_seq_create_simple_port(seq, b'Virtual MIDI', 1 | 2 | 32 | 64, 2 | (1 << 20))
        assert port >= 0
        stage('connected')
        live_slot = instrument_slot(song_path)
        assert live_slot != 255, 'automatic arrival did not restore the real instrument'
        assert all(count > 0 for count in subscribers(port)), 'MIDI input/output did not reconnect'
        assert alsa.snd_seq_delete_simple_port(seq, port) == 0
        stage('disconnected')
        assert instrument_slot(song_path) == 255, 'disconnected instrument retained a live destination'
        port = alsa.snd_seq_create_simple_port(seq, b'Virtual MIDI', 1 | 2 | 32 | 64, 2 | (1 << 20))
        assert port >= 0
        stage('returned')
        assert instrument_slot(song_path) == live_slot, 'returning device did not retain its application slot'
        assert all(count > 0 for count in subscribers(port))
        out, err = process.communicate(timeout=15)
        assert process.returncode == 0, err
        assert 'AddressSanitizer' not in err
finally:
    if process and process.poll() is None:
        process.kill()
        process.communicate()
    alsa.snd_seq_close(seq)
print('Real app MIDI: startup absence, automatic input/output arrival, instrument recovery, unplug and replug passed.')
