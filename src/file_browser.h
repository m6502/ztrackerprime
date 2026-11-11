#ifndef ZT_FILE_BROWSER_H
#define ZT_FILE_BROWSER_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <sys/param.h>
#include <sys/mount.h>
#endif

namespace file_browser {
namespace fs = std::filesystem;

struct Entry {
    std::string name;
    bool directory;
};
struct Place {
    std::string name;
    fs::path path;
};

inline std::string folded(std::string text) {
    for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

inline std::vector<Entry> entries(const fs::path &path, std::error_code &error) {
    std::vector<Entry> result;
    if (path != path.root_path()) result.push_back({"..", true});
    fs::directory_iterator it(path, fs::directory_options::none, error), end;
    while (!error && it != end) {
        std::error_code entry_error;
        bool directory = it->is_directory(entry_error);
        auto ext = folded(it->path().extension().string());
        if (!entry_error && (directory ||
            ((ext == ".zt" || ext == ".it" || ext == ".mid") && it->is_regular_file(entry_error)))) {
            if (!entry_error) result.push_back({it->path().filename().string(), directory});
        }
        it.increment(error);
    }
    std::sort(result.begin(), result.end(), [](const Entry &a, const Entry &b) {
        if (a.name == ".." || b.name == "..") return a.name == ".." && b.name != "..";
        if (a.directory != b.directory) return a.directory;
        auto af = folded(a.name), bf = folded(b.name);
        return af == bf ? a.name < b.name : af < bf;
    });
    return result;
}

// mountinfo escapes spaces, tabs, newlines and backslashes as octal bytes.
inline std::string unescape_mount(const std::string &text) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 3 < text.size() &&
            text[i+1] >= '0' && text[i+1] <= '7' &&
            text[i+2] >= '0' && text[i+2] <= '7' &&
            text[i+3] >= '0' && text[i+3] <= '7') {
            result += static_cast<char>((text[i+1]-'0')*64 + (text[i+2]-'0')*8 + text[i+3]-'0');
            i += 3;
        } else result += text[i];
    }
    return result;
}

inline bool within(const std::string &path, const std::string &base) {
    return path == base || (path.size() > base.size() &&
        path.compare(0, base.size(), base) == 0 && path[base.size()] == '/');
}

inline std::vector<Place> linux_mounts(std::istream &input) {
    std::vector<Place> result;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string id, parent, device, root, mount, options, field, type, source;
        if (!(fields >> id >> parent >> device >> root >> mount >> options)) continue;
        while (fields >> field && field != "-") {}
        if (field != "-" || !(fields >> type >> source)) continue;
        mount = unescape_mount(mount);
        source = unescape_mount(source);
        if (mount.empty() || mount[0] != '/' || mount == "/" ||
            within(mount, "/proc") || within(mount, "/sys") || within(mount, "/dev")) continue;
        // Select disk-backed filesystems and network/user FUSE mounts. This
        // excludes kernel pseudo-filesystems without assuming media paths.
        bool useful = source.compare(0, 5, "/dev/") == 0 ||
            type == "nfs" || type == "nfs4" || type == "cifs" || type == "smb3" ||
            type == "zfs" || type == "fuseblk" || (type.compare(0, 5, "fuse.") == 0 && type != "fuse.portal");
        auto name = fs::path(mount).filename().string();
        if (!useful || name.empty() || name[0] == '.' || type == "squashfs" || within(mount, "/boot") ||
            within(mount, "/snap") || within(mount, "/var/lib") ||
            within(mount, "/run/credentials")) continue;
        result.push_back({name, mount});
    }
    return result;
}

inline fs::path home_directory() {
    if (const char *home = std::getenv("HOME")) {
        if (fs::path(home).is_absolute()) return home;
    }
#ifndef _WIN32
    if (auto *user = getpwuid(getuid())) return user->pw_dir;
#endif
    return {};
}

inline std::vector<Place> places() {
    std::vector<Place> result{{"/", "/"}};
    auto home = home_directory();
    if (!home.empty()) result.push_back({"Home", home});
    std::vector<Place> mounts;
#ifdef __linux__
    std::ifstream input("/proc/self/mountinfo");
    mounts = linux_mounts(input);
#elif defined(__APPLE__)
    struct statfs *volumes = nullptr;
    int count = getmntinfo(&volumes, MNT_NOWAIT);
    for (int i = 0; i < count; ++i) {
        fs::path path = volumes[i].f_mntonname;
        if (!(volumes[i].f_flags & MNT_DONTBROWSE) && path != "/")
            mounts.push_back({path.filename().string(), path});
    }
#endif
    std::sort(mounts.begin(), mounts.end(), [](const Place &a, const Place &b) {
        auto af = folded(a.name), bf = folded(b.name);
        return af == bf ? a.path < b.path : af < bf;
    });
    for (const auto &mount : mounts) {
        if (std::none_of(result.begin(), result.end(), [&](const Place &p) { return p.path == mount.path; }))
            result.push_back(mount);
    }
    return result;
}
} // namespace file_browser
#endif
