#pragma once
#include "notices.h"
#include "scenario_vip_authority/alert_lifecycle.h"

namespace mt2mp::game::marketing_authority::notice {
namespace alert=scenario_vip_authority::client_alert;
struct Receipt {
    Row row;alert::MessageCalls calls;bool confirmed=false,unknown=false;std::string cause;
};
// The shared Port performs exact type0 readback. The Manager owns handed-off
// alerts; this receipt never creates a second Hide/D0 owner.
inline bool present(alert::Port& port,const Row& row,Receipt& r,std::string& e){
    r={};r.row=row;alert::Owner owner,current;uintptr_t selector=0;
    const auto reject=[&](const std::string& cause){r.cause=cause.empty()?"Campaign message failed without cause":cause;r.unknown=r.calls.attempted();e=r.cause;return false;};
    try{
        if(!port.owner(nullptr,false,owner,e))return reject(e);
        const auto discard=[&](const std::string& cause){
            std::string cleanup;
            if(r.calls.create.returned&&r.calls.create.result&&!r.calls.fire.attempted){
                if(!port.owner(&owner,false,current,cleanup)||!port.discard_message(current,r.calls.create.result,r.calls.discard,cleanup)||
                   !r.calls.discard.attempted||!r.calls.discard.returned||!port.owner(&owner,false,current,cleanup)||!port.absent(current,r.calls.create.result,0,cleanup))
                    return reject(cause+"; unhanded cleanup unconfirmed: "+cleanup);
                r.calls.create.result=0;
            }
            return reject(cause);
        };
        if(!port.create_message(owner,row.message,r.calls,e))return discard(e);
        if(!r.calls.create.attempted||!r.calls.create.returned||!r.calls.create.result)return discard("Campaign factory lacks returned receiver");
        if(!port.owner(&owner,false,current,e)||!port.inspect_message(current,r.calls.create.result,row.message,0,selector,e))return discard(e);
        r.calls.verified=true;
        if(!port.fire_message(current,r.calls.create.result,r.calls.fire,e))return discard(e);
        if(!r.calls.fire.attempted||!r.calls.fire.returned)return discard("Campaign FireAndForget lacks returned receipt");
        if(!port.owner(&owner,false,current,e)||!port.inspect_message(current,r.calls.create.result,row.message,1,selector,e))return reject(e);
        r.calls.handed_off=true;r.calls.create.result=0;r.confirmed=true;e.clear();return true;
    }catch(const std::exception& x){return reject(std::string("Campaign message exception: ")+x.what());}
    catch(...){return reject("Campaign message nonstandard exception");}
}
inline Json encode(const Receipt& r){
    const auto call=[](const alert::Call& c){return Json{{"attempted",c.attempted},{"returned",c.returned},{"seh",c.exception}};};
    return {{"sequence",r.row.sequence},{"message",r.row.message},{"confirmed",r.confirmed},{"unknown",r.unknown},{"cause",r.cause},
        {"verified",r.calls.verified},{"handed_off",r.calls.handed_off},{"create",call(r.calls.create)},{"fire",call(r.calls.fire)},
        {"discard",call(r.calls.discard)},{"loc_construct",call(r.calls.loc_construct)},
        {"loc_arguments_destroy",call(r.calls.loc_arguments_destroy)},{"loc_string_destroy",call(r.calls.loc_string_destroy)}};
}
}
