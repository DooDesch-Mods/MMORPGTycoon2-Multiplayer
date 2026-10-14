#include "action_session.h"
#include "../snapshot_building_key.h"
#include "../snapshot_zone_key.h"
#include "../snapshot_npc_key.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>

namespace mt2mp::action_session {
namespace {
using json=nlohmann::json;
template<class Function>bool callback(Function fn,std::string& cause) {
    try{const bool accepted=fn();if(!accepted&&cause.empty())cause="callback rejected without original cause";return accepted;}
    catch(const std::exception& e){cause=std::string("callback exception: ")+e.what();return false;}
    catch(...){cause="callback nonstandard exception";return false;}
}
bool uuid(const std::string& s) {
    static const std::regex pattern("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return std::regex_match(s,pattern);
}
bool key(const std::string& s) {
    if(s.starts_with("action:"))return uuid(s.substr(7));
    return game::snapshot_building_key::valid(s);
}
void exact(const json& j,std::initializer_list<const char*> keys) {
    if(!j.is_object()||j.size()!=keys.size())throw std::runtime_error("schema requires exact object fields");
    for(const auto* k:keys)if(!j.contains(k))throw std::runtime_error(std::string("missing field: ")+k);
}
uint64_t u64(const json& j) {
    if(!j.is_number_unsigned())throw std::runtime_error("unsigned integer required");
    return j.get<uint64_t>();
}
int32_t i32(const json& j) {
    if(!j.is_number_integer())throw std::runtime_error("signed int32 domain required");
    if(j.is_number_unsigned()) {
        if(j.get<uint64_t>()>uint64_t(INT32_MAX))throw std::runtime_error("int32 positive overflow");
        return int32_t(j.get<uint64_t>());
    }
    const auto n=j.get<int64_t>();
    if(n<INT32_MIN||n>INT32_MAX)throw std::runtime_error("int32 overflow");
    return int32_t(n);
}
json ledger_json(const Ledger& v) {
    return {{"cash",v.cash},{"flat_day",v.flat_day},{"record_present",v.record_present},{"record_day",v.record_day},
        {"record_cash",v.record_cash},{"record_income",v.record_income},{"record_expense",v.record_expense},
        {"line_counts",v.line_counts},{"line_income",v.line_income}};
}
Ledger ledger_read(const json& j) {
    exact(j,{"cash","flat_day","record_present","record_day","record_cash","record_income","record_expense","line_counts","line_income"});
    Ledger v;v.cash=i32(j.at("cash"));v.flat_day=i32(j.at("flat_day"));v.record_present=i32(j.at("record_present"));
    v.record_day=i32(j.at("record_day"));v.record_cash=i32(j.at("record_cash"));v.record_income=i32(j.at("record_income"));v.record_expense=i32(j.at("record_expense"));
    if(v.record_present<0||v.record_present>1)throw std::runtime_error("ledger record_present requires 0 or 1");
    for(const auto* name:{"line_counts","line_income"}) {
        const auto& a=j.at(name);if(!a.is_array()||a.size()!=20)throw std::runtime_error("ledger array length requires 20");
        auto& target=std::string(name)=="line_counts"?v.line_counts:v.line_income;
        for(size_t n=0;n<20;++n)target[n]=i32(a[n]);
    }
    return v;
}
bool valid_building(const BuildingState& v) {
    if(v.type.size()>256||v.variant.size()>256||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return false;
    for(float value:v.rotation)if(!std::isfinite(value))return false;
    for(float value:v.scale)if(!std::isfinite(value))return false;
    if(v.type.empty()||v.variant.empty())return v==BuildingState{};
    return true;
}
bool region_text(const std::string& s) {
    return !s.empty()&&s.size()<=256&&s.find('\0')==std::string::npos;
}
bool region_name(const std::string& s) {
    if(!region_text(s))return false;
    for(size_t i=0;i<s.size();) {
        const auto a=uint8_t(s[i]);size_t n=0;uint32_t cp=0;
        if(a<0x80){n=1;cp=a;}
        else if(a>=0xc2&&a<=0xdf){n=2;cp=a&0x1f;}
        else if(a>=0xe0&&a<=0xef){n=3;cp=a&0x0f;}
        else if(a>=0xf0&&a<=0xf4){n=4;cp=a&0x07;}
        else return false;
        if(i+n>s.size())return false;
        for(size_t j=1;j<n;++j){const auto b=uint8_t(s[i+j]);if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&0x3f);}
        if((n==2&&cp<0x80)||(n==3&&cp<0x800)||(n==4&&cp<0x10000)||
           (cp>=0xd800&&cp<=0xdfff)||cp>0x10ffff)return false;
        i+=n;
    }
    return true;
}
bool digest(const std::string& s) {
    return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool valid_region(const RegionState& v) {
    if(v==RegionState{})return true;
    return v.id>=0&&region_text(v.preset)&&std::all_of(v.styles.begin(),v.styles.end(),region_text)&&
        digest(v.scenery_sha256)&&digest(v.grid_sha256)&&v.scenery_count<=1000000;
}
json region_json(const RegionState& v) {
    return {{"id",v.id},{"preset",v.preset},{"styles",v.styles},{"random",v.random},
        {"scenery_count",v.scenery_count},{"scenery_sha256",v.scenery_sha256},{"grid_sha256",v.grid_sha256}};
}
RegionState region_read(const json& j) {
    exact(j,{"id","preset","styles","random","scenery_count","scenery_sha256","grid_sha256"});
    RegionState v;v.id=i32(j.at("id"));v.preset=j.at("preset").get<std::string>();
    for(const auto* k:{"styles","random"})if(!j.at(k).is_array()||j.at(k).size()!=4)throw std::runtime_error("region array requires four values");
    for(size_t i=0;i<4;++i){v.styles[i]=j.at("styles")[i].get<std::string>();v.random[i]=u64(j.at("random")[i]);}
    v.scenery_count=u64(j.at("scenery_count"));v.scenery_sha256=j.at("scenery_sha256").get<std::string>();v.grid_sha256=j.at("grid_sha256").get<std::string>();
    if(!valid_region(v))throw std::runtime_error("region state outside domain");return v;
}
float scalar(const json&);
bool valid_property_intent(const RegionPropertyIntent& v) {
    if(v.id<0)return false;
    if(v.kind==RegionPropertyKind::LevelRange)return v.name.empty()&&v.type==-1&&
        v.levels[0]>=1&&v.levels[1]>=v.levels[0]&&v.levels[1]<=INT32_MAX/10-10&&
        std::all_of(v.hit.begin(),v.hit.end(),[](float x){return std::isfinite(x)&&std::abs(x)<=1000000.0f;});
    if(v.levels!=std::array<int32_t,2>{-1,-1})return false;
    if(v.kind==RegionPropertyKind::Name)return region_name(v.name)&&v.type==-1&&v.hit==std::array<float,3>{};
    if(!v.name.empty())return false;
    if(v.kind==RegionPropertyKind::Type&&v.type!=0&&v.type!=1)return false;
    if(v.kind==RegionPropertyKind::Purchase&&v.type!=-1)return false;
    if(v.kind!=RegionPropertyKind::Type&&v.kind!=RegionPropertyKind::Purchase)return false;
    return std::all_of(v.hit.begin(),v.hit.end(),[](float x){return std::isfinite(x)&&std::abs(x)<=1000000.0f;});
}
bool valid_level_state(const RegionLevelState& v) {
    return digest(v.roster_sha256)&&digest(v.levels_sha256)&&digest(v.expected_sha256)&&
        std::all_of(v.counts.begin(),v.counts.end(),[](uint64_t n){return n<=100000;});
}
bool valid_property_state(const RegionPropertyState& v) {
    return v==RegionPropertyState{}||
        (v.content!=RegionState{}&&valid_region(v.content)&&region_name(v.name)&&v.type>=0&&v.type<=16&&
         v.status>=0&&v.status<=100000&&v.level>=0&&v.max_level>=0&&
         v.building_count<=1000000&&v.path_count<=1000000&&v.path_node_count<=1000000&&
         v.network_node_count<=1000000&&v.network_pipe_count<=1000000&&(!v.actors||valid_level_state(*v.actors)));
}
json property_intent_json(const RegionPropertyIntent& v) {
    json result={{"kind",uint8_t(v.kind)},{"id",v.id},{"name",v.name},{"type",v.type},{"hit",v.hit}};
    if(v.kind==RegionPropertyKind::LevelRange)result["levels"]=v.levels;return result;
}
RegionPropertyIntent property_intent_read(const json& j) {
    if(j.contains("levels"))exact(j,{"kind","id","name","type","hit","levels"});
    else exact(j,{"kind","id","name","type","hit"});
    RegionPropertyIntent v;const auto kind=u64(j.at("kind"));
    if(kind<1||kind>4)throw std::runtime_error("region property kind outside domain");
    v.kind=RegionPropertyKind(kind);v.id=i32(j.at("id"));v.name=j.at("name").get<std::string>();v.type=i32(j.at("type"));
    const auto& hit=j.at("hit");if(!hit.is_array()||hit.size()!=3)throw std::runtime_error("region hit requires three coordinates");
    for(size_t i=0;i<3;++i)v.hit[i]=scalar(hit[i]);
    if(v.kind==RegionPropertyKind::LevelRange){const auto& a=j.at("levels");
        if(!a.is_array()||a.size()!=2)throw std::runtime_error("region levels requires two integers");
        for(size_t i=0;i<2;++i)v.levels[i]=i32(a[i]);}
    else if(j.contains("levels"))throw std::runtime_error("region levels on unrelated property");
    if(!valid_property_intent(v))throw std::runtime_error("region property intent outside domain");return v;
}
json property_state_json(const RegionPropertyState& v) {
    json result={{"content",region_json(v.content)},{"type_random",v.type_random},{"name",v.name},{"type",v.type},{"status",v.status},
        {"level",v.level},{"max_level",v.max_level},{"owned",v.owned},
        {"building_count",v.building_count},
        {"path_count",v.path_count},{"path_node_count",v.path_node_count},
        {"network_node_count",v.network_node_count},{"network_pipe_count",v.network_pipe_count}};
    if(v.actors){const auto& a=*v.actors;result["actors"]={{"counts",a.counts},{"roster_sha256",a.roster_sha256},
        {"levels_sha256",a.levels_sha256},{"expected_sha256",a.expected_sha256},{"global_random",a.global_random}};}return result;
}
RegionPropertyState property_state_read(const json& j) {
    auto fields=j;fields.erase("actors");
    exact(fields,{"content","type_random","name","type","status","level","max_level","owned",
             "building_count","path_count","path_node_count","network_node_count","network_pipe_count"});
    RegionPropertyState v;v.content=region_read(j.at("content"));v.name=j.at("name").get<std::string>();
    const auto& random=j.at("type_random");if(!random.is_array()||random.size()!=4)throw std::runtime_error("region type RNG requires four words");
    for(size_t i=0;i<4;++i)v.type_random[i]=u64(random[i]);
    v.type=i32(j.at("type"));v.status=i32(j.at("status"));v.level=i32(j.at("level"));
    v.max_level=i32(j.at("max_level"));v.owned=j.at("owned").get<bool>();
    v.building_count=u64(j.at("building_count"));
    v.path_count=u64(j.at("path_count"));v.path_node_count=u64(j.at("path_node_count"));
    v.network_node_count=u64(j.at("network_node_count"));v.network_pipe_count=u64(j.at("network_pipe_count"));
    if(j.contains("actors")){const auto& a=j.at("actors");exact(a,{"counts","roster_sha256","levels_sha256","expected_sha256","global_random"});
        RegionLevelState value;const auto& counts=a.at("counts");const auto& rng=a.at("global_random");
        if(!counts.is_array()||counts.size()!=3||!rng.is_array()||rng.size()!=4)throw std::runtime_error("region actor arrays have wrong size");
        for(size_t i=0;i<3;++i)value.counts[i]=u64(counts[i]);for(size_t i=0;i<4;++i)value.global_random[i]=u64(rng[i]);
        value.roster_sha256=a.at("roster_sha256").get<std::string>();value.levels_sha256=a.at("levels_sha256").get<std::string>();
        value.expected_sha256=a.at("expected_sha256").get<std::string>();v.actors=value;}
    if(!valid_property_state(v))throw std::runtime_error("region property state outside domain");return v;
}
float scalar(const json&);
#include "terrain_codec.inc"
#include "path_codec.inc"
#include "network_codec.inc"
#include "network_pipe_codec.inc"
#include "npc_codec.inc"
bool valid_npc_target(const Request& r) {
    if(r.target_action_uuid.empty())return game::snapshot_npc_key::valid(r.canonical_record_key);
    return uuid(r.target_action_uuid)&&r.action_uuid!=r.target_action_uuid&&
        r.canonical_record_key=="action:"+r.target_action_uuid;
}
bool valid_request(const Request& r) {
    if(r.action==Action::MoveNPC)return uuid(r.action_uuid)&&valid_npc_target(r)&&
        valid_npc_transform(r.building)&&r.npc==NpcIntent{}&&r.region==RegionIntent{}&&
        r.region_property==RegionPropertyIntent{}&&r.price==PriceIntent{}&&r.terrain==TerrainIntent{}&&
        r.path==PathIntent{}&&r.path_delete==PathDeleteIntent{}&&r.network==NetworkIntent{}&&
        r.network_pipe==NetworkPipeIntent{};
    if(r.action==Action::RemoveNPC)return uuid(r.action_uuid)&&valid_npc_target(r)&&
        r.npc==NpcIntent{}&&r.building==BuildingState{}&&r.region==RegionIntent{}&&
        r.region_property==RegionPropertyIntent{}&&r.price==PriceIntent{}&&r.terrain==TerrainIntent{}&&
        r.path==PathIntent{}&&r.path_delete==PathDeleteIntent{}&&r.network==NetworkIntent{}&&
        r.network_pipe==NetworkPipeIntent{};
    if(r.action==Action::PlaceNPC)return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&
        r.canonical_record_key=="action:"+r.action_uuid&&valid_npc_intent(r.npc)&&
        r.building==BuildingState{}&&r.region==RegionIntent{}&&r.region_property==RegionPropertyIntent{}&&
        r.price==PriceIntent{}&&r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&
        r.path_delete==PathDeleteIntent{}&&r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{};
    if(r.npc!=NpcIntent{})return false;
    if(r.action==Action::SetPrices)return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&
        r.canonical_record_key=="business:prices"&&r.price.fields>=1&&r.price.fields<=3&&
        ((r.price.fields&1)||r.price.purchase==0)&&((r.price.fields&2)||r.price.subscription==0)&&
        r.building==BuildingState{}&&r.region==RegionIntent{}&&r.region_property==RegionPropertyIntent{}&&
        r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&r.path_delete==PathDeleteIntent{}&&
        r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{};
    if(r.price!=PriceIntent{})return false;
    if(r.action==Action::RemovePath)return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&
        r.canonical_record_key=="action:"+r.action_uuid&&valid_path_delete(r.path_delete)&&
        r.building==BuildingState{}&&r.region==RegionIntent{}&&r.region_property==RegionPropertyIntent{}&&
        r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{};
    if(r.path_delete!=PathDeleteIntent{})return false;
    if(r.action!=Action::EditRegionProperty&&r.region_property!=RegionPropertyIntent{})return false;
    if(r.action==Action::EditRegionProperty)
        return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&r.canonical_record_key=="region:"+std::to_string(r.region_property.id)&&
            valid_property_intent(r.region_property)&&r.building==BuildingState{}&&r.region==RegionIntent{}&&
            r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{};
    if(r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)
        return uuid(r.action_uuid)&&uuid(r.target_action_uuid)&&r.action_uuid!=r.target_action_uuid&&
            r.canonical_record_key=="action:"+r.target_action_uuid&&r.building==BuildingState{}&&
            r.region==RegionIntent{}&&r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&
            r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{};
    if(!r.target_action_uuid.empty())return false;
    if(r.action==Action::PlaceNetworkPipe)return uuid(r.action_uuid)&&r.building==BuildingState{}&&r.region==RegionIntent{}&&r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&r.network==NetworkIntent{}&&valid_network_pipe(r.network_pipe)&&r.canonical_record_key=="action:"+r.action_uuid;
    if(r.network_pipe!=NetworkPipeIntent{})return false;
    if(r.action==Action::PlaceNetworkNode)return uuid(r.action_uuid)&&r.building==BuildingState{}&&r.region==RegionIntent{}&&r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&valid_network(r.network)&&r.canonical_record_key=="action:"+r.action_uuid;
    if(r.network!=NetworkIntent{})return false;
    if(r.action==Action::PlacePath)return uuid(r.action_uuid)&&r.building==BuildingState{}&&r.region==RegionIntent{}&&r.terrain==TerrainIntent{}&&valid_path(r.path)&&r.canonical_record_key=="action:"+r.action_uuid;
    if(r.path!=PathIntent{})return false;
    if(r.action==Action::TerrainStroke)return uuid(r.action_uuid)&&r.building==BuildingState{}&&r.region==RegionIntent{}&&valid_terrain(r.terrain)&&r.canonical_record_key=="region:"+std::to_string(r.terrain.id);
    if(r.terrain!=TerrainIntent{})return false;
    if(r.action==Action::SetRegionStyles)return uuid(r.action_uuid)&&r.building==BuildingState{}&&r.region.id>=0&&r.region.preset.empty()&&
        std::all_of(r.region.styles.begin(),r.region.styles.end(),region_text)&&r.canonical_record_key=="region:"+std::to_string(r.region.id);
    if(r.action==Action::SetRegionPreset)return r.region.styles==std::array<std::string,4>{}&&uuid(r.action_uuid)&&r.building==BuildingState{}&&r.region.id>=0&&
        region_text(r.region.preset)&&r.canonical_record_key=="region:"+std::to_string(r.region.id);
    if(r.region!=RegionIntent{})return false;
    if(r.action<Action::RemoveSnapshotBuilding||r.action>Action::MoveBuilding||!uuid(r.action_uuid)||!key(r.canonical_record_key))return false;
    if(r.action==Action::RemoveSnapshotBuilding)return r.building==BuildingState{};
    if(!r.building.alive||!valid_building(r.building))return false;
    return r.action!=Action::PlaceBuilding||r.canonical_record_key=="action:"+r.action_uuid;
}
bool object_effect(const Request& r,const Evidence& e) {
    if(r.action==Action::UndoBuilding||r.action==Action::RedoBuilding) {
        auto other=e.object_before;other.alive=!other.alive;
        return e.object_after==other&&e.object_before.type==e.object_after.type&&
            e.object_before.variant==e.object_after.variant&&
            (r.action==Action::UndoBuilding?e.object_before.alive:!e.object_before.alive);
    }
    if(r.action==Action::RemoveSnapshotBuilding) {
        if(e.object_before==BuildingState{}&&e.object_after==BuildingState{})return !r.canonical_record_key.starts_with("action:");
        auto after=e.object_before;after.alive=false;
        return e.object_before.alive&&e.object_after==after;
    }
    if(!e.object_after.alive||e.object_after.type!=r.building.type||e.object_after.variant!=r.building.variant)return false;
    if(r.action==Action::PlaceBuilding)return e.object_before==BuildingState{};
    return e.object_before.alive&&e.object_before.type==e.object_after.type&&e.object_before.variant==e.object_after.variant;
}
bool execution(const Request& r,const Evidence& e) {
    if(r.action==Action::MoveNPC)return npc_move_effect(r,e);
    if(r.action==Action::RemoveNPC)return npc_remove_effect(e);
    if(r.action==Action::PlaceNPC)return npc_effect(r,e);
    if(r.action==Action::SetPrices) {
        const auto expected=PriceState{(r.price.fields&1)?r.price.purchase:e.price_before.purchase,
                                       (r.price.fields&2)?r.price.subscription:e.price_before.subscription};
        return e.price_after==expected&&e.price_after!=e.price_before&&e.before==e.after&&
            !e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&
            e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    }
    if(r.action==Action::EditRegionProperty) {
        const auto& before=e.property_before;const auto& after=e.property_after;
        if(!valid_property_state(before)||!valid_property_state(after)||before==RegionPropertyState{}||after==RegionPropertyState{}||
           before.content.id!=r.region_property.id||after.content.id!=r.region_property.id||e.target_retired||e.selection_confirmed||
           e.object_before!=BuildingState{}||e.object_after!=BuildingState{}||!digest(e.region_retired_sha256))return false;
        if(r.region_property.kind!=RegionPropertyKind::LevelRange&&(before.actors||after.actors))return false;
        if(r.region_property.kind==RegionPropertyKind::LevelRange){
            if(!before.actors||!after.actors||!before.owned||!after.owned||
               (before.level==r.region_property.levels[0]&&before.max_level==r.region_property.levels[1]))return false;
            auto expected=before;expected.level=r.region_property.levels[0];expected.max_level=r.region_property.levels[1];
            expected.content.random=after.content.random;expected.type_random=after.type_random;expected.actors=after.actors;
            return after==expected&&before.actors->counts==after.actors->counts&&
                before.actors->roster_sha256==after.actors->roster_sha256&&
                before.actors->expected_sha256==after.actors->levels_sha256&&
                after.actors->expected_sha256==after.actors->levels_sha256&&e.before==e.after&&e.undo_confirmed;
        }
        if(r.region_property.kind==RegionPropertyKind::Name) {
            auto expected=before;expected.name=r.region_property.name;
            return after==expected&&e.before==e.after&&!e.undo_confirmed;
        }
        if(r.region_property.kind==RegionPropertyKind::Purchase) {
            if(e.after.cash>e.before.cash||!e.before.record_present||e.before.flat_day!=e.after.flat_day||
               e.before.record_day!=e.after.record_day)return false;
            const int64_t cost=int64_t(e.before.cash)-e.after.cash;
            if(cost>INT32_MAX)return false;
            auto expected=e.before;auto update=[](int32_t& field,int64_t delta){const auto n=int64_t(field)+delta;
                if(n<INT32_MIN||n>INT32_MAX)return false;field=int32_t(n);return true;};
            if(!update(expected.cash,-cost)||!update(expected.record_cash,-cost)||!update(expected.record_expense,cost)||
               !update(expected.line_counts[7],1)||!update(expected.line_income[7],-cost))return false;
            return !before.owned&&after.owned&&before.type>0&&before.content==after.content&&
                before.name==after.name&&after.type==before.type&&expected==e.after&&e.undo_confirmed;
        }
        return !before.owned&&before.type!=r.region_property.type&&after.type==r.region_property.type&&
            !after.owned&&e.before==e.after&&e.undo_confirmed&&after.building_count==0&&after.content.scenery_count==0&&after.path_count==0&&
            after.path_node_count==0&&after.network_node_count==0&&after.network_pipe_count==0;
    }
    if(r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)
        return valid_building(e.object_before)&&valid_building(e.object_after)&&
            object_effect(r,e)&&e.undo_confirmed&&!e.selection_confirmed&&
            e.target_retired==(r.action==Action::UndoBuilding)&&
            e.region_before==RegionState{}&&e.region_after==RegionState{}&&
            e.terrain_before==TerrainState{}&&e.terrain_after==TerrainState{}&&
            e.path_before==PathState{}&&e.path_after==PathState{}&&
            e.network_before==NetworkState{}&&e.network_after==NetworkState{};
    if(r.action==Action::PlaceNetworkPipe){std::string error;return valid_network_state(e.network_before)&&valid_network_state(e.network_after)&&network_pipe_effect(r,e,error);}
    if(r.action==Action::PlaceNetworkNode){std::string error;return valid_network_state(e.network_before)&&valid_network_state(e.network_after)&&network_effect(r,e,error);}
    if(r.action==Action::RemovePath){std::string error;return valid_region(e.region_before)&&valid_region(e.region_after)&&valid_terrain_state(e.terrain_before)&&valid_terrain_state(e.terrain_after)&&valid_path_state(e.path_before)&&valid_path_state(e.path_after)&&path_delete_effect(r,e,error);}
    if(r.action==Action::PlacePath){std::string error;return valid_region(e.region_before)&&valid_region(e.region_after)&&valid_terrain_state(e.terrain_before)&&valid_terrain_state(e.terrain_after)&&valid_path_state(e.path_before)&&valid_path_state(e.path_after)&&path_effect(r,e,error);}
    if(r.action==Action::TerrainStroke&&r.terrain.height){std::string error;return terrain_height_effect(r,e,error);}
    if(r.action==Action::TerrainStroke)return valid_region(e.region_before)&&valid_region(e.region_after)&&
        e.region_before.id==r.terrain.id&&e.region_after.id==r.terrain.id&&
        e.region_before.preset==e.region_after.preset&&e.region_before.styles==e.region_after.styles&&
        e.before==e.after&&e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&
        !e.target_retired&&e.undo_confirmed&&!e.selection_confirmed&&digest(e.region_retired_sha256)&&
        digest(e.terrain_before.contours_sha256)&&digest(e.terrain_after.contours_sha256)&&
        valid_terrain_state(e.terrain_before)&&valid_terrain_state(e.terrain_after);
    if(e.terrain_before!=TerrainState{}||e.terrain_after!=TerrainState{})return false;
    if(r.action==Action::SetRegionStyles)return valid_region(e.region_before)&&valid_region(e.region_after)&&
        e.region_before.id==r.region.id&&e.region_after.id==r.region.id&&e.region_after.preset==e.region_before.preset&&e.region_after.styles==r.region.styles&&
        e.region_before.grid_sha256==e.region_after.grid_sha256&&e.before==e.after&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&
        !e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&digest(e.region_retired_sha256);
    if(r.action==Action::SetRegionPreset)return valid_region(e.region_before)&&valid_region(e.region_after)&&
        e.region_before.id==r.region.id&&e.region_after.id==r.region.id&&e.region_after.preset==r.region.preset&&
        e.region_before.grid_sha256==e.region_after.grid_sha256&&e.before==e.after&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&
        !e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&digest(e.region_retired_sha256);
    if(e.region_before!=RegionState{}||e.region_after!=RegionState{}||!e.region_retired_sha256.empty())return false;
    if(!valid_building(e.object_before)||!valid_building(e.object_after)||!object_effect(r,e))return false;
    return r.action==Action::RemoveSnapshotBuilding?e.target_retired&&e.undo_confirmed&&e.selection_confirmed:
        !e.target_retired&&e.undo_confirmed;
}
bool complete(const Request& r,const Evidence& e) {return execution(r,e)&&e.retirement_confirmed;}
bool extended(const Message& m) {
    return m.request.action!=Action::RemoveSnapshotBuilding||m.request.canonical_record_key.starts_with("action:")||
        m.evidence.object_before!=BuildingState{}||m.evidence.object_after!=BuildingState{};
}
json building_json(const BuildingState& v) {
    return {{"type",v.type},{"variant",v.variant},{"x",v.x},{"y",v.y},{"z",v.z},{"rotation",v.rotation},{"scale",v.scale},{"alive",v.alive}};
}
float scalar(const json& value) {
    if(!value.is_number())throw std::runtime_error("native float scalar required");
    const auto number=value.get<double>();
    if(!std::isfinite(number)||std::abs(number)>double(std::numeric_limits<float>::max()))throw std::runtime_error("native float scalar outside finite domain");
    return float(number);
}
BuildingState building_read(const json& j,bool npc_transform=false) {
    exact(j,{"type","variant","x","y","z","rotation","scale","alive"});
    BuildingState v;v.type=j.at("type").get<std::string>();v.variant=j.at("variant").get<std::string>();
    v.x=scalar(j.at("x"));v.y=scalar(j.at("y"));v.z=scalar(j.at("z"));v.alive=j.at("alive").get<bool>();
    for(const auto* name:{"rotation","scale"}) {
        const auto& a=j.at(name);const size_t count=std::string(name)=="rotation"?4:3;
        if(!a.is_array()||a.size()!=count)throw std::runtime_error("native transform array length differs");
        for(size_t i=0;i<count;++i){if(count==4)v.rotation[i]=scalar(a[i]);else v.scale[i]=scalar(a[i]);}
    }
    if(npc_transform?!valid_npc_transform(v):!valid_building(v))throw std::runtime_error("native building state invalid");
    return v;
}
bool valid_ledger(const Ledger& v) {return v.record_present>=0&&v.record_present<=1;}
bool same_action(const Message& a,const Message& b) {return a.session==b.session&&a.request==b.request;}
uint8_t bit(uint64_t p) {return uint8_t(1u<<(p-1));}
std::string identity(const Request& r) {return std::to_string(r.peer)+":"+r.action_uuid;}
bool valid_message(const Message& m,std::string& error) {
    if(!valid_ledger(m.evidence.before)||!valid_ledger(m.evidence.after)) {
        error="ledger record_present requires 0 or 1";return false;
    }
    if(m.kind<Kind::Request||m.kind>Kind::RequestRejected||!valid_request(m.request)||
       m.outcome>Outcome::Unknown||!m.session||m.request.peer<1||m.request.peer>4||!m.request.sequence||
       !uuid(m.request.snapshot_uuid)||!valid_region(m.evidence.region_before)||!valid_region(m.evidence.region_after)||!valid_property_state(m.evidence.property_before)||!valid_property_state(m.evidence.property_after)||!valid_building(m.evidence.object_before)||!valid_building(m.evidence.object_after)||
       m.cause.size()>1024||m.required_mask>15||m.ack_peer>4) {
        error="invalid typed action envelope";return false;
    }
    if((m.request.action!=Action::TerrainStroke||!m.request.terrain.height)&&(!m.evidence.terrain_before.heights_sha256.empty()||!m.evidence.terrain_after.heights_sha256.empty())){error="terrain height evidence outside height action domain";return false;}
    if((m.request.action!=Action::TerrainStroke&&m.request.action!=Action::PlacePath&&m.request.action!=Action::RemovePath&&(m.evidence.terrain_before!=TerrainState{}||m.evidence.terrain_after!=TerrainState{}))||
       !valid_terrain_state(m.evidence.terrain_before)||!valid_terrain_state(m.evidence.terrain_after)){error="terrain evidence outside action domain";return false;}
    if((m.request.action!=Action::SetRegionPreset&&m.request.action!=Action::SetRegionStyles&&m.request.action!=Action::TerrainStroke&&m.request.action!=Action::PlacePath&&m.request.action!=Action::RemovePath&&m.request.action!=Action::EditRegionProperty&&(m.evidence.region_before!=RegionState{}||m.evidence.region_after!=RegionState{}||!m.evidence.region_retired_sha256.empty()))||
       (!m.evidence.region_retired_sha256.empty()&&!digest(m.evidence.region_retired_sha256))) {
        error="region evidence outside action domain";return false;
    }
    if(m.request.action!=Action::EditRegionProperty&&(m.evidence.property_before!=RegionPropertyState{}||m.evidence.property_after!=RegionPropertyState{})){
        error="region property evidence outside action domain";return false;
    }
    if(m.request.action!=Action::SetPrices&&
       (m.evidence.price_before!=PriceState{}||m.evidence.price_after!=PriceState{})) {
        error="price evidence outside action domain";return false;
    }
    if((m.request.action!=Action::PlaceNPC&&m.request.action!=Action::RemoveNPC&&m.request.action!=Action::MoveNPC&&
        (m.evidence.npc_before!=NpcState{}||m.evidence.npc_after!=NpcState{}))||
       !valid_npc_state(m.evidence.npc_before)||!valid_npc_state(m.evidence.npc_after)){
        error="NPC evidence outside action domain";return false;
    }
    if(m.request.region_property.kind!=RegionPropertyKind::LevelRange&&(m.evidence.property_before.actors||m.evidence.property_after.actors)){error="region actor level evidence outside level range action";return false;}
    if(m.request.action==Action::EditRegionProperty&&(m.evidence.region_before!=RegionState{}||m.evidence.region_after!=RegionState{})){
        error="region property action carries separate region evidence";return false;
    }
    if((m.request.action!=Action::PlacePath&&m.request.action!=Action::RemovePath&&(m.evidence.path_before!=PathState{}||m.evidence.path_after!=PathState{}))||!valid_path_state(m.evidence.path_before)||!valid_path_state(m.evidence.path_after)){error="path evidence outside action domain";return false;}
    if((m.request.action!=Action::PlaceNetworkNode&&m.request.action!=Action::PlaceNetworkPipe&&(m.evidence.network_before!=NetworkState{}||m.evidence.network_after!=NetworkState{}))||!valid_network_state(m.evidence.network_before)||!valid_network_state(m.evidence.network_after)){error="network evidence outside action domain";return false;}
    const Evidence empty;
    if(m.kind==Kind::Request&&(m.revision||m.ack_peer||m.required_mask||m.outcome!=Outcome::Pending||m.evidence!=empty||!m.cause.empty())) {
        error="request carries result fields";return false;
    }
    if(m.kind!=Kind::Request&&((m.kind!=Kind::RequestRejected&&!m.revision)||!m.required_mask||!(m.required_mask&bit(m.request.peer)))) {
        error="result revision or immutable participant mask invalid";return false;
    }
    if((m.kind==Kind::ApplyAck)!=(m.ack_peer!=0)) {error="ACK peer field invalid";return false;}
    if(m.kind==Kind::HostResult&&(m.outcome!=Outcome::Confirmed||!complete(m.request,m.evidence))) {error="host result requires execution and retirement evidence";return false;}
    if(m.kind==Kind::ApplyAck&&m.outcome==Outcome::Confirmed&&!complete(m.request,m.evidence)) {error="confirmed ACK lacks native evidence";return false;}
    if(m.kind==Kind::ApplyAck&&m.outcome==Outcome::Pending) {error="native apply ACK cannot be pending";return false;}
    if(m.kind==Kind::Completion&&m.outcome==Outcome::Pending) {error="completion cannot be pending";return false;}
    if(m.kind==Kind::Completion&&m.outcome==Outcome::Confirmed&&!complete(m.request,m.evidence)) {error="confirmed completion lacks native evidence";return false;}
    if(m.kind==Kind::Completion&&m.outcome==Outcome::RejectedNoEffect&&m.evidence!=empty) {error="no-effect completion carries native evidence";return false;}
    if(m.outcome==Outcome::Confirmed&&!m.cause.empty()) {error="confirmed outcome carries failure cause";return false;}
    if(m.kind==Kind::RequestRejected&&(m.outcome!=Outcome::RejectedNoEffect||m.evidence!=empty)) {error="request rejection carries native effects";return false;}
    if((m.outcome==Outcome::RejectedNoEffect||m.outcome==Outcome::Unknown)&&m.cause.empty()) {error="failed outcome requires original cause";return false;}
    error.clear();return true;
}
}
bool valid_npc(const NpcIntent& value) {return valid_npc_intent(value);}
bool valid_npc_effect(const Request& request,const Evidence& evidence) {
    return (request.action==Action::PlaceNPC&&npc_effect(request,evidence))||
        (request.action==Action::RemoveNPC&&npc_remove_effect(evidence))||
        (request.action==Action::MoveNPC&&npc_move_effect(request,evidence));
}
bool encode(const Message& m,std::vector<uint8_t>& out,std::string& error) {
    out.clear();if(!valid_message(m,error))return false;
    try {
        const auto& r=m.request;const auto& e=m.evidence;const bool quests=npc_quest_schema(m);
        json j={{"schema",uint64_t(quests?19:r.action==Action::MoveNPC?18:r.action==Action::RemoveNPC?17:r.action==Action::PlaceNPC?16:r.action==Action::SetPrices?13:r.action==Action::RemovePath?12:r.action==Action::EditRegionProperty?(r.region_property.kind==RegionPropertyKind::LevelRange?15:11):r.action==Action::UndoBuilding||r.action==Action::RedoBuilding?10:r.action==Action::PlaceNetworkPipe?9:r.action==Action::PlaceNetworkNode?8:r.action==Action::PlacePath?(path_connected(r.path)?7:6):r.action==Action::SetRegionStyles?5:r.action==Action::TerrainStroke?(r.terrain.height?14:4):(r.action==Action::SetRegionPreset?3:(extended(m)?2:1)))},{"kind",uint64_t(m.kind)},{"session",m.session},{"revision",m.revision},
            {"ack_peer",m.ack_peer},{"required_mask",m.required_mask},{"outcome",uint64_t(m.outcome)},{"cause",m.cause},
            {"request",{{"action",uint64_t(r.action)},{"peer",r.peer},{"sequence",r.sequence},{"host_revision",r.host_revision},
                {"action_uuid",r.action_uuid},{"snapshot_uuid",r.snapshot_uuid},{"canonical_record_key",r.canonical_record_key}}},
            {"evidence",{{"before",ledger_json(e.before)},{"after",ledger_json(e.after)},{"target_retired",e.target_retired},
                {"undo_confirmed",e.undo_confirmed},{"selection_confirmed",e.selection_confirmed},{"retirement_confirmed",e.retirement_confirmed}}}};
        if(r.action==Action::PlaceNPC||r.action==Action::RemoveNPC||r.action==Action::MoveNPC) {
            if(r.action==Action::PlaceNPC)j["request"]["npc"]=npc_intent_json(r.npc);
            else j["request"]["target_action_uuid"]=r.target_action_uuid;
            if(r.action==Action::MoveNPC)j["request"]["building"]=building_json(r.building);
            j["evidence"]["npc_before"]=npc_state_json(e.npc_before,quests);
            j["evidence"]["npc_after"]=npc_state_json(e.npc_after,quests);
        } else if(r.action==Action::SetPrices) {
            j["request"]["price"]={{"fields",r.price.fields},{"purchase",r.price.purchase},{"subscription",r.price.subscription}};
            j["evidence"]["price_before"]={{"purchase",e.price_before.purchase},{"subscription",e.price_before.subscription}};
            j["evidence"]["price_after"]={{"purchase",e.price_after.purchase},{"subscription",e.price_after.subscription}};
        } else if(r.action==Action::EditRegionProperty) {
            j["request"]["region_property"]=property_intent_json(r.region_property);
            j["evidence"]["property_before"]=property_state_json(e.property_before);
            j["evidence"]["property_after"]=property_state_json(e.property_after);
            j["evidence"]["region_retired_sha256"]=e.region_retired_sha256;
        } else if(r.action==Action::UndoBuilding||r.action==Action::RedoBuilding) {
            j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["object_before"]=building_json(e.object_before);
            j["evidence"]["object_after"]=building_json(e.object_after);
        } else if(r.action==Action::PlaceNetworkPipe) {
            j["request"]["network_pipe"]=network_pipe_json(r.network_pipe);j["evidence"]["network_before"]=network_state_json(e.network_before);j["evidence"]["network_after"]=network_state_json(e.network_after);
        } else if(r.action==Action::PlaceNetworkNode) {
            j["request"]["network"]=network_json(r.network);j["evidence"]["network_before"]=network_state_json(e.network_before);j["evidence"]["network_after"]=network_state_json(e.network_after);
        } else if(r.action==Action::PlacePath||r.action==Action::RemovePath) {
            if(r.action==Action::RemovePath)j["request"]["path_delete"]=path_delete_json(r.path_delete);else j["request"]["path"]=path_json(r.path);
            j["evidence"]["region_before"]=region_json(e.region_before);j["evidence"]["region_after"]=region_json(e.region_after);
            j["evidence"]["terrain_before"]=terrain_state_json(e.terrain_before);j["evidence"]["terrain_after"]=terrain_state_json(e.terrain_after);
            j["evidence"]["path_before"]=path_state_json(e.path_before);j["evidence"]["path_after"]=path_state_json(e.path_after);
        } else if(r.action==Action::TerrainStroke) {
            j["request"]["terrain"]=terrain_json(r.terrain);
            j["evidence"]["region_before"]=region_json(e.region_before);j["evidence"]["region_after"]=region_json(e.region_after);j["evidence"]["region_retired_sha256"]=e.region_retired_sha256;
            j["evidence"]["terrain_before"]=terrain_state_json(e.terrain_before);j["evidence"]["terrain_after"]=terrain_state_json(e.terrain_after);
        } else if(r.action==Action::SetRegionPreset||r.action==Action::SetRegionStyles) {
            j["request"]["region"]={{"id",r.region.id}};
            if(r.action==Action::SetRegionStyles)j["request"]["region"]["styles"]=r.region.styles;
            else j["request"]["region"]["preset"]=r.region.preset;
            j["evidence"]["region_before"]=region_json(e.region_before);j["evidence"]["region_after"]=region_json(e.region_after);j["evidence"]["region_retired_sha256"]=e.region_retired_sha256;
        } else if(extended(m)) {
            j["request"]["building"]=building_json(r.building);
            j["evidence"]["object_before"]=building_json(e.object_before);j["evidence"]["object_after"]=building_json(e.object_after);
        }
        const auto bytes=j.dump();if(bytes.size()>max_payload) {error="action payload exceeds 16384 bytes";return false;}
        out.assign(bytes.begin(),bytes.end());error.clear();return true;
    }catch(const std::exception& e){error=std::string("action serialization: ")+e.what();return false;}
}
bool decode(const uint8_t* data,size_t length,Message& out,std::string& error) {
    if(!data||!length||length>max_payload) {error="action payload length requires 1..16384";return false;}
    try {
        std::vector<std::set<std::string>> fields;int deepest=0;
        const auto j=json::parse(data,data+length,[&](int depth,json::parse_event_t event,json& value) {
            if(depth>8)throw std::runtime_error("action JSON nesting depth exceeds 8");
            deepest=std::max(deepest,depth);
            if(event==json::parse_event_t::object_start)fields.emplace_back();
            if(event==json::parse_event_t::key&&!fields.back().insert(value.get<std::string>()).second)throw std::runtime_error("duplicate JSON field");
            if(event==json::parse_event_t::object_end)fields.pop_back();
            return true;
        });
        exact(j,{"schema","kind","session","revision","ack_peer","required_mask","outcome","cause","request","evidence"});
        const auto schema=u64(j.at("schema"));if(schema<1||schema>19)throw std::runtime_error("unsupported action schema");
        if(schema!=19&&deepest>7)throw std::runtime_error("action JSON nesting depth exceeds 7");
        Message m;const auto k=u64(j.at("kind")),o=u64(j.at("outcome"));
        if(k<1||k>5||o>3)throw std::runtime_error("unknown message enum");
        m.kind=Kind(k);m.outcome=Outcome(o);m.session=u64(j.at("session"));m.revision=u64(j.at("revision"));
        m.ack_peer=u64(j.at("ack_peer"));m.required_mask=u64(j.at("required_mask"));m.cause=j.at("cause").get<std::string>();
        const auto& r=j.at("request");
        if(schema==19){const auto action=u64(r.at("action"));
            if(action==15)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc"});
            else if(action==16)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
            else if(action==17)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","building"});
            else throw std::runtime_error("Quest schema action invalid");}
        else if(schema==18)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","building"});
        else if(schema==17)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
        else if(schema==16)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc"});
        else if(schema==13)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","price"});
        else if(schema==12)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","path_delete"});
        else if(schema==11||schema==15)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","region_property"});
        else if(schema==10)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
        else if(schema==1)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key"});
        else if(schema==2)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","building"});
        else if(schema==3||schema==5)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","region"});
        else if(schema==9)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","network_pipe"});
        else if(schema==8)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","network"});
        else if(schema==6||schema==7)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","path"});
        else exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","terrain"});
        const auto action=u64(r.at("action"));if(action<1||action>17||(schema==19?(action<15||action>17):((schema==18&&action!=17)||(schema!=18&&action==17)||(schema==17&&action!=16)||(schema!=17&&action==16)||(schema==16&&action!=15)||(schema!=16&&action==15)||(schema==14&&(action!=5||!r.at("terrain").contains("height")))||(schema==13&&action!=14)||(schema==12&&action!=13)||((schema==11||schema==15)&&action!=12)||(schema==10&&action!=10&&action!=11)||(schema==1&&action!=1)||(schema==2&&action>3)||(schema==3&&action!=4)||(schema==4&&action!=5)||(schema==5&&action!=6)||((schema==6||schema==7)&&action!=7)||(schema==8&&action!=8)||(schema==9&&action!=9))))throw std::runtime_error("unsupported typed action");
        m.request.action=Action(action);
        m.request.peer=u64(r.at("peer"));m.request.sequence=u64(r.at("sequence"));m.request.host_revision=u64(r.at("host_revision"));
        m.request.action_uuid=r.at("action_uuid").get<std::string>();m.request.snapshot_uuid=r.at("snapshot_uuid").get<std::string>();
        m.request.canonical_record_key=r.at("canonical_record_key").get<std::string>();
        if(schema==10||schema==17||schema==18||(schema==19&&action!=15))m.request.target_action_uuid=r.at("target_action_uuid").get<std::string>();
        const auto& e=j.at("evidence");
        if(schema==16||schema==17||schema==18||schema==19) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","npc_before","npc_after"});
            if(schema==16||(schema==19&&action==15))m.request.npc=npc_intent_read(r.at("npc"));
            if(schema==18||(schema==19&&action==17))m.request.building=building_read(r.at("building"),true);
            m.evidence.npc_before=npc_state_read(e.at("npc_before"),schema==19);
            m.evidence.npc_after=npc_state_read(e.at("npc_after"),schema==19);
        } else if(schema==13) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","price_before","price_after"});
            const auto& p=r.at("price");exact(p,{"fields","purchase","subscription"});
            const auto price_fields=u64(p.at("fields"));if(price_fields>3)throw std::runtime_error("price fields outside mask");
            m.request.price={uint8_t(price_fields),i32(p.at("purchase")),i32(p.at("subscription"))};
            const auto read_price=[](const json& p){exact(p,{"purchase","subscription"});return PriceState{i32(p.at("purchase")),i32(p.at("subscription"))};};
            m.evidence.price_before=read_price(e.at("price_before"));m.evidence.price_after=read_price(e.at("price_after"));
        } else if(schema==11||schema==15) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","property_before","property_after","region_retired_sha256"});
            m.request.region_property=property_intent_read(r.at("region_property"));
            if((schema==15)!=(m.request.region_property.kind==RegionPropertyKind::LevelRange))throw std::runtime_error("region level schema mismatch");
            m.evidence.property_before=property_state_read(e.at("property_before"));
            m.evidence.property_after=property_state_read(e.at("property_after"));
            m.evidence.region_retired_sha256=e.at("region_retired_sha256").get<std::string>();
        } else if(schema==10) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","object_before","object_after"});
            m.evidence.object_before=building_read(e.at("object_before"));
            m.evidence.object_after=building_read(e.at("object_after"));
        } else if(schema==1)exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed"});
        else if(schema==9) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","network_before","network_after"});
            m.request.network_pipe=network_pipe_read(r.at("network_pipe"));m.evidence.network_before=network_state_read(e.at("network_before"));m.evidence.network_after=network_state_read(e.at("network_after"));
        } else if(schema==8) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","network_before","network_after"});
            m.request.network=network_read(r.at("network"));m.evidence.network_before=network_state_read(e.at("network_before"));m.evidence.network_after=network_state_read(e.at("network_after"));
        } else if(schema==6||schema==7||schema==12) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","region_before","region_after","terrain_before","terrain_after","path_before","path_after"});
            if(schema==12)m.request.path_delete=path_delete_read(r.at("path_delete"));else m.request.path=path_read(r.at("path"),schema==7);m.evidence.path_before=path_state_read(e.at("path_before"));m.evidence.path_after=path_state_read(e.at("path_after"));
            m.evidence.region_before=region_read(e.at("region_before"));m.evidence.region_after=region_read(e.at("region_after"));
            m.evidence.terrain_before=terrain_state_read(e.at("terrain_before"));m.evidence.terrain_after=terrain_state_read(e.at("terrain_after"));
        } else if(schema==4||schema==14) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","region_before","region_after","region_retired_sha256","terrain_before","terrain_after"});
            m.request.terrain=terrain_read(r.at("terrain"));if((schema==14)!=m.request.terrain.height.has_value())throw std::runtime_error("terrain height schema mismatch");
            m.evidence.region_before=region_read(e.at("region_before"));m.evidence.region_after=region_read(e.at("region_after"));m.evidence.region_retired_sha256=e.at("region_retired_sha256").get<std::string>();
            m.evidence.terrain_before=terrain_state_read(e.at("terrain_before"));m.evidence.terrain_after=terrain_state_read(e.at("terrain_after"));
        } else if(schema==3||schema==5) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","region_before","region_after","region_retired_sha256"});
            const auto& region=r.at("region");m.request.region.id=i32(region.at("id"));
            if(schema==5){exact(region,{"id","styles"});const auto& styles=region.at("styles");
                if(!styles.is_array()||styles.size()!=4)throw std::runtime_error("region styles requires four values");
                for(size_t i=0;i<4;++i)m.request.region.styles[i]=styles[i].get<std::string>();
            } else {exact(region,{"id","preset"});m.request.region.preset=region.at("preset").get<std::string>();}
            m.evidence.region_before=region_read(e.at("region_before"));m.evidence.region_after=region_read(e.at("region_after"));m.evidence.region_retired_sha256=e.at("region_retired_sha256").get<std::string>();
        }
        else {exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","object_before","object_after"});
            m.request.building=building_read(r.at("building"));m.evidence.object_before=building_read(e.at("object_before"));m.evidence.object_after=building_read(e.at("object_after"));}
        m.evidence.before=ledger_read(e.at("before"));m.evidence.after=ledger_read(e.at("after"));
        m.evidence.target_retired=e.at("target_retired").get<bool>();m.evidence.undo_confirmed=e.at("undo_confirmed").get<bool>();
        m.evidence.selection_confirmed=e.at("selection_confirmed").get<bool>();m.evidence.retirement_confirmed=e.at("retirement_confirmed").get<bool>();
        if((schema==19)!=npc_quest_schema(m))throw std::runtime_error("Quest NPC schema mismatch");
        if(schema==1&&extended(m))throw std::runtime_error("shared object identity requires action schema2");
        if(!valid_message(m,error))return false;out=std::move(m);error.clear();return true;
    }catch(const std::exception& e){error=std::string("action decode: ")+e.what();return false;}
}
#include "path_effect.inc"
#include "network_effect.inc"
#include "network_pipe_effect.inc"
Controller::Controller(Session& s,Engine& e,Transport& t,Logger l,uint64_t timeout):session_(s),engine_(e),transport_(t),logger_(std::move(l)),timeout_(timeout) {}
bool Controller::invoke(const std::function<bool()>& fn,std::string& cause) {
    const bool was_locked=status_.locked;busy_=true;
    const bool accepted=callback(fn,cause);busy_=false;
    if(!was_locked&&status_.locked){cause=status_.cause;return false;}
    return accepted;
}
bool Controller::fail(const std::string& cause,std::string& out,bool lock) {
    out=cause.empty()?"action failure lacks cause":cause;status_.cause=out;++status_.failed;
    if(lock){status_.locked=true;status_.outcome=Outcome::Unknown;}
    if(last_logged_error_!=out){last_logged_error_=out;if(logger_)logger_("error","ActionSession",out);}
    else ++status_.suppressed_errors;
    return false;
}
bool Controller::guard(std::string& error) {
    if(!status_.started)return fail("action controller not started",error);
    if(std::this_thread::get_id()!=thread_)return fail("action controller requires bound game thread",error);
    if(busy_)return fail("action controller reentrant call",error,true);
    if(status_.locked){error="action controller locked: "+status_.cause;++status_.suppressed_errors;return false;}
    Context actual;std::string cause;
    if(!invoke([&]{return session_.current(actual,cause);},cause))return fail("session observation: "+cause,error,true);
    if(actual!=context_)return fail("immutable action session/snapshot/participant binding changed",error,true);
    if(!invoke([&]{return engine_.current(binding_,context_.snapshot_uuid,cause);},cause))return fail("native world observation: "+cause,error,true);
    return true;
}
bool Controller::start(uint64_t now,std::string& error) {
    if(status_.started||thread_!=std::thread::id{})return fail("action controller cannot restart or clear replay history",error);
    thread_=std::this_thread::get_id();now_=now;
    if(!timeout_)return fail("ACK timeout must be positive",error);
    if(!invoke([&]{return session_.current(context_,error);},error))return fail("session binding: "+error,error);
    if(!context_.session||!uuid(context_.snapshot_uuid)||(context_.role!=Role::Host&&context_.role!=Role::Client)||context_.local_peer<1||context_.local_peer>4||context_.host_peer!=1||
       context_.peer_mask>15||(context_.peer_mask&3)!=3||!(context_.peer_mask&bit(context_.local_peer))||
       ((context_.role==Role::Host)!=(context_.local_peer==context_.host_peer)))return fail("invalid connected action session binding",error);
    if(!invoke([&]{return engine_.bind(context_.snapshot_uuid,binding_,error);},error)||!binding_.native_epoch||!binding_.native_generation)return fail("actual native world binding: "+error,error);
    if(!invoke([&]{return transport_.claim([this](uint64_t peer,const uint8_t* bytes,size_t size) {
        std::string cause;status_.last_packet_accepted=receive(peer,bytes,size,now_,cause);
    },error);},error))return fail("exclusive action channel claim: "+error,error);
    status_.started=true;error.clear();if(logger_)logger_("info","start","immutable action session bound");return true;
}
bool Controller::send(uint64_t target,const Message& m,std::string& error) {
    std::vector<uint8_t> bytes;
    if(!encode(m,bytes,error))return fail(error,error,true);
    if(!invoke([&]{return transport_.send(target,bytes,error);},error))return fail("action send target="+std::to_string(target)+": "+error,error,true);
    return true;
}
void Controller::remember(const std::string& id,Cached value) {
    if(!history_.contains(id))history_order_.push_back(id);
    history_[id]=std::move(value);
    if(history_order_.size()>history_limit){history_.erase(history_order_.front());history_order_.pop_front();}
}
bool Controller::submit_building_history(Action action,const std::string& id,const std::string& target,uint64_t now,std::string& error) {
    if(action!=Action::UndoBuilding&&action!=Action::RedoBuilding)return fail("building history action invalid",error);
    Request request;request.action=action;request.action_uuid=id;request.target_action_uuid=target;
    request.canonical_record_key="action:"+target;return submit(std::move(request),now,error);
}
bool Controller::submit_remove(const std::string& id,const std::string& record,uint64_t now,std::string& error) {
    Request request;request.action_uuid=id;request.canonical_record_key=record;return submit(std::move(request),now,error);
}
bool Controller::submit_place(const std::string& id,const BuildingState& building,uint64_t now,std::string& error) {
    Request request;request.action=Action::PlaceBuilding;request.action_uuid=id;request.canonical_record_key="action:"+id;request.building=building;
    return submit(std::move(request),now,error);
}
bool Controller::submit_move(const std::string& id,const std::string& object,const BuildingState& building,uint64_t now,std::string& error) {
    Request request;request.action=Action::MoveBuilding;request.action_uuid=id;request.canonical_record_key=object;request.building=building;
    return submit(std::move(request),now,error);
}
#include "height_effect.inc"
bool terrain_receipt_fits(const Request& request,const Ledger& money,const RegionState& region_before,std::string& e) {
    Message worst;worst.kind=Kind::Completion;worst.outcome=Outcome::Unknown;
    worst.session=worst.revision=UINT64_MAX;worst.required_mask=15;worst.request=request;
    worst.request.sequence=worst.request.host_revision=UINT64_MAX;worst.cause=std::string(1024,'\x01');
    auto& evidence=worst.evidence;evidence.before=evidence.after=money;
    evidence.region_before=region_before;evidence.region_before.random.fill(UINT64_MAX);evidence.region_before.scenery_count=1000000;
    evidence.region_after=evidence.region_before;evidence.region_retired_sha256=std::string(64,'f');
    evidence.terrain_before.global_random.fill(UINT64_MAX);evidence.terrain_before.release_edits.fill(INT32_MIN);
    evidence.terrain_before.contours_sha256=evidence.terrain_before.bugs_sha256=std::string(64,'f');evidence.terrain_before.bug_count=1000000;
    if(request.terrain.height){evidence.terrain_before.heights_sha256=std::string(64,'f');evidence.terrain_before.height_cell=999999;evidence.terrain_before.height_bits=UINT32_MAX;}
    evidence.terrain_after=evidence.terrain_before;std::vector<uint8_t> encoded;
    if(!encode(worst,encoded,e)){e="terrain receipt exceeds current transport before native effect: "+e;return false;}return true;
}

