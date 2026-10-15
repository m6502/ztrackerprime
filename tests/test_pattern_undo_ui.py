#!/usr/bin/env python3
"""Drive real editor shortcuts, drawing, popups, load and New Song headlessly."""
from pathlib import Path
import subprocess
import sys
import tempfile
from app_fixture import create_test_app

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-pattern-undo-ui-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    (app / 'zt.conf').write_text('skin: default\ndefault_directory: \npattern_length: 64\n')
    report = app / 'checks.txt'
    saved = app / 'song.zt'
    commands = ['wait 100', 'key alt+1', 'wait 50',
                f'lua function check(name, condition) local f=assert(io.open("{report}","a")); f:write((condition and "ok " or "FAIL ")..name.."\\n"); f:close() end']
    checks = []

    def key(chord):
        commands.extend([f'key {chord}', 'wait 50'])

    def lua(code):
        commands.append('lua ' + code)

    def check(name, expression):
        checks.append(name)
        lua(f'check("{name}", {expression})')

    # Note insertion/removal, several levels, redo, page navigation and copy.
    key('z')
    check('note entry', 'not zt.cell(0,0).empty')
    lua('first=zt.cell(0,0).note')
    key('x')
    check('second note', 'not zt.cell(0,1).empty')
    lua('second=zt.cell(0,1).note')
    key('ctrl+z')
    check('undo second insertion', 'zt.cell(0,1).empty and zt.cell(0,0).note==first')
    key('ctrl+z')
    check('undo first insertion', 'zt.cell(0,0).empty')
    key('alt+z')
    check('clear without selection preserves redo', 'zt.cell(0,0).empty and zt.cell(0,1).empty')
    key('ctrl+y')
    check('redo first insertion', 'zt.cell(0,0).note==first and zt.cell(0,1).empty')
    key('f1'); key('f2'); key('up'); key('ctrl+c'); key('ctrl+y')
    check('redo survives page switch and copy', 'zt.cell(0,1).note==second')

    # Alt+Z clears a selected block; Ctrl+Z/Ctrl+Y undo and redo the clear.
    lua('zt.set_cur_row(0)')
    key('ctrl+b'); key('down'); key('ctrl+e'); key('alt+z')
    check('clear block', 'zt.cell(0,0).empty and zt.cell(0,1).empty')
    key('ctrl+z')
    check('undo clear', 'zt.cell(0,0).note==first and zt.cell(0,1).note==second')
    key('ctrl+y')
    check('redo clear', 'zt.cell(0,0).empty and zt.cell(0,1).empty')
    key('ctrl+z')
    check('undo redone clear', 'zt.cell(0,0).note==first')
    key('c')
    lua('branch=zt.cell(0,1).note')
    key('ctrl+y')
    check('new edit discards redo', 'zt.cell(0,1).note==branch and zt.cell(0,0).note==first')

    # Ordinary field editing, delete, paste and the resize apply popup.
    lua('zt.set_cur_row(0)')
    key('right'); key('right'); key('right')
    key('2')
    lua('edited_inst=zt.cell(0,0).instrument')
    key('ctrl+z')
    check('undo instrument entry', 'zt.cell(0,0).instrument~=edited_inst')
    key('ctrl+y')
    check('redo instrument entry', 'zt.cell(0,0).instrument==edited_inst')
    lua('zt.set_cur_row(0)'); key('home'); key('delete')
    lua('deleted=zt.cell(0,0).note')
    key('ctrl+z')
    check('undo delete', 'zt.cell(0,0).note==first')
    key('ctrl+y')
    check('redo delete', 'zt.cell(0,0).note==deleted')
    key('ctrl+z')
    key('ctrl+b'); key('down'); key('ctrl+e'); key('ctrl+c')
    lua('zt.set_cur_row(4)')
    key('ctrl+v')
    check('paste', 'zt.cell(0,4).note==first')
    key('ctrl+z')
    check('undo paste', 'zt.cell(0,4).empty')
    key('ctrl+y')
    check('redo paste', 'zt.cell(0,4).note==first')
    lua('zt.cell(0,63).note=79; original_length=zt.pattern().length')
    key('f2'); key('tab'); key('home'); key('enter')
    check('apply pattern length', 'zt.pattern().length==32 and zt.cell(0,63).empty')
    key('ctrl+z')
    check('undo pattern shrink', 'zt.pattern().length==original_length and zt.cell(0,63).note==79')
    key('ctrl+y')
    check('redo pattern shrink', 'zt.pattern().length==32 and zt.cell(0,63).empty')
    key('ctrl+z')

    # Find/replace is one operation and preserves instrument/volume fields.
    lua('zt.set_cur_row(0); inst_before=zt.cell(0,0).instrument')
    key('ctrl+h'); key('tab'); key('end'); key('enter')
    lua('replacement=zt.cell(0,0).note')
    check('find replace applied', 'replacement~=first')
    key('ctrl+z')
    check('undo find replace', 'zt.cell(0,0).note==first and zt.cell(0,0).instrument==inst_before')
    key('ctrl+y')
    check('redo find replace', 'zt.cell(0,0).note==replacement')

    # Cloning records the destination, and undo returns to the source pattern.
    key('ctrl+shift+d')
    lua('clone=zt.cur_pattern()')
    check('clone', 'clone~=0 and zt.cell(0,0,clone).note==replacement')
    key('ctrl+z')
    check('undo clone', 'zt.cur_pattern()==0 and zt.pattern(clone).empty')
    key('ctrl+y')
    check('redo clone', 'zt.cur_pattern()==clone and zt.cell(0,0).note==replacement')

    # History survives saving; replacing the song clears undo AND redo.
    lua(f'check("save song", zt.save("{saved}"))')
    checks.append('save song')
    key('ctrl+z')
    check('save keeps undo', 'zt.cur_pattern()==0 and zt.pattern(clone).empty')
    lua(f'check("load song", zt.load("{saved}"))')
    checks.append('load song')
    lua('zt.set_cur_pattern(clone); loaded=zt.cell(0,0).note')
    key('ctrl+z'); key('ctrl+y')
    check('load clears histories', 'zt.cur_pattern()==clone and zt.cell(0,0).note==loaded')
    key('z'); key('ctrl+z')
    key('alt+n'); key('y')
    key('ctrl+z'); key('ctrl+y')
    check('new song clears histories', 'zt.pattern(0).empty and zt.pattern(clone).empty')

    # Mouse drawing: each stroke is atomic and undo works in draw mode.
    key('shift+grave')
    commands.extend(['mousedown 72 96', 'wait 50', 'mousemove 96 104', 'wait 50',
                     'mousemove 120 112', 'wait 50', 'mouseup 120 112', 'wait 50'])
    check('mouse stroke', 'not zt.cell(0,0).empty and not zt.cell(0,1).empty and not zt.cell(0,2).empty')
    lua('v0=zt.cell(0,0).volume; v1=zt.cell(0,1).volume; v2=zt.cell(0,2).volume')
    key('ctrl+z')
    check('undo entire stroke', 'zt.cell(0,0).empty and zt.cell(0,1).empty and zt.cell(0,2).empty')
    key('ctrl+y')
    check('redo entire stroke', 'zt.cell(0,0).volume==v0 and zt.cell(0,1).volume==v1 and zt.cell(0,2).volume==v2')
    commands.append('quit')
    script = app / 'undo.txt'
    script.write_text('\n'.join(commands) + '\n')
    result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                            cwd=app.parent, capture_output=True, text=True, timeout=60)
    assert result.returncode == 0, result.stderr
    actual = report.read_text().splitlines() if report.exists() else []
    expected = ['ok ' + name for name in checks]
    if actual != expected:
        errors = [line for line in actual if line.startswith('FAIL')]
        missing = [name for name in checks if 'ok ' + name not in actual and 'FAIL ' + name not in actual]
        raise AssertionError(f'Failed: {errors}; missing: {missing}\n{result.stderr}')
print(f'Pattern editor undo/redo: {len(checks)} headless checks passed.')
