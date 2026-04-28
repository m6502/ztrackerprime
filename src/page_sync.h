// page_sync.h
//
// Pure helpers that mirror the per-frame work CUI_Arpeggioeditor::update
// and CUI_Midimacroeditor::update do AFTER UI->update() returns: pull
// freshly-applied preset values into slider widgets, then run the
// "widgets -> data" sync that lets manual slider changes flow into the
// arpeggio / midimacro structs.
//
// Without these helpers the order-of-operations between OnSelect and
// the sync block silently bit us: applying a preset wrote new values
// into arpeggios[ar_slot], but the slider widgets still held the OLD
// values, so the sync block immediately overwrote the fresh data with
// the stale widget values. The page appeared to "select but not
// change" / "freeze on Enter" depending on which keypath was used.
//
// The unit tests in tests/test_page_sync.cpp exercise the exact
// scenarios that broke during PR #64 development.

#ifndef ZT_PAGE_SYNC_H
#define ZT_PAGE_SYNC_H

#include "module.h"

// ---------------------------------------------------------------------------
// Arpeggio editor

// Slider widget snapshot. Each field mirrors the value field of the
// corresponding ValueSlider element on the page. Plain ints to keep the
// helper SDL-free.
struct ArpSliders {
    int length;
    int speed;
    int repeat_pos;
    int num_cc;
    int cc[ZTM_ARPEGGIO_NUM_CC];
};

// Pull arpeggio data into the slider widgets. Called by the editor
// page right after a preset apply (preset_just_applied flag) -- without
// this, the next sync_arp_widgets_to_data() call would clobber the
// freshly-applied data with stale widget values.
inline void refresh_arp_widgets_from_data(ArpSliders &widgets,
                                          const arpeggio &data) {
    widgets.length     = data.length;
    widgets.speed      = data.speed;
    widgets.repeat_pos = data.repeat_pos;
    widgets.num_cc     = data.num_cc;
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
        widgets.cc[i] = data.cc[i];
}

// Apply the inverse direction: when the user moves a slider, push the
// new value into the data struct. Returns true if anything actually
// changed (caller uses this to decide whether to bump file_changed).
inline bool sync_arp_widgets_to_data(const ArpSliders &widgets,
                                     arpeggio &data) {
    bool changed =
        data.length     != widgets.length     ||
        data.speed      != widgets.speed      ||
        data.repeat_pos != widgets.repeat_pos ||
        data.num_cc     != widgets.num_cc     ||
        data.cc[0] != (unsigned char)widgets.cc[0] ||
        data.cc[1] != (unsigned char)widgets.cc[1] ||
        data.cc[2] != (unsigned char)widgets.cc[2] ||
        data.cc[3] != (unsigned char)widgets.cc[3];
    if (!changed) return false;
    data.length     = widgets.length;
    data.speed      = widgets.speed;
    data.repeat_pos = widgets.repeat_pos;
    data.num_cc     = widgets.num_cc;
    for (int i = 0; i < ZTM_ARPEGGIO_NUM_CC; ++i)
        data.cc[i] = (unsigned char)widgets.cc[i];
    // Keep repeat_pos sane vs new length (matches CUI_Arpeggioeditor).
    if (data.length > 0 && data.repeat_pos >= data.length)
        data.repeat_pos = data.length - 1;
    return true;
}

// ---------------------------------------------------------------------------
// MIDI Macro editor

struct MmSliders {
    int length;   // mm_length(m): index of first ZTM_MIDIMAC_END
};

inline void refresh_mm_widgets_from_data(MmSliders &widgets,
                                         const midimacro &data) {
    int len = 0;
    while (len < ZTM_MIDIMACRO_MAXLEN && data.data[len] != ZTM_MIDIMAC_END)
        ++len;
    widgets.length = len;
}

#endif // ZT_PAGE_SYNC_H
