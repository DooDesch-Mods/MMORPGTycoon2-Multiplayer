#include "loan_gate.h"
#include "native_guards.h"
#include "native_open_runtime.h"
#include "typed_action_runtime.h"
#include "session_provider.h"
#include "live_host_state/economic_fence.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/config.h"
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include <set>
namespace mt2mp::game::loan_diagnostics {
namespace {
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){if(!p||p>UINTPTR_MAX-offset)return fail(e,"loan native address extent invalid");return memory.copy(p+offset,&out,sizeof out,e);}
bool ap(uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){uintptr_t got=0;return at(p,0,got,e)&&(got==image+expected||fail(e,"loan actual native class differs expected="+std::to_string(expected)));}
bool body(uintptr_t image,const guards::Body& g,std::string& e){std::vector<uint8_t> got(g.size);return memory.copy(image+g.rva,got.data(),got.size(),e)&&(!std::memcmp(got.data(),g.bytes,g.size)||fail(e,"loan full original callable differs rva="+std::to_string(g.rva)));}
bool text(uintptr_t p,size_t offset,size_t limit,std::string& value,std::string& e){uintptr_t bytes=0;uint64_t length=0;if(!at(p,offset,bytes,e)||!at(p,offset+8,length,e))return false;if(!bytes||length>limit||bytes>UINTPTR_MAX-length)return fail(e,"loan native GNU string extent invalid");value.resize(size_t(length));if(length&&!memory.copy(bytes,value.data(),value.size(),e))return false;char end=1;if(!at(bytes,size_t(length),end,e))return false;return (!end&&value.find('\0')==std::string::npos)||fail(e,"loan native GNU string terminator differs");}
// These wrappers contain no C++ objects across the GNU native call.
uint32_t available(uintptr_t image,uintptr_t offer,bool& value){uint32_t code=0;__try {value=reinterpret_cast<bool(*)(void*)>(image+0x61c90)(reinterpret_cast<void*>(offer));}__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}return code;}
void original(Operation op,uintptr_t image,uintptr_t business,uintptr_t object,NativeResult& result){
    result.attempted=true;
    __try {
        if(op==Operation::TakeFixture)result.accepted=reinterpret_cast<bool(*)(void*,const void*)>(image+0x555a0)(reinterpret_cast<void*>(business),reinterpret_cast<void*>(object+0x18));
        else {reinterpret_cast<void(*)(void*)>(image+0x60780)(reinterpret_cast<void*>(object));result.accepted=true;}
        result.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){result.seh=GetExceptionCode();}
}
bool rows(Survey& s,bool offer,std::string& e){
    uintptr_t data=0;int32_t count=-1,capacity=-1;const size_t offset=offer?0xc0:0x90;
    if(!at(s.business,offset+8,data,e)||!at(s.business,offset+0x10,count,e)||!at(s.business,offset+0x14,capacity,e))return false;
    if(count<0||capacity<count||count>1024||capacity>1000000||(!data&&count))return fail(e,"loan actual native array bounds invalid");
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t owner=0;Loan loan;Quote quote;if(!at(data,size_t(i)*8,owner,e)||!ap(owner,s.image,offer?0x16df370:0x17438e0,e))return false;
        if(!unique.insert(owner).second)return fail(e,"loan actual native duplicate owner");
        auto& strings=offer?quote.offer.text:loan.text;for(size_t n=0;n<3;++n)if(!text(owner,0x18+n*0x20,n?2048:256,strings[n],e))return false;
        if(offer){for(size_t n=0;n<3;++n)if(!at(owner,0x78+n*4,quote.offer.amounts[n],e))return false;
            if(!at(owner,0x84,quote.eligibility,e))return false;int32_t measured=0;if(!at(s.analytics,0x44,measured,e))return false;
            const auto seh=available(s.image,owner,quote.available);if(seh)return fail(e,"loan original IsAvailable SEH="+std::to_string(seh));
            if(quote.available!=(measured>=quote.eligibility))return fail(e,"loan original IsAvailable differs from actual eligibility comparison");
            s.offers.push_back(std::move(quote));s.offer_owners.push_back(owner);
        }else {for(size_t n=0;n<4;++n)if(!at(owner,0x78+n*4,loan.amounts[n],e))return false;s.loans.push_back(std::move(loan));s.loan_owners.push_back(owner);}
    }
    return true;
}
class NativePort final:public Port {
    bool once(Survey& s,std::string& e){
        s={};std::string sha;if(!native::read_resolver_receipt(s.image,s.thread,sha,e))return false;
        if(s.thread!=GetCurrentThreadId()||s.image!=uintptr_t(GetModuleHandleW(nullptr))||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"loan exact executable/current Mainthread differs");
        for(const auto& g:guards::bodies)if(!body(s.image,g,e))return false;
        building_mixed_identity::opened_file::CurrentWorld current;if(!native_open_runtime::read_current_world(current,e))return false;
        if(!current.world||!current.state||!current.world_epoch||!current.generation)return fail(e,"loan actual current native world absent");
        s.world=uintptr_t(current.world);s.native_origin=current.uuid;s.epoch=current.world_epoch;s.generation=current.generation;s.host=Config::instance().is_host;
        native::WorldStatus activity;if(!native::read_world_status(activity,e))return false;
        if(activity.active_mode_index!=3||activity.in_game_state!=4||!activity.world_present||activity.saves_in_progress>0||activity.edit_operations!=0||activity.quitting||activity.title_transition_pending||activity.map_task_pending)return fail(e,"loan requires actual idle InGame world, no transition/save/edit");
        const auto controller=typed_action_runtime::status();if(controller.contains("error")||!controller.contains("initialized")||!controller.at("initialized").is_boolean())return fail(e,"loan actual Controller status unavailable");
        s.initialized=controller.at("initialized").get<bool>();
        if(s.initialized){for(const char* name:{"started","pending","locked"})if(!controller.contains(name)||!controller.at(name).is_boolean())return fail(e,"loan actual Controller state fields absent");
            s.started=controller.at("started").get<bool>();s.pending=controller.at("pending").get<bool>();s.locked=controller.at("locked").get<bool>();
            if(!controller.contains("live_state")||!controller.at("live_state").contains("active")||!controller.at("live_state").at("active").is_boolean()||!controller.at("live_state").contains("locked")||!controller.at("live_state").at("locked").is_boolean())return fail(e,"loan actual live state fields absent");
            s.live=controller.at("live_state").at("active").get<bool>();s.locked=s.locked||controller.at("live_state").at("locked").get<bool>();
            if(s.started){action_session::Context ctx;if(!action_session::selected_session().current(ctx,e))return false;if((ctx.role==action_session::Role::Host)!=s.host)return fail(e,"loan actual role/config differs");s.snapshot=ctx.snapshot_uuid;}
        }
        uintptr_t clock=0;if(!at(s.image,0x13df9a0,clock,e)||!ap(clock,s.image,0x17499d0,e)||!at(clock,0x58,s.scale,e)||!at(clock,0x5c,s.pause,e)||!at(s.image,0x13df220,s.business,e)||!ap(s.business,s.image,0x16d97e0,e)||!at(s.business,0x100,s.record,e)||!ap(s.record,s.image,0x1712c00,e))return false;
        int32_t version=0;if(!at(s.record,0x18,version,e)||version!=2)return fail(e,e.empty()?"loan native CurrentRecord version differs":e);
        uintptr_t history=0;int32_t count=0,capacity=0;if(!ap(s.business+0x60,s.image,0x16ec280,e)||!at(s.business,0x68,history,e)||!at(s.business,0x70,count,e)||!at(s.business,0x74,capacity,e))return false;
        if(count<=0||capacity<count||count>1000000||!history)return fail(e,"loan actual History bounds invalid");size_t membership=0;for(int32_t i=0;i<count;++i){uintptr_t p=0;if(!at(history,size_t(i)*8,p,e))return false;membership+=p==s.record;}if(membership!=1)return fail(e,"loan CurrentRecord not unique native History member");
        for(size_t i=0;i<20;++i){const auto p=s.record+0x30+i*0x48;int32_t category=-1;if(!ap(p,s.image,0x171d0d0,e)||!at(p,0x20,category,e))return false;if(category!=int32_t(i))return fail(e,"loan native Ledger line category differs");}
        // Guard the existing singleton so IsAvailable cannot initialize one during a read.
        if(!at(s.image,0x13df280,s.analytics,e)||!ap(s.analytics,s.image,0x16dd3b0,e))return false;
        native::SimulationState state;if(!native::read_simulation_state(state,e))return false;
        s.ledger={state.cash,state.clock.flat_day,int32_t(state.record_present),state.record_day,state.record_cash,state.record_income,state.record_expense,state.line_counts,state.line_income};
        return rows(s,false,e)&&rows(s,true,e);
    }
public:
    bool read(Survey& out,std::string& e)override{Survey a,b;if(!once(a,e)||!once(b,e))return false;if(a!=b)return fail(e,"loan actual native roots/values changed during double read");out=std::move(a);e.clear();return true;}
    bool invoke(Operation op,const Survey& before,size_t index,NativeResult& result,std::string& e)override{
        Survey current;if(!read(current,e))return false;if(current!=before)return fail(e,"loan actual native binding changed before original");
        auto& fence=live_host_state::economic_fence();if(fence.locked())return fail(e,fence.cause().empty()?"loan actual economic owner locked":fence.cause());
        if(op==Operation::TakeFixture){if(index>=before.offer_owners.size())return fail(e,"loan original Offer owner absent");if(!fence.can_arm(e))return false;}
        else {if(index>=before.loan_owners.size())return fail(e,"loan original Loan owner absent");
            const auto controller=typed_action_runtime::status();
            if(!controller.contains("native_economy_read_error")||controller.at("native_economy_read_error")!=""||!controller.contains("native_economy")||!controller.at("native_economy").is_object())return fail(e,"loan payment existing Economy1 actual capture unavailable");
            const auto& economy=controller.at("native_economy");json offers=json::array();for(const auto& q:before.offers)offers.push_back({{"text",q.offer.text},{"amounts",q.offer.amounts}});
            if(!economy.contains("kind")||economy.at("kind")!="host_state_economy"||!economy.contains("loans")||economy.at("loans")!=public_survey(before).at("loans")||!economy.contains("offers")||economy.at("offers")!=offers)return fail(e,"loan payment actual Economy1 owner rows differ or subscriber/loan integration absent");
            action_session::Context ctx;if(!action_session::selected_session().current(ctx,e))return false;if(ctx.snapshot_uuid!=before.snapshot||ctx.role!=action_session::Role::Host)return fail(e,"loan actual shared Host context differs");
            live_host_state::NativePort reader(ctx,{before.epoch,before.generation},&fence);live_host_state::Pin pin;if(!reader.read(pin,e)||pin.image!=before.image||pin.world!=before.world||pin.business!=before.business||pin.record!=before.record||live_host_state::ledger(pin.values)!=before.ledger||!fence.read_current_fence(pin,e))return fail(e,e.empty()?"loan actual owned GameUpdate fence/Pin differs":e);
        }
        e.clear();original(op,before.image,before.business,op==Operation::TakeFixture?before.offer_owners[index]:before.loan_owners[index],result);
        if(result.seh)return fail(e,"loan native original SEH="+std::to_string(result.seh)+"; no replay");
        if(fence.locked()){result.cause=fence.cause();return fail(e,result.cause.empty()?"loan native economic owner locked during original":result.cause);}
        if(!result.returned||!result.accepted)return fail(e,"loan original returned without accepted effect; inspect actual state, no replay");
        return true;
    }
};
Gate gate;NativePort native_port;
}
bool observe(json& out,std::string& e){return gate.read(native_port,out,e);}
bool mutate(Operation op,const json& args,json& out,std::string& e){return gate.run(native_port,op,args,out,e);}
}
