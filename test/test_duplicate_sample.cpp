#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "duplicate_reader.h"
#include "duplicate_records.h"
#include "region_saved_identity.h"
#include "typed_action/action_session.h"
#include "scenery_stroke_state.h"
#include <functional>

namespace sample_fixture {
namespace action=mt2mp::action_session;
namespace q=mt2mp::game::npc_quest_targets;
namespace duplicate_reader=mt2mp::game::duplicate_reader;
namespace duplicate_native=mt2mp::game::duplicate_native;
namespace duplicate_records=mt2mp::game::duplicate_records;
namespace group_reader=mt2mp::game::group_reader;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
namespace record_graph=mt2mp::subscriber_lifecycle;
using Family=action::DuplicateFamily;
using Handle=duplicate_native::Handle;
using json=nlohmann::json;
struct Config {bool diagnostic_mailbox_enabled=true;static Config& instance(){static Config value;return value;}};
constexpr uintptr_t map_owner=f::root-0x1c0,building_manager=0xb0000,scenery_manager=0xc0000;
constexpr uintptr_t building=0xd0000,fixture_release_manager=0xe0000,release=0xf0000,random=0x100000,business=0x110000;
constexpr uint64_t fixture_building_uid=0x400000007;
const std::string building_key="action:11111111-1111-4111-8111-111111111111";
const std::string scenery_key="action:22222222-2222-4222-8222-222222222222";
struct Memory:f::Memory {int process=0;};
struct Runtime {Memory memory;uintptr_t image=f::image;uint32_t thread=7;bool ready=true;};
namespace host {
struct Origin {uint64_t world_epoch=3,generation=4;void* map=reinterpret_cast<void*>(map_owner);bool operator==(const Origin&)const=default;};
struct WorldState {Origin origin;uintptr_t root=f::root,selection=f::selection;};
bool same_world(const Origin& first,const Origin& last){return first==last;}
}
namespace witness {
struct UndoWitness {uintptr_t active_graph=0;bool operator==(const UndoWitness&)const=default;};
bool read_undo(int&,uintptr_t,UndoWitness& out,std::string&){out={};return true;}
}
host::WorldState current_world;
bool world(Runtime&,host::WorldState& out,std::string&){out=current_world;return true;}
bool ledger(action::Ledger& out,std::string&){out={};out.cash=94600;return true;}
namespace net {std::string current_snapshot_id(){return "fixture-snapshot";}}
std::map<uint64_t,uintptr_t> uid_objects;
namespace group_edit {
bool find(Runtime&,uint64_t uid,uintptr_t& out,std::string&){const auto at=uid_objects.find(uid);out=at==uid_objects.end()?0:at->second;return true;}
bool canonical_group(const action::Binding&,const host::WorldState&,uint64_t,const std::string& derived,std::string& key,std::string& e){
    if(derived.empty())return q::fail(e,"fixture group identity is absent");key=derived;return true;}
}
namespace remove_diagnostics {
json rows;
unsigned fixture_identity_reads=0,fixture_refund_reads=0;
std::string fixture_identity_failure;
bool bindings(json&,std::string& error){++fixture_refund_reads;return q::fail(error,"fixture unexpected refund hydration");}
bool identity_bindings(json& out,std::string& error){
    ++fixture_identity_reads;if(!fixture_identity_failure.empty())return q::fail(error,fixture_identity_failure);
    out={{"bindings",rows}};return true;
}
}
namespace native {struct BuildingState {struct Prop{uint64_t uid=0;} prop;action::BuildingState value;};}
namespace scenery_kiss {
bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& e){
    if(!uid_objects.contains(uid))return q::fail(e,"fixture actual prop UID absent");out.prop.uid=uid;
    out.value={};out.value.type=uid==fixture_building_uid?"SmallInn":"cactus";out.value.variant=uid==fixture_building_uid?"base":"p_ext_treecactus_01.vmb";out.value.alive=true;return true;}
}
namespace world_state {action::BuildingState building_state(const native::BuildingState& value){return value.value;}}
namespace npc_saved_identity_runtime {
struct Current {std::string key;struct Row{uintptr_t local_pointer=0;uint64_t local_uid=0;}row;};
bool read_current(std::vector<Current>& out,std::string&){out.clear();return true;}
}
#include "duplicate_sample_type_actual.inc"
#include "duplicate_sample_owner_actual.inc"
std::map<uintptr_t,record_graph::Graph> records;
std::vector<Handle> captured;
uintptr_t failed_record=0;
bool change_during_record=false;
// The native save callback supplies a complete original graph. Use the actual
// record summarizer and actual external-owner resolver after this boundary.
bool record_object(Runtime& runtime,const Sample& sample,const Handle& handle,action::DuplicateObject& row,std::string& error,json* diagnostic=nullptr,duplicate_records::UptimeWitness* uptime=nullptr){
    captured.push_back(handle);
    if(handle.object==failed_record)return q::fail(error,"fixture original SaveRecord failed");
    const auto found=records.find(handle.object);
    if(found==records.end())return q::fail(error,"fixture original graph absent");
    auto owner=[&](uintptr_t pointer,std::string& key,std::string& cause){
        return external_owner(runtime,sample,pointer,key,cause);
    };
    duplicate_records::Witness value;
    if(!duplicate_records::summarize_source(found->second,handle.object,owner,value,error,diagnostic,uptime))return false;
    row.record_sha=value.sha;
    if(change_during_record&&handle.object==f::scenery){
        auto pose=f::pose();pose[4]=0x3f800000;runtime.memory.put(f::scenery+0x100,pose);
    }
    return true;
}
bool record(Runtime& runtime,const Sample& sample,const std::string& key,action::DuplicateObject& row,std::string& error,json* diagnostic=nullptr,duplicate_records::UptimeWitness* uptime=nullptr){
    return record_object(runtime,sample,sample.handles.at(key),row,error,diagnostic,uptime);
}
#include "duplicate_sample_actual.inc"

