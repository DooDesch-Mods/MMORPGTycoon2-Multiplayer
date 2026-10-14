#include "mt2mp/hooks/pattern_scanner.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <algorithm>
#include <sstream>

namespace mt2mp::hooks {

std::vector<PatternByte> parse_pattern(const std::string& pattern) {
    std::vector<PatternByte> result;
    std::istringstream stream(pattern);
    std::string token;

    while (stream >> token) {
        if (token == "??" || token == "?") {
            result.push_back({0, true});
        } else {
            result.push_back({static_cast<uint8_t>(std::stoul(token, nullptr, 16)), false});
        }
    }
    return result;
}

uintptr_t scan_region(uintptr_t start, size_t size, const std::vector<PatternByte>& pattern) {
    if (pattern.empty() || size < pattern.size()) return 0;

    const auto* data = reinterpret_cast<const uint8_t*>(start);
    const size_t scan_end = size - pattern.size();

    for (size_t i = 0; i <= scan_end; ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (!pattern[j].wildcard && data[i + j] != pattern[j].value) {
                match = false;
                break;
            }
        }
        if (match) return start + i;
    }
    return 0;
}

static HMODULE get_main_module() {
    return GetModuleHandleA(nullptr);
}

static IMAGE_NT_HEADERS* get_nt_headers(HMODULE module) {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    return nt;
}

static SectionInfo get_section(const char* name) {
    HMODULE module = get_main_module();
    auto* nt = get_nt_headers(module);
    if (!nt) return {0, 0};

    auto* section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
        if (strncmp(reinterpret_cast<const char*>(section->Name), name, 8) == 0) {
            return {
                reinterpret_cast<uintptr_t>(module) + section->VirtualAddress,
                section->Misc.VirtualSize
            };
        }
    }
    return {0, 0};
}

SectionInfo get_text_section() { return get_section(".text"); }
SectionInfo get_rdata_section() { return get_section(".rdata"); }

uintptr_t scan_module(const std::vector<PatternByte>& pattern) {
    auto text = get_text_section();
    if (!text.base) {
        LOG_ERROR("Failed to find .text section");
        return 0;
    }
    return scan_region(text.base, text.size, pattern);
}

uintptr_t find_pattern(const std::string& pattern) {
    return scan_module(parse_pattern(pattern));
}

// Check if a byte sequence looks like NOP padding (MinGW function separator)
static bool is_nop_padding(const uint8_t* p, size_t max_len) {
    // MinGW uses these NOP patterns between functions:
    // 90                    = nop
    // 66 90                 = 66 nop
    // 0F 1F 00              = nop dword [rax]
    // 0F 1F 40 00           = nop dword [rax+0]
    // 66 66 2E 0F 1F 84 00 00 00 00 00 = long nop
    // 66 2E 0F 1F 84 00 00 00 00 00    = long nop
    for (size_t i = 0; i < max_len; ++i) {
        if (p[i] == 0x90) continue;                    // nop
        if (p[i] == 0x66 && i+1 < max_len) {
            if (p[i+1] == 0x90) { i++; continue; }     // 66 nop
            if (p[i+1] == 0x66) continue;               // part of long nop
            if (p[i+1] == 0x2E) continue;               // part of long nop
        }
        if (p[i] == 0x0F && i+2 < max_len && p[i+1] == 0x1F) {
            return true; // definitely a multi-byte NOP
        }
        if (p[i] == 0x2E) continue; // segment override in long nop
        return false; // not NOP padding
    }
    return true;
}

