#include "fs_compat.h"

#if defined(__APPLE__)
#include <Availability.h>   // __MAC_OS_X_VERSION_MIN_REQUIRED (the deployment target)
#endif

// Use the POSIX backend only when std::filesystem is genuinely unavailable, so
// modern builds get the standard library and ztfs is purely the old-floor
// fallback (addresses cmicali's review note):
//   * Apple with a deployment target below macOS 10.15 -- libc++ marks
//     std::filesystem _unavailable_ before Catalina, so an older floor (e.g.
//     the 10.13 universal build) cannot link it. A target >= 10.15 falls
//     through to std::filesystem, exactly like Linux and Windows.
//   * Or when forced for the test build (ZTFS_FORCE_POSIX, run on Linux CI),
//     so the POSIX path never rots untested.
#if defined(ZTFS_FORCE_POSIX) || \
    (defined(__APPLE__) && (!defined(__MAC_OS_X_VERSION_MIN_REQUIRED) || \
                            __MAC_OS_X_VERSION_MIN_REQUIRED < 101500))
#define ZTFS_POSIX 1
#endif

namespace ztfs {

// --- lexical helpers (shared by both backends) -------------------------------

static char path_sep() {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

static bool is_sep(char c) { return c == '/' || c == '\\'; }

std::string filename(const std::string &p) {
    size_t pos = p.find_last_of("/\\");
    return pos == std::string::npos ? p : p.substr(pos + 1);
}

std::string extension(const std::string &p) {
    const std::string fn = filename(p);
    size_t dot = fn.find_last_of('.');
    if (dot == std::string::npos || dot == 0) return "";
    return fn.substr(dot);
}

std::string join(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (is_sep(a.back())) return a + b;
    return a + path_sep() + b;
}

} // namespace ztfs

#ifdef ZTFS_POSIX
// ---------------------------------------------------------------------------
// POSIX backend (Apple + forced test builds): no libc++ filesystem dependency.
// ---------------------------------------------------------------------------
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

namespace ztfs {

std::string current_path() {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == nullptr) return "";
    return std::string(buf);
}

bool set_current_path(const std::string &p) {
    return chdir(p.c_str()) == 0;
}

bool exists(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

bool is_directory(const std::string &p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

std::vector<DirEntry> list_directory(const std::string &dir) {
    std::vector<DirEntry> out;
    DIR *d = opendir(dir.c_str());
    if (!d) return out;
    struct dirent *de;
    while ((de = readdir(d)) != nullptr) {
        const std::string name = de->d_name;
        if (name == "." || name == "..") continue;
        DirEntry e;
        e.name = name;
        e.path = join(dir, name);
        struct stat st;                 // stat() follows symlinks, matching directory_entry
        if (stat(e.path.c_str(), &st) == 0) {
            e.is_directory = S_ISDIR(st.st_mode);
            e.is_regular_file = S_ISREG(st.st_mode);
        }
        out.push_back(std::move(e));
    }
    closedir(d);
    return out;
}

bool remove(const std::string &p) {
    return ::remove(p.c_str()) == 0;
}

bool rename(const std::string &from, const std::string &to) {
    return ::rename(from.c_str(), to.c_str()) == 0;
}

bool is_root_directory(const std::string &p) {
    if (!is_directory(p)) return false;
    return p == "/";
}

} // namespace ztfs

#else
// ---------------------------------------------------------------------------
// std::filesystem backend (Linux + Windows): behaviour unchanged from before.
// ---------------------------------------------------------------------------
#include <filesystem>

namespace fs = std::filesystem;

namespace ztfs {

std::string current_path() {
    std::error_code ec;
    fs::path p = fs::current_path(ec);
    return ec ? std::string() : p.string();
}

bool set_current_path(const std::string &p) {
    std::error_code ec;
    fs::current_path(p, ec);
    return !ec;
}

bool exists(const std::string &p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

bool is_directory(const std::string &p) {
    std::error_code ec;
    return fs::is_directory(p, ec);
}

std::vector<DirEntry> list_directory(const std::string &dir) {
    std::vector<DirEntry> out;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        DirEntry e;
        e.name = entry.path().filename().string();
        e.path = entry.path().string();
        std::error_code ec2;
        e.is_directory = entry.is_directory(ec2);
        e.is_regular_file = entry.is_regular_file(ec2);
        out.push_back(std::move(e));
    }
    return out;
}

bool remove(const std::string &p) {
    std::error_code ec;
    return fs::remove(p, ec) && !ec;
}

bool rename(const std::string &from, const std::string &to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
}

bool is_root_directory(const std::string &p) {
    std::error_code ec;
    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) return false;
    fs::path path(p);
    return path == path.root_path();
}

} // namespace ztfs

#endif
