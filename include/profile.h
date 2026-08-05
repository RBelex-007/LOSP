#include <unistd.h>
#include <cstdint>

struct StackTrace {
    uintptr_t frames[64];
    size_t depth;
};