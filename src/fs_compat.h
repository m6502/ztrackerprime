#pragma once

// Minimal filesystem facade so the codebase does not call std::filesystem
// directly. On Apple, std::filesystem is annotated _unavailable_ below macOS
// 10.15 (libc++ ships the symbols only from Catalina on), which would pin the
// deployment floor at 10.15 and block High Sierra / Mojave. This facade is
// implemented with POSIX (opendir/stat/getcwd/...) on Apple so the floor drops
// to SDL3's own 10.13, and with std::filesystem everywhere else so Linux and
// Windows behaviour is unchanged. See fs_compat.cpp.
//
// SDL-free and exception-free: every call reports failure by return value, so
// it is safe in the unit-test harness (ZT_TEST_NO_SDL) and in the headless
// renderer. The POSIX path is also exercised on Linux CI via -DZTFS_FORCE_POSIX
// (see tests/test_fs_compat.cpp) so it never rots untested.

#include <string>
#include <vector>

namespace ztfs {

struct DirEntry {
    std::string name;            // filename component only ("colors.conf")
    std::string path;            // dir joined with name, matching std::filesystem
    bool is_directory = false;
    bool is_regular_file = false;
};

// Current working directory. current_path() returns "" on failure;
// set_current_path() returns false on failure. Neither throws.
std::string current_path();
bool set_current_path(const std::string &p);

bool exists(const std::string &p);
bool is_directory(const std::string &p);

// Directory listing. Skips "." and ".." (like std::filesystem::directory_iterator).
// Returns an empty vector on any error (a non-existent or unreadable directory),
// never throws. is_directory / is_regular_file follow symlinks, matching
// directory_entry's default behaviour.
std::vector<DirEntry> list_directory(const std::string &dir);

bool remove(const std::string &p);                              // unlink a file; false on failure
bool rename(const std::string &from, const std::string &to);    // false on failure

// Path string helpers (lexical only — no disk access).
std::string filename(const std::string &p);   // component after the last separator
std::string extension(const std::string &p);  // ".ext" including the dot, or "" (a leading-dot name has none)
std::string join(const std::string &a, const std::string &b);
bool is_root_directory(const std::string &p);  // exists, is a directory, and equals the filesystem root

} // namespace ztfs
