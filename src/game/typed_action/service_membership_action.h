#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::action_session {
enum class ServiceMembershipKind:uint8_t {AddSpawn=1,Remove=2};
struct ServiceMembershipIntent {
    ServiceMembershipKind kind=ServiceMembershipKind::AddSpawn;
    int32_t character=-1,ordinal=-1;
    bool operator==(const ServiceMembershipIntent&)const=default;
};
struct ServiceDaily {
    int32_t today=0,total=0,highest=0;
    std::vector<int32_t> history;
    bool warning=false;
    bool operator==(const ServiceDaily&)const=default;
};
struct ServiceMember {
    // index is a reserved legacy wire slot. Native Service order is the vector order.
    int32_t type=16,character=-1,index=-1;
    std::string destination,config_key;
    std::array<int32_t,2> price{};
    std::array<bool,3> flags{};
    bool prototype=false,config=false;
    std::array<ServiceDaily,2> counts;
    bool operator==(const ServiceMember&)const=default;
};
struct ServiceConfigValue {
    std::string key;std::array<int32_t,2> price{};
    bool operator==(const ServiceConfigValue&)const=default;
};
struct ServiceCharacter {
    int32_t id=-1;std::string name;bool deleted=false;
    bool operator==(const ServiceCharacter&)const=default;
};
struct ServiceMembershipState {
    std::string building_definition,spawn_type;
    std::vector<ServiceMember> rows;
    std::vector<ServiceCharacter> characters;
    bool config_present=false;
    std::vector<ServiceConfigValue> configs;
    bool operator==(const ServiceMembershipState&)const=default;
};
inline bool service_membership_intent_valid(const ServiceMembershipIntent& v){
    return v.kind==ServiceMembershipKind::AddSpawn?v.character>=-1&&v.character<10000&&v.ordinal==-1:
        v.kind==ServiceMembershipKind::Remove&&v.character==-1&&v.ordinal>=0&&v.ordinal<1024;
}
inline bool service_membership_expected(const ServiceMembershipIntent& intent,const ServiceMembershipState& before,ServiceMembershipState& after){
    if(!service_membership_intent_valid(intent))return false;after=before;
    if(intent.kind==ServiceMembershipKind::Remove){
        if(size_t(intent.ordinal)>=after.rows.size())return false;
        after.rows.erase(after.rows.begin()+intent.ordinal);return true;
    }
    if(intent.character>=0&&(size_t(intent.character)>=before.characters.size()||before.characters[size_t(intent.character)].deleted))return false;
    std::erase_if(after.rows,[&](const auto& row){return row.type==2&&row.destination.empty()&&row.character==intent.character;});
    if(after.rows.size()>=1024)return false;
    ServiceMember added;added.type=2;added.character=intent.character;added.flags[2]=true;
    added.config_key=before.building_definition+"_"+before.spawn_type;after.rows.push_back(std::move(added));return true;
}
}