// Skip forward past a complete NOP/alignment padding sequence.
// Returns number of bytes consumed (0 if not a NOP).
static size_t skip_nop_sequence(const uint8_t* p) {
    size_t pos = 0;
    while (pos < 32) {
        if (p[pos] == 0x90) { pos++; continue; }                                       // 1-byte NOP
        if (p[pos] == 0x66 && p[pos+1] == 0x90) { pos += 2; continue; }                // 2-byte NOP
        if (p[pos] == 0x0F && p[pos+1] == 0x1F && p[pos+2] == 0x00) { pos += 3; continue; } // 3-byte NOP
        if (p[pos] == 0x0F && p[pos+1] == 0x1F && p[pos+2] == 0x40) { pos += 4; continue; } // 4-byte NOP
        if (p[pos] == 0x0F && p[pos+1] == 0x1F && p[pos+2] == 0x44) { pos += 5; continue; } // 5-byte NOP
        if (p[pos] == 0x66 && p[pos+1] == 0x0F && p[pos+2] == 0x1F && p[pos+3] == 0x44) { pos += 6; continue; }
        if (p[pos] == 0x0F && p[pos+1] == 0x1F && p[pos+2] == 0x80) { pos += 7; continue; } // 7-byte NOP: 0F 1F 80 xx xx xx xx
        if (p[pos] == 0x0F && p[pos+1] == 0x1F && p[pos+2] == 0x84) { pos += 8; continue; } // 8-byte NOP: 0F 1F 84 00 xx xx xx xx
        // 66 66 2E 0F 1F 84 00 XX XX XX XX = 11-byte NOP
        if (p[pos] == 0x66 && p[pos+1] == 0x66 && p[pos+2] == 0x2E && p[pos+3] == 0x0F && p[pos+4] == 0x1F) { pos += 11; continue; }
        // 66 2E 0F 1F 84 00 XX XX XX XX = 10-byte NOP
        if (p[pos] == 0x66 && p[pos+1] == 0x2E && p[pos+2] == 0x0F && p[pos+3] == 0x1F) { pos += 10; continue; }
        // CC = int3 (also used as padding)
        if (p[pos] == 0xCC) { pos++; continue; }
        break;
    }
    return pos;
}

