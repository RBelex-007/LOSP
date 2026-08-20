#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <map>
#include <set>
#include <unordered_map>
#include <string>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <cxxabi.h>

#if !defined(_WIN32)
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>
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

// Fast x86_64 RBP frame pointer unwinder
static size_t unwind_frame_pointer(StackTrace& trace) {
    trace.depth = 0;
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

    return trace.depth;
}

// Fallback DWARF .eh_frame unwinder using libunwind
static size_t unwind_dwarf(StackTrace& trace, void* ucontext) {
    (void)ucontext;
#if !defined(_WIN32)
    trace.depth = 0;
    unw_cursor_t cursor;
    unw_context_t uc;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);

    while (unw_step(&cursor) > 0 && trace.depth < MAX_STACK_DEPTH) {
        unw_word_t ip = 0;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        if (ip == 0) break;

        trace.frames[trace.depth++] = static_cast<uintptr_t>(ip);
    }

    return trace.depth;
#else
    (void)trace;
    return 0;
#endif
}

void profile_handler(int sig, siginfo_t* info, void* ucontext) {
    (void)sig;
    (void)info;

    StackTrace trace;
    trace.depth = 0;

#if !defined(_WIN32)
    trace.tid = static_cast<uint32_t>(syscall(SYS_gettid));
#else
    trace.tid = 0;
#endif

    // 1. Try fast frame pointer unwinding first
    size_t depth = unwind_frame_pointer(trace);

    // 2. If frame pointers were omitted (-fomit-frame-pointer), fallback to DWARF unwinding
    if (depth <= 1) {
        depth = unwind_dwarf(trace, ucontext);
    }

    // 3. Save stack trace atomically into ring buffer
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

        // Prefix with Thread ID if multi-threaded
        if (trace.tid != 0) {
            stack_key += "Thread-" + std::to_string(trace.tid) + ";";
        }

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

static std::string json_escape(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == '"') escaped += "\\\"";
        else if (c == '\\') escaped += "\\\\";
        else if (c == '\b') escaped += "\\b";
        else if (c == '\f') escaped += "\\f";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\t') escaped += "\\t";
        else escaped += c;
    }
    return escaped;
}

void export_speedscope(const char* filepath) {
    size_t head_idx = g_ring_buffer.head.load(std::memory_order_relaxed);
    size_t tail_idx = g_ring_buffer.tail.load(std::memory_order_relaxed);

    std::vector<std::string> shared_frames;
    std::unordered_map<std::string, size_t> frame_index_map;

    auto get_or_add_frame = [&](const std::string& name) -> size_t {
        auto it = frame_index_map.find(name);
        if (it != frame_index_map.end()) return it->second;
        size_t idx = shared_frames.size();
        shared_frames.push_back(name);
        frame_index_map[name] = idx;
        return idx;
    };

    // Group samples by thread ID
    std::map<uint32_t, std::vector<std::vector<size_t>>> thread_samples;

    for (size_t i = tail_idx; i < head_idx && i < tail_idx + MAX_SAMPLES; ++i) {
        size_t slot = i & (MAX_SAMPLES - 1);
        const StackTrace& trace = g_ring_buffer.buffer[slot];
        if (trace.depth == 0) continue;

        std::vector<size_t> sample_frames;
        for (int d = static_cast<int>(trace.depth) - 1; d >= 0; --d) {
            std::string symbol = resolve_symbol(trace.frames[d]);
            sample_frames.push_back(get_or_add_frame(symbol));
        }

        thread_samples[trace.tid].push_back(sample_frames);
    }

    FILE* out = std::fopen(filepath, "w");
    if (!out) {
        std::perror("[LOSP] Failed to open speedscope JSON output file");
        return;
    }

    std::fprintf(out, "{\n");
    std::fprintf(out, "  \"$schema\": \"https://www.speedscope.app/file-format-schema.json\",\n");
    std::fprintf(out, "  \"exporter\": \"LOSP (Low Overhead Sampling Profiler)\",\n");
    std::fprintf(out, "  \"name\": \"%s\",\n", json_escape(filepath).c_str());
    std::fprintf(out, "  \"activeProfileIndex\": 0,\n");
    std::fprintf(out, "  \"shared\": {\n");
    std::fprintf(out, "    \"frames\": [\n");

    for (size_t i = 0; i < shared_frames.size(); ++i) {
        std::fprintf(out, "      {\"name\": \"%s\"}%s\n",
                     json_escape(shared_frames[i]).c_str(),
                     (i + 1 < shared_frames.size()) ? "," : "");
    }
    std::fprintf(out, "    ]\n");
    std::fprintf(out, "  },\n");
    std::fprintf(out, "  \"profiles\": [\n");

    size_t profile_idx = 0;
    for (const auto& kv : thread_samples) {
        uint32_t tid = kv.first;
        const auto& samples = kv.second;

        char prof_name[64];
        if (tid != 0) {
            std::snprintf(prof_name, sizeof(prof_name), "Thread %u", tid);
        } else {
            std::snprintf(prof_name, sizeof(prof_name), "Main Profile");
        }

        std::fprintf(out, "    {\n");
        std::fprintf(out, "      \"type\": \"sampled\",\n");
        std::fprintf(out, "      \"name\": \"%s\",\n", prof_name);
        std::fprintf(out, "      \"unit\": \"samples\",\n");
        std::fprintf(out, "      \"startValue\": 0,\n");
        std::fprintf(out, "      \"endValue\": %zu,\n", samples.size());
        std::fprintf(out, "      \"samples\": [\n");

        for (size_t s = 0; s < samples.size(); ++s) {
            std::fprintf(out, "        [");
            for (size_t f = 0; f < samples[s].size(); ++f) {
                std::fprintf(out, "%zu%s", samples[s][f], (f + 1 < samples[s].size()) ? ", " : "");
            }
            std::fprintf(out, "]%s\n", (s + 1 < samples.size()) ? "," : "");
        }

        std::fprintf(out, "      ],\n");
        std::fprintf(out, "      \"weights\": [\n");
        for (size_t s = 0; s < samples.size(); ++s) {
            std::fprintf(out, "        1%s\n", (s + 1 < samples.size()) ? "," : "");
        }
        std::fprintf(out, "      ]\n");
        std::fprintf(out, "    }%s\n", (profile_idx + 1 < thread_samples.size()) ? "," : "");
        profile_idx++;
    }

    std::fprintf(out, "  ]\n");
    std::fprintf(out, "}\n");

    std::fclose(out);
    std::printf("[LOSP] Successfully exported Speedscope JSON profile to '%s'\n", filepath);
}

