// portar

#ifndef _HIRESTIMER_H_
#define _HIRESTIMER_H_

#if defined(_WIN32)
#include "winmm_compat.h"

class hires_timer
{
private:
    __int64    _timer_freq;
    __int64    _ms_div;
public:
    hires_timer(void) {
        timer_init();
    }
    inline void timer_init(void) {
        QueryPerformanceFrequency((_LARGE_INTEGER *)&_timer_freq);
        _ms_div = _timer_freq / 1000000;
    }
    inline __int64 get_counter(void) {
        __int64 p;
        if (QueryPerformanceCounter((_LARGE_INTEGER *)&p))
            return p;
        else
            return -1;
    }
    inline __int64 get_frequency(void) {
        return _timer_freq;
    }

    inline __int64 get_ms_freq(void) {
        return _ms_div;
    }
};

#else

#include <stdint.h>
#include <chrono>

class hires_timer
{
private:
    int64_t _timer_freq;
    int64_t _ms_div;
public:
    hires_timer(void) {
        timer_init();
    }
    inline void timer_init(void) {
        _timer_freq = 1000000;
        _ms_div = 1;
    }
    inline int64_t get_counter(void) {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }
    inline int64_t get_frequency(void) {
        return _timer_freq;
    }

    inline int64_t get_ms_freq(void) {
        return _ms_div;
    }
};

#endif

#endif //_HIRESTIMER_H_  - Greets to OMAN for some help with this and original code
