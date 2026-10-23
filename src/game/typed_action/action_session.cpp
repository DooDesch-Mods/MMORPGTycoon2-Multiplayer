#include "../monster_target_key.h"
#include "../monster_move_key.h"
#include "../tutorial_replication/start_ledger.h"
#include "../tutorial_replication/staff_action.h"
#include "action_session.h"
#include "zone_edit_codec.h"
#include "../region_history_key.h"
#include <memory>
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/reporting.h"
#include "mt2mp/core/logger.h"
#include <chrono>
#endif
#include "../bug_authority/job_codec.h"
#include "../snapshot_building_key.h"
#include "../snapshot_zone_key.h"
#include "../snapshot_npc_key.h"
#include "../snapshot_district_key.h"
#include "../district_lifecycle.h"
#include "../district_stroke_state.h"
#include "../group_transition.h"
#include "../group_history.h"
#include "../duplicate_transition.h"
#include "../scenery_stroke_transition.h"
#include "../group_object_transition.h"
#include "../modular_transition.h"
#include "../boss_placement_state.h"
#include "../duplicate_history.h"
#include "../building_history_slice/cash_effect.h"
#include "../dungeon_gizmo_portable.h"
#include "../npc_target_key.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>

namespace mt2mp::action_session {
namespace {
using json=nlohmann::json;
// Measure native work separately from waiting for a remote acknowledgement.
class ActionMeasurement {
public:
    explicit ActionMeasurement(const Request& request):request_(request) {}
    void checkpoint(const char* phase) {
#ifdef MT2MP_FRAMEWORK_PLUGIN
        const auto end=std::chrono::steady_clock::now();
        const auto micros=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(end-started_).count());
        reporting::event(phase,{{"action_type",uint64_t(request_.action)},{"duration_us",micros}},request_.action_uuid);
        if(micros>=100000)LOG_WARN("[ActionTiming] phase={} action_type={} action={} duration_us={}",phase,unsigned(request_.action),request_.action_uuid,micros);
        started_=std::chrono::steady_clock::now();
#else
        (void)phase;
#endif
    }
private:
    const Request& request_;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
#endif
};
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
    return game::snapshot_building_key::valid(s)||game::scenery_scene_key::saved(s);
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
#include "region_appearance_codec.inc"
bool valid_property_intent(const RegionPropertyIntent& v) {
    if(v.id<0)return false;
    if(appearance_kind(v.kind))return valid_appearance_intent(v);
    if(v.appearance_value!=std::array<float,16>{})return false;
    if(v.kind==RegionPropertyKind::LevelRange)return v.name.empty()&&v.type==-1&&
        v.levels[0]>=1&&v.levels[1]>=v.levels[0]&&v.levels[1]<=INT32_MAX/10-10&&
        std::all_of(v.hit.begin(),v.hit.end(),[](float x){return std::isfinite(x)&&std::abs(x)<=1000000.0f;});
    if(v.levels!=std::array<int32_t,2>{-1,-1})return false;
    if(v.kind==RegionPropertyKind::Name)return region_name(v.name)&&v.type==-1&&v.hit==std::array<float,3>{};
    if(!v.name.empty())return false;
    if(v.kind==RegionPropertyKind::Type&&v.type!=0&&v.type!=1)return false;
    if((v.kind==RegionPropertyKind::Purchase||v.kind==RegionPropertyKind::Sale)&&v.type!=-1)return false;
    if(v.kind!=RegionPropertyKind::Type&&v.kind!=RegionPropertyKind::Purchase&&v.kind!=RegionPropertyKind::Sale)return false;
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
         v.network_node_count<=1000000&&v.network_pipe_count<=1000000&&(!v.actors||valid_level_state(*v.actors))&&(!v.appearance||valid_appearance(*v.appearance)));
}
json property_intent_json(const RegionPropertyIntent& v) {
    json result={{"kind",uint8_t(v.kind)},{"id",v.id},{"name",v.name},{"type",v.type},{"hit",v.hit}};
    if(v.kind==RegionPropertyKind::LevelRange)result["levels"]=v.levels;
    if(appearance_kind(v.kind))result["appearance_value"]=v.appearance_value;return result;
}
RegionPropertyIntent property_intent_read(const json& j) {
    if(j.contains("appearance_value"))exact(j,{"kind","id","name","type","hit","appearance_value"});
    else if(j.contains("levels"))exact(j,{"kind","id","name","type","hit","levels"});
    else exact(j,{"kind","id","name","type","hit"});
    RegionPropertyIntent v;const auto kind=u64(j.at("kind"));
    if(kind<1||kind>11)throw std::runtime_error("region property kind outside domain");
    v.kind=RegionPropertyKind(kind);v.id=i32(j.at("id"));v.name=j.at("name").get<std::string>();v.type=i32(j.at("type"));
    const auto& hit=j.at("hit");if(!hit.is_array()||hit.size()!=3)throw std::runtime_error("region hit requires three coordinates");
    for(size_t i=0;i<3;++i)v.hit[i]=scalar(hit[i]);
    if(v.kind==RegionPropertyKind::LevelRange){const auto& a=j.at("levels");
        if(!a.is_array()||a.size()!=2)throw std::runtime_error("region levels requires two integers");
        for(size_t i=0;i<2;++i)v.levels[i]=i32(a[i]);}
    else if(j.contains("levels"))throw std::runtime_error("region levels on unrelated property");
    if(appearance_kind(v.kind))v.appearance_value=appearance_array<16>(j.at("appearance_value"));
    else if(j.contains("appearance_value"))throw std::runtime_error("appearance value on unrelated property");
    if(!valid_property_intent(v))throw std::runtime_error("region property intent outside domain");return v;
}
json property_state_json(const RegionPropertyState& v) {
    json result={{"content",region_json(v.content)},{"type_random",v.type_random},{"name",v.name},{"type",v.type},{"status",v.status},
        {"level",v.level},{"max_level",v.max_level},{"owned",v.owned},
        {"building_count",v.building_count},
        {"path_count",v.path_count},{"path_node_count",v.path_node_count},
        {"network_node_count",v.network_node_count},{"network_pipe_count",v.network_pipe_count}};
    if(v.appearance)result["appearance"]=appearance_json(*v.appearance);
    if(v.actors){const auto& a=*v.actors;result["actors"]={{"counts",a.counts},{"roster_sha256",a.roster_sha256},
        {"levels_sha256",a.levels_sha256},{"expected_sha256",a.expected_sha256},{"global_random",a.global_random}};}return result;
}
RegionPropertyState property_state_read(const json& j) {
    auto fields=j;fields.erase("actors");fields.erase("appearance");
    exact(fields,{"content","type_random","name","type","status","level","max_level","owned",
             "building_count","path_count","path_node_count","network_node_count","network_pipe_count"});
    RegionPropertyState v;if(j.contains("appearance"))v.appearance=appearance_read(j.at("appearance"));v.content=region_read(j.at("content"));v.name=j.at("name").get<std::string>();
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
#include "zone_codec.inc"
#include "monster_codec.inc"
#include "path_codec.inc"
#include "network_codec.inc"
#include "network_pipe_codec.inc"
#include "npc_codec.inc"
json building_json(const BuildingState&);
BuildingState building_read(const json&,bool);
#include "prop_text_codec.inc"
#include "district_codec.inc"
#include "district_title_codec.inc"
#include "district_property_codec.inc"
#include "district_stroke_codec.inc"
#include "zone_edit_codec.inc"
#include "group_codec.inc"
#include "duplicate_codec.inc"
#include "scenery_stroke_codec.inc"
#include "group_object_codec.inc"
#include "boss_placement_codec.inc"
#include "world_path_codec.inc"
#include "service_membership_codec.inc"
#include "selection_move_codec.inc"
#include "boss_phase_codec.inc"
#include "npc_property_codec.inc"
#include "region_sale_codec.inc"
#include "monster_quest_codec.inc"
#include "npc_quest_action_codec.inc"
#include "npc_quest_list_codec.inc"
#include "character_rename_codec.inc"
#include "character_membership_codec.inc"
#include "ability_edit_codec.inc"
#include "npc_quest_removal_codec.inc"
#include "monster_edit_codec.inc"
#include "npc_history_codec.inc"
#include "ability_membership_codec.inc"
#include "costume_edit_codec.inc"
#include "dungeon_config_codec.inc"
#include "dungeon_light_codec.inc"
#include "dungeon_sound_codec.inc"
#include "dungeon_palette_codec.inc"
#include "dungeon_gizmo_codec.inc"
#include "dungeon_input_codec.inc"
#include "building_status_codec.inc"
bool valid_npc_target(const Request& r) {
    if(r.target_action_uuid.empty())return game::snapshot_npc_key::valid(r.canonical_record_key)||game::building_npc_key::valid(r.canonical_record_key);
    return uuid(r.target_action_uuid)&&r.action_uuid!=r.target_action_uuid&&
        r.canonical_record_key=="action:"+r.target_action_uuid;
}
#include "bug_fix_codec.inc"
#include "management_codec.inc"
#include "network_remove_codec.inc"
json logo_state_json(const LogoState& v){return {{"present",v.present},{"sha256",v.sha256}};}
LogoState logo_state_read(const json& j){exact(j,{"present","sha256"});if(!j.at("present").is_boolean())throw std::runtime_error("Logo presence is not Boolean");return {j.at("present").get<bool>(),j.at("sha256").get<std::string>()};}
bool research_action(Action action){return action==Action::StartResearch||action==Action::SetResearchWorkers||action==Action::CommitResearch;}
bool path_history(const Request& r){
    return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&uuid(r.target_action_uuid)&&r.canonical_record_key=="path:"+r.target_action_uuid;
}
bool terrain_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&uuid(r.target_action_uuid)&&r.canonical_record_key=="terrain:"+r.target_action_uuid;}
bool quest_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="quests:"+r.target_action_uuid;}
bool dungeon_input_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="dungeon-input:"+r.target_action_uuid;}
bool dungeon_tiles_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="dungeon-tiles:"+r.target_action_uuid;}
bool region_history(const Request& r){
    return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&game::region_history_key::parse(r.canonical_record_key).has_value();
}
#include "duplicate_history_codec.inc"
#include "scenery_stroke_history_codec.inc"
#include "modular_codec.inc"
bool group_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="groups:"+r.target_action_uuid;}
bool group_history_effect(const Request& r,const Evidence& e){return group_history(r)&&e.group_before&&e.group_after&&
    group_state_valid(*e.group_before)&&group_state_valid(*e.group_after)&&e.before==e.after&&e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&
    e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&game::group_history::structural_effect(*e.group_before,*e.group_after);}
