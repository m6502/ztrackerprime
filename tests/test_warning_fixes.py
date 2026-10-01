#!/usr/bin/env python3
"""Boundary checks for device-name encoding, IT import fields and mute macros."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from drawing_harness import ROOT, function

source = '''#include <cassert>
#include <cstring>
#include <string>
'''
source += function('src/main.cpp', 'void encode(char *str, char w[256])')
source += function('src/CUI_Sysconfig.cpp', 'static void encode_dev_key(')
source += r'''
struct ITInstrument { unsigned short midiBank; unsigned char midiChannel, midiProgram; } input;
struct ITModule { ITInstrument &Instruments(int) { return input; } } mod;
struct Instrument { signed short bank; unsigned char channel; signed char patch; } instrument;
struct Module { Instrument *instruments[1] = {&instrument}; } module;
void import_values() {
    auto *zt = &module;
    const int i = 0;
'''
importer = function('src/import_export.cpp', 'int ZTImportExport::ImportIT(')
source += importer[importer.index('const int bank ='):importer.index('//strcpy((char *)&zt->instruments[i]->title')]
source += ' }\n'
source += '\n'.join(line for line in (ROOT / 'src/zt.h').read_text().splitlines()
                    if line.startswith(('#define mutetrack(', '#define unmutetrack(',
                                        '#define toggle_track_mute('))) + '\n'
source += r'''
struct Song { int track_mute[2] = {}; } song_value;
auto *song = &song_value;
struct MIDI {
    int mutes = 0, unmutes = 0;
    void mute_track(int) { mutes++; }
    void unmute_track(int) { unmutes++; }
} midi;
auto *MidiOut = &midi;
int main() {
    for (int length : {0, 1, 254, 255, 256, 1024}) {
        std::string name(length, 'a');
        if (length > 3) name[3] = '/';
        struct Guard { char before[8], text[256], after[8]; } a, b;
        memset(&a, '!', sizeof(a));
        memset(&b, '!', sizeof(b));
        encode(name.data(), a.text);
        encode_dev_key(name.c_str(), b.text);
        assert(strcmp(a.text, b.text) == 0);
        assert(strlen(a.text) == (size_t)(length < 255 ? length : 255));
        if (length > 3) assert(a.text[3] == '_');
        for (int i = 0; i < 8; i++)
            assert(a.before[i] == '!' && a.after[i] == '!' &&
                   b.before[i] == '!' && b.after[i] == '!');
    }
    char blank[256];
    encode(nullptr, blank); assert(blank[0] == 0);
    encode_dev_key(nullptr, blank); assert(blank[0] == 0);
    for (int value = 0; value <= 65535; value++) {
        input.midiBank = value;
        input.midiChannel = value & 255;
        input.midiProgram = value & 255;
        import_values();
        assert(instrument.bank == (value <= 16383 ? value : -1));
        const int channel = (value & 255) - 1;
        assert(instrument.channel == (channel >= 0 && channel < 16 ? channel : 0));
        const int program = value & 255;
        assert(instrument.patch == (program <= 127 ? program : -1));
    }
    if (false) mutetrack(0); else assert(midi.mutes == 0);
    if (false) unmutetrack(0); else assert(midi.unmutes == 0);
    if (false) toggle_track_mute(0); else assert(song->track_mute[0] == 0);
    mutetrack(0); assert(song->track_mute[0] == 1 && midi.mutes == 1);
    unmutetrack(0); assert(song->track_mute[0] == 0 && midi.unmutes == 1);
    toggle_track_mute(0); assert(song->track_mute[0] == 1 && midi.mutes == 2);
    toggle_track_mute(0); assert(song->track_mute[0] == 0 && midi.unmutes == 2);
}
'''
with tempfile.TemporaryDirectory(prefix='zt-warning-fixes-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    flags = ['-std=c++17', '-O2', '-Wall', '-Wextra']
    if '--sanitize' in sys.argv:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + flags +
                   [str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Warning fixes: bounded device names, IT field ranges and conditional mute macros passed.')
