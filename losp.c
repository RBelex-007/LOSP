#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
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
        inner_workload(50000);
    }
}

// Worker 1: Mathematical computation chain
void* worker_thread_math(void* arg) {
    (void)arg;
    start_thread_timer(10); // 100 Hz per-thread sampling

    for (int i = 0; i < 8; ++i) {
        compute_heavy_task(40);
    }

    stop_thread_timer();
    return NULL;
}

// Worker 2: Bitwise hashing chain
void* worker_thread_hash(void* arg) {
    (void)arg;
    start_thread_timer(10); // 100 Hz per-thread sampling

    for (int i = 0; i < 6; ++i) {
        compute_heavy_task(50);
    }

    stop_thread_timer();
    return NULL;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("==========================================\n");
    printf(" Starting LOSP (Multi-Threaded Profiler)  \n");
    printf("==========================================\n");

    // 1. Install global SIGPROF signal handler
    setup_signal_handler();

    // 2. Start process-wide timer for main thread
    start_timer(10);

    printf("[LOSP] Spawning concurrent worker threads...\n");

    pthread_t thread1, thread2;
    pthread_create(&thread1, NULL, worker_thread_math, NULL);
    pthread_create(&thread2, NULL, worker_thread_hash, NULL);

    // Main thread also executes some work
    for (int i = 0; i < 4; ++i) {
        compute_heavy_task(30);
    }

    // Wait for all worker threads to complete
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // Stop process timer
    stop_timer();

    printf("[LOSP] All workloads complete. Stopping profiler...\n");

    // 5. Print profile results summary
    print_profile_summary();

    // 6. Export flamegraph data to file
    export_flamegraph("profile.folded");

    // 7. Export speedscope JSON profile
    export_speedscope("profile.speedscope.json");

    return 0;
}