uintptr_t find_function_start(uintptr_t addr, size_t max_search_in) {
    // MinGW functions can be very large. Default 0x200 is too small.
    size_t max_search = (max_search_in > 0x200) ? max_search_in : 0x2000;
    const auto* data = reinterpret_cast<const uint8_t*>(addr);

    // Strategy 1: Look for ret/int3 + NOP padding (standard pattern)
    for (size_t i = 1; i < max_search; ++i) {
        const uint8_t* p = data - i;
        uint8_t b = *p;

        if (b != 0xC3 && b != 0xCC) continue;

        size_t nop_len = skip_nop_sequence(p + 1);
        if (nop_len < 2) continue;

        uintptr_t candidate = reinterpret_cast<uintptr_t>(p) + 1 + nop_len;
        if (candidate >= addr) continue;

        // Log bytes around candidate for diagnostics
        const uint8_t* fn = reinterpret_cast<const uint8_t*>(candidate);
        LOG_DEBUG("Candidate bytes [-4..+4]: {:02X} {:02X} {:02X} {:02X} [{:02X}] {:02X} {:02X} {:02X} {:02X}",
            fn[-4], fn[-3], fn[-2], fn[-1], fn[0], fn[1], fn[2], fn[3], fn[4]);

        // Verify: scan backward from candidate to find the actual prologue start.
        // The NOP length might be off by a few bytes. Look for push reg or sub rsp.
        uintptr_t verified = candidate;
        for (int adj = -8; adj <= 4; ++adj) {
            const uint8_t* try_fn = fn + adj;
            if (try_fn[0] == 0x55 || try_fn[0] == 0x57 || try_fn[0] == 0x56 ||
                try_fn[0] == 0x53 ||
                (try_fn[0] == 0x41 && (try_fn[1] & 0xF8) == 0x50) || // push r8-r15
                (try_fn[0] == 0x48 && try_fn[1] == 0x83 && try_fn[2] == 0xEC) ||
                (try_fn[0] == 0x48 && try_fn[1] == 0x89 && (try_fn[2] & 0xC0) != 0xC0)) {
                verified = candidate + adj;
                break;
            }
        }

        LOG_DEBUG("Found function start after {:02X} + {} NOP bytes at 0x{:X}{}",
            b, nop_len, verified,
            (verified != candidate) ?
                (" (adjusted by " + std::to_string((intptr_t)(verified - candidate)) + " from 0x" +
                 [](uintptr_t v) { char buf[32]; snprintf(buf, sizeof(buf), "%llX", (unsigned long long)v); return std::string(buf); }(candidate) + ")")
                : "");
        return verified;
    }

    // Strategy 2: MinGW sometimes ends functions with a call to a noreturn
    // function (e.g., __cxa_throw, _Unwind_Resume) followed by NOP padding
    // WITHOUT a preceding ret. Scan for NOP sequences of 4+ bytes that
    // are followed by a plausible function prologue.
    for (size_t i = 1; i < max_search; ++i) {
        const uint8_t* p = data - i;

        // Look for the start of a NOP sequence
        size_t nop_len = skip_nop_sequence(p);
        if (nop_len < 4) continue;

        uintptr_t candidate = reinterpret_cast<uintptr_t>(p) + nop_len;
        if (candidate >= addr) continue;

        // Verify: the byte after NOP padding should look like a function prologue
        const uint8_t* fn_start = reinterpret_cast<const uint8_t*>(candidate);
        bool is_prologue =
            fn_start[0] == 0x55 ||                       // push rbp
            fn_start[0] == 0x57 ||                       // push rdi
            fn_start[0] == 0x56 ||                       // push rsi
            fn_start[0] == 0x53 ||                       // push rbx
            fn_start[0] == 0x41 ||                       // push r8-r15 (41 5x)
            fn_start[0] == 0x50 ||                       // push rax
            fn_start[0] == 0x51 ||                       // push rcx
            fn_start[0] == 0x52 ||                       // push rdx
            (fn_start[0] == 0x48 && fn_start[1] == 0x83 && fn_start[2] == 0xEC) || // sub rsp, imm8
            (fn_start[0] == 0x48 && fn_start[1] == 0x81 && fn_start[2] == 0xEC) || // sub rsp, imm32
            (fn_start[0] == 0x48 && fn_start[1] == 0x89) ||                        // mov reg/mem, reg (common spill)
            (fn_start[0] == 0x4C && fn_start[1] == 0x89) ||                        // mov reg/mem, r8-r15
            (fn_start[0] == 0xB8) ||                                                // mov eax, imm32
            (fn_start[0] == 0x31 && fn_start[1] == 0xC0) ||                        // xor eax, eax
            (fn_start[0] == 0xC7);                                                  // mov r/m32, imm32

        if (!is_prologue) continue;

        // Also verify: the byte BEFORE the NOP sequence shouldn't be a NOP itself
        // (we want the START of the NOP block)
        if (i + 1 < max_search) {
            const uint8_t* before = data - i - 1;
            if (*before == 0x90 || *before == 0xCC) continue; // still in NOP block
        }

        // Also verify with small adjustment
        uintptr_t verified2 = candidate;
        for (int adj = -4; adj <= 4; ++adj) {
            const uint8_t* try_fn = fn_start + adj;
            if (try_fn[0] == 0x55 || try_fn[0] == 0x57 || try_fn[0] == 0x56 ||
                try_fn[0] == 0x53 || try_fn[0] == 0x41 ||
                (try_fn[0] == 0x48 && try_fn[1] == 0x83 && try_fn[2] == 0xEC) ||
                (try_fn[0] == 0x48 && try_fn[1] == 0x89 && (try_fn[2] & 0xC0) != 0xC0)) {
                verified2 = candidate + adj;
                break;
            }
        }

        LOG_DEBUG("Found function start after NOP padding ({} bytes) at 0x{:X} (prologue: {:02X})",
            nop_len, verified2, *reinterpret_cast<const uint8_t*>(verified2));
        return verified2;
    }

    return 0;
}

