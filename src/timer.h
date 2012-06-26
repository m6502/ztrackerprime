#ifndef _HIRESTIMER_H_
#define _HIRESTIMER_H_

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

#endif //_HIRESTIMER_H_  - Greets to OMAN for some help with this and origional code

