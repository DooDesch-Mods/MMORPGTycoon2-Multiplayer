#include "mt2mp/net/action_order.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::net;
static void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() {
    try {
        ActionOrder<int> ledger; int previous=0; std::string error;
        require(ledger.claim(2,1,"one","original",previous,error)==ActionClaim::Fresh,"first request rejected");
        ledger.confirm(2,1,"one","original",42);
        require(ledger.claim(2,1,"one","original",previous,error)==ActionClaim::Cached&&previous==42,"matching duplicate does not return original result");
        require(ledger.claim(2,1,"one","changed",previous,error)==ActionClaim::Rejected,"conflicting duplicate accepted");
        require(ledger.claim(2,1,"other","original",previous,error)==ActionClaim::Rejected,"reordered request accepted");
        require(ledger.claim(3,1,"one","original",previous,error)==ActionClaim::Fresh,"peers share sequence state");
        require(ledger.claim(2,2,"reserved","original",previous,error)==ActionClaim::Fresh,"next request rejected");
        require(ledger.claim(2,2,"reserved","original",previous,error)==ActionClaim::Rejected,"unconfirmed operation re-executed");
        for(uint64_t i=3;i<70;++i) {
            auto id=std::to_string(i);
            require(ledger.claim(2,i,id,"payload",previous,error)==ActionClaim::Fresh,"new request rejected");
            ledger.confirm(2,i,id,"payload",int(i));
        }
        require(ledger.claim(2,1,"one","original",previous,error)==ActionClaim::Rejected,"evicted request re-executed");
        require(ledger.claim(0,100,"invalid","payload",previous,error)==ActionClaim::Rejected,"invalid peer accepted");
        require(ledger.claim(4,0,"invalid","payload",previous,error)==ActionClaim::Rejected,"zero sequence accepted");
        ledger.reset();
        require(ledger.claim(2,1,"one","original",previous,error)==ActionClaim::Fresh,"world reset retains old replay state");
        std::cout<<"action order checks passed\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"action order failure: "<<error.what()<<'\n'; return 1; }
}
