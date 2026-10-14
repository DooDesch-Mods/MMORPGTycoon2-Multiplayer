#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::native_catalog {
struct Counts {
    bool observed = false;
    bool scan_complete = false;
    uint64_t regions = 0, managers = 0, native_main_slots = 0;
    uint64_t non_null = 0, typed_valid = 0, null_slots = 0, rejected = 0, duplicates = 0;
};
struct Sample {
    uint64_t uid = 0;
    uint32_t root_region_index = 0;
    bool region_id_available = false;
    uint64_t region_id = 0;
    std::string scope = "root", type, variant;
};
struct ReasonCounter { std::string reason; uint64_t count = 0; };
struct State {
    bool shared = false;
    bool scan_complete = false;
    Counts root, dungeon_template, dungeon_instance;
    std::string unobserved = "Dungeon template and instance managers are not enumerated";
    std::string first_error;
    std::vector<ReasonCounter> reasons;
    std::vector<Sample> samples;
};
// On failure, output retains partial counts and the first failed guard.
bool read_native_catalog(State& output, std::string& error);
}
