#pragma once
#include <cstdint>

namespace mt2mp::game::wall_private_native {
struct Receipt {
    bool prepared=false,purchase_attempted=false,purchase_returned=false,purchase_allowed=false;
    bool finish_attempted=false,finish_returned=false,cleanup_attempted=false,cleanup_returned=false;
    uintptr_t receiver=0,ghost=0,first_model=0,last_model=0,scene=0,shard=0,scratch_splines=0;
    uint32_t cause=0,cleanup_cause=0;
};
inline constexpr uint32_t purchase_denied=1223;

// Once an original call fails, its private objects remain available in the
// receipt. In particular a failed Finish may already have transferred the Path.
template<class Native> uint32_t execute(Native& native,Receipt& receipt){
    receipt.cause=native.prepare(receipt);
    if(receipt.cause)return receipt.cause;
    receipt.prepared=true;
    receipt.purchase_attempted=true;
    receipt.cause=native.purchase(receipt.purchase_allowed);
    if(receipt.cause)return receipt.cause;
    receipt.purchase_returned=true;
    if(receipt.purchase_allowed){
        receipt.finish_attempted=true;
        receipt.cause=native.finish();
        if(receipt.cause)return receipt.cause;
        receipt.finish_returned=true;
    }else receipt.cause=purchase_denied;
    receipt.cleanup_attempted=true;
    receipt.cleanup_cause=native.cleanup(receipt);
    receipt.cleanup_returned=receipt.cleanup_cause==0;
    return receipt.cause?receipt.cause:receipt.cleanup_cause;
}
}
