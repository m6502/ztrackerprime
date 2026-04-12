#ifndef ZT_PLATFORM_H
#define ZT_PLATFORM_H

#include <stdint.h>

#if defined(_WIN32)
    #define ZT_THREAD_CALL __stdcall
#else
    #define ZT_THREAD_CALL
#endif

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

typedef void *zt_mutex_handle;
typedef void *zt_thread_handle;
typedef uintptr_t zt_timer_handle;

typedef unsigned long (ZT_THREAD_CALL *zt_thread_entry)(void *user_data);
typedef void (*zt_timer_callback)(void);

enum {
    ZT_THREAD_PRIORITY_BELOW_NORMAL = -1,
    ZT_THREAD_PRIORITY_ABOVE_NORMAL = 1,
    ZT_THREAD_PRIORITY_TIME_CRITICAL = 15
};

zt_mutex_handle zt_mutex_create(const char *name);
void zt_mutex_destroy(zt_mutex_handle mutex);
int zt_mutex_lock(zt_mutex_handle mutex, int timeout_ms);
int zt_mutex_unlock(zt_mutex_handle mutex);

zt_thread_handle zt_thread_create(zt_thread_entry entry, void *user_data, unsigned long *thread_id);
int zt_thread_join(zt_thread_handle thread);
void zt_thread_terminate(zt_thread_handle thread, unsigned long exit_code);
void zt_thread_close(zt_thread_handle thread);
void zt_thread_set_priority(zt_thread_handle thread, int priority);
int zt_thread_get_current_priority(void);
void zt_thread_set_current_priority(int priority);

zt_timer_handle zt_timer_start(zt_timer_handle timer_id, unsigned int interval_ms, zt_timer_callback callback);
void zt_timer_stop(zt_timer_handle timer_id);

unsigned long zt_get_current_directory(unsigned long size, char *buffer);
int zt_set_current_directory(const char *path);
void zt_show_error(const char *title, const char *message);

#endif
