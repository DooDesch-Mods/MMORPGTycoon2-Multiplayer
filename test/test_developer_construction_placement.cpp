#include "developer_staff/construction_native.h"
#include "multiplayer_world/world_state.h"
#include "building_clearance_runtime.h"
#include "bug_authority/codec.h"
namespace bug_authority=mt2mp::game::bug_authority;
#include <atomic>
#include <bit>
#include <regex>
#include "duplicate_transition.h"
#include "scenery_scene_key.h"
namespace mt2mp::action_session {
#include "construction_duplicate_valid.inc"
}
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include "placement_kiss/placement_adapter.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
namespace action=mt2mp::action_session;
namespace developer_staff=mt2mp::game::developer_staff;
namespace world_state=mt2mp::world_state;
using json=nlohmann::json;
namespace fixture {
unsigned checks=0,places=0,finishes=0,captures=0,currents=0;bool capture_ok=true,finish_ok=true,commit_ok=true,current_ok=true,no_job=false;
bool* expected_retired_before_place=nullptr;
bool actual_scenery=false;
std::vector<std::string> calls;
void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
action::Request request;action::Context context;action::Binding binding{7,9};action::BuildingState building;
}
namespace native {
struct BuildingState:action::BuildingState {
    struct Prop {uint64_t uid=0;float x=0,y=0,z=0;} prop;
    BuildingState& operator=(const action::BuildingState& in){static_cast<action::BuildingState&>(*this)=in;prop={500,in.x,in.y,in.z};return *this;}
};
struct SimulationState {int64_t cash=0;};
bool read_simulation_state(SimulationState&,std::string&);
}
namespace remove_diagnostics {
struct LocalTarget {bool normal_scenery=false,normal_building=true;uint64_t uid=500;};
bool admit_created_dungeon_building(const std::string&,const std::string&,uint64_t,const std::string&,std::string&){
    throw std::runtime_error("Ordinary construction fixture entered Dungeon admission");
}
bool admit_created_building(const std::string&,const std::string&,uint64_t,std::string&){fixture::calls.push_back("admit");return true;}
bool resolve_record_target(const std::string&,const std::string&,LocalTarget& out,std::string&){fixture::calls.push_back("resolve");out.normal_scenery=fixture::actual_scenery;out.normal_building=!out.normal_scenery;return true;}
}
namespace mt2mp::world_state {
bool bind_root_readback(const Origin&,const std::string& key,const remove_diagnostics::LocalTarget&,const native::BuildingState& in,LocalBinding& out,std::string&){out.state=in;out.object_key=key;out.local_uid=500;return true;}
}
namespace scenery_kiss {bool read_action_prop(uint64_t,native::BuildingState& out,std::string&){out=fixture::building;return true;}}
#include "scenery_placement_receipt_ports.inc"
namespace building_history_slice {
bool remember_place(const std::string&,uint64_t graph,std::string& error){
    fixture::calls.push_back("remember");
    if(!fixture::actual_scenery)return true;
    ++scenery_receipt_fixture::remembered;
    fixture::need(graph==scenery_receipt_fixture::graph,"consumer did not receive the exact original Scenery Undo graph");
    fixture::need(scenery_receipt_fixture::graph_reads==1&&scenery_receipt_fixture::cleanup_returns==1,
        "history registered before actual producer graph validation/cleanup");
    if(!scenery_receipt_fixture::history_ok){error="actual history registration failure";return false;}
    return true;
}
}
namespace ui_build_runtime {
bool place(const action::Request& request,action::BuildingState& out,json& receipt,std::string& error){if(fixture::expected_retired_before_place)fixture::need(*fixture::expected_retired_before_place,"absent original Job was not retired before allocator reuse");++fixture::places;fixture::calls.push_back("place");
if(fixture::actual_scenery)return place_scenery(scenery_receipt_fixture::producer,request,out,receipt,error);
out=fixture::building;receipt={{"confirmed",true},{"attempted",true},{"native_uid",uint64_t(500)},{"local_undo_graph",uint64_t(600)}};return true;}
}
namespace mt2mp::game::developer_staff::construction {
bool capture_placement(PlacementBefore&,std::string& e){++fixture::captures;fixture::calls.push_back("capture");if(!fixture::capture_ok)e="actual before Job census failure";return fixture::capture_ok;}
bool finish_placement(const PlacementBefore&,const std::string&,const std::string& key,uint64_t uid,LocalJob& out,std::string& e){++fixture::finishes;fixture::calls.push_back("finish");if(!fixture::finish_ok){e="actual after Job census failure";return false;}if(!fixture::no_job)out={key,0x1234,uid};return true;}
}
#include "construction_clearance_ports.inc"
#include "duplicate_construction_engine_ports.inc"
namespace typed_action_runtime {json status(){return {{"pending",true},{"action_uuid",fixture::request.action_uuid},{"host_revision",uint64_t(1)}};}}
struct World {
std::map<std::string,world_state::Row> rows_;
#include "duplicate_construction_world_ports.inc"
const auto& rows()const{return rows_;}
world_state::Origin origin(){return {};}
world_state::Admission prepare(const world_state::Origin&,const world_state::Change&,std::string&){return world_state::Admission::New;}
world_state::Admission commit(const world_state::Origin&,const world_state::Change& change,const world_state::LocalBinding& observed,std::string& e){fixture::calls.push_back("commit");if(!fixture::commit_ok){e="actual world commit failure";return world_state::Admission::Rejected;}rows_[change.object_key].state=observed.state;return world_state::Admission::New;}
};
struct NativeEngine {
    const action::Request* active_dungeon_building_=nullptr;
#include "dungeon_original_scope.inc"
#include "construction_build_operation.inc"
#include "construction_operation.inc"
std::map<std::string,Operation> operations_;
#include "construction_history_operation.inc"
std::map<std::string,HistoryOperation> history_;
std::map<std::string,BuildOperation> buildings_;World world_;
bool current(const action::Binding&,const std::string&,std::string& e){++fixture::currents;if(!fixture::current_ok)e="actual current Origin failure";return fixture::current_ok;}
bool read_ledger(action::Ledger& out,std::string&){out={};return true;}
action::Binding loaded_construction_binding_{};
std::string loaded_construction_snapshot_;
std::vector<developer_staff::construction::LocalJob> loaded_construction_;
#include "construction_duplicate_type.inc"
std::map<std::string,DuplicateConstruction> duplicate_construction_;
#include "construction_retire_absent.inc"
#include "construction_engine_jobs.inc"
#include "construction_engine_bug_parent.inc"
#include "construction_perform_place.inc"
#include "construction_perform_duplicate.inc"
#include "construction_perform_duplicate_history.inc"
};
uint32_t GetCurrentThreadId(){return 1;}
struct Session {bool good=true;bool current(action::Context& out,std::string& e){if(!good){e="actual Session unavailable";return false;}out=fixture::context;return true;}};
struct Controller {action::Context context;action::Binding binding;const action::Context& bound_context(){return context;}const action::Binding& bound_binding(){return binding;}};
struct Runtime {uint32_t thread=1;bool lobby_retiring=false;Session session;Controller controller;NativeEngine engine;};
std::atomic<Runtime*> published_runtime{nullptr};
#include "construction_runtime_jobs.inc"
#include "construction_runtime_bug_parent.inc"
void reset(Runtime& r){clearance_placement_fixture::clear();using namespace fixture;places=finishes=captures=currents=0;capture_ok=finish_ok=commit_ok=current_ok=true;no_job=false;calls.clear();request={};request.action=action::Action::PlaceBuilding;request.action_uuid="11111111-1111-4111-8111-111111111111";request.snapshot_uuid="22222222-2222-4222-8222-222222222222";request.canonical_record_key="action:"+request.action_uuid;context={};context.snapshot_uuid=request.snapshot_uuid;context.session=3;context.role=action::Role::Client;r={};r.controller.context=context;r.controller.binding=binding;published_runtime=&r;}
#include "scenery_placement_receipt_cases.inc"
#include "loaded_construction_engine_cases.inc"
#include "duplicate_construction_engine_cases.inc"
#include "construction_clearance_cases.inc"
int main(){try{using namespace fixture;Runtime runtime;std::string e;std::vector<developer_staff::construction::LocalJob> jobs;
loaded_engine_cases();
duplicate_engine_cases(runtime);
clearance_placement_cases(runtime);
reset(runtime);bool known=true,alive=true;
need(bug_parent_alive(context,binding,request.canonical_record_key,known,alive,e)&&!known&&!alive,"missing Building row became a tombstone");
runtime.engine.world_.rows_[request.canonical_record_key].state.alive=true;
need(bug_parent_alive(context,binding,request.canonical_record_key,known,alive,e)&&known&&alive,"confirmed alive Building row lost");
runtime.engine.world_.rows_[request.canonical_record_key].state.alive=false;
need(bug_parent_alive(context,binding,request.canonical_record_key,known,alive,e)&&known&&!alive,"confirmed tombstone lost");
for(int wrong=0;wrong<7;++wrong){reset(runtime);known=alive=true;
    if(wrong==0)published_runtime=nullptr;if(wrong==1)runtime.thread=2;if(wrong==2)runtime.lobby_retiring=true;
    if(wrong==3)runtime.session.good=false;if(wrong==4)runtime.controller.context.session++;if(wrong==5)runtime.controller.binding.native_epoch++;if(wrong==6)current_ok=false;
    need(!bug_parent_alive(context,binding,request.canonical_record_key,known,alive,e)&&!known&&!alive&&!e.empty(),"Bug parent accessor ignored current owner or retained stale output");}
reset(runtime);known=alive=true;need(!bug_parent_alive(context,binding,std::string(64,'a')+":world:npc:34",known,alive,e)&&!known&&!alive,"NPC key admitted as Building tombstone");
reset(runtime);need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Confirmed,"actual placement failed "+e);
need(calls==std::vector<std::string>{"capture","place","remember","admit","resolve","finish","commit"},"original placement/census/order changed");
need(construction_jobs(context,binding,jobs,e)&&jobs.size()==1&&jobs[0].building_uid==500&&jobs[0].job==0x1234,"pending Controller lost owned local Job");
runtime.engine.world_.rows_[request.canonical_record_key].state.alive=true;need(construction_jobs(context,binding,jobs,e)&&jobs[0].target_alive,"live ordered Building lost target lifetime");
runtime.engine.world_.rows_[request.canonical_record_key].state.alive=false;need(construction_jobs(context,binding,jobs,e)&&jobs.size()==1&&!jobs[0].target_alive&&jobs[0].job==0x1234,"removed ordered Building silently discarded retained Job");
runtime.engine.world_.rows_.clear();need(!construction_jobs(context,binding,jobs,e)&&jobs.empty()&&e=="Construction local Job has no current ordered Building row","missing ordered Building became a completed Job");
need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Unknown&&places==1&&captures==1,"UUID replayed placement/census");
for(int fail=0;fail<3;++fail){reset(runtime);if(fail==0)capture_ok=false;if(fail==1)finish_ok=false;if(fail==2)commit_ok=false;const auto result=runtime.engine.perform_place(binding,request,nullptr,e);
need(result==(fail==0?action::Outcome::RejectedNoEffect:action::Outcome::Unknown),"census/commit failure outcome differs");need(!e.empty()&&places==(fail==0?0u:1u),"census failure lost cause/effect count");
if(fail==2)need(!construction_jobs(context,binding,jobs,e),"unconfirmed partial Job exposed as admitted");}
reset(runtime);no_job=true;need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Confirmed&&construction_jobs(context,binding,jobs,e)&&jobs.empty(),"ordinary no-Job scenery changed behavior");
for(int fail=0;fail<6;++fail){reset(runtime);if(fail==0)runtime.thread=2;if(fail==1)runtime.lobby_retiring=true;if(fail==2)runtime.session.good=false;if(fail==3)runtime.controller.context.session++;if(fail==4)runtime.controller.binding.native_epoch++;if(fail==5)current_ok=false;jobs.push_back({"stale",1,1});need(!construction_jobs(context,binding,jobs,e)&&jobs.empty()&&!e.empty(),"local Job accessor admitted wrong owner");}
reset(runtime);need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Confirmed,"setup");runtime.engine.buildings_.begin()->second.request.snapshot_uuid="foreign";need(!construction_jobs(context,binding,jobs,e)&&jobs.empty(),"foreign placement snapshot accepted");
reset(runtime);need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Confirmed,"first original occurrence setup");auto& old=runtime.engine.buildings_.begin()->second;expected_retired_before_place=&old.construction_job.retired;
request.action_uuid="33333333-3333-4333-8333-333333333333";request.canonical_record_key="action:"+request.action_uuid;
need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Confirmed,"new original occurrence setup");expected_retired_before_place=nullptr;
need(construction_jobs(context,binding,jobs,e)&&jobs.size()==2&&jobs[0].job==jobs[1].job&&jobs[0].retired&&!jobs[1].retired,"allocator reuse confused original placement Job occurrences");
reset(runtime);need(runtime.engine.perform_place(binding,request,nullptr,e)==action::Outcome::Confirmed,"restored placement setup");runtime.engine.world_.rows_[request.canonical_record_key].state.alive=true;
NativeEngine::HistoryOperation redo;redo.request=request;redo.request.action=action::Action::RedoBuilding;redo.request.action_uuid="44444444-4444-4444-8444-444444444444";redo.request.target_action_uuid=request.action_uuid;redo.binding=binding;redo.uid=500;redo.confirmed=true;redo.object_after.alive=true;
need(construction_jobs(context,binding,jobs,e)&&!jobs[0].target_restored,"live original placement falsely restored");
runtime.engine.history_[redo.request.action_uuid]=redo;need(construction_jobs(context,binding,jobs,e)&&jobs[0].target_restored,"confirmed exact original Redo not projected");
runtime.engine.world_.rows_[request.canonical_record_key].state.alive=false;need(construction_jobs(context,binding,jobs,e)&&!jobs[0].target_restored,"later tombstone retained live restoration");runtime.engine.world_.rows_[request.canonical_record_key].state.alive=true;
for(int wrong=0;wrong<9;++wrong){auto changed=redo;if(wrong==0)changed.confirmed=false;if(wrong==1)changed.request.action=action::Action::UndoBuilding;if(wrong==2)changed.request.action_uuid="different";if(wrong==3)changed.request.target_action_uuid="different";if(wrong==4)changed.binding.native_epoch++;if(wrong==5)changed.request.snapshot_uuid="different";if(wrong==6)changed.request.canonical_record_key="different";if(wrong==7)changed.uid++;if(wrong==8)changed.object_after.alive=false;runtime.engine.history_[redo.request.action_uuid]=changed;need(construction_jobs(context,binding,jobs,e)&&!jobs[0].target_restored,"unconfirmed/foreign Redo projected restoration");}
runtime.engine.history_.clear();
NativeEngine::Operation removed;removed.request=request;removed.request.action=action::Action::RemoveSnapshotBuilding;
removed.request.action_uuid="55555555-5555-4555-8555-555555555555";removed.binding=binding;removed.local_uid=500;removed.history_ready=true;
runtime.engine.operations_[removed.request.action_uuid]=removed;
auto restore=redo;restore.request.action=action::Action::UndoBuilding;restore.request.target_action_uuid=removed.request.action_uuid;
runtime.engine.history_[restore.request.action_uuid]=restore;
need(construction_jobs(context,binding,jobs,e)&&jobs[0].target_restored&&jobs[0].job==0x1234,
    "confirmed Remove Undo did not preserve original expired Job occurrence");
for(int wrong=0;wrong<7;++wrong){auto changed=removed;if(wrong==0)changed.history_ready=false;if(wrong==1)changed.binding.native_epoch++;
if(wrong==2)changed.local_uid++;if(wrong==3)changed.request.action=action::Action::PlaceBuilding;if(wrong==4)changed.request.snapshot_uuid="foreign";
if(wrong==5)changed.request.canonical_record_key="foreign";if(wrong==6)changed.request.action_uuid="foreign";
runtime.engine.operations_[removed.request.action_uuid]=changed;
need(construction_jobs(context,binding,jobs,e)&&!jobs[0].target_restored,"foreign/unconfirmed Remove history projected restoration");}
runtime.engine.operations_[removed.request.action_uuid]=removed;restore.object_before.alive=true;runtime.engine.history_[restore.request.action_uuid]=restore;
need(construction_jobs(context,binding,jobs,e)&&!jobs[0].target_restored,"destructive inverse was called a restored lifetime");
scenery_receipt_cases(runtime,e);
std::cout<<"PASS "<<checks<<" actual placement/accessor checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
