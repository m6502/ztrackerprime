#!/usr/bin/env python3
"""Exercise F11 pane switching and within-pane navigation in the actual UI."""
from pathlib import Path
import subprocess
import sys
import tempfile
from app_fixture import create_test_app

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-songconfig-focus-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    (app / 'zt.conf').write_text('skin: default\ndefault_directory: \n')
    commands = ['wait 50', 'key f11', 'wait 50', f'shot {app}/order.png']

    def key(chord):
        commands.extend([f'key {chord}', 'wait 50'])

    def shot(name):
        commands.append(f'shot {app}/{name}.png')
    # Arrow keys stay in the list at both ends; Tab handles focus changes.
    for _ in range(3):
        commands += ['key up', 'wait 50']
    commands += [f'shot {app}/up-at-start.png',
                 'key down', 'wait 50', f'shot {app}/second-order.png',
                 'key up', 'wait 50', f'shot {app}/back-at-start.png',
                 'key end', 'wait 50', 'key end', 'wait 50', f'shot {app}/last-order.png',
                 'key down', 'wait 50', f'shot {app}/down-at-end.png',
                 'key home', 'wait 50', 'key home', 'wait 50', f'shot {app}/reset-order.png']
    # Both Tab directions switch panes and restore every individual setting.
    key('tab')
    for index in range(13):
        shot(f'setting-{index}')
        for chord, label in [('tab', 'tab'), ('shift+tab', 'shift-tab')]:
            key(chord); shot(f'order-{index}-{label}')
            key(chord); shot(f'return-{index}-{label}')
        key('down')
    shot('settings-down-wrap')
    for index in reversed(range(13)):
        key('up'); shot(f'reverse-{index}')

    # Changing panes preserves the order row and the digit being edited.
    key('down'); key('tab'); key('down'); key('right')
    shot('order-position')
    key('tab'); shot('remembered-bpm')
    key('f11'); key('down'); shot('expected-bpm-at-order-position')
    key('tab'); shot('restored-order-position')
    key('up'); key('left'); shot('order-position-reset')

    # The left-side focus is also retained when returning from another page.
    key('tab'); key('f2'); key('f11'); shot('reentry-order')
    key('tab'); shot('reentry-setting')
    key('up'); key('tab')
    commands += ['key f11', 'wait 50', f'shot {app}/toggle-title.png',
                 'key f11', 'wait 50', f'shot {app}/toggle-order.png',
                 'key f2', 'wait 50', 'key f11', 'wait 50', f'shot {app}/fresh-order.png',
                 'quit']
    script = app / 'focus.txt'
    script.write_text('\n'.join(commands) + '\n')
    result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                            cwd=app.parent, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    order = (app / 'order.png').read_bytes()
    for name in ('up-at-start', 'back-at-start', 'reset-order'):
        assert (app / f'{name}.png').read_bytes() == order, \
            f'Order List lost focus or moved past position 000: {name}'
    assert (app / 'second-order.png').read_bytes() != order, 'Down did not move to position 001'
    assert (app / 'last-order.png').read_bytes() == (app / 'down-at-end.png').read_bytes(), \
        'Down moved past the last order or changed focus'
    settings = [(app / f'setting-{i}.png').read_bytes() for i in range(13)]
    assert len(set(settings)) == 13, 'Down skipped or repeated a setting'
    assert (app / 'settings-down-wrap.png').read_bytes() == settings[0], \
        'Down from Sync Offset did not wrap to Title'
    for index in range(13):
        assert (app / f'reverse-{index}.png').read_bytes() == settings[index], \
            f'Incorrect Up navigation to setting {index}'
        for label in ('tab', 'shift-tab'):
            assert (app / f'order-{index}-{label}.png').read_bytes() == order, \
                f'{label} did not switch from setting {index} to Order List'
            assert (app / f'return-{index}-{label}.png').read_bytes() == settings[index], \
                f'{label} did not restore setting {index}'
    assert (app / 'order-position.png').read_bytes() == (app / 'restored-order-position.png').read_bytes(), \
        'Pane switching lost the order row or digit position'
    assert (app / 'order-position-reset.png').read_bytes() == order
    assert (app / 'remembered-bpm.png').read_bytes() == (app / 'expected-bpm-at-order-position.png').read_bytes(), \
        'Tab did not restore BPM after navigating the Order List'
    assert (app / 'reentry-setting.png').read_bytes() == settings[1]
    assert (app / 'reentry-order.png').read_bytes() == order
    assert (app / 'toggle-title.png').read_bytes() == settings[0], 'F11 did not toggle to Title'
    assert (app / 'toggle-order.png').read_bytes() == order, 'F11 did not toggle to Order List'
    assert (app / 'fresh-order.png').read_bytes() == order, 'Fresh F11 entry lost Order List focus'

print('F11 focus: Tab/Shift+Tab restores all 13 settings, arrow wrap, order position and page reentry passed.')
