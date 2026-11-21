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
        {0x236260,288,"2c7b498d3932f5a3635e614f677549c4e0e244a03f3b950c50e7be19ff61fd24"},
        {0x235c60,336,"0dd3839d0b43080dbadaf0726bc5c7a5b6954ca1dced0273364333be93ef2446"},
        {0x2378e0,48,"fdcbf3f23e42477dd39c283c3a0dd57515bab8095cfeb40b6da7ea5d9b74a259"},
        {0x237800,224,"9a1a559b0030437c41a8ae4af51949c48d7922a031e832daafe8758984f57c25"},
        {0x237910,320,"baf504ce44f7304a203d697b62885737f51c4b55166dad8c34eda349e65cdb05"},
        {0x237a50,32,"ab6275aa676a0981aba210c41a7770ef0821f7786aefcb9ee35acbc94efecfb0"}};
    Pages pages;
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
        if(!memory.copy(image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha){e="VIP original effect body differs RVA="+std::to_string(body.rva);return false;}}
    return true;
}
bool message_code(placement_ui_probe::Memory& memory,uintptr_t image,std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    constexpr Body bodies[]={
        {0x235e80,528,"d660e46d6bb7f9d7dbcd6380322defa8340be374b3adb21b7524582ff605478d"},
        {0x237780,128,"bddf57ee0ca85af57b2ae04d0ffc245a69ca7ccbdcea323250d307f2744456ec"},
        {0x23b800,432,"beefcb2c0c3cf4e548cf3cc4670b074735b95f54fd8503ac17290e56990aa296"},
        {0x23afa0,2144,"3deb616623d87e08f1795bb7fbe481a35868c8d57f5bcb8fda44034969d2483b"},
        {0xa21c10,192,"2d56fe7dc155d03ab64d29a4eff04317e77c854b18d405856c22437c17e6274b"},
        {0xa237f0,176,"f99eb8d64dd44738df99c8a2aedab9c73761f43e356b274624f7888ab250f2c8"},
        {0x11be4a0,176,"a4efe58947d80e63e5d6ef578f2fcc1bcbbee2f949eb234f49e5c4143cfa9a08"},
        {0x11c8b80,32,"959c85086c72a0971d78253f37d70e041f67f69706a7872d0c0e387a720bf50d"}};
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
    if(thread!=GetCurrentThreadId()||sha!="7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb")
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
           !toon_weapon_visual::visual_read(memory,mode,0,ap,e)||ap!=image+0x16ecad0||
           !toon_weapon_visual::visual_read(memory,mode,0x58,state,e)||state!=out.state||
           !toon_weapon_visual::visual_read(memory,state,0,ap,e)||ap!=image+0x16e4cc0)
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
    try{Pages original(Boundary::OriginalCall);create_call(reinterpret_cast<Create>(o.image+0x236260),s.toon,r);return returned(r,"CreateToonAlert",e);}
    catch(const std::exception& x){e=std::string("VIP CreateToonAlert exception: ")+x.what();return false;}
    catch(...){e="VIP CreateToonAlert nonstandard exception";return false;}
}
bool NativePort::show(const Owner& o,uintptr_t alert,bool shown,Call& r,std::string& e){
    r={};placement_ui_probe::ProcessMemory memory;
    // Lifecycle freshly checks the exact owner immediately before each call.
    if(!effect_code(memory,o.image,e))return false;
    try{Pages original(Boundary::OriginalCall);show_call(reinterpret_cast<Show>(o.image+0x2378e0),alert,shown,r);return returned(r,shown?"Show":"Hide",e);}
    catch(const std::exception& x){e=std::string("VIP Show exception: ")+x.what();return false;}
    catch(...){e="VIP Show nonstandard exception";return false;}
}
bool NativePort::destroy(const Owner& o,uintptr_t alert,Call& r,std::string& e){
    r={};placement_ui_probe::ProcessMemory memory;uintptr_t ap=0,d0=0;
    if(!effect_code(memory,o.image,e)||!toon_weapon_visual::visual_read(memory,alert,0,ap,e)||
       !toon_weapon_visual::visual_read(memory,ap,8,d0,e))return false;
    if(ap!=o.image+0x174fb10||d0!=o.image+0x237a50)return fail(e,"VIP deleting destructor receiver/vtable differs");
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
        reinterpret_cast<message_invoke::LocConstruct>(o.image+0xa21c10),
        reinterpret_cast<message_invoke::Factory>(o.image+0x235e80),
        reinterpret_cast<message_invoke::Dispose>(o.image+0x11be4a0),
        reinterpret_cast<message_invoke::Dispose>(o.image+0x11c8b80)};
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
    try{Pages original(Boundary::OriginalCall);destroy_call(reinterpret_cast<Destroy>(o.image+0x237780),alert,r);return returned(r,"FireAndForget",e);}
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
    try{Pages original(Boundary::OriginalCall);destroy_call(reinterpret_cast<Destroy>(o.image+0x237a50),alert,r);return returned(r,"unhanded message deleting destructor",e);}
    catch(const std::exception& x){e=std::string("VIP unhanded message destructor exception: ")+x.what();return false;}
    catch(...){e="VIP unhanded message destructor nonstandard exception";return false;}
}

}
