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

### 1. Timer Subsystem (`src/timer.cpp`) — Status: 🟡 In Progress
- [x] POSIX `setitimer` configuration with `ITIMER_PROF`.
- [x] Configurable default interval (10 ms / 100 Hz sampling frequency).
- [ ] Support for dynamic sampling frequencies (100 Hz – 1000 Hz via CLI arguments).
- [ ] Thread-specific timer handling using `timer_create(CLOCK_THREAD_CPUTIME_ID, ...)` for multi-threaded applications.

### 2. Collector & Unwinder Subsystem (`src/collector.cpp`) — Status: 🟡 In Progress
- [x] `SIGPROF` signal action handler setup (`setup_signal_hanler`).
- [x] Fast x86_64 frame pointer (`RBP`) stack trace unwinding (`__builtin_frame_address(0)`).
- [x] Maximum stack depth limit enforcement (default: 64 frames).
- [x] Infinite loop guard via stack pointer progression check (`next_rbp <= rbp`).
- [ ] **Async-Signal Safety Audit**: Remove non-async-signal-safe calls in signal handler context.
- [ ] Lock-free ring buffer implementation for zero-allocation sample collection.
- [ ] Fallback unwinder (`libunwind` or DWARF `.eh_frame` parser) for binaries compiled with `-fomit-frame-pointer`.

### 3. Data Structures & Types (`include/profile.h`) — Status: 🟡 In Progress
- [x] `StackTrace` structure defining fixed-size frame arrays (`uintptr_t frames[64]`) and depth index.
- [ ] `SampleBuffer` ring buffer structure definition.
- [ ] Atomic reader/writer pointers for concurrent safe sampling.

### 4. Main Entry Point & Test Harness (`losp.c`) — Status: 🟡 In Progress
- [x] Initial header inclusions (`signal.h`, `sys/time.h`, `iostream`).
- [ ] Complete test workload generator (CPU-bound loops, recursive call chains).
- [ ] Profiler lifecycle management (`losp_init()`, `losp_start()`, `losp_stop()`, `losp_dump()`).

### 5. Symbolication & Reporting Engine — Status: 🔴 Pending
- [ ] `/proc/self/maps` dynamic memory map parser to obtain base load addresses.
- [ ] Symbol resolution using `dladdr`, `libbacktrace`, or ELF symbol tables (`libelf`).
- [ ] C++ symbol demangling (`abi::__cxa_demangle`).
- [ ] FlameGraph collapsed stack format exporter (`.folded` format).
- [ ] Console summary report generator (top hot functions and stack hit counters).

---

## 🐛 Known Technical Debt & Issues

1. **Typo in Signal Handler Function**: `setup_signal_hanler()` in `src/collector.cpp` is missing an 'd' (`setup_signal_handler`). Needs API cleanup.
2. **Missing Test Harness Implementation**: `losp.c` has header includes but lacks executable logic (`main()` entry point).
3. **Async Signal Safety**: Local stack allocation in `profile_handler` needs to dump directly into a static lock-free ring buffer to avoid stack overflow risks in restricted signal contexts.
4. **C vs C++ Linkage**: Mixing C (`losp.c`) and C++ (`src/collector.cpp`, `src/timer.cpp`) requires explicit `extern "C"` blocks in headers for C link compatibility.

---

## 🎯 Next Immediate Steps

1. Clean up header declarations in `include/profile.h` with `extern "C"` declarations and function prototypes (`setup_signal_handler`, `start_timer`).
2. Implement a static lock-free ring buffer in `include/profile.h` / `src/collector.cpp`.
3. Complete `losp.c` with a working test program and profile report summary printer.
4. Add a `Makefile` or `CMakeLists.txt` for standardized cross-platform / Linux builds.
