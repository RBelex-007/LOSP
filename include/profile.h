#ifndef PROFILE_H
#define PROFILE_H

#include <cstddef>
#include <cstdint>
#include <atomic>

#define MAX_STACK_DEPTH 64
#define MAX_SAMPLES 1024

#ifdef __cplusplus
extern "C" {
#endif

// Structure representing a single captured stack trace
typedef struct StackTrace {
    uintptr_t frames[MAX_STACK_DEPTH];
    size_t depth;
    uint32_t tid; // Thread ID (TID) that captured this sample

    bool operator==(const StackTrace& other) const {
        if (tid != other.tid) return false;
        if (depth != other.depth) return false;

        for (size_t i = 0; i < depth; ++i) {
            if (frames[i] != other.frames[i]) return false;
        }
        return true;
    }

    bool operator<(const StackTrace& other) const {
        if (tid != other.tid) return tid < other.tid;
        if (depth != other.depth) return depth < other.depth;

        for (size_t i = 0; i < depth; ++i) {
            if (frames[i] != other.frames[i]) return frames[i] < other.frames[i];
        }
        return false;
    }
} StackTrace;

// Global container for profile sample collection
typedef struct {
    StackTrace buffer[MAX_SAMPLES];
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
} RingBuffer;

// Global profile data instance declaration
extern RingBuffer g_ring_buffer;

// Core profiler API function declarations
void setup_signal_handler(void);

// Process-wide timer API (setitimer)
void start_timer(int interval_ms);
void stop_timer(void);

// Per-thread timer API (timer_create + CLOCK_THREAD_CPUTIME_ID)
void start_thread_timer(int interval_ms);
void stop_thread_timer(void);

// Statistics and export API
size_t get_sample_count(void);
void print_profile_summary(void);
void export_flamegraph(const char* filepath);
void export_speedscope(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // PROFILE_H