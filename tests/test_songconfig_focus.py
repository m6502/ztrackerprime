#!/usr/bin/env python3
"""Exercise complete F11 focus cycles through the actual headless UI."""
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-songconfig-focus-') as tmp:
    app = Path(tmp)
    (app / 'zt').symlink_to(binary)
    (app / 'skins').symlink_to(binary.parent / 'skins', target_is_directory=True)
    (app / 'syx').mkdir()
    (app / 'zt.conf').write_text('skin: default\ndefault_directory: \n')
    commands = ['wait 50', 'key f11', 'wait 50', f'shot {app}/order.png']
    for direction, key in [('forward', 'tab'), ('backward', 'shift+tab')]:
        for index in range(14):
            commands += [f'key {key}', 'wait 50', f'shot {app}/{direction}-{index}.png']
    commands += ['key f11', 'wait 50', f'shot {app}/toggle-title.png',
                 'key f11', 'wait 50', f'shot {app}/toggle-order.png',
                 'key f2', 'wait 50', 'key f11', 'wait 50', f'shot {app}/fresh-order.png',
                 'quit']
    script = app / 'focus.txt'
    script.write_text('\n'.join(commands) + '\n')
    result = subprocess.run([str(app / 'zt'), '--headless', '--script', str(script)],
                            cwd=app.parent, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    order = (app / 'order.png').read_bytes()
    forward = [(app / f'forward-{i}.png').read_bytes() for i in range(14)]
    backward = [(app / f'backward-{i}.png').read_bytes() for i in range(14)]
    assert len(set(forward)) == 14, 'Forward Tab skipped or repeated a control'
    assert forward[-1] == order, 'Forward Tab did not wrap to the order list'
    expected_backward = list(reversed(forward[:-1])) + [order]
    assert backward == expected_backward, 'Shift+Tab does not reverse the complete Tab cycle'
    assert (app / 'toggle-title.png').read_bytes() == forward[0], 'F11 did not toggle to Title'
    assert (app / 'toggle-order.png').read_bytes() == order, 'F11 did not toggle to Order List'
    assert (app / 'fresh-order.png').read_bytes() == order, 'Fresh F11 entry lost Order List focus'

print('F11 focus: 14 controls, forward/backward wrap and F11 toggling passed.')
