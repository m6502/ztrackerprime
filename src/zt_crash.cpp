#include "zt_crash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <signal.h>
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#endif

namespace {

bool g_installed = false;
#if !defined(_WIN32)
// sig_atomic_t lives in <signal.h>, which is only included on the
// POSIX path above. The flag itself is only read/written from the
// signal handler, which is also POSIX-only, so gate the
// declaration entirely on !_WIN32.
volatile sig_atomic_t g_in_handler = 0;
#endif

const char *signal_name(int sig) {
#if !defined(_WIN32)
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (invalid memory reference)";
        case SIGABRT: return "SIGABRT (abort -- failed assertion / std::terminate / abort())";
        case SIGBUS:  return "SIGBUS (bus error -- misaligned access / mmap fault)";
        case SIGFPE:  return "SIGFPE (arithmetic error -- div by zero / overflow)";
        case SIGILL:  return "SIGILL (illegal instruction)";
        case SIGTRAP: return "SIGTRAP (debugger trap / __builtin_trap)";
        default: break;
    }
#endif
    return "fatal signal";
}

#if !defined(_WIN32)

// Async-signal-safe write helper. fprintf/snprintf/etc. are NOT
// async-signal-safe, so for the stderr part we go through plain
// write() with pre-formatted small chunks. The file copy is allowed
// to use buffered IO since it runs after we've already flushed the
// raw safe trace.
void safe_write(int fd, const char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0) {
        ssize_t n = write(fd, s, len);
        if (n <= 0) break;
        s += n;
        len -= (size_t)n;
    }
}

void crash_handler(int sig) {
    // Re-entrant guard: if the handler itself faults, fall through to
    // the default disposition so we don't recurse forever.
    if (g_in_handler) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    g_in_handler = 1;

    static const char banner[] =
        "\n"
        "================================================================\n"
        "  zTracker fatal signal\n"
        "================================================================\n";
    safe_write(STDERR_FILENO, banner);
    safe_write(STDERR_FILENO, "  signal: ");
    safe_write(STDERR_FILENO, signal_name(sig));
    safe_write(STDERR_FILENO, "\n");

#if defined(__APPLE__) || defined(__linux__)
    void *frames[64];
    int n = backtrace(frames, (int)(sizeof(frames) / sizeof(frames[0])));
    safe_write(STDERR_FILENO, "  backtrace:\n");
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
#else
    safe_write(STDERR_FILENO, "  (no backtrace available on this platform)\n");
#endif

    // Best-effort copy to a log file in CWD. Allowed to use stdio
    // because we've already emitted the safe trace; if this faults
    // the re-entrant guard above catches it and we still die cleanly.
    FILE *fp = fopen("zt-crash.log", "a");
    if (fp) {
        time_t now = time(NULL);
        char tbuf[64];
        struct tm *tm_local = localtime(&now);
        if (tm_local && strftime(tbuf, sizeof(tbuf),
                                 "%Y-%m-%d %H:%M:%S", tm_local) > 0) {
            fprintf(fp, "\n[%s] ", tbuf);
        } else {
            fprintf(fp, "\n[t=%lld] ", (long long)now);
        }
        fprintf(fp, "%s\n", signal_name(sig));
#if defined(__APPLE__) || defined(__linux__)
        char **syms = backtrace_symbols(frames, n);
        if (syms) {
            for (int i = 0; i < n; i++) {
                fprintf(fp, "  %s\n", syms[i]);
            }
            free(syms);
        }
#endif
        fclose(fp);
        safe_write(STDERR_FILENO, "  log:    ./zt-crash.log\n");
    }

    safe_write(STDERR_FILENO,
               "================================================================\n");

    // Restore default disposition and re-raise. Lets the OS drop a
    // core if ulimit allows, and lets the shell report the original
    // signal name as the exit cause.
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif // !_WIN32

} // namespace

void zt_install_crash_handler(void) {
    if (g_installed) return;
    g_installed = true;
#if !defined(_WIN32)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = crash_handler;
    sigemptyset(&sa.sa_mask);
    // SA_NODEFER lets a nested fatal signal (via re-raise) deliver
    // immediately. SA_RESETHAND resets the handler to default after
    // first delivery, so the explicit raise() below does not loop.
    sa.sa_flags = SA_NODEFER | SA_RESETHAND;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGTRAP, &sa, NULL);
#endif
}
