#pragma once
#include "../dungeon_gizmo_identity.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <vector>
namespace mt2mp::game::scene_clone {
enum class Family:uint8_t {Gizmo=0,MonsterZone=1,DirectMonster=2};
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
