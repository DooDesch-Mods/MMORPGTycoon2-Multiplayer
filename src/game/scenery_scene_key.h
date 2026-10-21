#pragma once
#include <stdexcept>
#include "dungeon_gizmo_identity.h"
namespace mt2mp::game::scenery_scene_key {
inline std::string record(const std::string& sha,int32_t region,int32_t building,int32_t instance,int32_t child){
    auto key=dungeon_gizmo_identity::record(sha,region,building,instance,child);
    const auto mark=key.rfind(":gizmo:");
    if(mark==std::string::npos)throw std::runtime_error("Scene Scenery saved record path differs");
    key.replace(mark,7,":scenery:");const auto child_mark=key.rfind("/gizmo[");
    if(child_mark==std::string::npos)throw std::runtime_error("Scene Scenery saved child path differs");
    key.replace(child_mark,7,"/scenery[");return key;
}
// Original root Region -> SceneryManager.type[category] -> saved Scenery child.
inline std::string root_record(const std::string& sha,int32_t region,int32_t category,int32_t child){
    return sha+":world:scenery:inline:region{"+std::to_string(region)+"}/sceneryManager/type["+
        std::to_string(category)+"]/scenery["+std::to_string(child)+"]";
}
inline bool root_saved(std::string_view key){
    constexpr std::string_view prefix=":world:scenery:inline:region{";
    if(key.size()<=64+prefix.size()||!dungeon_gizmo_identity::digest(key.substr(0,64))||key.substr(64,prefix.size())!=prefix)return false;
    key.remove_prefix(64+prefix.size());
    if(!dungeon_gizmo_identity::take_number(key,"}/sceneryManager/type["))return false;
    const auto end=key.find("]/scenery[");
    if(end==std::string_view::npos||!snapshot_building_key::number(key.substr(0,end),46,false))return false;
    key.remove_prefix(end+10);
    return key.ends_with(']')&&snapshot_building_key::number(key.substr(0,key.size()-1),INT32_MAX,false);
}
inline bool scene(std::string_view key){return key=="world"||dungeon_gizmo_identity::scene(key);}
inline bool saved(std::string_view key){
    if(root_saved(key))return true;
    const auto mark=key.rfind(":scenery:");if(mark==std::string_view::npos)return false;
    const auto child=key.rfind("/scenery[");if(child==std::string_view::npos||child<=mark)return false;
    auto copy=std::string(key);copy.replace(child,9,"/gizmo[");copy.replace(mark,9,":gizmo:");return dungeon_gizmo_identity::saved(copy);
}
inline bool valid(std::string_view key){return saved(key)||(key.starts_with("action:")&&dungeon_gizmo_identity::uuid(key.substr(7)));}
}
