#ifndef _ZT_SYSEX_MACRO_H_
#define _ZT_SYSEX_MACRO_H_

// Convention: a midimacro whose `name` ends in ".syx" (case-insensitive)
// is dispatched by the playback engine as a SysEx file send. The macro's
// data array is ignored; instead the file at `<syx_folder>/<name>` is
// read and sent via MidiOut->sendSysEx.
//
// This keeps the .zt save format unchanged: the existing MMAC chunk
// already persists `name`, so nothing extra needs to round-trip.
//
// Old zTracker reading a song with a `.syx`-named macro sees an empty
// data array (no END token to walk) and dispatches nothing — safe
// forward-compat (the macro is silently inert on older builds).

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Predicate: does `name` end in ".syx" (case-insensitive)?
int  zt_sysex_macro_is_file(const char *name);

// Build the absolute path for a SysEx-named macro by joining the
// effective syx_folder (matches CUI_SysExLibrarian's resolution) with
// `name`. Returns 0 on success and writes a null-terminated path into
// `out` (capacity `out_cap`).
int  zt_sysex_macro_resolve_path(const char *name, char *out, size_t out_cap);

// Read a `.syx` file into a heap buffer. Caller frees with `free()`.
// On success returns 1 and sets *out_buf and *out_len; on failure
// returns 0. Refuses files that don't start with F0 / end with F7 or
// exceed `max_len` bytes.
int  zt_sysex_macro_read_file(const char *path,
                              unsigned char **out_buf,
                              int *out_len,
                              int max_len);

#ifdef __cplusplus
}
#endif

#endif // _ZT_SYSEX_MACRO_H_
