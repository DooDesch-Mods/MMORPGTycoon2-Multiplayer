#pragma once
#include "state.h"
#include "day_rollover/sync.h"
#include "day_rollover/economy/coupling.h"
#include <functional>
#include <optional>
#include <chrono>
namespace mt2mp::game::live_host_state {
// The existing Controller owns action validation, revision and ACK order.
struct NetworkActivationReceipt {
    nlohmann::json last=nullptr;
    uint64_t attempted_events=0,confirmed_events=0;
    void clear(){last=nullptr;attempted_events=confirmed_events=0;}
    bool record(const Frame& frame,const Binding& binding,const network::Receipt& receipt,std::string& error){
        if(receipt.activations.empty())return true;
        try{
            uint64_t attempted=0,confirmed=0;
            for(const auto& row:receipt.activations){const auto& event=row.event;
                if(event.attempted)++attempted;
                if(event.attempted&&event.confirmed&&event.sent&&event.cleaned)++confirmed;
            }
            if(attempted>UINT64_MAX-attempted_events||confirmed>UINT64_MAX-confirmed_events){error="network activation receipt counter overflow";return false;}
            auto next=nlohmann::json{{"session",frame.session},{"snapshot_uuid",frame.snapshot},{"epoch",frame.epoch},
                {"source_generation",frame.source_generation},{"local_native_epoch",binding.native_epoch},{"local_native_generation",binding.native_generation},
                {"frame_revision",frame.revision},{"base_action_revision",frame.base_action_revision},{"time_revision",frame.time_revision},
                {"apply_confirmed",receipt.confirmed},{"apply_unknown",receipt.unknown},
                {"activations",network::encode_receipt(receipt).at("activations")}};
            last=std::move(next);attempted_events+=attempted;confirmed_events+=confirmed;return true;
        }catch(const std::exception& x){error=std::string("network retain activation receipt original=")+x.what();return false;}
        catch(...){error="network retain activation receipt nonstandard exception";return false;}
    }
    nlohmann::json status()const{return {{"attempted_events",attempted_events},{"confirmed_events",confirmed_events},{"last",last}};}
};
class Bridge {
    day::Sync* day_=nullptr;
    day::economy::Coupling* economy_=nullptr;
    std::optional<day::economy::State> captured_economy_,queued_economy_,before_economy_,last_economy_;
    std::optional<day::Target> captured_day_,queued_day_,before_day_,last_day_;
    bool suspended_=false,lobby_retiring_=false;
    Context context_;Binding binding_;Port& port_;
    std::function<bool(uint64_t& revision,std::string&)> time_;
    action_session::Transport& transport_;
    std::optional<Frame> last_,queued_,host_before_;
    uint64_t last_day_sequence_=0;
    std::optional<std::chrono::steady_clock::time_point> last_capture_;
    uint64_t host_revision_=0,common_epoch_=0,source_generation_=0;
    bool locked_=false;
    ApplyReceipt receipt_;
    network::Receipt network_receipt_;
    NetworkActivationReceipt network_activation_receipt_;
    bool capture(uint64_t action_revision,Frame&,std::string&);
    bool send(const Frame&,std::string&);
    bool take(const Frame&,std::string&);
public:
    Bridge(const Context& c,const Binding& b,uint64_t common_epoch,Port& p,action_session::Transport& t,
        std::function<bool(uint64_t&,std::string&)> time):context_(c),binding_(b),port_(p),time_(std::move(time)),transport_(t),common_epoch_(common_epoch){}
    void bind_day(day::Sync& value){day_=&value;}
    void bind_economy(day::economy::Coupling& value){economy_=&value;}
    bool after_action(const action_session::Message&,std::string&);
    bool suspend_snapshot(std::string&);
    bool suspend_lobby(std::string&);
    bool rebind_snapshot(const Context&,const Binding&,uint64_t common_epoch,std::string&);
    bool capture_before(const action_session::Request&,const action_session::Ledger&,uint64_t previous,std::string&);
    bool publish_before(const action_session::Message&,std::string&);
    bool receive(uint64_t sender,const uint8_t*,size_t,std::string&);
    bool route_receive(uint64_t sender,const uint8_t*,size_t,bool& handled,std::string&);
    bool tick(uint64_t existing_action_revision,bool existing_action_pending,std::string&,
        std::chrono::steady_clock::time_point now=std::chrono::steady_clock::now());
    bool before_result(uint64_t sender,const action_session::Message&,uint64_t previous,std::string&);
    action_session::AuthorityReadiness before_ready(uint64_t sender,const action_session::Message&,uint64_t previous,std::string&);
    bool locked()const{return locked_;}
    const ApplyReceipt& receipt()const{return receipt_;}
    const network::Receipt& network_receipt()const{return network_receipt_;}
    const NetworkActivationReceipt& network_activation_receipt()const{return network_activation_receipt_;}
    const std::optional<Frame>& last()const{return last_;}
};
}
