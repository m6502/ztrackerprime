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

static void *zt_posix_timer_main(void *arg)
{
    zt_posix_timer *timer = (zt_posix_timer *)arg;
    while (timer->running.load()) {
        struct timespec req;
        req.tv_sec = timer->interval_ms / 1000U;
        req.tv_nsec = (long)(timer->interval_ms % 1000U) * 1000000L;
        nanosleep(&req, NULL);
        if (!timer->running.load()) {
            break;
        }
        if (timer->callback) {
            timer->callback();
        }
    }
    free(timer);
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
    if (pthread_create(&timer->thread, NULL, zt_posix_timer_main, timer) != 0) {
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

    timer->running.store(false);
    if (!pthread_equal(pthread_self(), timer->thread)) {
        pthread_join(timer->thread, NULL);
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
