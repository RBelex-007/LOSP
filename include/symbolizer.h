#ifndef SYMBOLIZER_H
#define SYMBOLIZER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Represents a memory mapping region parsed from /proc/self/maps
struct MemoryMapEntry {
    uintptr_t start_addr;      // Start virtual memory address of the mapping
    uintptr_t end_addr;        // End virtual memory address of the mapping
    uintptr_t offset;          // File offset in the backing binary or shared library
    bool is_executable;        // True if the segment has execute permissions (r-xp)
    std::string pathname;      // Path to the backing executable or shared library (.so)
};

// Initializes the symbolizer and parses /proc/self/maps into memory
void init_symbolizer(void);

// Cleans up any cached symbols and loaded memory mappings
void cleanup_symbolizer(void);

// Resolves a raw instruction address into a human-readable symbol name:
// 1. Checks internal cache
// 2. Tries dynamic lookup (dladdr) + C++ demangling
// 3. Falls back to /proc/self/maps binary offset (e.g. "libc.so.6+0x29d90")
// 4. Falls back to raw hex address (e.g. "0x7f34a000")
std::string resolve_symbol(uintptr_t addr);

// Demangles a C++ ABI symbol name (e.g. "_Z18compute_heavy_taski" -> "compute_heavy_task(int)")
std::string demangle_symbol(const char* mangled_name);

// Finds the memory map entry containing the given instruction pointer
const MemoryMapEntry* find_memory_map_entry(uintptr_t addr);

// Computes the static binary file offset for an address within a memory region
uintptr_t get_relative_offset(uintptr_t addr, const MemoryMapEntry* entry);

#endif // SYMBOLIZER_H