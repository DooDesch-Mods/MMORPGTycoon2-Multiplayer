#pragma once
#include "native_port.h"
#include <functional>
namespace mt2mp::game::live_host_state {
enum class EconomicLeaf:uint8_t {AddCash,SubtractCash,TakeLoan,MakeDay,Event,RepayLoan,Pay,NextDay,SubscriberDay};
class EconomicFence final:public EconomicBoundary {
    Context context_;Binding binding_;Pin pin_,action_pin_;
    bool suspended_=false;
    bool armed_=false,locked_=false,prepared_=false;
    uint32_t frame_depth_=0;
    std::optional<action_session::Message> action_;
    uint32_t action_cash_calls_=0;
    std::function<void(const char*,const std::string&)> logger_;
    std::array<uint64_t,9> suppressed_{};
    std::string cause_;
    uintptr_t existing_day_=0,existing_subscriber_day_=0;
    bool routes_ready(std::string&);
public:
    bool prepare_before_hooks(std::string&);
    bool bind_existing_day_routes(uintptr_t business_detour,uintptr_t subscriber_detour,std::string&);
    bool can_arm(std::string& e){if(!prepared_||armed_||locked_){e="live economic installation unprepared/armed/locked";return false;}return routes_ready(e);}
    bool arm(const Context&,const Binding&,std::function<void(const char*,const std::string&)>,std::string&);
    bool suspend_snapshot(std::string&);
    bool retire_lobby(std::string&);
    bool rebind_snapshot(const Context&,const Binding&,std::string&);
    bool adopt_day_commit(std::string&);
    void journal_failed(const std::string&);
    bool enter_game_update(std::string&);
    void leave_game_update();
    bool begin_action(const action_session::Message&,const Pin&,std::string&);
    bool end_action(std::string&);
    bool allow_needs(void* receiver);
    bool allow(EconomicLeaf,void* receiver,int32_t amount=0,int32_t category=0);
    void original_failed(EconomicLeaf,uint32_t seh);
    bool read_current_fence(const Pin&,std::string&)override;
    bool locked()const{return locked_;}
    const std::string& cause()const{return cause_;}
    const std::array<uint64_t,9>& suppressed()const{return suppressed_;}
};
EconomicFence& economic_fence();
bool predicted_cash(const action_session::Ledger&,bool subtract,int32_t amount,int32_t category,action_session::Ledger&,std::string&);
}

namespace mt2mp::game::live_host_state { bool retire_lobby_fence(std::string&); }
