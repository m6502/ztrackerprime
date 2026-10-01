#!/usr/bin/env python3
"""Check startup and live browsing paths using a scratch application directory."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from app_fixture import create_test_app

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import ROOT, function

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='zt-default-directory-') as tmp:
    app, executable = create_test_app(binary, Path(tmp))
    songs = app / 'songs with spaces'
    songs.mkdir()
    (app / 'next').mkdir()
    (app / 'old').mkdir()
    (songs / 'nested').mkdir()
    script = app / 'check.txt'

    def run(directory, expected, autoload=False):
        (app / 'zt.conf').write_text(
            f'skin: default\ndefault_directory: {directory}\n'
            f'autoload_ztfile: {"yes" if autoload else "no"}\n'
            'autoload_ztfile_filename: autoload.zt\n')
        marker = expected / 'cwd-marker'
        marker.unlink(missing_ok=True)
        check_note = 'assert(zt.pattern(0):cell(0,0).note == 60); ' if autoload else ''
        script.write_text(
            'wait 50\nlua ' + check_note +
            "local f=assert(io.open('cwd-marker','w')); f:write('ok'); f:close(); "
            "zt.pattern(0):set_note(0,0,60,0,100); assert(zt.save('autoload.zt'))\n"
            'wait 50\nquit\n')
        result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                                cwd=app.parent, capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, result.stderr
        assert marker.read_text() == 'ok', result.stderr
        assert (expected / 'autoload.zt').is_file()
        assert not (songs / 'zt.conf').exists(), 'Configuration leaked into song directory'
        return result

    run(str(songs), songs)
    run(songs.name, songs, autoload=True)
    run('', app)
    result = run('missing-directory', app)
    assert 'cannot use default directory' in result.stderr
    (app / 'not-a-directory').write_text('file')
    result = run('not-a-directory', app)
    assert 'cannot use default directory' in result.stderr

    # Edit the bound setting through the real config UI, then enter each
    # file browser without restarting. Delays allow repeated Delete keys.
    for browser, shortcut in [('load', 'ctrl+l'), ('save', 'ctrl+s')]:
        (app / 'zt.conf').write_text('skin: default\ndefault_directory: old\n')
        commands = ['key ctrl+f12', 'click 176 132', 'key home']
        commands += ['key delete'] * 3
        commands += ['key ' + char for char in 'next']
        commands += ['key ' + shortcut,
                     f'lua local f=assert(io.open("live-{browser}-marker", "w")); f:write("ok"); f:close()',
                     'quit']
        script.write_text('wait 50\n' + '\nwait 50\n'.join(commands) + '\n')
        result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                                cwd=app.parent, capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, result.stderr
        assert (app / 'next' / f'live-{browser}-marker').read_text() == 'ok', result.stderr
        assert not (app / 'old' / f'live-{browser}-marker').exists()
        assert not (app / 'next' / 'zt.conf').exists()

        # Run the real build's configuration-copy helper, then restart
        # without rewriting the saved config. Both must retain the edit.
        saved_config = (app / 'zt.conf').read_bytes()
        subprocess.run(['cmake', f'-DSOURCE={ROOT / "zt.conf"}',
                        f'-DDESTINATION={app / "zt.conf"}', '-P',
                        str(ROOT / 'cmake/CopyDefaultConfig.cmake')], check=True)
        assert (app / 'zt.conf').read_bytes() == saved_config
        script.write_text('wait 50\nlua local f=assert(io.open("restart-marker", "w")); '
                          'f:write("ok"); f:close()\nquit\n')
        marker = app / 'next' / 'restart-marker'
        marker.unlink(missing_ok=True)
        result = subprocess.run([str(executable), '--headless', '--script', str(script)],
                                cwd=app.parent, capture_output=True, text=True, timeout=20)
        assert result.returncode == 0, result.stderr
        assert marker.read_text() == 'ok', result.stderr

    # A new build directory still receives the shipped defaults.
    fresh_config = app / 'fresh build' / 'zt.conf'
    subprocess.run(['cmake', f'-DSOURCE={ROOT / "zt.conf"}',
                    f'-DDESTINATION={fresh_config}', '-P',
                    str(ROOT / 'cmake/CopyDefaultConfig.cmake')], check=True)
    assert fresh_config.read_bytes() == (ROOT / 'zt.conf').read_bytes()

    # Exercise the production path helper while CWD is the song directory,
    # as it is when saving settings during a session (before shutdown).
    source = app / 'configuration-path.cpp'
    source.write_text('''#include <filesystem>
#include <string>
#include <cassert>
#include <cstdio>
#include <cstring>
char *cur_dir;
class ZTConf {
    std::string applied_default_directory;
public:
    char default_directory[261] = {};
    bool apply_default_directory(bool only_if_changed = false);
};
''' + function('src/conf.cpp', 'static std::string configuration_path()') +
        function('src/conf.cpp', 'bool ZTConf::apply_default_directory(') + '''
int main(int argc, char **argv) {
    assert(argc == 2);
    cur_dir = argv[1];
    const auto expected = std::filesystem::path(cur_dir) / "zt.conf";
    std::filesystem::current_path(expected.parent_path() / "songs with spaces");
    assert(configuration_path() == expected.string());
    ZTConf config;
    strcpy(config.default_directory, "songs with spaces");
    assert(config.apply_default_directory());
    std::filesystem::current_path("nested");
    assert(config.apply_default_directory(true));
    assert(std::filesystem::current_path() == expected.parent_path() / "songs with spaces/nested");
    strcpy(config.default_directory, "next");
    assert(config.apply_default_directory(true));
    assert(std::filesystem::current_path() == expected.parent_path() / "next");
    strcpy(config.default_directory, "not-a-directory");
    assert(!config.apply_default_directory(true));
    assert(std::filesystem::current_path() == expected.parent_path() / "next");
    strcpy(config.default_directory, "missing-then-created");
    assert(!config.apply_default_directory(true));
    std::filesystem::create_directory(expected.parent_path() / "missing-then-created");
    assert(config.apply_default_directory(true));
    assert(std::filesystem::current_path() == expected.parent_path() / "missing-then-created");
    config.default_directory[0] = 0;
    assert(config.apply_default_directory(true));
    assert(std::filesystem::current_path() == expected.parent_path());
}
''')
    probe = app / 'configuration-path'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                   ['-std=c++17', str(source), '-o', str(probe)], check=True)
    subprocess.run([str(probe), str(app)], check=True)

print('Default directory: startup/autoload, live changes, unchanged browsing and invalid-path retry passed.')
