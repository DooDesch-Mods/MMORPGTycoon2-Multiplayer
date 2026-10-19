#pragma once
#include "control_host_step.h"
#include <array>

namespace mt2mp::control_input {
enum class Operation:uint8_t {Acquire=1,Input=2,Release=3,Activate=4,Deactivate=5};
struct Intent {
    Operation operation=Operation::Acquire;
    std::string lease;
    Target actor;
    uint64_t input_sequence{},jump_sequence{};
    BasisInput input;
    std::optional<Target> selected;
    int32_t ability=-1;
    std::string prototype;
    bool operator==(const Intent&)const=default;
};
struct Lease {
    uint64_t peer{},input_sequence{},jump_sequence{},consumed_jump{},sample_time{},step_frame{};
    std::string id;
    Target actor;
    BasisInput input;
    std::optional<Target> selected;
    Motion motion;
    bool expired{};
};
inline bool valid(const Intent& intent){
    if(!game::monster_target_key::uuid(intent.lease)||!game::interventions::valid(intent.actor)||!valid(intent.input)||
       (intent.selected&&!game::interventions::valid(*intent.selected)))return false;
    const bool ability=intent.operation==Operation::Activate||intent.operation==Operation::Deactivate;
    if(ability){if(intent.ability<0||intent.ability>=4096||intent.prototype.empty()||intent.prototype.size()>128||intent.prototype.find('\0')!=std::string::npos)return false;}
    else if(intent.ability!=-1||!intent.prototype.empty())return false;
    if(intent.operation==Operation::Input)return intent.input_sequence>0&&!intent.input.jump_pressed;
    return (intent.operation==Operation::Acquire||intent.operation==Operation::Release||ability)&&
        !intent.input_sequence&&!intent.jump_sequence&&intent.input==BasisInput{}&&!intent.selected;
}

// This state is subordinate to the existing ordered Controller. It owns no
// transport, retry or acknowledgment path. The caller passes the authenticated
// Request peer and action UUID only after the existing Controller admission.
class Leases {
    std::array<std::optional<Lease>,4> leases_;
    bool locked_{};
    std::string cause_;
    static bool fail(std::string& error,const std::string& cause){error=cause;return false;}
public:
    static constexpr uint64_t input_timeout_ms=1000;
    const auto& values()const{return leases_;}
    bool locked()const{return locked_;}
    const std::string& cause()const{return cause_;}
    Lease* find(uint64_t peer){return peer>=1&&peer<=4&&leases_[peer-1]?&*leases_[peer-1]:nullptr;}
    const Lease* find(uint64_t peer)const{return peer>=1&&peer<=4&&leases_[peer-1]?&*leases_[peer-1]:nullptr;}
    bool preflight(uint64_t peer,const std::string& action,const Intent& intent,std::string& error)const{
        if(locked_)return fail(error,"Control state locked after Unknown: "+cause_);
        if(peer<1||peer>4||!valid(intent))return fail(error,"Control ordered peer/intent is invalid");
        const auto* lease=find(peer);
        if(intent.operation==Operation::Acquire){
            if(intent.lease!=action||lease)return fail(error,"Control acquire requires its new action UUID and no current peer lease");
            for(const auto& current:leases_)if(current&&current->actor==intent.actor)return fail(error,"Control actor already has an owner");
        }else{
            if(!lease||lease->id!=intent.lease||lease->actor!=intent.actor)return fail(error,"Control input/release does not name the peer's current lease");
            if(intent.operation==Operation::Input){
                if(intent.input_sequence<=lease->input_sequence||intent.jump_sequence<lease->jump_sequence||intent.jump_sequence-lease->jump_sequence>1)return fail(error,"Control input sequence is stale or jump edge sequence skips");
            }
        }
        error.clear();return true;
    }
    // Commit only after original operation and its complete native readback.
    bool commit(uint64_t peer,const std::string& action,const Intent& intent,uint64_t now,std::string& error){
        if(!preflight(peer,action,intent,error))return false;
        if(intent.operation==Operation::Acquire){Lease value;value.peer=peer;value.id=intent.lease;value.actor=intent.actor;value.sample_time=now;leases_[peer-1]=std::move(value);}
        else if(intent.operation==Operation::Release)leases_[peer-1].reset();
        else if(intent.operation==Operation::Input){auto& lease=*leases_[peer-1];lease.input_sequence=intent.input_sequence;lease.jump_sequence=intent.jump_sequence;lease.input=intent.input;lease.selected=intent.selected;lease.sample_time=now;lease.expired=false;}
        return true;
    }
    void unknown(const StepReceipt& receipt){locked_=true;cause_=receipt.stage+": "+receipt.cause;}
    template<class Port>
    bool step(Port& port,uint64_t peer,uint64_t frame,uint64_t now,float dt,StepReceipt& receipt,std::string& error){
        receipt={};auto* lease=find(peer);
        if(locked_||!lease)return fail(error,locked_?"Control state locked after Unknown: "+cause_:"Control current lease absent");
        if(!frame||frame<=lease->step_frame)return fail(error,"Control native frame already stepped or out of order");
        if(now<lease->sample_time)return fail(error,"Control monotonic input clock moved backwards");
        auto input=lease->input;
        const bool stale=now-lease->sample_time>=input_timeout_ms;
        if(stale){input.axis1=0;input.negative_axis0=0;input.jump_pressed=false;}
        else input.jump_pressed=lease->jump_sequence>lease->consumed_jump;
        Motion proposed=lease->motion;
        if(!step_host(port,lease->actor,proposed,input,lease->selected,dt,receipt,error)){
            if(receipt.unknown)unknown(receipt);return false;
        }
        lease->motion=proposed;lease->step_frame=frame;lease->expired=stale;
        // A jump seen during paused time is consumed at this frame, matching
        // the native WasPressed edge rather than becoming a future held key.
        lease->consumed_jump=lease->jump_sequence;
        return true;
    }
    // The caller must first execute original release for each departed peer.
    bool retire_disconnected(uint64_t peer,const StepReceipt& receipt,std::string& error){
        if(!find(peer))return fail(error,"Control disconnect lease absent");
        if(!receipt.confirmed||receipt.unknown){unknown(receipt);return fail(error,"Control disconnect release is not confirmed: "+receipt.cause);}
        leases_[peer-1].reset();error.clear();return true;
    }
};
}