bool Controller::submit_network_pipe(const std::string& id,const NetworkPipeIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::PlaceNetworkPipe;request.action_uuid=id;request.canonical_record_key="action:"+id;request.network_pipe=intent;return submit(request,now,error);
}
bool Controller::submit_network(const std::string& id,const NetworkIntent& intent,uint64_t now,std::string& e) {
    Request request;request.action=Action::PlaceNetworkNode;request.action_uuid=id;request.canonical_record_key="action:"+id;request.network=intent;return submit(std::move(request),now,e);
}
bool Controller::submit_path_delete(const std::string& id,const PathDeleteIntent& intent,uint64_t now,std::string& e) {
    Request request;request.action=Action::RemovePath;request.action_uuid=id;request.canonical_record_key="action:"+id;request.path_delete=intent;return submit(std::move(request),now,e);
}
bool Controller::submit_path(const std::string& id,const PathIntent& intent,uint64_t now,std::string& e) {
    Request request;request.action=Action::PlacePath;request.action_uuid=id;request.canonical_record_key="action:"+id;request.path=intent;return submit(std::move(request),now,e);
}
bool Controller::submit_terrain(const std::string& id,const TerrainIntent& terrain,uint64_t now,std::string& error) {
    Request request;request.action=Action::TerrainStroke;request.action_uuid=id;request.canonical_record_key="region:"+std::to_string(terrain.id);request.terrain=terrain;
    return submit(std::move(request),now,error);
}
bool Controller::submit_region(const std::string& id,const RegionIntent& region,uint64_t now,std::string& error) {
    Request request;request.action=region.preset.empty()?Action::SetRegionStyles:Action::SetRegionPreset;request.action_uuid=id;request.canonical_record_key="region:"+std::to_string(region.id);request.region=region;
    return submit(std::move(request),now,error);
}
bool Controller::submit_region_property(const std::string& id,const RegionPropertyIntent& intent,uint64_t now,std::string& error) {
    Request request;request.action=Action::EditRegionProperty;request.action_uuid=id;
    request.canonical_record_key="region:"+std::to_string(intent.id);request.region_property=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_prices(const std::string& id,const PriceIntent& intent,uint64_t now,std::string& error) {
    Request request;request.action=Action::SetPrices;request.action_uuid=id;
    request.canonical_record_key="business:prices";request.price=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_npc(const std::string& id,const NpcIntent& intent,uint64_t now,std::string& error) {
    Request request;request.action=Action::PlaceNPC;request.action_uuid=id;
    request.canonical_record_key="action:"+id;request.npc=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_remove_npc(const std::string& id,const std::string& target,uint64_t now,std::string& error) {
    Request request;request.action=Action::RemoveNPC;request.action_uuid=id;
    if(uuid(target)){request.target_action_uuid=target;request.canonical_record_key="action:"+target;}
    else request.canonical_record_key=target;
    return submit(std::move(request),now,error);
}
bool Controller::submit_move_npc(const std::string& id,const std::string& target,const BuildingState& transform,uint64_t now,std::string& error) {
    Request request;request.action=Action::MoveNPC;request.action_uuid=id;
    if(uuid(target)){request.target_action_uuid=target;request.canonical_record_key="action:"+target;}
    else request.canonical_record_key=target;
    request.building=transform;return submit(std::move(request),now,error);
}
bool Controller::submit(Request request,uint64_t now,std::string& error) {
    if(!guard(error))return false;now_=now;
    if(pending_||local_request_)return fail("one action already pending; no queue",error);
    if(!valid_request(request))return fail("typed action UUID, object key or building payload invalid",error);
    for(const auto& [other,entry]:history_)if(entry.request.request.action_uuid==request.action_uuid)return fail("action UUID already used; no resubmission",error);
    if(local_sequence_==UINT64_MAX)return fail("request sequence exhausted",error,true);
    if(now>UINT64_MAX-timeout_)return fail("ACK deadline timestamp exhausted",error,true);
    request.peer=context_.local_peer;request.sequence=++local_sequence_;request.host_revision=status_.host_revision;request.snapshot_uuid=context_.snapshot_uuid;
    Message m;m.session=context_.session;m.request=std::move(request);
    pending_=m;deadline_=now+timeout_;status_.pending=true;status_.action_uuid=m.request.action_uuid;status_.outcome=Outcome::Pending;
    if(context_.role==Role::Host)return this->request(m,context_.local_peer,error);
    local_request_=m;local_deadline_=deadline_;remember(identity(m.request),Cached{m});return send(context_.host_peer,m,error);
}
bool Controller::observe(const Request& r,const Ledger& before,Evidence& evidence,std::string& error) {
    if(!invoke([&]{return engine_.observe_action(binding_,r,evidence,error);},error))return fail("native execution observation: "+error,error,true);
    Ledger actual;
    if(!execution(r,evidence)||evidence.before!=before||!invoke([&]{return engine_.ledger(binding_,actual,error);},error)||actual!=evidence.after)
        return fail("native execution receipt/ledger differs: "+error,error,true);
    if(!invoke([&]{return engine_.retire(binding_,r,error);},error))return fail("native receipt retirement: "+error,error,true);
    Evidence retired;
    if(!invoke([&]{return engine_.observe_action(binding_,r,retired,error);},error)||!complete(r,retired)||retired.before!=evidence.before||retired.after!=evidence.after||
       retired.object_before!=evidence.object_before||retired.object_after!=evidence.object_after||
       retired.region_before!=evidence.region_before||retired.region_after!=evidence.region_after||
       retired.property_before!=evidence.property_before||retired.property_after!=evidence.property_after||
       retired.region_retired_sha256!=evidence.region_retired_sha256||retired.terrain_before!=evidence.terrain_before||retired.terrain_after!=evidence.terrain_after||
       retired.npc_before!=evidence.npc_before||retired.npc_after!=evidence.npc_after)
        return fail("native retirement acknowledgment: "+error,error,true);
    evidence=retired;return true;
}
bool Controller::request(const Message& m,uint64_t sender,std::string& error) {
    if(context_.role!=Role::Host||sender!=m.request.peer)return fail("action request role/sender mismatch",error);
    const auto id=identity(m.request);auto old=history_.find(id);
    if(old!=history_.end()) {
        if(old->second.request!=m)return reject_request(m,"conflicting duplicate action request",false,error);
        ++status_.duplicates;
        if(old->second.has_completion)return send(sender,old->second.completion,error);
        if(old->second.has_result)return send(sender,old->second.result,error);
        error.clear();return true;
    }
    for(const auto& [other,c]:history_)if(c.request.request.action_uuid==m.request.action_uuid)return reject_request(m,"action UUID already belongs to another peer",false,error);
    if(high_water_[sender]==UINT64_MAX||m.request.sequence!=high_water_[sender]+1)return reject_request(m,"request sequence gap, expired or reordered",false,error);
    if(pending_&&!same_action(*pending_,m)) {
        high_water_[sender]=m.request.sequence;
        return reject_request(m,"host action pending; no queue",true,error);
    }
    if(now_>UINT64_MAX-timeout_)return fail("ACK deadline timestamp exhausted",error,true);
    high_water_[sender]=m.request.sequence;remember(id,Cached{m});pending_=m;status_.pending=true;deadline_=now_+timeout_;
    status_.action_uuid=m.request.action_uuid;status_.required_mask=context_.peer_mask;status_.ack_mask=0;
    if(status_.host_revision==UINT64_MAX)return fail("host world revision exhausted",error,true);
    const bool revision_conflict=m.request.host_revision!=status_.host_revision;
    ++status_.host_revision;
    if(revision_conflict)return finish(Outcome::RejectedNoEffect,"request host revision conflict",error);
    Ledger before;
    if(!invoke([&]{return engine_.ledger(binding_,before,error);},error))return finish(status_.locked?Outcome::Unknown:Outcome::RejectedNoEffect,"host before ledger: "+error,error);
    if(!valid_ledger(before))return finish(Outcome::RejectedNoEffect,"host before ledger record_present requires 0 or 1",error);
    if(!invoke([&]{return engine_.capture_host_authority_before(binding_,m.request,before,status_.host_revision-1,error);},error))return finish(Outcome::RejectedNoEffect,"host authority before capture: "+error,error);
    ++status_.attempted;busy_=true;Outcome outcome;
    try {outcome=engine_.execute_action(binding_,m.request,error);}catch(const std::exception& e){outcome=Outcome::Unknown;error=e.what();}
    catch(...){outcome=Outcome::Unknown;error="nonstandard native execution exception";}
    busy_=false;
    if(status_.locked)return finish(Outcome::Unknown,status_.cause,error);
    if(outcome!=Outcome::Confirmed&&error.empty())error="native execution returned failure without original cause";
    if(outcome!=Outcome::Confirmed)return finish(outcome==Outcome::RejectedNoEffect?outcome:Outcome::Unknown,"host native action: "+error,error);
    Message result=m;result.kind=Kind::HostResult;result.revision=status_.host_revision;result.required_mask=context_.peer_mask;result.outcome=Outcome::Confirmed;
    if(!observe(m.request,before,result.evidence,error))return finish(Outcome::Unknown,error,error);
    if(!invoke([&]{return engine_.publish_host_authority_before(result,error);},error))return finish(Outcome::Unknown,"host authority before publication: "+error,error);
    auto& cached=history_.at(id);cached.result=result;cached.has_result=true;status_.ack_mask=bit(context_.local_peer);
    for(uint64_t peer=1;peer<=4;++peer)if(peer!=context_.local_peer&&(context_.peer_mask&bit(peer)))if(!send(peer,result,error))return finish(Outcome::Unknown,error,error);
    return true;
}
bool Controller::result(const Message& m,std::string& error) {
    if(context_.role!=Role::Client)return fail("host result delivered to Host",error);
    const auto id=identity(m.request);auto old=history_.find(id);
    if(old!=history_.end()&&old->second.has_result) {
        if(old->second.result!=m)return fail("conflicting duplicate host result",error,true);
        ++status_.duplicates;if(old->second.has_ack)return send(context_.host_peer,old->second.ack,error);
        return fail("duplicate result lacks retained native ACK",error,true);
    }
    if(last_result_revision_==UINT64_MAX||m.revision!=last_result_revision_+1)return fail("host result revision gap or replay",error,true);
    if(pending_&&!same_action(*pending_,m)&&pending_->kind!=Kind::Request)return fail("different host action arrived while native application pending",error,true);
    if(now_>UINT64_MAX-timeout_)return fail("ACK deadline timestamp exhausted",error,true);
    if(pending_&&m.request.peer==context_.local_peer&&!same_action(*pending_,m))return fail("host result differs from submitted action",error,true);
    if(old!=history_.end()&&!same_action(old->second.request,m))return fail("host result conflicts with cached request",error,true);
    if(authority_waiting_&&*authority_waiting_!=m)return fail("different HostResult while exact Time authority is pending",error,true);
    AuthorityReadiness ready=AuthorityReadiness::Rejected;
    if(!invoke([&]{ready=engine_.client_authority_before_ready(m,m.revision-1,error);return true;},error))return fail("client authority readiness: "+error,error,true);
    if(ready==AuthorityReadiness::Rejected)return fail("client authority before rejected: "+error,error,true);
    if(ready==AuthorityReadiness::Wait) {
        if(!authority_waiting_) {authority_waiting_=m;if(!pending_)deadline_=now_+timeout_;pending_=m;status_.pending=true;status_.action_uuid=m.request.action_uuid;status_.required_mask=context_.peer_mask;}
        if(status_.cause!=error&&logger_)logger_("info","ActionSession",error);status_.cause=error;return true;
    }
    authority_waiting_.reset();status_.cause.clear();
    Message original=m;original.kind=Kind::Request;original.revision=0;original.required_mask=0;original.outcome=Outcome::Pending;original.evidence={};
    Cached c=old==history_.end()?Cached{original}:old->second;c.result=m;c.has_result=true;remember(id,c);
    last_result_revision_=m.revision;status_.host_revision=m.revision;status_.required_mask=context_.peer_mask;
    pending_=m;status_.pending=true;status_.action_uuid=m.request.action_uuid;deadline_=now_+timeout_;
    Message ack=m;ack.kind=Kind::ApplyAck;ack.ack_peer=context_.local_peer;
    Ledger before;
    if(!invoke([&]{return engine_.apply_client_authority_before(binding_,m,m.revision-1,error);},error)){ack.outcome=Outcome::Unknown;ack.cause="client authority before apply: "+error;}
    else if(!invoke([&]{return engine_.ledger(binding_,before,error);},error)||before!=m.evidence.before){ack.outcome=Outcome::Unknown;ack.cause="client before ledger differs from authority: "+error;}
    else {
        ++status_.attempted;busy_=true;Outcome applied;
        try{applied=engine_.apply_action(binding_,m.request,m.evidence,error);}catch(const std::exception& e){applied=Outcome::Unknown;error=e.what();}
        catch(...){applied=Outcome::Unknown;error="nonstandard native apply exception";}
        busy_=false;
        if(status_.locked){applied=Outcome::Unknown;error=status_.cause;}
        if(applied!=Outcome::Confirmed&&error.empty())error="native apply returned failure without original cause";
        if(applied!=Outcome::Confirmed){ack.outcome=Outcome::Unknown;ack.cause="client native apply: "+error;}
        else {
            Evidence local;
            if(!observe(m.request,before,local,error)||local!=m.evidence){ack.outcome=Outcome::Unknown;ack.cause="client after ledger/native receipt differs from authority: "+error;}
            else ack.evidence=local;
        }
    }
    history_.at(id).ack=ack;history_.at(id).has_ack=true;
    const bool sent=send(context_.host_peer,ack,error);
    if(ack.outcome!=Outcome::Confirmed)return fail(ack.cause,error,true);
    return sent;
}
bool Controller::ack(const Message& m,uint64_t sender,std::string& error) {
    if(context_.role!=Role::Host||m.ack_peer!=sender)return fail("native ACK role/sender mismatch",error);
    const auto id=identity(m.request);auto old=history_.find(id);
    if(old==history_.end()||!old->second.has_result||!same_action(old->second.result,m)||old->second.result.revision!=m.revision)
        return fail("unexpected native apply ACK",error);
    const auto& retained=old->second.peer_acks[sender];
    if(retained) {
        if(*retained!=m)return fail("conflicting duplicate native apply ACK",error);
        ++status_.duplicates;error.clear();return true;
    }
    if(old->second.has_completion)return fail("native apply ACK arrived after completion without retained ACK",error);
    if(!pending_||!same_action(*pending_,m))return fail("native ACK has no matching pending action",error);
    old->second.peer_acks[sender]=m;
    if(m.outcome!=Outcome::Confirmed)return finish(Outcome::Unknown,"peer="+std::to_string(sender)+" native application: "+m.cause,error);
    if(m.evidence!=old->second.result.evidence)return finish(Outcome::Unknown,"peer="+std::to_string(sender)+" ledger/native receipt differs from Host",error);
    if(status_.ack_mask&bit(sender)){++status_.duplicates;error.clear();return true;}
    status_.ack_mask|=bit(sender);
    if(status_.ack_mask==status_.required_mask)return finish(Outcome::Confirmed,"",error);
    return true;
}
bool Controller::finish(Outcome outcome,const std::string& cause,std::string& error) {
    if(!pending_)return fail("completion lacks pending action",error,true);
    const auto id=identity(pending_->request);auto& c=history_.at(id);
    Message m=c.has_result?c.result:*pending_;m.kind=Kind::Completion;m.ack_peer=0;m.revision=status_.host_revision;
    if(!m.revision)m.revision=1;
    m.required_mask=context_.peer_mask;m.outcome=outcome;m.cause=cause;c.completion=m;c.has_completion=true;
    pending_.reset();status_.pending=false;status_.outcome=outcome;status_.cause=cause;
    if(outcome==Outcome::Confirmed)++status_.confirmed;
    else {++status_.failed;if(logger_)logger_("error","completion",cause);}
    if(outcome==Outcome::Unknown)status_.locked=true;
    bool sent=true;
    for(uint64_t peer=1;peer<=4;++peer)if(peer!=context_.local_peer&&(context_.peer_mask&bit(peer)))if(!send(peer,m,error))sent=false;
    if(logger_&&outcome==Outcome::Confirmed)logger_("info","completion","all required native execution and retirement ACKs received");
    return sent;
}
bool Controller::completion(const Message& m,std::string& error) {
    if(context_.role!=Role::Client)return fail("completion delivered to Host",error);
    const auto id=identity(m.request);auto old=history_.find(id);
    if(old!=history_.end()&&old->second.has_completion) {
        if(old->second.completion!=m)return fail("conflicting duplicate completion",error,true);
        ++status_.duplicates;return true;
    }
    if(old!=history_.end()&&!same_action(old->second.request,m))return fail("completion request differs",error,true);
    const bool had_result=old!=history_.end()&&old->second.has_result;
    const bool matching_pending=pending_&&same_action(*pending_,m);
    const bool matching_local=local_request_&&same_action(*local_request_,m);
    if(had_result) {
        if(!matching_pending||pending_->kind!=Kind::HostResult||m.revision!=last_result_revision_||
           m.revision!=status_.host_revision||old->second.result.revision!=m.revision||old->second.result.evidence!=m.evidence)
            return fail("completion differs from current native application flow/revision",error,true);
        if(m.outcome==Outcome::RejectedNoEffect)return fail("no-effect completion follows native application",error,true);
        if(m.outcome==Outcome::Confirmed&&(!old->second.has_ack||old->second.ack.outcome!=Outcome::Confirmed))
            return fail("success completion lacks actual native local apply ACK",error,true);
    } else {
        if(m.outcome==Outcome::Confirmed||last_result_revision_==UINT64_MAX||m.revision!=last_result_revision_+1||
           m.request.host_revision>last_result_revision_||(pending_&&pending_->kind==Kind::HostResult))
            return fail("completion without native result is stale, reordered or overlaps native application",error,true);
        if(m.request.peer==context_.local_peer&&(!matching_local||!matching_pending||old==history_.end()))
            return fail("completion has no associated local request flow",error,true);
        if(m.outcome==Outcome::RejectedNoEffect&&m.evidence!=Evidence{})
            return fail("no-effect completion carries native evidence",error,true);
    }
    if(old==history_.end()) {
        Message original=m;original.kind=Kind::Request;original.revision=0;original.required_mask=0;original.outcome=Outcome::Pending;original.evidence={};original.cause.clear();
        remember(id,Cached{original});old=history_.find(id);
    }
    old->second.completion=m;old->second.has_completion=true;
    if(matching_pending)pending_.reset();
    if(local_request_&&same_action(*local_request_,m))local_request_.reset();
    if(!pending_&&local_request_){pending_=local_request_;deadline_=local_deadline_;}
    status_.pending=pending_.has_value();status_.outcome=m.outcome;status_.cause=m.cause;
    status_.host_revision=m.revision;last_result_revision_=m.revision;
    if(m.outcome==Outcome::Confirmed){++status_.confirmed;return true;}
    if(m.outcome==Outcome::Unknown)return fail("Host completion Unknown: "+m.cause,error,true);
    ++status_.failed;if(logger_)logger_("error","completion",m.cause);return true;
}
bool Controller::reject_request(const Message& request,const std::string& cause,bool cache,std::string& error) {
    Message rejected=request;rejected.kind=Kind::RequestRejected;rejected.revision=status_.host_revision;
    rejected.required_mask=context_.peer_mask;rejected.outcome=Outcome::RejectedNoEffect;rejected.cause=cause;
    if(cache){Cached entry{request};entry.completion=rejected;entry.has_completion=true;remember(identity(request.request),entry);}
    fail(cause,error);
    return send(request.request.peer,rejected,error);
}
bool Controller::rejection(const Message& rejected,std::string& error) {
    if(context_.role!=Role::Client||rejected.request.peer!=context_.local_peer)return fail("request rejection role/peer mismatch",error);
    auto old=history_.find(identity(rejected.request));
    if(old==history_.end()||!same_action(old->second.request,rejected))return fail("request rejection does not match local request",error,true);
    if(old->second.has_completion) {
        if(old->second.completion!=rejected)return fail("conflicting duplicate request rejection",error,true);
        ++status_.duplicates;return true;
    }
    if(old->second.has_result)return fail("Host rejects request after native execution",error,true);
    old->second.completion=rejected;old->second.has_completion=true;
    if(local_request_&&same_action(*local_request_,rejected))local_request_.reset();
    if(pending_&&same_action(*pending_,rejected))pending_.reset();
    status_.pending=pending_.has_value();status_.outcome=rejected.outcome;status_.cause=rejected.cause;++status_.failed;
    if(logger_)logger_("error","request rejection",rejected.cause);
    error.clear();return true;
}
bool Controller::receive(uint64_t sender,const uint8_t* bytes,size_t size,uint64_t now,std::string& error) {
    if(!guard(error))return false;now_=now;
    if(sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&bit(sender)))return fail("untrusted transport sender",error);
    bool state_handled=false;if(!invoke([&]{return engine_.receive_authority_state(sender,bytes,size,state_handled,error);},error))return fail("authority state receive: "+error,error);
    if(state_handled)return true;
    Message m;if(!decode(bytes,size,m,error))return fail(error,error);
    if(sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&bit(sender)))return fail("untrusted transport sender",error);
    if(m.session!=context_.session||m.request.snapshot_uuid!=context_.snapshot_uuid)return fail("packet session or snapshot differs",error);
    if(m.kind!=Kind::Request&&m.required_mask!=context_.peer_mask)return fail("packet immutable participant mask differs",error,true);
    if(m.kind!=Kind::Request&&m.kind!=Kind::ApplyAck&&sender!=context_.host_peer)return fail("result/completion sender is not Host",error);
    switch(m.kind) {
    case Kind::Request:return request(m,sender,error);
    case Kind::HostResult:return result(m,error);
    case Kind::ApplyAck:return ack(m,sender,error);
    case Kind::Completion:return completion(m,error);
    case Kind::RequestRejected:return rejection(m,error);
    }
    return fail("unsupported action packet",error);
}
bool Controller::tick(uint64_t now,std::string& error) {
    if(!guard(error))return false;now_=now;
    if(authority_waiting_&&now<deadline_) {const auto waiting=*authority_waiting_;if(!result(waiting,error))return false;}
    if((pending_&&now>=deadline_)||(local_request_&&now>=local_deadline_)) {
        const auto expired_deadline=local_request_?std::min(deadline_,local_deadline_):deadline_;
        const auto cause="native apply ACK observation timeout now_ms="+std::to_string(now)+" deadline_ms="+std::to_string(expired_deadline)+
            " actual_mask="+std::to_string(status_.ack_mask)+" required_mask="+std::to_string(status_.required_mask)+"; no native retry";
        if(context_.role==Role::Host)return finish(Outcome::Unknown,cause,error);
        return fail(cause,error,true);
    }
    error.clear();return true;
}
}
