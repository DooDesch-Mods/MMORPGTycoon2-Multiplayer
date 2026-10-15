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
constexpr uintptr_t win=0x2000,tabs=0x3000,view=0x4000,grid=0x5000,button=0x6000,rows=0x7000,fixture_pane=0x8000,tech=0x9000;
const std::string action="11111111-1111-1111-1111-111111111111";
struct Pin{World world;uintptr_t image=0x10000000,lounge=2,project=0,window=0,button=0,player=6,inventory=7,slots=8;std::array<uintptr_t,200> items{};json development,tutorial={{"index",8}};};
struct Request{std::string token="token",origin_uuid="local",operation="client_research";uint64_t epoch=1,generation=1;json expected_development;};
struct Progress{bool shown=false,completed=false,committed=false,locked=false,begun=false;std::string cause;};
std::map<std::string,Progress> worlds;std::map<std::string,json> receipts;
json controller,progress_state,development;
tutorial_development::StartingpointSnapshot native_snapshot;
std::map<uintptr_t,unsigned char> fixture_bytes;
std::map<uintptr_t,std::string> strings;
int activation_calls=0,show_calls=0,page_calls=0,select_calls=0,ledger_value=10,idle_binding_calls=0,pending_observations=0;
bool changed_row=false,change_ledger=false,change_origin=false,send_failed=false,wrong_pending=false,unknown_pending=false,stale_receipt=false,changed_context=false;
DWORD selection_exception=0;
bool layout_binds=true,selection_enables=true;
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
bool pin(const Request&,Pin& p,std::string& e,bool client=false,const std::string& pending={},const action_session::Context* context=nullptr,const action_session::Binding* binding=nullptr){
    if(!client||!client_controller_admission(controller,pending,e))return false;
    if(pending.empty()){action_session::Binding b;if(!typed_action_runtime::network_binding("common",b,e))return false;}
    else if(!context||!binding||!typed_action_runtime::observe_startingpoint_submission(*context,*binding,pending,e))return false;
    p={};p.development=development;p.world=native_snapshot.origin;
    if(activation_calls&&change_origin)++p.world.world_epoch;
    return true;
}
bool begin_snapshot(const Pin&,tutorial_development::StartingpointSnapshot& s,json& d,std::string&){s=native_snapshot;d=development;if(activation_calls&&changed_row)s.row->purchased=true;return true;}
bool research_code(uintptr_t,std::string&){return true;}
bool development_window(Pin& p,std::string&){p.window=win;return true;}
struct Memory{}memory;
namespace tutorial_replication::start_intent {
struct Window{uintptr_t address=0,tabs=0,selected=0;};
bool window(Memory&,const tutorial_development::StartingpointSnapshot&,uintptr_t source,Window& out,std::string&){out={win,::tabs,source};return true;}
}
namespace tutorial_replication {bool read_progress(json& out,std::string&){out=progress_state;return true;}}
namespace live_host_state {
struct Pin{int ledger=0;bool operator==(const Pin&)const=default;};
struct NativePort{NativePort(const action_session::Context&,const action_session::Binding&,void*){}bool read(Pin& out,std::string&){out.ledger=ledger_value;return true;}};
}
namespace native {
using SimulationState=mt2mp::game::native::SimulationState;
bool read_simulation_state(SimulationState& out,std::string&){out={};out.cash=ledger_value;out.clock.time_scale=0;out.clock.paused=true;return true;}
}
DWORD show(uintptr_t,uintptr_t){
    ++show_calls;
    if(layout_binds){
        // Original View::Layout traverses the nested children, passes their
        // immediate parent as RDX and the same root Window as R8.
        put<uintptr_t>(tabs,0x278,win);put<uintptr_t>(tabs,0x280,win);
        put<uintptr_t>(view,0x278,tabs);put<uintptr_t>(view,0x280,win);

        put<uint8_t>(win,0x460,0);
    }
    put<uint8_t>(win,0x45f,1);return 0;
}
DWORD research_page(uintptr_t,uintptr_t){++page_calls;strings[view+0x3e8]="{dev_tab_tech}";put<uintptr_t>(view,0x418,fixture_pane);put<int32_t>(view,0x408,1);return 0;}
DWORD research_select(uintptr_t,uintptr_t,int32_t row){++select_calls;if(selection_exception)return selection_exception;put<int32_t>(grid,0x3e8,row);put<uint8_t>(grid,0x3ec,1);put<uint8_t>(rows,size_t(row)*12+9,1);put<uintptr_t>(win,0x610,native_snapshot.creation->source);if(selection_enables)put<uint8_t>(button,0x46d,0);return 0;}
DWORD activate(uintptr_t,uintptr_t){
    ++activation_calls;if(change_ledger)++ledger_value;
    progress_state={{"client_start_submitted",!send_failed},{"client_start_suppressed",true},{"client_start_action_uuid",action},{"client_start_cause",send_failed?"original send failure":""}};
    controller["pending"]=true;controller["action_uuid"]=wrong_pending?"foreign":action;
    if(unknown_pending)controller["outcome"]=3;return 0;
}
#include "research_ui_read.inc"
#include "client_research_fixture.inc"
unsigned checks_completed=0;
void need(bool yes,const char* cause){if(!yes)throw std::runtime_error(cause);++checks_completed;}
void reset(){worlds.clear();receipts.clear();fixture_bytes.clear();strings.clear();controller=idle();progress_state=json::object();
    activation_calls=show_calls=page_calls=select_calls=idle_binding_calls=pending_observations=0;ledger_value=10;
    changed_row=change_ledger=change_origin=send_failed=wrong_pending=unknown_pending=stale_receipt=changed_context=false;selection_exception=0;layout_binds=selection_enables=true;
    development={{"project",{{"present",false},{"work",0},{"lines",0},{"desired_workers",0},{"bugs_total",0},{"bugs_known",0}}},
        {"startingpoint",{{"record_present",true},{"purchased",false},{"deployed",false},{"custom",false},{"owned",false},{"inventory_count",0}}},
        {"startingpoint_release",{{"feature_count",0}}}};
    native_snapshot={};native_snapshot.tech=tech;native_snapshot.record=10;native_snapshot.row.emplace();native_snapshot.creation.emplace();
    native_snapshot.creation->source=0xa000;native_snapshot.creation->catalogue={{0xb000,0,0,"other",""},{0xa000,0,0,"startingpoint",""}};
    put<int32_t>(tech,0xac,0);put<uintptr_t>(win,0x540,tabs);put<uintptr_t>(tabs,0,0x117015b0);put<uintptr_t>(tabs,0x280,win);
    put<uintptr_t>(tabs,0x3b8,view);put<uintptr_t>(view,0,0x116dfe90);put<uintptr_t>(view,0x280,win);
    put<uint8_t>(win,0x460,0);put<uintptr_t>(view,0x418,0);put<int32_t>(view,0x408,0);put<uintptr_t>(win,0x5d8,grid);put<uint8_t>(win,0x45f,1);put<uintptr_t>(grid,0,0x116dade0);put<uintptr_t>(grid,0x280,win);
    put<uintptr_t>(grid,0x3c0,rows);put<int32_t>(grid,0x3c8,2);put<int32_t>(grid,0x3cc,2);strings[grid+0x360]="MessageTo Parent SelectProject";
    put<int32_t>(rows,0,0);put<int32_t>(rows,12,1);put<int32_t>(grid,0x3e8,1);put<uint8_t>(grid,0x3ec,1);put<uint8_t>(rows,21,1);
    put<uintptr_t>(win,0x610,native_snapshot.creation->source);strings[view+0x3e8]="{dev_tab_tech}";
    put<uintptr_t>(view,0x410,fixture_pane);strings[fixture_pane+0x1b0]="{dev_tab_tech}";put<uintptr_t>(fixture_pane,0x280,win);
    put<uintptr_t>(win,0x600,button);put<uintptr_t>(button,0,0x116e3ec0);put<uintptr_t>(button,0x280,win);put<uint8_t>(button,0x46c,0);put<uint8_t>(button,0x46d,0);
    strings[button+0x388]="MessageTo Window StartTech";
}
bool run(json& out,std::string& e,const char* op="client_research"){
    e.clear();Request r;r.operation=op;r.expected_development=development;out={{"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false}};
    Progress* phase=nullptr;std::string key;return client_research(r,out,phase,key,e);
}
void unlaid_out(){
    put<uint8_t>(win,0x45f,0);put<uint8_t>(win,0x460,1);
    for(uintptr_t child:{tabs,view,grid,button,fixture_pane}){put<uintptr_t>(child,0x278,0);put<uintptr_t>(child,0x280,0);}
}
void no_selection(){
    put<uintptr_t>(win,0x610,0);put<int32_t>(grid,0x3e8,-1);put<uint8_t>(grid,0x3ec,0);put<uint8_t>(rows,21,0);put<uint8_t>(button,0x46d,1);
}
void staff_page(){
    constexpr uintptr_t staff=0xc000;
    strings[view+0x3e8]="{dev_tab_staff}";put<uintptr_t>(view,0x410,staff);strings[staff+0x1b0]="{dev_tab_staff}";put<uintptr_t>(staff,0x280,win);
    no_selection();put<uintptr_t>(grid,0x280,0);put<uintptr_t>(button,0x280,0);
}
void next_ui_update(){
    // MultiView::UpdateUI calls SwapToNextPane, then layouts the newly active
    // pane and its descendants against the existing root Window.
    put<uintptr_t>(view,0x410,fixture_pane);put<int32_t>(view,0x408,2);
    for(uintptr_t child:{fixture_pane,grid,button})put<uintptr_t>(child,0x280,win);
}
bool state(json& out,std::string& e){e.clear();Request r;r.operation="client_research_state";r.expected_development=development;return client_research_state(r,out,e);}
void check(){std::string e;json out;
    reset();unlaid_out();staff_page();
    need(run(out,e,"client_research_show")&&show_calls==1&&page_calls==1&&select_calls==0&&activation_calls==0&&out["page_requested"]==true&&out["selection_confirmed"]==false,"Show must request deferred Tech page without selecting inactive Grid");
    need(out["ledger_before"]==out["ledger_after"]&&out["price"]==0,"Show ledger/price receipt missing");
    const auto tokens=receipts.size(),phases=worlds.size();
    need(state(out,e)&&out["ready"]==false&&out["attempted"]==false&&out["read_only"]==true&&out["confirmed"]==true,"deferred Tech state not a successful read-only pending observation");
    need(out["ui"]["requested_page"]=="{dev_tab_tech}"&&out["ui"]["active_page"]=="{dev_tab_staff}"&&out["ui"]["next_page"]=="{dev_tab_tech}"&&out["ui"]["page_transition"]==1,"deferred page values missing");
    need(out["ui"]["grid_owner"]=="null"&&out["ui"]["button_owner"]=="null"&&out["ui"]["grid_command"]=="MessageTo Parent SelectProject"&&out["ui"]["button_command"]=="MessageTo Window StartTech"&&out["ui"]["grid_class_rva"]==0x16dade0,"pending state omitted independent owner/class/command diagnostics");
    need(state(out,e)&&receipts.size()==tokens&&worlds.size()==phases&&select_calls==0&&activation_calls==0,"repeated read consumed token/phase or called UI");
    need(!run(out,e)&&select_calls==0&&activation_calls==0,"inactive Tech page called child method");
    next_ui_update();
    need(state(out,e)&&out["ready"]==true&&out["ui"]["selection_confirmed"]==false&&out["ui"]["button_disabled"]==1,"laid-out unselected page must be ready for native selection");
    need(run(out,e)&&select_calls==1&&activation_calls==1&&out["selection_confirmed"]==true&&out["submitted"]==true,"ordinary select then enabled Research did not submit");
    need(out["action_uuid"]==action&&out["host_completion_confirmed"]==false&&out["ledger_before"]==out["ledger_after"]&&pending_observations==1,"submission receipt or pending binding differs");
    need(!run(out,e)&&activation_calls==1,"pending Research replayed");
    reset();need(state(out,e)&&worlds.empty()&&receipts.empty(),"read inserted a phase before any operation");
    reset();unlaid_out();put<uintptr_t>(tabs,0x280,win+1);need(!run(out,e,"client_research_show")&&show_calls==0,"foreign pre-Show tabs accepted");
    reset();unlaid_out();put<uintptr_t>(view,0x280,win+1);need(!run(out,e,"client_research_show")&&show_calls==0,"foreign pre-Show view accepted");
    reset();unlaid_out();layout_binds=false;need(!run(out,e,"client_research_show")&&show_calls==1&&page_calls==0&&out["unknown"]==true,"unbound tabs received SetPage");
    const auto locked_cause=e;
    need(state(out,e)&&out["phase_locked"]==true&&out["phase_cause"]==locked_cause,"read erased retained unknown phase cause");
    need(!run(out,e,"client_research_show")&&show_calls==1,"Unknown Show replayed after read");
    reset();put<uintptr_t>(grid,0x280,win+1);need(state(out,e)&&out["ready"]==false&&out["ui"]["grid_owner"]=="foreign"&&out["cause"]=="grid owner=foreign","foreign Grid diagnostic not concrete");
    need(!run(out,e)&&select_calls==0,"foreign Grid called SelectRow");
    reset();put<uint8_t>(win,0x45f,0);need(state(out,e)&&out["cause"]=="window visible=0 required=1","Window visibility cause conflated");
    reset();strings[grid+0x360]="MessageTo Window SelectProject";
    need(state(out,e)&&out["ready"]==false&&out["ui"]["grid_command"]=="MessageTo Window SelectProject"&&out["cause"]=="grid command=MessageTo Window SelectProject required=MessageTo Parent SelectProject","incorrect Window route accepted in place of native Parent route");
    need(!run(out,e)&&select_calls==0&&activation_calls==0,"incorrect Window route reached native selection or activation");
    reset();strings[grid+0x360]="wrong grid";strings[button+0x388]="wrong button";
    need(state(out,e)&&out["ready"]==false&&out["ui"]["grid_command"]=="wrong grid"&&out["ui"]["button_command"]=="wrong button"&&out["ui"]["causes"].size()==2,"state failed to collect both actual command mismatches");
    need(!run(out,e)&&select_calls==0&&e.find("grid command=wrong grid")!=std::string::npos,"wrong command selected");
    reset();put<uintptr_t>(grid,0,0x10001234);need(state(out,e)&&out["ready"]==false&&out["ui"]["grid_class_rva"]==0x1234,"normalized foreign class missing");
    reset();no_selection();selection_enables=false;need(!run(out,e)&&select_calls==1&&activation_calls==0&&out["unknown"]==true&&e.find("button disabled=1")!=std::string::npos,"native selection did not gate disabled button");
    need(!run(out,e)&&select_calls==1,"failed selected phase replayed");
    reset();selection_exception=5;need(!run(out,e)&&select_calls==1&&activation_calls==0&&out["selection_attempted"]==true&&out["activation_attempted"]==false&&out["returned"]==false&&out["unknown"]==true,"SelectRow exception hidden");
    reset();controller["initialized"]=false;need(!run(out,e)&&select_calls==0,"uninitialized Client admitted");
    reset();controller["pending"]=true;need(!run(out,e)&&select_calls==0,"pending admission accepted");
    reset();put<int32_t>(rows,0,1);need(!run(out,e)&&select_calls==0,"duplicate Grid identifier accepted");
    reset();put<int32_t>(rows,12,2);need(!run(out,e)&&select_calls==0,"out-of-census Grid identifier accepted");
    reset();put<int32_t>(grid,0x3cc,1025);need(!run(out,e)&&select_calls==0,"unbounded Grid accepted");
    reset();put<uintptr_t>(fixture_pane,0x280,win+1);need(!run(out,e)&&select_calls==0,"foreign active page accepted");
    reset();send_failed=true;need(!run(out,e)&&out["unknown"]==true&&e.find("original send failure")!=std::string::npos,"failed intent hidden");
    reset();progress_state["client_start_action_uuid"]=action;need(!run(out,e)&&out["unknown"]==true&&e.find("fresh")!=std::string::npos,"stale receipt accepted");
    reset();wrong_pending=true;need(!run(out,e)&&out["unknown"]==true,"foreign pending UUID accepted");
    reset();unknown_pending=true;need(!run(out,e)&&out["unknown"]==true,"Unknown bypassed pending guard");
    reset();changed_row=true;need(!run(out,e)&&out["unknown"]==true,"changed purchased Row accepted");
    reset();change_ledger=true;need(!run(out,e)&&out["unknown"]==true&&e.find("ledger")!=std::string::npos,"changed full ledger accepted");
    reset();change_origin=true;need(!run(out,e)&&out["unknown"]==true,"changed NativeOpen accepted");
    reset();changed_context=true;need(!run(out,e)&&out["unknown"]==true,"changed Context accepted");
}

}
int main(){try{check();std::cout<<"PASS "<<checks_completed<<" actual Research orchestration/selection checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
