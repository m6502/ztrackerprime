// Minimal arpeggio / midimacro constructors for the unit-test harness.
//
// The real implementations in src/module.cpp are bracketed with
// USE_ARPEGGIOS / USE_MIDIMACROS and the file pulls in zt.h (which
// transitively pulls in SDL, the Lua engine, and the world). Tests
// only need the data classes' default-constructed state, so we
// re-state them here as small inline copies of module.cpp's bodies.
//
// Keep these in sync if the real constructors change.

#include "module.h"

#ifdef USE_MIDIMACROS
midimacro::midimacro(void) {
    name[0] = 0;
    data[0] = ZTM_MIDIMAC_END;
}
midimacro::~midimacro(void) {}
int midimacro::isempty(void) {
    return (data[0] == ZTM_MIDIMAC_END && name[0] == 0);
}
#endif

#ifdef USE_ARPEGGIOS
arpeggio::arpeggio(void) {
    name[0] = 0;
    repeat_pos = 0;
    length = 0;
    speed = 1;
    num_cc = 0;
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; i++) cc[i] = 0;
    for (int i = 0; i < ZTM_ARPEGGIO_LEN; i++) {
        pitch[i] = ZTM_ARP_EMPTY_PITCH;
        for (int j = 0; j < ZTM_ARPEGGIO_NUM_CC; j++) ccval[j][i] = ZTM_ARP_EMPTY_CCVAL;
    }
}
arpeggio::~arpeggio(void) {}
int arpeggio::isempty(void) {
    return (length == 0 && name[0] == 0);
}
#endif
