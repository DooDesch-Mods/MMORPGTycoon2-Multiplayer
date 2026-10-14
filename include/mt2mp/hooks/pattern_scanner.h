#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

namespace mt2mp::hooks {

// AOB pattern byte: value + wildcard flag
struct PatternByte {
    uint8_t value;
    bool wildcard;
};

// Parse pattern string like "48 8B 05 ?? ?? ?? ?? 48 85 C0"
std::vector<PatternByte> parse_pattern(const std::string& pattern);

// Scan a memory region for pattern. Returns address of first match.
uintptr_t scan_region(uintptr_t start, size_t size, const std::vector<PatternByte>& pattern);

// Scan the main module (.text section) for pattern
uintptr_t scan_module(const std::vector<PatternByte>& pattern);

// Convenience: parse + scan main module
uintptr_t find_pattern(const std::string& pattern);

// Find function by searching for a string reference in .rdata,
// then finding the LEA instruction that references it,
// then walking back to the function prologue.
uintptr_t find_function_by_string(const char* search_string);

// Find the function prologue (push rbp / sub rsp) walking backwards from addr
uintptr_t find_function_start(uintptr_t addr, size_t max_search = 0x200);

// Get the .text section bounds of the main module
struct SectionInfo {
    uintptr_t base;
    size_t size;
};
SectionInfo get_text_section();
SectionInfo get_rdata_section();

// Find the LEA instruction that references a string (NOT the function start).
// Returns the address of the LEA instruction itself.
// This is used for hardware breakpoints which can break on any instruction.
uintptr_t find_lea_for_string(const char* search_string);

} // namespace mt2mp::hooks
