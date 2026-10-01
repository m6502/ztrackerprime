#!/usr/bin/env python3
"""Check production filename preparation and save dispatch without writing files."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import function

source = r'''
#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
char save_filename[261], szStatmsg[1024];
char *statusmsg = nullptr;
int status_change = 0, need_refresh = 0, saves = 0, popups = 0;
using VFunc = void (*)();
struct Button { int state = 0; } buttons[8];
struct TestUI { Button *get_element(int i) { return &buttons[i]; } } ui;
struct Page { TestUI *UI; } page{&ui};
Page *UIP_Savescreen = &page;
struct SaveMsg { int filetype = 0; } save_msg;
SaveMsg *UIP_SaveMsg = &save_msg;
struct Confirm { const char *str; VFunc OnYes; } confirm;
Confirm *UIP_RUSure = &confirm;
bool existing = false;
void do_save() { saves++; }
void popup_window(Confirm *) { popups++; }
bool file_exists(const char *) { return existing; }
'''
source += function('src/main.cpp', 'int zcmpi(const char *s1, const char *s2)')
source += function('src/CUI_Savescreen.cpp', 'static bool prepare_save_filename(')
source += function('src/CUI_Savescreen.cpp', 'void begin_save(void)')
source += r'''
void check(int type, const std::string &input, const std::string &expected, bool valid,
           bool file_exists = false) {
    for (auto &b : buttons) b.state = 0;
    buttons[type + 3].state = 1;
    memset(save_filename, 0, sizeof(save_filename));
    memcpy(save_filename, input.data(), input.size());
    saves = popups = need_refresh = status_change = 0;
    existing = file_exists;
    begin_save();
    assert(std::string(save_filename) == expected);
    assert(save_msg.filetype == type);
    if (!valid) {
        assert(saves == 0 && popups == 0);
        assert(status_change && need_refresh && statusmsg == szStatmsg);
    } else if (file_exists && type != 4) {
        assert(popups == 1 && saves == 0 && confirm.OnYes == do_save);
    } else {
        assert(saves == 1 && popups == 0);
    }
}
int main() {
    for (int type = 1; type <= 4; type++) {
        const std::string ext = type == 1 ? ".zt" : ".mid";
        check(type, "", "", false);
        check(type, "   ", "", false);
        check(type, std::string(260, ' '), "", false);
        check(type, "a", "a" + ext, true);
        check(type, "song name   ", "song name" + ext, true);
        check(type, "song" + ext + "  ", "song" + ext, true);
        const std::string upper = type == 1 ? "song.ZT" : "song.MID";
        check(type, upper, upper, true);
        check(type, "song", "song" + ext, true, true);
        check(type, std::string(260 - ext.size(), 'x'),
              std::string(260 - ext.size(), 'x') + ext, true);
        check(type, std::string(261 - ext.size(), 'x'),
              std::string(261 - ext.size(), 'x'), false);
        const std::string full = std::string(260 - ext.size(), 'x') + ext;
        check(type, full, full, true);
    }
    memset(save_filename, 'x', sizeof(save_filename));
    assert(!prepare_save_filename(".zt")); // Unterminated input is rejected.
}
'''
with tempfile.TemporaryDirectory(prefix='zt-save-filename-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    flags = ['-std=c++17', '-O2', '-Wall', '-Wextra']
    if '--sanitize' in sys.argv:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + flags +
                   [str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Save filenames: empty/spaces, extensions, capacity limits and all four formats passed.')