bool dungeon_building_request(const Request&);
bool dungeon_building_execution(const Request&,const Evidence&);
bool valid_request(const Request& r) {
    if(r.dungeon_building){auto base=std::make_unique<Request>(r);base->dungeon_building=false;base->dungeon_scene.clear();
        return dungeon_building_request(r)&&valid_request(*base);}
    if(!r.dungeon_scene.empty())return false;
    if(r.action==Action::EditZone){
        auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;
        base->canonical_record_key="tech:startingpoint";base->zone_edit={};
        return r.canonical_record_key=="zone-edit:"+r.action_uuid&&r.target_action_uuid.empty()&&
            valid_zone_edit_intent(r.zone_edit)&&valid_request(*base);
    }
    if(r.zone_edit!=ZoneEditIntent{})return false;
    if(r.action==Action::DistrictStroke){
        auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;
        base->canonical_record_key="tech:startingpoint";base->district_stroke={};
        return r.canonical_record_key=="district-stroke:"+r.action_uuid&&r.target_action_uuid.empty()&&
            game::district_stroke_state::intent(r.district_stroke)&&valid_request(*base);
    }
    if(r.district_stroke!=DistrictStrokeIntent{})return false;
    if(modular_action(r)){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.target_action_uuid.clear();base.modular={};
        return (r.action==Action::EditModularBuilding?(r.canonical_record_key=="modular:"+r.action_uuid&&r.target_action_uuid.empty()&&modular_valid(r.modular)):r.modular==ModularIntent{})&&valid_request(base);}
    if(r.modular!=ModularIntent{})return false;
    if(r.action==Action::DungeonInput){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.dungeon_input={};
        return r.canonical_record_key==r.dungeon_input.key&&r.target_action_uuid.empty()&&valid_dungeon_input(r.dungeon_input)&&valid_request(base);}
    if(r.dungeon_input!=DungeonInputIntent{})return false;
    if(npc_history(r)){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->target_action_uuid.clear();
        return uuid(r.target_action_uuid)&&r.action_uuid!=r.target_action_uuid&&valid_request(*base);}
    if(monster_history(r)){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.target_action_uuid.clear();
        return r.action_uuid!=r.target_action_uuid&&valid_request(base);}
    if(dungeon_input_history(r)){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.target_action_uuid.clear();
        return r.action_uuid!=r.target_action_uuid&&valid_request(base);}

    if(r.action==Action::EditDistrict){
        auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;
        base->canonical_record_key="tech:startingpoint";base->district_property={};
        return game::district_lifecycle::valid_key(r.canonical_record_key)&&r.target_action_uuid.empty()&&
            game::district_property::valid(r.district_property)&&valid_request(*base);
    }
    if(r.district_property!=game::district_property::Value{})return false;
    if(world_path_action(r.action)){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.world_path={};
        return r.canonical_record_key=="action:"+r.action_uuid&&r.target_action_uuid.empty()&&
            world_path_valid(r.world_path)&&
            (r.action==Action::PlaceWall)==(r.world_path.kind==WorldPathKind::Wall)&&
            (r.action==Action::RemoveWorldPath)==(r.world_path.kind==WorldPathKind::Remove)&&valid_request(base);
    }
    if(r.world_path!=WorldPathIntent{})return false;
    if(r.action==Action::PlaceBoss){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.boss_placement={};
        return r.canonical_record_key==game::boss_placement_state::key(r.action_uuid)&&r.target_action_uuid.empty()&&game::boss_placement_reader::valid(r.boss_placement)&&valid_request(base);}
    if(r.boss_placement!=BossPlacementIntent{})return false;
    if(group_object_action(r.action)){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.group_object={};
        return r.canonical_record_key==r.group_object.key&&r.target_action_uuid.empty()&&group_object_intent_valid(r.action==Action::MoveGroup,r.group_object)&&valid_request(base);}
    if(r.group_object!=GroupObjectIntent{})return false;
    if(scenery_stroke_action(r.action)){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.scenery_stroke={};
        return r.canonical_record_key=="scenery-stroke:"+r.action_uuid&&r.target_action_uuid.empty()&&scenery_stroke_intent_valid(r.scenery_stroke)&&
            (r.action==Action::SceneryBrushStroke)==(r.scenery_stroke.kind==SceneryStrokeKind::Paint)&&valid_request(base);}
    if(r.scenery_stroke!=SceneryStrokeIntent{})return false;
    if(r.action==Action::DuplicateSelection){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.duplicate={};
        return r.canonical_record_key=="duplicate:"+r.action_uuid&&r.target_action_uuid.empty()&&duplicate_intent_valid(r.duplicate)&&valid_request(base);}
    if(r.duplicate!=DuplicateIntent{})return false;
    if(r.action==Action::EditGroup){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.group={};
        return r.canonical_record_key=="groups:"+r.action_uuid&&r.target_action_uuid.empty()&&group_intent_valid(r.group)&&valid_request(base);}
    if(r.group!=GroupIntent{})return false;
    if(r.action==Action::MoveDistrictTitle){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.district_title={};
        return game::district_lifecycle::valid_key(r.canonical_record_key)&&r.target_action_uuid.empty()&&district_title_valid(r.district_title)&&valid_request(base);}
    if(r.district_title!=DistrictTitleIntent{})return false;
    if(r.action==Action::EditDistrictText){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.district_text={};
        return game::district_lifecycle::valid_key(r.canonical_record_key)&&r.target_action_uuid.empty()&&district_intent_valid(r.district_text)&&valid_request(base);}
    if(r.district_text!=DistrictTextIntent{})return false;
    if(r.action==Action::MoveSelection){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.selection_move={};
        return r.canonical_record_key=="selection:"+r.action_uuid&&r.target_action_uuid.empty()&&selection_move_intent_valid(r.selection_move)&&valid_request(base);}
    if(r.selection_move!=SelectionMoveIntent{})return false;
    if(r.action==Action::EditServiceMembership){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.service_membership={};
        return key(r.canonical_record_key)&&r.target_action_uuid.empty()&&service_membership_intent_valid(r.service_membership)&&valid_request(base);}
    if(r.service_membership!=ServiceMembershipIntent{})return false;
    if(r.action==Action::ReorderBossPhase){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.boss_phase={};
        return r.canonical_record_key=="character:3:"+std::to_string(r.boss_phase.character_id)&&r.target_action_uuid.empty()&&boss_intent_valid(r.boss_phase)&&valid_request(base);
    }
    if(r.boss_phase!=BossPhaseIntent{})return false;
    if(r.action==Action::EditPropText){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.prop_text={};
        return (r.prop_text.gizmo_scene.empty()?key(r.canonical_record_key):game::dungeon_gizmo_identity::key(r.canonical_record_key))&&r.target_action_uuid.empty()&&prop_text_intent_valid(r.prop_text)&&valid_request(base);
    }
    if(r.prop_text!=PropTextIntent{})return false;
    if(r.action==Action::EditNpcProperty){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";
        base.npc_property={};
        return game::npc_target_key::valid(r.canonical_record_key)&&r.target_action_uuid.empty()&&
            npc_property_intent_valid(r.npc_property)&&valid_request(base);
    }
    if(r.npc_property!=NpcPropertyIntent{})return false;
    if(r.action==Action::Intervention){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->intervention={};
        return r.canonical_record_key==r.intervention.target.key&&r.target_action_uuid.empty()&&game::interventions::valid(r.intervention)&&valid_request(*base);}
    if(r.intervention!=InterventionIntent{})return false;
    if(r.action==Action::GameRules){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->game_rules={};
        return r.canonical_record_key=="management:game-rules"&&r.target_action_uuid.empty()&&game::game_rules::valid(r.game_rules)&&valid_request(*base);}
    if(r.game_rules!=GameRulesIntent{})return false;
    if(r.action==Action::GmPolicy){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->gm_policy={};
        return r.canonical_record_key=="management:gm-policy"&&r.target_action_uuid.empty()&&game::gm_policy::valid(r.gm_policy)&&valid_request(*base);}
    if(r.gm_policy!=GmPolicyIntent{})return false;
    if(r.action==Action::ReleaseCycle){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->release_cycle={};
        return r.canonical_record_key=="release:cycle"&&r.target_action_uuid.empty()&&game::release_cycle::valid(r.release_cycle)&&
            (r.release_cycle.command!=game::release_cycle::Command::Show||r.release_cycle.cycle==r.action_uuid)&&
            (r.release_cycle.command!=game::release_cycle::Command::Finalize||r.peer==1)&&valid_request(*base);}
    if(r.release_cycle!=ReleaseCycleIntent{})return false;
    if(r.action==Action::ReleaseEdit){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->release_edit={};
        return r.canonical_record_key=="release:editing"&&r.target_action_uuid.empty()&&game::release_edit::valid(r.release_edit)&&valid_request(*base);}
    if(r.release_edit!=ReleaseEditIntent{})return false;
    if(r.action==Action::GeneralProject){auto base=std::make_unique<Request>(r);base->action=Action::StartStartingpoint;base->canonical_record_key="tech:startingpoint";base->general_project={};
        return r.canonical_record_key=="development:project"&&r.target_action_uuid.empty()&&general_project_valid(r.general_project)&&valid_request(*base);}
    if(r.general_project!=GeneralProjectIntent{})return false;
    if(r.action==Action::SetLogo){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.logo={};
        return r.canonical_record_key=="software:logo"&&r.target_action_uuid.empty()&&valid_logo(r.logo)&&valid_request(base);}
    if(r.logo!=LogoIntent{})return false;
    if(r.action==Action::RemoveNetworkObject){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.network_remove={};
        return game::network_object_identity::key(r.canonical_record_key)&&r.target_action_uuid.empty()&&valid_network_remove(r.network_remove)&&valid_request(base);}
    if(r.network_remove!=NetworkRemoveIntent{})return false;
    if(r.action==Action::ManagementCommand){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.management={};
        return r.canonical_record_key=="management:business"&&r.target_action_uuid.empty()&&management_valid(r.management)&&valid_request(base);}
    if(r.management!=ManagementIntent{})return false;
    if(r.action==Action::SetBuildingStatus){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.building_status={};
        return r.canonical_record_key==r.building_status.key&&r.target_action_uuid.empty()&&valid_building_status_intent(r.building_status)&&valid_request(base);}
    if(r.building_status!=BuildingStatusIntent{})return false;
    if(r.action==Action::SetDungeonLighting){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.dungeon_light={};
        return r.canonical_record_key==r.dungeon_light.key&&r.target_action_uuid.empty()&&valid_dungeon_light_intent(r.dungeon_light)&&valid_request(base);}
    if(r.dungeon_light!=DungeonLightIntent{})return false;
    if(r.action==Action::EditDungeonGizmo){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.dungeon_gizmo={};
        return r.canonical_record_key==r.dungeon_gizmo.key&&r.target_action_uuid.empty()&&valid_dungeon_gizmo_intent(r.dungeon_gizmo)&&valid_request(base);}
    if(r.dungeon_gizmo!=DungeonGizmoIntent{})return false;
    if(r.action==Action::SetDungeonPalette){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.dungeon_palette={};
        return r.canonical_record_key==r.dungeon_palette.key&&r.target_action_uuid.empty()&&valid_dungeon_palette_intent(r.dungeon_palette)&&valid_request(base);}
    if(r.dungeon_palette!=DungeonPaletteIntent{})return false;
    if(r.action==Action::SetDungeonAtmosphere){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.dungeon_sound={};
        return r.canonical_record_key==r.dungeon_sound.key&&r.target_action_uuid.empty()&&valid_dungeon_sound_intent(r.dungeon_sound)&&valid_request(base);}
    if(r.dungeon_sound!=DungeonSoundIntent{})return false;
    if(r.action==Action::ConfigureDungeonInstances){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.dungeon_config={};
        return r.canonical_record_key==r.dungeon_config.key&&r.target_action_uuid.empty()&&valid_dungeon_config_intent(r.dungeon_config)&&valid_request(base);}
    if(r.dungeon_config!=DungeonConfigIntent{})return false;
    if(r.action==Action::EditCharacterCostume){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.costume_edit={};
        return r.canonical_record_key==costume_key(r.costume_edit)&&r.target_action_uuid.empty()&&valid_costume_edit_intent(r.costume_edit)&&valid_request(base);}
    if(r.costume_edit!=CostumeEditIntent{})return false;
    if(r.action==Action::EditCharacterAbility){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.ability_edit={};
        return r.canonical_record_key==ability_key(r.ability_edit)&&r.target_action_uuid.empty()&&valid_ability_edit_intent(r.ability_edit)&&valid_request(base);}
    if(r.ability_edit!=AbilityEditIntent{})return false;
    if(r.action==Action::EditAbilityMembership){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.ability_membership={};
        return r.canonical_record_key==ability_membership_key(r.ability_membership)&&r.target_action_uuid.empty()&&valid_ability_membership_intent(r.ability_membership)&&valid_request(base);}
    if(r.ability_membership!=AbilityMembershipIntent{})return false;
    if((r.action>=Action::RenameCharacter&&r.action<=Action::SetCharacterSpeed)){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.character_rename={};
        return r.canonical_record_key==character_key(r.character_rename)&&r.target_action_uuid.empty()&&valid_character_rename_intent(r.character_rename)&&uint8_t(r.action)==80+r.character_rename.property&&valid_request(base);}
    if(r.character_rename!=CharacterRenameIntent{})return false;
    if(r.action==Action::EditCharacterMembership){auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.character_membership={};
        return r.canonical_record_key==character_membership_key(r.character_membership)&&r.target_action_uuid.empty()&&valid_character_membership_intent(r.character_membership)&&valid_request(base);}
    if(r.character_membership!=CharacterMembershipIntent{})return false;
    if((r.action==Action::SetNpcQuestList||r.action==Action::EditNpcQuest)){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.npc_quest_list={};
        return game::npc_target_key::valid(r.canonical_record_key)&&r.target_action_uuid.empty()&&
            valid_npc_quest_list_intent(r.npc_quest_list)&&((r.action==Action::SetNpcQuestList)==(r.npc_quest_list.edit==QuestListEdit::List))&&valid_request(base);
    }
    if(r.npc_quest_list!=NpcQuestListIntent{})return false;
    if(r.action==Action::SetNpcQuestDestination){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";
        base.npc_quest={};
        return game::npc_target_key::valid(r.canonical_record_key)&&r.target_action_uuid.empty()&&
            valid_npc_quest_intent(r.npc_quest)&&valid_request(base);
    }
    if(r.npc_quest!=NpcQuestIntent{})return false;
    if(r.action==Action::RemoveMonster){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";
        base.target_action_uuid.clear();base.monster_remove={};
        return game::monster_target_key::request(r.canonical_record_key,r.target_action_uuid)&&r.action_uuid!=r.target_action_uuid&&r.monster_remove.region>=0&&valid_request(base);
    }
    if(r.monster_remove!=MonsterRemoveIntent{})return false;
    if(r.action==Action::MoveMonster){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";
        base.target_action_uuid.clear();base.monster_move={};
        return game::monster_move_key::request(r.canonical_record_key,r.target_action_uuid)&&r.action_uuid!=r.target_action_uuid&&valid_monster_move(r.monster_move)&&valid_request(base);
    }
    if(r.monster_move!=MonsterMoveIntent{})return false;
    if(r.action==Action::PlaceMonster)return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&
        r.canonical_record_key=="action:"+r.action_uuid&&valid_monster_intent(r.monster)&&!r.desired_workers&&
        r.building==BuildingState{}&&r.region==RegionIntent{}&&r.region_property==RegionPropertyIntent{}&&
        r.price==PriceIntent{}&&r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&r.path_delete==PathDeleteIntent{}&&
        r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{}&&r.npc==NpcIntent{}&&r.zone==ZoneIntent{}&&r.bug_fix==BugFixIntent{};
    if(r.monster!=MonsterIntent{})return false;

    if(research_action(r.action)){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";base.desired_workers=0;
        return r.canonical_record_key=="tech:bridges"&&
            (r.action==Action::SetResearchWorkers?game::tutorial_replication::staff::value(r.desired_workers):r.desired_workers==0)&&valid_request(base);
    }
    if(r.action==Action::SetBugFix){auto base=r;base.action=Action::StartStartingpoint;
        base.bug_fix={};base.canonical_record_key="tech:startingpoint";
        return r.canonical_record_key==r.bug_fix.bug.parent_key&&bug_fix_intent_valid(r.bug_fix)&&valid_request(base);}
    if(r.bug_fix!=BugFixIntent{})return false;
    if(r.action==Action::PlaceMonsterZone)return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&
        r.canonical_record_key=="action:"+r.action_uuid&&valid_zone_intent(r.zone)&&!r.desired_workers&&
        r.building==BuildingState{}&&r.region==RegionIntent{}&&r.region_property==RegionPropertyIntent{}&&
        r.price==PriceIntent{}&&r.terrain==TerrainIntent{}&&r.path==PathIntent{}&&r.path_delete==PathDeleteIntent{}&&
        r.network==NetworkIntent{}&&r.network_pipe==NetworkPipeIntent{}&&r.npc==NpcIntent{};
    if(r.zone!=ZoneIntent{})return false;
    if(r.action==Action::HireDeveloper||r.action==Action::FireDeveloper){
        auto base=r;base.action=Action::StartStartingpoint;base.canonical_record_key="tech:startingpoint";
        return r.canonical_record_key=="staff:developers"&&r.desired_workers==0&&valid_request(base);
    }
    if(r.action==Action::SetStartingpointWorkers){auto base=r;base.action=Action::StartStartingpoint;base.desired_workers=0;return game::tutorial_replication::staff::value(r.desired_workers)&&valid_request(base);}
    if(r.desired_workers)return false;
    if(r.action==Action::CommitStartingpoint||r.action==Action::StartStartingpoint)return uuid(r.action_uuid)&&r.target_action_uuid.empty()&&
        r.canonical_record_key=="tech:startingpoint"&&r.building==BuildingState{}&&r.region==RegionIntent{}&&
        r.region_property==RegionPropertyIntent{}&&r.price==PriceIntent{}&&r.terrain==TerrainIntent{}&&
        r.path==PathIntent{}&&r.path_delete==PathDeleteIntent{}&&r.network==NetworkIntent{}&&
        r.network_pipe==NetworkPipeIntent{}&&r.npc==NpcIntent{};
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
            (key(r.canonical_record_key)||selection_history(r)||region_history(r)||path_history(r)||terrain_history(r)||quest_history(r)||network_history(r)||group_history(r)||duplicate_history(r)||scenery_stroke_history(r)||group_object_history(r)||district_property_history(r)||district_stroke_history(r)||zone_edit_history(r)||dungeon_tiles_history(r))&&r.building==BuildingState{}&&
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
    if(npc_history(r))return valid_npc_history_effect(r,e);
    if(monster_history(r))return monster_history_effect(r,e);
    if(modular_action(r))return modular_effect(r,e);
    if(group_object_history(r))return group_object_effect(r,e);
    if(scenery_stroke_history(r))return scenery_stroke_history_effect(r,e);
    if(duplicate_history(r))return duplicate_history_effect(r,e);
    if(group_history(r))return group_history_effect(r,e);
    if(r.action==Action::UndoBuilding||r.action==Action::RedoBuilding) {
        if(e.object_before.alive&&e.object_after.alive)
            return e.object_before.type==e.object_after.type&&e.object_before.variant==e.object_after.variant&&
                e.before==e.after;
        auto other=e.object_before;other.alive=!other.alive;
        return e.object_after==other&&e.object_before.type==e.object_after.type&&
            e.object_before.variant==e.object_after.variant;
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
#include "region_ownership_codec.inc"
#include "quest_history_codec.inc"
#include "terrain_history_codec.inc"

bool execution(const Request& r,const Evidence& e) {
    if(r.dungeon_building){auto base=std::make_unique<Request>(r);base->dungeon_building=false;base->dungeon_scene.clear();
        auto proof=std::make_unique<Evidence>(e);proof->dungeon_building_effect.reset();
        return dungeon_building_execution(r,e)&&execution(*base,*proof);}
    if(e.dungeon_building_effect)return false;
    if(r.action==Action::EditZone||zone_edit_history(r))return zone_edit_effect(r,e);
    if(npc_history(r))return npc_history_effect(r,e);
    if(monster_history(r))return monster_history_effect(r,e);
    if(r.action==Action::DistrictStroke||district_stroke_history(r))return district_stroke_effect(r,e);
    if(district_property_history(r))return district_property_history_effect(r,e);
    if(r.action==Action::PlaceBoss)return boss_placement_effect(r,e);
    if(terrain_history(r))return terrain_history_effect(r,e);
    if(quest_history(r))return quest_history_effect(r,e);
    if(network_history(r)){std::string cause;return network_history_effect(r,e,cause);}
    if(region_history(r)){
        if(e.property_before.owned!=e.property_after.owned)return region_ownership_effect(r,e)&&
            e.region_before==RegionState{}&&e.region_after==RegionState{}&&e.terrain_before==TerrainState{}&&e.terrain_after==TerrainState{}&&
            e.path_before==PathState{}&&e.path_after==PathState{}&&e.network_before==NetworkState{}&&e.network_after==NetworkState{};
        auto normal=r;normal.action=Action::EditRegionProperty;normal.target_action_uuid.clear();
        normal.region_property.kind=RegionPropertyKind::LevelRange;
        normal.region_property.id=*game::region_history_key::parse(r.canonical_record_key);
        normal.region_property.levels={e.property_after.level,e.property_after.max_level};
        return valid_request(normal)&&execution(normal,e)&&e.region_before==RegionState{}&&e.region_after==RegionState{}&&
            e.terrain_before==TerrainState{}&&e.terrain_after==TerrainState{}&&e.path_before==PathState{}&&e.path_after==PathState{}&&
            e.network_before==NetworkState{}&&e.network_after==NetworkState{};
    }
    if(r.action==Action::Intervention)return e.intervention_before&&e.intervention_after&&game::interventions::effect(r.intervention,*e.intervention_before,*e.intervention_after)&&(!game::interventions::control_command(r.intervention.command)||((!e.intervention_before->control->controlled||e.intervention_before->control->peer==r.peer)&&(!e.intervention_after->control->controlled||e.intervention_after->control->peer==r.peer)))&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed;
    if(r.action==Action::GameRules)return e.game_rules_before&&e.game_rules_after&&game::game_rules::effect(r.game_rules,*e.game_rules_before,*e.game_rules_after)&&e.before==e.after&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed;
    if(r.action==Action::GmPolicy)return e.gm_policy_before&&e.gm_policy_after&&game::gm_policy::effect(r.gm_policy,*e.gm_policy_before,*e.gm_policy_after)&&e.before==e.after&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed;
    if(r.action==Action::ReleaseCycle){std::string cause;return e.release_cycle_before&&e.release_cycle_after&&e.release_cycle_value&&game::release_cycle::effect(r.release_cycle,*e.release_cycle_before,*e.release_cycle_after,cause)&&
        (r.release_cycle.command==game::release_cycle::Command::Finalize||e.before==e.after)&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed;}
    if(r.action==Action::ReleaseEdit){std::string cause;return e.release_edit_before&&e.release_edit_after&&game::release_edit::effect(r.release_edit,*e.release_edit_before,*e.release_edit_after,cause)&&e.before==e.after&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed;}
    if(r.action==Action::GeneralProject){std::string cause;return e.general_project_before&&e.general_project_after&&
        game::general_project::effect(r.general_project,*e.general_project_before,*e.general_project_after,cause)&&
        (r.general_project.command==game::general_project::Command::Start||e.before==e.after)&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed;}
    if(r.action==Action::SetLogo)return e.logo_before&&e.logo_after&&e.logo_before->sha256==r.logo.before_sha256&&e.logo_after->present&&e.logo_after->sha256==r.logo.desired_sha256&&
        e.before==e.after&&!e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed;
    if(r.action==Action::RemoveNetworkObject){auto expected=e.before;
        auto add=[](int32_t& value,int64_t delta){const auto n=int64_t(value)+delta;if(n<INT32_MIN||n>INT32_MAX)return false;value=int32_t(n);return true;};
        const auto refund=r.network_remove.quoted_refund;
        if(refund&&(!expected.record_present||!add(expected.cash,refund)||!add(expected.record_cash,refund)||!add(expected.record_income,refund)||!add(expected.line_counts[6],1)||!add(expected.line_income[6],refund)))return false;
        return e.network_removal_before&&e.network_removal_after&&e.target_retired&&e.undo_confirmed&&e.selection_confirmed&&
        expected==e.after&&network_remove_transition(r.canonical_record_key,r.network_remove,*e.network_removal_before,*e.network_removal_after);}
    if(r.action==Action::ManagementCommand){Ledger expected;
        return e.management_before&&e.management_after&&management_transition(r.management,*e.management_before,*e.management_after)&&
            management_ledger(r.management,*e.management_before,e.before,expected)&&expected==e.after&&
            !e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};}
    if(r.action==Action::SetBuildingStatus)return building_status_transition(r.building_status,e.building_status_before,e.building_status_after)&&
        e.before==e.after&&!e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::SetDungeonLighting)return dungeon_light_transition(r.dungeon_light,e.dungeon_light_before,e.dungeon_light_after)&&e.before==e.after&&
        !e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::EditDungeonGizmo)return dungeon_gizmo_transition(r.dungeon_gizmo,e.dungeon_gizmo_before,e.dungeon_gizmo_after)&&e.before==e.after&&
        !e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(dungeon_tiles_history(r))return e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.before==e.after&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&dungeon_cells_history_transition(r.action==Action::UndoBuilding,e.dungeon_palette_before,e.dungeon_palette_after);
    if(r.action==Action::DungeonInput||dungeon_input_history(r)){
        const bool history=dungeon_input_history(r);const auto* intent=history?(e.dungeon_input_origin?&*e.dungeon_input_origin:nullptr):&r.dungeon_input;
        if(!intent||!e.undo_confirmed||e.selection_confirmed||e.target_retired||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
        return history?(dungeon_input_history_transition(r.target_action_uuid,r.action==Action::UndoBuilding,*intent,e.dungeon_input_before,e.dungeon_input_after)&&
            dungeon_input_history_cash(r.action==Action::UndoBuilding,*intent,e.before,e.after)):
            (dungeon_input_transition(r.action_uuid,*intent,e.dungeon_input_before,e.dungeon_input_after)&&dungeon_input_purchase(*intent,e.before,e.after));
    }
    if(r.action==Action::SetDungeonPalette)return dungeon_palette_transition(r.dungeon_palette,e.dungeon_palette_before,e.dungeon_palette_after)&&e.before==e.after&&
        e.undo_confirmed==(r.dungeon_palette.edit==DungeonPaletteEdit::Cells)&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::SetDungeonAtmosphere)return dungeon_sound_transition(r.dungeon_sound,e.dungeon_sound_before,e.dungeon_sound_after)&&e.before==e.after&&
        !e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::ConfigureDungeonInstances){Ledger expected;return dungeon_config_transition(r.dungeon_config,e.dungeon_config_before,e.dungeon_config_after)&&dungeon_config_ledger(r.dungeon_config,e.dungeon_config_before,e.before,expected)&&expected==e.after&&
        !e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};}
    if(r.action==Action::EditAbilityMembership)return ability_membership_transition(r.ability_membership,e.ability_membership_before,e.ability_membership_after)&&
        e.before==e.after&&!e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::EditCharacterMembership)return character_membership_transition(r.character_membership,e.character_membership_before,e.character_membership_after)&&
        !e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{}&&
        (r.character_membership.command==CharacterMembershipCommand::Destroy||e.before==e.after);
    if(r.action==Action::EditCharacterCostume)return costume_edit_transition(r.costume_edit,e.costume_edit_before,e.costume_edit_after)&&
        e.before==e.after&&!e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::EditCharacterAbility)return ability_edit_transition(r.ability_edit,e.ability_edit_before,e.ability_edit_after)&&
        e.before==e.after&&!e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if((r.action>=Action::RenameCharacter&&r.action<=Action::SetCharacterSpeed))return character_rename_transition(r.character_rename,e.character_rename_before,e.character_rename_after)&&
        e.before==e.after&&!e.undo_confirmed&&!e.selection_confirmed&&!e.target_retired&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if((r.action==Action::SetNpcQuestList||r.action==Action::EditNpcQuest))return npc_quest_list_transition(r.npc_quest_list,e.quest_list_before,e.quest_list_after)&&
        e.before==e.after&&e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(modular_action(r))return modular_effect(r,e);
    if(group_object_history(r))return group_object_effect(r,e);
    if(scenery_stroke_history(r))return scenery_stroke_history_effect(r,e);
    if(duplicate_history(r))return duplicate_history_effect(r,e);
    if(group_history(r))return group_history_effect(r,e);
    if(r.action==Action::SetNpcQuestDestination)return npc_quest_transition(r.npc_quest,e.quest_before,e.quest_after)&&
        e.before==e.after&&e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::RemoveMonster)return monster_remove_transition(e.monster_before,e.monster_after)&&
        valid_monster_quests(e.monster_quests,r.canonical_record_key,true)&&
        e.before==e.after&&e.undo_confirmed&&e.target_retired&&e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::MoveMonster)return monster_move_transition(r.monster_move,e.monster_before,e.monster_after)&&
        e.before==e.after&&e.undo_confirmed&&!e.target_retired&&e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::PlaceMonster)return monster_transition(r.monster,e.monster_before,e.monster_after)&&
        e.before==e.after&&e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};

    if(research_action(r.action)){
        namespace research=game::tutorial_replication::research;
        namespace start=game::tutorial_replication::start_intent;
        if(!e.research_before||!e.research_after||e.research_before->key!="bridges"||e.research_after->key!=e.research_before->key||
           e.target_retired||e.undo_confirmed||e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
        const auto& a=e.research_before->value;const auto& b=e.research_after->value;std::string cause;
        if(r.action==Action::StartResearch)return start::rows(a,b,cause)&&start::payment(e.before,e.after,cause)&&int64_t(e.before.cash)-e.after.cash==10000;
        if(r.action==Action::SetResearchWorkers)return game::tutorial_replication::staff::rows(a,b,r.desired_workers,cause)&&e.before==e.after;
        research::Plan operation;
        return research::plan(e.research_before,e.research_after,operation,cause)&&operation==research::Plan::Deploy&&e.before==e.after;
    }
    if(r.action==Action::SetBugFix)return bug_fix_effect(r,e);
    if(r.action==Action::PlaceMonsterZone)return zone_transition(e.zone_before,e.zone_after)&&
        e.before==e.after&&e.undo_confirmed&&!e.target_retired&&!e.selection_confirmed&&
        e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    if(r.action==Action::HireDeveloper||r.action==Action::FireDeveloper){std::string cause;
        return e.developers_before&&e.developers_after&&
            game::developer_staff::count_effect(r.action==Action::HireDeveloper,*e.developers_before,*e.developers_after,cause)&&
            game::developer_staff::payment(r.action==Action::HireDeveloper,e.before,e.after,cause)&&
            !e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    }
    if(r.action==Action::SetStartingpointWorkers){std::string cause;
        return e.startingpoint_before&&e.startingpoint_after&&
            game::tutorial_replication::staff::rows(*e.startingpoint_before,*e.startingpoint_after,r.desired_workers,cause)&&
            e.before==e.after&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&
            e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    }
    if(r.action==Action::StartStartingpoint){
        namespace start=game::tutorial_replication::start_intent;std::string cause;
        return e.startingpoint_before&&e.startingpoint_after&&
            start::rows(*e.startingpoint_before,*e.startingpoint_after,cause)&&start::payment(e.before,e.after,cause)&&
            !e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&
            e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    }
    if(r.action==Action::CommitStartingpoint){
        namespace sp=game::tutorial_replication::startingpoint;
        sp::Plan operation;std::string cause;
        return e.startingpoint_before&&e.startingpoint_after&&
            sp::plan(e.startingpoint_before,e.startingpoint_after,operation,cause)&&operation==sp::Plan::Deploy&&
            e.before==e.after&&!e.target_retired&&!e.undo_confirmed&&!e.selection_confirmed&&
            e.object_before==BuildingState{}&&e.object_after==BuildingState{};
    }
    if(r.action==Action::ReorderBossPhase)return boss_effect(r,e);
    if(modular_action(r))return modular_effect(r,e);
    if(group_object_action(r.action))return group_object_effect(r,e);
    if(scenery_stroke_action(r.action))return scenery_stroke_effect(r,e);
    if(r.action==Action::DuplicateSelection)return duplicate_effect(r,e);
    if(r.action==Action::EditGroup)return group_effect(r,e);
    if(r.action==Action::MoveDistrictTitle)return district_title_effect(r,e);
    if(r.action==Action::EditDistrictText)return district_effect(r,e);
    if(r.action==Action::EditDistrict)return district_property_effect(r,e);
    if(r.action==Action::MoveSelection)return selection_move_effect(r,e);
    if(r.action==Action::EditServiceMembership)return service_effect(r,e);
    if(r.action==Action::EditPropText)return prop_text_effect(r,e);
    if(r.action==Action::EditNpcProperty)return npc_property_effect(r,e);
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
        if(appearance_kind(r.region_property.kind)){
            auto expected=before;
            return before.appearance&&after.appearance&&!before.actors&&!after.actors&&
                appearance_expected(r.region_property,*before.appearance,*expected.appearance)&&after==expected&&
                e.before==e.after&&!e.undo_confirmed;
        }
        if(r.region_property.kind==RegionPropertyKind::Sale)return region_sale_effect(before,after,e);
        if(before.appearance||after.appearance)return false;
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
    if(world_path_action(r.action)||(world_path_history(r)&&e.world_path_before))return world_path_effect(r,e);
    if(path_history(r)){std::string cause;return valid_path_state(e.path_before)&&valid_path_state(e.path_after)&&valid_region(e.region_before)&&valid_region(e.region_after)&&valid_terrain_state(e.terrain_before)&&valid_terrain_state(e.terrain_after)&&path_history_effect(r,e,cause);}
    if(selection_history(r))return selection_history_effect(e)&&
        e.region_before==RegionState{}&&e.region_after==RegionState{}&&
        e.terrain_before==TerrainState{}&&e.terrain_after==TerrainState{}&&
        e.path_before==PathState{}&&e.path_after==PathState{}&&
        e.network_before==NetworkState{}&&e.network_after==NetworkState{};
    if(r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)
        return valid_building(e.object_before)&&valid_building(e.object_after)&&
            object_effect(r,e)&&e.undo_confirmed&&!e.selection_confirmed&&
            e.target_retired==!e.object_after.alive&&
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
bool authority_evidence(const Request& request,const Evidence& local,const Evidence& host){
    if(monster_history(request)){
        if(!monster_history_effect(request,local)||!monster_history_effect(request,host)||
           !monster_shared_state(local.monster_before,host.monster_before)||!monster_shared_state(local.monster_after,host.monster_after))return false;
        auto left=local,right=host;left.monster_before={};left.monster_after={};right.monster_before={};right.monster_after={};return left==right;
    }
    if(research_action(request.action)){
        namespace research=game::tutorial_replication::research;
        if(!execution(request,local)||!execution(request,host)||
           !research::same_state(local.research_before,host.research_before)||
           !research::same_state(local.research_after,host.research_after))return false;
        // Retain each peer's actual local call receipt, including its exact stamp
        // delta. Only cross-peer comparison excludes the process-local stamp.
        auto left=local,right=host;
        left.research_before.reset();left.research_after.reset();
        right.research_before.reset();right.research_after.reset();
        return left==right;
    }
    if(request.action==Action::RemoveMonster){
        if(!monster_remove_transition(local.monster_before,local.monster_after)||
           !monster_remove_transition(host.monster_before,host.monster_after)||
           !monster_shared_state(local.monster_before,host.monster_before)||
           !monster_shared_state(local.monster_after,host.monster_after))return false;
        auto left=local,right=host;left.monster_before={};left.monster_after={};right.monster_before={};right.monster_after={};
        return left==right;
    }
    if(request.action==Action::MoveMonster){
        if(!monster_move_transition(request.monster_move,local.monster_before,local.monster_after)||
           !monster_move_transition(request.monster_move,host.monster_before,host.monster_after)||
           !monster_shared_state(local.monster_before,host.monster_before)||
           !monster_shared_state(local.monster_after,host.monster_after))return false;
        auto left=local,right=host;left.monster_before={};left.monster_after={};right.monster_before={};right.monster_after={};
        return left==right;
    }
    if(request.action==Action::PlaceMonster){
        if(!monster_transition(request.monster,local.monster_before,local.monster_after)||
           !monster_transition(request.monster,host.monster_before,host.monster_after)||
           !monster_shared_state(local.monster_before,host.monster_before)||
           !monster_shared_state(local.monster_after,host.monster_after))return false;
        auto left=local,right=host;left.monster_before={};left.monster_after={};right.monster_before={};right.monster_after={};
        return left==right;
    }
    if(request.action!=Action::PlaceMonsterZone)return local==host;
    if(!zone_transition(local.zone_before,local.zone_after)||!zone_transition(host.zone_before,host.zone_after)||
       !zone_shared_state(local.zone_before,host.zone_before)||!zone_shared_state(local.zone_after,host.zone_after))return false;
    // Compare every remaining field without replacing the actual per-peer receipt.
    auto left=local,right=host;left.zone_before={};left.zone_after={};right.zone_before={};right.zone_after={};
    return left==right;
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
#include "dungeon_building_codec.inc"
bool valid_message(const Message& m,std::string& error) {
    if(m.evidence.building_clearance){
        const auto& request=m.request;const auto& proof=*m.evidence.building_clearance;
        const bool history=request.action==Action::UndoBuilding||request.action==Action::RedoBuilding;
        if(request.dungeon_building||!(request.action==Action::PlaceBuilding||history)||
           request.canonical_record_key!="action:"+(history?request.target_action_uuid:request.action_uuid)||
           m.kind==Kind::Request||m.kind==Kind::RequestRejected||m.outcome==Outcome::RejectedNoEffect){
            error="Building clearance proof is outside its exact original placement/history owner";return false;}
        auto forward=proof;
        if(history&&!m.evidence.object_after.alive){
            std::swap(forward.removed,forward.created);
            for(auto& change:forward.changed)std::swap(change.before,change.after);
        }
        if(!forward.created.empty()||!scenery_stroke_result_valid(forward,true)){
            error="Building clearance canonical original Scenery effect is invalid";return false;}
        auto base=std::make_unique<Message>(m);base->evidence.building_clearance.reset();
        return valid_message(*base,error);
    }
    if(m.request.dungeon_building){
        if(!dungeon_building_request(m.request)||(m.evidence.dungeon_building_effect&&!dungeon_building_execution(m.request,m.evidence))){
            error="Dungeon Building request/effect Scene, mapping or original authored state differs";return false;}
        if((m.kind==Kind::Request||m.kind==Kind::RequestRejected||m.outcome==Outcome::RejectedNoEffect)&&m.evidence.dungeon_building_effect){
            error="Dungeon Building no-effect/request packet carries an original mutation proof";return false;}
        if(m.outcome==Outcome::Confirmed&&!m.evidence.dungeon_building_effect){error="Dungeon Building confirmed result omitted original Scene proof";return false;}
        auto base=std::make_unique<Message>(m);base->request.dungeon_building=false;base->request.dungeon_scene.clear();base->evidence.dungeon_building_effect.reset();
        return valid_message(*base,error);
    }
    if(!m.request.dungeon_scene.empty()||m.evidence.dungeon_building_effect){error="Dungeon Building state outside its exact request owner";return false;}
    if(m.evidence.npc_quest_removal&&(m.request.action!=Action::RemoveNPC||
       !npc_quest_removal_valid(*m.evidence.npc_quest_removal,m.request.canonical_record_key))){
        error="NPC Quest retirement evidence target or complete records invalid";return false;}
    if((!modular_action(m.request)&&(m.evidence.modular_before||m.evidence.modular_after||m.evidence.modular_result))||bool(m.evidence.modular_before)!=bool(m.evidence.modular_after)||bool(m.evidence.modular_before)!=bool(m.evidence.modular_result)||
       (m.evidence.modular_before&&(!scenery_stroke_state_valid(*m.evidence.modular_before)||!scenery_stroke_state_valid(*m.evidence.modular_after)))){error="Modular evidence owner/pair/state differs";return false;}
    if(m.evidence.modular_result){std::string cause;if(!game::modular_transition::valid(*m.evidence.modular_result,[](const auto& value){return key(value);},cause)){error=cause;return false;}}
    if((!world_path_action(m.request.action)&&!world_path_history(m.request)&&
        (m.evidence.world_path_before||m.evidence.world_path_after))||
       bool(m.evidence.world_path_before)!=bool(m.evidence.world_path_after)||
       (m.evidence.world_path_before&&!world_path_state_valid(*m.evidence.world_path_before))||
       (m.evidence.world_path_after&&!world_path_state_valid(*m.evidence.world_path_after))){
        error="Wall/Pass evidence owner, pair or complete state invalid";return false;}
    if((m.request.action!=Action::PlaceBoss&&(m.evidence.boss_placement_before||m.evidence.boss_placement_after))||
       bool(m.evidence.boss_placement_before)!=bool(m.evidence.boss_placement_after)||
       (m.evidence.boss_placement_before&&!game::boss_placement_state::valid(*m.evidence.boss_placement_before))||
       (m.evidence.boss_placement_after&&!game::boss_placement_state::valid(*m.evidence.boss_placement_after))){error="Boss placement evidence exact owner/pair/state invalid";return false;}
    if((!group_object_action(m.request.action)&&!group_object_history(m.request)&&(m.evidence.group_object_before||m.evidence.group_object_after))||
       bool(m.evidence.group_object_before)!=bool(m.evidence.group_object_after)||
       (m.evidence.group_object_before&&!group_object_state_valid(*m.evidence.group_object_before))||
       (m.evidence.group_object_after&&!group_object_state_valid(*m.evidence.group_object_after))){error="Group object evidence owner/pair/state invalid";return false;}
    if((!scenery_stroke_action(m.request.action)&&!scenery_stroke_history(m.request)&&(m.evidence.scenery_stroke_before||m.evidence.scenery_stroke_after||m.evidence.scenery_stroke_result))||
       bool(m.evidence.scenery_stroke_before)!=bool(m.evidence.scenery_stroke_after)||bool(m.evidence.scenery_stroke_before)!=bool(m.evidence.scenery_stroke_result)||
       (m.evidence.scenery_stroke_before&&!scenery_stroke_state_valid(*m.evidence.scenery_stroke_before))||
       (m.evidence.scenery_stroke_after&&!scenery_stroke_state_valid(*m.evidence.scenery_stroke_after))||
       (m.evidence.scenery_stroke_result&&!scenery_stroke_result_valid(*m.evidence.scenery_stroke_result,scenery_stroke_history(m.request)))){error="Scenery stroke evidence owner/pair/state invalid";return false;}
    if((m.request.action!=Action::DuplicateSelection&&!duplicate_history(m.request)&&(m.evidence.duplicate_before||m.evidence.duplicate_after))||
       bool(m.evidence.duplicate_before)!=bool(m.evidence.duplicate_after)||
       (m.evidence.duplicate_before&&!duplicate_state_valid(*m.evidence.duplicate_before))||
       (m.evidence.duplicate_after&&!duplicate_state_valid(*m.evidence.duplicate_after))){error="Duplicate evidence pair/owner/state invalid";return false;}
    if(!duplicate_history(m.request)&&!scenery_stroke_history(m.request)&&!group_object_action(m.request.action)&&!group_object_history(m.request)&&!m.evidence.duplicate_cash.empty()){error="Duplicate history cash belongs to another action";return false;}
    if((m.request.action!=Action::EditGroup&&!group_history(m.request)&&(m.evidence.group_before||m.evidence.group_after))||
       bool(m.evidence.group_before)!=bool(m.evidence.group_after)||
       (m.evidence.group_before&&!group_state_valid(*m.evidence.group_before))||
       (m.evidence.group_after&&!group_state_valid(*m.evidence.group_after))){error="Group evidence pair/owner/state invalid";return false;}
    if((m.request.action!=Action::ReorderBossPhase&&(m.evidence.boss_phase_before||m.evidence.boss_phase_after))||
       bool(m.evidence.boss_phase_before)!=bool(m.evidence.boss_phase_after)||
       (m.evidence.boss_phase_before&&!boss_state_valid(*m.evidence.boss_phase_before))||
       (m.evidence.boss_phase_after&&!boss_state_valid(*m.evidence.boss_phase_after))){error="Boss phase evidence owner/pair/state invalid";return false;}
    if(m.request.action!=Action::EditZone&&!zone_edit_history(m.request)&&m.evidence.zone_edit){error="Foreign Zone edit evidence";return false;}
    if(m.request.action!=Action::DistrictStroke&&!district_stroke_history(m.request)&&m.evidence.district_stroke){error="Foreign District stroke evidence";return false;}
    if(m.request.action!=Action::EditDistrict&&m.evidence.district_continent_height){error="Foreign District property continent witness";return false;}
    if((!district_property_history(m.request)&&m.request.action!=Action::EditDistrict&&m.request.action!=Action::EditDistrictText&&m.request.action!=Action::MoveDistrictTitle&&(m.evidence.district_before||m.evidence.district_after))||
       bool(m.evidence.district_before)!=bool(m.evidence.district_after)||
       (m.evidence.district_before&&!district_state_valid(*m.evidence.district_before))||
       (m.evidence.district_after&&!district_state_valid(*m.evidence.district_after))){error="District evidence pair/owner/state invalid";return false;}
    if((m.request.action!=Action::MoveSelection&&!selection_history(m.request)&&(m.evidence.selection_move_before||m.evidence.selection_move_after))||
       bool(m.evidence.selection_move_before)!=bool(m.evidence.selection_move_after)||
       (m.evidence.selection_move_before&&!selection_move_state_valid(*m.evidence.selection_move_before))||
       (m.evidence.selection_move_after&&!selection_move_state_valid(*m.evidence.selection_move_after))){error="Selection Move evidence pair/owner invalid";return false;}
    if((m.request.action!=Action::EditServiceMembership&&(m.evidence.service_membership_before||m.evidence.service_membership_after))||
       bool(m.evidence.service_membership_before)!=bool(m.evidence.service_membership_after)||
       (m.evidence.service_membership_before&&!service_state_valid(*m.evidence.service_membership_before))||
       (m.evidence.service_membership_after&&!service_state_valid(*m.evidence.service_membership_after))){error="Service membership evidence pair/owner invalid";return false;}
    if((m.request.action!=Action::EditPropText&&(m.evidence.prop_text_before||m.evidence.prop_text_after))||
       bool(m.evidence.prop_text_before)!=bool(m.evidence.prop_text_after)||
       (m.evidence.prop_text_before&&!prop_text_state_valid(*m.evidence.prop_text_before))||
       (m.evidence.prop_text_after&&!prop_text_state_valid(*m.evidence.prop_text_after))){
        error="Prop text evidence owner/pair/state invalid";return false;
    }
    if((m.request.action!=Action::EditNpcProperty&&(m.evidence.npc_property_before||m.evidence.npc_property_after))||
       bool(m.evidence.npc_property_before)!=bool(m.evidence.npc_property_after)||
       (m.evidence.npc_property_before&&!npc_property_state_valid(*m.evidence.npc_property_before))||
       (m.evidence.npc_property_after&&!npc_property_state_valid(*m.evidence.npc_property_after))){
        error="NPC property evidence outside action domain";return false;}

    if((m.request.action!=Action::Intervention&&(m.evidence.intervention_before||m.evidence.intervention_after))||m.evidence.intervention_before.has_value()!=m.evidence.intervention_after.has_value()||
       (m.evidence.intervention_before&&(!game::interventions::valid(*m.evidence.intervention_before)||!game::interventions::valid(*m.evidence.intervention_after)))){error="Intervention evidence outside complete original action domain";return false;}
    if((m.request.action!=Action::GameRules&&(m.evidence.game_rules_before||m.evidence.game_rules_after))||m.evidence.game_rules_before.has_value()!=m.evidence.game_rules_after.has_value()||
       (m.evidence.game_rules_before&&(!game::game_rules::valid(*m.evidence.game_rules_before)||!game::game_rules::valid(*m.evidence.game_rules_after)))){error="Design rule evidence outside complete action domain";return false;}
    if((m.request.action!=Action::GmPolicy&&(m.evidence.gm_policy_before||m.evidence.gm_policy_after))||m.evidence.gm_policy_before.has_value()!=m.evidence.gm_policy_after.has_value()||
       (m.evidence.gm_policy_before&&(!game::gm_policy::valid(*m.evidence.gm_policy_before)||!game::gm_policy::valid(*m.evidence.gm_policy_after)))){error="GM policy evidence outside complete action domain";return false;}
    if((m.request.action!=Action::ReleaseCycle&&(m.evidence.release_cycle_before||m.evidence.release_cycle_after||m.evidence.release_cycle_value))||m.evidence.release_cycle_before.has_value()!=m.evidence.release_cycle_after.has_value()||m.evidence.release_cycle_before.has_value()!=m.evidence.release_cycle_value.has_value()||
       (m.evidence.release_cycle_before&&(!game::release_cycle::valid(*m.evidence.release_cycle_before)||!game::release_cycle::valid(*m.evidence.release_cycle_after)||!game::release_cycle::valid(*m.evidence.release_cycle_value,error)))){error="Release cycle evidence outside complete action domain";return false;}
    if((m.request.action!=Action::ReleaseEdit&&(m.evidence.release_edit_before||m.evidence.release_edit_after))||m.evidence.release_edit_before.has_value()!=m.evidence.release_edit_after.has_value()||
       (m.evidence.release_edit_before&&(!game::release_edit::valid(*m.evidence.release_edit_before)||!game::release_edit::valid(*m.evidence.release_edit_after)))){error="Release edit evidence outside complete action domain";return false;}
    if((m.request.action!=Action::GeneralProject&&(m.evidence.general_project_before||m.evidence.general_project_after))||m.evidence.general_project_before.has_value()!=m.evidence.general_project_after.has_value()||
       (m.evidence.general_project_before&&(!general_project_valid(*m.evidence.general_project_before)||!general_project_valid(*m.evidence.general_project_after)))){error="General project evidence outside complete action domain";return false;}
    if((m.request.action!=Action::SetLogo&&(m.evidence.logo_before||m.evidence.logo_after))||m.evidence.logo_before.has_value()!=m.evidence.logo_after.has_value()||
       (m.evidence.logo_before&&(!valid_logo(*m.evidence.logo_before)||!valid_logo(*m.evidence.logo_after)))){error="Logo evidence outside complete action domain";return false;}
    if((m.request.action!=Action::RemoveNetworkObject&&!network_history(m.request)&&(m.evidence.network_removal_before||m.evidence.network_removal_after))||
       m.evidence.network_removal_before.has_value()!=m.evidence.network_removal_after.has_value()||
       (m.evidence.network_removal_before&&(!valid_network_removal(*m.evidence.network_removal_before)||!valid_network_removal(*m.evidence.network_removal_after)))){error="Network removal evidence outside complete action domain";return false;}
    if((m.evidence.network_history&&!network_history(m.request))||(network_history(m.request)&&(m.evidence.network_history.has_value()!=m.evidence.network_removal_before.has_value()))){error="Network history proof outside complete action domain";return false;}
    if((m.request.action!=Action::ManagementCommand&&(m.evidence.management_before||m.evidence.management_after))||
       m.evidence.management_before.has_value()!=m.evidence.management_after.has_value()||
       (m.evidence.management_before&&(!management_valid(*m.evidence.management_before)||!management_valid(*m.evidence.management_after)))){error="Management evidence outside action domain";return false;}
    if((m.request.action!=Action::SetBuildingStatus&&(m.evidence.building_status_before!=BuildingStatusState{}||m.evidence.building_status_after!=BuildingStatusState{}))||
       !valid_building_status_state(m.evidence.building_status_before)||!valid_building_status_state(m.evidence.building_status_after)||
       ((m.evidence.building_status_before==BuildingStatusState{})!=(m.evidence.building_status_after==BuildingStatusState{}))){error="Building status evidence outside action domain";return false;}
    if((m.request.action!=Action::SetDungeonLighting&&(m.evidence.dungeon_light_before!=DungeonLightState{}||m.evidence.dungeon_light_after!=DungeonLightState{}))||
       !valid_dungeon_light_state(m.evidence.dungeon_light_before)||!valid_dungeon_light_state(m.evidence.dungeon_light_after)||
       ((m.evidence.dungeon_light_before==DungeonLightState{})!=(m.evidence.dungeon_light_after==DungeonLightState{}))){error="Dungeon light edit evidence outside action domain";return false;}
    if((m.request.action!=Action::EditDungeonGizmo&&(m.evidence.dungeon_gizmo_before!=DungeonGizmoState{}||m.evidence.dungeon_gizmo_after!=DungeonGizmoState{}))||
       !valid_dungeon_gizmo_state(m.evidence.dungeon_gizmo_before)||!valid_dungeon_gizmo_state(m.evidence.dungeon_gizmo_after)||
       ((m.evidence.dungeon_gizmo_before==DungeonGizmoState{})!=(m.evidence.dungeon_gizmo_after==DungeonGizmoState{}))){error="Gizmo edit evidence outside action domain";return false;}
    if((m.request.action!=Action::DungeonInput&&!dungeon_input_history(m.request)&&
            (m.evidence.dungeon_input_before!=DungeonInputState{}||m.evidence.dungeon_input_after!=DungeonInputState{}||m.evidence.dungeon_input_origin))||
       !valid_dungeon_input_state(m.evidence.dungeon_input_before)||!valid_dungeon_input_state(m.evidence.dungeon_input_after)||
       !valid_dungeon_sound_state(m.evidence.dungeon_input_before.protected_state)||!valid_dungeon_sound_state(m.evidence.dungeon_input_after.protected_state)||
       ((m.evidence.dungeon_input_before==DungeonInputState{})!=(m.evidence.dungeon_input_after==DungeonInputState{}))||
       (m.evidence.dungeon_input_origin&&(!dungeon_input_history(m.request)||!valid_dungeon_input(*m.evidence.dungeon_input_origin)))||
       (dungeon_input_history(m.request)&&((m.evidence.dungeon_input_before!=DungeonInputState{})!=m.evidence.dungeon_input_origin.has_value()))){
        error="Dungeon input evidence outside its exact tool/history owner or complete pair";return false;}
    if((m.request.action!=Action::SetDungeonPalette&&!dungeon_tiles_history(m.request)&&(m.evidence.dungeon_palette_before!=DungeonPaletteState{}||m.evidence.dungeon_palette_after!=DungeonPaletteState{}))||
       !valid_dungeon_palette_state(m.evidence.dungeon_palette_before)||!valid_dungeon_palette_state(m.evidence.dungeon_palette_after)||
       ((m.evidence.dungeon_palette_before==DungeonPaletteState{})!=(m.evidence.dungeon_palette_after==DungeonPaletteState{}))){error="Dungeon palette edit evidence outside action domain";return false;}
    if((m.request.action!=Action::SetDungeonAtmosphere&&(m.evidence.dungeon_sound_before!=DungeonSoundState{}||m.evidence.dungeon_sound_after!=DungeonSoundState{}))||
       !valid_dungeon_sound_state(m.evidence.dungeon_sound_before)||!valid_dungeon_sound_state(m.evidence.dungeon_sound_after)||
       ((m.evidence.dungeon_sound_before==DungeonSoundState{})!=(m.evidence.dungeon_sound_after==DungeonSoundState{}))){error="Dungeon sound edit evidence outside action domain";return false;}
    if((m.request.action!=Action::ConfigureDungeonInstances&&(m.evidence.dungeon_config_before!=DungeonConfigState{}||m.evidence.dungeon_config_after!=DungeonConfigState{}))||
       !valid_dungeon_config_state(m.evidence.dungeon_config_before)||!valid_dungeon_config_state(m.evidence.dungeon_config_after)||
       ((m.evidence.dungeon_config_before==DungeonConfigState{})!=(m.evidence.dungeon_config_after==DungeonConfigState{}))){error="Dungeon config edit evidence outside action domain";return false;}
    if((m.request.action!=Action::EditAbilityMembership&&(m.evidence.ability_membership_before!=AbilityMembershipState{}||m.evidence.ability_membership_after!=AbilityMembershipState{}))||
       !valid_ability_membership_state(m.evidence.ability_membership_before)||!valid_ability_membership_state(m.evidence.ability_membership_after)||
       ((m.evidence.ability_membership_before==AbilityMembershipState{})!=(m.evidence.ability_membership_after==AbilityMembershipState{}))){error="Ability membership evidence outside action domain";return false;}
    if((m.request.action!=Action::EditCharacterMembership&&(m.evidence.character_membership_before!=CharacterMembershipState{}||m.evidence.character_membership_after!=CharacterMembershipState{}))||
       !valid_character_membership_state(m.evidence.character_membership_before)||!valid_character_membership_state(m.evidence.character_membership_after)||
       ((m.evidence.character_membership_before==CharacterMembershipState{})!=(m.evidence.character_membership_after==CharacterMembershipState{}))){error="Character membership evidence outside action domain";return false;}
    if((m.request.action!=Action::EditCharacterCostume&&(m.evidence.costume_edit_before!=CostumeEditState{}||m.evidence.costume_edit_after!=CostumeEditState{}))||
       !valid_costume_edit_state(m.evidence.costume_edit_before)||!valid_costume_edit_state(m.evidence.costume_edit_after)||
       ((m.evidence.costume_edit_before==CostumeEditState{})!=(m.evidence.costume_edit_after==CostumeEditState{}))){error="Costume edit evidence outside action domain";return false;}
    if((m.request.action!=Action::EditCharacterAbility&&(m.evidence.ability_edit_before!=AbilityEditState{}||m.evidence.ability_edit_after!=AbilityEditState{}))||
       !valid_ability_edit_state(m.evidence.ability_edit_before)||!valid_ability_edit_state(m.evidence.ability_edit_after)||
       ((m.evidence.ability_edit_before==AbilityEditState{})!=(m.evidence.ability_edit_after==AbilityEditState{}))){error="Ability edit evidence outside action domain";return false;}
    if(((m.request.action<Action::RenameCharacter||m.request.action>Action::SetCharacterSpeed)&&(m.evidence.character_rename_before!=CharacterRenameState{}||m.evidence.character_rename_after!=CharacterRenameState{}))||
       !valid_character_rename_state(m.evidence.character_rename_before)||!valid_character_rename_state(m.evidence.character_rename_after)||
       ((m.evidence.character_rename_before==CharacterRenameState{})!=(m.evidence.character_rename_after==CharacterRenameState{}))){error="Character rename evidence outside action domain";return false;}
    if(((m.request.action!=Action::SetNpcQuestList&&m.request.action!=Action::EditNpcQuest&&!quest_history(m.request))&&(m.evidence.quest_list_before!=NpcQuestListState{}||m.evidence.quest_list_after!=NpcQuestListState{}))||
       !valid_npc_quest_list_state(m.evidence.quest_list_before)||!valid_npc_quest_list_state(m.evidence.quest_list_after)||
       ((m.evidence.quest_list_before==NpcQuestListState{})!=(m.evidence.quest_list_after==NpcQuestListState{}))){
        error="NPC Quest list evidence outside action domain";return false;}
    if((m.request.action!=Action::SetNpcQuestDestination&&(m.evidence.quest_before!=NpcQuestState{}||m.evidence.quest_after!=NpcQuestState{}))||
       !valid_npc_quest_state(m.evidence.quest_before)||!valid_npc_quest_state(m.evidence.quest_after)||
       ((m.evidence.quest_before==NpcQuestState{})!=(m.evidence.quest_after==NpcQuestState{}))){
        error="NPC Quest edit evidence outside action domain";return false;}
    if(m.evidence.npc_history_proof&&(!npc_history(m.request)||!valid_npc_history(*m.evidence.npc_history_proof))){error="NPC history proof outside its exact action domain";return false;}
    if((m.evidence.monster_history_proof&&!monster_history(m.request))||(m.evidence.monster_history_proof&&!valid_monster_history(*m.evidence.monster_history_proof))){error="Monster history proof outside its exact action domain";return false;}
    if(m.request.action!=Action::RemoveMonster&&m.evidence.monster_quests!=MonsterQuestRetirement{}){
        error="Monster Quest retirement evidence requires RemoveMonster";return false;}
    if(m.request.action==Action::RemoveMonster&&!valid_monster_quests(m.evidence.monster_quests,m.request.canonical_record_key,
        m.evidence.monster_before!=MonsterState{})){
        error="Monster Quest retirement evidence differs";return false;}
    if((m.request.action!=Action::PlaceMonster&&m.request.action!=Action::MoveMonster&&m.request.action!=Action::RemoveMonster&&!monster_history(m.request)&&(m.evidence.monster_before!=MonsterState{}||m.evidence.monster_after!=MonsterState{}))||
       !valid_monster_state(m.evidence.monster_before)||!valid_monster_state(m.evidence.monster_after)){
        error="Monster evidence outside action domain";return false;}

    if(!research_action(m.request.action)&&(m.evidence.research_before||m.evidence.research_after)){error="Research evidence outside Research action";return false;}
    if(m.evidence.research_before.has_value()!=m.evidence.research_after.has_value()){error="Research action evidence requires both rows";return false;}
    for(const auto* row:{&m.evidence.research_before,&m.evidence.research_after})
        if(*row&&!game::tutorial_replication::research::valid(**row,error))return false;
    if(m.request.action!=Action::SetBugFix&&(m.evidence.bug_before||m.evidence.bug_after)){
        error="Bug evidence outside SetBugFix action";return false;}
    if(m.evidence.bug_before.has_value()!=m.evidence.bug_after.has_value()){
        error="Bug Fix evidence requires both selected rows";return false;}
    for(const auto* row:{&m.evidence.bug_before,&m.evidence.bug_after})
        if(*row&&!bugfix::valid(bugfix::State{{**row}},error))return false;
    if((m.request.action!=Action::PlaceMonsterZone&&(m.evidence.zone_before!=ZoneState{}||m.evidence.zone_after!=ZoneState{}))||
       !valid_zone_state(m.evidence.zone_before)||!valid_zone_state(m.evidence.zone_after)){
        error="Zone evidence outside action domain";return false;
    }
    if(m.request.action!=Action::SetBugFix&&m.request.action!=Action::HireDeveloper&&m.request.action!=Action::FireDeveloper&&(m.evidence.developers_before||m.evidence.developers_after)){
        error="Developer pool evidence outside Hire/Fire action";return false;}
    if(m.evidence.developers_before.has_value()!=m.evidence.developers_after.has_value()){
        error="Developer action evidence requires both full pools";return false;}
    for(const auto* pool:{&m.evidence.developers_before,&m.evidence.developers_after})
        if(*pool&&!game::developer_staff::valid(**pool,error))return false;
    if(m.request.action!=Action::CommitStartingpoint&&m.request.action!=Action::StartStartingpoint&&m.request.action!=Action::SetStartingpointWorkers&&(m.evidence.startingpoint_before||m.evidence.startingpoint_after)){
        error="startingpoint evidence outside fixed tutorial action";return false;
    }
    if(m.evidence.startingpoint_before.has_value()!=m.evidence.startingpoint_after.has_value()){
        error="startingpoint action evidence requires both rows";return false;
    }
    for(const auto* row:{&m.evidence.startingpoint_before,&m.evidence.startingpoint_after})
        if(*row&&!game::tutorial_replication::startingpoint::valid(**row,error))return false;
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
    if((m.request.action!=Action::TerrainStroke&&m.request.action!=Action::PlacePath&&m.request.action!=Action::RemovePath&&!path_history(m.request)&&!terrain_history(m.request)&&(m.evidence.terrain_before!=TerrainState{}||m.evidence.terrain_after!=TerrainState{}))||
       !valid_terrain_state(m.evidence.terrain_before)||!valid_terrain_state(m.evidence.terrain_after)){error="terrain evidence outside action domain";return false;}
    if((m.request.action!=Action::SetRegionPreset&&m.request.action!=Action::SetRegionStyles&&m.request.action!=Action::TerrainStroke&&m.request.action!=Action::PlacePath&&m.request.action!=Action::RemovePath&&m.request.action!=Action::EditRegionProperty&&!region_history(m.request)&&!path_history(m.request)&&!terrain_history(m.request)&&(m.evidence.region_before!=RegionState{}||m.evidence.region_after!=RegionState{}||!m.evidence.region_retired_sha256.empty()))||
       (!m.evidence.region_retired_sha256.empty()&&!digest(m.evidence.region_retired_sha256))) {
        error="region evidence outside action domain";return false;
    }
    if(m.request.action!=Action::EditRegionProperty&&!region_history(m.request)&&(m.evidence.property_before!=RegionPropertyState{}||m.evidence.property_after!=RegionPropertyState{})){
        error="region property evidence outside action domain";return false;
    }
    if(m.request.action!=Action::SetPrices&&
       (m.evidence.price_before!=PriceState{}||m.evidence.price_after!=PriceState{})) {
        error="price evidence outside action domain";return false;
    }
    if((m.request.action!=Action::PlaceNPC&&m.request.action!=Action::RemoveNPC&&m.request.action!=Action::MoveNPC&&!npc_history(m.request)&&
        (m.evidence.npc_before!=NpcState{}||m.evidence.npc_after!=NpcState{}))||
       !valid_npc_state(m.evidence.npc_before)||!valid_npc_state(m.evidence.npc_after)){
        error="NPC evidence outside action domain";return false;
    }
    if(!region_history(m.request)&&m.request.region_property.kind!=RegionPropertyKind::LevelRange&&m.request.region_property.kind!=RegionPropertyKind::Sale&&(m.evidence.property_before.actors||m.evidence.property_after.actors)){error="region actor level evidence outside level range or sale action";return false;}
    if((m.request.action==Action::EditRegionProperty||region_history(m.request))&&(m.evidence.region_before!=RegionState{}||m.evidence.region_after!=RegionState{})){
        error="region property action carries separate region evidence";return false;
    }
    if((m.request.action!=Action::PlacePath&&m.request.action!=Action::RemovePath&&!path_history(m.request)&&(m.evidence.path_before!=PathState{}||m.evidence.path_after!=PathState{}))||!valid_path_state(m.evidence.path_before)||!valid_path_state(m.evidence.path_after)){error="path evidence outside action domain";return false;}
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
nlohmann::json management_state_observation(const ManagementState& value){return management_state_json(value);}
nlohmann::json management_intent_observation(const ManagementIntent& value){return management_intent_json(value);}
nlohmann::json management_ledger_observation(const Ledger& value){return ledger_json(value);}
bool valid_bug_fix(const BugFixIntent& intent){return bug_fix_intent_valid(intent);}
bool dungeon_light_wire_fits(const Request& request,const DungeonLightState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& e=probe.evidence;e.before=e.after=money;e.retirement_confirmed=true;e.dungeon_light_before=e.dungeon_light_after=before;e.dungeon_light_after.light=request.dungeon_light.light;
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;if(bytes.size()>max_payload-256){error="Dungeon light full receipt exceeds existing action packet budget before effect";return false;}return true;
}
bool dungeon_gizmo_wire_fits(const Request& request,const DungeonGizmoState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& e=probe.evidence;e.before=e.after=money;e.retirement_confirmed=true;e.dungeon_gizmo_before=before;
    if(!dungeon_gizmo_project(request.dungeon_gizmo,before,e.dungeon_gizmo_after)){error="Gizmo original edit has no exact current native target/value";return false;}
    size_t reserve=256;
    if(before.live){for(const auto& scene:*before.live)reserve+=scene.rows.size()*160;}
    if(before.network){const auto& graph=*before.network;reserve+=graph.nodes.size()*128+graph.connections.size()*200;
        if(graph.managers)reserve+=graph.managers->size()*20;for(const auto& pipe:graph.pipes)reserve+=(pipe.current.size()+pipe.smoothing.size())*24;}
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;if(reserve>max_payload||bytes.size()>max_payload-reserve){error="Gizmo full receipt exceeds existing action packet budget before effect";return false;}return true;
}
bool ability_membership_wire_fits(const Request& request,const AbilityMembershipState& before,const Ledger& money,std::string& error){
    if(request.action!=Action::EditAbilityMembership||!valid_ability_membership_intent(request.ability_membership)||
       !valid_ability_membership_state(before)||before==AbilityMembershipState{}){error="Ability packet preflight requires its complete current owner state";return false;}
    const auto& intent=request.ability_membership;
    if((intent.command==AbilityMembershipCommand::Create&&before.slots.size()>=12)||
       ((intent.command==AbilityMembershipCommand::Remove||intent.command==AbilityMembershipCommand::Dock)&&
        (intent.slot<0||size_t(intent.slot)>=before.slots.size()))){error="Ability packet preflight current native slot/count differs";return false;}
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;
    probe.outcome=Outcome::Unknown;probe.cause="preflight";probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& e=probe.evidence;e.before=e.after=money;e.ability_membership_before=e.ability_membership_after=before;
    auto& after=e.ability_membership_after;
    if(intent.command==AbilityMembershipCommand::Copy){auto copied=intent.prefab;copied.text[0]=intent.copy_name;copied.level=intent.level;after.slots.push_back({std::move(copied),-1});}
    else if(intent.command==AbilityMembershipCommand::Create)after.slots.push_back({});
    else if(intent.command==AbilityMembershipCommand::Remove)after.slots.erase(after.slots.begin()+intent.slot);
    else {after.slots[size_t(intent.slot)].record.level=intent.level;after.slots[size_t(intent.slot)].display_slot=intent.display_slot;}
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;
    const size_t generated=intent.command==AbilityMembershipCommand::Create?20480:0;
    if(bytes.size()+generated+4096>max_payload){error="Ability complete current/future receipt exceeds existing action packet budget before native mutation";return false;}
    return true;
}
bool character_membership_wire_fits(const Request& request,const CharacterMembershipState& before,const Ledger& money,std::string& error){
    if(request.action!=Action::EditCharacterMembership||!valid_character_membership_intent(request.character_membership)||!valid_character_membership_state(before)||
       before==CharacterMembershipState{}||before.selected.kind!=request.character_membership.kind||before.selected.id!=request.character_membership.id||before.selected.deleted||before.created){
        error="Character membership packet preflight requires its valid current selected family";return false;}
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;
    probe.outcome=Outcome::Confirmed;probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& evidence=probe.evidence;evidence.before=evidence.after=money;evidence.retirement_confirmed=true;
    evidence.character_membership_before=evidence.character_membership_after=before;auto& after=evidence.character_membership_after;
    after.manager_sha=before.manager_sha==std::string(64,'f')?std::string(64,'e'):std::string(64,'f');
    after.global_random.fill(UINT64_MAX);after.release_edits.fill(INT32_MIN);
    if(request.character_membership.command==CharacterMembershipCommand::CreateVariant){const auto kind=size_t(before.selected.kind);
        const auto id=before.first_deleted[kind]<0?after.counts[kind]++:before.first_deleted[kind];after.first_deleted[kind]=-1;
        after.selected.variants.push_back(id);after.created=CharacterMembershipRow{before.selected.kind,id,before.selected.id,false,request.character_membership.name,{}};
    }else if(request.character_membership.command==CharacterMembershipCommand::Import){
        after.selected.name=std::string(1024,'"');after.import_art_sha=after.import_release_sha=std::string(64,'f');
    }else {after.selected.deleted=true;after.selected.variants.clear();after.retired_definitions=before.selected.variants;after.retired_definitions.push_back(before.selected.id);
        std::sort(after.retired_definitions.begin(),after.retired_definitions.end());after.undo_count=20;}
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;
    if(bytes.size()>max_payload-4096){error="Character membership complete family receipt exceeds existing action packet budget before native mutation";return false;}
    return true;
}
bool dungeon_input_wire_fits(const Request& request,const DungeonInputState& before,const Ledger& money,std::string& error,const DungeonInputIntent* origin,const DungeonInputBirth* restored,const std::vector<std::string>* control_names){
    try{
        if(!valid_request(request)||!valid_dungeon_input_state(before)||before==DungeonInputState{}||
           (dungeon_input_history(request)!=bool(origin))||(origin&&!valid_dungeon_input(*origin))){error="Dungeon input wire preflight owner/state/source differs";return false;}
        const auto& intent=origin?*origin:request.dungeon_input;
        const bool undo=request.action==Action::UndoBuilding;
        const bool redo=request.action==Action::RedoBuilding;
        if((restored&&(!redo||intent.tool==DungeonInputTool::Paint||intent.tool==DungeonInputTool::Gizmos))||
           (redo&&intent.tool!=DungeonInputTool::Paint&&intent.tool!=DungeonInputTool::Gizmos&&!restored)){
            error="Dungeon input Redo wire preflight requires its retained original birth";return false;
        }
        if(control_names&&(!intent.controls||intent.controls->command!=game::dungeon_gizmo_control::Command::Duplicate||undo||control_names->size()!=intent.controls->rows.size())){
            error="Gizmo Duplicate wire name bounds require the exact current output count";return false;}
        auto probe=std::make_unique<Message>();
        probe->kind=Kind::ApplyAck;probe->session=probe->revision=UINT64_MAX;probe->ack_peer=4;probe->required_mask=15;
        probe->outcome=Outcome::Confirmed;probe->request=request;
        probe->request.sequence=probe->request.host_revision=UINT64_MAX;
        auto& evidence=probe->evidence;evidence.before=evidence.after=money;
        evidence.dungeon_input_before=evidence.dungeon_input_after=before;
        if(origin)evidence.dungeon_input_origin=*origin;
        if(intent.controls){
            auto& rows=evidence.dungeon_input_after.edits;rows.emplace();
            if(game::dungeon_gizmo_control::mixed(*intent.controls)){
                evidence.dungeon_input_after.scenery_edits.emplace();evidence.dungeon_input_after.scenery_release=before.scenery_release;}
            using Command=game::dungeon_gizmo_control::Command;const auto command=intent.controls->command;
            const bool deleted=(undo&&command==Command::Duplicate)||(!undo&&command==Command::Destroy);
            if(!deleted||command==Command::Duplicate){
                for(size_t i=0;i<intent.controls->rows.size();++i){const auto& source=intent.controls->rows[i];
                    if(source.scenery){game::dungeon_gizmo_control::SceneryRow row{source.key,*source.scenery};
                        if(!undo){row.value.pose=source.after;if(command==Command::Duplicate)row.key=game::dungeon_gizmo_control::birth(source,redo?request.target_action_uuid:request.action_uuid,uint32_t(i));}
                        row.value.height=0xff7fffffu;evidence.dungeon_input_after.scenery_edits->push_back(std::move(row));continue;}
                    DungeonGizmoRow row{source.key,source.before};
                    if(!undo){game::dungeon_gizmo_control::pose(row.value,source.after);
                        if(command==Command::Duplicate){row.key=game::dungeon_gizmo_identity::birth(redo?request.target_action_uuid:request.action_uuid,uint32_t(i));
                            row.value.parent.clear();row.value.name=control_names?control_names->at(i):std::string(8192,'\1');
                            if(row.value.name.find('\0')!=std::string::npos){error="Gizmo Duplicate wire name bound contains NUL";return false;}}}
                    rows->push_back(std::move(row));}
            }
        }else if(undo)evidence.dungeon_input_after.birth.reset();
        else if(intent.tool!=DungeonInputTool::Paint){
            DungeonInputBirth born;
            if(restored){
                auto restored_state=before;restored_state.birth=*restored;
                if(!valid_dungeon_input_state(restored_state)||restored->tool!=intent.tool||restored->scene!=intent.scene){
                    error="Dungeon input retained Redo birth is outside its original scene/tool";return false;
                }
                born=*restored;
            }else{
                born.scene=intent.scene;born.tool=intent.tool;
                if(intent.tool==DungeonInputTool::Monster){
                    born.key="action:"+request.action_uuid;born.monster={0,3,intent.rank,1,intent.character_type,intent.level,
                        std::string(256,'\1'),intent.position,{1,1,1},intent.rotation};
                }else{
                    born.key=game::dungeon_gizmo_identity::birth(request.action_uuid,0);
                    born.gizmo.name=std::string(8192,'\1');born.gizmo.type=intent.type;born.gizmo.variant=intent.variant;
                    born.gizmo.position=intent.position;born.gizmo.rotation=intent.rotation;born.gizmo.scale=intent.scale;
                }
            }
            evidence.dungeon_input_after.birth=std::move(born);
        }
        // Serialize the same full envelope as the sender. Only counters and
        // original-call outputs that are not known yet need a width bound.
        evidence.dungeon_input_before.protected_state.dungeon.global_random.fill(UINT64_MAX);
        evidence.dungeon_input_after.protected_state.dungeon.global_random.fill(UINT64_MAX);
        evidence.dungeon_input_after.protected_state.dungeon.undo_count=20;
        auto document=dungeon_input_message_json(*probe);
        auto& ledger=document["evidence"]["after"];
        for(auto& entry:ledger.items()){
            if(entry.value().is_array())for(auto& number:entry.value())number=INT32_MIN;
            else if(entry.key()!="record_present")entry.value()=INT32_MIN;
        }
        auto& birth=document["evidence"]["dungeon_input_after"]["birth"];
        if(!birth.is_null()&&intent.tool!=DungeonInputTool::Monster){
            for(const auto* field:{"state","gold","loot","vendor_trash","keys","boss_keys","lock_id","edge_a","edge_b"})
                birth["value"][field]=INT32_MIN;
            birth["value"]["triggered"]=false;
        }
        if(intent.controls){auto& after=document["evidence"]["dungeon_input_after"];
            if(after.contains("scenery_release"))for(auto& counter:after["scenery_release"])counter=INT32_MIN;
            if(after.contains("edits"))for(auto& row:after["edits"]){
                for(const auto* field:{"state","gold","loot","vendor_trash","keys","boss_keys","lock_id","edge_a","edge_b"})row[field]=INT32_MIN;}
        }
        const auto required=document.dump().size();
        if(required>max_payload){error="Dungeon input full original proof requires "+std::to_string(required)+
            " bytes; action packet limit is "+std::to_string(max_payload)+" bytes before mutation";return false;}
        error.clear();return true;
    }catch(const std::exception& ex){error=std::string("Dungeon input wire preflight: ")+ex.what();return false;}
}
bool dungeon_palette_wire_fits(const Request& request,const DungeonPaletteState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& e=probe.evidence;e.before=e.after=money;e.retirement_confirmed=true;e.dungeon_palette_before=e.dungeon_palette_after=before;
    if(before.palette.size()<4){error="Dungeon palette preflight has fewer than four native channels";return false;}
    if(dungeon_tiles_history(request)){
        e.undo_confirmed=true;auto& after=e.dungeon_palette_after;
        if(after.protected_state.dungeon.undo_count>20||(request.action==Action::UndoBuilding&&!after.protected_state.dungeon.undo_count)){
            error="Dungeon cell history preflight Undo count differs";return false;}
        if(request.action==Action::UndoBuilding)--after.protected_state.dungeon.undo_count;
        else if(after.protected_state.dungeon.undo_count<20)++after.protected_state.dungeon.undo_count;
        after.stroke.dungeon_sha=before.stroke.dungeon_sha==std::string(64,'f')?std::string(64,'e'):std::string(64,'f');
    }else if(request.dungeon_palette.edit==DungeonPaletteEdit::Cells){
        e.undo_confirmed=true;
        auto& after=e.dungeon_palette_after;const auto& stroke=request.dungeon_palette.stroke;
        if(after.protected_state.dungeon.undo_count>20){error="Dungeon cell preflight Undo count exceeds original history limit";return false;}if(after.protected_state.dungeon.undo_count<20)++after.protected_state.dungeon.undo_count;
        after.stroke.dungeon_sha=before.stroke.dungeon_sha==std::string(64,'f')?std::string(64,'e'):std::string(64,'f');
        if(after.stroke.cells.size()!=stroke.cells.size()){error="Dungeon cell preflight indexed receipt differs from request";return false;}
        for(auto& row:after.stroke.cells){if(stroke.mode==DungeonCellMode::Close)row.value.open=false;
            else{if(!row.value.open&&after.defaults)for(size_t i=0;i<4;++i)after.palette[i]=stroke.theme_packed[i];
                row.value.open=true;row.value.theme=stroke.theme;for(size_t i=0;i<4;++i)row.value.colors[i]=after.palette[i];}}
    }else{e.dungeon_palette_after.defaults=request.dungeon_palette.defaults;for(size_t i=0;i<4;++i)e.dungeon_palette_after.palette[i]=request.dungeon_palette.packed[i];}
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;if(bytes.size()>max_payload-256){error="Dungeon palette full receipt exceeds existing action packet budget before effect";return false;}return true;
}
bool dungeon_sound_wire_fits(const Request& request,const DungeonSoundState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& e=probe.evidence;e.before=e.after=money;e.retirement_confirmed=true;e.dungeon_sound_before=e.dungeon_sound_after=before;e.dungeon_sound_after.atmosphere=request.dungeon_sound.atmosphere;
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;if(bytes.size()>max_payload-256){error="Dungeon sound full receipt exceeds existing action packet budget before effect";return false;}return true;
}
bool dungeon_config_effect(const Request& request,const Ledger& before,const Ledger& after,const DungeonConfigState& state){Ledger expected;return request.action==Action::ConfigureDungeonInstances&&dungeon_config_ledger(request.dungeon_config,state,before,expected)&&after==expected;}
bool dungeon_config_wire_fits(const Request& request,const DungeonConfigState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;
    probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;auto& e=probe.evidence;e.before=money;e.retirement_confirmed=true;
    if(!dungeon_config_ledger(request.dungeon_config,before,money,e.after)){error="Dungeon config original count/price/cash/ledger arithmetic rejected before effect";return false;}
    e.dungeon_config_before=e.dungeon_config_after=before;e.dungeon_config_after.desired=request.dungeon_config.desired;
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;if(bytes.size()>max_payload-256){error="Dungeon config full receipt exceeds existing packet budget before original effect";return false;}return true;
}
bool costume_edit_wire_fits(const Request& request,const CostumeEditState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;
    probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& evidence=probe.evidence;evidence.before=evidence.after=money;evidence.retirement_confirmed=true;
    evidence.costume_edit_before=before;evidence.costume_edit_after=before;evidence.costume_edit_after.record=request.costume_edit.record;
    if(request.costume_edit.weapon_category){if(!costume_weapon_after(before.weapon_categories,*request.costume_edit.weapon_category,evidence.costume_edit_after.weapon_categories)){error="Costume weapon checkbox is unchanged";return false;}}
    else{
    evidence.costume_edit_after.owner.edited=true;evidence.costume_edit_after.owner.rename_day=before.owner.day;
    if(!before.owner.edited){if(before.owner.release_edits[7]==INT32_MAX){error="Costume native release edit counter exhausted before original effect";return false;}++evidence.costume_edit_after.owner.release_edits[7];}
    }
    evidence.costume_edit_after.all_sha256=std::string(64,before.all_sha256==std::string(64,'a')?'b':'a');
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;
    // The original keeps authored names and scalar sets; only bounded release
    // counters, flags, day and four RNG words can grow in the final receipt.
    if(bytes.size()>max_payload-256){error="Costume full before/after receipt exceeds existing action packet budget before original effect";return false;}
    return true;
}
bool ability_edit_wire_fits(const Request& request,const AbilityEditState& before,const Ledger& money,std::string& error){
    Message probe;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.kind=Kind::ApplyAck;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;
    probe.request=request;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    auto& evidence=probe.evidence;evidence.before=evidence.after=money;evidence.retirement_confirmed=true;
    evidence.ability_edit_before=before;evidence.ability_edit_after=before;evidence.ability_edit_after.record=request.ability_edit.record;
    evidence.ability_edit_after.all_sha256=std::string(64,before.all_sha256==std::string(64,'a')?'b':'a');
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;
    // The original keeps authored names and scalar sets; only bounded release
    // counters, flags, day and four RNG words can grow in the final receipt.
    if(bytes.size()>max_payload-256){error="Ability full before/after receipt exceeds existing action packet budget before original effect";return false;}
    return true;
}
bool valid_bug_fix_effect(const Request& request,const Evidence& evidence){
    return request.action==Action::SetBugFix&&valid_request(request)&&bug_fix_effect(request,evidence);
}
#include "duplicate_budget.inc"
#include "group_history_budget.inc"
#include "duplicate_history_budget.inc"
bool valid_group(const GroupIntent& v){return group_intent_valid(v);}
bool valid_group_effect(const Request& r,const Evidence& e){return group_effect(r,e);}
bool valid_boss_placement(const BossPlacementIntent& intent){return game::boss_placement_reader::valid(intent);}
bool valid_boss_placement_effect(const Request& request,const Evidence& evidence){return boss_placement_effect(request,evidence);}
bool valid_world_path_effect(const Request& request,const Evidence& evidence){return world_path_effect(request,evidence);}
bool boss_placement_wire_fits(const Request& request,const Evidence& evidence,std::string& error){
    Message message;message.kind=Kind::ApplyAck;message.session=UINT64_MAX;message.revision=UINT64_MAX;message.required_mask=15;message.ack_peer=4;
    message.request=request;message.evidence=evidence;message.evidence.retirement_confirmed=true;message.outcome=Outcome::Confirmed;
    std::vector<uint8_t> payload;return encode(message,payload,error);
}
bool valid_modular(const ModularIntent& v){return modular_valid(v);}
bool valid_modular_effect(const Request& r,const Evidence& e){return modular_effect(r,e);}
bool valid_group_object(bool moving,const GroupObjectIntent& value){return group_object_intent_valid(moving,value);}
bool valid_group_object_effect(const Request& r,const Evidence& e){return group_object_effect(r,e);}
bool group_object_wire_fits(const Request& r,const Evidence& e,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.request=r;m.evidence=e;m.outcome=Outcome::Confirmed;
    m.evidence.retirement_confirmed=true;std::vector<uint8_t> payload;return encode(m,payload,error);
}
bool valid_scenery_stroke(const SceneryStrokeIntent& value){return scenery_stroke_intent_valid(value);}
bool valid_scenery_stroke_effect(const Request& r,const Evidence& e){return scenery_stroke_history(r)?scenery_stroke_history_effect(r,e):scenery_stroke_effect(r,e);}
bool scenery_stroke_wire_fits(const Request& r,const Evidence& e,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.request=r;m.evidence=e;m.outcome=Outcome::Confirmed;
    m.evidence.retirement_confirmed=true;std::vector<uint8_t> payload;return encode(m,payload,error);
}
bool group_wire_fits(const Request& r,const GroupState& before,const Ledger& money,std::string& error){
    try{Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
        m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;m.evidence.retirement_confirmed=true;
        m.evidence.group_before=before;m.evidence.group_after=group_budget_after(before,r.group,"group:action:"+r.action_uuid);m.evidence.undo_confirmed=m.evidence.selection_confirmed=true;
        std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
        if(bytes.size()>max_payload-256){error="Group complete result exceeds transport budget before mutation";return false;}return true;
    }catch(const std::exception& x){error=std::string("Group result preflight: ")+x.what();return false;}
}
bool valid_district_title(const DistrictTitleIntent& v){return district_title_valid(v);}
bool valid_district_title_effect(const Request& r,const Evidence& e){return district_title_effect(r,e);}
bool district_title_wire_fits(const Request& r,const DistrictState& before,const Ledger& money,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;m.evidence.retirement_confirmed=true;
    m.evidence.district_before=before;m.evidence.district_after=district_title_after(before,r.district_title);
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="District title complete result exceeds transport budget before mutation";return false;}return true;
}
bool valid_zone_edit_evidence(const Request& r,const Evidence& e){return zone_edit_effect(r,e);}
bool zone_edit_wire_fits(const Request& request,const Ledger& money,const ZoneEditState& before,std::string& error){
    auto message=std::make_unique<Message>();auto& m=*message;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;
    m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=request;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.retirement_confirmed=true;
    m.evidence.before=m.evidence.after=money;
    const bool history=request.action!=Action::EditZone||request.zone_edit.kind!=ZoneEditKind::Respawn;
    m.evidence.undo_confirmed=history;
    // Size preflight only. The actual original supplies all published values.
    auto after=before;after.counts.fill(UINT32_MAX);after.monsters=UINT32_MAX;
    after.global_random.fill(UINT64_MAX);after.region_random.fill(UINT64_MAX);
    after.next_tag=after.created_monsters=INT32_MAX;after.release_edits.fill(INT32_MIN);
    m.evidence.zone_edit=ZoneEditEffect{before,after,history?before.values_sha:std::string{},history?100000u:0u};
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="Zone edit complete proof exceeds transport budget before mutation";return false;}return true;
}
bool valid_district_stroke_effect(const Request& r,const Evidence& e){return district_stroke_effect(r,e);}
bool district_stroke_wire_fits(const Request& r,const Ledger& money,const DistrictStrokeState& before,std::string& error){
    auto storage=std::make_unique<Message>();auto& m=*storage;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.retirement_confirmed=true;
    m.evidence.before=m.evidence.after=money;m.evidence.undo_confirmed=true;
    // This is only a size preflight. The original operation supplies the real
    // after-state and inverse digest before any result is published.
    m.evidence.district_stroke=DistrictStrokeEffect{before,before,before.values_sha,100000};
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="District complete stroke exceeds transport budget before mutation";return false;}return true;
}
bool valid_district_property_effect(const Request& r,const Evidence& e){return district_property_history(r)?district_property_history_effect(r,e):district_property_effect(r,e);}
bool district_history_wire_fits(const Request& r,const Evidence& e,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;m.evidence=e;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.retirement_confirmed=true;
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="District original history proof exceeds transport budget before mutation";return false;}return true;
}
bool district_property_wire_fits(const Request& r,const DistrictState& before,const Ledger& money,uint32_t continent,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.retirement_confirmed=true;
    m.evidence.before=m.evidence.after=money;m.evidence.district_before=before;
    DistrictState after;if(!game::district_property::expected(before,r.district_property,continent,after,error))return false;
    m.evidence.district_after=std::move(after);m.evidence.district_continent_height=continent;
    m.evidence.undo_confirmed=game::district_property::field(r.district_property.kind)->original_history;
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="District property complete result exceeds transport budget before mutation";return false;}return true;
}
bool valid_district_text(const DistrictTextIntent& v){return district_intent_valid(v);}
bool valid_district_effect(const Request& r,const Evidence& e){return district_effect(r,e);}
bool district_wire_fits(const Request& r,const DistrictState& before,const Ledger& money,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;m.evidence.retirement_confirmed=true;
    m.evidence.district_before=m.evidence.district_after=before;
    auto& after=*m.evidence.district_after;(r.district_text.kind==DistrictTextKind::Name?after.name:after.description)=district_text_value(r.district_text);
    if(r.district_text.kind==DistrictTextKind::Name&&!after.manual_title_inside){after.flags[11]=0;after.manual_title_inside=true;}
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;if(bytes.size()>max_payload-256){error="District complete result exceeds transport budget before mutation";return false;}return true;
}
bool valid_selection_move(const SelectionMoveIntent& v){return selection_move_intent_valid(v);}
bool valid_selection_move_effect(const Request& r,const Evidence& e){return selection_move_effect(r,e);}
bool selection_move_wire_fits(const Request& r,const Ledger& money,std::string& error){
    Message m;m.kind=Kind::ApplyAck;m.session=UINT64_MAX;m.revision=UINT64_MAX;m.required_mask=15;m.ack_peer=4;m.outcome=Outcome::Confirmed;m.request=r;
    m.request.sequence=UINT64_MAX;m.request.host_revision=UINT64_MAX;m.evidence.before=m.evidence.after=money;m.evidence.retirement_confirmed=true;
    m.evidence.undo_confirmed=m.evidence.selection_confirmed=true;m.evidence.selection_move_before=SelectionMoveState{};m.evidence.selection_move_after=SelectionMoveState{};
    for(const auto& row:r.selection_move.rows){m.evidence.selection_move_before->rows.push_back(row.before);m.evidence.selection_move_after->rows.push_back(row.after);}
    std::vector<uint8_t> bytes;if(!encode(m,bytes,error))return false;if(bytes.size()>max_payload-256){error="Selection Move complete result exceeds transport budget before mutation";return false;}return true;
}
bool valid_service_membership_effect(const Request& r,const Evidence& e){return service_effect(r,e);}
bool service_membership_wire_fits(const Request& r,const ServiceMembershipState& before,const Ledger& money,std::string& error){
    ServiceMembershipState after;if(!service_membership_expected(r.service_membership,before,after)){error="Service membership cannot derive complete preflight effect";return false;}
    Message probe;probe.kind=Kind::ApplyAck;probe.session=UINT64_MAX;probe.revision=UINT64_MAX;probe.ack_peer=4;probe.required_mask=15;probe.outcome=Outcome::Confirmed;
    probe.request=r;probe.request.host_revision=UINT64_MAX;probe.request.sequence=UINT64_MAX;
    probe.evidence.before=probe.evidence.after=money;probe.evidence.retirement_confirmed=true;
    probe.evidence.service_membership_before=before;probe.evidence.service_membership_after=std::move(after);
    std::vector<uint8_t> bytes;if(!encode(probe,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="Service membership complete result exceeds existing action packet reserve";return false;}return true;
}
bool valid_boss_phase(const BossPhaseIntent& v){return boss_intent_valid(v);}
bool valid_boss_phase_effect(const Request& r,const Evidence& e){return boss_effect(r,e);}
bool valid_prop_text(const PropTextIntent& v){return prop_text_intent_valid(v);}
bool valid_prop_text_effect(const Request& r,const Evidence& e){return prop_text_effect(r,e);}
bool valid_npc_property(const NpcPropertyIntent& v){return npc_property_intent_valid(v);}
bool valid_npc_property_effect(const Request& r,const Evidence& e){return npc_property_effect(r,e);}
bool valid_npc(const NpcIntent& value) {return valid_npc_intent(value);}
bool valid_npc_history_effect(const Request& request,const Evidence& evidence) {return npc_history_effect(request,evidence);}
bool valid_npc_effect(const Request& request,const Evidence& evidence) {
    return (request.action==Action::PlaceNPC&&npc_effect(request,evidence))||
        (request.action==Action::RemoveNPC&&npc_remove_effect(evidence)&&
         (!evidence.npc_quest_removal||npc_quest_removal_valid(*evidence.npc_quest_removal,request.canonical_record_key)))||
        (request.action==Action::MoveNPC&&npc_move_effect(request,evidence));
}
bool valid_npc_quest_removal(const NpcQuestRemovalProof& proof,const std::string& target){return npc_quest_removal_valid(proof,target);}
bool npc_quest_removal_wire_fits(const Request& request,const NpcState& before,const Ledger& money,
                               const NpcQuestRemovalProof& proof,std::string& error){
    if(request.action!=Action::RemoveNPC||before.count==0||before.undo_count>=100000){error="NPC removal packet preflight requires a current actor and Undo capacity";return false;}
    auto probe=std::make_unique<Message>();probe->session=UINT64_MAX;probe->revision=UINT64_MAX;
    probe->kind=Kind::ApplyAck;probe->ack_peer=4;probe->required_mask=15;probe->outcome=Outcome::Confirmed;
    probe->request=request;probe->request.host_revision=UINT64_MAX;probe->request.sequence=UINT64_MAX;
    auto& e=probe->evidence;e.before=e.after=money;e.npc_before=e.npc_after=before;
    --e.npc_after.count;++e.npc_after.undo_count;e.npc_after.created={};
    e.npc_after.roster_sha256=std::string(64,before.roster_sha256==std::string(64,'a')?'b':'a');
    e.npc_quest_removal=proof;auto& predicted=e.npc_quest_removal->quests;
    if(predicted.removed.size()>predicted.before_count){error="NPC removal preflight retired Quest count exceeds its global pool";return false;}
    predicted.after_count=predicted.before_count-uint32_t(predicted.removed.size());predicted.completed_after=predicted.completed_before;
    for(const auto& row:predicted.removed)if(row.record.completed.total>0)--predicted.completed_after;
    predicted.after_sha256=predicted.removed.empty()?predicted.before_sha256:std::string(64,predicted.before_sha256==std::string(64,'a')?'b':'a');
    e.npc_quest_removal->references_after=std::string(64,'a');
    e.target_retired=e.undo_confirmed=e.selection_confirmed=e.retirement_confirmed=true;
    std::vector<uint8_t> bytes;if(!encode(*probe,bytes,error))return false;
    if(bytes.size()>max_payload-256){error="NPC complete Quest retirement receipt exceeds packet budget before original effect";return false;}
    return true;
}
bool encode(const Message& m,std::vector<uint8_t>& out,std::string& error) {
    out.clear();if(!valid_message(m,error))return false;
    try {
        const auto& r=m.request;const auto& e=m.evidence;const bool quests=npc_quest_schema(m)||e.npc_quest_removal.has_value();
        json j={{"schema",uint64_t(npc_history(r)?36:(r.action==Action::EditZone||zone_edit_history(r))?36:(r.action==Action::MoveMonster||r.action==Action::RemoveMonster||monster_history(r))?36:modular_action(r)?36:(r.action==Action::DungeonInput||dungeon_input_history(r))?36:(world_path_action(r.action)||r.action==Action::PlaceBoss)?35:(group_object_action(r.action)||group_object_history(r))?35:(scenery_stroke_action(r.action)||scenery_stroke_history(r))?35:dungeon_tiles_history(r)?35:terrain_history(r)?35:quest_history(r)?35:network_history(r)?35:duplicate_history(r)?35:group_history(r)?35:r.action==Action::DuplicateSelection?35:r.action==Action::Intervention?35:r.action==Action::GameRules?35:r.action==Action::GmPolicy?35:r.action==Action::ReleaseCycle?35:r.action==Action::ReleaseEdit?35:r.action==Action::EditGroup?35:path_history(r)?35:r.action==Action::EditDungeonGizmo?(r.dungeon_gizmo.edit==DungeonGizmoEdit::State?36:35):r.action==Action::SetDungeonPalette?35:r.action==Action::GeneralProject?35:region_history(r)?35:(r.action==Action::EditDistrict||district_property_history(r)||r.action==Action::DistrictStroke||district_stroke_history(r))?36:r.action==Action::MoveDistrictTitle?35:r.action==Action::EditDistrictText?35:r.action==Action::SetDungeonAtmosphere?35:selection_history(r)?35:r.action==Action::SetDungeonLighting?35:r.action==Action::MoveSelection?35:r.action==Action::ConfigureDungeonInstances?35:r.action==Action::SetLogo?35:r.action==Action::EditAbilityMembership?35:r.action==Action::EditCharacterMembership?(r.character_membership.command==CharacterMembershipCommand::Import?37:35):r.action==Action::EditCharacterCostume||r.action==Action::EditCharacterAbility?36:r.action==Action::RemoveNetworkObject?35:r.action==Action::SetBuildingStatus||(r.action>=Action::RenameCharacter&&r.action<=Action::SetCharacterSpeed)||r.action==Action::SetNpcQuestList||r.action==Action::EditNpcQuest?35:r.action==Action::ManagementCommand?35:r.action==Action::EditPropText?(!r.prop_text.gizmo_scene.empty()?36:35):r.action==Action::EditServiceMembership||r.action==Action::ReorderBossPhase||r.action==Action::EditNpcProperty?35:r.action==Action::SetNpcQuestDestination?34:r.action==Action::RemoveMonster?33:r.action==Action::MoveMonster?31:r.action==Action::PlaceMonster?30:r.action==Action::StartResearch?27:r.action==Action::SetResearchWorkers?28:r.action==Action::CommitResearch?29:r.action==Action::SetBugFix?26:r.action==Action::PlaceMonsterZone?25:r.action==Action::HireDeveloper?23:r.action==Action::FireDeveloper?24:r.action==Action::SetStartingpointWorkers?22:r.action==Action::StartStartingpoint?21:r.action==Action::CommitStartingpoint?20:quests?19:r.action==Action::MoveNPC?18:r.action==Action::RemoveNPC?17:r.action==Action::PlaceNPC?16:r.action==Action::SetPrices?13:r.action==Action::RemovePath?12:r.action==Action::EditRegionProperty?((appearance_kind(r.region_property.kind)||r.region_property.kind==RegionPropertyKind::Sale)?35:r.region_property.kind==RegionPropertyKind::LevelRange?15:11):r.action==Action::UndoBuilding||r.action==Action::RedoBuilding?10:r.action==Action::PlaceNetworkPipe?9:r.action==Action::PlaceNetworkNode?8:r.action==Action::PlacePath?(path_connected(r.path)?7:6):r.action==Action::SetRegionStyles?5:r.action==Action::TerrainStroke?(r.terrain.height?14:4):(r.action==Action::SetRegionPreset?3:(extended(m)?2:1)))},{"kind",uint64_t(m.kind)},{"session",m.session},{"revision",m.revision},
            {"ack_peer",m.ack_peer},{"required_mask",m.required_mask},{"outcome",uint64_t(m.outcome)},{"cause",m.cause},
            {"request",{{"action",uint64_t(r.action)},{"peer",r.peer},{"sequence",r.sequence},{"host_revision",r.host_revision},
                {"action_uuid",r.action_uuid},{"snapshot_uuid",r.snapshot_uuid},{"canonical_record_key",r.canonical_record_key}}},
            {"evidence",{{"before",ledger_json(e.before)},{"after",ledger_json(e.after)},{"target_retired",e.target_retired},
                {"undo_confirmed",e.undo_confirmed},{"selection_confirmed",e.selection_confirmed},{"retirement_confirmed",e.retirement_confirmed}}}};
        if(e.npc_quest_removal)j["schema"]=uint64_t(36);
        if(r.dungeon_building){j=dungeon_building_message_json(m);
        }else if(npc_history(r)){
            j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["npc_before"]=npc_state_json(e.npc_before,true);j["evidence"]["npc_after"]=npc_state_json(e.npc_after,true);
            j["evidence"]["npc_history"]=e.npc_history_proof?npc_history_json(*e.npc_history_proof):json(nullptr);
        }else if(monster_history(r)){
            j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["monster_before"]=monster_state_json(e.monster_before);j["evidence"]["monster_after"]=monster_state_json(e.monster_after);
            j["evidence"]["monster_history"]=e.monster_history_proof?monster_history_json(*e.monster_history_proof):json(nullptr);
        }else if(r.action==Action::DungeonInput||dungeon_input_history(r)){
            j=dungeon_input_message_json(m);
        }else if(world_path_action(r.action)||(world_path_history(r)&&e.world_path_before)){
            if(world_path_action(r.action))j["request"]["world_path"]=world_path_json(r.world_path);
            else j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["world_path_before"]=world_path_state_json(e.world_path_before);
            j["evidence"]["world_path_after"]=world_path_state_json(e.world_path_after);
        }else if(r.action==Action::PlaceBoss){
            j["request"]["boss_placement"]=boss_placement_json(r.boss_placement);
            j["evidence"]["boss_placement_before"]=boss_placement_state_json(e.boss_placement_before);
            j["evidence"]["boss_placement_after"]=boss_placement_state_json(e.boss_placement_after);
        }else if(modular_action(r)){if(r.action==Action::EditModularBuilding)j["request"]["modular"]=modular_json(r.modular);else j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["modular_before"]=scenery_stroke_state_json(e.modular_before);j["evidence"]["modular_after"]=scenery_stroke_state_json(e.modular_after);j["evidence"]["modular_result"]=modular_result_json(e.modular_result);
        }else if(group_object_action(r.action)||group_object_history(r)){
            if(group_object_action(r.action))j["request"]["group_object"]=group_object_json(r.group_object);else j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["group_object_before"]=group_object_state_json(e.group_object_before);j["evidence"]["group_object_after"]=group_object_state_json(e.group_object_after);
            j["evidence"]["duplicate_cash"]=duplicate_cash_json(e.duplicate_cash);
        }else if(scenery_stroke_action(r.action)){j["request"]["scenery_stroke"]=scenery_stroke_json(r.scenery_stroke);
            j["evidence"]["scenery_stroke_before"]=scenery_stroke_state_json(e.scenery_stroke_before);j["evidence"]["scenery_stroke_after"]=scenery_stroke_state_json(e.scenery_stroke_after);
            j["evidence"]["scenery_stroke_result"]=scenery_stroke_result_json(e.scenery_stroke_result);
        }else if(scenery_stroke_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["scenery_stroke_before"]=scenery_stroke_state_json(e.scenery_stroke_before);j["evidence"]["scenery_stroke_after"]=scenery_stroke_state_json(e.scenery_stroke_after);
            j["evidence"]["scenery_stroke_result"]=scenery_stroke_result_json(e.scenery_stroke_result);j["evidence"]["duplicate_cash"]=duplicate_cash_json(e.duplicate_cash);
        }else if(dungeon_tiles_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["dungeon_palette_before"]=dungeon_palette_state_json(e.dungeon_palette_before);j["evidence"]["dungeon_palette_after"]=dungeon_palette_state_json(e.dungeon_palette_after);
        }else if(duplicate_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["duplicate_before"]=duplicate_state_json(e.duplicate_before);j["evidence"]["duplicate_after"]=duplicate_state_json(e.duplicate_after);
            j["evidence"]["duplicate_cash"]=duplicate_cash_json(e.duplicate_cash);
        }else if(group_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["group_before"]=group_state_json(e.group_before);j["evidence"]["group_after"]=group_state_json(e.group_after);
        }else if(r.action==Action::DuplicateSelection){j["request"]["duplicate"]=duplicate_intent_json(r.duplicate);
            j["evidence"]["duplicate_before"]=duplicate_state_json(e.duplicate_before);j["evidence"]["duplicate_after"]=duplicate_state_json(e.duplicate_after);
        }else if(r.action==Action::Intervention){
            j["request"]["intervention"]=game::interventions::encode(r.intervention);
            j["evidence"]["intervention_before"]=e.intervention_before?game::interventions::encode(*e.intervention_before):json(nullptr);
            j["evidence"]["intervention_after"]=e.intervention_after?game::interventions::encode(*e.intervention_after):json(nullptr);
        }else if(r.action==Action::GameRules){
            j["request"]["game_rules"]=game::game_rules::encode(r.game_rules);
            j["evidence"]["game_rules_before"]=e.game_rules_before?game::game_rules::encode(*e.game_rules_before):json(nullptr);
            j["evidence"]["game_rules_after"]=e.game_rules_after?game::game_rules::encode(*e.game_rules_after):json(nullptr);
        }else if(r.action==Action::GmPolicy){
            j["request"]["gm_policy"]=game::gm_policy::encode(r.gm_policy);
            j["evidence"]["gm_policy_before"]=e.gm_policy_before?game::gm_policy::encode(*e.gm_policy_before):json(nullptr);
            j["evidence"]["gm_policy_after"]=e.gm_policy_after?game::gm_policy::encode(*e.gm_policy_after):json(nullptr);
        }else if(r.action==Action::ReleaseCycle){
            j["request"]["release_cycle"]=game::release_cycle::encode(r.release_cycle);
            j["evidence"]["release_cycle_before"]=e.release_cycle_before?game::release_cycle::encode(*e.release_cycle_before):json(nullptr);
            j["evidence"]["release_cycle_after"]=e.release_cycle_after?game::release_cycle::encode(*e.release_cycle_after):json(nullptr);
            j["evidence"]["release_cycle_value"]=e.release_cycle_value?game::release_cycle::encode(*e.release_cycle_value):json(nullptr);
        }else if(r.action==Action::ReleaseEdit){

            j["request"]["release_edit"]=game::release_edit::encode(r.release_edit);
            j["evidence"]["release_edit_before"]=e.release_edit_before?game::release_edit::encode(*e.release_edit_before):json(nullptr);
            j["evidence"]["release_edit_after"]=e.release_edit_after?game::release_edit::encode(*e.release_edit_after):json(nullptr);
        }else if(r.action==Action::EditGroup){j["request"]["group"]=group_intent_json(r.group);
            j["evidence"]["group_before"]=group_state_json(e.group_before);j["evidence"]["group_after"]=group_state_json(e.group_after);
        }else if(r.action==Action::GeneralProject){
            j["request"]["general_project"]=game::general_project::encode(r.general_project);
            j["evidence"]["general_project_before"]=e.general_project_before?game::general_project::encode(*e.general_project_before):json(nullptr);
            j["evidence"]["general_project_after"]=e.general_project_after?game::general_project::encode(*e.general_project_after):json(nullptr);
        }else if(r.action==Action::EditZone||zone_edit_history(r)){
            if(r.action==Action::EditZone)j["request"]["zone_edit"]=zone_edit_codec::intent_json(r.zone_edit);
            else j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["zone_edit"]=zone_edit_optional(e.zone_edit);
        }else if(r.action==Action::DistrictStroke||district_stroke_history(r)){
            if(r.action==Action::DistrictStroke)j["request"]["district_stroke"]=district_stroke_intent_json(r.district_stroke);
            else j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["district_stroke"]=district_stroke_effect_json(e.district_stroke);
        }else if(r.action==Action::EditDistrict||district_property_history(r)){if(r.action==Action::EditDistrict)j["request"]["district_property"]=district_property_json(r.district_property);else j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["district_before"]=district_state_json(e.district_before);j["evidence"]["district_after"]=district_state_json(e.district_after);
            j["evidence"]["district_continent_height"]=e.district_continent_height;
        }else if(r.action==Action::MoveDistrictTitle){j["request"]["district_title"]=r.district_title.position;
            j["evidence"]["district_before"]=district_state_json(e.district_before);j["evidence"]["district_after"]=district_state_json(e.district_after);
        }else if(r.action==Action::EditDistrictText){j["request"]["district_text"]=district_intent_json(r.district_text);
            j["evidence"]["district_before"]=district_state_json(e.district_before);j["evidence"]["district_after"]=district_state_json(e.district_after);
        }else if(r.action==Action::SetLogo){
            j["request"]["logo"]={{"before_sha256",r.logo.before_sha256},{"desired_sha256",r.logo.desired_sha256},{"records",r.logo.records}};
            j["evidence"]["logo_before"]=e.logo_before?logo_state_json(*e.logo_before):json(nullptr);
            j["evidence"]["logo_after"]=e.logo_after?logo_state_json(*e.logo_after):json(nullptr);
        }else if(r.action==Action::RemoveNetworkObject){
            j["request"]["network_remove"]={{"region",r.network_remove.region},{"graph_before_sha256",r.network_remove.graph_before_sha256},{"quoted_refund",r.network_remove.quoted_refund}};
            j["evidence"]["network_removal_before"]=e.network_removal_before?network_removal_json(*e.network_removal_before):json(nullptr);
            j["evidence"]["network_removal_after"]=e.network_removal_after?network_removal_json(*e.network_removal_after):json(nullptr);

        }else if(r.action==Action::SetBuildingStatus){j["request"]["building_status"]=building_status_intent_json(r.building_status);
            j["evidence"]["building_status_before"]=building_status_state_json(e.building_status_before);j["evidence"]["building_status_after"]=building_status_state_json(e.building_status_after);
        }else if(r.action==Action::SetDungeonLighting){j["request"]["dungeon_light"]=dungeon_light_intent_json(r.dungeon_light);
            j["evidence"]["dungeon_light_before"]=dungeon_light_state_json(e.dungeon_light_before);j["evidence"]["dungeon_light_after"]=dungeon_light_state_json(e.dungeon_light_after);
        }else if(r.action==Action::EditDungeonGizmo){j["request"]["dungeon_gizmo"]=dungeon_gizmo_intent_json(r.dungeon_gizmo);
            j["evidence"]["dungeon_gizmo_before"]=dungeon_gizmo_state_json(e.dungeon_gizmo_before);j["evidence"]["dungeon_gizmo_after"]=dungeon_gizmo_state_json(e.dungeon_gizmo_after);
        }else if(r.action==Action::SetDungeonPalette){j["request"]["dungeon_palette"]=dungeon_palette_intent_json(r.dungeon_palette);
            j["evidence"]["dungeon_palette_before"]=dungeon_palette_state_json(e.dungeon_palette_before);j["evidence"]["dungeon_palette_after"]=dungeon_palette_state_json(e.dungeon_palette_after);
        }else if(r.action==Action::SetDungeonAtmosphere){j["request"]["dungeon_sound"]=dungeon_sound_intent_json(r.dungeon_sound);
            j["evidence"]["dungeon_sound_before"]=dungeon_sound_state_json(e.dungeon_sound_before);j["evidence"]["dungeon_sound_after"]=dungeon_sound_state_json(e.dungeon_sound_after);
        }else if(r.action==Action::ConfigureDungeonInstances){j["request"]["dungeon_config"]=dungeon_config_intent_json(r.dungeon_config);
            j["evidence"]["dungeon_config_before"]=dungeon_config_state_json(e.dungeon_config_before);j["evidence"]["dungeon_config_after"]=dungeon_config_state_json(e.dungeon_config_after);
        }else if(r.action==Action::EditAbilityMembership){j["request"]["ability_membership"]=ability_membership_intent_json(r.ability_membership);
            j["evidence"]["ability_membership_before"]=ability_membership_state_json(e.ability_membership_before);j["evidence"]["ability_membership_after"]=ability_membership_state_json(e.ability_membership_after);
        }else if(r.action==Action::EditCharacterMembership){j["request"]["character_membership"]=character_membership_intent_json(r.character_membership);
            j["evidence"]["character_membership_before"]=character_membership_state_json(e.character_membership_before);j["evidence"]["character_membership_after"]=character_membership_state_json(e.character_membership_after);
        }else if(r.action==Action::EditCharacterCostume){j["request"]["costume_edit"]=costume_edit_intent_json(r.costume_edit);
            j["evidence"]["costume_edit_before"]=costume_edit_state_json(e.costume_edit_before);j["evidence"]["costume_edit_after"]=costume_edit_state_json(e.costume_edit_after);
        }else if(r.action==Action::EditCharacterAbility){j["request"]["ability_edit"]=ability_edit_intent_json(r.ability_edit);
            j["evidence"]["ability_edit_before"]=ability_edit_state_json(e.ability_edit_before);j["evidence"]["ability_edit_after"]=ability_edit_state_json(e.ability_edit_after);
        }else if((r.action>=Action::RenameCharacter&&r.action<=Action::SetCharacterSpeed)){j["request"]["character_rename"]=character_rename_intent_json(r.character_rename);
            j["evidence"]["character_rename_before"]=character_rename_state_json(e.character_rename_before);j["evidence"]["character_rename_after"]=character_rename_state_json(e.character_rename_after);
        }else if((r.action==Action::SetNpcQuestList||r.action==Action::EditNpcQuest)){
            j["request"]["npc_quest_list"]=npc_quest_list_intent_json(r.npc_quest_list);
            j["evidence"]["quest_list_before"]=npc_quest_list_state_json(e.quest_list_before);
            j["evidence"]["quest_list_after"]=npc_quest_list_state_json(e.quest_list_after);

        }else if(r.action==Action::ManagementCommand){
            j["request"]["management"]=management_intent_json(r.management);
            j["evidence"]["management_before"]=e.management_before?management_state_json(*e.management_before):json(nullptr);
            j["evidence"]["management_after"]=e.management_after?management_state_json(*e.management_after):json(nullptr);

        }else if(terrain_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["region_before"]=region_json(e.region_before);j["evidence"]["region_after"]=region_json(e.region_after);
            j["evidence"]["terrain_before"]=terrain_state_json(e.terrain_before);j["evidence"]["terrain_after"]=terrain_state_json(e.terrain_after);
            j["evidence"]["region_retired_sha256"]=e.region_retired_sha256;
        }else if(quest_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["quest_list_before"]=npc_quest_list_state_json(e.quest_list_before);
            j["evidence"]["quest_list_after"]=npc_quest_list_state_json(e.quest_list_after);
        }else if(network_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["network_removal_before"]=e.network_removal_before?network_removal_json(*e.network_removal_before):json(nullptr);
            j["evidence"]["network_removal_after"]=e.network_removal_after?network_removal_json(*e.network_removal_after):json(nullptr);
            j["evidence"]["network_history"]=e.network_history?json{{"source",e.network_history->source},{"amount",e.network_history->amount},{"junction",e.network_history->junction}}:json(nullptr);
        }else if(path_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["path_before"]=path_state_json(e.path_before);j["evidence"]["path_after"]=path_state_json(e.path_after);
            j["evidence"]["region_before"]=region_json(e.region_before);j["evidence"]["region_after"]=region_json(e.region_after);
            j["evidence"]["terrain_before"]=terrain_state_json(e.terrain_before);j["evidence"]["terrain_after"]=terrain_state_json(e.terrain_after);
        }else if(region_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["property_before"]=property_state_json(e.property_before);j["evidence"]["property_after"]=property_state_json(e.property_after);
            j["evidence"]["region_retired_sha256"]=e.region_retired_sha256;
        }else if(selection_history(r)){j["request"]["target_action_uuid"]=r.target_action_uuid;
            j["evidence"]["selection_move_before"]=selection_move_state_json(e.selection_move_before);j["evidence"]["selection_move_after"]=selection_move_state_json(e.selection_move_after);
        }else if(r.action==Action::MoveSelection){j["request"]["selection_move"]=selection_move_intent_json(r.selection_move);
            j["evidence"]["selection_move_before"]=selection_move_state_json(e.selection_move_before);j["evidence"]["selection_move_after"]=selection_move_state_json(e.selection_move_after);
        }else if(r.action==Action::EditServiceMembership){j["request"]["service_membership"]=service_intent_json(r.service_membership);

            j["evidence"]["service_membership_before"]=service_state_json(e.service_membership_before);j["evidence"]["service_membership_after"]=service_state_json(e.service_membership_after);
        }else if(r.action==Action::ReorderBossPhase){

            j["request"]["boss_phase"]=boss_intent_json(r.boss_phase);
            j["evidence"]["boss_phase_before"]=boss_state_json(e.boss_phase_before);
            j["evidence"]["boss_phase_after"]=boss_state_json(e.boss_phase_after);
        }else if(r.action==Action::EditPropText){
            j["request"]["prop_text"]=prop_text_intent_json(r.prop_text);
            j["evidence"]["prop_text_before"]=prop_text_state_json(e.prop_text_before);
            j["evidence"]["prop_text_after"]=prop_text_state_json(e.prop_text_after);
        }else if(r.action==Action::EditNpcProperty){
            j["request"]["npc_property"]=npc_property_intent_json(r.npc_property);
            j["evidence"]["npc_property_before"]=npc_property_state_json(e.npc_property_before);
            j["evidence"]["npc_property_after"]=npc_property_state_json(e.npc_property_after);
        }else if(r.action==Action::SetNpcQuestDestination){
            j["request"]["npc_quest"]=npc_quest_intent_json(r.npc_quest);
            j["evidence"]["quest_before"]=npc_quest_state_json(e.quest_before);
            j["evidence"]["quest_after"]=npc_quest_state_json(e.quest_after);
        }else if(r.action==Action::RemoveMonster){
            j["request"]["target_action_uuid"]=r.target_action_uuid;j["request"]["monster_remove"]={{"region",r.monster_remove.region}};
            j["evidence"]["monster_before"]=monster_state_json(e.monster_before);j["evidence"]["monster_after"]=monster_state_json(e.monster_after);
            j["evidence"]["monster_quests"]=monster_quests_json(e.monster_quests);
        }else if(r.action==Action::MoveMonster){
            j["request"]["target_action_uuid"]=r.target_action_uuid;j["request"]["monster_move"]=monster_move_json(r.monster_move);
            j["evidence"]["monster_before"]=monster_state_json(e.monster_before);j["evidence"]["monster_after"]=monster_state_json(e.monster_after);
        }else if(r.action==Action::PlaceMonster){
            j["request"]["monster"]=monster_intent_json(r.monster);
            j["evidence"]["monster_before"]=monster_state_json(e.monster_before);j["evidence"]["monster_after"]=monster_state_json(e.monster_after);
        }else if(research_action(r.action)){
            if(r.action==Action::SetResearchWorkers)j["request"]["desired_workers"]=r.desired_workers;
            namespace research=game::tutorial_replication::research;
            j["evidence"]["research_before"]=e.research_before?research::encode(*e.research_before):json(nullptr);
            j["evidence"]["research_after"]=e.research_after?research::encode(*e.research_after):json(nullptr);
        }else if(r.action==Action::SetBugFix){
            j["request"]["bug_fix"]=bug_fix_json(r.bug_fix);
            j["evidence"]["bug_before"]=bug_fix_row_json(e.bug_before);j["evidence"]["bug_after"]=bug_fix_row_json(e.bug_after);
            j["evidence"]["developers_before"]=e.developers_before?game::developer_staff::encode(*e.developers_before):json(nullptr);
            j["evidence"]["developers_after"]=e.developers_after?game::developer_staff::encode(*e.developers_after):json(nullptr);
        }else if(r.action==Action::PlaceMonsterZone){
            j["request"]["zone"]=zone_intent_json(r.zone);
            j["evidence"]["zone_before"]=zone_state_json(e.zone_before);j["evidence"]["zone_after"]=zone_state_json(e.zone_after);
        }else if(r.action==Action::HireDeveloper||r.action==Action::FireDeveloper){
            j["evidence"]["developers_before"]=e.developers_before?game::developer_staff::encode(*e.developers_before):json(nullptr);
            j["evidence"]["developers_after"]=e.developers_after?game::developer_staff::encode(*e.developers_after):json(nullptr);
        } else if(r.action==Action::CommitStartingpoint||r.action==Action::StartStartingpoint||r.action==Action::SetStartingpointWorkers){
            if(r.action==Action::SetStartingpointWorkers)j["request"]["desired_workers"]=r.desired_workers;
            namespace sp=game::tutorial_replication::startingpoint;
            j["evidence"]["startingpoint_before"]=e.startingpoint_before?sp::encode(*e.startingpoint_before):json(nullptr);
            j["evidence"]["startingpoint_after"]=e.startingpoint_after?sp::encode(*e.startingpoint_after):json(nullptr);
        } else if(r.action==Action::PlaceNPC||r.action==Action::RemoveNPC||r.action==Action::MoveNPC) {
            if(r.action==Action::PlaceNPC)j["request"]["npc"]=npc_intent_json(r.npc);
            else j["request"]["target_action_uuid"]=r.target_action_uuid;
            if(r.action==Action::MoveNPC)j["request"]["building"]=building_json(r.building);
            j["evidence"]["npc_before"]=npc_state_json(e.npc_before,quests);
            j["evidence"]["npc_after"]=npc_state_json(e.npc_after,quests);
            if(e.npc_quest_removal)j["evidence"]["npc_quest_removal"]=npc_quest_removal_json(*e.npc_quest_removal);
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
        if(e.building_clearance){j["schema"]=uint64_t(38);j["evidence"]["building_clearance"]=scenery_stroke_result_json(e.building_clearance);}
        const auto bytes=j.dump();if(bytes.size()>max_payload) {error="action payload exceeds "+std::to_string(max_payload)+" bytes";return false;}
        out.assign(bytes.begin(),bytes.end());error.clear();return true;
    }catch(const std::exception& e){error=std::string("action serialization: ")+e.what();return false;}
}
bool decode(const uint8_t* data,size_t length,Message& out,std::string& error) {
    if(!data||!length||length>max_payload) {error="action payload length requires 1.."+std::to_string(max_payload);return false;}
    try {
        std::vector<std::set<std::string>> fields;int deepest=0;
        auto j=json::parse(data,data+length,[&](int depth,json::parse_event_t event,json& value) {
            if(depth>10)throw std::runtime_error("action JSON nesting depth exceeds 10");
            deepest=std::max(deepest,depth);
            if(event==json::parse_event_t::object_start)fields.emplace_back();
            if(event==json::parse_event_t::key&&!fields.back().insert(value.get<std::string>()).second)throw std::runtime_error("duplicate JSON field");
            if(event==json::parse_event_t::object_end)fields.pop_back();
            return true;
        });
        exact(j,{"schema","kind","session","revision","ack_peer","required_mask","outcome","cause","request","evidence"});
        const bool clearance_packet=u64(j.at("schema"))==38;
        json clearance=nullptr;
        if(clearance_packet){
            const auto action=u64(j.at("request").at("action"));
            if((action!=2&&action!=10&&action!=11)||!j.at("evidence").contains("building_clearance")||
               j.at("evidence").at("building_clearance").is_null())
                throw std::runtime_error("Building clearance schema38 requires its exact placement/history proof");
            clearance=j["evidence"]["building_clearance"];j["evidence"].erase("building_clearance");
            j["schema"]=uint64_t(action==2?2:10);
        }
        const auto schema=u64(j.at("schema"));
        if(dungeon_building_packet(j)){if(schema!=37||deepest>10)throw std::runtime_error("Dungeon Building requires exact schema37/depth10");
            auto fresh=std::make_unique<Message>();if(!dungeon_building_message_read(j,*fresh,error)||!valid_message(*fresh,error))return false;
            out=std::move(*fresh);error.clear();return true;}
        if(npc_history_packet(j)){if(schema!=36||deepest>12)throw std::runtime_error("NPC history schema or nesting depth differs");auto fresh=std::make_unique<Message>();if(!npc_history_message_read(j,*fresh,error)||!valid_message(*fresh,error))return false;out=std::move(*fresh);error.clear();return true;}if(schema==36&&npc_quest_removal_packet(j)){Message fresh;if(!npc_quest_removal_message_read(j,fresh,error)||!valid_message(fresh,error))return false;out=std::move(fresh);error.clear();return true;}if(schema==36&&monster_edit_packet(j)){Message fresh;if(!monster_edit_message_read(j,fresh,error)||!valid_message(fresh,error))return false;out=std::move(fresh);error.clear();return true;}if(schema==36&&dungeon_input_packet(j)){Message fresh;if(!dungeon_input_message_read(j,fresh,error)||!valid_message(fresh,error))return false;out=std::move(fresh);error.clear();return true;}if(schema<1||schema>37||schema==32)throw std::runtime_error("unsupported action schema");
        if(!clearance_packet&&schema!=35&&schema!=36&&schema!=37&&deepest>8)throw std::runtime_error("old action JSON nesting depth exceeds 8");
        if(!clearance_packet&&schema!=19&&schema!=33&&schema!=34&&schema!=35&&schema!=36&&schema!=37&&deepest>7)throw std::runtime_error("action JSON nesting depth exceeds 7");
        Message m;const auto k=u64(j.at("kind")),o=u64(j.at("outcome"));
        if(k<1||k>5||o>3)throw std::runtime_error("unknown message enum");
        m.kind=Kind(k);m.outcome=Outcome(o);m.session=u64(j.at("session"));m.revision=u64(j.at("revision"));
        m.ack_peer=u64(j.at("ack_peer"));m.required_mask=u64(j.at("required_mask"));m.cause=j.at("cause").get<std::string>();
        const auto& r=j.at("request");
        if(schema==36&&u64(r.at("action"))==58)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","zone_edit"});
        else if(schema==36&&u64(r.at("action"))==59)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","modular"});
        else if(schema==35&&(u64(r.at("action"))==54||u64(r.at("action"))==55||u64(r.at("action"))==56))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","world_path"});
        else if(schema==35&&u64(r.at("action"))==53)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","boss_placement"});
        else if(schema==35&&(u64(r.at("action"))==51||u64(r.at("action"))==52))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","group_object"});
        else if(schema==35&&(u64(r.at("action"))==49||u64(r.at("action"))==50))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","scenery_stroke"});
        else if(schema==35&&u64(r.at("action"))==138)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","intervention"});
        else if(schema==35&&u64(r.at("action"))==137)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","game_rules"});
        else if(schema==35&&u64(r.at("action"))==136)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","gm_policy"});
        else if(schema==35&&u64(r.at("action"))==135)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","release_cycle"});
        else if(schema==35&&u64(r.at("action"))==48)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","duplicate"});
        else if(schema==35&&u64(r.at("action"))==134)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","release_edit"});
        else if(schema==35&&u64(r.at("action"))==47)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","group"});
        else if(schema==35&&u64(r.at("action"))==133)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","general_project"});
        else if(schema==36&&u64(r.at("action"))==60)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","district_stroke"});
        else if(schema==36&&u64(r.at("action"))==57)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","district_property"});
        else if(schema==36&&(u64(r.at("action"))==10||u64(r.at("action"))==11))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
        else if(schema==35&&u64(r.at("action"))==46)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","district_title"});
        else if(schema==35&&u64(r.at("action"))==45)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","district_text"});
        else if(schema==35&&(u64(r.at("action"))==10||u64(r.at("action"))==11))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
        else if(schema==35&&u64(r.at("action"))==130)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","logo"});
        else if(schema==35&&u64(r.at("action"))==129)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","network_remove"});
        else if(schema==35&&u64(r.at("action"))==100)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","building_status"});
        else if(schema==35&&u64(r.at("action"))==102)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","dungeon_light"});
        else if((schema==35||schema==36)&&u64(r.at("action"))==105)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","dungeon_gizmo"});
        else if(schema==35&&u64(r.at("action"))==104)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","dungeon_palette"});
        else if(schema==35&&u64(r.at("action"))==103)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","dungeon_sound"});
        else if(schema==35&&u64(r.at("action"))==101)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","dungeon_config"});
        else if(schema==35&&u64(r.at("action"))==88)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","ability_membership"});
        else if((schema==35||schema==37)&&u64(r.at("action"))==87)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","character_membership"});
        else if((schema==35||schema==36)&&u64(r.at("action"))==86)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","costume_edit"});
        else if((schema==35||schema==36)&&u64(r.at("action"))==85)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","ability_edit"});
        else if(schema==35&&(u64(r.at("action"))>=80&&u64(r.at("action"))<=84))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","character_rename"});
        else if(schema==35&&(u64(r.at("action"))==32||u64(r.at("action"))==33))exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc_quest_list"});
        else if(schema==35&&u64(r.at("action"))==128)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","management"});
        else if(schema==35&&u64(r.at("action"))==44)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","selection_move"});
        else if(schema==35&&u64(r.at("action"))==43)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","service_membership"});
        else if(schema==35&&u64(r.at("action"))==42)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","boss_phase"});
        else if((schema==35||schema==36)&&u64(r.at("action"))==41)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","prop_text"});
        else if(schema==35&&u64(r.at("action"))==40)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc_property"});
        else if(schema==34)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc_quest"});
        else if(schema==33)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","monster_remove"});
        else if(schema==31)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","monster_move"});
        else if(schema==30)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","monster"});
        else if(schema==26)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","bug_fix"});
        else if(schema==25)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","zone"});
        else if(schema==19){const auto action=u64(r.at("action"));
            if(action==15)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc"});
            else if(action==16)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
            else if(action==17)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","building"});
            else throw std::runtime_error("Quest schema action invalid");}
        else if(schema==22||schema==28)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","desired_workers"});
        else if(schema==18)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid","building"});
        else if(schema==17)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
        else if(schema==16)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","npc"});
        else if(schema==13)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","price"});
        else if(schema==12)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","path_delete"});
        else if(schema==11||schema==15||schema==35)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","region_property"});
        else if(schema==10)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","target_action_uuid"});
        else if(schema==1||schema==20||schema==21||schema==23||schema==24||schema==27||schema==29)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key"});
        else if(schema==2)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","building"});
        else if(schema==3||schema==5)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","region"});
        else if(schema==9)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","network_pipe"});
        else if(schema==8)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","network"});
        else if(schema==6||schema==7)exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","path"});
        else exact(r,{"action","peer","sequence","host_revision","action_uuid","snapshot_uuid","canonical_record_key","terrain"});
        const auto action=u64(r.at("action"));const bool modular_form=schema==36&&(action==59||((action==10||action==11)&&r.at("canonical_record_key").get<std::string>()=="modular:"+r.at("target_action_uuid").get<std::string>()));
        const bool gizmo_text_new=schema==36&&action==41;const bool gizmo_new=schema==36&&action==105;const bool costume_new=schema==36&&action==86;const bool ability_new=schema==36&&action==85;const bool character_import_new=schema==37&&action==87;
        const int action_depth=(gizmo_new||gizmo_text_new)?9:modular_form||costume_new?10:schema==35&&(action==86||action==88||action==54||action==55||action==56)?10:schema==35&&(action==40||action==42)?9:8;if(!clearance_packet&&deepest>action_depth)throw std::runtime_error("action JSON nesting depth exceeds its action bound");const bool district_new=schema==36&&(action==57||action==58||action==60||((action==10||action==11)&&
            (r.at("canonical_record_key").get<std::string>()=="district-edit:"+r.at("target_action_uuid").get<std::string>()||r.at("canonical_record_key").get<std::string>()=="district-stroke:"+r.at("target_action_uuid").get<std::string>()||r.at("canonical_record_key").get<std::string>()=="zone-edit:"+r.at("target_action_uuid").get<std::string>())));
        if((schema==37&&!character_import_new)||(schema==36&&!district_new&&!modular_form&&!costume_new&&!ability_new&&!gizmo_new&&!gizmo_text_new)||((action==57||action==58||action==59||action==60)&&schema!=36)||(!district_new&&!modular_form&&!costume_new&&!ability_new&&!gizmo_new&&!gizmo_text_new&&!character_import_new&&(action<1||(action>33&&action!=40&&action!=41&&action!=42&&action!=43&&action!=44&&action!=45&&action!=46&&action!=47&&action!=48&&action!=49&&action!=50&&action!=51&&action!=52&&action!=53&&action!=54&&action!=55&&action!=56&&action!=57&&action!=128&&action!=129&&action!=130&&action!=133&&action!=134&&action!=135&&action!=136&&action!=137&&action!=138&&action!=100&&action!=101&&action!=103&&action!=104&&action!=105&&action!=102&&(action<80||action>88))||((action==32||action==33||action==40||action==41||action==42||action==43||action==44||action==45||action==46||action==47||action==48||action==49||action==50||action==51||action==52||action==53||action==54||action==55||action==56||action==57||action==128||action==129||action==130||action==133||action==134||action==135||action==136||action==137||action==138||action==100||action==101||action==103||action==104||action==105||action==102||(action>=80&&action<=88))&&schema!=35)||(schema==34)!=(action==31)||(schema==33)!=(action==30)||(schema==31)!=(action==29)||(schema==30)!=(action==28)||(schema==27)!=(action==25)||(schema==28)!=(action==26)||(schema==29)!=(action==27)||(schema==26)!=(action==24)||(schema==25)!=(action==23)||(schema==23)!=(action==21)||(schema==24)!=(action==22)||(schema==22)!=(action==20)||(schema==21)!=(action==19)||(schema==20)!=(action==18)||(schema==19?(action<15||action>17):((schema==18&&action!=17)||(schema!=18&&action==17)||(schema==17&&action!=16)||(schema!=17&&action==16)||(schema==16&&action!=15)||(schema!=16&&action==15)||(schema==14&&(action!=5||!r.at("terrain").contains("height")))||(schema==13&&action!=14)||(schema==12&&action!=13)||((schema==11||schema==15||(schema==35&&action!=10&&action!=11&&action!=32&&action!=33&&action!=40&&action!=41&&action!=42&&action!=43&&action!=44&&action!=45&&action!=46&&action!=47&&action!=48&&action!=49&&action!=50&&action!=51&&action!=52&&action!=53&&action!=54&&action!=55&&action!=56&&action!=57&&action!=128&&action!=129&&action!=130&&action!=133&&action!=134&&action!=135&&action!=136&&action!=137&&action!=138&&action!=100&&action!=101&&action!=103&&action!=104&&action!=105&&action!=102&&(action<80||action>88)))&&action!=12)||(schema==10&&action!=10&&action!=11)||(schema==1&&action!=1)||(schema==2&&action>3)||(schema==3&&action!=4)||(schema==4&&action!=5)||(schema==5&&action!=6)||((schema==6||schema==7)&&action!=7)||(schema==8&&action!=8)||(schema==9&&action!=9))))))throw std::runtime_error("unsupported typed action");
        m.request.action=Action(action);
        m.request.peer=u64(r.at("peer"));m.request.sequence=u64(r.at("sequence"));m.request.host_revision=u64(r.at("host_revision"));
        m.request.action_uuid=r.at("action_uuid").get<std::string>();m.request.snapshot_uuid=r.at("snapshot_uuid").get<std::string>();
        m.request.canonical_record_key=r.at("canonical_record_key").get<std::string>();
        if(schema==33||schema==31||schema==10||((schema==35||schema==36)&&(action==10||action==11))||schema==17||schema==18||(schema==19&&action!=15))m.request.target_action_uuid=r.at("target_action_uuid").get<std::string>();
        if((action==10||action==11)&&((schema==35)!=(selection_history(m.request)||region_history(m.request)||path_history(m.request)||terrain_history(m.request)||quest_history(m.request)||network_history(m.request)||group_history(m.request)||duplicate_history(m.request)||scenery_stroke_history(m.request)||group_object_history(m.request)||dungeon_tiles_history(m.request))))throw std::runtime_error("history schema and canonical family differ");
        const auto& e=j.at("evidence");
        if(zone_edit_history(m.request)&&schema!=36)throw std::runtime_error("Zone edit history requires schema36");
        if(district_stroke_history(m.request)&&schema!=36)throw std::runtime_error("District stroke history requires schema36");
        if(district_property_history(m.request)&&schema!=36)throw std::runtime_error("District history requires schema36");
        if(modular_form){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","modular_before","modular_after","modular_result"});
            if(action==59)m.request.modular=modular_read(r.at("modular"));m.evidence.modular_before=scenery_stroke_state_read(e.at("modular_before"));m.evidence.modular_after=scenery_stroke_state_read(e.at("modular_after"));m.evidence.modular_result=modular_result_read(e.at("modular_result"));
        }else if(schema==35&&(action==54||action==55||action==56)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","world_path_before","world_path_after"});
            m.request.world_path=world_path_read(r.at("world_path"));
            m.evidence.world_path_before=world_path_state_read(e.at("world_path_before"));
            m.evidence.world_path_after=world_path_state_read(e.at("world_path_after"));
        }else if(schema==35&&(action==10||action==11)&&e.contains("world_path_before")){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","world_path_before","world_path_after"});
            m.evidence.world_path_before=world_path_state_read(e.at("world_path_before"));
            m.evidence.world_path_after=world_path_state_read(e.at("world_path_after"));
        }else if(schema==35&&action==53){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","boss_placement_before","boss_placement_after"});
            m.request.boss_placement=boss_placement_read(r.at("boss_placement"));
            m.evidence.boss_placement_before=boss_placement_state_read(e.at("boss_placement_before"));
            m.evidence.boss_placement_after=boss_placement_state_read(e.at("boss_placement_after"));
        }else if(schema==35&&(action==51||action==52||group_object_history(m.request))){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","group_object_before","group_object_after","duplicate_cash"});
            if(action==51||action==52)m.request.group_object=group_object_read(r.at("group_object"),action==51);
            m.evidence.group_object_before=group_object_state_read(e.at("group_object_before"));m.evidence.group_object_after=group_object_state_read(e.at("group_object_after"));
            m.evidence.duplicate_cash=duplicate_cash_read(e.at("duplicate_cash"));
        }else if(schema==35&&(action==49||action==50)){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","scenery_stroke_before","scenery_stroke_after","scenery_stroke_result"});
            m.request.scenery_stroke=scenery_stroke_read(r.at("scenery_stroke"));m.evidence.scenery_stroke_before=scenery_stroke_state_read(e.at("scenery_stroke_before"));
            m.evidence.scenery_stroke_after=scenery_stroke_state_read(e.at("scenery_stroke_after"));m.evidence.scenery_stroke_result=scenery_stroke_result_read(e.at("scenery_stroke_result"));
        }else if(schema==35&&scenery_stroke_history(m.request)){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","scenery_stroke_before","scenery_stroke_after","scenery_stroke_result","duplicate_cash"});
            m.evidence.scenery_stroke_before=scenery_stroke_state_read(e.at("scenery_stroke_before"));m.evidence.scenery_stroke_after=scenery_stroke_state_read(e.at("scenery_stroke_after"));
            m.evidence.scenery_stroke_result=scenery_stroke_result_read(e.at("scenery_stroke_result"),true);m.evidence.duplicate_cash=duplicate_cash_read(e.at("duplicate_cash"));
        }else if(schema==35&&action==138){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","intervention_before","intervention_after"});
            m.request.intervention=game::interventions::codec::intent(r.at("intervention"));
            if(!e.at("intervention_before").is_null())m.evidence.intervention_before=game::interventions::codec::proof(e.at("intervention_before"));
            if(!e.at("intervention_after").is_null())m.evidence.intervention_after=game::interventions::codec::proof(e.at("intervention_after"));
        }else if(schema==35&&action==137){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","game_rules_before","game_rules_after"});
            m.request.game_rules=game::game_rules::codec::intent(r.at("game_rules"));
            if(!e.at("game_rules_before").is_null())m.evidence.game_rules_before=game::game_rules::codec::proof(e.at("game_rules_before"));
            if(!e.at("game_rules_after").is_null())m.evidence.game_rules_after=game::game_rules::codec::proof(e.at("game_rules_after"));
        }else if(schema==35&&action==136){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","gm_policy_before","gm_policy_after"});
            m.request.gm_policy=game::gm_policy::codec::intent(r.at("gm_policy"));
            if(!e.at("gm_policy_before").is_null())m.evidence.gm_policy_before=game::gm_policy::codec::proof(e.at("gm_policy_before"));
            if(!e.at("gm_policy_after").is_null())m.evidence.gm_policy_after=game::gm_policy::codec::proof(e.at("gm_policy_after"));
        }else if(schema==35&&action==135){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","release_cycle_before","release_cycle_after","release_cycle_value"});
            m.request.release_cycle=game::release_cycle::codec::intent(r.at("release_cycle"));
            if(!e.at("release_cycle_before").is_null())m.evidence.release_cycle_before=game::release_cycle::codec::proof(e.at("release_cycle_before"));
            if(!e.at("release_cycle_after").is_null())m.evidence.release_cycle_after=game::release_cycle::codec::proof(e.at("release_cycle_after"));
            if(!e.at("release_cycle_value").is_null())m.evidence.release_cycle_value=game::release_cycle::codec::publication(e.at("release_cycle_value"));
        }else if(schema==35&&dungeon_tiles_history(m.request)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_palette_before","dungeon_palette_after"});
            m.evidence.dungeon_palette_before=dungeon_palette_state_read(e.at("dungeon_palette_before"));m.evidence.dungeon_palette_after=dungeon_palette_state_read(e.at("dungeon_palette_after"));
        }else if(schema==35&&duplicate_history(m.request)){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","duplicate_before","duplicate_after","duplicate_cash"});
            m.evidence.duplicate_before=duplicate_state_read(e.at("duplicate_before"));m.evidence.duplicate_after=duplicate_state_read(e.at("duplicate_after"));m.evidence.duplicate_cash=duplicate_cash_read(e.at("duplicate_cash"));
        }else if(schema==35&&group_history(m.request)){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","group_before","group_after"});
            m.evidence.group_before=group_state_read(e.at("group_before"));m.evidence.group_after=group_state_read(e.at("group_after"));
        }else if(schema==35&&action==48){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","duplicate_before","duplicate_after"});
            m.request.duplicate=duplicate_intent_read(r.at("duplicate"));m.evidence.duplicate_before=duplicate_state_read(e.at("duplicate_before"));m.evidence.duplicate_after=duplicate_state_read(e.at("duplicate_after"));
        }else if(schema==35&&action==134){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","release_edit_before","release_edit_after"});
            m.request.release_edit=game::release_edit::codec::intent(r.at("release_edit"));
            if(!e.at("release_edit_before").is_null())m.evidence.release_edit_before=game::release_edit::codec::proof(e.at("release_edit_before"));
            if(!e.at("release_edit_after").is_null())m.evidence.release_edit_after=game::release_edit::codec::proof(e.at("release_edit_after"));
        }else if(schema==35&&action==47){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","group_before","group_after"});
            m.request.group=group_intent_read(r.at("group"));m.evidence.group_before=group_state_read(e.at("group_before"));m.evidence.group_after=group_state_read(e.at("group_after"));
        }else if(schema==35&&action==133){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","general_project_before","general_project_after"});
            m.request.general_project=game::general_project::codec::intent(r.at("general_project"));
            if(!e.at("general_project_before").is_null())m.evidence.general_project_before=game::general_project::codec::proof(e.at("general_project_before"));
            if(!e.at("general_project_after").is_null())m.evidence.general_project_after=game::general_project::codec::proof(e.at("general_project_after"));
        }else if(schema==36&&(action==58||zone_edit_history(m.request))){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","zone_edit"});
            if(action==58)m.request.zone_edit=zone_edit_codec::intent_read(r.at("zone_edit"));
            m.evidence.zone_edit=zone_edit_optional_read(e.at("zone_edit"));
        }else if(schema==36&&(action==60||district_stroke_history(m.request))){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","district_stroke"});
            if(action==60)m.request.district_stroke=district_stroke_intent_read(r.at("district_stroke"));
            m.evidence.district_stroke=district_stroke_effect_read(e.at("district_stroke"));
        }else if(schema==36&&(action==57||district_property_history(m.request))){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","district_before","district_after","district_continent_height"});
            if(action==57)m.request.district_property=district_property_read(r.at("district_property"));
            m.evidence.district_before=district_state_read(e.at("district_before"));m.evidence.district_after=district_state_read(e.at("district_after"));
            m.evidence.district_continent_height=district_property_u32(e.at("district_continent_height"));
        }else if(schema==35&&action==46){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","district_before","district_after"});
            m.request.district_title=district_title_read(r.at("district_title"));m.evidence.district_before=district_state_read(e.at("district_before"));m.evidence.district_after=district_state_read(e.at("district_after"));
        }else if(schema==35&&action==45){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","district_before","district_after"});
            m.request.district_text=district_intent_read(r.at("district_text"));m.evidence.district_before=district_state_read(e.at("district_before"));m.evidence.district_after=district_state_read(e.at("district_after"));
        }else if(schema==35&&network_history(m.request)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","network_removal_before","network_removal_after","network_history"});
            if(!e.at("network_removal_before").is_null())m.evidence.network_removal_before=parse_network_removal(e.at("network_removal_before"));
            if(!e.at("network_removal_after").is_null())m.evidence.network_removal_after=parse_network_removal(e.at("network_removal_after"));
            if(!e.at("network_history").is_null()){const auto& proof=e.at("network_history");exact(proof,{"source","amount","junction"});const auto source=u64(proof.at("source"));if((source!=8&&source!=9&&source!=129)||!proof.at("junction").is_boolean())throw std::runtime_error("Network history original source/Boolean invalid");m.evidence.network_history=NetworkHistoryProof{uint8_t(source),i32(proof.at("amount")),proof.at("junction").get<bool>()};}
        }else if(schema==35&&path_history(m.request)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","region_before","region_after","terrain_before","terrain_after","path_before","path_after"});
            m.evidence.path_before=path_state_read(e.at("path_before"));m.evidence.path_after=path_state_read(e.at("path_after"));
            m.evidence.region_before=region_read(e.at("region_before"));m.evidence.region_after=region_read(e.at("region_after"));
            m.evidence.terrain_before=terrain_state_read(e.at("terrain_before"));m.evidence.terrain_after=terrain_state_read(e.at("terrain_after"));
        }else if(schema==35&&terrain_history(m.request)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","region_before","region_after","terrain_before","terrain_after","region_retired_sha256"});
            m.evidence.region_before=region_read(e.at("region_before"));m.evidence.region_after=region_read(e.at("region_after"));
            m.evidence.terrain_before=terrain_state_read(e.at("terrain_before"));m.evidence.terrain_after=terrain_state_read(e.at("terrain_after"));
            m.evidence.region_retired_sha256=e.at("region_retired_sha256").get<std::string>();
        }else if(schema==35&&quest_history(m.request)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","quest_list_before","quest_list_after"});
            m.evidence.quest_list_before=npc_quest_list_state_read(e.at("quest_list_before"));m.evidence.quest_list_after=npc_quest_list_state_read(e.at("quest_list_after"));
        }else if(schema==35&&region_history(m.request)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","property_before","property_after","region_retired_sha256"});
            m.evidence.property_before=property_state_read(e.at("property_before"));m.evidence.property_after=property_state_read(e.at("property_after"));
            m.evidence.region_retired_sha256=e.at("region_retired_sha256").get<std::string>();
        }else if(schema==35&&(action==10||action==11)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","selection_move_before","selection_move_after"});
            m.evidence.selection_move_before=selection_move_state_read(e.at("selection_move_before"));m.evidence.selection_move_after=selection_move_state_read(e.at("selection_move_after"));
        }else if(schema==35&&action==130){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","logo_before","logo_after"});
            const auto& p=r.at("logo");exact(p,{"before_sha256","desired_sha256","records"});
            m.request.logo={p.at("before_sha256").get<std::string>(),p.at("desired_sha256").get<std::string>(),p.at("records")};
            if(!e.at("logo_before").is_null())m.evidence.logo_before=logo_state_read(e.at("logo_before"));
            if(!e.at("logo_after").is_null())m.evidence.logo_after=logo_state_read(e.at("logo_after"));
        }else if(schema==35&&action==129){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","network_removal_before","network_removal_after"});
            const auto& p=r.at("network_remove");exact(p,{"region","graph_before_sha256","quoted_refund"});m.request.network_remove={i32(p.at("region")),p.at("graph_before_sha256").get<std::string>(),i32(p.at("quoted_refund"))};
            if(!e.at("network_removal_before").is_null())m.evidence.network_removal_before=parse_network_removal(e.at("network_removal_before"));
            if(!e.at("network_removal_after").is_null())m.evidence.network_removal_after=parse_network_removal(e.at("network_removal_after"));

        }else if(schema==35&&action==100){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","building_status_before","building_status_after"});
            m.request.building_status=building_status_intent_read(r.at("building_status"));m.evidence.building_status_before=building_status_state_read(e.at("building_status_before"));m.evidence.building_status_after=building_status_state_read(e.at("building_status_after"));
        }else if(schema==35&&action==102){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_light_before","dungeon_light_after"});
            m.request.dungeon_light=dungeon_light_intent_read(r.at("dungeon_light"));m.evidence.dungeon_light_before=dungeon_light_state_read(e.at("dungeon_light_before"));m.evidence.dungeon_light_after=dungeon_light_state_read(e.at("dungeon_light_after"));
        }else if((schema==35||schema==36)&&action==105){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_gizmo_before","dungeon_gizmo_after"});
            m.request.dungeon_gizmo=dungeon_gizmo_intent_read(r.at("dungeon_gizmo"));if((schema==36)!=(m.request.dungeon_gizmo.edit==DungeonGizmoEdit::State))throw std::runtime_error("Gizmo command/schema domain differs");m.evidence.dungeon_gizmo_before=dungeon_gizmo_state_read(e.at("dungeon_gizmo_before"));m.evidence.dungeon_gizmo_after=dungeon_gizmo_state_read(e.at("dungeon_gizmo_after"));
        }else if(schema==35&&action==104){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_palette_before","dungeon_palette_after"});
            m.request.dungeon_palette=dungeon_palette_intent_read(r.at("dungeon_palette"));m.evidence.dungeon_palette_before=dungeon_palette_state_read(e.at("dungeon_palette_before"));m.evidence.dungeon_palette_after=dungeon_palette_state_read(e.at("dungeon_palette_after"));
        }else if(schema==35&&action==103){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_sound_before","dungeon_sound_after"});
            m.request.dungeon_sound=dungeon_sound_intent_read(r.at("dungeon_sound"));m.evidence.dungeon_sound_before=dungeon_sound_state_read(e.at("dungeon_sound_before"));m.evidence.dungeon_sound_after=dungeon_sound_state_read(e.at("dungeon_sound_after"));
        }else if(schema==35&&action==101){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","dungeon_config_before","dungeon_config_after"});
            m.request.dungeon_config=dungeon_config_intent_read(r.at("dungeon_config"));m.evidence.dungeon_config_before=dungeon_config_state_read(e.at("dungeon_config_before"));m.evidence.dungeon_config_after=dungeon_config_state_read(e.at("dungeon_config_after"));
        }else if(schema==35&&action==88){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","ability_membership_before","ability_membership_after"});
            m.request.ability_membership=ability_membership_intent_read(r.at("ability_membership"));m.evidence.ability_membership_before=ability_membership_state_read(e.at("ability_membership_before"));m.evidence.ability_membership_after=ability_membership_state_read(e.at("ability_membership_after"));
        }else if((schema==35||schema==37)&&action==87){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","character_membership_before","character_membership_after"});
            m.request.character_membership=character_membership_intent_read(r.at("character_membership"),schema==37);m.evidence.character_membership_before=character_membership_state_read(e.at("character_membership_before"),schema==37);m.evidence.character_membership_after=character_membership_state_read(e.at("character_membership_after"),schema==37);
        }else if((schema==35||schema==36)&&action==86){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","costume_edit_before","costume_edit_after"});
            m.request.costume_edit=costume_edit_intent_read(r.at("costume_edit"),schema==36);m.evidence.costume_edit_before=costume_edit_state_read(e.at("costume_edit_before"),schema==36);m.evidence.costume_edit_after=costume_edit_state_read(e.at("costume_edit_after"),schema==36);
        }else if((schema==35||schema==36)&&action==85){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","ability_edit_before","ability_edit_after"});
            m.request.ability_edit=ability_edit_intent_read(r.at("ability_edit"),schema==36);m.evidence.ability_edit_before=ability_edit_state_read(e.at("ability_edit_before"));m.evidence.ability_edit_after=ability_edit_state_read(e.at("ability_edit_after"));
        }else if(schema==35&&action>=80&&action<=84){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","character_rename_before","character_rename_after"});
            m.request.character_rename=character_rename_intent_read(r.at("character_rename"));m.evidence.character_rename_before=character_rename_state_read(e.at("character_rename_before"));m.evidence.character_rename_after=character_rename_state_read(e.at("character_rename_after"));
        }else if(schema==35&&(action==32||action==33)){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","quest_list_before","quest_list_after"});
            m.request.npc_quest_list=npc_quest_list_intent_read(r.at("npc_quest_list"));
            m.evidence.quest_list_before=npc_quest_list_state_read(e.at("quest_list_before"));m.evidence.quest_list_after=npc_quest_list_state_read(e.at("quest_list_after"));

        }else if(schema==35&&action==128){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","management_before","management_after"});
            m.request.management=management_intent_read(r.at("management"));
            if(!e.at("management_before").is_null())m.evidence.management_before=management_state_read(e.at("management_before"));
            if(!e.at("management_after").is_null())m.evidence.management_after=management_state_read(e.at("management_after"));

        }else if(schema==35&&action==44){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","selection_move_before","selection_move_after"});
            m.request.selection_move=selection_move_intent_read(r.at("selection_move"));m.evidence.selection_move_before=selection_move_state_read(e.at("selection_move_before"));m.evidence.selection_move_after=selection_move_state_read(e.at("selection_move_after"));
        }else if(schema==35&&action==43){exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","service_membership_before","service_membership_after"});

            m.request.service_membership=service_intent_read(r.at("service_membership"));m.evidence.service_membership_before=service_state_read(e.at("service_membership_before"));m.evidence.service_membership_after=service_state_read(e.at("service_membership_after"));
        }else if(schema==35&&action==42){

            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","boss_phase_before","boss_phase_after"});
            m.request.boss_phase=boss_intent_read(r.at("boss_phase"));
            m.evidence.boss_phase_before=boss_state_read(e.at("boss_phase_before"));
            m.evidence.boss_phase_after=boss_state_read(e.at("boss_phase_after"));
        }else if((schema==35||schema==36)&&action==41){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","prop_text_before","prop_text_after"});
            m.request.prop_text=prop_text_intent_read(r.at("prop_text"),schema==36);
            m.evidence.prop_text_before=prop_text_state_read(e.at("prop_text_before"),schema==36);
            m.evidence.prop_text_after=prop_text_state_read(e.at("prop_text_after"),schema==36);
        }else if(schema==35&&action==40){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","npc_property_before","npc_property_after"});
            m.request.npc_property=npc_property_intent_read(r.at("npc_property"));
            m.evidence.npc_property_before=npc_property_state_read(e.at("npc_property_before"));
            m.evidence.npc_property_after=npc_property_state_read(e.at("npc_property_after"));
        }else if(schema==34){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","quest_before","quest_after"});
            m.request.npc_quest=npc_quest_intent_read(r.at("npc_quest"));
            m.evidence.quest_before=npc_quest_state_read(e.at("quest_before"));m.evidence.quest_after=npc_quest_state_read(e.at("quest_after"));
        }else if(schema==33){
            if(!game::monster_target_key::request(m.request.canonical_record_key,m.request.target_action_uuid))throw std::runtime_error("Legacy Monster Remove key differs");
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","monster_before","monster_after","monster_quests"});
            const auto& remove=r.at("monster_remove");exact(remove,{"region"});m.request.monster_remove.region=i32(remove.at("region"));
            m.evidence.monster_before=monster_state_read(e.at("monster_before"));m.evidence.monster_after=monster_state_read(e.at("monster_after"));
            m.evidence.monster_quests=monster_quests_read(e.at("monster_quests"));
        }else if(schema==31){
            if(!game::monster_target_key::request(m.request.canonical_record_key,m.request.target_action_uuid))throw std::runtime_error("Legacy Monster Move key differs");
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","monster_before","monster_after"});
            m.request.monster_move=monster_move_read(r.at("monster_move"));m.evidence.monster_before=monster_state_read(e.at("monster_before"));m.evidence.monster_after=monster_state_read(e.at("monster_after"));
        }else if(schema==30){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","monster_before","monster_after"});
            m.request.monster=monster_intent_read(r.at("monster"));m.evidence.monster_before=monster_state_read(e.at("monster_before"));m.evidence.monster_after=monster_state_read(e.at("monster_after"));
        }else if(schema>=27&&schema<=29){
            if(schema==28){const auto desired=u64(r.at("desired_workers"));if(desired>50)throw std::runtime_error("Research desired workers exceeds50");m.request.desired_workers=int32_t(desired);}
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","research_before","research_after"});
            const auto read=[](const json& j,std::optional<game::tutorial_replication::research::Row>& out){
                if(j.is_null())return;
                game::tutorial_replication::research::Row row;std::string cause;
                if(!game::tutorial_replication::research::decode(j,row,cause))throw std::runtime_error(cause);
                out=std::move(row);
            };
            read(e.at("research_before"),m.evidence.research_before);read(e.at("research_after"),m.evidence.research_after);
        }else if(schema==26){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","bug_before","bug_after","developers_before","developers_after"});
            m.request.bug_fix=bug_fix_read(r.at("bug_fix"));
            bug_fix_row_read(e.at("bug_before"),m.evidence.bug_before);bug_fix_row_read(e.at("bug_after"),m.evidence.bug_after);
            const auto read=[](const json& j,std::optional<game::developer_staff::PoolState>& out){
                if(j.is_null())return;game::developer_staff::PoolState pool;std::string cause;
                if(!game::developer_staff::decode(j,pool,cause))throw std::runtime_error(cause);out=std::move(pool);};
            read(e.at("developers_before"),m.evidence.developers_before);read(e.at("developers_after"),m.evidence.developers_after);
        }else if(schema==25){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","zone_before","zone_after"});
            m.request.zone=zone_intent_read(r.at("zone"));m.evidence.zone_before=zone_state_read(e.at("zone_before"));m.evidence.zone_after=zone_state_read(e.at("zone_after"));
        }else if(schema==23||schema==24){
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","developers_before","developers_after"});
            const auto read=[](const json& j,std::optional<game::developer_staff::PoolState>& out){
                if(j.is_null())return;game::developer_staff::PoolState pool;std::string cause;
                if(!game::developer_staff::decode(j,pool,cause))throw std::runtime_error(cause);out=std::move(pool);
            };
            read(e.at("developers_before"),m.evidence.developers_before);read(e.at("developers_after"),m.evidence.developers_after);
        } else if(schema==20||schema==21||schema==22){
            if(schema==22){const auto desired=u64(r.at("desired_workers"));if(desired>50)throw std::runtime_error("Startingpoint desired workers exceeds50");m.request.desired_workers=int32_t(desired);}
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","startingpoint_before","startingpoint_after"});
            const auto read=[](const json& j,std::optional<game::tutorial_replication::startingpoint::Row>& out){
                if(j.is_null())return;
                game::tutorial_replication::startingpoint::Row row;std::string cause;
                if(!game::tutorial_replication::startingpoint::decode(j,row,cause))throw std::runtime_error(cause);
                out=row;
            };
            read(e.at("startingpoint_before"),m.evidence.startingpoint_before);
            read(e.at("startingpoint_after"),m.evidence.startingpoint_after);
        } else if(schema==16||schema==17||schema==18||schema==19) {
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
        } else if(schema==11||schema==15||schema==35) {
            exact(e,{"before","after","target_retired","undo_confirmed","selection_confirmed","retirement_confirmed","property_before","property_after","region_retired_sha256"});
            m.request.region_property=property_intent_read(r.at("region_property"));
            if((schema==35)!=(appearance_kind(m.request.region_property.kind)||m.request.region_property.kind==RegionPropertyKind::Sale))throw std::runtime_error("region appearance schema mismatch");
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
        if((schema==31||schema==33)&&((m.evidence.monster_before.actor!=MonsterActor{}&&m.evidence.monster_before.actor.rank<=0)||(m.evidence.monster_after.actor!=MonsterActor{}&&m.evidence.monster_after.actor.rank<=0)))throw std::runtime_error("Legacy Monster edit requires positive rank");
        if(clearance_packet){
            auto forward=clearance;
            const bool restoring=m.request.action!=Action::PlaceBuilding&&!m.evidence.object_after.alive;
            if(restoring){
                if(!forward.is_array()||forward.size()!=4||!forward[2].is_array())throw std::runtime_error("Building clearance effect width differs");
                std::swap(forward[0],forward[1]);
                for(auto& change:forward[2]){if(!change.is_array()||change.size()!=2)throw std::runtime_error("Building clearance Group pair width differs");std::swap(change[0],change[1]);}
            }
            m.evidence.building_clearance=scenery_stroke_result_read(forward,true);
            if(restoring){std::swap(m.evidence.building_clearance->removed,m.evidence.building_clearance->created);
                for(auto& change:m.evidence.building_clearance->changed)std::swap(change.before,change.after);}
        }
        if(!valid_message(m,error))return false;out=std::move(m);error.clear();return true;
    }catch(const std::exception& e){error=std::string("action decode: ")+e.what();return false;}
}
#include "path_effect.inc"
#include "network_effect.inc"
#include "network_pipe_effect.inc"
#include "network_history_effect.inc"
Controller::Controller(Session& s,Engine& e,Transport& t,Logger l,uint64_t timeout,Clock clock):session_(s),engine_(e),transport_(t),logger_(std::move(l)),clock_(std::move(clock)),timeout_(timeout) {}
bool Controller::update_time(uint64_t fallback,std::string& error) {
    uint64_t actual=fallback;
    if(clock_){
        if(!invoke([&]{actual=clock_();return true;},error))return fail("action monotonic clock: "+error,error,true);
        if(actual<now_)return fail("action monotonic clock regressed",error,true);
    }
    now_=actual;return true;
}
bool Controller::arm_deadline(uint64_t& deadline,std::string& error) {
    if(!update_time(now_,error))return false;
    if(now_>UINT64_MAX-timeout_)return fail("ACK deadline timestamp exhausted",error,true);
    deadline=now_+timeout_;return true;
}
bool Controller::invoke(const std::function<bool()>& fn,std::string& cause) {
    const bool was_locked=status_.locked;busy_=true;
    const bool accepted=callback(fn,cause);busy_=false;
    if(!was_locked&&status_.locked){cause=status_.cause;return false;}
    return accepted;
}
bool Controller::can_retire_lobby() const {
    return (thread_==std::thread::id{}||thread_==std::this_thread::get_id())&&!busy_&&
        !status_.pending&&!pending_&&!local_request_&&!authority_waiting_&&(!status_.locked||idle_session_loss_);
}
bool Controller::fail(const std::string& cause,std::string& out,bool lock,bool session_loss) {
    out=cause.empty()?"action failure lacks cause":cause;status_.cause=out;++status_.failed;
    if(lock){
        // Lost session observation may retire an idle owner, but never resume it.
        idle_session_loss_=session_loss&&!status_.locked&&!status_.pending&&!busy_&&!pending_&&!local_request_&&!authority_waiting_;
        status_.locked=true;status_.outcome=Outcome::Unknown;
    }
    if(last_logged_error_!=out){last_logged_error_=out;
#ifdef MT2MP_FRAMEWORK_PLUGIN
        reporting::event("action.failed",{{"revision",status_.host_revision},{"required_mask",uint64_t(status_.required_mask)},
            {"ack_mask",uint64_t(status_.ack_mask)}},status_.action_uuid);
#endif
        if(logger_)logger_("error","ActionSession",out);}
    else ++status_.suppressed_errors;
    return false;
}
bool Controller::guard(std::string& error) {
    if(!status_.started)return fail("action controller not started",error);
    if(std::this_thread::get_id()!=thread_)return fail("action controller requires bound game thread",error);
    if(busy_)return fail("action controller reentrant call",error,true);
    if(status_.locked){error="action controller locked: "+status_.cause;++status_.suppressed_errors;return false;}
    Context actual;std::string cause;
    if(!invoke([&]{return session_.current(actual,cause);},cause))return fail("session observation: "+cause,error,true,true);
    if(actual!=context_)return fail("immutable action session/snapshot/participant binding changed",error,true,true);
    if(!invoke([&]{return engine_.current(binding_,context_.snapshot_uuid,cause);},cause))return fail("native world observation: "+cause,error,true);
    return true;
}
bool Controller::start(uint64_t now,std::string& error) {
    if(status_.started||thread_!=std::thread::id{})return fail("action controller cannot restart or clear replay history",error);
    thread_=std::this_thread::get_id();if(!update_time(now,error))return false;
    if(!timeout_)return fail("ACK timeout must be positive",error);
    if(!invoke([&]{return session_.current(context_,error);},error))return fail("session binding: "+error,error);
    if(!context_.session||!uuid(context_.snapshot_uuid)||(context_.role!=Role::Host&&context_.role!=Role::Client)||context_.local_peer<1||context_.local_peer>4||context_.host_peer!=1||
       context_.peer_mask>15||!(context_.peer_mask&1)||!(context_.peer_mask&bit(context_.local_peer))||
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
#ifdef MT2MP_FRAMEWORK_PLUGIN
    reporting::event("action.sent."+std::to_string(unsigned(m.kind)),{{"action_sequence",m.request.sequence},{"action_type",uint64_t(m.request.action)},
        {"revision",m.revision},{"peer_slot",target},{"required_mask",m.required_mask},{"ack_mask",uint64_t(status_.ack_mask)},
        {"bytes",uint64_t(bytes.size())}},m.request.action_uuid);
#endif
    return true;
}
Controller::Cached& Controller::remember(const std::string& id,const Message& request) {
    // Cache records hold several complete messages. Construct them in the map
    // so the growing action DTO does not produce large native stack copies.
    auto [entry,inserted]=history_.try_emplace(id);
    if(inserted){entry->second.request=request;history_order_.push_back(id);}
    if(history_order_.size()>history_limit){history_.erase(history_order_.front());history_order_.pop_front();}
    return entry->second;
}
bool Controller::submit_building_history(Action action,const std::string& id,const std::string& target,uint64_t now,std::string& error) {
    return submit_building_history(action,id,target,"action:"+target,now,error);
}
bool Controller::submit_building_history(Action action,const std::string& id,const std::string& target,const std::string& object_key,uint64_t now,std::string& error) {
    if(action!=Action::UndoBuilding&&action!=Action::RedoBuilding)return fail("building history action invalid",error);
    Request request;request.action=action;request.action_uuid=id;request.target_action_uuid=target;
    request.canonical_record_key=object_key;return submit(std::move(request),now,error);
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
bool Controller::submit_intervention(const std::string& id,const InterventionIntent& intent,uint64_t now,std::string& error){
    auto request=std::make_unique<Request>();request->action=Action::Intervention;request->peer=context_.local_peer;request->action_uuid=id;request->canonical_record_key=intent.target.key;request->intervention=intent;return submit(std::move(*request),now,error);
}
bool Controller::submit_game_rules(const std::string& id,const GameRulesIntent& intent,uint64_t now,std::string& error){
    auto r=std::make_unique<Request>();r->action=Action::GameRules;r->peer=context_.local_peer;r->action_uuid=id;r->canonical_record_key="management:game-rules";r->game_rules=intent;return submit(std::move(*r),now,error);
}
bool Controller::submit_gm_policy(const std::string& id,const GmPolicyIntent& intent,uint64_t now,std::string& error){
    auto r=std::make_unique<Request>();r->action=Action::GmPolicy;r->peer=context_.local_peer;r->action_uuid=id;r->canonical_record_key="management:gm-policy";r->gm_policy=intent;return submit(std::move(*r),now,error);
}
bool Controller::submit_release_cycle(const std::string& id,const ReleaseCycleIntent& intent,uint64_t now,std::string& error){
    if(intent.command==game::release_cycle::Command::Finalize&&context_.role!=Role::Host)return fail("Release natural finalize may originate only on Host",error);
    auto r=std::make_unique<Request>();r->action=Action::ReleaseCycle;r->peer=context_.local_peer;r->action_uuid=id;r->canonical_record_key="release:cycle";r->release_cycle=intent;return submit(std::move(*r),now,error);
}
bool Controller::submit_release_edit(const std::string& id,const ReleaseEditIntent& intent,uint64_t now,std::string& error){
    auto r=std::make_unique<Request>();r->action=Action::ReleaseEdit;r->action_uuid=id;r->canonical_record_key="release:editing";r->release_edit=intent;return submit(std::move(*r),now,error);
}
bool Controller::submit_general_project(const std::string& id,const GeneralProjectIntent& intent,uint64_t now,std::string& error){
    auto r=std::make_unique<Request>();r->action=Action::GeneralProject;r->action_uuid=id;r->canonical_record_key="development:project";r->general_project=intent;return submit(std::move(*r),now,error);
}
bool Controller::submit_logo(const std::string& id,const LogoIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::SetLogo;r.action_uuid=id;r.canonical_record_key="software:logo";r.logo=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_network_remove(const std::string& id,const std::string& key,const NetworkRemoveIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::RemoveNetworkObject;r.action_uuid=id;r.canonical_record_key=key;r.network_remove=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_management(const std::string& id,const ManagementIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::ManagementCommand;request.action_uuid=id;request.canonical_record_key="management:business";request.management=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_prices(const std::string& id,const PriceIntent& intent,uint64_t now,std::string& error) {
    Request request;request.action=Action::SetPrices;request.action_uuid=id;
    request.canonical_record_key="business:prices";request.price=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_building_status(const std::string& id,const BuildingStatusIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::SetBuildingStatus;r.action_uuid=id;r.canonical_record_key=intent.key;r.building_status=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_light(const std::string& id,const DungeonLightIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::SetDungeonLighting;r.action_uuid=id;r.canonical_record_key=intent.key;r.dungeon_light=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_gizmo(const std::string& id,const DungeonGizmoIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditDungeonGizmo;r.action_uuid=id;r.canonical_record_key=intent.key;r.dungeon_gizmo=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_input(const std::string& id,const DungeonInputIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::DungeonInput;r.action_uuid=id;r.canonical_record_key=intent.key;r.dungeon_input=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_palette(const std::string& id,const DungeonPaletteIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::SetDungeonPalette;r.action_uuid=id;r.canonical_record_key=intent.key;r.dungeon_palette=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_sound(const std::string& id,const DungeonSoundIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::SetDungeonAtmosphere;r.action_uuid=id;r.canonical_record_key=intent.key;r.dungeon_sound=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_config(const std::string& id,const DungeonConfigIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::ConfigureDungeonInstances;r.action_uuid=id;r.canonical_record_key=intent.key;r.dungeon_config=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_costume_edit(const std::string& id,const CostumeEditIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditCharacterCostume;r.action_uuid=id;r.canonical_record_key=costume_key(intent);r.costume_edit=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_ability_membership(const std::string& id,const AbilityMembershipIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditAbilityMembership;r.action_uuid=id;r.canonical_record_key=ability_membership_key(intent);r.ability_membership=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_ability_edit(const std::string& id,const AbilityEditIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditCharacterAbility;r.action_uuid=id;r.canonical_record_key=ability_key(intent);r.ability_edit=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_dungeon_building(const std::string& id,Action action,const std::string& key,const std::string& scene,
    const BuildingState& building,const std::string& target_action,uint64_t now,std::string& error){
    Request request;request.action=action;request.action_uuid=id;request.canonical_record_key=key;request.building=building;
    request.target_action_uuid=target_action;request.dungeon_building=true;request.dungeon_scene=scene;
    return submit(std::move(request),now,error);
}
bool Controller::submit_character_membership(const std::string& id,const CharacterMembershipIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditCharacterMembership;r.action_uuid=id;r.canonical_record_key=character_membership_key(intent);r.character_membership=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_character_rename(const std::string& id,const CharacterRenameIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action(80+intent.property);r.action_uuid=id;r.canonical_record_key=character_key(intent);r.character_rename=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_npc_quest_list(const std::string& id,const std::string& owner,const NpcQuestListIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=intent.edit==QuestListEdit::List?Action::SetNpcQuestList:Action::EditNpcQuest;request.action_uuid=id;
    request.canonical_record_key=owner;request.npc_quest_list=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_npc_quest(const std::string& id,const std::string& owner,const NpcQuestIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::SetNpcQuestDestination;request.action_uuid=id;
    request.canonical_record_key=owner;request.npc_quest=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_remove_monster(const std::string& id,const std::string& target,const MonsterRemoveIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::RemoveMonster;request.action_uuid=id;
    if(game::monster_target_key::uuid(target))request.target_action_uuid=target;
    request.canonical_record_key=game::monster_target_key::canonical(target);request.monster_remove=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_move_monster(const std::string& id,const std::string& target,const MonsterMoveIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::MoveMonster;request.action_uuid=id;
    if(game::monster_target_key::uuid(target))request.target_action_uuid=target;
    request.canonical_record_key=game::monster_move_key::canonical(target);request.monster_move=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_monster(const std::string& id,const MonsterIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::PlaceMonster;request.action_uuid=id;request.canonical_record_key="action:"+id;request.monster=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_zone(const std::string& id,const ZoneIntent& intent,uint64_t now,std::string& error) {
    Request request;request.action=Action::PlaceMonsterZone;request.action_uuid=id;
    request.canonical_record_key="action:"+id;request.zone=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_boss_placement(const std::string& id,const BossPlacementIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::PlaceBoss;request.action_uuid=id;request.canonical_record_key=game::boss_placement_state::key(id);request.boss_placement=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_world_path(const std::string& id,const WorldPathIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=intent.kind==WorldPathKind::Remove?Action::RemoveWorldPath:intent.kind==WorldPathKind::Wall?Action::PlaceWall:Action::PlacePass;
    request.action_uuid=id;request.canonical_record_key="action:"+id;request.world_path=intent;
    return submit(std::move(request),now,error);
}
bool Controller::submit_group_object(bool moving,const std::string& id,const GroupObjectIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=moving?Action::MoveGroup:Action::RemoveGroup;r.action_uuid=id;r.canonical_record_key=intent.key;r.group_object=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_modular(const std::string& id,const ModularIntent& intent,uint64_t now,std::string& e){Request r;r.action=Action::EditModularBuilding;r.action_uuid=id;r.canonical_record_key="modular:"+id;r.modular=intent;return submit(std::move(r),now,e);}
bool Controller::submit_scenery_stroke(const std::string& id,const SceneryStrokeIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=intent.kind==SceneryStrokeKind::Paint?Action::SceneryBrushStroke:Action::SceneryEraseStroke;r.action_uuid=id;
    r.canonical_record_key="scenery-stroke:"+id;r.scenery_stroke=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_duplicate(const std::string& id,const DuplicateIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::DuplicateSelection;r.action_uuid=id;r.canonical_record_key="duplicate:"+id;r.duplicate=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_group(const std::string& id,const GroupIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditGroup;r.action_uuid=id;r.canonical_record_key="groups:"+id;r.group=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_district_title(const std::string& id,const std::string& key,const DistrictTitleIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::MoveDistrictTitle;r.action_uuid=id;r.canonical_record_key=key;r.district_title=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_zone_edit(const std::string& id,const ZoneEditIntent& value,uint64_t now,std::string& error){
    Request request;request.action=Action::EditZone;request.action_uuid=id;request.canonical_record_key="zone-edit:"+id;request.zone_edit=value;
    return submit(std::move(request),now,error);
}
bool Controller::submit_district_stroke(const std::string& id,const DistrictStrokeIntent& value,uint64_t now,std::string& error){
    Request r;r.action=Action::DistrictStroke;r.action_uuid=id;r.canonical_record_key="district-stroke:"+id;r.district_stroke=value;return submit(std::move(r),now,error);
}
bool Controller::submit_district_property(const std::string& id,const std::string& key,const game::district_property::Value& value,uint64_t now,std::string& error){
    Request r;r.action=Action::EditDistrict;r.action_uuid=id;r.canonical_record_key=key;r.district_property=value;return submit(std::move(r),now,error);
}
bool Controller::submit_district_text(const std::string& id,const std::string& key,const DistrictTextIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditDistrictText;r.action_uuid=id;r.canonical_record_key=key;r.district_text=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_selection_move(const std::string& id,const SelectionMoveIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::MoveSelection;r.action_uuid=id;r.canonical_record_key="selection:"+id;r.selection_move=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_service_membership(const std::string& id,const std::string& key,const ServiceMembershipIntent& intent,uint64_t now,std::string& error){
    Request r;r.action=Action::EditServiceMembership;r.action_uuid=id;r.canonical_record_key=key;r.service_membership=intent;return submit(std::move(r),now,error);
}
bool Controller::submit_boss_phase(const std::string& id,const BossPhaseIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::ReorderBossPhase;request.action_uuid=id;request.canonical_record_key="character:3:"+std::to_string(intent.character_id);
    request.boss_phase=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_prop_text(const std::string& id,const std::string& target,const PropTextIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::EditPropText;request.action_uuid=id;request.canonical_record_key=target;
    request.prop_text=intent;return submit(std::move(request),now,error);
}
bool Controller::submit_npc_property(const std::string& id,const std::string& target,const NpcPropertyIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::EditNpcProperty;request.action_uuid=id;
    request.canonical_record_key=target;request.npc_property=intent;return submit(std::move(request),now,error);
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
bool Controller::submit_bug_fix(const std::string& id,const BugFixIntent& intent,uint64_t now,std::string& error){
    Request request;request.action=Action::SetBugFix;request.action_uuid=id;request.bug_fix=intent;
    request.canonical_record_key=intent.bug.parent_key;return submit(std::move(request),now,error);
}
bool Controller::submit_developer(bool hire,const std::string& id,uint64_t now,std::string& error){
    Request request;request.action=hire?Action::HireDeveloper:Action::FireDeveloper;request.action_uuid=id;
    request.canonical_record_key="staff:developers";return submit(std::move(request),now,error);
}
bool Controller::submit_start_research(const std::string& id,const std::string& key,uint64_t now,std::string& error){
    Request request;request.action=Action::StartResearch;request.action_uuid=id;
    request.canonical_record_key="tech:"+key;return submit(std::move(request),now,error);
}
bool Controller::submit_research_workers(const std::string& id,const std::string& key,int32_t desired,uint64_t now,std::string& error){
    Request request;request.action=Action::SetResearchWorkers;request.action_uuid=id;request.desired_workers=desired;
    request.canonical_record_key="tech:"+key;return submit(std::move(request),now,error);
}
bool Controller::submit_commit_research(const std::string& id,const std::string& key,uint64_t now,std::string& error){
    Request request;request.action=Action::CommitResearch;request.action_uuid=id;
    request.canonical_record_key="tech:"+key;return submit(std::move(request),now,error);
}
bool Controller::submit_commit_startingpoint(const std::string& id,uint64_t now,std::string& error){
    Request request;request.action=Action::CommitStartingpoint;request.action_uuid=id;
    request.canonical_record_key="tech:startingpoint";return submit(std::move(request),now,error);
}
bool Controller::submit_startingpoint_workers(const std::string& id,int32_t desired,uint64_t now,std::string& error){
    Request request;request.action=Action::SetStartingpointWorkers;request.action_uuid=id;request.desired_workers=desired;
    request.canonical_record_key="tech:startingpoint";return submit(std::move(request),now,error);
}
bool Controller::submit_start_startingpoint(const std::string& id,uint64_t now,std::string& error){
    Request request;request.action=Action::StartStartingpoint;request.action_uuid=id;
    request.canonical_record_key="tech:startingpoint";return submit(std::move(request),now,error);
}
bool Controller::submit(Request request,uint64_t now,std::string& error) {
    if(!guard(error)||!update_time(now,error))return false;
    if(pending_||local_request_)return fail("one action already pending; no queue",error);
    if(!valid_request(request))return fail("typed action UUID, object key or building payload invalid",error);
    for(const auto& [other,entry]:history_)if(entry.request.request.action_uuid==request.action_uuid)return fail("action UUID already used; no resubmission",error);
    if(local_sequence_==UINT64_MAX)return fail("request sequence exhausted",error,true);
    if(now_>UINT64_MAX-timeout_)return fail("ACK deadline timestamp exhausted",error,true);
    request.peer=context_.local_peer;request.sequence=++local_sequence_;request.host_revision=status_.host_revision;request.snapshot_uuid=context_.snapshot_uuid;
    Message m;m.session=context_.session;m.request=std::move(request);
    pending_=m;deadline_=now_+timeout_;status_.pending=true;status_.action_uuid=m.request.action_uuid;status_.outcome=Outcome::Pending;
    if(context_.role==Role::Host)return this->request(m,context_.local_peer,error);
    local_request_=m;remember(identity(m.request),m);
    if(!send(context_.host_peer,m,error)||!arm_deadline(local_deadline_,error))return false;
    deadline_=local_deadline_;return true;
}
bool Controller::observe(const Request& r,const Ledger& before,Evidence& evidence,std::string& error) {
    if(!invoke([&]{return engine_.observe_action(binding_,r,evidence,error);},error))return fail("native execution observation: "+error,error,true);
    Ledger actual;
    if(!execution(r,evidence)||evidence.before!=before||!invoke([&]{return engine_.ledger(binding_,actual,error);},error)||actual!=evidence.after)
        return fail("native execution receipt/ledger differs: "+error,error,true);
    if(!invoke([&]{return engine_.retire(binding_,r,error);},error))return fail("native receipt retirement: "+error,error,true);
    auto expected=std::make_unique<Evidence>(evidence);
    expected->retirement_confirmed=true;
    auto retired=std::make_unique<Evidence>();
    if(!invoke([&]{return engine_.observe_action(binding_,r,*retired,error);},error))
        return fail("native retirement observation: "+error,error,true);
    if(!complete(r,*retired))return fail("native retirement acknowledgment lacks complete execution evidence",error,true);
    // Retirement acknowledges the receipt; it must not change any action family.
    if(*retired!=*expected)return fail("native retirement receipt changed after retirement",error,true);
    evidence=std::move(*retired);return true;
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
    high_water_[sender]=m.request.sequence;remember(id,m);pending_=m;status_.pending=true;deadline_=now_+timeout_;
    status_.action_uuid=m.request.action_uuid;status_.required_mask=context_.peer_mask;status_.ack_mask=0;
    if(status_.host_revision==UINT64_MAX)return fail("host world revision exhausted",error,true);
    const bool revision_conflict=m.request.host_revision!=status_.host_revision;
    ++status_.host_revision;
    if(revision_conflict)return finish(Outcome::RejectedNoEffect,"request host revision conflict",error);
    ActionMeasurement measurement(m.request);
    Ledger before;
    if(!invoke([&]{return engine_.ledger(binding_,before,error);},error))return finish(status_.locked?Outcome::Unknown:Outcome::RejectedNoEffect,"host before ledger: "+error,error);
    if(!valid_ledger(before))return finish(Outcome::RejectedNoEffect,"host before ledger record_present requires 0 or 1",error);
    if(!invoke([&]{return engine_.capture_host_authority_before(binding_,m.request,before,status_.host_revision-1,error);},error))return finish(Outcome::RejectedNoEffect,"host authority before capture: "+error,error);
    measurement.checkpoint("action.host.capture");
    ++status_.attempted;busy_=true;Outcome outcome;
    try {outcome=engine_.execute_action(binding_,m.request,error);}catch(const std::exception& e){outcome=Outcome::Unknown;error=e.what();}
    catch(...){outcome=Outcome::Unknown;error="nonstandard native execution exception";}
    busy_=false;
    measurement.checkpoint("action.host.execute");
    if(status_.locked)return finish(Outcome::Unknown,status_.cause,error);
    if(outcome!=Outcome::Confirmed&&error.empty())error="native execution returned failure without original cause";
    if(outcome!=Outcome::Confirmed)return finish(outcome==Outcome::RejectedNoEffect?outcome:Outcome::Unknown,"host native action: "+error,error);
    Message result=m;result.kind=Kind::HostResult;result.revision=status_.host_revision;result.required_mask=context_.peer_mask;result.outcome=Outcome::Confirmed;
    if(!observe(m.request,before,result.evidence,error))return finish(Outcome::Unknown,error,error);
    measurement.checkpoint("action.host.observe");
    if(!invoke([&]{return engine_.publish_host_authority_before(result,error);},error))return finish(Outcome::Unknown,"host authority before publication: "+error,error);
    measurement.checkpoint("action.host.publish");
    auto& cached=history_.at(id);cached.result=result;cached.has_result=true;status_.ack_mask=bit(context_.local_peer);
    for(uint64_t peer=1;peer<=4;++peer)if(peer!=context_.local_peer&&(context_.peer_mask&bit(peer)))if(!send(peer,result,error))return finish(Outcome::Unknown,error,error);
    if(status_.ack_mask==status_.required_mask)return finish(Outcome::Confirmed,"",error);
    return arm_deadline(deadline_,error);
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
    if(!authority_waiting_){
        // A matching HostResult supersedes the request wait, even when its
        // BEFORE authority needs a separate finite wait before native apply.
        if(!arm_deadline(deadline_,error))return false;
        if(local_request_&&same_action(*local_request_,m))local_deadline_=0;
    }
    AuthorityReadiness ready=AuthorityReadiness::Rejected;
    if(!invoke([&]{ready=engine_.client_authority_before_ready(m,m.revision-1,error);return true;},error))return fail("client authority readiness: "+error,error,true);
    if(ready==AuthorityReadiness::Rejected)return fail("client authority before rejected: "+error,error,true);
    if(ready==AuthorityReadiness::Wait) {
        if(!authority_waiting_) {authority_waiting_=m;pending_=m;status_.pending=true;status_.action_uuid=m.request.action_uuid;status_.required_mask=context_.peer_mask;}
        if(status_.cause!=error&&logger_)logger_("info","ActionSession",error);status_.cause=error;return true;
    }
    authority_waiting_.reset();status_.cause.clear();
    Message original=m;original.kind=Kind::Request;original.revision=0;original.required_mask=0;original.outcome=Outcome::Pending;original.evidence={};
    auto& cached=remember(id,original);cached.result=m;cached.has_result=true;
    last_result_revision_=m.revision;status_.host_revision=m.revision;status_.required_mask=context_.peer_mask;
    pending_=m;status_.pending=true;status_.action_uuid=m.request.action_uuid;
    Message ack=m;ack.kind=Kind::ApplyAck;ack.ack_peer=context_.local_peer;
    ActionMeasurement measurement(m.request);
    Ledger before;
    if(!invoke([&]{return engine_.apply_client_authority_before(binding_,m,m.revision-1,error);},error)){ack.outcome=Outcome::Unknown;ack.cause="client authority before apply: "+error;}
    else if(!invoke([&]{return engine_.ledger(binding_,before,error);},error)||before!=m.evidence.before){ack.outcome=Outcome::Unknown;ack.cause="client before ledger differs from authority: "+error;}
    else {
        measurement.checkpoint("action.client.authority");
        ++status_.attempted;busy_=true;Outcome applied;
        try{applied=engine_.apply_action(binding_,m.request,m.evidence,error);}catch(const std::exception& e){applied=Outcome::Unknown;error=e.what();}
        catch(...){applied=Outcome::Unknown;error="nonstandard native apply exception";}
        busy_=false;
        measurement.checkpoint("action.client.apply");
        if(status_.locked){applied=Outcome::Unknown;error=status_.cause;}
        if(applied!=Outcome::Confirmed&&error.empty())error="native apply returned failure without original cause";
        if(applied!=Outcome::Confirmed){ack.outcome=Outcome::Unknown;ack.cause="client native apply: "+error;}
        else {
            Evidence local;
            if(!observe(m.request,before,local,error)||!authority_evidence(m.request,local,m.evidence)){ack.outcome=Outcome::Unknown;ack.cause="client after ledger/native receipt differs from authority: "+error;}
            else ack.evidence=local;
            measurement.checkpoint("action.client.observe");
        }
    }
    history_.at(id).ack=ack;history_.at(id).has_ack=true;
    const bool sent=send(context_.host_peer,ack,error);
    if(ack.outcome!=Outcome::Confirmed)return fail(ack.cause,error,true);
    if(!sent)return false;
    return arm_deadline(deadline_,error);
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
    if(!authority_evidence(m.request,m.evidence,old->second.result.evidence))return finish(Outcome::Unknown,"peer="+std::to_string(sender)+" ledger/native receipt differs from Host",error);
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
        remember(id,original);old=history_.find(id);
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
    if(cache){auto& entry=remember(identity(request.request),request);entry.completion=rejected;entry.has_completion=true;}
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
    if(!guard(error)||!update_time(now,error))return false;
    if(sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&bit(sender)))return fail("untrusted transport sender",error);
    bool state_handled=false;if(!invoke([&]{return engine_.receive_authority_state(sender,bytes,size,state_handled,error);},error))return fail("authority state receive: "+error,error);
    if(state_handled)return true;
    Message m;if(!decode(bytes,size,m,error))return fail(error,error);
    if(sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&bit(sender)))return fail("untrusted transport sender",error);
    if(m.session!=context_.session||m.request.snapshot_uuid!=context_.snapshot_uuid)return fail("packet session or snapshot differs",error);
    if(m.kind!=Kind::Request&&m.required_mask!=context_.peer_mask)return fail("packet immutable participant mask differs",error,true);
    if(m.kind!=Kind::Request&&m.kind!=Kind::ApplyAck&&sender!=context_.host_peer)return fail("result/completion sender is not Host",error);
#ifdef MT2MP_FRAMEWORK_PLUGIN
    reporting::event("action.received."+std::to_string(unsigned(m.kind)),{{"action_sequence",m.request.sequence},{"action_type",uint64_t(m.request.action)},
        {"revision",m.revision},{"peer_slot",sender},{"required_mask",m.required_mask},{"ack_mask",uint64_t(status_.ack_mask)},
        {"bytes",uint64_t(size)}},m.request.action_uuid);
#endif
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
    if(!guard(error)||!update_time(now,error))return false;
    if(authority_waiting_&&now_<deadline_) {const auto waiting=*authority_waiting_;if(!result(waiting,error))return false;}
    if(!update_time(now_,error))return false;
    if((pending_&&now_>=deadline_)||(local_request_&&local_deadline_&&now_>=local_deadline_)) {
        const auto expired_deadline=local_request_&&local_deadline_?std::min(deadline_,local_deadline_):deadline_;
        const auto cause="native apply ACK observation timeout now_ms="+std::to_string(now_)+" deadline_ms="+std::to_string(expired_deadline)+
            " actual_mask="+std::to_string(status_.ack_mask)+" required_mask="+std::to_string(status_.required_mask)+"; no native retry";
        if(context_.role==Role::Host)return finish(Outcome::Unknown,cause,error);
        return fail(cause,error,true);
    }
    error.clear();return true;
}
}
