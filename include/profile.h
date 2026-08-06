#ifndef PROFILE_H
#define PROFILE_H

#include <cstddef>
#include <cstdint>

#define MAX_STACK_DEPTH 64
#define MAX_SAMPLES 1024

#ifdef __cplusplus
extern "C" {
#endif

// Structure representing a single captured stack trace
typedef struct StackTrace {
    uintptr_t frames[MAX_STACK_DEPTH];
    size_t depth;

    bool operator==(const StackTrace& other) const {
        if (depth != other.depth) return false;

        for (size_t i = 0; i < depth; ++i) {
            if (frames[i] != other.frames[i]) return false;
        }
        return true;
    }

    bool operator<(const StackTrace& other) const {
        if (depth != other.depth) return depth < other.depth;

        for (size_t i = 0; i < depth; ++i) {
            if (frames[i] != other.frames[i]) return frames[i] < other.frames[i];
        }
        return false;
    }
} StackTrace;

// Global container for profile sample collection
typedef struct {
    StackTrace samples[MAX_SAMPLES];
    size_t sample_count;
} ProfileData;

// Global profile data instance declaration
extern ProfileData g_profile_data;

// Core profiler API function declarations
void setup_signal_handler(void);
void start_timer(int interval_ms);
void stop_timer(void);
size_t get_sample_count(void);
void print_profile_summary(void);
void export_flamegraph(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif // PROFILE_H