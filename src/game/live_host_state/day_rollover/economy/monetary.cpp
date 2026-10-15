#include "monetary.h"
#include "state_difference.h"
#include "subscriber_order.h"
#include "live_host_state/apply_timing.h"
#include <set>
namespace mt2mp::game::live_host_state::day::economy {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class A,class B>bool same_owners(const A& a,const B& b){
    auto p=a.history.pin,q=b.history.pin;p.values={};q.values={};
    return p==q&&a.history.owners==b.history.owners&&a.subscriber_manager==b.subscriber_manager&&a.subscriber_data==b.subscriber_data&&a.loan_data==b.loan_data&&a.offer_data==b.offer_data&&a.subscribers==b.subscribers&&a.subscriber_slots==b.subscriber_slots&&a.needs_owners==b.needs_owners&&a.loans==b.loans&&a.offers==b.offers;
}
template<class Row>bool valid_rows(const std::vector<Row>& rows,std::string& error){
    std::set<std::string> ids;
    for(const auto& row:rows){
        if(row.text[0].empty()||row.text[0].size()>256||row.text[1].size()>2048||row.text[2].size()>2048||!ids.insert(row.text[0]).second)return fail(error,"Host economy Loan/Offer native ID/string bound differs");
        for(const auto& text:row.text)if(text.find('\0')!=std::string::npos)return fail(error,"Host economy native string contains embedded NUL");
        for(auto n:row.amounts)if(n<0)return fail(error,"Host economy negative Loan/Offer amount/period");
    }
    return true;
}
}
bool Port::read_motion(MotionRead& out,std::string& error){
    out={};Owners current;if(!read(current,error))return false;
    out.history=std::move(current.history);out.subscriber_manager=current.subscriber_manager;
    out.subscriber_data=current.subscriber_data;out.loan_data=current.loan_data;out.offer_data=current.offer_data;
    out.subscribers=std::move(current.subscribers);out.loans=std::move(current.loans);out.offers=std::move(current.offers);
    out.subscriber_slots=std::move(current.subscriber_slots);out.needs_owners=std::move(current.needs_owners);out.next_id=current.next_id;
    for(auto& row:current.state.subscribers)out.rows.push_back({row.serialized_id,std::move(row.motion)});
    return true;
}
bool valid(const State& state,std::string& error){
    if(!day::valid(state.day,error))return false;
    int32_t previous=-1;
    for(const auto& subscriber:state.subscribers){if(subscriber.serialized_id<=previous)return fail(error,"Host economy Subscriber IDs require strict increasing canonical order");previous=subscriber.serialized_id;}
    for(const auto& subscriber:state.subscribers)if(subscriber.motion&&!subscriber_motion::valid(*subscriber.motion,error))return false;
    for(const auto& subscriber:state.subscribers)if(!subscriber_needs::valid(subscriber.needs_bits,error)||!subscriber_activity::valid(subscriber.activity,error))return false;
    for(const auto& subscriber:state.subscribers)if(subscriber.quest_state&&!mt2mp::subscriber_quest_state::valid(*subscriber.quest_state,error))return false;
    if(!mt2mp::subscriber_quest_state::valid(state.npcs,error))return false;
    if(!valid_rows(state.loans,error)||!valid_rows(state.offers,error))return false;
    error.clear();return true;
}
bool admit_existing(const Owners& expected,const State& target,std::string& error){
    if(!valid(target,error)||!valid(expected.state,error)||!valid_subscriber_slots(expected,error))return false;
    const auto& before=expected.state;
    if(expected.history.pin.context.role!=action_session::Role::Client||expected.history.pin.selected!=0||before.day.frame.session!=target.day.frame.session||before.day.frame.epoch!=target.day.frame.epoch||before.day.frame.snapshot!=target.day.frame.snapshot||expected.subscribers.size()!=before.subscribers.size()||expected.loans.size()!=before.loans.size()||expected.offers.size()!=before.offers.size())return fail(error,"actual economic owner/ClientClock0/frame binding differs");
    if(before.subscribers.size()!=target.subscribers.size()||before.loans.size()!=target.loans.size()||before.offers!=target.offers)return fail(error,"native economy membership transition requires actual C1/Add/D0 receipts before any Day/scalar store");
    for(size_t i=0;i<before.subscribers.size();++i)if(before.subscribers[i].serialized_id!=target.subscribers[i].serialized_id)return fail(error,"actual Subscriber serialized identity/order differs");
    for(size_t i=0;i<before.loans.size();++i)if(before.loans[i].text!=target.loans[i].text)return fail(error,"actual Loan GNU string identity differs; scalar writes cannot invent native strings");
    if(before.npcs.size()!=target.npcs.size())return fail(error,"NPC state membership differs before ordered action retirement");
    for(size_t i=0;i<before.npcs.size();++i){
        const auto& old=before.npcs[i];const auto& after=target.npcs[i];
        if(old.key!=after.key||old.type!=after.type||old.quests.size()!=after.quests.size())return fail(error,"NPC state canonical identity/type/Quest membership differs");
        for(size_t q=0;q<old.quests.size();++q)if(old.quests[q].sequence_id!=after.quests[q].sequence_id)return fail(error,"NPC state Quest sequence/order differs");
    }
    error.clear();return true;
}
Receipt advance_next_id(Port& port,const Owners& expected,const State& target,Owners& actual){
    Receipt receipt;actual={};
    const auto required=target.additions.next_id;
    auto stop=[&](const std::string& cause){
        receipt.unknown=receipt.attempted;receipt.confirmed=false;
        receipt.cause=cause+" actual="+std::to_string(actual.next_id)+" before="+std::to_string(expected.next_id)+
            " required="+std::to_string(required)+" actual_count="+std::to_string(actual.state.subscribers.size())+
            " target_count="+std::to_string(target.subscribers.size())+" original="+receipt.cause;
    };
    try{
        if(!target.additions.available||!mt2mp::subscriber_lifecycle::additions::valid(target.additions,receipt.cause)||
           !admit_existing(expected,target,receipt.cause)||expected.next_id<0||required<expected.next_id){stop("Subscriber nextID admission differs");return receipt;}
        for(const auto& row:expected.state.subscribers)if(row.serialized_id>=expected.next_id){stop("Subscriber existing ID outside current nextID");return receipt;}
        if(!port.read(actual,receipt.cause)||!same_owners(actual,expected)||actual.state!=expected.state||actual.next_id!=expected.next_id){stop("Subscriber nextID prior readback differs");return receipt;}
        if(required==expected.next_id){receipt.confirmed=true;return receipt;}
        // Rejected native signups consume an ID without leaving a cohort member.
        // Advance only the allocator; all current objects and values must survive.
        receipt.attempted=true;receipt.attempted_stores=1;
        if(!port.store_next_id(expected,required,receipt.cause)){stop("Subscriber nextID CAS Unknown; no replay");return receipt;}
        receipt.stores=1;
        if(!port.read(actual,receipt.cause)||!same_owners(actual,expected)||actual.state!=expected.state||actual.next_id!=required){stop("Subscriber nextID full readback Unknown; no replay");return receipt;}
        receipt.confirmed=true;receipt.cause.clear();return receipt;
    }catch(const std::exception& x){receipt.cause=x.what();stop("Subscriber nextID exception; no replay");return receipt;}
    catch(...){stop("Subscriber nextID nonstandard exception; no replay");return receipt;}
}
template<class Preflight>
static Receipt apply_existing_body(Port& port,const Owners& expected,const State& target,Preflight&& preflight){
    Receipt receipt;
    try{
        {apply_timing::Scope measured(apply_timing::Part::Preflight);
        if(!admit_existing(expected,target,receipt.cause))return receipt;
        if(!preflight(receipt.cause))return receipt;}
        const auto& before=expected.state;
        if(before.day!=target.day){receipt.cause="actual Day allocation/scalar receipt must be committed before economic stores";return receipt;}
        Owners actual;
        {apply_timing::Scope measured(apply_timing::Part::FullBefore);if(!port.read(actual,receipt.cause)||!same_owners(actual,expected)||actual.state!=before){if(receipt.cause.empty())receipt.cause="actual economic prior owner/value receipt differs";return receipt;}}
        auto stop=[&](const std::string& stage){receipt.unknown=receipt.attempted;receipt.cause=stage+" original="+receipt.cause+"; no replay";};
        {apply_timing::Scope measured(apply_timing::Part::Scalar);
        if(!port.begin_scalar_stores(expected,receipt.cause))return receipt;
        struct ScalarScope {Port& port;~ScalarScope(){port.end_scalar_stores();}} scalar_scope{port};
        for(size_t i=0;i<before.subscribers.size();++i)for(size_t field=0;field<6;++field){
            const auto old=field<5?before.subscribers[i].pay[field]:std::bit_cast<int32_t>(before.subscribers[i].play_days);
            const auto after=field<5?target.subscribers[i].pay[field]:std::bit_cast<int32_t>(target.subscribers[i].play_days);
            if(old==after)continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_subscriber(expected,i,field,old,after,receipt.cause)){stop("actual Subscriber CAS Unknown");return receipt;}
            ++receipt.stores;
        }
        for(size_t i=0;i<before.subscribers.size();++i)for(size_t field=0;field<7;++field){
            const auto old=before.subscribers[i].needs_bits[field],after=target.subscribers[i].needs_bits[field];
            if(old==after)continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_need(expected,i,field,old,after,receipt.cause)){stop("actual Subscriber Needs CAS Unknown");return receipt;}
            ++receipt.stores;
        }
        for(size_t i=0;i<before.subscribers.size();++i)for(size_t field=0;field<3;++field){
            const auto old=subscriber_activity::patterns(before.subscribers[i].activity)[field],after=subscriber_activity::patterns(target.subscribers[i].activity)[field];
            if(old==after)continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_activity(expected,i,field,old,after,receipt.cause)){stop("actual Subscriber activity CAS Unknown");return receipt;}
            ++receipt.stores;
        }
        for(size_t i=0;i<before.subscribers.size();++i)for(size_t field=0;field<3;++field){
            const auto old=before.subscribers[i].rewards[field],after=target.subscribers[i].rewards[field];
            if(old==after)continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_rewards(expected,i,field,old,after,receipt.cause)){stop("actual Subscriber reward CAS Unknown");return receipt;}
            ++receipt.stores;
        }
        for(size_t i=0;i<before.loans.size();++i)for(size_t field=0;field<4;++field){
            const auto old=before.loans[i].amounts[field],after=target.loans[i].amounts[field];
            if(old==after)continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_loan(expected,i,field,old,after,receipt.cause)){stop("actual Loan CAS Unknown");return receipt;}
            ++receipt.stores;
        }
        }
        {apply_timing::Scope measured(apply_timing::Part::Motion);
        for(size_t i=0;i<before.subscribers.size();++i){
            const auto& old=before.subscribers[i].motion;const auto& after=target.subscribers[i].motion;
            if(old.has_value()==after.has_value())continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_lifecycle(expected,i,old,after,receipt.cause)){stop("actual Subscriber native lifecycle Unknown");return receipt;}
            ++receipt.stores;
        }
        }
        for(size_t i=0;i<before.subscribers.size();++i){
            const auto& old=before.subscribers[i].quest_state;const auto& after=target.subscribers[i].quest_state;
            if(old==after)continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_quest_state(expected,i,old,after,receipt.cause)){stop("actual Toon Quest state Unknown");return receipt;}
            ++receipt.stores;
        }
        for(size_t i=0;i<before.npcs.size();++i){
            const auto& old=before.npcs[i];const auto& after=target.npcs[i];
            if(same_npc_non_pose(old,after))continue;receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_npc_state(expected,i,old,after,receipt.cause)){stop("actual NPC state Unknown");return receipt;}
            ++receipt.stores;
        }
        // Native SaveScopes can flush actor movement. Set authoritative poses
        // after those scopes, using the freshly observed local pose as CAS before.
        {apply_timing::Scope measured(apply_timing::Part::Motion);
        MotionRead motion;
        if(!port.read_motion(motion,receipt.cause)||!same_owners(motion,expected)){
            if(receipt.cause.empty())receipt.cause="native final motion owner receipt changed";
            stop("actual Subscriber final motion read Unknown");return receipt;
        }
        if(motion.rows.size()!=target.subscribers.size()){
            receipt.cause="native final motion Subscriber count differs";stop("actual Subscriber final motion Unknown");return receipt;
        }
        for(size_t i=0;i<target.subscribers.size();++i){
            if(motion.rows[i].serialized_id!=target.subscribers[i].serialized_id){
                receipt.cause="native final motion canonical Subscriber ID differs";stop("actual Subscriber final motion Unknown");return receipt;
            }
            const auto& old=motion.rows[i].motion;const auto& after=target.subscribers[i].motion;
            if(old==after)continue;
            if(!old||!after){receipt.cause="native lifecycle must settle before final motion";stop("actual Subscriber final motion Unknown");return receipt;}
            receipt.attempted=true;++receipt.attempted_stores;
            if(!port.store_motion(expected,i,*old,*after,receipt.cause)){stop("actual Subscriber final motion Unknown");return receipt;}
            ++receipt.stores;
        }
        }
        {apply_timing::Scope measured(apply_timing::Part::FullFinal);
        if(!port.read(actual,receipt.cause)){stop("actual Subscriber/Loan/Day full readback Unknown");return receipt;}
        if(!same_owners(actual,expected)){receipt.cause=owner_difference(actual,expected);stop("actual Subscriber/Loan/Day full readback Unknown");return receipt;}
        if(actual.state!=target){
            // No SaveScope or Subscriber setter follows this pose phase. A fresh
            // complete read must already match every other authoritative value.
            State settled=actual.state;
            if(settled.npcs.size()!=target.npcs.size()){receipt.cause=state_difference(actual.state,target);stop("actual NPC final pose membership Unknown");return receipt;}
            for(size_t i=0;i<settled.npcs.size();++i){
                if(!same_npc_non_pose(settled.npcs[i],target.npcs[i])){receipt.cause=npc_non_pose_difference(settled.npcs[i],target.npcs[i]);stop("actual NPC final non-pose readback Unknown");return receipt;}
                settled.npcs[i].pose=target.npcs[i].pose;
            }
            if(settled!=target){receipt.cause=state_difference(settled,target);stop("actual Subscriber/Loan/Day full readback Unknown");return receipt;}
            const auto poses=actual.state.npcs;
            for(size_t i=0;i<poses.size();++i){
                if(poses[i].pose==target.npcs[i].pose)continue;
                receipt.attempted=true;++receipt.attempted_stores;
                if(!port.store_npc_pose(expected,i,poses[i],target.npcs[i],receipt.cause)){stop("actual NPC final pose Unknown");return receipt;}
                ++receipt.stores;
            }
            if(!port.read(actual,receipt.cause)){stop("actual NPC final pose full readback Unknown");return receipt;}
            if(!same_owners(actual,expected)){receipt.cause=owner_difference(actual,expected);stop("actual NPC final pose full readback Unknown");return receipt;}
            if(actual.state!=target){receipt.cause=state_difference(actual.state,target);stop("actual NPC final pose full readback Unknown");return receipt;}
        }}
        receipt.confirmed=true;receipt.cause.clear();return receipt;
    }catch(const std::exception& x){receipt.unknown=receipt.attempted;receipt.cause=std::string("economic native apply exception original=")+x.what();return receipt;}
    catch(...){receipt.unknown=receipt.attempted;receipt.cause="economic native apply nonstandard exception";return receipt;}
}
bool Port::ReadScope::preflight(const State& target,std::string& e){
    if(!fresh_){e="economic local read receipt is absent or already consumed";return false;}
    if(thread_!=std::this_thread::get_id()){close();e="economic local read receipt thread differs";return false;}
    close();
    return port_.preflight_from_read(owner_,target,e);
}
Receipt Port::ReadScope::apply(const State& target){
    return apply_existing_body(port_,owner_,target,[&](std::string& e){return preflight(target,e);});
}
Receipt apply_existing(Port& port,const Owners& expected,const State& target){
    return apply_existing_body(port,expected,target,[&](std::string& e){return port.preflight_lifecycle(expected,target,e)&&port.preflight_motion(expected,target,e);});
}

}
