#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <sys/time.h>
#include <csignal>
#include <cstdio>
#include "../include/profile.h"

// Cross-Platform POSIX Compatibility Shims for Windows / MinGW
#if defined(_WIN32) || defined(__MINGW32__)

#ifndef ITIMER_PROF
#define ITIMER_PROF 2
#endif

#ifndef _TIMEVAL_DEFINED
struct timeval_compat {
    long tv_sec;
    long tv_usec;
};
#endif

struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};

static inline int setitimer(int which, const struct itimerval* new_value, struct itimerval* old_value) {
    (void)which; (void)new_value; (void)old_value;
    return 0;
}

#endif // _WIN32

void start_timer(int interval_ms) {
    if (interval_ms <= 0) interval_ms = 10; // Default: 10 ms (100 Hz)

    struct itimerval timer;

    // Time until first signal
    timer.it_value.tv_sec = interval_ms / 1000;
    timer.it_value.tv_usec = (interval_ms % 1000) * 1000;

    // Interval for recurring signals
    timer.it_interval.tv_sec = timer.it_value.tv_sec;
    timer.it_interval.tv_usec = timer.it_value.tv_usec;

    // Start POSIX interval timer
    setitimer(ITIMER_PROF, &timer, NULL);
}

void stop_timer(void) {
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    setitimer(ITIMER_PROF, &timer, NULL);
}