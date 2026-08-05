# LOSP — Low Overhead Sampling Profiler

LOSP (**L**ow **O**verhead **S**ampling **P**rofiler) is a lightweight C/C++ sampling profiler designed for Linux x86_64 systems. It uses POSIX interval timers (`ITIMER_PROF`) and signal handling (`SIGPROF`) to sample application stack traces with minimal CPU and memory overhead.

---

## 📌 Architecture Overview

LOSP operates by periodically interrupting the host program using CPU timer signals to inspect call stacks:

```
+------------------+         SIGPROF (100 Hz / 10ms)        +-------------------+
|  Target Program  | -------------------------------------> |  LOSP Collector   |
|  (User Process)  | <------------------------------------- |  Signal Handler   |
+------------------+         Captures RBP Frame Chain       +-------------------+
                                                                      |
                                                                      v
                                                            +-------------------+
                                                            |  Sample Storage   |
                                                            |  (Stack Traces)   |
                                                            +-------------------+
```

### Core Components
- **Timer Module (`src/timer.cpp`)**: Configures `ITIMER_PROF` via POSIX `setitimer()` to trigger `SIGPROF` at fixed sampling intervals (default: 10ms / 100 Hz).
- **Collector Module (`src/collector.cpp`)**: Installs a POSIX signal handler (`SIGPROF`) that performs fast frame pointer (`rbp`) stack unwinding.
- **Data Structures (`include/profile.h`)**: Defines `StackTrace` structures representing captured instruction pointers up to a configurable maximum depth (default: 64 frames).

---

## 🛠️ Requirements & Dependencies

- **OS**: Linux / POSIX-compliant x86_64 environment (wsl2 / Linux native)
- **Compiler**: `GCC` or `Clang` supporting C++11/C99
- **Build Flags**: Compilation with `-fno-omit-frame-pointer` is required for frame-pointer-based stack unwinding.

---

## 🚀 Quick Start (Development)

### Building
```bash
g++ -std=c++11 -Iinclude -fno-omit-frame-pointer src/collector.cpp src/timer.cpp losp.c -o losp
```

### Running
```bash
./losp
```

---

## 📑 Project Structure

```
LOSP/
├── include/
│   └── profile.h         # Core data structures (StackTrace, Sample definitions)
├── src/
│   ├── collector.cpp     # Signal handling and RBP stack unwinding logic
│   └── timer.cpp         # ITIMER_PROF interval timer setup
├── losp.c                # Profiler main entry point & test harness
├── README.md             # Project overview & usage (this document)
├── PLAN.md               # Architecture specification and roadmap
└── PROGRESS.md           # Implementation progress & issue tracking
```
