#ifndef ZT_PATTERN_NOTE_FORMAT_H
#define ZT_PATTERN_NOTE_FORMAT_H

#include "module.h"
#include <cctype>
#include <cstring>

char *hex2note(char *str, unsigned char note);

enum class PatternNoteView { Volume, InstrumentEffect, Regular, Effect, Full };

// Both pattern views use the same numeric fields, but the playback view uses
// spaces for a missing instrument and has an extra compact instrument/FX view.
inline char *format_pattern_note(char *str, const event &e, PatternNoteView view,
                                 char missing_instrument = '.') {
    static constexpr char hex[] = "0123456789ABCDEF";
    char *p = str;
    hex2note(p, e.note);
    p += 3;
    auto volume = [&]() {
        *p++ = ' ';
        *p++ = e.vol < 128 ? hex[e.vol >> 4] : '.';
        *p++ = e.vol < 128 ? hex[e.vol & 15] : '.';
    };
    auto instrument = [&]() {
        *p++ = ' ';
        *p++ = e.inst < MAX_INSTS ? '0' + e.inst / 10 : missing_instrument;
        *p++ = e.inst < MAX_INSTS ? '0' + e.inst % 10 : missing_instrument;
    };
    auto effect = [&]() {
        // Preserve the old %s formatting of a NUL effect: no character.
        if (e.effect) *p++ = e.effect < 255 ? std::toupper(e.effect) : '.';
    };

    if (view == PatternNoteView::InstrumentEffect) {
        instrument();
        effect();
    } else {
        if (view == PatternNoteView::Regular || view == PatternNoteView::Full)
            instrument();
        volume();
        if (view == PatternNoteView::Regular || view == PatternNoteView::Full) {
            *p++ = ' ';
            if (!e.length) std::memcpy(p, "...", 3);
            else if (e.length > 999) std::memcpy(p, "INF", 3);
            else {
                p[0] = '0' + e.length / 100;
                p[1] = '0' + e.length / 10 % 10;
                p[2] = '0' + e.length % 10;
            }
            p += 3;
        }
        if (view == PatternNoteView::Effect || view == PatternNoteView::Full) {
            *p++ = ' ';
            effect();
            for (int shift = 12; shift >= 0; shift -= 4)
                *p++ = hex[(e.effect_data >> shift) & 15];
        }
    }
    *p = '\0';
    return str;
}

#endif