void print_profile_summary(void) {
    std::map<StackTrace, uint64_t> aggregated_profile;
    std::set<uint32_t> unique_threads;

    size_t head_idx = g_ring_buffer.head.load(std::memory_order_relaxed);
    size_t tail_idx = g_ring_buffer.tail.load(std::memory_order_relaxed);

    size_t total_samples = (head_idx >= tail_idx) ? (head_idx - tail_idx) : 0;
    if (total_samples > MAX_SAMPLES) total_samples = MAX_SAMPLES;

    // First pass: aggregate samples from ring buffer
    for (size_t i = tail_idx; i < head_idx && i < tail_idx + MAX_SAMPLES; ++i) {
        size_t slot = i & (MAX_SAMPLES - 1);
        const StackTrace& trace = g_ring_buffer.buffer[slot];
        aggregated_profile[trace]++;
        if (trace.tid != 0) {
            unique_threads.insert(trace.tid);
        }
    }

    std::printf("\n==========================================\n");
    std::printf("        LOSP Profiling Report             \n");
    std::printf("==========================================\n");
    std::printf("Total Samples Collected : %zu\n", total_samples);
    std::printf("Unique Stack Traces     : %zu\n", aggregated_profile.size());
    if (!unique_threads.empty()) {
        std::printf("Active Threads Sampled  : %zu\n", unique_threads.size());
    }
    std::printf("------------------------------------------\n");

    size_t rank = 1;
    for (const auto& entry : aggregated_profile) {
        const StackTrace& trace = entry.first;
        uint64_t count = entry.second;
        double pct = (total_samples > 0) ? (count * 100.0 / total_samples) : 0.0;

        if (trace.tid != 0) {
            std::printf("Trace #%zu [TID %u]: Hits = %lu (%.1f%%)\n", rank++, trace.tid, (unsigned long)count, pct);
        } else {
            std::printf("Trace #%zu: Hits = %lu (%.1f%%)\n", rank++, (unsigned long)count, pct);
        }

        for (size_t d = 0; d < trace.depth; ++d) {
            std::string sym = resolve_symbol(trace.frames[d]);
            std::printf("  [%2zu] %s (0x%lx)\n", d, sym.c_str(), (unsigned long)trace.frames[d]);
        }
        std::printf("\n");
        if (rank > 5) break;
    }
    std::printf("==========================================\n\n");
}
