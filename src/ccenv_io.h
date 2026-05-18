/* ccenv_io.h — text-format .env preset reader/writer for CC envelopes.
 *
 * Mirrors the role of ccizer.cpp/.h for CCizer banks: lets users save
 * frequently-used curves as standalone .env files in a configurable
 * folder so they can be reused across songs. The file format is the
 * authoritative human-editable representation; the in-memory
 * ccenvelope is the runtime mirror.
 *
 * File format (line-based, all numeric tokens decimal):
 *
 *   # any number of comment lines (starting with '#' or ';')
 *   name=My Envelope
 *   cc=1
 *   kind=0                        # 0 CC, 1 Pitchbend, 2 Channel Pressure
 *   speed=1
 *   flags=enabled,sustain,loop,carry  # any subset, comma-separated
 *   loop=2..4                     # node-index range (inclusive)
 *   sustain=0..3
 *   nodes                         # marker, then tick<TAB>value lines
 *   0	0
 *   16	127
 *   32	64
 *
 * Robust to extra whitespace, blank lines, missing optional keys.
 * The reader sets defaults for missing values (cc=1, kind=0, speed=1,
 * empty flags, no loop, no sustain) so a minimal file with only
 * `nodes` and a few rows produces a working envelope.
 */
#ifndef _CCENV_IO_H_
#define _CCENV_IO_H_

class ccenvelope;

// Read .env file at `path` into `dst`. Returns 0 on success, -1 on
// I/O error or malformed file. `dst` is overwritten entirely.
int ccenv_read_file(const char *path, ccenvelope *dst);

// Write `src` to .env file at `path`. Returns 0 on success, -1 on
// I/O error.
int ccenv_write_file(const char *path, const ccenvelope *src);

// Scan `folder` for `*.env` files. Fills `out_basenames[]` (each
// entry a NUL-terminated basename, dot-prefixed entries skipped) and
// returns the number found (capped at max_out). Results are
// alphabetically sorted (case-insensitive).
int ccenv_scan_folder(const char *folder,
                      char out_basenames[][256],
                      int max_out);

// Resolve the user-configured envelope folder, in order:
//   1. zt_config_globals.ccenv_folder (if non-empty)
//   2. zt_directory + "/envelopes"
//   3. "./envelopes"
// Writes the result into `out` (size >= 1024). Returns 0 on success,
// -1 if no folder could be resolved (folder doesn't exist).
int ccenv_resolve_folder(char *out, int out_size);

#endif /* _CCENV_IO_H_ */
