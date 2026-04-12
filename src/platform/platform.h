// platform.h - Portable shims for Win32 types and functions that leaked into zTracker source.
// Include this where Win32-specific types/functions were previously assumed via <windows.h>.

#ifndef ZT_PLATFORM_H
#define ZT_PLATFORM_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>

// --- Win32 type aliases (for source compat — prefer C++ types in new code) ---
#ifndef _WIN32
typedef char* LPSTR;
typedef const char* LPCTSTR;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int BOOL;
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#endif

// --- MessageBox replacement using SDL ---
#ifndef MB_OK
#define MB_OK 0
#endif
#ifndef MB_ICONERROR
#define MB_ICONERROR 0
#endif

// Portable MessageBox — on Win32 the real MessageBox comes from <windows.h>.
// On other platforms, just log to stderr.
#ifndef _WIN32
inline void MessageBox(void* /*hwnd*/, const char* text, const char* caption, unsigned int /*flags*/) {
    fprintf(stderr, "%s: %s\n", caption, text);
}
#endif

// --- GetCurrentDirectory / SetCurrentDirectory ---
#ifndef _WIN32
#include <unistd.h>
inline int GetCurrentDirectory(int buflen, char* buf) {
    if (getcwd(buf, buflen)) return (int)strlen(buf);
    return 0;
}
inline int SetCurrentDirectory(const char* path) {
    return (chdir(path) == 0) ? 1 : 0;
}
#endif

// --- Drive enumeration (stub on non-Windows) ---
#ifndef _WIN32
inline unsigned long GetLogicalDrives() { return 0; }
inline unsigned int GetDriveType(const char* /*path*/) { return 0; }
#define DRIVE_CDROM 5
#define DRIVE_REMOVABLE 2
#endif

#endif // ZT_PLATFORM_H
