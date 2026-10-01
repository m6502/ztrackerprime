# Drawing measurements — 2026-09-26

The application now includes VU redraw gating, a shared optimized note
formatter, faster single-glyph rendering, and incremental pattern-cell drawing.
Full texture uploads, event storage, MIDI dispatch, synchronization, and
main-loop waits are unchanged. Indexing alternatives remain benchmark-only.

The event/text tables below record the initial experiments. The implementation
and final measurements are documented under **Implemented text and cell drawing**.

## Method

Run `python3 tools/benchmark_drawing.py` (GCC/Clang, Python 3, SDL3 development
files and pkg-config required). On Linux, optionally pin it to an available
core: `taskset -c 2 python3 tools/benchmark_drawing.py`. Use
`--optimization O0`, `O2`, or `O3` to compare compiler settings; default is O3.

Measurements here used an Intel Core i7-6950X, GCC 16.2.1, Linux, `-O3`, pinned
to CPU 2, after the application build finished. Frequency scaling remained
enabled. Each result is the median of seven samples, each calibrated to run
for at least approximately 25 ms. These are warm-cache CPU microbenchmarks,
not whole-application profiles or hardware MIDI latency measurements.

The harness extracts current production function bodies verbatim and compiles
them against the real application headers, in separate translation units with
no LTO. It uses the default skin's font and an actual ARGB8888 SDL surface.
`Drawable::getLine` remains in its own translation unit, as in the app.
For before/after comparisons, the old text/glyph routines are preserved in
`tests/fixtures/drawing_reference.cpp`.

Every synthetic grid contains **512 cells: 8 tracks × 64 visible rows** in a
1344×512 framebuffer. Tracks contain 64, 256, or 999 rows. Sparse tracks have
one event every 16 rows; dense tracks have an event on every row. Events are
individually allocated, interleaved between tracks, with shuffled insertion
order. The viewport is centered within the pattern. These cases explore the
cost curve; they do not establish how common dense patterns are in real songs.

Combined drawing measurements include event lookup, VIEW_BIG cell formatting,
opaque glyph drawing, and a separator glyph per cell. They exclude status
fields, selection/color calculations, locks, page backgrounds, texture uploads,
presentation, and event handling. Results from separate measurements should
not be added as though they were a whole-frame profile.

## Event lookup

Microseconds for all 512 lookups:

| Pattern | Current linked list | Temporary visible-row index, including rebuild |
|---|---:|---:|
| 64 rows, sparse | 2.95 | 0.77 |
| 64 rows, dense | 21.07 | 1.45 |
| 256 rows, sparse | 7.46 | 0.85 |
| 256 rows, dense | 227.71 | 8.11 |
| 999 rows, sparse | 34.24 | 1.27 |
| 999 rows, dense | 1,039.83 | 54.54 |

A prebuilt persistent index measured about 0.80 µs, **excluding maintenance**.
The temporary alternative includes clearing a 4 KiB pointer table, walking each
visible track's complete event list once, and looking up every visible cell.
It avoids persistent cache invalidation, but still needs integration/validation
against edits and the application's existing threading model.

The original claim that lookup is necessarily a major drawing cost was too
broad. At 64 sparse rows it is only about 0.003 ms. It becomes material for
long, dense patterns: roughly 0.23 ms at 256 rows and 1.04 ms at 999 rows.
The 999-row dense sample ranged from 1.033 to 1.041 ms.

## Text formatting and rasterization

Microseconds per 512 cells; these are formatting alone:

| Input / view | Current `printNote` | Candidate direct formatting |
|---|---:|---:|
| Blank / SQUISH | 62.21 | 3.20 |
| Blank / REGULAR | 83.56 | 3.97 |
| Blank / FX | 94.40 | 3.81 |
| Blank / BIG | 113.07 | 4.90 |
| Populated / SQUISH | 144.90 | 3.21 |
| Populated / REGULAR | 168.68 | 5.41 |
| Populated / FX | 180.32 | 5.19 |
| Populated / BIG | 196.69 | 7.22 |

