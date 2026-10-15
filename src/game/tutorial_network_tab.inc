#include "tutorial_network_tab_reader.h"
#include "tutorial_network_tab_code.h"
#include "mt2mp/game/native_engine.h"
#include "typed_action/action_session.h"

namespace mt2mp::game::tutorial_welcome {
namespace network_fixed {
namespace tab=tutorial_network_tab;
struct Memory {bool copy(uintptr_t p,void* out,size_t n,std::string& e){return bytes(p,out,n,e);}};
bool same(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.bytes==b.bytes&&a.uuid==b.uuid&&a.sha256==b.sha256;}
bool ledger(action_session::Ledger& out,std::string& e){native::SimulationState s;if(!native::read_simulation_state(s,e))return false;out={s.cash,s.clock.flat_day,int32_t(s.record_present),s.record_day,s.record_cash,s.record_income,s.record_expense,s.line_counts,s.line_income};return true;}
bool step(const World& w,uintptr_t image,std::string& e){
    uintptr_t log=0,global=0,instance=0;Memory m;std::vector<uintptr_t> instances,steps;
    if(!at(reinterpret_cast<uintptr_t>(w.state),0x1e8,log,e)||!at(image,0x13df330,global,e)||global!=log||!tab::typed(m,log,image+0x16fd7b0,e)||!tab::array(m,log,0x18,64,instances,e))return fail(e,e.empty()?"Network step actual Log owner differs":e);
    for(auto value:instances){std::string id;if(!tab::text(m,value+0x18,id,e))return false;if(id=="initial_tutorial"){if(instance)return fail(e,"Network step duplicate Initial instance");instance=value;}}
    if(!instance||!tab::typed(m,instance,image+0x173ce70,e))return fail(e,e.empty()?"Network step Initial instance absent":e);
    uintptr_t sequence=0,current=0;uint8_t complete=0;int32_t next=0;
    if(!at(instance,0x38,complete,e)||complete||!at(instance,0x58,sequence,e)||!tab::typed(m,sequence,image+0x16dbd80,e)||!at(sequence,0x54,complete,e)||complete||!at(sequence,0x48,current,e)||!at(sequence,0x50,next,e)||!tab::array(m,sequence,0x18,128,steps,e))return fail(e,e.empty()?"Network step sequence owner/completion differs":e);
    if(steps.size()!=107||next!=57||current!=steps[56])return fail(e,"Network tab requires actual Initial step56 next57 count107");
    if(!tab::typed(m,current,image+0x16fda00,e)||!at(current,0x18,complete,e)||complete||!literal(current,0x20,"",e))return fail(e,e.empty()?"Network step Message type/ID/completion differs":e);
    int32_t kind=-1,view=-1;std::string kind_name,view_name;
    if(!at(current,0xa0,kind,e)||!at(current,0x148,view,e)||kind<0||kind>=19||view<0||view>=5)return fail(e,e.empty()?"Network step Message enum bounds differ":e);
    if(!tab::text(m,image+0x1a38ba0+uintptr_t(kind)*32,kind_name,e)||!tab::text(m,image+0x1a36520+uintptr_t(view)*32,view_name,e))return false;
    if(kind_name!="view"||view_name!="Network")return fail(e,"Network step actual message kind/view differs");
    return true;
}
DWORD press(uintptr_t image,const tab::Pin& p){
    // This exact Press ignores cursor/position and consumes only its owned command.
    const std::array<float,2> position{};
    __try{reinterpret_cast<void(*)(void*,void*,const void*)>(image+0x57c950)(reinterpret_cast<void*>(p.tab),reinterpret_cast<void*>(p.cursor),&position);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Calls {
    const Request& request;World origin;uintptr_t image;Memory memory;action_session::Ledger before;std::set<std::string>& used;
    bool pin(tab::Pin& p,std::string& e){World current;if(!world(request,current,e))return false;if(!same(origin,current))return fail(e,"Network tab full NativeOpen owner changed");return step(current,image,e)&&tab::inspect(memory,image,reinterpret_cast<uintptr_t>(current.world),p,e);}
    bool reserve(std::string& e){return used.insert(request.token).second||fail(e,"Network tab token already consumed");}
    bool press(const tab::Pin& p,std::string& e){const auto code=network_fixed::press(image,p);return !code||fail(e,"Network tab original Press exception="+std::to_string(code));}
    bool selected(const tab::Pin& p,std::string& e){return tab::selected(memory,p,e);}
    bool unchanged(std::string& e){action_session::Ledger after;if(!ledger(after,e))return false;return after==before||fail(e,"Network tab full ledger changed");}
};
}
bool network_tab(const Request& request,nlohmann::json& receipt,std::string& e){
    namespace tab=tutorial_network_tab;
    static std::set<std::string> used;static bool unresolved=false;
    tab::Receipt result;receipt={{"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false}};e.clear();
    auto publish=[&]{receipt["attempted"]=result.attempted;receipt["returned"]=result.returned;receipt["confirmed"]=result.confirmed;receipt["unknown"]=result.unknown;receipt["cause"]=e;if(result.unknown)unresolved=true;};
    try{
        if(request.index!=56||request.token.empty()||request.token.size()>64||!request.epoch||!request.generation)return fail(e,"Network tab requires bounded token and exact origin/index56");
        if(unresolved||used.contains(request.token)||used.size()>=64)return fail(e,"Network tab prior Unknown/used token/capacity blocks replay");
        uintptr_t image=0;uint32_t thread=0;std::string hash;
        if(!native::read_resolver_receipt(image,thread,hash,e))return false;
        if(thread!=GetCurrentThreadId()||hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"Network tab actual Mainthread/executable differs");
        std::array<unsigned char,tab::press_code.size()> actual{};
        if(!bytes(image+0x57c950,actual.data(),actual.size(),e))return false;
        if(actual!=tab::press_code)return fail(e,"Network tab original Press body differs");
        nlohmann::json observation;if(!tutorial_observer::read(observation,e))return false;
        World current;if(!world(request,current,e))return false;
        network_fixed::Calls calls{request,current,image,{}, {},used};if(!network_fixed::ledger(calls.before,e))return false;
        const bool ok=tab::activate(calls,result,e);publish();if(!ok)return false;
        tab::Pin after;if(!calls.pin(after,e)||!calls.selected(after,e)){result.confirmed=false;result.unknown=true;publish();return false;}
        receipt["token"]=request.token;receipt["tab"]="Network";receipt["tab_active"]=true;receipt["view_mode_key"]="Network";receipt["native_view_mode"]=after.view;receipt["ledger_unchanged"]=true;
        receipt["progression"]="await_normal_native_update";receipt["before"]=std::move(observation);return true;
    }catch(const std::exception& exception){result.confirmed=false;result.unknown=result.attempted;e=std::string("Network tab exception: ")+exception.what();publish();return false;}
    catch(...){result.confirmed=false;result.unknown=result.attempted;e="Network tab unknown exception";publish();return false;}
}
}
