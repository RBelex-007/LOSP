#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <sys/time.h>
#include <time.h>
#include <csignal>
#include <cstdio>
#include <cstring>

#if !defined(_WIN32)
#include <unistd.h>
#include <sys/syscall.h>
#endif

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

// Per-thread timer state
#if !defined(_WIN32)
#ifndef SIGEV_THREAD_ID
#define SIGEV_THREAD_ID 4
#endif

static thread_local timer_t g_thread_timer;
static thread_local bool g_thread_timer_active = false;
#endif

// ==========================================
// Process-Wide Profiling Timer (setitimer)
// ==========================================

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

// ==========================================
// Per-Thread Profiling Timer (timer_create)
// ==========================================

void start_thread_timer(int interval_ms) {
    if (interval_ms <= 0) interval_ms = 10;

#if !defined(_WIN32)
    if (g_thread_timer_active) return;

    struct sigevent sev;
    std::memset(&sev, 0, sizeof(sev));

    // Target the calling thread specifically with SIGPROF
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev._sigev_un._tid = static_cast<pid_t>(syscall(SYS_gettid));

    // Create timer using the thread's dedicated CPU clock
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &sev, &g_thread_timer) != 0) {
        std::perror("[LOSP] timer_create failed");
        return;
    }

    struct itimerspec its;
    its.it_value.tv_sec = interval_ms / 1000;
    its.it_value.tv_nsec = (interval_ms % 1000) * 1000000; // milliseconds to nanoseconds
    its.it_interval = its.it_value;

    // Arm the per-thread timer
    if (timer_settime(g_thread_timer, 0, &its, NULL) != 0) {
        std::perror("[LOSP] timer_settime failed");
        timer_delete(g_thread_timer);
        return;
    }

    g_thread_timer_active = true;
#endif
}

void stop_thread_timer(void) {
#if !defined(_WIN32)
    if (!g_thread_timer_active) return;

    timer_delete(g_thread_timer);
    g_thread_timer_active = false;
#endif
}