The candidate formats only visible fields, writes decimal digits directly,
and uses uppercase hex digits instead of `sprintf` plus `toupper`. It does not
yet use a cached blank string. The current code formats hidden fields even in
SQUISH view; populated SQUISH formatting costs 74% of populated BIG formatting
despite displaying far fewer fields. Current populated BIG samples ranged
from 195.47 to 198.08 µs.

The production `printBG` rasterizer alone takes **258.27 µs** for preformatted
BIG cells. It is already optimized and is not replaced in this experiment.
For 512 separator glyphs, current `printchar` takes **40.37 µs**; hoisting row
addresses/bounds and unrolling pixel tests reduces that to **14.17 µs**.
Thus, formatting is a better first target than this single-glyph primitive,
but rasterization still accounts for substantial work after formatting improves.

The candidate formatter passed 200,000 byte-for-byte string comparisons
covering all four views, blank/default events, arbitrary byte fields, numeric
boundaries, note cut/off, and length sentinels. The glyph candidate passed 768
full-surface comparisons: all 256 glyphs, in-bounds, right clipping and bottom
clipping. These were the initial prototype checks; the implementation's expanded
checks, including left/top clipping, are described below.

Combined lookup + formatting + rasterization, still using the original linked
lookup in both columns:

| Pattern | Current | Candidate formatter + separator drawing | Time reduction |
|---|---:|---:|---:|
| 64 rows, sparse | 0.412 ms | 0.284 ms | 31% |
| 64 rows, dense | 0.533 ms | 0.328 ms | 39% |
| 256 rows, sparse | 0.422 ms | 0.295 ms | 30% |
| 256 rows, dense | 0.759 ms | 0.548 ms | 28% |
| 999 rows, sparse | 0.480 ms | 0.353 ms | 26% |
| 999 rows, dense | 1.786 ms | 1.560 ms | 13% |

These percentages apply only to this synthetic grid work. For example, saving
0.205 ms on each of 60 redraws/second saves about 12.3 ms of CPU time per second,
roughly 1.2% of one core on this machine. At lower redraw rates the CPU saving
is smaller. Slower CPUs need their own measurements; no whole-program speedup
or minimum supported machine is established here.

## Build settings

The existing `build-linux-sdl3` cache had an empty `CMAKE_BUILD_TYPE`, and its
compile commands contained no optimization flag. The same benchmark with
`-O0` took **1.233 ms** for the current 64-row dense grid versus **0.533 ms**
with `-O3` (about 2.3×). `printBG` alone was 816.27 versus 258.27 µs.

`scripts/compile_linux.sh` already defaults to Release. Keep performance
comparisons on optimized builds; the user's existing build configuration was
not changed by this task.

## VU fix and validation

VU refresh requests now occur on playback position changes (row, pattern, or
order), start/stop, and track scrolling. Decay advances once per observed row
instead of repeatedly while polling the same row. Drawing no longer compares
the audible row against the player's prebuffer position. Explicit refreshes
still paint the widget; page entry and view switching explicitly invalidate it.
View switching clears the complete previous content area, and static VU labels
are drawn when stopped too.

`python3 tests/test_vu_refresh.py` compiles the actual class declaration,
constructor, and update function with deterministic key/player fixtures. It
checks 10,000 idle updates and 10,000 unchanged playing updates without refresh
requests, plus start/stop, same-position restart, pattern/order transitions,
backward loop, decay saturation, and scrolling bounds. It is also registered
as `vu_refresh` in CTest when Python and a supported Unix compiler are available.

The application builds successfully; all 19 pre-existing CTest tests pass, and
the new VU regression test passes. Headless screenshot checks exercise active
notes, stopping, idle, view restoration, and scrolling. Stopped, idle, and
restored screenshots are byte-identical; scrolling changes the image, and the
playing screenshot shows active notes with no stale pattern grid behind them.

## Implemented text and cell drawing

