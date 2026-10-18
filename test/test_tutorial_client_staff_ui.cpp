#include "tutorial_startingpoint_fixture/client_admission.h"
#include "tutorial_startingpoint_fixture/begin_guards.h"
#include "tutorial_development.h"
#include <iostream>
#include <map>
#include <set>
#include <cstring>
#include <stdexcept>
using json=nlohmann::json;
using namespace mt2mp::game;
using namespace mt2mp::game::tutorial_startingpoint_fixture;
namespace {
using DWORD=unsigned long;
using World=building_mixed_identity::opened_file::CurrentWorld;
constexpr uintptr_t win=0x2000,tabs=0x3000,view=0x4000,pips=0x5000,button=0x6000,rows=0x7000,fixture_pane=0x8000,tech=0x9000,project_view=0xa000,fixture_dev=0xb000,fixture_children=0xc000,business=0xd000;
const std::string action="11111111-1111-1111-1111-111111111111";
struct Pin{World world;uintptr_t image=0x10000000,lounge=2,project=3,window=0,button=0,player=6,inventory=7,slots=8;std::array<uintptr_t,200> items{};json development,tutorial={{"index",8}};std::string tech_key="startingpoint";};
struct Request{std::string token="token",origin_uuid="local",operation="client_staff_one";uint64_t epoch=1,generation=1;json expected_development;};
struct Progress{bool shown=false,completed=false,committed=false,locked=false,begun=false;std::string cause;};
std::map<std::string,Progress> worlds;std::map<std::string,json> receipts;
json controller,progress_state,development;
tutorial_development::StartingpointSnapshot native_snapshot;
std::map<uintptr_t,unsigned char> fixture_bytes;
std::map<uintptr_t,std::string> strings;
int activation_calls=0,show_calls=0,page_calls=0,select_calls=0,ledger_value=10,idle_binding_calls=0,pending_observations=0;
bool changed_row=false,change_ledger=false,change_origin=false,send_failed=false,wrong_pending=false,unknown_pending=false,stale_receipt=false,changed_context=false;
DWORD selection_exception=0;
bool layout_binds=true,debug_done=false;DWORD getter_exception=0,activate_exception=0;
json idle(){return {{"initialized",true},{"started",true},{"locked",false},{"pending",false},{"lobby_retiring",false},{"cause",""},{"outcome",0},{"action_uuid",""},
    {"live_state",{{"active",true},{"locked",false},{"unknown",false},{"cause",""}}}};}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>void put(uintptr_t p,size_t off,T value){const auto* b=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(value);++i)fixture_bytes[p+off+i]=b[i];}
template<class T>bool at(uintptr_t p,size_t off,T& value,std::string& e){
    auto* b=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(value);++i){auto found=fixture_bytes.find(p+off+i);if(found==fixture_bytes.end())return fail(e,"unreadable native address");b[i]=found->second;}return true;}
