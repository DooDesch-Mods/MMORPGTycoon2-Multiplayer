#pragma once
#include "control_host_step.h"
#include "control_native_calls.h"
#include "../typed_action/action_session.h"
#include "../placement_ui_probe/ui_probe.h"

namespace mt2mp::control_input {
class NativePort {
    action_session::Context context_;
    action_session::Binding binding_;
    uintptr_t image_{};
    bool replica_{};
    ::placement_ui_probe::ProcessMemory memory_;
    bool resolve(const Target&,LocalBinding&,uint64_t&,std::string&);
    bool code(std::string&);
    bool checked(uint32_t,const char*,std::string&);
public:
    NativePort(const action_session::Context& context,const action_session::Binding& binding,bool replica=false):context_(context),binding_(binding),replica_(replica){}
    bool read(const Target&,ActorState&,std::string&);
    bool target_exists(const Target&,std::string&);
    bool scaled_dt(float,float&,std::string&);
    bool propose(const Sample&,const Motion&,const BasisInput&,float,Proposal&,uint32_t&,std::string&);
    bool jump(const Target&,uint32_t&,std::string&);
    bool constrain(const Target&,Vec3&,float&,uint32_t&,std::string&);
    bool position(const Target&,const Vec3&,uint32_t&,std::string&);
    bool facing(const Target&,const Motion&,uint32_t&,std::string&);
    bool set_target(const Target&,const std::optional<Target>&,uint32_t&,std::string&);
    bool controlled(const Target&,bool,StepReceipt&,std::string&);
    bool replica_flags(const Target&,bool,bool,StepReceipt&,std::string&);
    bool ability(const Target&,int32_t,const std::string&,bool,StepReceipt&,std::string&);
};
}
