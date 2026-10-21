#pragma once
#include "../dungeon_gizmo_identity.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <array>
#include <vector>
namespace mt2mp::game::scene_clone {
enum class Family:uint8_t {Gizmo=0,MonsterZone=1,DirectMonster=2,NoneZone=3,PuzzleZone=4,StoryZone=5,Building=6,Scenery=7};
inline int32_t zone_type(Family family){return family==Family::NoneZone?0:family==Family::PuzzleZone?2:family==Family::StoryZone?3:-1;}
struct Assignment {
    std::string scene,key;
    Family family=Family::Gizmo;
    uint32_t child=0,ordinal=0;
    std::string origin;
    bool operator==(const Assignment&)const=default;
};
struct SceneCount {
    std::string scene;
    uint32_t gizmos=0,zones=0,monsters=0;
    std::array<uint32_t,3> other_zones{};
    uint32_t buildings=0,scenery=0;
    bool operator==(const SceneCount&)const=default;
};
struct Mapping {
    std::string operation;
    std::vector<SceneCount> scenes;
    std::vector<Assignment> members,retired;
    bool operator==(const Mapping&)const=default;
};
bool valid(const Mapping&,std::string&);
nlohmann::json encode(const Mapping&);
bool decode(const nlohmann::json&,Mapping&,std::string&);
// UUIDv8 from SHA256(operation UUID, family, ordered roster ordinal). The
// ordinal is assigned once across the complete original ordered scene batch.
std::string birth_key(const std::string&,Family,uint32_t);
}