bool type(uintptr_t p,uintptr_t expected,std::string& e){uintptr_t value=0;return at(p,0,value,e)&&(value==expected||fail(e,"native type differs"));}
bool text(uintptr_t p,size_t off,std::string& out,std::string& e){auto it=strings.find(p+off);if(it==strings.end())return fail(e,"unreadable native text");out=it->second;return true;}
bool same(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
bool same_inventory_owner(const Pin& a,const Pin& b){return a.inventory==b.inventory&&a.player==b.player&&a.slots==b.slots;}
namespace action_session {
enum class Role{Host,Client};struct Context{Role role=Role::Client;std::string snapshot_uuid="common";bool operator==(const Context&)const=default;};struct Binding{int owner=7;};
struct Session{bool current(Context& c,std::string&){c={};if(activation_calls&&changed_context)c.snapshot_uuid="foreign";return true;}}fixture_session;
Session& selected_session(){return fixture_session;}
}
namespace typed_action_runtime {
bool network_binding(const std::string&,action_session::Binding&,std::string& e){++idle_binding_calls;return !controller.at("pending").get<bool>()||fail(e,"network admission requires idle unlocked Controller");}
bool observe_startingpoint_submission(const action_session::Context& c,const action_session::Binding& b,const std::string& id,std::string& e){
    ++pending_observations;return (c.snapshot_uuid=="common"&&b.owner==7&&client_controller_admission(controller,id,e))||fail(e,e.empty()?"bound Context changed":e);}
}
bool pin(const Request& request,Pin& p,std::string& e,bool client=false,const std::string& pending={},const action_session::Context* context=nullptr,const action_session::Binding* binding=nullptr){
    if(!client||!client_controller_admission(controller,pending,e))return false;
    if(pending.empty()){action_session::Binding b;if(!typed_action_runtime::network_binding("common",b,e))return false;}
    else if(!context||!binding||!typed_action_runtime::observe_startingpoint_submission(*context,*binding,pending,e))return false;
    p={};p.tech_key=request.operation.starts_with("client_bridges_")?"bridges":"startingpoint";p.development=development;p.world=native_snapshot.origin;
    if(activation_calls&&change_origin)++p.world.world_epoch;
    return true;
}
bool begin_snapshot(const Pin&,tutorial_development::StartingpointSnapshot& s,json& d,std::string&){s=native_snapshot;d=development;if(activation_calls&&changed_row)s.row->desired_workers=1;return true;}
bool staff_ui_code(uintptr_t,std::string&){return true;}
DWORD staff_done(uintptr_t,uintptr_t,bool& done){done=debug_done;return getter_exception;}
bool development_window(Pin& p,std::string&){p.window=win;return true;}
namespace tutorial_replication {bool read_progress(json& out,std::string&){out=progress_state;return true;}}
namespace live_host_state {
struct Pin{int ledger=0;uintptr_t business=::business;bool operator==(const Pin&)const=default;};
struct NativePort{NativePort(const action_session::Context&,const action_session::Binding&,void*){}bool read(Pin& out,std::string&){out.ledger=ledger_value;return true;}};
}
namespace native {
using SimulationState=mt2mp::game::native::SimulationState;
bool read_simulation_state(SimulationState& out,std::string&){out={};out.cash=ledger_value;out.clock.time_scale=0;out.clock.paused=true;return true;}
}
DWORD show(uintptr_t,uintptr_t){
    ++show_calls;
    if(layout_binds){
        // Original View::Layout traverses the nested fixture_children, passes their
        // immediate parent as RDX and the same root Window as R8.
        put<uintptr_t>(tabs,0x278,win);put<uintptr_t>(tabs,0x280,win);
        put<uintptr_t>(view,0x278,tabs);put<uintptr_t>(view,0x280,win);

        put<uint8_t>(win,0x460,0);
    }
    put<uint8_t>(win,0x45f,1);return 0;
}
DWORD staff_page(uintptr_t,uintptr_t){++page_calls;strings[view+0x3e8]="{dev_tab_staff}";put<uintptr_t>(view,0x418,fixture_pane);put<int32_t>(view,0x408,1);return 0;}
DWORD activate(uintptr_t,uintptr_t){
    ++activation_calls;if(change_ledger)++ledger_value;
    progress_state={{"client_staff_submitted",!send_failed},{"client_staff_suppressed",true},{"client_staff_action_uuid",action},{"client_staff_cause",send_failed?"original send failure":""}};
    progress_state["client_staff_desired_workers"]=1;put<int32_t>(pips,0x3e0,1);
    controller["pending"]=true;controller["action_uuid"]=wrong_pending?"foreign":action;
    if(unknown_pending)controller["outcome"]=3;return activate_exception;
}
#include "staff_common_extracted.inc"
#include "staff_ui_read.inc"
#include "client_staff_fixture.inc"
unsigned checks_completed=0;
void need(bool yes,const char* cause){if(!yes)throw std::runtime_error(cause);++checks_completed;}
void reset(){
    worlds.clear();receipts.clear();fixture_bytes.clear();strings.clear();controller=idle();progress_state=json::object();
    activation_calls=show_calls=page_calls=select_calls=idle_binding_calls=pending_observations=0;ledger_value=10;
    changed_row=change_ledger=change_origin=send_failed=wrong_pending=unknown_pending=stale_receipt=changed_context=false;
    layout_binds=true;debug_done=false;getter_exception=activate_exception=0;
    development={{"project",{{"present",true},{"work",0},{"lines",0},{"desired_workers",0},{"bugs_total",0},{"bugs_known",0}}},
        {"startingpoint",{{"record_present",true},{"purchased",true},{"deployed",false},{"custom",false},{"owned",false},{"inventory_count",0}}}};
    native_snapshot={};native_snapshot.tech=tech;native_snapshot.record=10;native_snapshot.project=3;
    native_snapshot.row.emplace();native_snapshot.row->project_present=true;native_snapshot.row->purchased=true;
    native_snapshot.creation.emplace();native_snapshot.creation->source=0xf000;native_snapshot.creation->max_workers=1;
    put<int32_t>(business,0x4c,1);
    put<uintptr_t>(win,0x540,tabs);put<uintptr_t>(tabs,0,0x117015b0);put<uintptr_t>(tabs,0x280,win);put<uintptr_t>(tabs,0x278,win);
    put<uintptr_t>(tabs,0x3b8,view);put<uintptr_t>(view,0,0x116dfe90);put<uintptr_t>(view,0x280,win);put<uintptr_t>(view,0x278,tabs);
    put<uint8_t>(win,0x460,0);put<uint8_t>(win,0x45f,1);put<uintptr_t>(view,0x418,0);put<int32_t>(view,0x408,0);
    put<uintptr_t>(view,0x410,fixture_pane);strings[view+0x3e8]="{dev_tab_staff}";
    strings[fixture_pane+0x1b0]="{dev_tab_staff}";put<uintptr_t>(fixture_pane,0x280,win);put<uintptr_t>(fixture_pane,0x278,view);
    put<uintptr_t>(win,0x568,project_view);put<uintptr_t>(project_view,0,0x116dfe90);put<uintptr_t>(project_view,0x280,win);put<uintptr_t>(project_view,0x278,fixture_pane);
    put<uintptr_t>(project_view,0x410,fixture_dev);put<uintptr_t>(project_view,0x418,0);put<int32_t>(project_view,0x408,0);strings[project_view+0x3e8]="dev";
    strings[fixture_dev+0x1b0]="dev";put<uintptr_t>(fixture_dev,0x280,win);put<uintptr_t>(fixture_dev,0x278,project_view);
    put<uintptr_t>(win,0x588,pips);put<uintptr_t>(pips,0,0x11744290);put<uintptr_t>(pips,0x280,win);put<uintptr_t>(pips,0x278,fixture_dev);
    strings[pips+0x398]="MessageTo Parent SetTechWorkers";put<int32_t>(pips,0x3c4,1);put<int32_t>(pips,0x3e0,0);
    put<uintptr_t>(pips,0x3d0,rows);put<int32_t>(pips,0x3d8,1);put<int32_t>(pips,0x3dc,1);
    put<uintptr_t>(pips,0x2d0,fixture_children);put<int32_t>(pips,0x2d8,1);put<int32_t>(pips,0x2dc,1);
    put<uintptr_t>(rows,0,button);put<uintptr_t>(fixture_children,0,button);
    put<uintptr_t>(button,0,0x116e3ec0);put<uintptr_t>(button,0x280,win);put<uintptr_t>(button,0x278,pips);
    put<uint8_t>(button,0x46c,1);put<uint8_t>(button,0x46d,0);strings[button+0x388]="MessageTo Parent Pressed 0";
}
bool run(json& out,std::string& e,const char* op="client_staff_one"){
    e.clear();Request r;r.operation=op;r.expected_development=development;out={{"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false}};
    Progress* phase=nullptr;std::string key;return client_staff(r,out,phase,key,e);
}
bool state(json& out,std::string& e,const char* op="client_staff_state"){e.clear();Request r;r.operation=op;r.expected_development=development;return client_staff_state(r,out,e);}
void check_scan_observations(){std::string e;json out;
    reset();native_snapshot.creation->max_workers=3;
    development["startingpoint_release"]={{"mutation_counter",440},{"change_count",1},{"feature_count",0},{"feature_present",false}};
    Request request;request.operation="client_bridges_staff_state";request.expected_development=development;
    development["startingpoint_release"]["mutation_counter"]=560;
    need(client_staff_state(request,out,e)&&out["confirmed"]==true&&out["attempted"]==false,
         "actual Staff state rejected later scan stamp");
    need(worlds.empty()&&receipts.empty()&&activation_calls==0,"scan-only Staff observation mutated");
    for(const char* field:{"change_count","feature_count"}){
        auto bad=request;bad.expected_development["startingpoint_release"][field]=99;e.clear();
        need(!client_staff_state(bad,out,e)&&e=="startingpoint Staff state expected development differs",
             "actual Staff state ignored meaningful Release change");
    }
    auto bad=request;bad.expected_development["project"]["work"]=1;e.clear();
    need(!client_staff_state(bad,out,e)&&activation_calls==0,"actual Staff state ignored project work");
    request.operation="client_bridges_staff_one";Progress* phase=nullptr;std::string key;e.clear();
    out={{"attempted",false},{"unknown",false}};
    need(client_staff(request,out,phase,key,e)&&activation_calls==1&&out["submitted"]==true&&
         out["before"]==out["after"],"actual Staff intent rejected prior scan or changed local diagnostics");
}
void check_bridges(){std::string e;json out;
    reset();native_snapshot.creation->max_workers=3;
    need(run(out,e,"client_bridges_staff_show")&&show_calls==1&&activation_calls==0,"Bridges Staff Show failed");
    need(worlds.contains("local:1:1:client_staff:bridges")&&!worlds.contains("local:1:1:client_staff:startingpoint"),"Bridges Staff consumed Startingpoint phase");
    need(state(out,e,"client_bridges_staff_state")&&out["ready"]==true&&!activation_calls,"Bridges Staff read-only state failed");
    need(run(out,e,"client_bridges_staff_one")&&activation_calls==1&&out["submitted"]==true&&out["desired_workers"]==1&&out["before"]==out["after"],"Bridges Staff original toggle failed or wrote project locally");
    need(!run(out,e,"client_bridges_staff_one")&&activation_calls==1,"Bridges Staff pending toggle replayed");
}
void check(){std::string e;json out;
    reset();need(state(out,e)&&out["ready"]==true&&out["ui"]["button_toggle"]==1&&out["ui"]["button_membership_matches"]==true,"actual owned toggle not ready");
    need(worlds.empty()&&receipts.empty()&&activation_calls==0&&out["attempted"]==false,"state inserted phase or mutated");
    put<uint8_t>(win,0x45f,0);put<uint8_t>(win,0x460,1);put<uintptr_t>(tabs,0x280,0);put<uintptr_t>(view,0x280,0);
    strings[fixture_pane+0x1b0]="{dev_tab_tech}";put<uintptr_t>(pips,0x280,0);
    need(run(out,e,"client_staff_show")&&show_calls==1&&page_calls==1&&activation_calls==0&&out["page_requested"]==true,"Show must request Staff without touching deferred descendants");
    need(out["ledger_before"]==out["ledger_after"]&&out["ledger_before"].contains("clock_day"),"Show omitted full ledger");
    const auto tokens=receipts.size(),phases=worlds.size();
    need(state(out,e)&&out["ready"]==false&&out["ui"]["staff_page_active"]=="{dev_tab_tech}"&&out["ui"]["pips_owner"]=="null","deferred layout not observed");
    need(state(out,e)&&receipts.size()==tokens&&worlds.size()==phases,"repeated State consumed token/phase");
    need(!run(out,e)&&activation_calls==0,"inactive Staff activated");
    strings[fixture_pane+0x1b0]="{dev_tab_staff}";put<uintptr_t>(pips,0x280,win);
    need(run(out,e)&&activation_calls==1&&out["submitted"]==true&&out["action_uuid"]==action&&out["desired_workers"]==1,"real first toggle failed to submit");
    need(out["before"]==out["after"]&&out["ledger_before"]==out["ledger_after"]&&out["host_completion_confirmed"]==false&&out["ui_after"]["pips_value"]==1&&pending_observations==1,"submission confused presentation with native desired or used idle binding");
    need(!run(out,e)&&activation_calls==1,"pending activation replayed");
    reset();put<uint8_t>(button,0x46c,0);need(state(out,e)&&out["ready"]==false&&out["cause"]=="first Button toggle=0 required=1","ordinary non-toggle admitted");
    reset();put<uint8_t>(button,0x46d,1);need(!run(out,e)&&activation_calls==0,"disabled toggle activated");
    reset();put<uintptr_t>(button,0x278,fixture_dev);need(!run(out,e)&&activation_calls==0,"foreign Button parent activated");
    reset();put<uintptr_t>(button,0x280,win+1);need(state(out,e)&&out["ui"]["button_owner"]=="foreign"&&!out["ready"].get<bool>(),"foreign Window owner hidden");
    reset();put<uintptr_t>(fixture_children,0,button+1);need(!run(out,e)&&activation_calls==0,"different owned child accepted");
    reset();put<int32_t>(pips,0x3d8,2);put<int32_t>(pips,0x3dc,2);put<uintptr_t>(rows,8,button);need(!run(out,e)&&activation_calls==0,"duplicate Button accepted");
    reset();put<int32_t>(pips,0x2dc,65);need(!run(out,e)&&activation_calls==0,"unbounded child array read");
    reset();put<uintptr_t>(fixture_dev,0x278,fixture_dev);need(state(out,e)&&out["ready"]==false&&out["ui"]["pips_parent_chain_bound"]==false,"cyclic ancestry not bounded");
    reset();strings[pips+0x398]="MessageTo Window SetTechWorkers";strings[button+0x388]="MessageTo Parent Pressed 1";need(state(out,e)&&out["ui"]["causes"].size()==2,"both exact command mismatches not reported");
    reset();put<int32_t>(pips,0x3e0,1);need(!run(out,e)&&activation_calls==0,"selected pip toggled to zero");
    reset();put<int32_t>(business,0x4c,0);need(state(out,e)&&out["ready"]==false&&out["ui"]["available_workers"]==0,"missing Developer hidden");
    reset();native_snapshot.creation->max_workers=0;need(!run(out,e)&&activation_calls==0,"max worker bound ignored");
    reset();debug_done=true;need(state(out,e)&&out["ready"]==false&&out["ui"]["debugging_complete"]==true,"complete debugging activated");
    reset();getter_exception=5;need(!state(out,e)&&e.find("exception=5")!=std::string::npos,"getter exception lost");
    reset();native_snapshot.row->desired_workers=1;need(!run(out,e)&&activation_calls==0,"nonzero native desired admitted");
    reset();native_snapshot.row->purchased=false;need(!run(out,e)&&activation_calls==0,"unpurchased project admitted");
    reset();controller["initialized"]=false;need(!run(out,e)&&activation_calls==0,"uninitialized Client admitted");
    reset();put<uint8_t>(win,0x45f,0);put<uint8_t>(win,0x460,1);put<uintptr_t>(tabs,0x280,0);layout_binds=false;
    need(!run(out,e,"client_staff_show")&&show_calls==1&&page_calls==0&&out["unknown"]==true,"unbound tabs got SetPage");
    need(!run(out,e,"client_staff_show")&&show_calls==1,"Unknown Show replayed");
    reset();send_failed=true;need(!run(out,e)&&out["unknown"]==true&&e.find("original send failure")!=std::string::npos,"submission cause lost");
    reset();progress_state["client_staff_action_uuid"]=action;need(!run(out,e)&&e.find("fresh")!=std::string::npos,"stale receipt accepted");
    reset();wrong_pending=true;need(!run(out,e)&&out["unknown"]==true,"foreign pending UUID accepted");
    reset();unknown_pending=true;need(!run(out,e)&&out["unknown"]==true,"Unknown Controller accepted");
    reset();changed_row=true;need(!run(out,e)&&out["unknown"]==true,"local desired write accepted before Host order");
    reset();change_ledger=true;need(!run(out,e)&&out["unknown"]==true&&e.find("ledger")!=std::string::npos,"ledger changed unnoticed");
    reset();change_origin=true;need(!run(out,e)&&out["unknown"]==true,"Origin changed unnoticed");
    reset();changed_context=true;need(!run(out,e)&&out["unknown"]==true,"Context changed unnoticed");
    reset();activate_exception=5;need(!run(out,e)&&out["returned"]==false&&out["unknown"]==true&&activation_calls==1,"Activate exception hidden");
}
}
int main(){try{check();check_bridges();check_scan_observations();std::cout<<"PASS "<<checks_completed<<" actual Staff UI checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
