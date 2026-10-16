#include "parent_visual.h"
#include <cmath>
namespace mt2mp::game::bug_authority {
namespace {bool fail(std::string& e,const char* cause){e=cause;return false;}}
bool read_parent_visual(const JobRead& read,const ParentOwner& owner,ParentVisual& out,std::string& e){
    out={};out.kind=owner.kind;out.key=owner.key;uintptr_t shard=0;uint8_t flag=0;
    if(!owner.object||owner.object>UINTPTR_MAX-0xbe0||!owner.shard)return fail(e,"Bug-work parent owner/address absent");
    if(!read(owner.object+0x170,&shard,8,e)||shard!=owner.shard)return fail(e,e.empty()?"Bug-work parent root shard differs":e.c_str());
    const bool building=owner.kind==ParentKind::Building;
    if(!read(owner.object+(building?0x7a8:0x838),&out.state,4,e))return false;
    if(building){if(!read(owner.object+0x988,&out.bandwidth,4,e)||!read(owner.object+0xbd0,&flag,1,e))return false;out.user_set=flag!=0;}
    else{if(!read(owner.object+0x6c0,&flag,1,e)||!read(owner.object+0x6b0,out.tint.data(),16,e))return false;out.tint_override=flag!=0;}
    if(out.state<0||out.state>2||flag>1||!std::isfinite(out.bandwidth)||out.bandwidth<0)return fail(e,"Bug-work parent state/flag/bandwidth invalid");
    for(float value:out.tint)if(!std::isfinite(value))return fail(e,"Bug-work parent tint is not finite");
    return true;
}
bool apply_parent_visual(ParentServices& services,const ParentOwner& owner,const ParentVisual& target,ParentReceipt& receipt){
    if(receipt.attempted||receipt.confirmed){receipt.cause="Bug-work parent receipt already consumed; no replay";return false;}
    auto& e=receipt.cause;ParentVisual before;
    if(target.kind!=owner.kind||target.key!=owner.key||target.state<0||target.state>2)return fail(e,"Bug-work parent target identity/state differs");
    if(!services.authorize(owner,e)||!services.observe(owner,before,e))return false;
    if(before==target){receipt.confirmed=true;return true;}
    struct CallAttempt {bool& total;const JobCall& call;~CallAttempt(){total|=call.attempted;}};
    struct DetailAttempt {bool& total;const bool& call;~DetailAttempt(){total|=call;}};
    try{
        if(before.state!=target.state){JobCall call;CallAttempt retain{receipt.attempted,call};
            if(!services.authorize(owner,e))return false;
            const bool ok=services.state(owner,target.state,call,e);receipt.attempted|=call.attempted;
            if(!ok||!call.returned||!call.accepted||!call.record_confirmed||!call.cleanup_confirmed||call.exception||call.cleanup_exception)
                throw std::runtime_error(e.empty()?"Bug-work parent original enum Load unconfirmed":e);
        }
        bool attempted=false;DetailAttempt retain_detail{receipt.attempted,attempted};const bool detail=services.detail(owner,before,target,attempted,e);receipt.attempted|=attempted;
        if(!detail)throw std::runtime_error(e);
        JobCall refresh;CallAttempt retain_refresh{receipt.attempted,refresh};if(!services.authorize(owner,e))throw std::runtime_error(e);
        const bool ok=services.refresh(owner,refresh,e);receipt.attempted|=refresh.attempted;
        if(!ok||!refresh.returned||refresh.exception)throw std::runtime_error(e.empty()?"Bug-work parent original visual refresh unconfirmed":e);
        ParentVisual after;if(!services.authorize(owner,e)||!services.observe(owner,after,e)||after!=target)
            throw std::runtime_error(e.empty()?"Bug-work parent exact final state/tint/bandwidth differs":e);
        receipt.confirmed=true;return true;
    }catch(const std::exception& cause){if(e.empty())e=cause.what();}
    catch(...){if(e.empty())e="Bug-work parent native apply nonstandard exception";}
    receipt.unknown=receipt.attempted;return false;
}
}
