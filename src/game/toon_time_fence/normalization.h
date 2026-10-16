#pragma once
#include <bit>
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>

namespace mt2mp::game::toon_time_fence {
struct Receipt {
    bool owned=false,forward=false,cache_attempted=false,cache_cleared=false;
    bool original_attempted=false,original_returned=false,confirmed=false,unknown=false;
    int32_t subscriber_id=-1;
    uint32_t input_bits=0,backlog_bits=0,after_bits=0;
    std::string cause;
};
// The original Toon entry adds its culling backlog to dt before movement and
// other simulation branches. Normalize that time source, then retain the
// original distance, marker, actor and zero-time presentation path.
template<class Calls> bool normalize(Calls& calls,uintptr_t toon,float dt,Receipt& receipt) {
    receipt={};receipt.input_bits=std::bit_cast<uint32_t>(dt);
    std::string error;
    const auto failed=[&]{
        receipt.unknown=receipt.cache_attempted||receipt.original_attempted;
        receipt.cause=error.empty()?"Toon time fence failed without cause":error;
        return false;
    };
    try {
        typename Calls::Owner before,after;
        if(!calls.pin(toon,before,receipt.owned,error))return failed();
        if(!receipt.owned){receipt.forward=true;return true;}
        receipt.subscriber_id=before.subscriber_id;
        if(!std::isfinite(dt)||dt<0){error="Toon time incoming dt is not finite and nonnegative";return failed();}
        if(!calls.read_backlog(toon,receipt.backlog_bits,error))return failed();
        const auto backlog=std::bit_cast<float>(receipt.backlog_bits);
        if(!std::isfinite(backlog)||backlog<0){error="Toon culling backlog is not finite and nonnegative";return failed();}
        bool still_owned=false;
        if(!calls.pin(toon,after,still_owned,error))return failed();
        if(!still_owned||after!=before){error="Toon time owner changed before cache normalization";return failed();}
        if(receipt.backlog_bits!=0){
            receipt.cache_attempted=true;
            if(!calls.clear_backlog(toon,receipt.backlog_bits,error))return failed();
            receipt.cache_cleared=true;
        }
        if(!calls.read_backlog(toon,receipt.after_bits,error))return failed();
        if(receipt.after_bits!=0){error="Toon time cache is not zero before original entry";return failed();}
        receipt.original_attempted=true;
        if(!calls.original(toon,0.0f,error))return failed();
        receipt.original_returned=true;
        if(!calls.read_backlog(toon,receipt.after_bits,error))return failed();
        if(receipt.after_bits!=0){error="Toon original zero-time entry left a nonzero culling backlog";return failed();}
        if(!calls.pin(toon,after,still_owned,error))return failed();
        if(!still_owned||after!=before){error="Toon time owner changed after original entry";return failed();}
        receipt.confirmed=true;return true;
    }catch(const std::exception& exception){error=std::string("Toon time boundary exception: ")+exception.what();return failed();}
    catch(...){error="Toon time boundary nonstandard exception";return failed();}
}
}
