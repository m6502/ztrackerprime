ztracker is a win32 MIDI only tracker with an interface that is almost a 1:1 clone
of the popular Impulse Tracker DOS tracking software.

It supports multiple midi-out devices, 32 midi tracks (or 64, or 128, it doesnt
matter), it can sync to an external sequencer via midi clock, and more...

The original ztracker code (upto 0.82) was written completely by Christopher Micali,
except for the impulse tracker loader code, which was written by Austin Luminais.

The license for ztracker is in the file LICENSE.txt, read it!
See the file BUILDING.txt for information on how to build the source code.

Visit the ztracker home page at http://ztracker.sourceforge.net/


SOURCE CODE STYLE:
------------------

Indent:

Use indent-size=4 and use spaces instead of tabs.

In MSVC select 'Tools' / 'Options...', and under the 'Tabs' tab, first select 'File type'
C/C++, then set indent-size=4 and tab-size=8, and check the 'Insert spaces' option.

Ex:
    int multiply(int x, int y) {
        return x*y;
    }