json binding(uint64_t uid,const std::string& key,bool revoked=false){
    return {{"local_uid",uid},{"record_key",key},{"revoked",revoked},{"unknown",false},{"snapshot_id",net::current_snapshot_id()}};
}
record_graph::Graph graph(uintptr_t self,uint32_t content){
    record_graph::Graph result;
    result.records.push_back({SIZE_MAX,0,{1,0,"original"},{record_graph::Token{4,content,{}}}});
    if(self==building){
        result.records.push_back({0,0,{1,0,"uptime"},{}});result.records.push_back({1,0,{1,0,"history"},{}});
        for(size_t i=0;i<9;++i)result.records.push_back({2,i,{},{record_graph::Token{4,1,{}}}});
    }
    result.private_links={{1,self,false},{2,f::root,false},{3,f::region,false}};
    return result;
}
Runtime setup(){
    Runtime result;static_cast<f::Memory&>(result.memory)=f::setup();auto& m=result.memory;
    current_world={};captured.clear();failed_record=0;change_during_record=false;
    remove_diagnostics::fixture_identity_reads=0;remove_diagnostics::fixture_refund_reads=0;remove_diagnostics::fixture_identity_failure.clear();
    f::array(m,f::manager+0x18,0x16f0060,{});
    m.put(map_owner,f::image+0x1742860);m.put(f::root,f::image+0x174ad20);m.put(f::root+0x48,int32_t(0));
    f::array(m,map_owner+0x230,0x16f04e0,{f::region});
    f::array(m,map_owner+0x350,0,{f::region});
    m.put(f::region,f::image+0x1787930);m.put(f::region+0x70,f::root);m.put(f::region+0x834,int32_t(67));
    m.put(f::region+0x950,building_manager);m.put(f::region+0x960,scenery_manager);
    m.put(building_manager,f::image+0x17183c0);m.put(building_manager+0x450,f::root);m.put(building_manager+0x458,f::region);
    f::array(m,building_manager+0x408,0x172d7c0,{building});
    m.put(scenery_manager,f::image+0x17152d0);m.put(scenery_manager+0x900,f::region);m.put(scenery_manager+0x908,f::root);
    for(size_t category=0;category<47;++category)
        f::array(m,scenery_manager+0x18+category*0x30,0x172d6a0,category==3?std::vector<uintptr_t>{f::scenery,f::free_prop}:std::vector<uintptr_t>{});
    for(auto pointer:{f::scenery,f::free_prop}){
        m.put(pointer+0x178,f::region);m.put(pointer+0x268,int32_t(3));m.put(pointer+0x180,uintptr_t(0));
    }
    m.put(building,f::image+0x16d93c0);m.put(building+0x18,fixture_building_uid);m.put(building+0x170,f::root);
    m.put(building+0x178,f::region);m.put(building+0x180,uintptr_t(0));m.put(building+0x100,f::pose());
    m.put(f::image+0x164b250,random);m.put(random,std::array<uint64_t,4>{17,18,19,20});
    m.put(f::image+0x13df4f0,fixture_release_manager);m.put(fixture_release_manager,f::image+0x1714ed0);m.put(fixture_release_manager+0x38,release);
    m.put(release,f::image+0x16c12b0);m.put(release+0x120,std::array<int32_t,8>{8,7,6,5,4,3,2,1});m.put(f::image+0x13df220,business);
    uid_objects={{fixture_building_uid,building},{f::member_uid,f::scenery},{f::free_uid,f::free_prop},{f::group_uid,f::group}};
    remove_diagnostics::rows=json::array({binding(fixture_building_uid,building_key),binding(33,"action:retired",true)});
    records={{building,graph(building,10)},{f::scenery,graph(f::scenery,20)},{f::free_prop,graph(f::free_prop,30)},{f::group,graph(f::group,40)}};
    return result;
}
}
#include "building_clearance_native_ports.inc"
#include "building_clearance_native_cases.inc"
namespace group_sample_fixture {
namespace action=mt2mp::action_session;
namespace q=mt2mp::game::npc_quest_targets;
namespace duplicate_edit=sample_fixture;
namespace duplicate_transition=mt2mp::game::duplicate_transition;
namespace scenery_stroke_state=mt2mp::game::scenery_stroke_state;
using Runtime=sample_fixture::Runtime;
using json=nlohmann::json;
#include "group_object_sample_actual.inc"
#include "group_object_trim_actual.inc"
}
int main()try{
    sample_fixture::clearance_cases::run();
    using namespace sample_fixture;
    const action::Binding admitted{3,4};std::string error;Sample actual;
    auto runtime=setup();
    remove_diagnostics::fixture_identity_failure="fixture current canonical identity unavailable";
    f::need(!sample_duplicate(runtime,admitted,nullptr,{},actual,error)&&error==remove_diagnostics::fixture_identity_failure&&
        remove_diagnostics::fixture_identity_reads==1&&remove_diagnostics::fixture_refund_reads==0&&captured.empty(),
        "Identity admission failure was hidden or original Record capture continued");
    runtime=setup();error.clear();
    f::need(!sample(runtime,admitted,nullptr,{},actual,error)&&error=="Duplicate Group member lacks current canonical Scenery key",
        "strict shared sample must still reject unrelated unbound Scenery");
    runtime=setup();f::need(sample_duplicate(runtime,admitted,nullptr,{},actual,error),error);
    f::need(remove_diagnostics::fixture_identity_reads==1&&remove_diagnostics::fixture_refund_reads==0,
        "Duplicate sampled unused native refunds instead of complete canonical identities");
    f::need(actual.selected_records.empty(),"unselected whole-world capture retained Record diagnostics");
    const auto initial=actual;
    f::need(actual.census.objects.size()==3&&actual.untouched_scenery.size()==2&&actual.objects.size()==1&&
        actual.handles.size()==1&&captured.size()==3,"Duplicate lost native census or complete original record capture");
    f::need(!actual.handles.contains("")&&actual.groups.state.props.empty(),"witness-only Scenery acquired canonical/group identity");
    action::DuplicateIntent intent;action::DuplicateTarget target;target.key=building_key;
    target.family=Family::Building;target.region=67;target.before=f::pose();target.after=f::pose();intent.rows.push_back(target);
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.state.sources.size()==1&&
        actual.state.sources[0].key==building_key&&actual.untouched_scenery.size()==2,"exact selected Building or untouched witnesses lost");
    f::need(actual.selected_records.size()==1&&actual.selected_records.contains(building_key)&&
        actual.selected_records.at(building_key).at("record_sha")==actual.state.sources[0].record_sha,
        "actual selected-source sample lost its original Record witness or included unrelated world records");
    const auto selected=actual;
    {
        auto uptime_intent=intent;uptime_intent.rows[0].record_sha=selected.state.sources[0].record_sha;
        uptime_intent.rows[0].captured_uptime=initial.uptime.at(building_key).actual;
        records.at(building).records.push_back({2,9,{},{record_graph::Token{4,1,{}}}});captured.clear();
        Sample grown;f::need(sample_duplicate(runtime,admitted,&uptime_intent,{},grown,error),error);
        const auto& proof=grown.uptime.at(building_key);
        f::need(proof.actual==std::vector<int32_t>(10,1)&&proof.historical_sha==selected.state.sources[0].record_sha&&
            grown.state.sources[0].record_sha!=selected.state.sources[0].record_sha&&grown.state.untouched_sha==selected.state.untouched_sha&&captured.size()==3,
            "actual sample lost full captured/current records or repeated native Save for uptime proof");
        records.at(building).records[0].tokens[0].bits++;
        f::need(sample_duplicate(runtime,admitted,&uptime_intent,{},grown,error)&&grown.uptime.at(building_key).historical_sha!=uptime_intent.rows[0].record_sha,
            "actual sample hid unrelated selected Building content drift");
        records.at(building)=graph(building,10);
    }
    Config::instance().diagnostic_mailbox_enabled=false;
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.selected_records.empty()&&actual.state==selected.state,
        "disabled diagnostic mailbox changed authority state or retained selected Record details");
    Config::instance().diagnostic_mailbox_enabled=true;
    {
        runtime=setup();
        const std::string result_key="action:33333333-3333-4333-8333-333333333333";
        remove_diagnostics::rows.push_back(binding(f::member_uid,scenery_key));
        remove_diagnostics::rows.push_back(binding(f::free_uid,result_key));
        action::DuplicateIntent paired;action::DuplicateTarget source;source.key=scenery_key;source.family=Family::Scenery;
        source.created_keys={result_key};paired.rows.push_back(source);captured.clear();
        f::need(sample_duplicate(runtime,admitted,&paired,{{f::free_uid,result_key}},actual,error),error);
        f::need(actual.selected_records.size()==2&&actual.selected_records.contains(scenery_key)&&actual.selected_records.contains(result_key)&&
            !actual.selected_records.contains(building_key)&&captured.size()==3,
            "actual sample must retain selected/created Records from the same original captures without unrelated detail or extra Save");
        const auto with_diagnostics=actual.state;Config::instance().diagnostic_mailbox_enabled=false;captured.clear();
        f::need(sample_duplicate(runtime,admitted,&paired,{{f::free_uid,result_key}},actual,error)&&actual.selected_records.empty()&&
            actual.state==with_diagnostics&&captured.size()==3,"created Record diagnostics changed full authority state or Save count");
        Config::instance().diagnostic_mailbox_enabled=true;runtime=setup();
    }
    // Changing local UIDs and manager order must not change portable content.
    runtime.memory.put(f::scenery+0x18,uint64_t(700));runtime.memory.put(f::free_prop+0x18,uint64_t(800));
    uid_objects.erase(f::member_uid);uid_objects.erase(f::free_uid);uid_objects.emplace(700,f::scenery);uid_objects.emplace(800,f::free_prop);
    f::array(runtime.memory,scenery_manager+0x18+3*0x30,0x172d6a0,{f::free_prop,f::scenery});
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.state==selected.state,
        "untouched original content digest leaked native UID or manager order");
    constexpr uintptr_t relocated=0x130000;
    const auto prior_bytes=runtime.memory.bytes;
    for(const auto& [address,value]:prior_bytes)if(address>=f::scenery&&address<f::scenery+0x280)
        runtime.memory.bytes[relocated+address-f::scenery]=value;
    auto relocated_record=records.at(f::scenery);relocated_record.private_links[0].object=relocated;
    records.erase(f::scenery);records.emplace(relocated,std::move(relocated_record));uid_objects[700]=relocated;
    f::array(runtime.memory,scenery_manager+0x18+3*0x30,0x172d6a0,{f::free_prop,relocated});
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.state==selected.state&&
        actual.untouched_scenery.contains(relocated)&&!actual.untouched_scenery.contains(f::scenery),
        "portable content leaked native pointer or local census retained the old pointer");
    records.at(relocated).records[0].tokens[0].bits++;
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.state.untouched_sha!=selected.state.untouched_sha,
        "changed untouched original record was omitted");
    runtime=setup();auto pose=f::pose();pose[4]=0x3f800000;runtime.memory.put(f::scenery+0x100,pose);
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.state.untouched_sha!=selected.state.untouched_sha,
        "changed untouched full pose was omitted");
    runtime=setup();records.at(f::free_prop)=graph(f::free_prop,20);
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error),error);const auto duplicate_content=actual.state.untouched_sha;
    f::array(runtime.memory,scenery_manager+0x18+3*0x30,0x172d6a0,{f::scenery});
    f::need(sample_duplicate(runtime,admitted,&intent,{},actual,error)&&actual.state.untouched_sha!=duplicate_content,
        "untouched witness multiplicity was collapsed");
    auto rejected=[&](auto change,const std::string& cause){
        runtime=setup();change();error.clear();
        f::need(!sample_duplicate(runtime,admitted,&intent,{},actual,error)&&error.find(cause)!=std::string::npos,
            "expected distinct rejection: "+cause+" actual="+error);
    };
    rejected([&]{runtime.memory.put(f::scenery+0x180,f::group);},"unbound Scenery has a Group parent");
    rejected([&]{remove_diagnostics::rows.push_back(binding(f::member_uid,"action:retired",true));},"live retired binding");
    rejected([&]{remove_diagnostics::rows[0]["unknown"]=true;},"registry is unknown or foreign");
    rejected([&]{remove_diagnostics::rows[0]["snapshot_id"]="foreign";},"registry is unknown or foreign");
    rejected([&]{remove_diagnostics::rows.push_back(binding(fixture_building_uid,"action:other"));},"registry full UID repeats");
    rejected([&]{remove_diagnostics::rows=json::array();},"native census contains unbound prop");
    rejected([&]{current_world.origin.generation++;},"World binding differs");
    rejected([&]{runtime.memory.put(f::scenery+0x18,uint64_t(777));},"manager class/full UID");
    rejected([&]{failed_record=f::scenery;},"original SaveRecord failed");
    rejected([&]{change_during_record=true;},"untouched native census changed during original records");
    rejected([&]{records.at(building).private_links.push_back({4,f::scenery,false});},"external owner has no current canonical binding");
    rejected([&]{records.at(f::scenery).private_links.push_back({4,f::free_prop,false});},"external owner has no current canonical binding");
    runtime=setup();auto missing=intent;missing.rows[0].key="unbound-natural";
    f::need(!sample_duplicate(runtime,admitted,&missing,{},actual,error)&&error.find("requested source absent")!=std::string::npos,
        "witness-only Scenery became a selectable Duplicate source");
    // Canonical Scenery and a Group keep the same full ownership path.
    runtime=setup();remove_diagnostics::rows.push_back(binding(f::member_uid,scenery_key));
    f::array(runtime.memory,f::manager+0x18,0x16f0060,{f::group});runtime.memory.put(f::scenery+0x180,f::group);
    f::need(sample_duplicate(runtime,admitted,nullptr,{},actual,error)&&actual.groups.state.groups.size()==1&&
        actual.objects.at(scenery_key).parent==actual.groups.state.groups[0].key&&actual.untouched_scenery.size()==1,
        "mixed canonical Group/member and unrelated Scenery were not retained");
    runtime=setup();remove_diagnostics::rows.push_back(binding(f::member_uid,scenery_key));
    remove_diagnostics::rows.push_back(binding(f::free_uid,"action:33333333-3333-4333-8333-333333333333"));
    Sample strict;f::need(sample(runtime,admitted,nullptr,{},strict,error)&&
        sample_duplicate(runtime,admitted,nullptr,{},actual,error)&&actual.state==strict.state,
        "fully canonical world changed its existing digest or strict behavior");
    // Group Move/Destroy keeps unrelated original records in its own digest.
    runtime=setup();remove_diagnostics::rows.push_back(binding(f::member_uid,scenery_key));
    f::array(runtime.memory,f::manager+0x18,0x16f0060,{f::group});runtime.memory.put(f::scenery+0x180,f::group);
    action::GroupObjectIntent group_intent;group_intent.key="group:"+scenery_key;group_intent.members={scenery_key};
    group_sample_fixture::Sample group_before,group_after;
    f::need(group_sample_fixture::sample(runtime,admitted,group_intent,group_before,error)&&
        group_before.state.records.sources.size()==2&&group_before.props.untouched_scenery.size()==1,
        "Group object sample lost selected Group/member or unrelated natural Scenery");
    records.at(f::free_prop).records[0].tokens[0].bits++;
    f::need(group_sample_fixture::sample(runtime,admitted,group_intent,group_after,error)&&
        group_after.state.records.untouched_sha!=group_before.state.records.untouched_sha&&
        group_after.state.records.sources==group_before.state.records.sources,
        "Group object untouched digest omitted unrelated original Scenery content");
    const auto retained_state=group_after.state;
    group_sample_fixture::trim(group_after);
    f::need(group_after.props.untouched_scenery.empty()&&group_after.props.census.objects.empty()&&
        group_after.state==retained_state,"Group history trim lost authoritative digest or kept local witness rows");
    runtime=setup();remove_diagnostics::rows.push_back(binding(f::member_uid,scenery_key));
    f::array(runtime.memory,f::manager+0x18,0x16f0060,{f::group});runtime.memory.put(f::scenery+0x180,f::group);
    records.at(building).private_links.push_back({4,f::free_prop,false});
    f::need(!group_sample_fixture::sample(runtime,admitted,group_intent,group_after,error)&&
        error.find("external owner has no current canonical binding")!=std::string::npos,
        "Group object witness-only row authorized an external reference");
    (void)initial;
    std::cout<<f::checks<<" actual Duplicate sample/census/record/reference/event-scope cases passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<problem.what()<<'\n';return 1;}
