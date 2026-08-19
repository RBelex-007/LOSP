#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <unordered_map>
#include <cxxabi.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include "../include/symbolizer.h"

// Internal module state
static std::vector<MemoryMapEntry> g_map_entries;
static std::unordered_map<uintptr_t, std::string> g_symbol_cache;
static bool g_symbolizer_initialized = false;

// Helper to extract the filename basename from a full file path
static std::string get_basename(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        return path.substr(last_slash + 1);
    }
    return path;
}

void init_symbolizer(void) {
    if (g_symbolizer_initialized) return;
    g_symbolizer_initialized = true;

#if !defined(_WIN32)
    FILE* fp = std::fopen("/proc/self/maps", "r");
    if (!fp) return;

    char line[512];
    while (std::fgets(line, sizeof(line), fp)) {
        uintptr_t start = 0, end = 0, offset = 0;
        char perms[16] = {0};
        char dev[16] = {0};
        unsigned long inode = 0;
        char path[256] = {0};

        int count = std::sscanf(line, "%" SCNxPTR "-%" SCNxPTR " %15s %" SCNxPTR " %15s %lu %255s",
                                &start, &end, perms, &offset, dev, &inode, path);
        if (count >= 4) {
            MemoryMapEntry entry;
            entry.start_addr = start;
            entry.end_addr = end;
            entry.offset = offset;
            entry.is_executable = (perms[2] == 'x');
            entry.pathname = (count >= 7) ? path : "";

            // Only retain executable segments backed by actual files (ignore [stack], [heap], [vdso])
            if (entry.is_executable && !entry.pathname.empty() && entry.pathname[0] != '[') {
                g_map_entries.push_back(entry);
            }
        }
    }
    std::fclose(fp);
#endif
}

void cleanup_symbolizer(void) {
    g_map_entries.clear();
    g_symbol_cache.clear();
    g_symbolizer_initialized = false;
}

const MemoryMapEntry* find_memory_map_entry(uintptr_t addr) {
    for (const auto& entry : g_map_entries) {
        if (entry.start_addr <= addr && addr < entry.end_addr) {
            return &entry;
        }
    }
    return nullptr;
}

uintptr_t get_relative_offset(uintptr_t addr, const MemoryMapEntry* entry) {
    if (!entry) return 0;
    return addr - entry->start_addr + entry->offset;
}

std::string demangle_symbol(const char* mangled_name) {
    if (!mangled_name || mangled_name[0] == '\0') {
        return "";
    }

    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
        std::string result(demangled);
        std::free(demangled);
        return result;
    }
    return std::string(mangled_name);
}

std::string resolve_symbol(uintptr_t addr) {
    if (!g_symbolizer_initialized) {
        init_symbolizer();
    }

    // Step 1: Check fast lookup cache
    auto cache_it = g_symbol_cache.find(addr);
    if (cache_it != g_symbol_cache.end()) {
        return cache_it->second;
    }

    // Step 2: Tier 1 - Dynamic library symbol lookup (dladdr)
#if !defined(_WIN32)
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(addr), &info) && info.dli_sname && info.dli_sname[0] != '\0') {
        std::string demangled = demangle_symbol(info.dli_sname);
        g_symbol_cache[addr] = demangled;
        return demangled;
    }
#endif

    // Step 3: Tier 2 - /proc/self/maps binary offset fallback
    const MemoryMapEntry* entry = find_memory_map_entry(addr);
    if (entry) {
        std::string module_name = get_basename(entry->pathname);
        uintptr_t rel_offset = get_relative_offset(addr, entry);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s+0x%lx", module_name.c_str(), (unsigned long)rel_offset);
        std::string result(buf);
        g_symbol_cache[addr] = result;
        return result;
    }

    // Step 4: Tier 3 - Raw hexadecimal fallback
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%lx", (unsigned long)addr);
    std::string fallback(buf);
    g_symbol_cache[addr] = fallback;
    return fallback;
}