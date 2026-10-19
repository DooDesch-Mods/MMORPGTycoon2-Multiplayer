#include "state.h"
#include "../monster_target_key.h"
#include "../snapshot_zone_key.h"
#include "mt2mp/core/hash.h"
#include <set>
namespace mt2mp::game::scene_clone {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void exact(const nlohmann::json& j,std::initializer_list<const char*> keys){if(!j.is_object()||j.size()!=keys.size())throw std::runtime_error("Scene clone exact object fields differ");for(const auto* key:keys)if(!j.contains(key))throw std::runtime_error("Scene clone required field absent");}
uint32_t number(const nlohmann::json& j,uint32_t max=4096){if(!j.is_number_integer()||(!j.is_number_unsigned()&&j.get<int64_t>()<0)||j.get<uint64_t>()>max)throw std::runtime_error("Scene clone integer outside bounded domain");return j.get<uint32_t>();}
std::string text(const nlohmann::json& j){if(!j.is_string())throw std::runtime_error("Scene clone string required");auto out=j.get<std::string>();if(out.size()>4096||out.find('\0')!=std::string::npos)throw std::runtime_error("Scene clone text outside bounded identity");return out;}
bool assignment_key(const Assignment& a){if(a.family==Family::Gizmo)return dungeon_gizmo_identity::key(a.key);if(a.family==Family::DirectMonster)return monster_target_key::valid(a.key);return snapshot_zone_key::valid(a.key)||(a.key.starts_with("action:")&&monster_target_key::uuid(std::string_view(a.key).substr(7)));}
}
std::string birth_key(const std::string& operation,Family family,uint32_t ordinal){
    if(!dungeon_gizmo_identity::uuid(operation)||uint8_t(family)>2||ordinal>=12288)throw std::runtime_error("Scene clone birth provenance invalid");
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
        if(!dungeon_gizmo_identity::scene(scene.scene)||scene.gizmos>4096||scene.zones>4096||scene.monsters>100000||!scenes.insert(scene.scene).second)return fail(e,"Scene clone mapping scene/count duplicated or invalid");
        const uint32_t counts[]={scene.gizmos,scene.zones,scene.monsters};
        for(uint8_t family=0;family<3;++family)for(uint32_t child=0;child<counts[family];++child){
            if(index>=value.members.size())return fail(e,"Scene clone complete current mapping omitted native member");
            const auto& row=value.members[index++];
            if(row.scene!=scene.scene||uint8_t(row.family)!=family||row.child!=child||!provenance(row)||!keys.insert(row.key).second)return fail(e,"Scene clone current mapping order/key/provenance differs");
        }
    }
    if(index!=value.members.size())return fail(e,"Scene clone current mapping contains extra member");
    std::set<std::tuple<std::string,Family,uint32_t>> slots;
    for(const auto& row:value.retired)if(uint8_t(row.family)>2||!dungeon_gizmo_identity::scene(row.scene)||scenes.contains(row.scene)||!provenance(row)||row.child>=100000||!keys.insert(row.key).second||!slots.emplace(row.scene,row.family,row.child).second)return fail(e,"Scene clone retirement mapping invalid or overlaps current scene");
    e.clear();return true;
}
nlohmann::json encode(const Mapping& value){nlohmann::json scenes=nlohmann::json::array(),members=nlohmann::json::array(),retired=nlohmann::json::array();
    for(const auto& s:value.scenes)scenes.push_back({s.scene,s.gizmos,s.zones,s.monsters});
    const auto rows=[](const auto& input,auto& out){for(const auto& r:input)out.push_back({r.scene,uint8_t(r.family),r.child,r.ordinal,r.key,r.origin});};rows(value.members,members);rows(value.retired,retired);
    return {{"schema",1},{"operation",value.operation},{"scenes",scenes},{"members",members},{"retired",retired}};
}
bool decode(const nlohmann::json& j,Mapping& out,std::string& e){out={};try{exact(j,{"schema","operation","scenes","members","retired"});if(number(j.at("schema"),1)!=1)throw std::runtime_error("Scene clone mapping schema differs");Mapping value;value.operation=text(j.at("operation"));
    const auto& scenes=j.at("scenes");if(!scenes.is_array()||scenes.size()>4096)throw std::runtime_error("Scene clone bounded scene array required");for(const auto& s:scenes){if(!s.is_array()||s.size()!=4)throw std::runtime_error("Scene clone scene tuple differs");value.scenes.push_back({text(s[0]),number(s[1]),number(s[2]),number(s[3],100000)});}
    const auto rows=[](const auto& input,auto& values){if(!input.is_array()||input.size()>12288)throw std::runtime_error("Scene clone bounded mapping array required");for(const auto& r:input){if(!r.is_array()||r.size()!=6)throw std::runtime_error("Scene clone member tuple differs");values.push_back(Assignment{text(r[0]),text(r[4]),Family(number(r[1],2)),number(r[2],99999),number(r[3],12287),text(r[5])});}};
    rows(j.at("members"),value.members);rows(j.at("retired"),value.retired);if(!valid(value,e))return false;out=std::move(value);return true;
}catch(const std::exception& x){return fail(e,std::string("Scene clone mapping decode: ")+x.what());}}
}