The editor and F5 playback view now share `pattern_note_format.h`. It formats
only displayed fields and preserves their different missing-instrument spacing
and compact layouts. `printchar` hoists the row address and unrolls bit tests;
negative origins now use a safe clipped path too.

`pattern_draw_cache.h` remembers the last painted event values, foreground,
background, separator color, and caret for each visible cell. Unchanged cells
skip both formatting and rasterization. It still reads current event values on
every refresh, so there is no persistent event index to maintain. The cache
uses about 16 KiB for 512 cells plus a 2 KiB font snapshot, with allocations
only when growing the viewport. Track headers, borders, and row numbers still
draw normally; the status renderer also writes row numbers directly.

Full grid redraws occur on page entry, clears, popup restoration, drawbar mode
transitions, viewport/layout changes, and font changes. Final cell colors detect
palette, track-color, beat-highlight and selection changes. Surface identity,
dimensions, pitch, pattern length, and view mode are part of the cache layout.
The existing full-refresh/debug drawing switches bypass reuse.

Final text measurements, same CPU/compiler and `-O3`, median of seven samples:

| Work for 512 cells | Legacy | Implemented |
|---|---:|---:|
| Populated BIG formatting | 201.85 µs | 6.42 µs |
| Separator glyphs | 41.66 µs | 14.08 µs |

The actual `disp_pattern` renderer was also measured using
`taskset -c 2 python3 tests/test_drawing.py --benchmark`. This workload has eight
visible tracks, 64 visible rows, dense 128-row tracks, a 1504×720 surface,
track coloring, and optimized text enabled in **all** cases. Each sample
contains 500 redraws; results are median [minimum, maximum] across seven samples.

| Refresh | Cells painted | Time |
|---|---:|---:|
| Forced full grid | 512 | 419.79 [419.68, 421.45] µs |
| Unchanged grid | 0 | 118.31 [117.97, 118.46] µs |
| Caret moves between tracks | 2 | 119.68 [119.39, 119.80] µs |
| Cursor moves between rows | 16 | 127.98 [127.79, 128.80] µs |

Incremental drawing therefore saves about 70–72% of this grid-rendering time
relative to full redraws **after** the text optimization. Remaining work includes
linked-list lookup, color/selection calculations, and headers/borders/gutters.
Full texture upload and presentation costs are outside this measurement.

Validation:

- The application builds, and all 21 CTest tests pass.
- 600,000 editor/playback/null-event string comparisons against the saved legacy
  functions; all four views and arbitrary event fields, including sentinel values.
- 1,280 glyph comparisons covering every glyph and all four clipping edges.
- Pixel-for-pixel incremental/full-grid comparisons through edits, deletion,
  event replacement, selections, caret/row moves, playback gutter changes,
  colors, fonts, scroll, view modes, resize, blank rows, and 300 randomized edits.
  Counts assert that a caret-column move paints one cell, a track move two,
  and a row move 16 out of 512.
- Renderer tests pass under AddressSanitizer and UndefinedBehaviorSanitizer
  (`python3 tests/test_drawing.py --sanitize`). Vptr instrumentation is excluded
  because the isolated harness does not link or instantiate polymorphic UI objects.
- Sixteen actual-application screenshots from `tests/scripts/pattern-rendering.txt`
  are byte-identical between the pre-change binary and the new one, including
  popup restoration, paste/undo, page switches, drawbar switches, and resizing
  the pattern length.

The `drawing` test is registered with CTest on supported Unix builds when Python
and pkg-config SDL3 are available. These checks use no MIDI hardware and introduce
no changes to the application's MIDI handling or polling cadence.

## Remaining opportunities and latency constraint

1. Consider a temporary visible-row index for dense patterns, with combined
   drawing measurements and real-song workloads before choosing persistent
   storage changes.
2. Leave partial texture uploads until last, as requested.
3. Leave idle waiting unchanged. Longer sleeps cannot be assumed latency-neutral
   for MIDI input, keyjazz, or main-thread synchronization pumps. No additional
   wait or throttling was introduced; hardware input-to-output tail latency has
   not been measured, so that optimization remains deferred.
