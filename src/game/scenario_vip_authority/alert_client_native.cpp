#include "alert_client_native.h"
#include "alert_client_reader.h"
#include "../native_open_runtime.h"
#include "../session_provider.h"
#include "../live_host_state/memory_query_timing.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include "disconnect_read.h"
#include "disconnect_invoke.h"

namespace mt2mp::game::scenario_vip_authority::client_alert {
namespace {
using Pages=live_host_state::apply_timing::PageReadScope;
using Boundary=live_host_state::apply_timing::PageBoundary;
using World=building_mixed_identity::opened_file::CurrentWorld;
bool fail(std::string& e,const char* cause){e=cause;return false;}
bool same_world(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&
    a.world==b.world&&a.map==b.map&&a.world_epoch==b.world_epoch&&a.generation==b.generation&&
    a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes;}
bool effect_code(placement_ui_probe::Memory& memory,uintptr_t image,std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    constexpr Body bodies[]={
        {0x233480,288,"2c3a4ab3023a083597fef6254fb2f8e89361728d64187f91b194253e19487252"},
        {0x232e80,336,"1ee62d33efcbabe851255387a247b5a72b41f70a33951dd2a2ddbf767b613062"},
        {0x234b00,48,"f34ef7bb7f4ba44ce943c76f9300ca1926ba1ed704696792d2e722e42d00b4bd"},
        {0x234a20,224,"a3230264e909b7cb3058d02a3dcf9e8a760e07dca749472540ac97a0e23dd506"},
        {0x234b30,320,"206ce9ed032b7dec2d4f075ed68b26df7e68970c17632275e98f4da06f738a18"},
        {0x234c70,32,"8ea77603a041e0575232131d1320b35d3477a3531773c307b5b98175b6e6d05d"}};
    Pages pages;
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha){e="VIP original effect body differs RVA="+std::to_string(body.rva);return false;}}
    return true;
}
bool message_code(placement_ui_probe::Memory& memory,uintptr_t image,std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    constexpr Body bodies[]={
        {0x2330a0,528,"aaff7f4158fb9d7d37d6874f5e06a3b6074e28bf1b3b700aacafae6c985de1c1"},
        {0x2349a0,128,"c77587ff94471a388406f8196aaf59db5959f6c6c82f4f4177bb22ef67c69228"},
        {0x238990,432,"7dbb2151878149fe46f78bdebeb221b4e8587d9351ea1e9d693215d70ea39f43"},
        {0x238170,2080,"f05210f6a5c171cd5000e29c02e871fc624dc6c5c6073eec6dfcf126873c38b4"},
        {0xa16f10,192,"0558e10498c849f04b3af18716eca5ac8deef6e1b82fb12ad27263fd9ff8b1cb"},
        {0xa18e60,176,"3ade962fa191a303d5b60871adef0a5ac791780c1eab4214b49761cc7865b5d8"},
        {0x11b2b00,176,"e6155e49c38d380ed513ce15ead8505762a5e50d6f1bde2e9985ec668fb95524"},
        {0x11bd1e0,32,"4a8cc79ce002b866698caded8eaea996814366638d34ce5cdf3ccb0394dc95e5"}};
    Pages pages;
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha){e="VIP original message effect body differs RVA="+std::to_string(body.rva);return false;}}
    return effect_code(memory,image,e);
}
using Create=uintptr_t(*)(int32_t,uintptr_t,int32_t);
using Show=void(*)(uintptr_t,bool);
using Destroy=void(*)(uintptr_t);
int filter(uint32_t code){return code==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER;}
void create_call(Create fn,uintptr_t toon,Call& r){
    __try{r.attempted=true;r.result=fn(8,toon,0);r.returned=true;}
    __except(filter(GetExceptionCode())){r.exception=GetExceptionCode();}
}
void show_call(Show fn,uintptr_t alert,bool shown,Call& r){
    __try{r.attempted=true;fn(alert,shown);r.returned=true;}
    __except(filter(GetExceptionCode())){r.exception=GetExceptionCode();}
}
void destroy_call(Destroy fn,uintptr_t alert,Call& r){
    __try{r.attempted=true;fn(alert);r.returned=true;}
    __except(filter(GetExceptionCode())){r.exception=GetExceptionCode();}
}
bool returned(const Call& r,const char* name,std::string& e){
    if(r.returned){e.clear();return true;}
    e=std::string("VIP original ")+name+" did not return exception="+std::to_string(r.exception);return false;
}
}
bool NativePort::owner(const Owner* expected,bool teardown,Owner& out,std::string& e){
    out={};
    if((message_type_!=0&&message_type_!=9)||!authorize_||!vip::valid_origin(origin_)||context_.role!=action_session::Role::Client||
       origin_.session!=context_.session||origin_.snapshot!=context_.snapshot_uuid||
       !binding_.native_epoch||!binding_.native_generation)return fail(e,"VIP Client provider/binding missing");
    if(!authorize_(origin_,context_,binding_,teardown,e))return false;
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,e))return false;
    if(thread!=GetCurrentThreadId()||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
        return fail(e,"VIP pinned Mainthread/executable differs");
    World before,after;action_session::Context current;
    if(teardown){
        if(!expected||expected->image!=image||expected->thread!=thread||expected->epoch!=binding_.native_epoch||
           expected->generation!=binding_.native_generation)return fail(e,"VIP teardown lacks retained exact local lifetime");
        out=*expected;
    }else{
        if(!action_session::selected_session().current(current,e)||current!=context_)
            return fail(e,e.empty()?"VIP current Session differs":e.c_str());
        if(!native_open_runtime::read_current_world(before,e))return false;
        if(before.world_epoch!=binding_.native_epoch||before.generation!=binding_.native_generation)
            return fail(e,"VIP completed World differs from local Binding");
        native::ClockState clock;if(!native::read_clock(clock,e))return false;
        if(clock.time_scale!=0)return fail(e,"VIP Client actual simulation Clock is not zero");
        out.image=image;out.thread=thread;out.game=uintptr_t(before.game);out.mode=uintptr_t(before.mode);
        out.state=uintptr_t(before.state);out.world=uintptr_t(before.world);out.map=uintptr_t(before.map);
        out.epoch=before.world_epoch;out.generation=before.generation;out.native_uuid=before.uuid;out.native_sha=before.sha256;
    }
    placement_ui_probe::ProcessMemory memory;alert_probe::Graph graph;
    {
        Pages pages;uintptr_t mode=0,state=0,ap=0;
        if(!toon_weapon_visual::visual_read(memory,out.game,0x28,mode,e)||mode!=out.mode||
           !toon_weapon_visual::visual_read(memory,mode,0,ap,e)||ap!=image+0x16e6730||
           !toon_weapon_visual::visual_read(memory,mode,0x58,state,e)||state!=out.state||
           !toon_weapon_visual::visual_read(memory,state,0,ap,e)||ap!=image+0x16de920)
            return fail(e,e.empty()?"VIP retained Game/Mode/State receiver differs":e.c_str());
        Reader reader(memory);if(!reader.graph(out,graph,e))return false;
    }
    out.hud=graph.hud;out.manager=graph.manager;out.window=graph.window;out.window_ap=graph.window_ap;out.list=graph.list;
    if(expected&&out!=*expected)return fail(e,"VIP exact retained UI/local World owner differs");
    if(!teardown){
        if(!native_open_runtime::read_current_world(after,e)||!action_session::selected_session().current(current,e))return false;
        if(!same_world(before,after)||current!=context_)return fail(e,"VIP owner changed during fresh graph read");
    }
    if(!authorize_(origin_,context_,binding_,teardown,e))return false;
    e.clear();return true;
}
bool NativePort::subject(const Owner& o,int32_t id,const std::string& name,Subject& out,std::string& e){
    Pages pages;placement_ui_probe::ProcessMemory memory;Reader reader(memory);return reader.subject(o,id,name,out,e);
}
bool NativePort::inspect(const Owner& o,uintptr_t alert,const Subject& s,bool live,int shown,uintptr_t& selector,std::string& e){
    Pages pages;placement_ui_probe::ProcessMemory memory;Reader reader(memory);return reader.inspect(o,alert,s,live,shown,selector,e);
}
bool NativePort::absent(const Owner& o,uintptr_t alert,uintptr_t selector,std::string& e){
    Pages pages;placement_ui_probe::ProcessMemory memory;Reader reader(memory);return reader.absent(o,alert,selector,e);
}
bool NativePort::create(const Owner& o,const Subject& s,Call& r,std::string& e){
    r={};Owner fresh;Subject selected;
    if(!owner(&o,false,fresh,e)||!subject(fresh,s.id,s.name,selected,e))return false;
    if(selected!=s)return fail(e,"VIP constructor Subject changed before original call");
    placement_ui_probe::ProcessMemory memory;if(!effect_code(memory,o.image,e))return false;
    try{Pages original(Boundary::OriginalCall);create_call(reinterpret_cast<Create>(o.image+0x233480),s.toon,r);return returned(r,"CreateToonAlert",e);}
    catch(const std::exception& x){e=std::string("VIP CreateToonAlert exception: ")+x.what();return false;}
    catch(...){e="VIP CreateToonAlert nonstandard exception";return false;}
}
bool NativePort::show(const Owner& o,uintptr_t alert,bool shown,Call& r,std::string& e){
    r={};placement_ui_probe::ProcessMemory memory;
    // Lifecycle freshly checks the exact owner immediately before each call.
    if(!effect_code(memory,o.image,e))return false;
    try{Pages original(Boundary::OriginalCall);show_call(reinterpret_cast<Show>(o.image+0x234b00),alert,shown,r);return returned(r,shown?"Show":"Hide",e);}
    catch(const std::exception& x){e=std::string("VIP Show exception: ")+x.what();return false;}
    catch(...){e="VIP Show nonstandard exception";return false;}
}
bool NativePort::destroy(const Owner& o,uintptr_t alert,Call& r,std::string& e){
    r={};placement_ui_probe::ProcessMemory memory;uintptr_t ap=0,d0=0;
    if(!effect_code(memory,o.image,e)||!toon_weapon_visual::visual_read(memory,alert,0,ap,e)||
       !toon_weapon_visual::visual_read(memory,ap,8,d0,e))return false;
    if(ap!=o.image+0x1749770||d0!=o.image+0x234c70)return fail(e,"VIP deleting destructor receiver/vtable differs");
    try{Pages original(Boundary::OriginalCall);destroy_call(reinterpret_cast<Destroy>(d0),alert,r);return returned(r,"deleting destructor",e);}
    catch(const std::exception& x){e=std::string("VIP deleting destructor exception: ")+x.what();return false;}
    catch(...){e="VIP deleting destructor nonstandard exception";return false;}
}
bool NativePort::create_message(const Owner& o,const std::string& text,MessageCalls& r,std::string& e){
    r={};Owner current;if(!owner(&o,false,current,e))return false;
    if(text.empty()||text.size()>live_host_state::max_logical_frame_payload||text.find('\0')!=std::string::npos||!npc_census::utf8(text))
        return fail(e,"VIP message constructor text differs from safe Frame payload");
    placement_ui_probe::ProcessMemory memory;if(!message_code(memory,o.image,e))return false;
    const message_invoke::Functions functions{
        reinterpret_cast<message_invoke::LocConstruct>(o.image+0xa16f10),
        reinterpret_cast<message_invoke::Factory>(o.image+0x2330a0),
        reinterpret_cast<message_invoke::Dispose>(o.image+0x11b2b00),
        reinterpret_cast<message_invoke::Dispose>(o.image+0x11bd1e0)};
    try{Pages original(Boundary::OriginalCall);message_invoke::create(functions,text.c_str(),r,message_type_);
        return returned(r.loc_construct,"LocString C-string constructor",e)&&returned(r.create,"CreateMessageAlert",e)&&
            returned(r.loc_arguments_destroy,"LocString argument vector destructor",e)&&returned(r.loc_string_destroy,"LocString string disposal",e);
    }catch(const std::exception& x){e=std::string("VIP CreateMessageAlert exception: ")+x.what();return false;}
    catch(...){e="VIP CreateMessageAlert nonstandard exception";return false;}
}
bool NativePort::inspect_message(const Owner& o,uintptr_t alert,const std::string& text,int shown,uintptr_t& selector,std::string& e){
    Pages pages;placement_ui_probe::ProcessMemory memory;Reader reader(memory);alert_probe::Graph graph;
    if(!reader.graph(o,graph,e))return false;
    if(graph.hud!=o.hud||graph.manager!=o.manager||graph.window!=o.window||graph.list!=o.list)
        return fail(e,"VIP message actual UI owner graph differs");
    std::string actual;return disconnect_read::inspect(memory,o.image,graph,alert,shown,&text,selector,actual,e,message_type_);
}
bool NativePort::fire_message(const Owner& o,uintptr_t alert,Call& r,std::string& e){
    r={};Owner current;if(!owner(&o,false,current,e))return false;
    placement_ui_probe::ProcessMemory memory;if(!message_code(memory,o.image,e))return false;
    try{Pages original(Boundary::OriginalCall);destroy_call(reinterpret_cast<Destroy>(o.image+0x2349a0),alert,r);return returned(r,"FireAndForget",e);}
    catch(const std::exception& x){e=std::string("VIP FireAndForget exception: ")+x.what();return false;}
    catch(...){e="VIP FireAndForget nonstandard exception";return false;}
}
bool NativePort::discard_message(const Owner& o,uintptr_t alert,Call& r,std::string& e){
    r={};Owner current;if(!owner(&o,false,current,e))return false;
    placement_ui_probe::ProcessMemory memory;
    {
        Pages pages;Reader reader(memory);alert_probe::Graph graph;uintptr_t selector=0;std::string text;
        if(!reader.graph(current,graph,e)||!disconnect_read::inspect(memory,o.image,graph,alert,0,nullptr,selector,text,e,message_type_))return false;
    }
    if(!message_code(memory,o.image,e))return false;
    try{Pages original(Boundary::OriginalCall);destroy_call(reinterpret_cast<Destroy>(o.image+0x234c70),alert,r);return returned(r,"unhanded message deleting destructor",e);}
    catch(const std::exception& x){e=std::string("VIP unhanded message destructor exception: ")+x.what();return false;}
    catch(...){e="VIP unhanded message destructor nonstandard exception";return false;}
}

}
