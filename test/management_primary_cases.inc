namespace management_primary_fixture {
struct Runtime {};
namespace a {enum class Role {Host,Client};struct Context {Role role=Role::Client;};struct Binding {};}
namespace live {
struct Pin {int identity=1;bool operator==(const Pin&)const=default;};
namespace apply_timing {
enum class Part {HostQueryManagementFinish,HostManagementFinishCalendar,HostManagementCalendar,HostManagementFinishThought,HostManagementThought,HostManagementPrimaryRead,ClientManagementPrimaryRead,ClientManagementCalendarRead,ClientManagementThoughtRead,ClientManagementIntroRead,ClientManagementGizmoRead,ClientManagementAnalyticsRead,ClientManagementServiceRead,ClientManagementProjectRead};
inline int host_capture_depth=0,take_depth=0;inline Part host_query_part=Part::HostManagementCalendar;
inline int intro_count=0,gizmo_count=0;inline std::optional<Part> active;
struct Scope {std::optional<Part> previous=active;explicit Scope(Part part){active=part;if(part==Part::ClientManagementIntroRead)++intro_count;if(part==Part::ClientManagementGizmoRead)++gizmo_count;}~Scope(){active=previous;}};
enum class PageBoundary {OriginalCall};struct PageReadScope {explicit PageReadScope(PageBoundary){}};
}
}
namespace management {
struct LiveState {int intro=0,thoughts=0,gizmos=0,character_calendar=0,subscriber_demands=0,analytics=0,services=0,project=0,other=0;bool operator==(const LiveState&)const=default;};
bool valid(const LiveState&,std::string&){return true;}
}
struct Local {live::Pin pin;int software=0,release=0,current_release=0,lounge=0,gm_rows=0,gm_uids=0,services=0,service_destinations=0;};
namespace subscriber_demand {using State=int;}
inline int calendar_reads=0,calendar_failure=0,calendar_drift=0,analytics_reads=0,service_reads=0,project_reads=0;
namespace character_calendar {using State=int;bool capture_demands(const a::Context&,const a::Binding&,const live::Pin&,State& out,subscriber_demand::State& demands,std::string& error){++calendar_reads;if(calendar_reads==calendar_failure){error="original Calendar/Demand reciprocal failure";return false;}out=4;demands=5+(calendar_reads==2?calendar_drift:0);return true;}}
namespace scenario_intro {using State=int;}
inline int fail_reader=0;
namespace tutorial_replication {bool capture_intro(const a::Context&,const a::Binding&,int& out,std::string& error){if(live::apply_timing::take_depth)need(live::apply_timing::active==live::apply_timing::Part::ClientManagementIntroRead,"actual Intro read lies inside child timer");if(fail_reader==1){error="original Intro failure";return false;}out=6;return true;}}
namespace dungeon_gizmo_live {using State=int;}
namespace npc_probe::monster_action_runtime::dungeon_gizmo {bool capture_live(const a::Context&,const a::Binding&,int& out,std::string& error){if(live::apply_timing::take_depth)need(live::apply_timing::active==live::apply_timing::Part::ClientManagementGizmoRead,"actual Gizmo read lies inside child timer");if(fail_reader==2){error="original Gizmo failure";return false;}out=7;return true;}}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
namespace thought_frame_native {
struct Bound {live::Pin pin;int identity=0;bool operator==(const Bound&)const=default;};
struct Snapshot {Bound bound;int owners=0,value=0;};
inline int reads=0,samples=0,drift=0,fail_at=0;
bool once(Runtime&,const a::Context&,const a::Binding&,Snapshot& out,std::string& error){
    ++reads;if(reads==fail_at)return fail(error,"fixture original read failed");out={};out.value=8;
    if(reads==2){if(drift==1)out.bound.identity=1;if(drift==2)out.owners=1;if(drift==3)out.value=9;if(drift==4)out.bound.pin.identity=2;}
    return true;
}
bool sample(Runtime& r,const a::Context& c,const a::Binding& b,Snapshot& out,std::string& error){
    ++samples;Snapshot first;if(!once(r,c,b,first,error)||!once(r,c,b,out,error))return false;
    return (first.bound==out.bound&&first.owners==out.owners&&first.value==out.value)||fail(error,"fixture standalone pair changed");
}
}
#include "management_calendar_actual.inc"
namespace analytics_state {using State=int;using Owners=int;}
namespace analytics_native {bool sample(Runtime&,const a::Context&,const a::Binding&,int&,int& out,live::Pin& pin,std::string& error){++analytics_reads;if(fail_reader==3)return fail(error,"original Analytics failure");out=9;pin={};return true;}}
bool services_read(Runtime&,const a::Context&,const a::Binding&,Local& local,int& out,std::string& error){++service_reads;if(fail_reader==4)return fail(error,"original Service failure");local.services=1;local.service_destinations=2;out=3;return true;}
namespace general_project_runtime {bool capture_progress(const a::Context&,const a::Binding&,int& out,std::string& error){++project_reads;if(fail_reader==5)return fail(error,"original Project failure");out=6;return true;}}
inline int primary_calls=0,local_drift=0,value_drift=0;
bool primary_once(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,management::LiveState& out,thought_frame_native::Snapshot& thoughts,std::string& e,bool planning){
    ++primary_calls;local={};out={};if(primary_calls==2){local.software=local_drift;out.other=value_drift;}
#include "management_planning_analytics_actual.inc"
    if(!calendar_sample(r,context,binding,local.pin,out,e,&thoughts,planning))return false;
#include "management_planning_tail_actual.inc"
}
#include "management_primary_actual.inc"
struct FailureFixture {
    struct Receipt {bool attempted=false,unknown=false;std::string cause;};
    struct Frame {uint64_t revision=17;std::optional<management::LiveState> management{std::in_place};};
    bool locked_=false;std::string cause_;nlohmann::json last_;
    nlohmann::json services_difference(const Local&,int,int){return nullptr;}
    bool fail_child(Receipt& receipt,std::string& e){Local local;management::LiveState before;Frame frame;
#include "management_apply_failure_actual.inc"
    }
};
void run(){
    Runtime r;a::Context context;a::Binding binding;Local local;management::LiveState out;std::string error;
    auto reset=[&](){thought_frame_native::reads=0;thought_frame_native::samples=0;thought_frame_native::drift=0;thought_frame_native::fail_at=0;primary_calls=0;local_drift=0;value_drift=0;calendar_reads=calendar_failure=calendar_drift=analytics_reads=service_reads=project_reads=0;fail_reader=0;error.clear();};
    reset();need(primary_read(r,context,binding,local,out,error),"Management stable full outer pair");
    need(thought_frame_native::reads==2&&thought_frame_native::samples==0&&out.thoughts==8,"Management two fresh Thought reads, complete value retained");
    for(int drift:{1,2,3}){reset();thought_frame_native::drift=drift;need(!primary_read(r,context,binding,local,out,error)&&error=="Thought complete current owners/values changed during double read","Management rejects Bound, Owners or value drift");}
    reset();thought_frame_native::drift=4;need(!primary_read(r,context,binding,local,out,error)&&error=="Management Thought complete Clock/Ledger origin differs","Management retains each sample pin-origin guard");
    for(int fail_at:{1,2}){reset();thought_frame_native::fail_at=fail_at;need(!primary_read(r,context,binding,local,out,error)&&error=="fixture original read failed","Management retains original read failure");}
    reset();local_drift=1;need(!primary_read(r,context,binding,local,out,error)&&error=="Management live roots/scalars changed during double read","Management retains outer Local guard");
    reset();value_drift=1;need(!primary_read(r,context,binding,local,out,error)&&error=="Management live roots/scalars changed during double read","Management retains complete value guard");
    reset();need(calendar_sample(r,context,binding,local.pin,out,error)&&thought_frame_native::reads==2&&thought_frame_native::samples==1,"Standalone and finish retain original double sample");
    reset();thought_frame_native::drift=2;need(!calendar_sample(r,context,binding,local.pin,out,error)&&error=="fixture standalone pair changed","Standalone owner drift still rejected");
    need(live::apply_timing::intro_count==0&&live::apply_timing::gizmo_count==0,"actual calendar child timers absent outside Take");
    reset();live::apply_timing::take_depth=1;
    need(primary_read(r,context,binding,local,out,error)&&live::apply_timing::intro_count==2&&live::apply_timing::gizmo_count==2,"actual paired calendar times each Intro and Gizmo read once");
    for(int reader:{1,2}){reset();fail_reader=reader;
        need(!calendar_sample(r,context,binding,local.pin,out,error)&&error==(reader==1?"original Intro failure":"original Gizmo failure"),"actual calendar child keeps original failure cause");
        need(!live::apply_timing::active.has_value(),"actual calendar failure unwinds all timer scopes");}
    fail_reader=0;live::apply_timing::take_depth=0;
    reset();need(primary_read(r,context,binding,local,out,error,true),"Client planning actual paired read succeeds");
    need(primary_calls==2&&calendar_reads==2&&service_reads==2&&project_reads==2&&analytics_reads==0&&thought_frame_native::reads==0,
         "planning retains Calendar/Demand, Service and Project but omits unused Analytics and Thought reads");
    need(out.intro==0&&out.thoughts==0&&out.gizmos==0&&out.analytics==0&&out.character_calendar==4&&out.subscriber_demands==5&&out.services==3&&out.project==6,
         "planning output truthfully omits independent children and retains required values");
    for(int fail_at:{1,2}){reset();calendar_failure=fail_at;
        need(!primary_read(r,context,binding,local,out,error,true)&&error=="original Calendar/Demand reciprocal failure","planning preserves original reciprocal failure in both passes");}
    reset();calendar_drift=1;need(!primary_read(r,context,binding,local,out,error,true)&&error=="Management live roots/scalars changed during double read","planning retains reciprocal pair drift rejection");
    reset();local_drift=1;need(!primary_read(r,context,binding,local,out,error,true)&&error=="Management live roots/scalars changed during double read","planning retains root owner pair drift rejection");
    for(int reader:{1,2,3}){reset();fail_reader=reader;need(primary_read(r,context,binding,local,out,error,true),"planning does not invoke deferred independent child captures");}
    for(int reader:{4,5}){reset();fail_reader=reader;need(!primary_read(r,context,binding,local,out,error,true)&&error==(reader==4?"original Service failure":"original Project failure"),"planning preserves required child failure");}
    reset();context.role=a::Role::Host;need(!primary_read(r,context,binding,local,out,error,true)&&primary_calls==0&&error=="Management planning read requires Client role","Host cannot request partial planning");
    reset();need(primary_read(r,context,binding,local,out,error)&&analytics_reads==2&&thought_frame_native::reads==2&&out.intro==6&&out.gizmos==7,"Host/default callers keep complete capture");context.role=a::Role::Client;
    reset();fail_reader=3;need(!primary_read(r,context,binding,local,out,error)&&error=="original Analytics failure","full read retains Analytics validation");
    for(bool attempted:{false,true})for(const char* cause:{"original Thought owner failed","original Gizmo owner failed","original Intro owner failed","original Analytics owner failed"}){
        FailureFixture failure;FailureFixture::Receipt receipt;receipt.attempted=attempted;error=cause;
        need(!failure.fail_child(receipt,error)&&error==cause&&receipt.cause==cause&&receipt.unknown==attempted&&failure.locked_==attempted&&failure.last_.at("confirmed")==false&&failure.last_.at("unknown")==attempted,
             "actual child failure preserves cause and locks Unknown after prior writes");
    }
}
}
