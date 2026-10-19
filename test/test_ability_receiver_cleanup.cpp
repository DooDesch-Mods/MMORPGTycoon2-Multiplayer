#include "ability_receiver_cleanup.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace cleanup=mt2mp::game::ability_receiver_cleanup;
struct Port {
    uintptr_t docked=0;int fail=0,calls=0;std::vector<std::string> order;
    bool step(const std::string& name,std::string& error){order.push_back(name);++calls;if(calls==fail){error="original native "+name+" failure";return false;}return true;}
    bool slot_owner(uintptr_t& out,std::string& error){if(!step("owner",error))return false;out=docked;return true;}
    bool destroy_slot(std::string& error){return step("slot",error);}
    bool destroy_icon(uintptr_t,bool constructed,std::string& error){return step(constructed?"icon":"raw_icon",error);}
    bool destroy_source(uintptr_t,bool constructed,std::string& error){return step(constructed?"source":"raw_source",error);}
    bool destroy_record(std::string& error){return step("record",error);}
};
int main()try{
    unsigned checks=0;const auto check=[&](bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);};
    for(bool slot:{false,true})for(bool docked:{false,true})for(bool constructed:{false,true}){
        cleanup::State initial;initial.slot_constructed=slot;initial.icon=100;initial.source=200;
        initial.icon_constructed=initial.source_constructed=constructed;initial.record_constructed=true;
        Port expected;expected.docked=docked&&slot?100:0;auto good=initial;cleanup::Receipt receipt;std::string error;
        check(cleanup::finish(expected,good,receipt,error),error);check(good.cleanup_confirmed&&!good.icon&&!good.source&&!good.record_constructed&&!good.slot_constructed,"Private cleanup did not close its exact original ownership");
        check(receipt.icon_deleted_by_slot==(docked&&slot),"Original Slot-owned Icon delete was not retained");
        check(receipt.icon_returned==!(docked&&slot),"Icon was deleted both through Slot and independently");
        const auto count=expected.calls;check(!cleanup::finish(expected,good,receipt,error)&&expected.calls==count,"Completed cleanup was replayed");
        for(int failure=1;failure<=count;++failure){Port failing;failing.docked=docked&&slot?100:0;failing.fail=failure;
            auto state=initial;cleanup::Receipt failed;error.clear();
            check(!cleanup::finish(failing,state,failed,error)&&!state.cleanup_confirmed&&state.cleanup_attempted,"Native cleanup failure became confirmed success");
            check(error.starts_with("original native ")&&failed.cause==error,"Original private destruction cause was replaced");
            const auto attempted=failing.calls;check(!cleanup::finish(failing,state,failed,error)&&failing.calls==attempted,"Partly completed native destruction was retried");}}
    cleanup::State foreign;foreign.slot_constructed=true;foreign.icon=100;Port port;port.docked=999;cleanup::Receipt receipt;std::string error;
    check(!cleanup::finish(port,foreign,receipt,error)&&port.order==std::vector<std::string>{"owner"},"Private cleanup destroyed a foreign original Icon");
    check(error.find("foreign Icon")!=std::string::npos,"Foreign private receiver failure has no cause");
    cleanup::State empty;Port none;cleanup::Receipt no_effect;
    check(cleanup::finish(none,empty,no_effect,error)&&none.calls==0&&empty.cleanup_confirmed,"Never-constructed receivers caused native destruction");
    std::cout<<checks<<" actual private Ability receiver ownership, original Slot deletion, partial failure and no-retry comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
