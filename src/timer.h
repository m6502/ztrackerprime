// Portable high-resolution timer — replaces Win32 QueryPerformanceCounter.

#ifndef _HIRESTIMER_H_
#define _HIRESTIMER_H_

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
        // std::chrono::high_resolution_clock counts in nanoseconds typically.
        // We express frequency as counts per second, matching QueryPerformanceFrequency semantics.
        using namespace std::chrono;
        auto period = high_resolution_clock::period();
        _timer_freq = static_cast<int64_t>(high_resolution_clock::period::den)
                    / static_cast<int64_t>(high_resolution_clock::period::num);
        _ms_div = _timer_freq / 1000000;
    }
    inline int64_t get_counter(void) {
        using namespace std::chrono;
        auto now = high_resolution_clock::now();
        return static_cast<int64_t>(now.time_since_epoch().count());
    }
    inline int64_t get_frequency(void) {
        return _timer_freq;
    }
    inline int64_t get_ms_freq(void) {
        return _ms_div;
    }
};

#endif //_HIRESTIMER_H_
