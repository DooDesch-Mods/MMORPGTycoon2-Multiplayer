#include "mt2mp/hooks/original_body.h"
#include "bug_policy.h"
#include "bug_policy_read.h"
#include "bug_policy_calls.h"
#include "bug_policy_fingerprints.h"
#include "developer_staff/probe.h"
#include "hud_time_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cmath>

namespace mt2mp::game::developer_staff {uint32_t original_show(uintptr_t,uintptr_t);}
namespace mt2mp::game::developer_staff::bug_policy {
namespace {
using json=nlohmann::json;
Fixture fixture;
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& why){e=why;return false;}
const auto copy=[](uintptr_t p,void* out,size_t size,std::string& e){return memory.copy(p,out,size,e);};
bool code(uintptr_t image,std::string& e){
    try{for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(!hooks::framework::verify_original_hash(image+body.rva,bytes.data(),bytes.size(),body.sha,e))return fail(e,"Bug-C policy original complete body differs RVA="+std::to_string(body.rva)+" original="+e);}}
    catch(const std::exception& ex){return fail(e,std::string("Bug-C policy native body hash failed original=")+ex.what());}
    return hud_time_runtime::verify_window_show_hook(image+0x81ec20,e);
}
struct NativePort final:Port {
    uintptr_t image=0;
    bool read(Snapshot& out,std::string& e)override{
        Capture first,after;
        if(!authorize_ui_update(first,e))return false;
        if(!Config::instance().local_test||!Config::instance().is_host||first.thread!=GetCurrentThreadId()||first.image!=uintptr_t(GetModuleHandleW(nullptr)))
            return fail(e,"Bug-C policy UI requires owned local Host/GameUpdate/Mainthread/process");
        const auto status=typed_action_runtime::controller_status();
        if(!ready_controller(status,typed_action_runtime::live_state_active(),e))return false;
        action_session::Context context;action_session::Binding binding;
        if(!action_session::selected_session().current(context,e)||context.role!=action_session::Role::Host||context.local_peer!=context.host_peer||
           !typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return fail(e,e.empty()?"Bug-C policy actual Host session/live authority unavailable":e);
        if(binding.native_epoch!=first.pin.origin.world_epoch||binding.native_generation!=first.pin.origin.generation)return fail(e,"Bug-C policy current Controller/native Origin differs");
        native::WorldStatus world;if(!native::read_world_status(world,e))return false;
        if(world.active_mode_index!=3||world.in_game_state!=4||!world.world_present||world.quitting||world.title_transition_pending||world.map_task_pending||world.saves_in_progress>0||world.edit_operations)
            return fail(e,"Bug-C policy UI requires idle actual InGame without edit/save/transition");
        if(!std::isfinite(first.pin.time_scale)||first.pin.time_scale!=0||!std::isfinite(first.pin.pause_scale)||first.pin.pause_scale<0)
            return fail(e,"Bug-C policy UI proof requires actual paused native Host Clock0");
        if(!code(first.image,e)||!read_policy(copy,first,out,e))return false;
        Snapshot again;
        if(!authorize_ui_update(after,e)||!read_policy(copy,after,again,e))return false;
        if(!same_owner(out,again)||out.capture.state!=again.capture.state||out.policies!=again.policies)return fail(e,"Bug-C policy full native ownership/policy double read differs");
        image=first.image;return true;
    }
    bool window(const Snapshot& s,Control& out,std::string& e,bool shown=false)override{
        if(!read_tabs(copy,s,out,e))return false;
        if(shown){uint8_t visible=0;if(!detail::at(copy,out.window,0x45f,visible,e))return false;
            if(visible!=1)return fail(e,"Bug-C policy original Show visible readback differs");}
        return true;
    }
    bool control(const Snapshot& s,Scope scope,Control& out,bool pressed,std::string& e)override{return read_control(copy,s,scope,out,pressed,e);}
    bool show(const Control& c,std::string& e)override{const auto exception=developer_staff::original_show(image,c.window);return !exception||fail(e,"Bug-C policy original Window.Show SEH="+std::to_string(exception));}
    bool page(const Control& c,std::string& e)override{
        const auto exception=original_page(image,c.tabs);
        return !exception||fail(e,"Bug-C policy original SetPage SEH="+std::to_string(exception));
    }
    bool requested(const Control& c,std::string& e)override{
        std::string requested;if(!detail::text(copy,c.view,0x3e8,requested,e))return false;
        return requested=="{dev_tab_policies}"||fail(e,"Bug-C policy original SetPage request readback differs");
    }
    bool input(const Control& c,Input input,std::string& e)override{
        const auto exception=original_input(image,c.checkbox,input);
        return !exception||fail(e,"Bug-C policy original checkbox input SEH="+std::to_string(exception)+" phase="+std::to_string(int(input)));
    }
};
bool access(std::string& e){Capture c;return authorize_ui_update(c,e);}
}
bool observe(json& out,std::string& e){
    out={{"observed",false}};if(!access(e))return false;
    out["locked"]=fixture.locked();out["cause"]=fixture.cause();out["receipts"]=json::array();
    for(const auto& [token,r]:fixture.receipts()){(void)token;out["receipts"].push_back(encode(r));}
    NativePort port;Snapshot s;
    if(!port.read(s,e)){out["read_error"]=e;return false;}
    out["observed"]=true;out["state"]=encode(s);out["ui"]=json::object();
    for(const auto scope:{Scope::Online,Scope::Offline}){Control control;PageWitness page;std::string cause;
        const bool ready=read_control(copy,s,scope,control,false,cause,&page);
        out["ui"][scope==Scope::Online?"online":"offline"]={{"ready",ready},{"cause",cause},{"page",page_witness(page)}};
    }
    return true;
}
bool run(const json& args,json& out,std::string& e){
    out={{"confirmed",false},{"unknown",false},{"attempted_mask",0},{"peer_ack",false}};
    if(!access(e))return false;
    Request request;if(!parse(args,request,e))return false;
    NativePort port;Receipt receipt;const bool okay=fixture.run(request,port,receipt,e);out=encode(receipt);return okay;
}
}
