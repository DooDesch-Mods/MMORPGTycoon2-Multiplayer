#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::native_network {
inline constexpr uint32_t default_node_limit=64,max_node_limit=256;
inline constexpr uint32_t default_region_limit=32,max_region_limit=128;
inline constexpr std::size_t network_response_max_bytes=262144;
struct NodeState {
    uint32_t region_index=0,node_index=0;
    float current=0,last_current=0,capacity=0,supply=0,startup_timer=0;
    bool provides_supply=false,activated=false;
    int32_t effective_supply=0;
};
struct RegionState {
    uint32_t region_index=0,node_slots=0,node_count=0,active_supply_nodes=0;
    bool owned=false,network_manager_present=false;
    int32_t type=0,base_capacity=0,extra_supply=0,total_capacity=0;
};
struct State {
    int32_t base_capacity=0,extra_supply=0,total_capacity=0;
    int32_t analytics_online=0,analytics_capacity=0;
    bool analytics_available=false,serialized_ids_available=false;
    uint32_t region_count=0,eligible_regions=0,node_slots=0,null_node_slots=0,active_supply_nodes=0,activated_nodes=0;
    std::string analytics_error;
    std::string identity_error="serialized Node/Region identity mapping is unproven; indices are local observations";
    std::vector<RegionState> regions;
    std::vector<NodeState> nodes;
};
// Publish a state only after exact-build, thread, receiver and aggregate guards pass.
bool read_state(State& state,std::string& error);
nlohmann::json state_json(const State& state,uint32_t node_limit=default_node_limit,uint32_t region_limit=default_region_limit);
#ifdef MT2MP_NETWORK_TEST
namespace testing {
using Getter=int32_t(*)(void*);
void backend(uintptr_t base,Getter map_capacity,Getter region_capacity,Getter extra_supply);
}
#endif
}
