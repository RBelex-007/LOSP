# LOSP — Progress Tracker & Implementation Status

## 📌 Overview
This document tracks the current development status, completed tasks, ongoing work, and upcoming roadmap items for **LOSP** (Low Overhead Sampling Profiler).

---

## 🚦 Project Status Summary

| Status Flag | Meaning |
|---|---|
| ✅ **Completed** | Fully implemented, verified, and integrated |
| 🟡 **In Progress** | Partially implemented or active work underway |
| 🔴 **Pending** | Planned for future development phases |
| ⚠️ **Blocked / Issue** | Requires refactoring, fix, or design decision |

---

## 🛠 Module & Component Progress

### 1. Build System & Repository Setup — Status: ✅ Completed
- [x] Created `.gitignore` ignoring build binaries, `.exe`, `.o`, and OS artifacts.
- [x] Clean compilation verified with `g++ -std=c++11 -Iinclude -fno-omit-frame-pointer`.
- [x] Cross-platform compatibility shims for Windows (MinGW) and Linux (WSL/GCC).

### 2. Header & API Contracts (`include/profile.h`) — Status: ✅ Completed
- [x] `#ifndef PROFILE_H` guards and `extern "C"` linkage blocks.
- [x] `StackTrace` struct with `operator==` and `operator<` for map-based trace grouping.
- [x] API function prototypes (`setup_signal_handler`, `start_timer`, `stop_timer`, `print_profile_summary`, `export_flamegraph`).

### 3. Collector & Unwinder Subsystem (`src/collector.cpp`) — Status: ✅ Completed
- [x] `SIGPROF` signal action handler setup (`setup_signal_handler`).
- [x] Fast x86_64 frame pointer (`RBP`) stack trace unwinding (`__builtin_frame_address(0)`).
- [x] DWARF `.eh_frame` fallback unwinding via `libunwind` (`unw_init_local`, `unw_step`, `unw_get_reg`).
- [x] Hybrid fast-path + fallback unwinding strategy.
- [x] Maximum stack depth limit enforcement (64 frames).
- [x] Infinite loop guard via pointer type checking (`next_rbp <= current_rbp`).
- [x] Lock-Free Atomic Ring Buffer (`RingBuffer`) with `std::atomic<size_t>` head/tail for async-signal safety.
- [x] Stack trace aggregation map with sample frequency percentages.
- [x] FlameGraph collapsed stack exporter (`export_flamegraph("profile.folded")`).
- [x] Speedscope interactive JSON profile exporter (`export_speedscope("profile.speedscope.json")`).

### 4. Timer Subsystem (`src/timer.cpp`) — Status: ✅ Completed
- [x] POSIX `setitimer` configuration with `ITIMER_PROF` for process-wide profiling.
- [x] Parameterized sampling interval (`start_timer(int interval_ms)`).
- [x] Per-thread POSIX timers using `timer_create` with `CLOCK_THREAD_CPUTIME_ID` and `SIGEV_THREAD_ID`.
- [x] Per-thread timer lifecycle routines (`start_thread_timer`, `stop_thread_timer`).
- [x] Timer disarming and shutdown routine (`stop_timer()`).

### 5. Symbolizer Subsystem (`src/symbolizer.cpp`, `include/symbolizer.h`) — Status: ✅ Completed
- [x] Memory mapping representation with `MemoryMapEntry`.
- [x] `/proc/self/maps` parsing and executable segment filtering.
- [x] Instruction pointer module lookup & relative offset computation (`get_relative_offset`).
- [x] Multi-tier symbol resolution (`dladdr` -> demangle -> proc maps module+offset -> hex fallback).
- [x] Symbol caching (`std::unordered_map<uintptr_t, std::string>`).

### 6. Main Entry Point & Multi-Thread Test Harness (`losp.c`) — Status: ✅ Completed
- [x] Executable `main()` test harness.
- [x] Multi-threaded workload simulation using `pthread_create` with concurrent worker threads.
- [x] Distinct parallel execution chains (math computation, bitwise hashing, and main workload).
- [x] Per-thread profiling aggregation, summary output with TID breakdown, and `.folded` FlameGraph export.
- [x] Automatic Speedscope JSON export (`profile.speedscope.json`).

### 7. Performance & Overhead Benchmarking (`benchmark.cpp`) — Status: ✅ Completed
- [x] Automated benchmark comparing baseline vs 100 Hz vs 1000 Hz profiling.
- [x] Microsecond-accurate overhead calculation and pass/eval criteria.

---

## 🎯 Next Roadmap Steps

1. **Native Windows Backend**: Implement native Windows kernel / multimedia timers and `CaptureStackBackTrace()` + `DbgHelp` for native Windows profiling.
2. **pprof Protobuf Exporter**: Support Google/Go `pprof` binary format.




