#include "platform.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unordered_map>
#include <mutex>

static std::unordered_map<UINT_PTR, zt_timer_callback> g_timer_callbacks;
static std::mutex g_timer_mutex;

static VOID CALLBACK zt_timer_thunk(HWND, UINT, UINT_PTR timer_id, DWORD)
{
    zt_timer_callback cb = NULL;
    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        const auto it = g_timer_callbacks.find(timer_id);
        if (it != g_timer_callbacks.end()) {
            cb = it->second;
        }
    }
    if (cb) {
        cb();
    }
}

zt_mutex_handle zt_mutex_create(const char *name)
{
    return (zt_mutex_handle)CreateMutexA(NULL, FALSE, name);
}

void zt_mutex_destroy(zt_mutex_handle mutex)
{
    if (mutex) {
        CloseHandle((HANDLE)mutex);
    }
}

int zt_mutex_lock(zt_mutex_handle mutex, int timeout_ms)
{
    if (!mutex) {
        return 0;
    }
    return WaitForSingleObject((HANDLE)mutex, (DWORD)timeout_ms) == WAIT_OBJECT_0;
}

int zt_mutex_unlock(zt_mutex_handle mutex)
{
    if (!mutex) {
        return 0;
    }
    return ReleaseMutex((HANDLE)mutex);
}

zt_thread_handle zt_thread_create(zt_thread_entry entry, void *user_data, unsigned long *thread_id)
{
    DWORD tid = 0;
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entry, user_data, 0, &tid);
    if (thread_id) {
        *thread_id = (unsigned long)tid;
    }
    return (zt_thread_handle)thread;
}

int zt_thread_join(zt_thread_handle thread)
{
    if (!thread) {
        return 0;
    }
    return WaitForSingleObject((HANDLE)thread, INFINITE) == WAIT_OBJECT_0;
}

void zt_thread_terminate(zt_thread_handle thread, unsigned long exit_code)
{
    if (thread) {
        TerminateThread((HANDLE)thread, (DWORD)exit_code);
    }
}

void zt_thread_close(zt_thread_handle thread)
{
    if (thread) {
        CloseHandle((HANDLE)thread);
    }
}

void zt_thread_set_priority(zt_thread_handle thread, int priority)
{
    if (thread) {
        SetThreadPriority((HANDLE)thread, priority);
    }
}

int zt_thread_get_current_priority(void)
{
    return GetThreadPriority(GetCurrentThread());
}

void zt_thread_set_current_priority(int priority)
{
    SetThreadPriority(GetCurrentThread(), priority);
}

zt_timer_handle zt_timer_start(zt_timer_handle timer_id, unsigned int interval_ms, zt_timer_callback callback)
{
    const UINT_PTR id = SetTimer(NULL, (UINT_PTR)timer_id, (UINT)interval_ms, zt_timer_thunk);
    if (id == 0) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        g_timer_callbacks[id] = callback;
    }
    return (zt_timer_handle)id;
}

void zt_timer_stop(zt_timer_handle timer_id)
{
    const UINT_PTR id = (UINT_PTR)timer_id;
    if (id == 0) {
        return;
    }
    KillTimer(NULL, id);
    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        g_timer_callbacks.erase(id);
    }
}

unsigned long zt_get_current_directory(unsigned long size, char *buffer)
{
    return (unsigned long)GetCurrentDirectoryA((DWORD)size, buffer);
}

int zt_set_current_directory(const char *path)
{
    return SetCurrentDirectoryA(path) ? 1 : 0;
}

void zt_show_error(const char *title, const char *message)
{
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONERROR);
}

#else

#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <unordered_map>
#include <mutex>

struct zt_posix_thread {
    pthread_t thread;
    bool joined;
};

struct zt_posix_timer {
    pthread_t thread;
    std::atomic<bool> running;
    unsigned int interval_ms;
    zt_timer_callback callback;
    pthread_mutex_t wake_mutex;
    pthread_cond_t wake_cond;
};

