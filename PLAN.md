# LOSP Development Plan & Technical Roadmap

## 🎯 Objective
LOSP (**L**ow **O**verhead **S**ampling **P**rofiler) aims to deliver a high-performance, non-intrusive sampling profiler written in C/C++. It enables developers to profile CPU time and hot call paths in native Linux applications with minimal performance degradation (<1-2% overhead).

---

## 🏛️ System Architecture Design

### Phase 1: Core Sampling Infrastructure (Current Phase)
1. **Timer Mechanism**: Use `ITIMER_PROF` (or `timer_create` / `perf_event_open`) to generate `SIGPROF` at configurable sampling frequencies (e.g., 100 Hz - 1000 Hz).
2. **Signal Handler Unwinder**:
   - Fast, async-signal-safe frame pointer unwinding using x86_64 RBP navigation.
   - Fallback/extension to `libunwind` or DWARF `.eh_frame` unwinding for optimized binaries compiled without frame pointers.
3. **Async-Safe Storage**:
   - Lock-free, fixed-size ring buffer for recording raw stack trace samples inside the signal handler context without allocations or mutex locks.

### Phase 2: Symbolication & Resolution
1. **Address-to-Symbol Mapping**:
   - Read `/proc/self/maps` to resolve dynamic library load addresses and base offsets.
   - Use `libbacktrace`, `dladdr`, or ELF binary parsing (`libelf` / `binutils` / `addr2line`) to convert instruction pointers (`uintptr_t`) to function names, source files, and line numbers.
2. **Demangling**: C++ symbol demangling using `abi::__cxa_demangle`.

### Phase 3: Aggregation & Output Formatting
1. **Sample Aggregation**: Hash map based counter mapping stack traces to hit counts.
2. **Export Formats**:
   - **Collapsed Stack Format**: Compatible with FlameGraph (`flamegraph.pl`).
   - **pprof / gperftools Format**: Compatible with `google-pprof` / `go tool pprof`.
   - **JSON / Chrome Trace Format**: For visual inspection in modern UI tooling.

### Phase 4: Advanced Features & Robustness
1. **Multi-Thread Support**: Per-thread timers using `timer_create(CLOCK_THREAD_CPUTIME_ID, ...)` or `pthread_kill` signal redirection.
2. **Low Overhead Verification**: Benchmarking profiler overhead against standard CPU intensive workloads.

---

## 🗓️ Milestones & Tasks

| Milestone | Task | Priority | Status |
|---|---|---|---|
| **M1: Baseline Engine** | Basic signal handler & ITIMER_PROF setup | High | 🟡 In Progress |
| | Fix memory safety (buffer allocation for samples) | High | 🔴 Pending |
| | Implement lock-free sample ring buffer | High | 🔴 Pending |
| **M2: Symbolication** | Proc map parser & address resolution | Medium | 🔴 Pending |
| | C++ symbol demangling | Medium | 🔴 Pending |
| **M3: Reporting** | Collapsed stack format exporter for FlameGraphs | High | 🔴 Pending |
| | Summary report output generator | Medium | 🔴 Pending |
| **M4: Multi-threading** | Multi-threaded signal handling & thread-local state | Low | 🔴 Pending |
