#include "zt.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

char *reference_printNote(char *, event *, int);
char *printNote(char *, event *, int);
void printchar(int, int, unsigned char, TColor, Drawable *);
void reference_printchar(int, int, unsigned char, TColor, Drawable *);
event *indexed_event(event **, int);

namespace {
constexpr int tracks = 8, rows = 64, cells = tracks * rows;
volatile uintptr_t sink;
using Clock = std::chrono::steady_clock;

template<class F> void measure(const std::string &name, F work) {
    work();
    int iterations = 1;
    for (;;) {
        auto start = Clock::now();
        for (int i = 0; i < iterations; ++i) work();
        if (std::chrono::duration<double>(Clock::now() - start).count() >= .025) break;
        iterations *= 2;
    }
    std::vector<double> times;
    for (int sample = 0; sample < 7; ++sample) {
        auto start = Clock::now();
        for (int i = 0; i < iterations; ++i) work();
        times.push_back(std::chrono::duration<double, std::micro>(Clock::now() - start).count() / iterations);
    }
    std::sort(times.begin(), times.end());
    printf("%-47s %9.2f  [%9.2f, %9.2f] us/grid\n", name.c_str(), times[3], times.front(), times.back());
}

void verify(Drawable &surface, std::mt19937 &rng) {
    // Check valid field ranges, blanks, cut/off, all modes and numeric edges.
    event e;
    char a[64], b[64];
    for (int n = 0; n < 50000; ++n) {
        if (n) {
            e.note = rng() % 256; e.inst = rng() % 256; e.vol = rng() % 256;
            e.effect = rng() % 256;
            e.length = rng() % 65536; e.effect_data = rng() % 65536;
            if (n < 1100) e.length = n - 1;
        }
        for (int mode : {VIEW_SQUISH, VIEW_REGULAR, VIEW_FX, VIEW_BIG}) {
            reference_printNote(a, &e, mode); printNote(b, &e, mode);
            if (strcmp(a, b)) { fprintf(stderr, "Formatting mismatch\n"); exit(1); }
        }
    }
    const size_t bytes = surface.surface->pitch * surface.surface->h;
    std::vector<unsigned char> reference(bytes);
    for (int glyph = 0; glyph < 256; ++glyph) {
        for (int edge = 0; edge < 3; ++edge) {
            int x = edge == 1 ? surface.surface->w - 4 : 5;
            int y = edge == 2 ? surface.surface->h - 4 : 5;
            memset(surface.surface->pixels, 0x23, bytes);
            reference_printchar(x, y, glyph, 0xffaabbcc, &surface);
            memcpy(reference.data(), surface.surface->pixels, bytes);
            memset(surface.surface->pixels, 0x23, bytes);
            printchar(x, y, glyph, 0xffaabbcc, &surface);
            if (memcmp(reference.data(), surface.surface->pixels, bytes)) {
                fprintf(stderr, "Glyph mismatch\n"); exit(1);
            }
        }
    }
    puts("Legacy/production equivalence: 200,000 strings and 768 glyph/clipping cases passed.");
}
}

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(font, 1, 2048, f) != 2048) return 1;
    fclose(f);
    Drawable surface(8 * 21 * tracks, 8 * rows);
    if (!surface.surface) return 1;
    std::mt19937 rng(1701);
    verify(surface, rng);
    puts("512 cells (8 tracks x 64 rows); median [min, max], seven samples; CPU only.");
    event blank;
    for (int length : {64, 256, 999}) {
        for (int spacing : {16, 1}) {
            std::vector<std::unique_ptr<track>> data;
            std::vector<std::vector<event *>> index(tracks, std::vector<event *>(length));
            std::vector<int> order(length);
            std::iota(order.begin(), order.end(), 0);
            // Interleaved track allocations and shuffled insertion order avoid
            // the unrealistically ideal case of a contiguous, sorted list.
            std::shuffle(order.begin(), order.end(), rng);
            for (int t = 0; t < tracks; ++t) data.emplace_back(new track(length));
            for (int row : order) for (int t = 0; t < tracks; ++t) {
                if (row % spacing) continue;
                event *e = new event;
                e->row = row; e->note = 36 + rng() % 60; e->inst = rng() % 100;
                e->vol = rng() % 128; e->length = 1 + rng() % 128;
                e->effect = 'A' + rng() % 26; e->effect_data = rng() % 65536;
                e->next_event = data[t]->event_list; data[t]->event_list = e;
                index[t][row] = e;
            }
            std::string label = std::to_string(length) + (spacing == 1 ? " dense " : " sparse ");
            const int first = (length - rows) / 2;
            measure(label + "linked lookup", [&]() {
                uintptr_t sum = 0;
                for (int row = first; row < first + rows; ++row)
                    for (int t = 0; t < tracks; ++t)
                        sum ^= reinterpret_cast<uintptr_t>(data[t]->get_event(row));
                sink = sum;
            });
            measure(label + "persistent indexed lookup (no upkeep)", [&]() {
                uintptr_t sum = 0;
                for (int row = first; row < first + rows; ++row)
                    for (int t = 0; t < tracks; ++t)
                        sum ^= reinterpret_cast<uintptr_t>(indexed_event(index[t].data(), row));
                sink = sum;
            });
            measure(label + "temporary visible index incl. build", [&]() {
                event *visible[tracks][rows] = {};
                for (int t = 0; t < tracks; ++t)
                    for (event *e = data[t]->event_list; e; e = e->next_event)
                        if (e->row >= first && e->row < first + rows)
                            visible[t][e->row - first] = e;
                uintptr_t sum = 0;
                for (int row = 0; row < rows; ++row)
                    for (int t = 0; t < tracks; ++t)
                        sum ^= reinterpret_cast<uintptr_t>(indexed_event(visible[t], row));
                sink = sum;
            });
            measure(label + "lookup + legacy format/glyphs", [&]() {
                char str[64];
                for (int row = 0; row < rows; ++row) for (int t = 0; t < tracks; ++t) {
                    event *e = data[t]->get_event(first + row);
                    reference_printNote(str, e ? e : &blank, VIEW_BIG);
                    printBG(t * 168, row * 8, str, 0xffeeeeee, 0xff112233, &surface);
                    reference_printchar(t * 168 + 160, row * 8, 168, 0xffaaaaaa, &surface);
                }
                sink = static_cast<TColor *>(surface.surface->pixels)[0];
            });
            measure(label + "lookup + optimized format/glyphs", [&]() {
                char str[64];
                for (int row = 0; row < rows; ++row) for (int t = 0; t < tracks; ++t) {
                    event *e = data[t]->get_event(first + row);
                    printNote(str, e ? e : &blank, VIEW_BIG);
                    printBG(t * 168, row * 8, str, 0xffeeeeee, 0xff112233, &surface);
                    printchar(t * 168 + 160, row * 8, 168, 0xffaaaaaa, &surface);
                }
                sink = static_cast<TColor *>(surface.surface->pixels)[0];
            });
        }
    }
    std::vector<event> events(cells);
    for (int filled : {0, 1}) {
        if (filled) for (auto &e : events) {
            e.note = 36 + rng() % 60; e.inst = rng() % 100; e.vol = rng() % 128;
            e.length = 1 + rng() % 1000; e.effect = 'A' + rng() % 26;
            e.effect_data = rng() % 65536;
        }
        for (int mode : {VIEW_SQUISH, VIEW_REGULAR, VIEW_FX, VIEW_BIG}) {
            std::string label = std::string(filled ? "notes " : "blank ") +
                (mode == VIEW_BIG ? "BIG " : mode == VIEW_FX ? "FX " : mode == VIEW_REGULAR ? "REGULAR " : "SQUISH ");
            for (bool fast : {false, true}) {
                auto format = fast ? printNote : reference_printNote;
                measure(label + (fast ? "optimized formatting" : "legacy formatting"), [&]() {
                    char str[64]; uintptr_t sum = 0;
                    for (auto &e : events) { format(str, &e, mode); sum += str[0]; }
                    sink = sum;
                });
            }
        }
    }
    char strings[cells][64];
    for (int i = 0; i < cells; ++i) reference_printNote(strings[i], &events[i], VIEW_BIG);
    measure("printBG only, preformatted BIG notes", [&]() {
        for (int i = 0; i < cells; ++i)
            printBG(i % tracks * 168, i / tracks * 8, strings[i], 0xffeeeeee, 0xff112233, &surface);
        sink = static_cast<TColor *>(surface.surface->pixels)[0];
    });
    for (bool fast : {false, true}) {
        auto glyph = fast ? printchar : reference_printchar;
        measure(fast ? "optimized printchar, separators" : "legacy printchar, separators", [&]() {
            for (int i = 0; i < cells; ++i)
                glyph(i % tracks * 168 + 160, i / tracks * 8, 168, 0xffaaaaaa, &surface);
            sink = static_cast<TColor *>(surface.surface->pixels)[160];
        });
    }
    return 0;
}
