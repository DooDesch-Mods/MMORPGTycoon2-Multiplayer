#pragma once
#include "control_motion.h"
#include "../intervention_state.h"
#include <optional>
#include <string>

namespace mt2mp::control_input {
using Target=game::interventions::Target;
struct ActorState {
    uintptr_t pointer{};uint64_t uid{};
    Sample sample;
    bool controlled{},moving{},dead{},on_map{};
    std::optional<Target> target;
};
struct StepReceipt {
    bool attempted{},confirmed{},unknown{};
    uint32_t exception{};
    std::string stage,cause;
};

// Port operations validate the current canonical owner before each native
// call. An actor's Region may change through its original transform callback.
template<class Port>
bool step_host(Port& port,const Target& actor,Motion& motion,const BasisInput& input,
               const std::optional<Target>& target,float frame_dt,StepReceipt& receipt,std::string& error) {
    receipt={};
    const auto failed=[&](const char* stage){receipt.stage=stage;receipt.cause=error;receipt.unknown=receipt.attempted;return false;};
    if(!valid(input)||!std::isfinite(frame_dt)||frame_dt<0.0f){error="Control input or actual native frame delta is invalid";return failed("preflight");}
    ActorState initial;float scaled_dt{};
    if(!port.read(actor,initial,error)||!port.scaled_dt(frame_dt,scaled_dt,error))return failed("preflight");
    if(!initial.controlled||initial.dead||!initial.on_map){error="Control lease actor is not live, controlled and on-map";return failed("preflight");}
    if(target&&!port.target_exists(*target,error))return failed("target-preflight");
    Proposal proposal;
    if(!port.propose(initial.sample,motion,input,scaled_dt,proposal,receipt.exception,error))return failed("proposal");
    const auto current=[&](){ActorState value;if(!port.read(actor,value,error))return false;
        if(value.pointer!=initial.pointer||value.uid!=initial.uid||!value.controlled||value.dead||!value.on_map){error="Control actor identity/liveness changed during native step";return false;}return true;};
    if(proposal.advance){
        if(proposal.jump){receipt.attempted=true;if(!port.jump(actor,receipt.exception,error)||!current())return failed("jump");}
        float height{};
        if(!port.constrain(actor,proposal.position,height,receipt.exception,error)||!ground(proposal,height)){if(error.empty())error="Control native NavMesh/height returned nonfinite position";return failed("collision");}
        if(!current())return failed("position-preflight");
        receipt.attempted=true;
        if(!port.position(actor,proposal.position,receipt.exception,error)||!current())return failed("position");
    }
    receipt.attempted=true;
    if(!port.facing(actor,proposal.next,receipt.exception,error)||!current())return failed("facing");
    if(!port.set_target(actor,target,receipt.exception,error)||!current())return failed("target");
    ActorState actual;if(!port.read(actor,actual,error))return failed("readback");
    if(actual.sample.position!=proposal.position||actual.moving!=moving(proposal.next)||actual.target!=target){error="Control native pose/moving/live-target readback differs";return failed("readback");}
    motion=proposal.next;receipt.confirmed=true;error.clear();return true;
}
}
