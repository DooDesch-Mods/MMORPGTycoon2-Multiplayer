#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::live_host_state::network::activation {
struct Owner {
    uintptr_t image=0,node=0,region=0,manager=0,definition=0;
    uint64_t uid=0;
    uint32_t thread=0;
};
struct Receipt {
    bool attempted=false,flag_written=false,constructed=false,send_attempted=false,sent=false;
    bool cleanup_attempted=false,cleaned=false,confirmed=false,unknown=false;
    uint8_t dispatch_enabled=0;
    uint32_t seh_code=0;
    std::string cause;
};
// Caller owns the exact Client/Clock0/snapshot pin and complete graph preflight.
bool preflight(const Owner&,bool expected,bool target,std::string&);
Receipt invoke(const Owner&,bool expected,bool target);

namespace detail {
struct Calls {
    virtual ~Calls()=default;
    virtual bool guard(std::string&)=0;
    virtual bool mark(Receipt&)=0;
    virtual bool construct(Receipt&)=0;
    virtual bool send(Receipt&)=0;
    virtual bool cleanup(Receipt&)=0;
    virtual bool verify(Receipt&)=0;
};
inline Receipt sequence(Calls& calls,bool expected,bool target){
    Receipt r;
    if(expected&&!target){r.cause="network Activated cannot transition 1 to 0";return r;}
    if(!calls.guard(r.cause))return r;
    if(expected==target){r.confirmed=true;return r;}
    r.attempted=true;
    if(!calls.mark(r)){r.unknown=true;return r;}r.flag_written=true;
    if(!calls.construct(r)){r.unknown=true;return r;}r.constructed=true;
    r.send_attempted=true;const bool sent=calls.send(r);r.sent=sent;
    const auto send_cause=r.cause;
    r.cleanup_attempted=true;const bool cleaned=calls.cleanup(r);r.cleaned=cleaned;
    if(!sent){if(!cleaned)r.cause=send_cause+"; cleanup original="+r.cause;else r.cause=send_cause;}
    if(!sent||!cleaned){r.unknown=true;return r;}
    if(!calls.verify(r)){r.unknown=true;return r;}
    r.confirmed=true;return r;
}
}
}
