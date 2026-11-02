#pragma once
#include "original_go_scope.h"
#include "mt2mp/core/game_update_pair.h"
#include "native_steps_draft.h"
#include <algorithm>
#include <limits>

namespace mt2mp::action_session::tick_commands {
using RunOriginalGo=std::function<Outcome(const OriginalGoBoundary&,const OriginalGoScope::Read&,
                                        const OriginalGoScope::Next&,std::string&)>;
// Call synchronously from Queue Advance inside the current borrowed boundary.
// The existing Runtime runner owns Controller, Session and native-world proofs.
inline Outcome framework_step(const MT2FW_MT2_StepServiceDraftV1& service,
    const MT2FW_MT2_StepBoundary& boundary,MT2FW_MT2_StepNext next,void* continuation,
    const Context& context,const RunOriginalGo& run,std::string& error) {
    const auto role=context.role==Role::Host?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
    if(service.size!=sizeof(service)||service.version!=MT2FW_MT2_STEP_DRAFT_VERSION||!service.read||
       !next||!continuation||!run||boundary.size!=sizeof(boundary)||boundary.reserved||
       boundary.dt_bits!=MT2FW_MT2_STEP_FIXED_DT_BITS||boundary.actual_role!=uint32_t(role)||
       !boundary.thread||boundary.thread>std::numeric_limits<uint32_t>::max()||
       !boundary.lease||!boundary.tick||!boundary.invocation||!boundary.world_epoch||boundary.session!=context.session){
        error="Framework step API or actual borrowed boundary identity differs";return Outcome::RejectedNoEffect;
    }
    const OriginalGoBoundary expected{boundary.lease,boundary.session,boundary.world_epoch,
        boundary.tick,boundary.invocation,uint32_t(boundary.thread)};
    const auto failure=[&](const char* operation,MT2FW_MT2_Result result,const MT2FW_MT2_StepReceipt& receipt){
        const auto end=std::find(std::begin(receipt.cause),std::end(receipt.cause),'\0');
        error=std::string(operation)+" result="+std::to_string(result);
        if(end!=std::begin(receipt.cause))error+=" cause="+std::string(std::begin(receipt.cause),end);
        else error+=" without original cause";
    };
    const auto identity=[&](const MT2FW_MT2_StepReceipt& receipt,std::string& cause){
        if(receipt.size!=sizeof(receipt)||receipt.actual_role!=boundary.actual_role||
           receipt.lease!=boundary.lease||receipt.session!=boundary.session||receipt.world_epoch!=boundary.world_epoch||
           receipt.tick!=boundary.tick||receipt.invocation!=boundary.invocation||receipt.thread!=boundary.thread||
           receipt.dt_bits!=boundary.dt_bits||receipt.callbacks_inflight!=1){
            cause="Framework step receipt changed owner, invocation, thread, or fixed delta";return false;
        }
        return true;
    };
    bool attempted=false,returned=false;
    const auto read=[&](OriginalGoObservation& out,std::string& cause){
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service.read(service.context,boundary.lease,&receipt);
        if(result!=MT2FW_MT2_OK){failure("Framework step read",result,receipt);cause=error;return false;}
        if(!identity(receipt,cause))return false;
        bool running=false;
        if(!attempted){
            if(receipt.phase!=MT2FW_MT2_STEP_HELD||receipt.outcome!=MT2FW_MT2_STEP_NOT_ATTEMPTED||
               receipt.original_attempted||receipt.original_returned||receipt.timer_calls||receipt.workers_held!=1){
                cause="Framework step has no untouched held command boundary";return false;
            }
        }else if(!returned){
            if(receipt.phase!=MT2FW_MT2_STEP_ENTERED||receipt.original_attempted!=1||receipt.original_returned!=0){
                cause="Framework step read is outside the actual in-flight original Go";return false;
            }
            running=true;
        }else if(receipt.phase!=MT2FW_MT2_STEP_ENTERED||receipt.outcome!=MT2FW_MT2_STEP_CONFIRMED||
                  receipt.original_attempted!=1||receipt.original_returned!=1||receipt.timer_calls!=1||receipt.workers_held!=1){
            cause="Framework step lacks returned original, Timer, and worker-drain proof";return false;
        }
        out.boundary=expected;out.original_running=running;return true;
    };
    const auto advance=[&](std::string& cause){
        if(attempted){cause="Framework borrowed continuation was already attempted; no replay";return Outcome::Unknown;}
        attempted=true;
        MT2FW_MT2_StepReceipt receipt{};receipt.size=sizeof(receipt);
        // This is continuation wall time, including the provider's original Step and worker drain.
        MT2FW_MT2_Result result;
        {game_update_pair::Scope traced(game_update_pair::Section::OrderedNativeStep);result=next(continuation,boundary.invocation,&receipt);}
        returned=true;
        if(result!=MT2FW_MT2_OK){failure("Framework step continuation",result,receipt);cause=error;return Outcome::Unknown;}
        if(!identity(receipt,cause))return Outcome::Unknown;
        if(receipt.phase!=MT2FW_MT2_STEP_ENTERED||receipt.outcome!=MT2FW_MT2_STEP_CONFIRMED||
           receipt.original_attempted!=1||receipt.original_returned!=1||receipt.timer_calls!=1||receipt.workers_held!=1){
            cause="Framework continuation returned without complete original/Timer/drain proof";return Outcome::Unknown;
        }
        return Outcome::Confirmed;
    };
    const auto result=run(expected,read,advance,error);
    if(result==Outcome::Confirmed&&(!attempted||!returned)){
        error="Framework scope reported success without its original continuation";return Outcome::Unknown;
    }
    if(attempted&&result==Outcome::RejectedNoEffect){
        if(error.empty())error="Framework scope rejected after an original attempt";return Outcome::Unknown;
    }
    return result;
}
}
