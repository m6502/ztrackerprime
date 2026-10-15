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

    # Block transforms overwrite the selected tracks without moving later
    # data or resizing the pattern. Check every field of every cell.
    lua("function signature(c) if c.empty then return '_' end return table.concat({c.note,c.instrument,c.volume,c.length,c.effect,c.effect_data},',') end")
    lua("for t=0,1 do for r=0,15 do local c=zt.cell(t,r,0); c.note=40+t*12+r; c.instrument=t+3; c.volume=80+r; c.length=12+r; c.effect=83; c.effect_data=0x4a00+r end end; zt.cell(2,32,0).note=75; zt.cell(63,63,0).note=91; zt.cell(3,9,0).effect=87; zt.cell(3,9,0).effect_data=16383; zt.cell(0,0,1).note=99; zt.cell(0,62,0).note=89; zt.cell(0,63,0).note=90")
    lua("original={}; for t=0,63 do original[t]={}; for r=0,63 do original[t][r]=signature(zt.cell(t,r,0)) end end")
    lua("function matches(map) if zt.pattern(0).length~=64 then return false end; for t=0,63 do for r=0,63 do if signature(zt.cell(t,r,0))~=(original[t][map(r,t)] or '_') then return false end end end return true end")

    def select_block(first_row, last_row, first_track=0, last_track=1):
        key('ctrl+u')
        lua(f'zt.song.cur_track={first_track}; zt.set_cur_row({first_row})')
        key('ctrl+b')
        lua(f'zt.song.cur_track={last_track}; zt.set_cur_row({last_row})')
        key('ctrl+e')

    unchanged = 'matches(function(r) return r end)'
    expanded = 'matches(function(r,t) if t<=1 and r>=3 and r<=8 then if (r-3)%2==0 then return 3+(r-3)//2 else return -1 end else return r end end)'
    select_block(3,5)
    key('ctrl+shift+e')
    check('expand only selected tracks and overwrite destination', expanded)
    check('expand keeps other patterns', 'zt.cell(0,0,1).note==99')
    key('ctrl+shift+h')
    check('selection follows expanded block for halving', 'matches(function(r,t) if t<=1 and r>=6 and r<=8 then return -1 else return r end end)')
    key('ctrl+z')
    check('undo block halve', expanded)
    key('ctrl+z')
    check('undo block expand', unchanged)
    for chord in ['ctrl+shift+g','ctrl+shift+e','ctrl+shift+h']:
        key(chord)
        check(chord+' requires selection', unchanged)
    key('ctrl+y')
    check('no-selection transforms preserve redo', expanded)
    key('ctrl+z')
    check('undo redone expansion', unchanged)

    select_block(3,5)
    key('ctrl+shift+g')
    duplicated = 'matches(function(r,t) if t<=1 and r>=6 and r<=8 then return r-3 else return r end end)'
    check('duplicate overwrites copy below selected block', duplicated)
    key('ctrl+z')
    check('undo block duplicate', unchanged)
    key('ctrl+y')
    check('redo block duplicate', duplicated)
    key('ctrl+z')
    select_block(3,5)
    key('ctrl+shift+h')
    halved = 'matches(function(r,t) if t<=1 and r>=3 and r<=5 then if r<=4 then return 3+(r-3)*2 else return -1 end else return r end end)'
    check('halve relative odd rows and clear unused block rows', halved)
    key('ctrl+z')
    check('undo block halve restores discarded rows', unchanged)
    key('ctrl+y')
    check('redo block halve', halved)
    key('ctrl+z')

    select_block(62,63,0,0)
    key('ctrl+shift+e')
    clipped = 'matches(function(r,t) if t==0 and r==63 then return -1 else return r end end)'
    check('expansion clips at existing pattern end', clipped)
    check('expansion creates no out-of-range events', 'zt.cell(0,64,0).empty and zt.cell(0,66,0).empty')
    key('ctrl+z')
    check('undo clipped expansion', unchanged)
    select_block(62,63,0,0)
    key('ctrl+shift+g')
    check('duplication beyond pattern end is clipped', unchanged)
    key('ctrl+u')
    lua('zt.song.cur_track=0')
    key('alt+l')
    key('ctrl+shift+e')
    check('alt L selects only current track', 'matches(function(r,t) if t==0 then if r%2==0 then return r//2 else return -1 end else return r end end)')
    key('ctrl+z')
    check('undo full track expansion', unchanged)
    key('alt+l'); key('alt+l')
    key('ctrl+shift+h')
    check('alt L twice applies to entire pattern without resizing', 'matches(function(r) if r<32 then return r*2 else return -1 end end)')
    key('ctrl+shift+e')
    check('selection follows full-pattern halve for expansion', 'matches(function(r) if r%2==0 then return r else return -1 end end)')
    key('ctrl+z'); key('ctrl+z')
    check('undo full-pattern transforms', unchanged)

    select_block(3,5)
    key('ctrl+shift+r')
    reversed_block = 'matches(function(r,t) if t<=1 and r>=3 and r<=5 then return 8-r else return r end end)'
    check('reverse only selected block', reversed_block)
    key('ctrl+z')
    check('undo reverse block', unchanged)
    key('ctrl+y')
    check('redo reverse block', reversed_block)
    key('alt+n'); key('y')

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
