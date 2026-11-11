#include "file_browser.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>

int main() {
    using namespace file_browser;
    std::istringstream mounts(
        "1 0 8:1 / / rw - ext4 /dev/sda1 rw\n"
        "2 1 0:1 / /proc rw - proc proc rw\n"
        "3 1 0:2 / /run rw - tmpfs tmpfs rw\n"
        "4 1 8:2 / /unusual/My\\040Disk rw shared:1 - ext4 /dev/sdb1 rw\n"
        "5 1 0:3 / /custom/share rw - cifs //server/share rw\n"
        "6 1 7:1 / /snap/app/1 ro - squashfs /dev/loop0 ro\n"
        "7 1 8:3 / /boot/efi rw - vfat /dev/sda3 rw\n"
        "8 1 0:4 / /remote rw - fuse.sshfs user@host:/ rw\n"
        "9 1 0:5 / /pool rw - zfs tank/data rw\n"
        "10 1 0:6 / /tmp/.mount_app ro - fuse.example.appimage app ro\n"
        "invalid line\n");
    auto places = linux_mounts(mounts);
    assert(places.size() == 4);
    assert(places[0].path == "/unusual/My Disk");
    assert(places[1].path == "/custom/share");
    assert(places[2].path == "/remote");
    assert(places[3].path == "/pool");
    assert(unescape_mount("a\\134b\\011c") == "a\\b\tc");
    assert(within("/media/a", "/media"));
    assert(!within("/media-other", "/media"));

    auto scratch = fs::temp_directory_path() / ("zt-browser-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(scratch / "Z folder");
    fs::create_directory(scratch / "a folder");
    for (auto name : {"Z.zt", "a.MID", "b.it", "hidden.txt"}) std::ofstream(scratch / name) << "fixture";
    std::error_code error;
    auto items = entries(scratch, error);
    assert(!error && items.size() == 6);
    assert(items[0].name == ".." && items[0].directory);
    assert(items[1].name == "a folder" && items[1].directory);
    assert(items[2].name == "Z folder" && items[2].directory);
    assert(items[3].name == "a.MID" && !items[3].directory);
    assert(items[4].name == "b.it" && !items[4].directory);
    assert(items[5].name == "Z.zt" && !items[5].directory);
    entries(scratch / "missing", error);
    assert(error);
    error.clear();
    auto root = entries(scratch.root_path(), error);
    assert(!error);
    for (auto &entry : root) assert(entry.name != "..");
    fs::remove_all(scratch);
}