struct zt_posix_thread_start {
    zt_thread_entry entry;
    void *user_data;
};

static std::mutex g_timer_mutex;
static std::unordered_map<zt_timer_handle, zt_posix_timer *> g_timers;
static std::atomic<zt_timer_handle> g_next_timer_id(1);

static void *zt_posix_thread_main(void *arg)
{
    zt_posix_thread_start *start = (zt_posix_thread_start *)arg;
    const zt_thread_entry entry = start->entry;
    void *user_data = start->user_data;
    free(start);
    if (entry) {
        (void)entry(user_data);
    }
    return NULL;
}

// Monotonic timespec helpers — a wall-clock jump must never disturb MIDI timing,
// and callbacks must be scheduled from an absolute deadline (not "now +
// interval") so runtime doesn't accumulate into the rhythm.
static void zt_timespec_add_ns(struct timespec *ts, long ns)
{
    ts->tv_nsec += ns;
    while (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec  += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

static bool zt_timespec_lt(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec != b->tv_sec) return a->tv_sec < b->tv_sec;
    return a->tv_nsec < b->tv_nsec;
}

static void *zt_posix_timer_main(void *arg)
{
    zt_posix_timer *timer = (zt_posix_timer *)arg;
    const long interval_ns = (long)timer->interval_ms * 1000000L;

    // Absolute next-tick deadline on CLOCK_MONOTONIC. Each iteration advances
    // this by exactly interval_ns so callback runtime never leaks into the
    // tempo. If we fall badly behind (debugger pause, OS stall), the catch-up
    // loop skips missed ticks rather than firing a burst — preferable for MIDI.
    struct timespec next_deadline;
    clock_gettime(CLOCK_MONOTONIC, &next_deadline);

    while (timer->running.load()) {
        zt_timespec_add_ns(&next_deadline, interval_ns);

        pthread_mutex_lock(&timer->wake_mutex);
        int wait_result = 0;
        while (timer->running.load() && wait_result != ETIMEDOUT) {
#if defined(__APPLE__)
            // macOS pthread_condattr_setclock doesn't honor CLOCK_MONOTONIC on
            // older SDKs, so use the relative-wait extension and recompute the
            // remaining time each loop — immune to wall-clock jumps.
            struct timespec now, rel;
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (!zt_timespec_lt(&now, &next_deadline)) {
                wait_result = ETIMEDOUT;
                break;
            }
            rel.tv_sec  = next_deadline.tv_sec  - now.tv_sec;
            rel.tv_nsec = next_deadline.tv_nsec - now.tv_nsec;
            if (rel.tv_nsec < 0) { rel.tv_sec -= 1; rel.tv_nsec += 1000000000L; }
            wait_result = pthread_cond_timedwait_relative_np(
                &timer->wake_cond, &timer->wake_mutex, &rel);
#else
            // Condvar was initialized with CLOCK_MONOTONIC in zt_timer_start.
            wait_result = pthread_cond_timedwait(
                &timer->wake_cond, &timer->wake_mutex, &next_deadline);
#endif
        }
        const bool should_callback = timer->running.load() && wait_result == ETIMEDOUT;
        pthread_mutex_unlock(&timer->wake_mutex);

        if (should_callback && timer->callback) {
            timer->callback();
        }

        // If a stall pushed 'now' past the deadline by more than one interval,
        // skip the missed ticks so we resume on cadence rather than firing a
        // backlog of callbacks all at once.
        if (should_callback) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            while (zt_timespec_lt(&next_deadline, &now)) {
                zt_timespec_add_ns(&next_deadline, interval_ns);
            }
        }
    }

    // Intentionally no destroy/free here — zt_timer_stop does it after
    // pthread_join returns, so timer->thread stays valid for the join.
    return NULL;
}

