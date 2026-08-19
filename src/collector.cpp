#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <cxxabi.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include "../include/profile.h"
#include "../include/symbolizer.h"

// Cross-Platform POSIX Compatibility Shims for Windows / MinGW
#if defined(_WIN32) || defined(__MINGW32__)

struct siginfo_t {
    int si_signo;
    int si_errno;
    int si_code;
};

struct sigaction {
    void (*sa_handler)(int);
    void (*sa_sigaction)(int, siginfo_t*, void*);
    unsigned long sa_mask;
    int sa_flags;
};

// Global instance of profile data
RingBuffer g_ring_buffer = {};

#ifndef SA_SIGINFO
#define SA_SIGINFO 4
#endif
#ifndef SA_RESTART
#define SA_RESTART 0x10000000
#endif
#ifndef SIGPROF
#define SIGPROF 27
#endif

static inline int sigemptyset(unsigned long* set) {
    if (set) *set = 0;
    return 0;
}

static inline int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    (void)signum; (void)act; (void)oldact;
    return 0;
}

#endif // _WIN32

void profile_handler(int sig, siginfo_t* info, void* ucontext) {
    (void)sig;
    (void)info;
    (void)ucontext;

    StackTrace trace;
    trace.depth = 0;

    // Obtain current Frame Pointer (RBP)
    uintptr_t current_rbp = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));

    while (current_rbp != 0 && trace.depth < MAX_STACK_DEPTH) {
        uintptr_t* rbp_ptr = reinterpret_cast<uintptr_t*>(current_rbp);
        
        // Return address is stored at (rbp + 1)
        uintptr_t ret_addr = *(rbp_ptr + 1);
        if (ret_addr == 0) break;

        trace.frames[trace.depth++] = ret_addr;

        // Next RBP is stored at *rbp
        uintptr_t next_rbp = *rbp_ptr;

        // Prevent infinite loops (RBP must strictly grow towards higher memory addresses)
        if (next_rbp <= current_rbp) break;
        current_rbp = next_rbp;
    }

    // Save stack trace atomically into ring buffer
    if (trace.depth > 0) {
        size_t write_idx = g_ring_buffer.head.fetch_add(1, std::memory_order_relaxed);
        size_t slot = write_idx & (MAX_SAMPLES - 1);
        g_ring_buffer.buffer[slot] = trace;
    }
}

void setup_signal_handler(void) {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));

    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = profile_handler;

    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);
}

// Sample count helper
size_t get_sample_count(void) {
    size_t head_idx = g_ring_buffer.head.load(std::memory_order_relaxed);
    size_t tail_idx = g_ring_buffer.tail.load(std::memory_order_relaxed);
    return (head_idx >= tail_idx) ? (head_idx - tail_idx) : 0;
}


void export_flamegraph(const char* filepath) {
    std::map<std::string, size_t> stack_counts;

    size_t head_idx = g_ring_buffer.head.load(std::memory_order_relaxed);
    size_t tail_idx = g_ring_buffer.tail.load(std::memory_order_relaxed);

    for (size_t i = tail_idx; i < head_idx && i < tail_idx + MAX_SAMPLES; ++i) {
        size_t slot = i & (MAX_SAMPLES - 1);
        const StackTrace& trace = g_ring_buffer.buffer[slot];
        std::string stack_key;

        for (int d = static_cast<int>(trace.depth) - 1; d >= 0; --d) {
            std::string symbol = resolve_symbol(trace.frames[d]);
            stack_key += symbol + ";";
        }

        if (!stack_key.empty()) {
            stack_key.pop_back();
            stack_counts[stack_key]++;
        }
    }

    FILE* out_file = std::fopen(filepath, "w");
    if (!out_file) return;

    for (const auto& entry : stack_counts) {
        std::fprintf(out_file, "%s %zu\n", entry.first.c_str(), entry.second);
    }
    std::fclose(out_file);
    std::printf("[LOSP] Successfully exported FlameGraph data to '%s'\n", filepath);
    }

void print_profile_summary(void) {
    std::map<StackTrace, uint64_t> aggregated_profile;

    size_t head_idx = g_ring_buffer.head.load(std::memory_order_relaxed);
    size_t tail_idx = g_ring_buffer.tail.load(std::memory_order_relaxed);

    size_t total_samples = (head_idx >= tail_idx) ? (head_idx - tail_idx) : 0;
    if (total_samples > MAX_SAMPLES) total_samples = MAX_SAMPLES;

    // First pass: aggregate samples from ring buffer
    for (size_t i = tail_idx; i < head_idx && i < tail_idx + MAX_SAMPLES; ++i) {
        size_t slot = i & (MAX_SAMPLES - 1);
        aggregated_profile[g_ring_buffer.buffer[slot]]++;
    }

    std::printf("\n==========================================\n");
    std::printf("        LOSP Profiling Report             \n");
    std::printf("==========================================\n");
    std::printf("Total Samples Collected : %zu\n", total_samples);
    std::printf("Unique Stack Traces     : %zu\n", aggregated_profile.size());
    std::printf("------------------------------------------\n");

    size_t rank = 1;
    for (const auto& entry : aggregated_profile) {
        const StackTrace& trace = entry.first;
        uint64_t count = entry.second;
        double pct = (total_samples > 0) ? (count * 100.0 / total_samples) : 0.0;

        std::printf("Trace #%zu: Hits = %lu (%.1f%%)\n", rank++, (unsigned long)count, pct);
        for (size_t d = 0; d < trace.depth; ++d) {
            std::string sym = resolve_symbol(trace.frames[d]);
            std::printf("  [%2zu] %s (0x%lx)\n", d, sym.c_str(), (unsigned long)trace.frames[d]);
        }
        std::printf("\n");
        if (rank > 5) break;
    }
    std::printf("==========================================\n\n");
}
