#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "profile.h"

// CPU-bound workload functions to generate realistic call stacks
static volatile unsigned long g_dummy_result = 0;

void inner_workload(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        g_dummy_result += (i * 31) ^ (i >> 3);
    }
}

void compute_heavy_task(int count) {
    for (int i = 0; i < count; ++i) {
        inner_workload(100000);
    }
}

void worker_thread_simulation(void) {
    for (int i = 0; i < 5; ++i) {
        compute_heavy_task(50);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("==========================================\n");
    printf(" Starting LOSP (Low Overhead Sampling Profiler)\n");
    printf("==========================================\n");

    // 1. Install SIGPROF signal handler
    setup_signal_handler();

    // 2. Start ITIMER_PROF interval timer (10 ms / 100 Hz)
    start_timer(10);

    printf("[LOSP] Profiler initialized. Executing profiled workload...\n");

    // 3. Execute profiled workload
    worker_thread_simulation();

    // 4. Stop interval timer
    stop_timer();

    printf("[LOSP] Workload complete. Stopping profiler...\n");

    // 5. Print profile results summary
    print_profile_summary();

    // 6. Export flamegraph data to file
    export_flamegraph("profile.folded");

    return 0;
}
