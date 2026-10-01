#!/usr/bin/env python3
"""Exercise F12 Tab/Shift+Tab cycles with empty and remembered MIDI lists."""
from pathlib import Path
import subprocess
import sys
import tempfile
from app_fixture import create_test_app

binary = Path(sys.argv[1]).resolve()
for remembered in [False, True]:
    with tempfile.TemporaryDirectory(prefix='zt-sysconfig-focus-') as tmp:
        app, executable = create_test_app(binary, Path(tmp))
        (app / 'zt.conf').write_text('skin: default\ndefault_directory: \n')
        if remembered:
            (app / 'devices.conf').write_text(
                'known_in_device_0: Focus Test MIDI In\n'
                'known_out_device_0: Focus Test MIDI Out\n')
        commands = ['wait 50', 'key f12', 'wait 50', f'shot {app}/top.png']
        for direction, key in [('forward', 'tab'), ('backward', 'shift+tab')]:
            for index in range(18):
                commands += [f'key {key}', 'wait 50', f'shot {app}/{direction}-{index}.png']
        commands += ['quit']
        script = app / 'focus.txt'
        script.write_text('\n'.join(commands) + '\n')
        result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                                cwd=app.parent, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        top = (app / 'top.png').read_bytes()
        forward = [(app / f'forward-{i}.png').read_bytes() for i in range(18)]
        backward = [(app / f'backward-{i}.png').read_bytes() for i in range(18)]
        assert len(set(forward)) == 18, 'Tab skipped or repeated a visible control'
        assert forward[-1] == top, 'Tab did not wrap to the page button'
        assert backward == list(reversed(forward[:-1])) + [top], \
            'Shift+Tab did not reverse the complete cycle (list/button loop)'

print('F12 focus: 18 controls, forward/backward wrap, empty and remembered MIDI lists passed.')
