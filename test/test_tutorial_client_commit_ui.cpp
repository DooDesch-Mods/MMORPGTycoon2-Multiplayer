#include "tutorial_startingpoint_fixture/client_admission.h"
#include <iostream>
#include <map>
#include <stdexcept>
using json=nlohmann::json;
using namespace mt2mp::game::tutorial_startingpoint_fixture;
namespace {
const std::string action="11111111-1111-1111-1111-111111111111";
struct World{uint64_t owner=1;};
struct Pin{World world;uintptr_t image=1,lounge=2,project=3,window=0,button=0,player=6,inventory=7,slots=8;std::array<uintptr_t,200> items{};json development={{"present",true}},tutorial={{"index",93}};};
struct Request{std::string token,origin_uuid,operation;uint64_t epoch=1,generation=1;json expected_development={{"present",true}};};
struct Progress{bool shown=false,completed=false,committed=false,locked=false,begun=false;std::string cause;};
std::map<std::string,Progress> worlds;std::map<std::string,json> receipts;
json controller,progress_state;int pin_calls=0,activate_calls=0,show_calls=0,ledger_value=1;
bool native_complete=true,button_enabled=true,change_project=false,change_ledger=false,bad_owner=false,submission_fails=false;
json idle(){return { {"initialized",true},{"started",true},{"locked",false},{"pending",false},{"lobby_retiring",false},{"cause",""},{"outcome",0},{"action_uuid",""},
    {"live_state",{{"active",true},{"locked",false},{"unknown",false},{"cause",""}}}};}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool same(const World& a,const World& b){return a.owner==b.owner;}
bool same_inventory_owner(const Pin& a,const Pin& b){return a.inventory==b.inventory&&a.player==b.player&&a.slots==b.slots;}
bool pin(const Request&,Pin& p,std::string& e,bool client=false,const std::string& pending={},const void* expected_context=nullptr,const void* expected_binding=nullptr){
    ++pin_calls;if(!client)return fail(e,"wrong Host pin");
    if(!pending.empty()&&(!expected_context||!expected_binding))return fail(e,"missing prebound submission Context/binding");
    if(!client_controller_admission(controller,pending,e))return false;
    p={};if(pin_calls==3&&change_project)++p.project;if(pin_calls==3&&bad_owner)++p.world.owner;return true;
}
bool before(const Pin&,std::string&){return true;}
bool development_window(Pin& p,std::string&){p.window=4;return true;}
bool commit_button(Pin& p,std::string& e){p.window=4;p.button=5;return button_enabled||fail(e,"original Commit button disabled");}
unsigned long complete_state(uintptr_t,uintptr_t,bool& result){result=native_complete;return 0;}
unsigned long show(uintptr_t,uintptr_t){++show_calls;return 0;}
unsigned long activate(uintptr_t,uintptr_t){
    ++activate_calls;if(change_ledger)++ledger_value;
    if(submission_fails){progress_state={{"client_commit_submitted",false},{"client_commit_suppressed",true},{"client_commit_cause","original Controller send failed"}};return 0;}
    progress_state={{"client_commit_submitted",true},{"client_commit_suppressed",true},{"client_commit_action_uuid",action},{"client_commit_cause",""}};
    controller["pending"]=true;controller["action_uuid"]=action;return 0;
}
template<class T>bool at(uintptr_t,size_t,T& value,std::string&){value=T(1);return true;}
namespace action_session {
enum class Role{Host,Client};struct Context{Role role=Role::Client;std::string snapshot_uuid="common";bool operator==(const Context&)const=default;};struct Binding{};
struct Session{bool current(Context& c,std::string&){c={};return true;}}session;
Session& selected_session(){return session;}
}
namespace typed_action_runtime {bool network_binding(const std::string&,action_session::Binding&,std::string&){return true;}}
namespace tutorial_replication {bool read_progress(json& out,std::string&){out=progress_state;return true;}}
namespace live_host_state {
struct Pin{int ledger=0;bool operator==(const Pin&)const=default;};
struct NativePort{NativePort(const action_session::Context&,const action_session::Binding&,void*){}bool read(Pin& out,std::string&){out.ledger=ledger_value;return true;}};
}
#include "client_ui_fixture.inc"
void need(bool yes,const char* cause){if(!yes)throw std::runtime_error(cause);}
void reset(){worlds.clear();receipts.clear();controller=idle();progress_state=json::object();pin_calls=activate_calls=show_calls=0;ledger_value=1;
    native_complete=button_enabled=true;change_project=change_ledger=bad_owner=submission_fails=false;}
Request request(const char* op="client_commit"){return {"token","local",op};}
bool run(const Request& r,json& out,std::string& e){out={{"attempted",false},{"confirmed",false},{"unknown",false}};Progress* phase=nullptr;std::string key;return client_ui(r,out,phase,key,e);}
void check(){std::string e;json out;
    reset();need(run(request(),out,e)&&activate_calls==1&&out["submitted"]==true&&out["action_uuid"]==action&&out["host_completion_confirmed"]==false&&out["unknown"]==false,"Client UI submission not confirmed distinctly");
    need(!run(request(),out,e)&&activate_calls==1,"second pending Commit replayed");
    reset();need(run(request("client_show"),out,e)&&show_calls==1&&activate_calls==0,"Client Show failed");
    need(!run(request("client_show"),out,e)&&show_calls==1,"Client Show phase replayed");
    reset();controller["initialized"]=false;need(!run(request(),out,e)&&activate_calls==0,"uninitialized Client allowed");
    reset();controller["pending"]=true;need(!run(request(),out,e)&&activate_calls==0,"pending Client admitted initially");
    reset();native_complete=false;need(!run(request(),out,e)&&activate_calls==0,"incomplete project activated");
    reset();button_enabled=false;need(!run(request(),out,e)&&activate_calls==0&&e=="original Commit button disabled","disabled button activated");
    reset();submission_fails=true;need(!run(request(),out,e)&&activate_calls==1&&out["unknown"]==true&&e.find("original Controller send failed")!=std::string::npos,"failed submission became success");
    reset();change_project=true;need(!run(request(),out,e)&&out["unknown"]==true,"changed project falsely confirmed");
    reset();change_ledger=true;need(!run(request(),out,e)&&out["unknown"]==true&&e.find("ledger")!=std::string::npos,"changed ledger falsely confirmed");
    reset();bad_owner=true;need(!run(request(),out,e)&&out["unknown"]==true,"changed full Origin falsely confirmed");
    reset();controller["pending"]=true;controller["action_uuid"]=action;
    need(client_controller_admission(controller,action,e),"exact submitted Pending rejected");
    controller["outcome"]=3;need(!client_controller_admission(controller,action,e),"Unknown accepted as Pending");
    controller["outcome"]=0;controller["live_state"]["unknown"]=true;need(!client_controller_admission(controller,action,e),"live Unknown bypassed by submitted readback");
    controller["live_state"]["unknown"]=false;controller["locked"]=true;need(!client_controller_admission(controller,action,e),"locked Controller bypassed");
    reset();controller["pending"]=true;controller["action_uuid"]=action;need(!client_controller_admission(controller,"other",e),"foreign pending action accepted");
}
}
int main(){try{check();std::cout<<"PASS actual Client UI body and exact Pending admission checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