zt_mutex_handle zt_mutex_create(const char *)
{
    pthread_mutex_t *mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (!mutex) {
        return NULL;
    }
    pthread_mutexattr_t attr;
    if (pthread_mutexattr_init(&attr) != 0) {
        free(mutex);
        return NULL;
    }
    if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(mutex);
        return NULL;
    }
    if (pthread_mutex_init(mutex, &attr) != 0) {
        pthread_mutexattr_destroy(&attr);
        free(mutex);
        return NULL;
    }
    pthread_mutexattr_destroy(&attr);
    return (zt_mutex_handle)mutex;
}

void zt_mutex_destroy(zt_mutex_handle mutex)
{
    if (!mutex) {
        return;
    }
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    free(mutex);
}

int zt_mutex_lock(zt_mutex_handle mutex, int timeout_ms)
{
    if (!mutex) {
        return 0;
    }
    if (timeout_ms < 0) {
        return pthread_mutex_lock((pthread_mutex_t *)mutex) == 0;
    }

#if defined(__APPLE__)
    /* macOS lacks pthread_mutex_timedlock in older SDKs; retry with short sleeps. */
    const int step_ms = 5;
    int waited = 0;
    while (waited <= timeout_ms) {
        if (pthread_mutex_trylock((pthread_mutex_t *)mutex) == 0) {
            return 1;
        }
        usleep((useconds_t)(step_ms * 1000));
        waited += step_ms;
    }
    return 0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_mutex_timedlock((pthread_mutex_t *)mutex, &ts) == 0;
#endif
}

int zt_mutex_unlock(zt_mutex_handle mutex)
{
    if (!mutex) {
        return 0;
    }
    return pthread_mutex_unlock((pthread_mutex_t *)mutex) == 0;
}

zt_thread_handle zt_thread_create(zt_thread_entry entry, void *user_data, unsigned long *thread_id)
{
    zt_posix_thread *thread = (zt_posix_thread *)malloc(sizeof(zt_posix_thread));
    zt_posix_thread_start *start = (zt_posix_thread_start *)malloc(sizeof(zt_posix_thread_start));
    if (!thread || !start) {
        free(thread);
        free(start);
        return NULL;
    }
    start->entry = entry;
    start->user_data = user_data;
    if (pthread_create(&thread->thread, NULL, zt_posix_thread_main, start) != 0) {
        free(start);
        free(thread);
        return NULL;
    }
    thread->joined = false;
    if (thread_id) {
        *thread_id = 0;
        const size_t copy_size = sizeof(*thread_id) < sizeof(thread->thread)
            ? sizeof(*thread_id)
            : sizeof(thread->thread);
        memcpy(thread_id, &thread->thread, copy_size);
    }
    return (zt_thread_handle)thread;
}

int zt_thread_join(zt_thread_handle thread)
{
    if (!thread) {
        return 0;
    }
    zt_posix_thread *t = (zt_posix_thread *)thread;
    if (t->joined) {
        return 1;
    }
    if (pthread_join(t->thread, NULL) == 0) {
        t->joined = true;
        return 1;
    }
    return 0;
}

void zt_thread_terminate(zt_thread_handle thread, unsigned long)
{
    if (!thread) {
        return;
    }
    zt_posix_thread *t = (zt_posix_thread *)thread;
    pthread_cancel(t->thread);
}

void zt_thread_close(zt_thread_handle thread)
{
    if (!thread) {
        return;
    }
    zt_posix_thread *t = (zt_posix_thread *)thread;
    if (!t->joined) {
        /* Ignore detach errors (already detached/joined/terminated). */
        (void)pthread_detach(t->thread);
    }
    free(t);
}

void zt_thread_set_priority(zt_thread_handle, int) {}
int zt_thread_get_current_priority(void) { return 0; }
void zt_thread_set_current_priority(int) {}

