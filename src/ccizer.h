#ifndef _ZT_CCIZER_H_
#define _ZT_CCIZER_H_

// CCizer — Paketti-format CC bank loader for the Shift+F3 CC Console page.
//
// File format (one slot per non-comment, non-empty line):
//
//     <CC# 0..127 | "PB"> <Name>
//
// Lines starting with '#' are comments. Blank lines OK. Slot index is the
// line's position among non-comment/non-empty lines (1-based).
//
// Sidecar file `<basename>.cc-view` next to each `.txt` stores per-slot view
// hints (slider | knob), one per line:
//
//     1 slider
//     2 knob
//
// Missing entries default to slider. Sidecars keep the CCizer .txt files
// untouched so they remain compatible with Paketti.

#include <stddef.h>

#define ZT_CCIZER_PB_MARKER 0xFF   // sentinel for pitchbend slots (status 0xE0)
#define ZT_CCIZER_MAX_SLOTS 128
#define ZT_CCIZER_VIEW_SLIDER 0
#define ZT_CCIZER_VIEW_KNOB   1

struct ZtCcizerSlot {
    unsigned char cc;          // 0..127, or ZT_CCIZER_PB_MARKER for pitchbend
    char name[64];
    unsigned char view;        // ZT_CCIZER_VIEW_SLIDER | _KNOB
    unsigned short value;      // 0..127 normally, 0..16383 when cc==PB_MARKER

    // MIDI Learn: incoming (status, data1) that maps to this slot.
    // learn_status == 0 => unmapped. Persisted in the `.cc-midi` sidecar.
    unsigned char learn_status;
    unsigned char learn_data1;
};

struct ZtCcizerFile {
    char path[1024];           // absolute path to the .txt
    char basename[256];        // e.g. "sc88st.txt"
    int  num_slots;
    ZtCcizerSlot slots[ZT_CCIZER_MAX_SLOTS];
};

// Parse a CCizer file. Returns 0 on success, non-zero on I/O error.
// Malformed lines are skipped; well-formed slots accumulate in `out`.
// Sidecar `<path>.cc-view` (replace .txt extension) is read if present.
int  zt_ccizer_load(const char *path, ZtCcizerFile *out);

// Write the .cc-view sidecar for `f` based on each slot's `view` field.
// Path is `<f->path>` with the trailing ".txt" replaced by ".cc-view".
// Returns 0 on success.
int  zt_ccizer_save_view_sidecar(const ZtCcizerFile *f);

// Write the .cc-midi sidecar for `f` based on each slot's learn_status
// / learn_data1 fields. Slots with learn_status==0 are skipped. Path is
// `<f->path>` with `.txt` replaced by `.cc-midi`.
int  zt_ccizer_save_learn_sidecar(const ZtCcizerFile *f);

// Match an incoming (status, data1) pair against any learnt slot. Returns
// the matching slot index (0-based) or -1 on no match.
int  zt_ccizer_find_learn_match(const ZtCcizerFile *f,
                                unsigned char status,
                                unsigned char data1);

// Scan `dir` for files ending in ".txt". Returns sorted basenames in `out`,
// up to `max_results`. Returns count.
int  zt_ccizer_list_dir(const char *dir, char (*out)[256], int max_results);

// Build the (status, data1, data2) bytes for sending slot `s` at the given
// MIDI channel (1..16). For PB slots, value is 14-bit (0..16383); for CC
// slots, value is 7-bit (0..127). Returns 1 if a 3-byte short message was
// produced; 0 on invalid args.
int  zt_ccizer_build_midi(const ZtCcizerSlot *s, int channel,
                          unsigned char *status,
                          unsigned char *data1,
                          unsigned char *data2);

// Resolve a default ccizer folder path. Search order:
//   1. `override` if non-NULL and non-empty
//   2. `<exe_dir>/ccizer/`
//   3. `~/.config/zt/ccizer/` (or platform equivalent)
// Returns 0 if a usable folder was found and copied to `out` (size `out_sz`).
int  zt_ccizer_resolve_folder(const char *override_path,
                              const char *exe_dir,
                              char *out, size_t out_sz);

#endif // _ZT_CCIZER_H_
