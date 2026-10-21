#include "state.h"
#include "../scenery_scene_key.h"
#include "../monster_target_key.h"
#include "../snapshot_zone_key.h"
#include "../world_zone_key.h"
#include "mt2mp/core/hash.h"
#include <set>
#include <algorithm>
namespace mt2mp::game::scene_clone {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void exact(const nlohmann::json& j,std::initializer_list<const char*> keys){if(!j.is_object()||j.size()!=keys.size())throw std::runtime_error("Scene clone exact object fields differ");for(const auto* key:keys)if(!j.contains(key))throw std::runtime_error("Scene clone required field absent");}
uint32_t number(const nlohmann::json& j,uint32_t max=4096){if(!j.is_number_integer()||(!j.is_number_unsigned()&&j.get<int64_t>()<0)||j.get<uint64_t>()>max)throw std::runtime_error("Scene clone integer outside bounded domain");return j.get<uint32_t>();}
std::string text(const nlohmann::json& j){if(!j.is_string())throw std::runtime_error("Scene clone string required");auto out=j.get<std::string>();if(out.size()>4096||out.find('\0')!=std::string::npos)throw std::runtime_error("Scene clone text outside bounded identity");return out;}
bool assignment_key(const Assignment& a){if(a.family==Family::Scenery)return scenery_scene_key::valid(a.key);if(a.family==Family::Building)return dungeon_scene_key::building_key(a.key);if(zone_type(a.family)>=0)return world_zone_key::valid(a.key);if(a.family==Family::Gizmo)return dungeon_gizmo_identity::key(a.key);if(a.family==Family::DirectMonster)return monster_target_key::valid(a.key);return snapshot_zone_key::valid(a.key)||(a.key.starts_with("action:")&&monster_target_key::uuid(std::string_view(a.key).substr(7)));}
}
std::string birth_key(const std::string& operation,Family family,uint32_t ordinal){
    if(!dungeon_gizmo_identity::uuid(operation)||uint8_t(family)>7||ordinal>=12288)throw std::runtime_error("Scene clone birth provenance invalid");
    if(family==Family::Gizmo)return dungeon_gizmo_identity::birth(operation,ordinal);
    const auto input="mt2mp-scene-clone-v1/"+operation+"/"+std::to_string(uint8_t(family))+"/"+std::to_string(ordinal);
    auto hash=mt2mp::sha256(std::vector<unsigned char>(input.begin(),input.end()));hash.resize(32);hash[12]='8';
    const unsigned original=hash[16]<='9'?unsigned(hash[16]-'0'):unsigned(hash[16]-'a'+10);hash[16]="89ab"[original&3];
    return "action:"+hash.substr(0,8)+"-"+hash.substr(8,4)+"-"+hash.substr(12,4)+"-"+hash.substr(16,4)+"-"+hash.substr(20,12);
}
bool valid(const Mapping& value,std::string& e){
    if(!dungeon_gizmo_identity::uuid(value.operation)||value.scenes.size()>4096||value.members.size()>12288||value.retired.size()>12288)return fail(e,"Scene clone mapping provenance/extent invalid");
    std::set<std::string> scenes,keys;size_t index=0;
    const auto provenance=[](const Assignment& row){return assignment_key(row)&&(row.origin.empty()?row.ordinal==0:dungeon_gizmo_identity::uuid(row.origin)&&row.ordinal<12288&&row.key==birth_key(row.origin,row.family,row.ordinal));};
    for(const auto& scene:value.scenes){
        if(!dungeon_gizmo_identity::scene(scene.scene)||scene.gizmos>4096||scene.zones>4096||scene.monsters>100000||scene.other_zones[0]>4096||scene.other_zones[1]>4096||scene.other_zones[2]>4096||scene.buildings>4096||scene.scenery>4096||!scenes.insert(scene.scene).second)return fail(e,"Scene clone mapping scene/count duplicated or invalid");
        const uint32_t counts[]={scene.gizmos,scene.zones,scene.monsters,scene.other_zones[0],scene.other_zones[1],scene.other_zones[2],scene.buildings,scene.scenery};
        for(uint8_t family=0;family<8;++family)for(uint32_t child=0;child<counts[family];++child){
            if(index>=value.members.size())return fail(e,"Scene clone complete current mapping omitted native member");
            const auto& row=value.members[index++];
            if(row.scene!=scene.scene||uint8_t(row.family)!=family||row.child!=child||!provenance(row)||!keys.insert(row.key).second)return fail(e,"Scene clone current mapping order/key/provenance differs");
        }
    }
    if(index!=value.members.size())return fail(e,"Scene clone current mapping contains extra member");
    std::set<std::tuple<std::string,Family,uint32_t>> slots;
    for(const auto& row:value.retired)if(uint8_t(row.family)>7||!dungeon_gizmo_identity::scene(row.scene)||scenes.contains(row.scene)||!provenance(row)||row.child>=100000||!keys.insert(row.key).second||!slots.emplace(row.scene,row.family,row.child).second)return fail(e,"Scene clone retirement mapping invalid or overlaps current scene");
    e.clear();return true;
}
nlohmann::json encode(const Mapping& value){nlohmann::json scenes=nlohmann::json::array(),members=nlohmann::json::array(),retired=nlohmann::json::array();
    const bool scenery=std::any_of(value.scenes.begin(),value.scenes.end(),[](const auto& scene){return scene.scenery!=0;})||
        std::any_of(value.retired.begin(),value.retired.end(),[](const auto& row){return row.family==Family::Scenery;});
    for(const auto& s:value.scenes){auto row=nlohmann::json::array({s.scene,s.gizmos,s.zones,s.monsters,s.other_zones[0],s.other_zones[1],s.other_zones[2],s.buildings});if(scenery)row.push_back(s.scenery);scenes.push_back(std::move(row));}
    const auto rows=[](const auto& input,auto& out){for(const auto& r:input)out.push_back({r.scene,uint8_t(r.family),r.child,r.ordinal,r.key,r.origin});};rows(value.members,members);rows(value.retired,retired);
    return {{"schema",scenery?4:3},{"operation",value.operation},{"scenes",scenes},{"members",members},{"retired",retired}};
}
bool decode(const nlohmann::json& j,Mapping& out,std::string& e){out={};try{exact(j,{"schema","operation","scenes","members","retired"});const auto schema=number(j.at("schema"),4);if(schema!=2&&schema!=3&&schema!=4)throw std::runtime_error("Scene clone mapping schema differs");Mapping value;value.operation=text(j.at("operation"));
    const auto& scenes=j.at("scenes");if(!scenes.is_array()||scenes.size()>4096)throw std::runtime_error("Scene clone bounded scene array required");for(const auto& s:scenes){if(!s.is_array()||s.size()!=(schema==2?7u:schema==3?8u:9u))throw std::runtime_error("Scene clone scene tuple differs");value.scenes.push_back({text(s[0]),number(s[1]),number(s[2]),number(s[3],100000),{number(s[4]),number(s[5]),number(s[6])},schema==2?0u:number(s[7]),schema<4?0u:number(s[8])});}
    const auto rows=[&](const auto& input,auto& values){if(!input.is_array()||input.size()>12288)throw std::runtime_error("Scene clone bounded mapping array required");for(const auto& r:input){if(!r.is_array()||r.size()!=6)throw std::runtime_error("Scene clone member tuple differs");values.push_back(Assignment{text(r[0]),text(r[4]),Family(number(r[1],schema==2?5u:schema==3?6u:7u)),number(r[2],99999),number(r[3],12287),text(r[5])});}};
    rows(j.at("members"),value.members);rows(j.at("retired"),value.retired);if(!valid(value,e))return false;out=std::move(value);return true;
}catch(const std::exception& x){return fail(e,std::string("Scene clone mapping decode: ")+x.what());}}
}