zt_timer_handle zt_timer_start(zt_timer_handle timer_id, unsigned int interval_ms, zt_timer_callback callback)
{
    if (!callback || interval_ms == 0) {
        return 0;
    }

    if (timer_id != 0) {
        zt_timer_stop(timer_id);
    } else {
        timer_id = g_next_timer_id.fetch_add(1);
    }

    zt_posix_timer *timer = (zt_posix_timer *)malloc(sizeof(zt_posix_timer));
    if (!timer) {
        return 0;
    }
    timer->running.store(true);
    timer->interval_ms = interval_ms;
    timer->callback = callback;
    if (pthread_mutex_init(&timer->wake_mutex, NULL) != 0) {
        free(timer);
        return 0;
    }
#if defined(__APPLE__)
    // macOS: clock attr is ignored; we use pthread_cond_timedwait_relative_np
    // in the worker, which reads CLOCK_MONOTONIC directly.
    if (pthread_cond_init(&timer->wake_cond, NULL) != 0) {
        pthread_mutex_destroy(&timer->wake_mutex);
        free(timer);
        return 0;
    }
#else
    pthread_condattr_t cond_attr;
    if (pthread_condattr_init(&cond_attr) != 0) {
        pthread_mutex_destroy(&timer->wake_mutex);
        free(timer);
        return 0;
    }
    // Bind the condvar to CLOCK_MONOTONIC so pthread_cond_timedwait deadlines
    // are immune to wall-clock steps (NTP, DST, user-set time).
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    if (pthread_cond_init(&timer->wake_cond, &cond_attr) != 0) {
        pthread_condattr_destroy(&cond_attr);
        pthread_mutex_destroy(&timer->wake_mutex);
        free(timer);
        return 0;
    }
    pthread_condattr_destroy(&cond_attr);
#endif
    if (pthread_create(&timer->thread, NULL, zt_posix_timer_main, timer) != 0) {
        pthread_cond_destroy(&timer->wake_cond);
        pthread_mutex_destroy(&timer->wake_mutex);
        free(timer);
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        g_timers[timer_id] = timer;
    }
    return timer_id;
}

void zt_timer_stop(zt_timer_handle timer_id)
{
    if (timer_id == 0) {
        return;
    }

    zt_posix_timer *timer = NULL;
    {
        std::lock_guard<std::mutex> lock(g_timer_mutex);
        const auto it = g_timers.find(timer_id);
        if (it == g_timers.end()) {
            return;
        }
        timer = it->second;
        g_timers.erase(it);
    }

    pthread_mutex_lock(&timer->wake_mutex);
    timer->running.store(false);
    pthread_cond_signal(&timer->wake_cond);
    pthread_mutex_unlock(&timer->wake_mutex);

    // Ownership of teardown lives here (not in the worker) so that the
    // worker's exit can't race pthread_join's read of timer->thread. On the
    // self-stop path (stop called from inside the timer callback) we can't
    // join ourselves — detach instead and accept the struct leak; the process
    // is almost certainly shutting down in that case.
    if (!pthread_equal(pthread_self(), timer->thread)) {
        pthread_join(timer->thread, NULL);
        pthread_cond_destroy(&timer->wake_cond);
        pthread_mutex_destroy(&timer->wake_mutex);
        free(timer);
    } else {
        pthread_detach(timer->thread);
    }
}

unsigned long zt_get_current_directory(unsigned long size, char *buffer)
{
    if (!buffer || size == 0) {
        return 0;
    }
    if (getcwd(buffer, (size_t)size) == NULL) {
        buffer[0] = '\0';
        return 0;
    }
    return (unsigned long)strlen(buffer);
}

int zt_set_current_directory(const char *path)
{
    if (!path || !path[0]) {
        return 0;
    }
    return chdir(path) == 0;
}

void zt_show_error(const char *title, const char *message)
{
    fprintf(stderr, "%s: %s\n", title ? title : "Error", message ? message : "");
}

#endif
