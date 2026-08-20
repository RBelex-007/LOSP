#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <cmath>
#include <pthread.h>
#include <unistd.h>
#include "profile.h"

static volatile unsigned long g_bench_sink = 0;

// Standard CPU-intensive workload
void bench_inner_loop(int iterations) {
    unsigned long local_acc = 0;
    for (int i = 0; i < iterations; ++i) {
        local_acc += (i * 31) ^ (i >> 3);
        local_acc = (local_acc << 5) | (local_acc >> 27);
    }
    g_bench_sink += local_acc;
}

void bench_workload(int multiplier) {
    for (int i = 0; i < multiplier; ++i) {
        bench_inner_loop(100000);
    }
}

void* bench_thread_worker(void* arg) {
    int interval_ms = *reinterpret_cast<int*>(arg);
    if (interval_ms > 0) {
        start_thread_timer(interval_ms);
    }

    bench_workload(150);

    if (interval_ms > 0) {
        stop_thread_timer();
    }
    return NULL;
}

double run_benchmark_pass(int interval_ms, size_t& samples_collected) {
    if (interval_ms > 0) {
        setup_signal_handler();
        start_timer(interval_ms);
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch concurrent threads
    pthread_t t1, t2;
    int interval_arg = interval_ms;
    pthread_create(&t1, NULL, bench_thread_worker, &interval_arg);
    pthread_create(&t2, NULL, bench_thread_worker, &interval_arg);

    // Main thread also does computation
    bench_workload(150);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    auto end_time = std::chrono::high_resolution_clock::now();

    if (interval_ms > 0) {
        stop_timer();
        samples_collected = get_sample_count();
    } else {
        samples_collected = 0;
    }

    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;
    return elapsed.count();
}

int main(void) {
    std::cout << "========================================================\n";
    std::cout << "        LOSP Performance & Overhead Benchmark          \n";
    std::cout << "========================================================\n";
    std::cout << "Workload: Multi-threaded heavy CPU compute & bitwise hashing\n\n";

    const int NUM_RUNS = 3;

    // 1. Baseline Benchmark (No Profiler)
    std::cout << "[1/3] Running Baseline benchmark (No Profiler)...\n";
    double baseline_total = 0.0;
    for (int r = 0; r < NUM_RUNS; ++r) {
        size_t samples = 0;
        double t = run_benchmark_pass(0, samples);
        baseline_total += t;
        std::cout << "  Run " << (r + 1) << ": " << std::fixed << std::setprecision(2) << t << " ms\n";
    }
    double baseline_avg = baseline_total / NUM_RUNS;

    // 2. 100 Hz Profiling Benchmark (10 ms interval)
    std::cout << "\n[2/3] Running Standard Profiling (100 Hz / 10 ms interval)...\n";
    double prof_100hz_total = 0.0;
    size_t samples_100hz = 0;
    for (int r = 0; r < NUM_RUNS; ++r) {
        size_t s = 0;
        double t = run_benchmark_pass(10, s);
        prof_100hz_total += t;
        samples_100hz += s;
        std::cout << "  Run " << (r + 1) << ": " << std::fixed << std::setprecision(2) << t << " ms (Samples: " << s << ")\n";
    }
    double prof_100hz_avg = prof_100hz_total / NUM_RUNS;

    // 3. 1000 Hz High-Frequency Benchmark (1 ms interval)
    std::cout << "\n[3/3] Running High-Frequency Profiling (1000 Hz / 1 ms interval)...\n";
    double prof_1000hz_total = 0.0;
    size_t samples_1000hz = 0;
    for (int r = 0; r < NUM_RUNS; ++r) {
        size_t s = 0;
        double t = run_benchmark_pass(1, s);
        prof_1000hz_total += t;
        samples_1000hz += s;
        std::cout << "  Run " << (r + 1) << ": " << std::fixed << std::setprecision(2) << t << " ms (Samples: " << s << ")\n";
    }
    double prof_1000hz_avg = prof_1000hz_total / NUM_RUNS;

    // Calculate overhead percentages
    double overhead_100hz = ((prof_100hz_avg - baseline_avg) / baseline_avg) * 100.0;
    double overhead_1000hz = ((prof_1000hz_avg - baseline_avg) / baseline_avg) * 100.0;

    std::cout << "\n========================================================\n";
    std::cout << "                 Benchmark Summary Results              \n";
    std::cout << "========================================================\n";
    std::cout << std::left << std::setw(28) << "Configuration"
              << std::setw(16) << "Avg Time (ms)"
              << std::setw(14) << "Overhead (%)"
              << "Status\n";
    std::cout << "--------------------------------------------------------\n";
    std::cout << std::left << std::setw(28) << "Baseline (No Profiler)"
              << std::setw(16) << baseline_avg
              << std::setw(14) << "0.00%"
              << "Reference\n";
    std::cout << std::left << std::setw(28) << "LOSP Standard (100 Hz)"
              << std::setw(16) << prof_100hz_avg
              << (overhead_100hz >= 0 ? "+" : "") << std::setprecision(2) << overhead_100hz << "%"
              << std::setw(8) << ""
              << (std::abs(overhead_100hz) < 3.0 ? "PASS (<3%)" : "EVAL") << "\n";
    std::cout << std::left << std::setw(28) << "LOSP Stress (1000 Hz)"
              << std::setw(16) << prof_1000hz_avg
              << (overhead_1000hz >= 0 ? "+" : "") << std::setprecision(2) << overhead_1000hz << "%"
              << std::setw(8) << ""
              << (std::abs(overhead_1000hz) < 8.0 ? "PASS (<8%)" : "EVAL") << "\n";
    std::cout << "========================================================\n\n";

    return 0;
}
