namespace building_mixed_identity::opened_file {using CurrentWorld=identity_refund_fixture::World;}
namespace native_open_runtime {bool read_current_world(World& out,std::string& e){return current(out,e);}}
namespace remove_diagnostics {
bool bindings(json& out,std::string& e){return identity_refund_fixture::bindings(out,e);}
namespace scene_scenery {
bool registered(const std::string& snapshot,const std::string& record,uint64_t uid){
    const auto found=registry.find(registry_id(snapshot,record));
    return found!=registry.end()&&found->second.uid==uid&&!found->second.scene_owner.empty()&&found->second.scene_owner!="world"&&found->second.scene_owner!="retired";
}
}
}
namespace world_state {
using Origin=mt2mp::world_state::Origin;using SnapshotObject=mt2mp::world_state::SnapshotObject;using Table=mt2mp::world_state::Table;
mt2mp::world_state::Building building_state(const native::BuildingState& readback){
    mt2mp::world_state::Building result;result.type=readback.type;result.variant=readback.variant;result.x=readback.prop.x;result.y=readback.prop.y;result.z=readback.prop.z;result.rotation=readback.rotation;result.scale=readback.scale;result.alive=true;return result;
}
}
namespace developer_staff::construction {
using LocalJob=::developer_staff::construction::LocalJob;
unsigned fixture_loaded_calls=0;
bool capture_loaded(const std::string& snapshot,const action::Binding& binding,std::vector<LocalJob>& out,std::string& e){
    ++fixture_loaded_calls;out.clear();
    if(snapshot!=fixture_session_snapshot||binding.native_epoch!=live_world.world_epoch||
       binding.native_generation!=live_world.generation||
       mt2mp::build_actions::private_native::host::RootSceneryReadBatch::active)
        return fail(e,"continuation empty construction snapshot/native binding or read scope differs");
    // This complete refund-census fixture has no native construction Jobs.
    // Its adapter must not inherit another fixture's loaded-Job failure state.
    e.clear();return true;
}
}
struct JoinedEngine {
    world_state::Table world_;uint32_t thread_=0;
    action::Binding loaded_construction_binding_{};std::string loaded_construction_snapshot_;
    std::vector<developer_staff::construction::LocalJob> loaded_construction_;
    bool current(const action::Binding& binding,const std::string& snapshot,std::string& e){
        return (binding.native_epoch==live_world.world_epoch&&binding.native_generation==live_world.generation&&snapshot==fixture_session_snapshot)||fail(e,"fixture current Engine binding differs");
    }
#include "empty_world_bind_body.inc"
};
std::string natural_setup(){
    reset();developer_staff::construction::fixture_loaded_calls=0;scene_target=saved_only=true;
    const auto source=registry_id(::uuid,key(101));auto entry=registry.at(source);registry.erase(source);
    entry.key=std::string(64,'c')+":world:scenery:inline:region{59}/sceneryManager/type[8]/scenery[0]";
    entry.scene_record=::fixture_sha+":world:scenery:inline:region{59}/sceneryManager/type[8]/scenery[0]";
    entry.scene_owner="world";entry.object=object(101);entry.class_addresspoint=fixture_image+0x16c1390;
    auto& state=live.at(101);state.type="cactus";state.variant="p_ext_treecactus_01.vmb";
    entry.metadata["type"]=state.type;entry.metadata["variant"]=state.variant;
    const auto result=entry.key;registry.emplace(registry_id(::uuid,result),std::move(entry));return result;
}
void continuation_checks(){
    std::string e;LocalTarget selected;json report;
    auto canonical=natural_setup();JoinedEngine engine;action::Binding binding;
    need(engine.bind(::uuid,binding,e)&&engine.world_.rows().size()==4&&saved_prop_reads==2&&refund_calls==4&&registered_reads==8,
         "actual Engine bind -> full collector -> original refund reader -> real World table failed saved-only Root Scenery");
    need(developer_staff::construction::fixture_loaded_calls==1&&engine.loaded_construction_.empty()&&
         engine.loaded_construction_snapshot_==fixture_session_snapshot&&
         engine.loaded_construction_binding_.native_epoch==live_world.world_epoch&&
         engine.loaded_construction_binding_.native_generation==live_world.generation,
         "actual continuation bind did not independently admit the empty construction queue");
    need(engine.world_.rows().at(canonical).snapshot_record_key==::fixture_sha+":world:scenery:inline:region{59}/sceneryManager/type[8]/scenery[0]"&&engine.world_.origin().sha256==::fixture_sha,
         "complete baseline lost persistent Root key vs current original saved record");
    std::vector<LocalTarget> census;canonical=natural_setup();
    need(identity_targets(census,e)&&census.size()==4&&saved_prop_reads==1&&refund_calls==0,"actual complete read-only identity census omitted saved Root Scenery or invoked refund getters");
    need(std::count_if(census.begin(),census.end(),[&](const auto& row){return row.record_key==canonical&&row.uid==101&&row.normal_scenery;})==1,
         "Service/Thought/Release census lost canonical Scenery member");
    for(bool by_key:{false,true}){
        canonical=natural_setup();e.clear();
        need(!(by_key?resolve_record_target(::uuid,canonical,selected,e):resolve_local_target(fixture_selection,object(101),selected,e))&&e=="remove_region_absent_root_map"&&saved_prop_reads==0,
             "readonly metadata path relaxed ordinary selected or record action admission");
    }
    canonical=natural_setup();saved_owner_failure=true;JoinedEngine failed_engine;e.clear();
    need(!failed_engine.bind(::uuid,binding,e)&&e=="original saved Root Scenery owner changed"&&failed_engine.world_.rows().empty()&&refund_uids==std::vector<uint64_t>{102,103,104},
         "failed baseline saved owner proof fabricated World rows or called the failed member refund getter");
    canonical=natural_setup();saved_owner_failure=true;e.clear();
    need(!identity_targets(census,e)&&e=="original saved Root Scenery owner changed","read-only complete census lost original saved owner failure");
    canonical=natural_setup();refund_failure=101;JoinedEngine refund_engine;e.clear();
    need(!refund_engine.bind(::uuid,binding,e)&&e=="fixture foreign refund getter failed"&&refund_engine.world_.rows().empty(),"complete bind suppressed original native refund failure");
    canonical=natural_setup();registry.at(registry_id(::uuid,canonical)).object=object(101)+8;e.clear();
    need(!identity_targets(census,e)&&e=="native UI target type/coordinates or snapshot registry differs","metadata census admitted a replaced canonical receiver");
    canonical=natural_setup();heap_failure=true;e.clear();need(!identity_targets(census,e)&&e=="fixture heap receipt failed","metadata census lost original heap/World authority failure");
}
