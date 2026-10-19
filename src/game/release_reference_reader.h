#pragma once
#include "vendor/nlohmann/json.hpp"
#include "character_definition_identity.h"
#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::release_references {
struct Type {
    uintptr_t object=0,parent=0;uint64_t uid=0;int32_t kind=0,id=0;
    std::array<uint8_t,3> flags{};std::string name,definition;
    bool operator==(const Type&)const=default;
};
struct Types {
    uintptr_t manager=0;std::array<std::array<uintptr_t,3>,5> arrays{};
    std::vector<Type> rows;
    bool operator==(const Types&)const=default;
};
inline std::string type_key(int32_t kind,int32_t id){return nlohmann::json::array({"character",kind,id}).dump();}
inline std::string object_key(const char* family,const std::string& key){return nlohmann::json::array({family,key}).dump();}
template<class Read,class Text>bool base_types(Read& read,Text& text,uintptr_t image,uintptr_t native_state,Types& out,std::string& e){
    out={};e.clear();
    if(!read(image,0x13df7a0,out.manager,e))return false;
    character_definition_identity::Census census;
    if(!character_definition_identity::read(read,image,native_state,out.manager,census,e))return false;
    for(size_t kind=0;kind<5;++kind){const auto& span=census.arrays[kind];out.arrays[kind]={span.data,uintptr_t(span.count),uintptr_t(span.capacity)};}
    for(const auto& item:census.definitions){Type row;row.object=item.object;row.parent=item.parent;row.uid=item.uid;row.kind=item.kind;row.id=item.id;
        row.flags={uint8_t(item.deleted),uint8_t(item.edited),uint8_t(item.released)};
        if(!text(row.object+0xb0,row.name,e)||!text(row.object+0x248,row.definition,e))return false;out.rows.push_back(std::move(row));}
    e.clear();return true;
}
inline bool add_base_keys(const Types& before,const Types& after,std::map<uintptr_t,std::string>& keys,std::string& e){
    e.clear();
    if(before!=after){e="Release CharacterType full ownership/type/name witness changed during double read";return false;}
    for(const auto& row:after.rows){
        // Destroyed global slots remain census tombstones, never live links.
        if(row.flags[0])continue;
        if(!keys.emplace(row.object,type_key(row.kind,row.id)).second){e="Release CharacterType reference aliases another canonical owner";return false;}
    }
    e.clear();return true;
}
}
