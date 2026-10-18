#pragma once
#include "subscriber_lifecycle/record_graph.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>

namespace mt2mp::game::marketing_native_probe {
using Json=nlohmann::json;
using Memory=mt2mp::subscriber_lifecycle::Memory;
using Records=std::vector<mt2mp::subscriber_lifecycle::Record>;
inline constexpr const char* baseline_sha="675f562070b00d29ef30bdb9b47bf30494ca9c0fa28d570a8f730fbd0e7350eb";
inline constexpr const char* source_sha="044e51739ee901902621dab042c38fa1a408e3c1ac8d282e420c562cfd972395";
struct Payload { Records award,campaign,launch; };
bool parse_payload(const Json&,Payload&,std::string&);
bool current_campaign(int32_t day,int32_t start,int32_t duration,uint8_t active,std::string&);
enum class ClockPhase { Before, Held, Closed };
struct ClockSample {int32_t day=0,seconds=0;float time_scale=0,pause_scale=0;bool paused=false;};
// Increment sets pause_scale to zero; Decrement leaves it until Mode::Update.
bool same_clock(const ClockSample& before,const ClockSample& actual,ClockPhase,Json& diagnostic,std::string&);
struct Snapshot {
    uintptr_t marketing=0,hud=0,window=0,clock=0;
    std::vector<uintptr_t> awards,campaigns,views,panes;
    Json values,bindings;
    uint32_t width=0,height=0;
    bool operator==(const Snapshot&) const=default;
};
// Read saved fields and cached active bytes. Never call IsActive or a formatter.
bool read_snapshot(Memory&,uintptr_t image,uintptr_t state,uintptr_t mode,Snapshot&,std::string&);
Json report(const Snapshot&);
Json expected_values(bool inserted);
bool baseline(const Snapshot&,std::string&);
bool completed(const Snapshot& before,const Snapshot& after,uintptr_t award,uintptr_t campaign,std::string&);
}