uintptr_t find_function_by_string(const char* search_string) {
    // Step 1: Find the string in .rdata
    auto rdata = get_rdata_section();
    if (!rdata.base) {
        LOG_ERROR("Failed to find .rdata section");
        return 0;
    }

    size_t str_len = strlen(search_string);
    uintptr_t str_addr = 0;

    const auto* rdata_bytes = reinterpret_cast<const uint8_t*>(rdata.base);
    for (size_t i = 0; i <= rdata.size - str_len; ++i) {
        if (memcmp(rdata_bytes + i, search_string, str_len) == 0) {
            str_addr = rdata.base + i;
            break;
        }
    }

    if (!str_addr) {
        LOG_WARN("String not found in .rdata: \"{}\"", search_string);
        return 0;
    }

    // Walk back to the start of the full string (previous null byte).
    // LEA instructions reference the START of the string, not our substring.
    uintptr_t full_str_addr = str_addr;
    while (full_str_addr > rdata.base) {
        uint8_t prev = *reinterpret_cast<const uint8_t*>(full_str_addr - 1);
        if (prev == 0) break;
        full_str_addr--;
    }
    if (full_str_addr != str_addr) {
        LOG_DEBUG("String \"{}\" is substring at 0x{:X}, full string starts at 0x{:X} (offset -{})",
            search_string, str_addr, full_str_addr, str_addr - full_str_addr);
        str_addr = full_str_addr; // Search for LEA to the full string start
    }

    LOG_DEBUG("Found string \"{}\" at 0x{:X}", search_string, str_addr);

    // Step 2: Find LEA instruction referencing this string in .text
    auto text = get_text_section();
    if (!text.base) return 0;

    const auto* text_bytes = reinterpret_cast<const uint8_t*>(text.base);

    for (size_t i = 0; i < text.size - 7; ++i) {
        bool is_lea = false;
        size_t disp_offset = 0;

        if ((text_bytes[i] == 0x48 || text_bytes[i] == 0x4C) &&
            text_bytes[i + 1] == 0x8D &&
            (text_bytes[i + 2] & 0xC7) == 0x05) {
            is_lea = true;
            disp_offset = i + 3;
        }

        if (!is_lea) continue;

        uintptr_t instr_addr = text.base + i;
        int32_t disp = *reinterpret_cast<const int32_t*>(text_bytes + disp_offset);
        uintptr_t target = instr_addr + 7 + disp;

        if (target == str_addr) {
            LOG_DEBUG("Found LEA referencing string at 0x{:X}", instr_addr);

            // Step 3: Find the containing function
            uintptr_t func = find_function_start(instr_addr);
            if (func && func < instr_addr && (instr_addr - func) < 0x2000) {
                LOG_INFO("Found function for \"{}\" at 0x{:X} (LEA at +0x{:X})",
                    search_string, func, instr_addr - func);
                return func;
            }

            LOG_WARN("Could not find function start for \"{}\", LEA at 0x{:X}",
                search_string, instr_addr);
            return 0; // Don't return mid-function fallback - it crashes
        }
    }

    LOG_WARN("No code reference found for string: \"{}\"", search_string);
    return 0;
}

uintptr_t find_lea_for_string(const char* search_string) {
    auto rdata = get_rdata_section();
    if (!rdata.base) return 0;

    size_t str_len = strlen(search_string);
    uintptr_t str_addr = 0;

    const auto* rdata_bytes = reinterpret_cast<const uint8_t*>(rdata.base);
    for (size_t i = 0; i <= rdata.size - str_len; ++i) {
        if (memcmp(rdata_bytes + i, search_string, str_len) == 0) {
            str_addr = rdata.base + i;
            break;
        }
    }
    if (!str_addr) return 0;

    // Walk back to full string start
    while (str_addr > rdata.base && *reinterpret_cast<const uint8_t*>(str_addr - 1) != 0) {
        str_addr--;
    }

    auto text = get_text_section();
    if (!text.base) return 0;

    const auto* text_bytes = reinterpret_cast<const uint8_t*>(text.base);
    for (size_t i = 0; i < text.size - 7; ++i) {
        if ((text_bytes[i] == 0x48 || text_bytes[i] == 0x4C) &&
            text_bytes[i + 1] == 0x8D &&
            (text_bytes[i + 2] & 0xC7) == 0x05) {
            uintptr_t instr_addr = text.base + i;
            int32_t disp = *reinterpret_cast<const int32_t*>(text_bytes + i + 3);
            uintptr_t target = instr_addr + 7 + disp;
            if (target == str_addr) {
                LOG_INFO("Found LEA for \"{}\" at 0x{:X}", search_string, instr_addr);
                return instr_addr;
            }
        }
    }

    LOG_WARN("No LEA found for string: \"{}\"", search_string);
    return 0;
}

} // namespace mt2mp::hooks
