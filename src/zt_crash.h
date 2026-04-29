#ifndef ZT_CRASH_H
#define ZT_CRASH_H

// Install fatal-signal handlers (SIGSEGV, SIGABRT, SIGBUS, SIGFPE,
// SIGILL) that, before letting the process die, dump:
//
//   * the signal name and faulting address
//   * a backtrace of the active thread (POSIX: execinfo's
//     backtrace_symbols; Windows: skipped — handled by Windows
//     Error Reporting / dr. Watson)
//   * the path of the crash log file just written
//
// The trace goes to stderr AND to `zt-crash.log` next to the working
// directory so a Linux/macOS user running from Finder/desktop still
// has something to email back. The previous failure mode was
//
//     fish: Job 1, './zt' terminated by signal SIGABRT (Abort)
//
// — no stack, no clue what asserted. With a backtrace we can at least
// see that popc/erase_at/refresh_display was on the stack instead of
// guessing.
//
// Idempotent: calling it multiple times is a no-op after the first.
// Once a fatal signal fires, the handler restores the default
// disposition and re-raises so the OS can still drop a core if the
// user has `ulimit -c` enabled.

void zt_install_crash_handler(void);

#endif // ZT_CRASH_H
