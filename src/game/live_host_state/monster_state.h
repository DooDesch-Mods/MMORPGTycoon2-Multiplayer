#pragma once
#include "subscriber_motion.h"
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace mt2mp::game::live_host_state::monsters {
constexpr size_t max_rows=4096;
constexpr size_t animation_slots=49;
struct Lifecycle {
    bool ready=false;uint32_t respawn_time_bits=0;int32_t action=-1;
    bool operator==(const Lifecycle&)const=default;
};
struct TargetObservation {
    std::string state="empty",key;
    bool operator==(const TargetObservation&)const=default;
};
struct CombatObservation {
    std::array<uint32_t,3> values{},recovery{},maxima{};
    bool dead=false;int32_t ai=0,status=0;
    TargetObservation target,aggro;
    bool operator==(const CombatObservation&)const=default;
};
struct Row {
    std::string birth,name;
    int32_t tag=0,rank=0,slot=-1,type=-1,level=0;
    subscriber_motion::Pose pose;
    std::array<float,3> spawn{};
    bool on_map=false;
    // CharacterActor semantic enums are shared; render clip indexes stay local.
    std::array<std::optional<uint32_t>,animation_slots> animation;
    std::array<std::optional<uint32_t>,animation_slots> animation_length;
    std::array<bool,animation_slots> animation_paused{};
    std::array<uint32_t,animation_slots> animation_speed{};
    Lifecycle lifecycle;
    CombatObservation host_combat;
    bool operator==(const Row&)const=default;
};
struct State {
    std::vector<Row> rows;
    std::vector<std::string> retired;
    bool operator==(const State&)const=default;
};
struct Receipt {
    bool attempted=false,confirmed=false,unknown=false;
    uint32_t attempted_rows=0,completed_rows=0;
    std::string cause;
};
bool valid(const State&,std::string&);
nlohmann::json encode(const State&);
bool decode(const nlohmann::json&,State&,std::string&);
bool same_identity(const Row&,const Row&);
bool same_applied(const Row&,const Row&);
bool preflight(const State& local,const State& target,std::string&);
bool adopted(const State& local,const State& target,std::string&);
}
