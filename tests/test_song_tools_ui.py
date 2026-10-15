#!/usr/bin/env python3
"""Exercise Pattern tools through real keys, clicks, scope switches and undo."""
from pathlib import Path
import subprocess
import sys
import tempfile
from app_fixture import create_test_app

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-song-tools-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    (app / 'zt.conf').write_text('skin: default\ndefault_directory: \n')
    report = app / 'checks.txt'
    commands = ['wait 100', f'lua function check(n,c) local f=assert(io.open("{report}","a")); f:write((c and "ok " or "FAIL ")..n.."\\n"); f:close() end']
    checks = []
    rows = [5,8,16,19,22,29,32,35,38,45]

    def key(chord):
        commands.extend([f'key {chord}', 'wait 70'])

    def lua(code):
        commands.append('lua ' + code)

    def check(name, expression):
        checks.append(name)
        lua(f'check("{name}", {expression})')

    def tools():
        key('ctrl+shift+f2')

    def click(action):
        # Default 840x480 logical screen; buttons are full width and stacked.
        commands.extend([f'click {27*8} {(6+rows[action])*8+3}', 'wait 70'])

    def destination(pattern, track=0):
        lua(f'zt.song.cur_pattern={pattern}; zt.song.cur_track={track}; zt.set_cur_row(0)')

    # Disabled actions must neither mutate the song nor leave the input FIFO stuck.
    lua('for p=0,12 do zt.pattern(p):set_note(0,0,40+p,0,90) end; zt.orders[0]=1; zt.orders[1]=4; zt.orders[2]=3; zt.orders[3]=7; zt.orders[4]=3; zt.orders[5]=7; zt.orders[6]=9; zt.orders[20]=11')
    tools()
    commands.append(f'shot {app}/initial.png')
    for action in [3,4,7,8]:
        click(action)
    key('esc')
    check('disabled actions do nothing and input remains responsive', 'zt.cell(0,0,0).note==40 and zt.orders[0]==1')

    # The keyboard path also skips disabled buttons.
    tools(); key('tab'); key('tab'); key('enter')  # Mark Pattern, source 000.
    key('esc')
    destination(3)
    tools()
    commands.append(f'shot {app}/pattern-source.png')
    click(3); key('esc')
    check('move pattern into position with shifting', 'zt.cell(0,0,3).note==40 and zt.cell(0,0,0).note==41 and zt.cell(0,0,1).note==42 and zt.orders[0]==0 and zt.orders[2]==2')
    key('ctrl+z')
    check('undo pattern move and order remap', 'zt.cell(0,0,0).note==40 and zt.cell(0,0,3).note==43 and zt.orders[0]==1 and zt.orders[2]==3')
    key('ctrl+y')
    check('redo pattern move', 'zt.cell(0,0,3).note==40 and zt.orders[0]==0')
    key('ctrl+z')
    destination(4)
    tools(); click(4); key('esc')
    check('marked source follows undo and swap', 'zt.cell(0,0,4).note==40 and zt.cell(0,0,0).note==44 and zt.orders[1]==0')
    key('ctrl+z')

    # Defrag: first occurrence, repeats, blank gaps and remaining slot order.
    destination(7)
    tools(); click(0); key('esc')
    check('defrag used patterns and order list', 'zt.orders[0]==0 and zt.orders[1]==1 and zt.orders[2]==2 and zt.orders[3]==3 and zt.orders[4]==2 and zt.orders[5]==3 and zt.orders[6]==4 and zt.orders[20]==5')
    check('defrag preserves parked sections and unused pattern order', 'zt.cell(0,0,5).note==51 and zt.cell(0,0,6).note==40 and zt.cell(0,0,7).note==42 and zt.cur_pattern()==3')
    key('ctrl+z')
    check('undo defrag', 'zt.cur_pattern()==7 and zt.orders[0]==1 and zt.orders[20]==11 and zt.cell(0,0,0).note==40')
    key('ctrl+y'); key('ctrl+z')
    tools(); click(1); key('esc')
    check('clear only unused patterns including after blank protection', 'zt.pattern(0).empty and zt.pattern(2).empty and zt.pattern(12).empty and zt.cell(0,0,1).note==41 and zt.cell(0,0,11).note==51 and zt.orders[20]==11')
    key('ctrl+z')
    check('undo clear unused', 'zt.cell(0,0,0).note==40 and zt.cell(0,0,12).note==52')
    key('ctrl+y'); key('ctrl+z')

    # Distinct track data across patterns and global cosmetic/mute properties.
    lua("for p=0,2 do for t=0,3 do zt.cell(t,1,p).note=50+p*12+t; zt.cell(t,1,p).instrument=t; zt.cell(t,1,p).volume=80+t; zt.cell(t,1,p).length=12+t; zt.cell(t,1,p).effect=83; zt.cell(t,1,p).effect_data=0x4a00+t end end; zt.track(0).muted=true; zt.track(2).muted=false")
    destination(0,0)
    tools(); click(6); key('esc')
    destination(1,2)
    tools(); click(7); key('esc')
    check('current-pattern track move', 'zt.cell(2,1,1).note==62 and zt.cell(0,1,1).note==63 and zt.cell(1,1,1).note==64 and zt.cell(0,1,0).note==50 and zt.cell(0,1,2).note==74')
    check('current-pattern move keeps song-wide mute state', "zt.track(0).muted and not zt.track(2).muted")
    check('track move keeps every event field', 'zt.cell(2,1,1).instrument==0 and zt.cell(2,1,1).volume==80 and zt.cell(2,1,1).length==12 and zt.cell(2,1,1).effect==83 and zt.cell(2,1,1).effect_data==0x4a00')
    key('ctrl+z')
    check('undo track move', 'zt.cur_track()==2 and zt.cell(0,1,1).note==62 and zt.cell(2,1,1).note==64')
    key('ctrl+y'); key('ctrl+z')
    tools(); click(8); key('esc')
    check('current-pattern track swap', 'zt.cell(0,1,1).note==64 and zt.cell(2,1,1).note==62 and zt.cell(1,1,1).note==63 and zt.cell(0,1,2).note==74')
    key('ctrl+z')
    tools(); click(5)
    commands.append(f'shot {app}/whole-song.png')
    click(7); key('esc')
    check('whole-song move affects every pattern', 'zt.cell(2,1,0).note==50 and zt.cell(2,1,1).note==62 and zt.cell(2,1,2).note==74 and zt.cell(0,1,0).note==51')
    check('whole-song move carries mute state', "zt.track(2).muted and not zt.track(1).muted")
    key('ctrl+z')
    check('undo whole-song track data and properties', "zt.cell(0,1,2).note==74 and zt.track(0).muted and not zt.track(2).muted")
    key('ctrl+y'); key('ctrl+z')
    tools(); click(8); key('esc')
    check('whole-song swap preserves intervening tracks', "zt.cell(0,1,0).note==52 and zt.cell(2,1,2).note==74 and zt.cell(1,1,1).note==63 and zt.track(2).muted")
    key('ctrl+z')

    # New Song clears both source selections; stale enabled buttons are inert.
    key('alt+n'); key('y')
    destination(4,2)
    lua('zt.cell(0,0,0).note=60; zt.cell(0,0,4).note=72; zt.cell(2,0,4).note=80')
    tools()
    commands.append(f'shot {app}/new-song.png')
    for action in [3,4,7,8]:
        click(action)
    key('esc')
    check('new song resets marked sources', 'zt.cell(0,0,0).note==60 and zt.cell(0,0,4).note==72 and zt.cell(2,0,4).note==80')
    saved = app / 'source-reset.zt'
    lua(f'check("save marked-source fixture", zt.save("{saved}"))')
    checks.append('save marked-source fixture')
    tools(); click(2); click(6); key('esc')
    destination(0,0)
    lua(f'check("reload marked-source fixture", zt.load("{saved}"))')
    checks.append('reload marked-source fixture')
    tools()
    for action in [3,4,7,8]:
        click(action)
    key('esc')
    check('song load resets marked sources', 'zt.cell(0,0,0).note==60 and zt.cell(0,0,4).note==72 and zt.cell(2,0,4).note==80')
    destination(4,2)
    # Regular F2 and Shift+F2 still open their original pages.
    key('f2'); key('esc')
    key('shift+f2'); key('f2')
    lua('zt.set_cur_row(3)')
    key('z')
    check('original F2 shortcuts still return to editor', 'not zt.cell(2,3,4).empty')
    commands.append('quit')
    script = app / 'tools.txt'
    script.write_text('\n'.join(commands) + '\n')
    result = subprocess.run([str(executable),'--headless','--script',str(script)],
                            cwd=app.parent,capture_output=True,text=True,timeout=60)
    assert result.returncode == 0, result.stderr
    actual = report.read_text().splitlines() if report.exists() else []
    expected = ['ok ' + name for name in checks]
    if actual != expected:
        errors = [line for line in actual if line.startswith('FAIL')]
        missing = [name for name in checks if 'ok '+name not in actual and 'FAIL '+name not in actual]
        raise AssertionError(f'Failed: {errors}; missing: {missing}\n{result.stderr}')
print(f'Pattern tools: {len(checks)} headless keyboard/mouse/data checks passed.')
