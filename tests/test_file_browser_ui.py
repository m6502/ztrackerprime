#!/usr/bin/env python3
"""Exercise the real combined load/save browser in an isolated app directory."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
from app_fixture import create_test_app

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-file-browser-ui-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    songs = app / 'songs'
    nested = songs / 'nested folder'
    nested.mkdir(parents=True)
    (songs / 'Z folder').mkdir()
    (songs / 'vanishing folder').mkdir()
    home = app / 'test-home'
    home.mkdir()
    (app / 'zt.conf').write_text(
        f'skin: default\nscreen_width: 960\nscreen_height: 640\nzoom: 1.0\n'
        f'default_directory: {songs}\n')
    commands = ['wait 100', 'lua zt.pattern(0):set_note(0,0,60,0,100); assert(zt.save("alpha.zt"))']

    def command(text):
        commands.extend([text, 'wait 100'])

    def marker(name):
        command(f'lua local f=assert(io.open("{name}","w")); f:write("ok"); f:close()')

    for dialog, shortcut in [('load', 'ctrl+l'), ('save', 'ctrl+shift+s')]:
        command(f'key {shortcut}')
        command('key n')  # Typeahead matches the folder name despite its slash marker.
        command('key return')
        marker(f'{dialog}-entered')
        command('key backspace')
        marker(f'{dialog}-parent')
        command('key return')  # Going up selected the folder we just left.
        marker(f'{dialog}-reentered')
        command('key backspace')
        # Move between Places and the browser without losing input or focus.
        command('key left')
        command('key right')
        command('key n')
        command('key return')
        marker(f'{dialog}-focus')
        command('key backspace')
        command('click 200 116')
        marker(f'{dialog}-click')
        command('key backspace')
        command(f'shot {app}/{dialog}.png')

    # A folder removed after enumeration must produce an error without changing CWD.
    command('key v')
    command('lua assert(os.remove("vanishing folder"))')
    command('key return')
    marker('failed-navigation')
    # Filename field follows browser -> Places in the Tab cycle. Save a new song.
    command('key tab')
    command('key tab')
    command('key home')
    for _ in range(len('alpha.zt')):
        command('key delete')
    for char in 'saved':
        command(f'key {char}')
    command('key return')
    command('wait 500')
    # Load a file from the combined list and verify its song data was restored.
    command('lua zt.pattern(0):set_note(0,0,72,0,100); assert(zt.save("other.zt"))')
    command('key ctrl+l')
    command('key a')
    command('key return')
    command('wait 500')
    command('lua assert(zt.pattern(0):cell(0,0).note == 60)')
    command('key ctrl+l')
    command('key left')
    command('key h')
    command('key return')
    marker('home-selected')
    command('quit')
    script = app / 'browser.txt'
    script.write_text('\n'.join(commands) + '\n')
    result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                            cwd=app.parent, env={**os.environ, 'HOME': str(home)},
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    for dialog in ('load', 'save'):
        for suffix in ('entered', 'reentered', 'focus', 'click'):
            assert (nested / f'{dialog}-{suffix}').read_text() == 'ok', result.stderr
        assert (songs / f'{dialog}-parent').read_text() == 'ok', result.stderr
    assert (songs / 'failed-navigation').read_text() == 'ok'
    assert (songs / 'saved.zt').is_file(), result.stderr
    assert (home / 'home-selected').read_text() == 'ok', result.stderr
    assert not (songs / 'nested folder.zt').exists(), 'Enter on a folder invoked save'
print('Combined browser: folder activation, parent selection, Places focus and saving passed.')
