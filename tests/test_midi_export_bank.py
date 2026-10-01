#!/usr/bin/env python3
"""Check the production program-change encoders with bank and channel separated."""
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
#include <vector>
struct CDataBuf {
    std::vector<unsigned char> bytes;
    void pushuc(unsigned char b) { bytes.push_back(b); }
    void pushc(unsigned char b) { bytes.push_back(b); }
};
'''
source += function('src/playback.h', 'enum Emeventtypes') + ';\n'
source += function('src/playback.h', 'struct midi_event') + ';\n'
source += function('src/import_export.cpp', 'void push_varlen(')
source += function('src/import_export.cpp', 'static void push_midi_event(')

# ExportMID has its own inline switch; test its verbatim ET_PC branch too.
export = function('src/import_export.cpp', 'int ZTImportExport::ExportMID(const char *fn, int format)')
branch = export[export.index('case ET_PC:'):export.index('case ET_CC:')]
source += '''
void legacy_export(CDataBuf *mp, midi_event *e, int &delay) {
    const int trk = 0;
    int dtime[] = {delay};
    switch(e->type) {
''' + branch + '''
    default: break;
    }
    delay = dtime[0];
}
'''
source += r'''
int main() {
    const unsigned short banks[] = {0, 1, 127, 128, 0x1234, 0x3FFF, 0x4000, 0xFFFF};
    for (auto bank : banks) {
        for (int channel = 0; channel < 16; channel++) {
            for (int delta : {0, 127, 128, 16383}) {
                midi_event event{};
                event.type = ET_PC;
                event.command = 42;
                event.data1 = bank;
                event.data2 = channel;
                // Preserve the existing bank-byte encoding; the regression
                // concerns the source field and disabled-bank sentinel.
                std::vector<unsigned char> expected;
                if (delta >= 128) expected.push_back((delta >> 7) | 0x80);
                expected.push_back(delta & 0x7F);
                if (bank <= 0x3FFF) {
                    expected.insert(expected.end(), {
                        (unsigned char)(0xB0 + channel), 0, (unsigned char)(bank & 127),
                        0, (unsigned char)(0xB0 + channel), 32, (unsigned char)(bank >> 7), 0});
                }
                expected.push_back(0xC0 + channel);
                expected.push_back(42);
                for (auto encode : {legacy_export, push_midi_event}) {
                    CDataBuf output;
                    int delay = delta;
                    encode(&output, &event, delay);
                    assert(output.bytes == expected);
                    assert(delay == 0);
                }
            }
        }
    }
}
'''
with tempfile.TemporaryDirectory(prefix='zt-midi-bank-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(source)
    exe = Path(tmp) / 'test'
    subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) +
                   ['-std=c++17', '-O2', '-Wall', '-Wextra', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('MIDI bank export: both encoders, valid/disabled banks, 16 channels and delta times passed.')
