#pragma once
#include "live_host_state/day_rollover/day.h"
#include <bit>
#include <optional>
#include <thread>
#include "subscriber_motion.h"
#include "subscriber_quest_state.h"
#include "subscriber_needs.h"
#include "subscriber_activity.h"
#include "subscriber_lifecycle/additions.h"
namespace mt2mp::game::live_host_state::day::economy {
struct Subscriber {
    int32_t serialized_id=-1;
    std::array<int32_t,5> pay{};
    uint32_t play_days=0;
    subscriber_needs::Bits needs_bits{};
    subscriber_activity::State activity;
    std::optional<subscriber_motion::Pose> motion;
    std::array<uint32_t,3> rewards{};
    std::optional<mt2mp::subscriber_quest_state::Toon> quest_state;
    bool operator==(const Subscriber&)const=default;
};
struct Loan {
    std::array<std::string,3> text;
    std::array<int32_t,4> amounts{};
    bool operator==(const Loan&)const=default;
};
struct Offer {
    std::array<std::string,3> text;
    std::array<int32_t,3> amounts{};
    bool operator==(const Offer&)const=default;
};
struct State {
    Target day;
    mt2mp::subscriber_lifecycle::additions::State additions;
    std::vector<Subscriber> subscribers;
    std::vector<Loan> loans;
    std::vector<Offer> offers;
    std::vector<mt2mp::subscriber_quest_state::Npc> npcs;
    bool operator==(const State&)const=default;
};
// Native receivers belong to the actual local manager arrays. Only IDs are wire data.
struct Owners {
    NativeHistory history;
    uintptr_t subscriber_manager=0,subscriber_data=0,loan_data=0,offer_data=0;
    std::vector<uintptr_t> subscribers,loans,offers;
    // Canonical rows retain their actual local native array slot.
    std::vector<size_t> subscriber_slots;
    std::vector<subscriber_needs::Owner> needs_owners;
    State state;
    int32_t next_id=-1;
};
// Only freshly sampled ownership and poses, never a partial economic State.
struct MotionRead {
    struct Row {
        int32_t serialized_id=-1;
        std::optional<subscriber_motion::Pose> motion;
        bool operator==(const Row&)const=default;
    };
    NativeHistory history;
    uintptr_t subscriber_manager=0,subscriber_data=0,loan_data=0,offer_data=0;
    std::vector<uintptr_t> subscribers,loans,offers;
    std::vector<size_t> subscriber_slots;
    std::vector<subscriber_needs::Owner> needs_owners;
    std::vector<Row> rows;
    int32_t next_id=-1;
};
struct Receipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
class Coupling;
class Apply;
struct Port {
private:
    friend class Coupling;
    friend class Apply;
    // A receipt can cross only callback-free code on the reading thread.
    // Its only consumers discard it before native preflight queries run.
    class ReadScope {
        Port& port_;
        Owners& owner_;
        std::thread::id thread_;
        bool fresh_=false;
    public:
        ReadScope(Port& port,Owners& owner):port_(port),owner_(owner){}
        ReadScope(const ReadScope&)=delete;
        ReadScope& operator=(const ReadScope&)=delete;
        bool read(std::string& e){close();if(!port_.read(owner_,e))return false;thread_=std::this_thread::get_id();fresh_=true;return true;}
        void close()noexcept{fresh_=false;}
        bool preflight(const State&,std::string&);
        Receipt apply(const State&);
    };
    virtual bool preflight_from_read(const Owners& before,const State& target,std::string& e){return preflight_lifecycle(before,target,e)&&preflight_motion(before,target,e);}
public:
    virtual ~Port()=default;
    virtual bool bind_actual_receipt(const Target&,std::string& error){error="actual economy Day receipt provider unavailable";return false;}
    virtual bool read(Owners&,std::string&)=0;
    virtual bool read_motion(MotionRead&,std::string&);
    // Ports without a native History optimization retain their existing store guards.
    virtual bool begin_scalar_stores(const Owners&,std::string& e){e.clear();return true;}
    virtual void end_scalar_stores()noexcept{}
    virtual bool capture_subscriber(int32_t,nlohmann::json&,std::string& e){e="native Subscriber capture unavailable";return false;}
    virtual bool append_subscriber(const nlohmann::json&,nlohmann::json&,std::string& e){e="native Subscriber append unavailable";return false;}
    virtual bool subscriber_retirements(const Owners&,std::vector<mt2mp::subscriber_lifecycle::additions::Retirement>& rows,std::string& e){rows.clear();e.clear();return true;}
    virtual bool retire_subscriber(int32_t,int32_t,nlohmann::json&,std::string& e){e="native Subscriber retirement unavailable";return false;}
    virtual bool preflight_motion(const Owners& before,const State& target,std::string& e){
        const auto& current=before.state.subscribers;
        if(before.subscribers.size()!=current.size()||current.size()!=target.subscribers.size()){
            e="Subscriber motion preflight unavailable for changed owner/row shape";return false;
        }
        for(size_t i=0;i<current.size();++i)if(current[i].serialized_id!=target.subscribers[i].serialized_id||current[i].motion!=target.subscribers[i].motion){
            e="Subscriber motion preflight unavailable for changed saved ID/pose";return false;
        }
        e.clear();return true;
    }
    virtual bool preflight_lifecycle(const Owners& before,const State& target,std::string& e){
        if(before.state.subscribers.size()!=target.subscribers.size()){e="Subscriber lifecycle row shape differs";return false;}
        for(size_t i=0;i<target.subscribers.size();++i)if(before.state.subscribers[i].motion.has_value()!=target.subscribers[i].motion.has_value()){
            e="Subscriber online transition requires native lifecycle capability";return false;
        }
        e.clear();return true;
    }
    virtual bool store_lifecycle(const Owners&,size_t,const std::optional<subscriber_motion::Pose>&,const std::optional<subscriber_motion::Pose>&,std::string& e){e="native Subscriber lifecycle unavailable";return false;}
    virtual bool store_motion(const Owners&,size_t,const subscriber_motion::Pose&,const subscriber_motion::Pose&,std::string& e){e="native Subscriber motion setter unavailable";return false;}
    virtual bool store_activity(const Owners&,size_t,size_t,uint32_t,uint32_t,std::string& e){e="native Subscriber activity CAS unavailable";return false;}
    virtual bool store_rewards(const Owners&,size_t,size_t,uint32_t,uint32_t,std::string& e){e="native Subscriber reward CAS unavailable";return false;}
    virtual bool store_quest_state(const Owners&,size_t,const std::optional<subscriber_quest_state::Toon>&,const std::optional<subscriber_quest_state::Toon>&,std::string& e){e="native Toon Quest state apply unavailable";return false;}
    virtual bool store_npc_state(const Owners&,size_t,const subscriber_quest_state::Npc&,const subscriber_quest_state::Npc&,std::string& e){e="native NPC simulation state apply unavailable";return false;}
    virtual bool store_need(const Owners&,size_t,size_t,uint32_t,uint32_t,std::string& e){e="native Subscriber Needs CAS unavailable";return false;}
    virtual bool store_next_id(const Owners&,int32_t,std::string& e){e="native Subscriber nextID store unavailable";return false;}
    virtual bool store_subscriber(const Owners&,size_t owner,size_t field,int32_t before,int32_t after,std::string&)=0;
    virtual bool store_loan(const Owners&,size_t owner,size_t field,int32_t before,int32_t after,std::string&)=0;
};
bool valid(const State&,std::string&);
bool admit_existing(const Owners&,const State&,std::string&);
Receipt advance_next_id(Port&,const Owners&,const State&,Owners& actual);
Receipt apply_existing(Port&,const Owners&,const State&);
class Apply {
    bool locked_=false;
    Receipt receipt_;
    friend class Coupling;
    const Receipt& run_observed(Port::ReadScope& scope,const State& target){if(locked_)return receipt_;receipt_=scope.apply(target);locked_=receipt_.unknown;return receipt_;}
public:
    const Receipt& run(Port& port,const Owners& before,const State& target){if(locked_)return receipt_;receipt_=apply_existing(port,before,target);locked_=receipt_.unknown;return receipt_;}
    bool locked()const{return locked_;}
};
}
