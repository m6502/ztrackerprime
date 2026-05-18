/* ccenv_io.h — text-format .env preset reader/writer.
 *
 * A .env file holds ONE envelope curve. Bindings (which instrument /
 * which CCizer slot) are NOT carried in the file -- the curve is
 * portable across CC numbers and instruments. The editor stamps the
 * loaded curve onto whichever inst_envelope slot is focused.
 *
 * File format (line-based, all numeric tokens decimal):
 *
 *   # comment lines (# or ;) ok anywhere
 *   cc=74
 *   kind=0                              # 0 CC, 1 Pitchbend, 2 Channel Pressure
 *   speed=1
 *   flags=enabled,sustain,loop,carry    # any subset, comma-separated
 *   loop=2..4                           # node-index range (inclusive)
 *   sustain=0..3
 *   nodes                               # marker, then tick<TAB>value lines
 *   0	0
 *   16	127
 *   32	64
 *
 * Robust to extra whitespace, blank lines, missing optional keys.
 * Defaults for missing values: cc=1, kind=0, speed=1, no flags, no
 * loop, no sustain.
 */
#ifndef _CCENV_IO_H_
#define _CCENV_IO_H_

struct inst_envelope;

int ccenv_read_file(const char *path, struct inst_envelope *dst);
int ccenv_write_file(const char *path, const struct inst_envelope *src);

int ccenv_scan_folder(const char *folder,
                      char out_basenames[][256],
                      int max_out);

/* Resolve the configured envelopes folder. Order:
 *   1. zt_config_globals.ccenv_folder (if set)
 *   2. zt_directory/envelopes
 *   3. ./envelopes
 * Returns 0 if the resolved path exists and is a dir; -1 if not (the
 * fallback path is still written to `out` so callers can mkdir it).
 */
int ccenv_resolve_folder(char *out, int out_size);

#endif /* _CCENV_IO_H_ */
