// preset_data.h
//
// Pure-data preset tables + apply functions for the F4 (MIDI Macro)
// and Shift+F4 (Arpeggio) editors. Extracted from CUI_Midimacroeditor.cpp
// and CUI_Arpeggioeditor.cpp so the same logic can be exercised from
// a unit-test harness without pulling in SDL / UI globals.
//
// The "*_apply_preset_to" functions write only into the passed-in
// arpeggio / midimacro structs. They do NOT touch globals (file_changed,
// statusmsg, ar_slot, mm_slot, etc.); the editors keep their existing
// wrappers for those side-effects.

#ifndef ZT_PRESET_DATA_H
#define ZT_PRESET_DATA_H

#include <cstring>
#include <cstdint>
#include "module.h"

// ---------------------------------------------------------------------------
// MIDI Macro presets

struct mm_preset {
    const char *name;
    const unsigned short *data;
    int len;
};

inline const unsigned short ZT_PRESET_CC_MOD[]    = { 0xB0, 0x01, 0x101 };       // CC1  Modulation, value=PARAM1
inline const unsigned short ZT_PRESET_CC_FILT[]   = { 0xB0, 0x4A, 0x101 };       // CC74 Filter cutoff
inline const unsigned short ZT_PRESET_CC_RES[]    = { 0xB0, 0x47, 0x101 };       // CC71 Resonance
inline const unsigned short ZT_PRESET_PROG_CHG[]  = { 0xC0, 0x101 };             // Program change
inline const unsigned short ZT_PRESET_PITCH_BEND[]= { 0xE0, 0x00, 0x101 };       // Pitch wheel coarse
inline const unsigned short ZT_PRESET_ALL_NOTES[] = { 0xB0, 0x7B, 0x00 };        // CC123 All Notes Off

inline const mm_preset MM_PRESETS[] = {
    { "CC 1 Modulation (param=value)",     ZT_PRESET_CC_MOD,     3 },
    { "CC 74 Filter Cutoff (param=value)", ZT_PRESET_CC_FILT,    3 },
    { "CC 71 Resonance (param=value)",     ZT_PRESET_CC_RES,     3 },
    { "Program Change (param=program)",    ZT_PRESET_PROG_CHG,   2 },
    { "Pitch Bend Coarse (param=value)",   ZT_PRESET_PITCH_BEND, 3 },
    { "All Notes Off",                     ZT_PRESET_ALL_NOTES,  3 },
};
inline const int MM_PRESET_COUNT = (int)(sizeof(MM_PRESETS) / sizeof(MM_PRESETS[0]));

// Apply preset `idx` into `m`. No-op on null/out-of-range.
inline void mm_apply_preset_to(midimacro *m, int idx) {
    if (!m || idx < 0 || idx >= MM_PRESET_COUNT) return;
    const mm_preset &p = MM_PRESETS[idx];
    for (int i = 0; i < ZTM_MIDIMACRO_MAXLEN; ++i) m->data[i] = ZTM_MIDIMAC_END;
    for (int i = 0; i < p.len; ++i) m->data[i] = p.data[i];
    if (p.len < ZTM_MIDIMACRO_MAXLEN) m->data[p.len] = ZTM_MIDIMAC_END;
    std::memset(m->name, 0, ZTM_MIDIMACRONAME_MAXLEN);
    std::strncpy(m->name, p.name, ZTM_MIDIMACRONAME_MAXLEN - 1);
}

// ---------------------------------------------------------------------------
// Arpeggio presets

struct ar_preset {
    const char *name;
    int length;
    int speed;
    int repeat_pos;
    int pitches[16];   // semitone offsets (negative = flatten by ZTM_ARP_EMPTY_PITCH cast)
    int len_pitches;
};

inline const ar_preset AR_PRESETS[] = {
    { "Major Triad Up",        3,  6, 0, { 0, 4, 7 },                  3 },
    { "Minor Triad Up",        3,  6, 0, { 0, 3, 7 },                  3 },
    { "Major Triad Up+Octave", 4,  6, 0, { 0, 4, 7, 12 },              4 },
    { "Octave Bounce",         2,  6, 0, { 0, 12 },                    2 },
    { "Trill (whole step)",    2,  3, 0, { 0, 2 },                     2 },
    { "Trill (half step)",     2,  3, 0, { 0, 1 },                     2 },
    { "Major 7 Chord",         4,  6, 0, { 0, 4, 7, 11 },              4 },
    { "Minor 7 Chord",         4,  6, 0, { 0, 3, 7, 10 },              4 },
    { "Major Scale (1 oct)",   8,  4, 0, { 0, 2, 4, 5, 7, 9, 11, 12 }, 8 },
    { "Minor Scale (1 oct)",   8,  4, 0, { 0, 2, 3, 5, 7, 8, 10, 12 }, 8 },
};
inline const int AR_PRESET_COUNT = (int)(sizeof(AR_PRESETS) / sizeof(AR_PRESETS[0]));

inline void ar_apply_preset_to(arpeggio *a, int idx) {
    if (!a || idx < 0 || idx >= AR_PRESET_COUNT) return;
    const ar_preset &p = AR_PRESETS[idx];
    a->length     = p.length;
    a->speed      = p.speed;
    a->repeat_pos = p.repeat_pos;
    a->num_cc     = 0;
    for (int i = 0; i < ZTM_ARPEGGIO_LEN; ++i) a->pitch[i] = ZTM_ARP_EMPTY_PITCH;
    for (int i = 0; i < p.len_pitches; ++i)
        a->pitch[i] = (unsigned short)(int16_t)p.pitches[i];
    std::memset(a->name, 0, ZTM_ARPEGGIONAME_MAXLEN);
    std::strncpy(a->name, p.name, ZTM_ARPEGGIONAME_MAXLEN - 1);
}

#endif // ZT_PRESET_DATA_H
