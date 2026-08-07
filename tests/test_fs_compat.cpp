// Unit tests for src/fs_compat.cpp.
//
// Compiled with -DZTFS_FORCE_POSIX so the POSIX backend (the one Apple ships,
// to keep the deployment floor at 10.13 instead of std::filesystem's 10.15) is
// exercised here on Linux/macOS CI -- not just on a developer's Mac. Not built
// on Windows (the POSIX backend needs <dirent.h>/<unistd.h>); the std::filesystem
// backend Windows uses is the same one it has always used.

#include "fs_compat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

#include <sys/stat.h>
#include <unistd.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void write_file(const std::string &path, const char *contents) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (f) {
        std::fputs(contents, f);
        std::fclose(f);
    }
}

static bool has_entry(const std::vector<ztfs::DirEntry> &v, const std::string &name) {
    return std::any_of(v.begin(), v.end(),
                       [&](const ztfs::DirEntry &e) { return e.name == name; });
}

static const ztfs::DirEntry *find_entry(const std::vector<ztfs::DirEntry> &v,
                                        const std::string &name) {
    for (const auto &e : v)
        if (e.name == name) return &e;
    return nullptr;
}

int main() {
    // --- lexical helpers (no disk) -------------------------------------------
    CHECK(ztfs::filename("/a/b/c.txt") == "c.txt");
    CHECK(ztfs::filename("c.txt") == "c.txt");
    CHECK(ztfs::filename("/a/b/") == "");
    CHECK(ztfs::extension("song.zt") == ".zt");
    CHECK(ztfs::extension("a.tar.gz") == ".gz");
    CHECK(ztfs::extension("noext") == "");
    CHECK(ztfs::extension(".hidden") == "");      // leading dot is not an extension
    CHECK(ztfs::extension("/a/b/song.IT") == ".IT");
    CHECK(ztfs::join("/a/b", "c") == "/a/b/c");
    CHECK(ztfs::join("/a/b/", "c") == "/a/b/c");  // no doubled separator
    CHECK(ztfs::join(".", "x") == "./x");
    CHECK(ztfs::join("", "c") == "c");
    CHECK(ztfs::join("/a", "") == "/a");

    // --- build a scratch tree ------------------------------------------------
    char tmpl[] = "/tmp/ztfs_test_XXXXXX";
    char *base = mkdtemp(tmpl);
    CHECK(base != nullptr);
    if (!base) {
        std::printf("could not create temp dir; aborting\n");
        return 1;
    }
    const std::string root = base;

    const std::string saved_cwd = ztfs::current_path();
    CHECK(!saved_cwd.empty());

    // cwd round-trip
    CHECK(ztfs::set_current_path(root));
    // macOS resolves /tmp -> /private/tmp, so compare via a sentinel file
    // rather than string-equality of current_path().
    write_file("cwd_probe", "x");
    CHECK(ztfs::exists("cwd_probe"));

    // files + subdirs
    write_file(ztfs::join(root, "song.zt"), "data");
    write_file(ztfs::join(root, "tune.it"), "data");
    write_file(ztfs::join(root, "readme"), "data");
    CHECK(mkdir(ztfs::join(root, "skins").c_str(), 0755) == 0);
    write_file(ztfs::join(ztfs::join(root, "skins"), "colors.conf"), "data");

    // --- exists / is_directory ----------------------------------------------
    CHECK(ztfs::exists(ztfs::join(root, "song.zt")));
    CHECK(!ztfs::exists(ztfs::join(root, "does_not_exist")));
    CHECK(ztfs::is_directory(ztfs::join(root, "skins")));
    CHECK(!ztfs::is_directory(ztfs::join(root, "song.zt")));
    CHECK(!ztfs::is_directory(ztfs::join(root, "nope")));

    // --- list_directory ------------------------------------------------------
    auto entries = ztfs::list_directory(root);
    CHECK(!has_entry(entries, "."));      // never yields . or ..
    CHECK(!has_entry(entries, ".."));
    CHECK(has_entry(entries, "song.zt"));
    CHECK(has_entry(entries, "skins"));
    CHECK(has_entry(entries, "cwd_probe"));

    const ztfs::DirEntry *song = find_entry(entries, "song.zt");
    CHECK(song != nullptr);
    if (song) {
        CHECK(song->is_regular_file);
        CHECK(!song->is_directory);
        CHECK(song->path == ztfs::join(root, "song.zt"));
    }
    const ztfs::DirEntry *skins = find_entry(entries, "skins");
    CHECK(skins != nullptr);
    if (skins) {
        CHECK(skins->is_directory);
        CHECK(!skins->is_regular_file);
    }

    // listing a non-existent directory yields empty, no throw/crash
    CHECK(ztfs::list_directory(ztfs::join(root, "nope")).empty());

    // --- remove / rename -----------------------------------------------------
    const std::string a = ztfs::join(root, "to_rename");
    const std::string b = ztfs::join(root, "renamed");
    write_file(a, "data");
    CHECK(ztfs::exists(a));
    CHECK(ztfs::rename(a, b));
    CHECK(!ztfs::exists(a));
    CHECK(ztfs::exists(b));
    CHECK(ztfs::remove(b));
    CHECK(!ztfs::exists(b));
    CHECK(!ztfs::remove(ztfs::join(root, "already_gone")));  // false, no throw

    // --- is_root_directory ---------------------------------------------------
    CHECK(ztfs::is_root_directory("/"));
    CHECK(!ztfs::is_root_directory(root));
    CHECK(!ztfs::is_root_directory(ztfs::join(root, "song.zt")));

    // --- clean up ------------------------------------------------------------
    ztfs::set_current_path(saved_cwd);
    ztfs::remove(ztfs::join(root, "song.zt"));
    ztfs::remove(ztfs::join(root, "tune.it"));
    ztfs::remove(ztfs::join(root, "readme"));
    ztfs::remove(ztfs::join(root, "cwd_probe"));
    ztfs::remove(ztfs::join(ztfs::join(root, "skins"), "colors.conf"));
    rmdir(ztfs::join(root, "skins").c_str());
    rmdir(root.c_str());

    std::printf("%s: %d checks, %d failures\n",
                g_failures == 0 ? "PASS" : "FAIL", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
