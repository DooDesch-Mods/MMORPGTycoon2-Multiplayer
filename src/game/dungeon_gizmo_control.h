#pragma once
#include "dungeon_gizmo_reader.h"
#include "dungeon_gizmo_identity.h"
#include "scenery_scene_key.h"
#include "scene_clone/state.h"
#include <optional>
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
#include <vector>
namespace mt2mp::game::dungeon_gizmo_control {
enum class Command:uint8_t {Move=1,Duplicate=2,Destroy=3};
struct Scenery {
    std::string type,variant;std::array<uint32_t,10> pose{};uint32_t height=0;bool automatic=false;
    bool operator==(const Scenery&)const=default;
};
struct SceneryRow {std::string key;Scenery value;bool operator==(const SceneryRow&)const=default;};
inline bool valid(const Scenery& value){
    for(const auto* word:{&value.type,&value.variant})if(word->empty()||word->size()>256||word->find('\0')!=std::string::npos)return false;
    for(auto bits:value.pose)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    if(!std::isfinite(std::bit_cast<float>(value.height)))return false;
    return true;
}
struct Row {
    std::string key;dungeon_gizmo_reader::Record before;
    std::array<uint32_t,10> after{};int32_t cost=0;std::optional<Scenery> scenery;
    bool operator==(const Row&)const=default;
};
struct Intent {
    Command command=Command::Move;std::vector<Row> rows;std::array<uint32_t,3> cash_position{};
    bool operator==(const Intent&)const=default;
};
inline std::array<uint32_t,10> pose(const dungeon_gizmo_reader::Record& row){
    std::array<uint32_t,10> out{};for(size_t n=0;n<4;++n)out[n]=std::bit_cast<uint32_t>(row.rotation[n]);
    for(size_t n=0;n<3;++n){out[n+4]=std::bit_cast<uint32_t>(row.position[n]);out[n+7]=std::bit_cast<uint32_t>(row.scale[n]);}return out;
}
inline void pose(dungeon_gizmo_reader::Record& row,const std::array<uint32_t,10>& value){
    for(size_t n=0;n<4;++n)row.rotation[n]=std::bit_cast<float>(value[n]);
    for(size_t n=0;n<3;++n){row.position[n]=std::bit_cast<float>(value[n+4]);row.scale[n]=std::bit_cast<float>(value[n+7]);}
}
inline const std::array<uint32_t,10> before_pose(const Row& row){return row.scenery?row.scenery->pose:pose(row.before);}
inline std::string birth(const Row& row,const std::string& action,uint32_t index){return row.scenery?scene_clone::birth_key(action,scene_clone::Family::Scenery,index):dungeon_gizmo_identity::birth(action,index);}
inline bool mixed(const Intent& value){return std::any_of(value.rows.begin(),value.rows.end(),[](const auto& row){return row.scenery.has_value();});}
inline bool valid(const Intent& value){
    if(uint8_t(value.command)<1||uint8_t(value.command)>3||value.rows.empty())return false;
    std::set<std::string> keys;int64_t cost=0;bool moved=false;
    for(const auto& row:value.rows){
        if(!(row.scenery?scenery_scene_key::valid(row.key):dungeon_gizmo_identity::key(row.key))||!keys.insert(row.key).second||row.cost<0)return false;
        if(row.scenery&& (row.before!=dungeon_gizmo_reader::Record{}||!valid(*row.scenery)))return false;
        cost+=row.cost;if(cost>INT32_MAX)return false;
        for(auto bits:before_pose(row))if(!std::isfinite(std::bit_cast<float>(bits)))return false;
        for(auto bits:row.after)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
        for(const auto* word:{&row.before.name,&row.before.type,&row.before.variant})if(word->size()>8192||word->find('\0')!=std::string::npos)return false;
        if(!row.before.parent.empty()&&(!dungeon_gizmo_identity::key(row.before.parent)||row.before.parent==row.key))return false;
        if(value.command==Command::Move&&row.cost)return false;
        moved|=row.after!=before_pose(row);
        if(value.command==Command::Destroy&&row.after!=before_pose(row))return false;
    }
    for(auto bits:value.cash_position)if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    return value.command!=Command::Move||moved;
}
inline int32_t total(const Intent& value){int64_t result=0;for(const auto& row:value.rows)result+=row.cost;return int32_t(result);}
inline uint32_t cash_count(const Intent& value){return value.command==Command::Move?0:value.rows.size()>9?1:uint32_t(value.rows.size());}
}
