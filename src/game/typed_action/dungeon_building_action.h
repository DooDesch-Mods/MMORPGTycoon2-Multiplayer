#pragma once
#include "../scene_clone/state.h"
#include "../dungeon_scene_key.h"
#include "../network_object_identity.h"
#include <array>
#include <map>
#include <optional>
#include <string>
namespace mt2mp::action_session {
struct DungeonBuildingState {
    // Values exclude live Actor motion, which remains in the Actor frame.
    // Each value covers original Canvas/Grid/Layout/Plan and typed scene members.
    std::map<std::string,std::string> authored;
    std::string quests,references;
    std::array<uint64_t,4> random{};
    bool operator==(const DungeonBuildingState&)const=default;
};
struct DungeonBuildingEffect {
    DungeonBuildingState before,after;
    game::scene_clone::Mapping mapping;
    std::string inverse;
    bool operator==(const DungeonBuildingEffect&)const=default;
};
inline bool valid_dungeon_building_state(const DungeonBuildingState& value){
    if(value.authored.size()>4096||!game::network_object_identity::sha(value.quests)||
       !game::network_object_identity::sha(value.references))return false;
    for(const auto& [key,sha]:value.authored)
        if(!game::dungeon_scene_key::building_key(key)||!game::network_object_identity::sha(sha))return false;
    return true;
}
inline bool valid_dungeon_building_scene(const std::string& scene){
    return scene=="world"||(game::dungeon_gizmo_identity::scene(scene)&&scene.ends_with("/template"));
}
inline bool valid_dungeon_building_effect(const DungeonBuildingEffect& value){
    std::string error;
    return valid_dungeon_building_state(value.before)&&valid_dungeon_building_state(value.after)&&
        game::scene_clone::valid(value.mapping,error)&&game::network_object_identity::sha(value.inverse);
}
inline bool dungeon_building_membership(const std::string& key,bool creating,
    const DungeonBuildingState& before,const DungeonBuildingState& after){
    if(!valid_dungeon_building_state(before)||!valid_dungeon_building_state(after)||
       !game::dungeon_scene_key::building_key(key))return false;
    if(creating){if(before.authored.contains(key)||!after.authored.contains(key))return false;}
    else if(!before.authored.contains(key)||after.authored.contains(key))return false;
    auto retained_before=before.authored,retained_after=after.authored;
    retained_before.erase(key);retained_after.erase(key);
    return retained_before==retained_after;
}
}
