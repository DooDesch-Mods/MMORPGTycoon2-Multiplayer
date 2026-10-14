#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace mt2mp::game::snapshot_building_key {
inline std::string root_suffix(int32_t region, int32_t child) {
    return "inline:region{" + std::to_string(region) + "}/buildingManager/building[" + std::to_string(child) + "]";
}
inline bool number(std::string_view text, uint64_t maximum, bool positive) {
    if (text.empty() || (text.size() > 1 && text.front() == '0') || text.front() < '0' || text.front() > '9') return false;
    uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value <= maximum && (!positive || value);
}
inline bool valid(std::string_view value) {
    if (value.size() <= 80 || value.substr(0, 64).find_first_not_of("0123456789abcdef") != std::string_view::npos ||
        value.substr(64, 16) != ":world:building:") return false;
    const auto suffix = value.substr(80);
    if (!suffix.starts_with("inline:")) return number(suffix, INT64_MAX, true);
    constexpr std::string_view prefix = "inline:region{", middle = "}/buildingManager/building[";
    if (!suffix.starts_with(prefix) || !suffix.ends_with(']')) return false;
    const auto split = suffix.find(middle, prefix.size());
    if (split == std::string_view::npos) return false;
    return number(suffix.substr(prefix.size(), split - prefix.size()), INT32_MAX, false) &&
        number(suffix.substr(split + middle.size(), suffix.size() - split - middle.size() - 1), INT32_MAX, false);
}
}
