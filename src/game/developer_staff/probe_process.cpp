#include "action_native.h"
#include "probe.h"
#include "job_census.h"
#include "job_target_observation.h"
#include "movement_observation.h"
#include "entry_fingerprints.h"
#include "window.h"
#include "hud_time_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "live_host_state/economic_fence.h"
#include "typed_action_runtime.h"
#include "mt2mp/core/config.h"
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mt2mp/core/hash.h"
#include <cmath>
#include <cstring>
#include <set>
namespace mt2mp::game::developer_staff {
uint32_t original_command(Command,uintptr_t,uintptr_t,NativeReceipt*);
uint32_t original_show(uintptr_t,uintptr_t);
namespace {
thread_local void* update_game=nullptr;
thread_local uint32_t update_depth=0;
Probe probe;
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(uintptr_t owner,size_t offset,T& out,std::string& e){
    if(!owner||owner>UINTPTR_MAX-offset||owner+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Developer fixture owned field address overflow");
    return memory.copy(owner+offset,&out,sizeof(T),e);
}
bool window_owner(const Capture& c,uintptr_t& out,std::string& e){
    return read_window([](uintptr_t p,void* target,size_t n,std::string& cause){return memory.copy(p,target,n,cause);},c,out,e);
}
bool code(const Capture& c,std::string& e){
    try{for(const auto& body:original_bodies){if(body.rva==0x6c7b70){if(!action_runtime::verify_entry(c.image+body.rva,e))return false;continue;}std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(c.image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha256)return fail(e,"Developer fixture original full body differs RVA="+std::to_string(body.rva));}}
    catch(const std::exception& ex){return fail(e,std::string("Developer fixture native body hash failed: ")+ex.what());}
    // The installed cash wrapper belongs to the existing EconomicFence, whose
    // initialization guards its original body and whose route is checked here.
    return live_host_state::economic_fence().can_arm(e)&&hud_time_runtime::verify_window_show_hook(c.image+0x81ec20,e);
}
struct ProcessProbe final:ProbePort {
    bool shown=false;uintptr_t shown_window=0;
    bool authorize(Capture& c,uintptr_t& window,std::string& e)override{
        if(!read_process(c,e))return false;
        if(!Config::instance().local_test||!Config::instance().is_host||update_depth!=1||!update_game||
           c.image!=uintptr_t(GetModuleHandleW(nullptr))||c.thread!=GetCurrentThreadId()||c.pin.origin.game!=update_game)
            return fail(e,"Developer fixture requires isolated Host/owned GameUpdate/Mainthread/process");
        const auto controller=typed_action_runtime::status();
        if(!controller.contains("initialized")||!controller.at("initialized").is_boolean()||controller.at("initialized").get<bool>()||controller.contains("error"))
            return fail(e,"Developer fixture requires actual uninitialized Controller");
        native::WorldStatus world;if(!native::read_world_status(world,e))return false;
        if(world.active_mode_index!=3||world.in_game_state!=4||!world.world_present||world.quitting||world.title_transition_pending||world.map_task_pending||world.saves_in_progress>0||world.edit_operations)
            return fail(e,"Developer fixture requires actual idle loaded InGame without quit/map/edit/save transition");
        if(c.pin.time_scale!=0||c.pin.pause_scale<0)return fail(e,"Developer fixture requires native selected Clock0");
        return window_owner(c,window,e)&&code(c,e);
    }
    bool call(Command command,uintptr_t image,uintptr_t window,NativeReceipt& r,std::string& e)override{
        if(command==Command::Show){shown=true;shown_window=window;r.attempted=true;r.seh_code=original_show(image,window);r.returned=r.seh_code==0;
            return !r.seh_code||fail(e,"Developer fixture original Window.Show SEH="+std::to_string(r.seh_code));}
        r.seh_code=original_command(command,image,window,&r);
        if(r.seh_code)return fail(e,"Developer fixture original UICommand SEH="+std::to_string(r.seh_code));
        if(r.cleanup_exception)return fail(e,"Developer fixture genuine vsRecord cleanup SEH="+std::to_string(r.cleanup_exception));
        if(!r.label_confirmed)return fail(e,"Developer fixture genuine vsRecord label differs");
        if(!r.returned||!r.accepted||!r.cleanup_confirmed)return fail(e,"Developer fixture original command/cleanup not confirmed");
        e.clear();return true;
    }
    bool after(Capture& c,std::string& e)override{
        if(!read_process(c,e))return false;
        if(shown){uintptr_t window=0;uint8_t visible=0;if(!window_owner(c,window,e)||window!=shown_window||!at(window,0x45f,visible,e))return fail(e,e.empty()?"Developer fixture Show owner changed":e);
            if(visible!=1)return fail(e,"Developer fixture original Show did not make exact window visible");}
        return true;
    }
};
bool thread(std::string& e){
    uintptr_t image=0;uint32_t current=0;std::string sha;
    if(!native::read_resolver_receipt(image,current,sha,e))return false;
    return (current==GetCurrentThreadId()&&update_depth==1&&update_game&&image==uintptr_t(GetModuleHandleW(nullptr))&&
        sha=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")||fail(e,"Developer fixture requires owned GameUpdate/exact process before receipt access");
}
}
FixtureUpdateScope::FixtureUpdateScope(void* game)noexcept:outer_(update_depth==0){if(outer_)update_game=game;++update_depth;}
FixtureUpdateScope::~FixtureUpdateScope(){if(update_depth)--update_depth;if(outer_)update_game=nullptr;}
bool authorize_ui_update(Capture& c,std::string& e){if(!thread(e)||!read_process(c,e))return false;return c.pin.origin.game==update_game||fail(e,"Developer UI proof actual GameUpdate owner differs");}
bool authorize_fixture(Capture& c,uintptr_t& window,std::string& e){ProcessProbe port;return port.authorize(c,window,e);}
bool observe_fixture(nlohmann::json& out,std::string& e){
    out={{"observed",false}};if(!thread(e))return false;Capture c;if(!read_process(c,e))return false;
    job_census::Census jobs;
    const job_census::Read copy=[](auto p,auto target,auto n,auto& cause){return memory.copy(p,target,n,cause);};
    if(!job_census::read(copy,c,jobs,e))return false;
    nlohmann::json targets;
    if(!job_census::observe_building_targets(copy,c,jobs,targets,e))return false;
    nlohmann::json movement;
    if(!job_census::observe_movement(copy,c,jobs,movement,e))return false;
    job_census::Census jobs_after;
    if(!job_census::read(copy,c,jobs_after,e))return false;
    if(jobs!=jobs_after)return fail(e,"Developer Job target read changed the actual queue/actor graph");
    Capture after;if(!read_process(after,e))return false;
    if(!same_origin(c.pin.origin,after.pin.origin)||c.pin.ledger!=after.pin.ledger||c.pin.time_scale!=after.pin.time_scale||
       c.pin.pause_scale!=after.pin.pause_scale||c.image!=after.image||c.thread!=after.thread||c.state!=after.state||c.local!=after.local)
        return fail(e,"Developer Job observation changed actual Origin/ledger/Clock/fixed50/local owners");
    out=encode(c);out["jobs"]=job_census::encode(jobs);out["job_building_targets"]=std::move(targets);out["developer_movement"]=std::move(movement);out["observed"]=true;out["fixture_locked"]=probe.locked();out["fixture_cause"]=probe.cause();out["receipts"]=nlohmann::json::array();
    for(const auto& [token,r]:probe.retained()){(void)token;out["receipts"].push_back(encode(r));}return true;
}
bool run_fixture(const nlohmann::json& args,nlohmann::json& out,std::string& e){
    out={{"native_attempted",false},{"native_returned",false},{"immediate_effect",false},{"unknown",false},{"fixture_only",true},{"peer_ack",false}};
    if(!thread(e))return false;
    if(!args.is_object()||args.size()!=6||!args.contains("command")||!args.contains("token")||!args.contains("native_origin_uuid")||!args.contains("epoch")||!args.contains("generation")||!args.contains("expected_count")||
       !args.at("command").is_string()||!args.at("token").is_string()||!args.at("native_origin_uuid").is_string()||
       !args.at("epoch").is_number_unsigned()||!args.at("generation").is_number_unsigned()||!args.at("expected_count").is_number_integer())
        return fail(e,"Developer fixture exact six typed fields required");
    const auto command=args.at("command").get<std::string>();
    if(command!="Hire"&&command!="Fire"&&command!="Show")return fail(e,"Developer fixture command must be Show/Hire/Fire");
    const auto count=args.at("expected_count").get<int64_t>();if(count<0||count>50)return fail(e,"Developer fixture expected count requires0..50");
    const Request request{command=="Hire"?Command::Hire:command=="Fire"?Command::Fire:Command::Show,args.at("token").get<std::string>(),args.at("native_origin_uuid").get<std::string>(),args.at("epoch").get<uint64_t>(),args.at("generation").get<uint64_t>(),int32_t(count)};
    ProcessProbe port;Receipt receipt;const bool ok=probe.run(request,port,receipt,e);out=encode(receipt);return ok;
}
}
