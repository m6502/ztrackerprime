// Windows XP API compatibility shims.
//
// Built and linked only into 32-bit Windows builds (the windows-x86-xp CI
// target). Modern Windows builds skip this file because their toolchain
// already targets Vista+.
//
// ## GetTickCount64 polyfill
//
// XP's KERNEL32.dll does NOT export GetTickCount64 -- that API was added in
// Windows Vista. zt does not call GetTickCount64 directly, but the static
// libwinpthread (pulled in by `std::mutex` / `std::thread` after the
// PR #117 -static fix) does, internally, for timed waits. The compile-time
// `_WIN32_WINNT=0x0501` define on our side does not help because the
// prebuilt libwinpthread shipped with Ubuntu's mingw-w64 was compiled
// against modern headers.
//
// On launch XP fails immediately:
//
//     zt.exe Unable to Locate Component
//     The procedure entry point GetTickCount64 could not be located in the
//     dynamic link library KERNEL32.dll.
//
// We polyfill GetTickCount64 by tracking the high word ourselves on top of
// XP-safe GetTickCount (32-bit, wraps every ~49 days), then redirect the
// import-table slot `__imp__GetTickCount64@0` to point at our function.
// Because the i686 stdcall import slot has an `@0` suffix that C identifiers
// can't carry, we use the GCC asm-name extension (`__asm__("...")`) to set
// the linker symbol verbatim. With our object file linked before
// libkernel32.a, the linker resolves the slot from us and never pulls
// kernel32's import.
//
// Concurrency: the high-word increment runs at most once per 32-bit wrap
// (~49 days) and the worst-case race produces an off-by-2^32 ms reading
// for a single sample, which is harmless for the millisecond-scale timed
// waits libwinpthread uses.

#if defined(_WIN32) && !defined(_WIN64)

#include <windows.h>

static volatile DWORD     g_xp_last_tick = 0;
static volatile ULONGLONG g_xp_tick_high = 0;

static ULONGLONG WINAPI zt_xp_GetTickCount64(void) {
    DWORD now = GetTickCount();
    if (now < g_xp_last_tick) {
        g_xp_tick_high += 0x100000000ULL;
    }
    g_xp_last_tick = now;
    return g_xp_tick_high + (ULONGLONG)now;
}

// Override kernel32's import-table slot. The `__asm__` extension forces the
// linker symbol name, sidestepping the `@0` stdcall suffix that plain C
// identifiers can't represent.
__attribute__((used))
ULONGLONG (WINAPI *const zt_xp_imp_GetTickCount64)(void)
    __asm__("__imp__GetTickCount64@0") = zt_xp_GetTickCount64;

#else
// Silence -Wempty-translation-unit on non-Win32 toolchains that may scan
// this file (e.g. an IDE's clang index pass). On Windows builds this branch
// is never taken; on non-Windows builds the CMake list does not include
// this file at all.
typedef int zt_xp_compat_translation_unit_marker;
#endif // defined(_WIN32) && !defined(_WIN64)
