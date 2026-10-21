#include "placement_ui_probe/ui_probe.h"
#include "placement_ui_diagnostics/admission.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace placement_ui_probe {
namespace fixture {
Context context{1,2,3,4,5};
std::vector<unsigned> calls;
Receipt published;
bool active=false,pending=false,pressed=false,context_valid=true;
unsigned fail_step=99;
std::array<float,3> pose{};
void reset(){calls.clear();published={};active=false;pending=false;pressed=false;context_valid=true;fail_step=99;pose={};}
}
bool fail(std::string& e,const char* s){e=s;return false;}
bool context_actual(const Context& c,std::string& e){return fixture::context_valid&&c==fixture::context||fail(e,"fixture context differs");}
bool scalar_request(const Request&,std::string&){return true;}
bool ProcessMemory::copy(std::uintptr_t,void*,std::size_t,std::string&){return false;}
bool Probe::initialize(const Context& c,std::string&){context_=c;return true;}
bool read_existing_tool(Memory&,const Context& c,const Request& r,Binding& b,std::string&){
    b.context=c;b.family=r.family;b.behaviour=100;b.cursor=200;b.button=300;return true;
}
bool read_palette_index(Memory&,const Binding&,const Request&,std::int32_t& i,std::uintptr_t& p,std::string&){i=0;p=400;return true;}
bool read_scenery_mode_window(Memory&,const Binding&,std::uintptr_t& p,std::string&){p=500;return true;}
bool find_mode_pane_seh(const Binding&,std::uintptr_t,std::uintptr_t& p,Receipt&){p=600;return true;}
bool read_scenery_mode_button(Memory&,const Binding&,std::uintptr_t,std::uintptr_t,std::string&){return true;}
bool read_scenery_release_pose(Memory&,const Binding&,const Request& r,NativePose& p,std::string&){p.position=r.position;return true;}
bool merge_release_admission(const Request&,const Receipt&,Receipt&,std::string&){return false;}
bool call_seh(unsigned step,const Binding& b,const Request& r,std::uintptr_t,std::int32_t,Receipt& out)noexcept{
    fixture::calls.push_back(step);out.attempted_mask|=1u<<step;
    if(step==fixture::fail_step){out.exception=0xdead;return false;}
    if(b.family==Family::Building&&step>=2&&step<=4&&fixture::published.phase!=Phase::Attempted)return false;
    if(step==2){fixture::pose=r.position;fixture::active=true;fixture::pending=false;}
    if(step==3)fixture::pressed=true;
    if(step==4){fixture::active=false;fixture::pending=true;fixture::pressed=false;}
    out.returned_mask|=1u<<step;return true;
}
bool read_live_pose_and_pending(Memory&,const Binding&,const Request& r,bool pending,std::string& e){
    return fixture::pose==r.position&&fixture::pending==pending&&fixture::active!=pending||fail(e,"fixture actual pose or pending differs");
}
#include "placement_input_order.inc"
}
namespace ui=placement_ui_probe;
struct Services final:ui::RootServices {
    ui::Binding binding;
    bool reserved=false,changed=false,routes=true;
    bool actual_context(ui::Context& c,std::string&)override{c=ui::fixture::context;if(changed)++c.generation;return true;}
    bool actual_input_routes(const ui::Binding&,std::string& e)override{if(!routes)e="fixture routes changed";return routes;}
    bool reserve_job_once(const ui::Request&,const ui::Binding& b,std::string&)override{if(reserved)return false;binding=b;reserved=true;return true;}
    bool read_actual_admission(const ui::Request&,ui::Receipt& r,std::string&)override{r={};return true;}
    void record_receipt(const ui::Request&,const ui::Receipt& r)noexcept override{ui::fixture::published=r;}
};
namespace observer {
std::uint32_t fixture_thread=5,thread=5;
std::uint32_t GetCurrentThreadId(){return fixture_thread;}
struct Job {struct {ui::Request native;} input;ui::Binding binding;ui::Receipt receipt;};
Job* active=nullptr;
unsigned observations=0;
struct Services final:ui::RootServices {
    Job& job;
    explicit Services(Job& j):job(j){}
    bool actual_context(ui::Context& c,std::string&)override{c=ui::fixture::context;return true;}
    bool actual_input_routes(const ui::Binding&,std::string&)override{return true;}
    bool reserve_job_once(const ui::Request&,const ui::Binding&,std::string&)override{return false;}
    bool read_actual_admission(const ui::Request&,ui::Receipt&,std::string&)override{return false;}
    void record_receipt(const ui::Request&,const ui::Receipt& r)noexcept override{job.receipt=r;ui::fixture::published=r;}
};
void mark_unknown(Job& j,const std::string& cause){j.receipt.phase=ui::Phase::Unknown;j.receipt.cause=cause;}
void record_frame(Job&,void*,float,const char*,bool){++observations;}
void observation_exception(Job& j,const char*,const std::string& cause){mark_unknown(j,cause);}
#include "placement_observer_order.inc"
}
unsigned checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
ui::Request request(){ui::Request r;r.token="one";r.variant="base";r.position={3963.1943359375f,39.103004455566406f,7980.14453125f};return r;}
ui::Receipt arm(Services& services,const ui::Request& r){
    ui::Probe probe;std::string error;probe.initialize(ui::fixture::context,error);return probe.arm(r,services);
}
#include "placement_observer_deadline_cases.inc"
int main()try{
    observer_deadline_cases();
    auto r=request();ui::fixture::reset();Services services;auto first=arm(services,r);
    require(first.phase==ui::Phase::ArmedAwaitingGameUpdate,"arm waits for normal Update");
    require(ui::fixture::calls==std::vector<unsigned>({0,1,7}),"arm selects and warps only; no premature Press or Release");
    observer::Job job{{r},services.binding,first};observer::active=&job;
    observer::build_update_entered(reinterpret_cast<void*>(101),0.016f);
    require(ui::fixture::calls.size()==3,"other behaviour receiver cannot consume input");
    observer::fixture_thread=6;observer::build_update_entered(reinterpret_cast<void*>(100),0.016f);
    require(ui::fixture::calls.size()==3,"foreign thread cannot consume input");
    observer::fixture_thread=5;
    // The normal Cursor hover creates the current preview before Build.Update.
    ui::fixture::active=true;ui::fixture::pending=false;ui::fixture::pose={4433.9658f,159.329f,7277.6514f};
    observer::build_update_entered(reinterpret_cast<void*>(100),0.016f);
    auto complete=job.receipt;
    require(complete.phase==ui::Phase::ArmedAwaitingGameUpdate,"normal boundary completes original input");
    require(ui::fixture::calls==std::vector<unsigned>({0,1,7,2,3,4}),"original input executes once after normal hover");
    require(ui::fixture::pending&&!ui::fixture::active&&ui::fixture::pose==r.position,"unchanged capture sees pending exact requested pose");
    observer::build_update_entered(reinterpret_cast<void*>(100),0.016f);
    require(ui::fixture::calls.size()==6,"later normal Update does not replay input");
    auto duplicate=ui::complete_build_input_at_update(r,services.binding,complete,services);
    require(duplicate.phase==ui::Phase::Unknown&&ui::fixture::calls.size()==6,"duplicate boundary cannot replay");
    ui::fixture::reset();Services changed;first=arm(changed,r);changed.changed=true;
    complete=ui::complete_build_input_at_update(r,changed.binding,first,changed);
    require(complete.phase==ui::Phase::Unknown&&ui::fixture::calls.size()==3,"changed generation prevents all input");
    ui::fixture::reset();Services broken;first=arm(broken,r);ui::fixture::fail_step=3;
    complete=ui::complete_build_input_at_update(r,broken.binding,first,broken);
    require(complete.phase==ui::Phase::Unknown&&complete.exception==0xdead,"native Press failure retained");
    require(ui::fixture::calls==std::vector<unsigned>({0,1,7,2,3}),"failed Press never invokes Release");
    auto failed_repeat=ui::complete_build_input_at_update(r,broken.binding,complete,broken);
    require(failed_repeat.phase==ui::Phase::Unknown&&ui::fixture::calls.size()==5,"failed native input cannot replay");
    ui::fixture::reset();Services warp;ui::fixture::fail_step=7;first=arm(warp,r);
    require(first.phase==ui::Phase::Unknown&&ui::fixture::calls==std::vector<unsigned>({0,1,7}),"failed warp stops before placement input");
    ui::fixture::reset();Services scene;r.family=ui::Family::Scenery;first=arm(scene,r);
    require(first.phase==ui::Phase::ArmedAwaitingGameUpdate&&ui::fixture::calls==std::vector<unsigned>({0,5,6,1,2,3,4}),"Scenery keeps synchronous original Release route");
    require(first.native_pose_measured,"Scenery keeps actual release pose measurement");
    std::cout<<checks<<" actual placement input ordering checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
