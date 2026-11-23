// test_jump_command.cpp -- Jnnnn: jump to song order position.

#include "jump_command.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

static int g_failures = 0;
static int g_total    = 0;

#define CHECK(expr) do {                                       \
    ++g_total;                                                 \
    if (!(expr)) {                                             \
        ++g_failures;                                          \
        std::fprintf(stderr, "FAIL  %s:%d  %s\n",              \
                     __FILE__, __LINE__, #expr);               \
    }                                                          \
} while (0)

static const int END  = 0x100;
static const int SKIP = 0x101;

struct Song {
    int orderlist[ZT_JUMP_MAX_ORDERS];
    std::map<int, std::vector<std::pair<unsigned char, unsigned short>>> effects;

    explicit Song(std::vector<int> orders) {
        for (int i = 0; i < ZT_JUMP_MAX_ORDERS; i++) orderlist[i] = END;
        for (size_t i = 0; i < orders.size(); i++) orderlist[i] = orders[i];
    }

    void collect(bool targets[ZT_JUMP_MAX_ORDERS]) {
        zt_jump_collect_targets(orderlist, ZT_JUMP_MAX_ORDERS, targets,
            [this](int p, auto fn) {
                for (auto &fx : effects[p]) fn(fx.first, fx.second);
            });
    }
};

static void test_resolve()
{
    Song s({0, 1, SKIP, 2});

    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 0, 1, false) == 0);
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 3, 2, false) == 3);

    // F6 (single pattern) and MIDI export never jump.
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 1, 0, false) == -1);
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 1, 1, true)  == -1);

    // Targets that are not a playable pattern: skip marker, end, out of range.
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 2, 1, false) == -1);
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 4, 1, false) == -1);
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 256, 1, false) == -1);
    CHECK(zt_jump_resolve(s.orderlist, ZT_JUMP_MAX_ORDERS, 0xFFFF, 1, false) == -1);
}

static void test_collect()
{
    // Orders: 0 -> pat 5, 1 -> pat 6, 2 -> pat 5 again, then end. Pattern 7 is
    // not in the order list.
    Song s({5, 6, 5});
    s.effects[5] = {{'J', 1}, {'C', 0}};
    s.effects[6] = {{'T', 120}, {'J', 0}, {'J', 9}};   // 9 = past the end: ignored
    s.effects[7] = {{'J', 2}};                         // never played: ignored

    bool targets[ZT_JUMP_MAX_ORDERS];
    s.collect(targets);

    CHECK(targets[0]);
    CHECK(targets[1]);
    CHECK(!targets[2]);
    CHECK(!targets[9]);
    int count = 0;
    for (bool t : targets) count += t;
    CHECK(count == 2);

    Song none({0, 1});
    none.effects[0] = {{'C', 0}};
    none.collect(targets);
    count = 0;
    for (bool t : targets) count += t;
    CHECK(count == 0);
}

static void test_marker_text()
{
    char text[8];
    CHECK(zt_jump_marker_text(text, true, 0x0003) == 5);
    CHECK(std::strcmp(text, "[0003") == 0);
    CHECK(zt_jump_marker_text(text, false, 0x00A3) == 5);
    CHECK(std::strcmp(text, "]00A3") == 0);
    CHECK(zt_jump_marker_text(text, false, 0xFFFF) == 5);
    CHECK(std::strcmp(text, "]FFFF") == 0);
}

int main()
{
    test_resolve();
    test_collect();
    test_marker_text();

    std::printf("jump_command: %d/%d checks passed\n", g_total - g_failures, g_total);
    return g_failures ? 1 : 0;
}
