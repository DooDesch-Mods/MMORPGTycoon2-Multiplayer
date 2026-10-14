#pragma once
#include "monetary.h"
#include "live_host_state/day_rollover/native_day.h"
namespace mt2mp::game::live_host_state::day::economy {
class NativePort final:public Port {
    friend class LoanLifecycle;
    day::NativePort& history_;
    EconomicBoundary& fence_;
    Target actual_day_receipt_;
    const Owners* scalar_before_=nullptr;
    NativeHistory scalar_history_;
    bool once(Owners&,std::string&);
    bool motion_once(MotionRead&,std::string&);
    bool current(const Owners&,uintptr_t owner,bool subscriber,std::string&);
    bool current_subscriber(const Owners&,size_t,std::string&);
    bool preflight_from_read(const Owners&,const State&,std::string&)override;
    bool preflight_motion_body(const Owners&,const State&,std::string&);
    bool preflight_lifecycle_body(const Owners&,const State&,std::string&);
public:
    // Root supplies the actual Sync capture/applied receipt, already authenticated by Controller.
    NativePort(day::NativePort& history,EconomicBoundary& fence,const Target& actual_receipt):history_(history),fence_(fence),actual_day_receipt_(actual_receipt){}
    bool bind_actual_receipt(const Target&,std::string&)override;
    bool read(Owners&,std::string&)override;
    bool read_motion(MotionRead&,std::string&)override;
    bool begin_scalar_stores(const Owners&,std::string&)override;
    void end_scalar_stores()noexcept override;
    bool capture_subscriber(int32_t,nlohmann::json&,std::string&)override;
    bool append_subscriber(const nlohmann::json&,nlohmann::json&,std::string&)override;
    bool subscriber_retirements(const Owners&,std::vector<mt2mp::subscriber_lifecycle::additions::Retirement>&,std::string&)override;
    bool retire_subscriber(int32_t,int32_t,nlohmann::json&,std::string&)override;
    bool preflight_motion(const Owners&,const State&,std::string&)override;
    bool preflight_lifecycle(const Owners&,const State&,std::string&)override;
    bool store_lifecycle(const Owners&,size_t,const std::optional<subscriber_motion::Pose>&,const std::optional<subscriber_motion::Pose>&,std::string&)override;
    bool store_motion(const Owners&,size_t,const subscriber_motion::Pose&,const subscriber_motion::Pose&,std::string&)override;
    bool store_activity(const Owners&,size_t,size_t,uint32_t,uint32_t,std::string&)override;
    bool store_rewards(const Owners&,size_t,size_t,uint32_t,uint32_t,std::string&)override;
    bool store_quest_state(const Owners&,size_t,const std::optional<subscriber_quest_state::Toon>&,
        const std::optional<subscriber_quest_state::Toon>&,std::string&)override;
    bool store_npc_state(const Owners&,size_t,const subscriber_quest_state::Npc&,
        const subscriber_quest_state::Npc&,std::string&)override;
    bool store_need(const Owners&,size_t,size_t,uint32_t,uint32_t,std::string&)override;
    bool store_next_id(const Owners&,int32_t,std::string&)override;
    bool store_subscriber(const Owners&,size_t,size_t,int32_t,int32_t,std::string&)override;
    bool store_loan(const Owners&,size_t,size_t,int32_t,int32_t,std::string&)override;
};
}
