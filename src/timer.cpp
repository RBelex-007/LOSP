#include <sys/time.h>

void start_timer(){
    struct itimerval timer;

    // Time until first signal
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 10000; // 10 ms

    //Interval for all other signals
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 10000; // 10 ms

    // Start the timer
    setitimer(ITIMER_PROF, &timer, NULL);
}