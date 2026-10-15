#pragma once
#include "monetary.h"

namespace mt2mp::game::live_host_state::day::economy {
template<class T>inline std::string difference(const std::string& field,const T& actual,const T& required){
    return field+" actual="+nlohmann::json(actual).dump()+" required="+nlohmann::json(required).dump();
}
inline std::string daily_difference(const std::string& path,const npc_quest_records::DailyCount& actual,const npc_quest_records::DailyCount& required){
    if(actual.today!=required.today)return difference(path+".today",actual.today,required.today);
    if(actual.total!=required.total)return difference(path+".total",actual.total,required.total);
    if(actual.history.size()!=required.history.size())return difference(path+".history.count",actual.history.size(),required.history.size());
    for(size_t i=0;i<actual.history.size();++i)if(actual.history[i]!=required.history[i])return difference(path+".history["+std::to_string(i)+"]",actual.history[i],required.history[i]);
    return {};
}
inline std::string npc_difference(const std::vector<subscriber_quest_state::Npc>& actual,const std::vector<subscriber_quest_state::Npc>& required){
    if(actual.size()!=required.size())return difference("npc.count",actual.size(),required.size());
    for(size_t i=0;i<actual.size();++i){
        const auto& a=actual[i];const auto& b=required[i];const auto key="npc["+b.key+"].";
        if(a.key!=b.key)return difference("npc["+std::to_string(i)+"].key",a.key,b.key);
        if(a.type!=b.type)return difference(key+"type",a.type,b.type);
        if(a.level!=b.level)return difference(key+"level",a.level,b.level);
        if(a.pose.region!=b.pose.region)return difference(key+"pose.region",a.pose.region,b.pose.region);
        if(a.pose.position!=b.pose.position)return difference(key+"pose.position",a.pose.position,b.pose.position);
        if(a.pose.orientation!=b.pose.orientation)return difference(key+"pose.orientation",a.pose.orientation,b.pose.orientation);
        if(a.pose.scale!=b.pose.scale)return difference(key+"pose.scale",a.pose.scale,b.pose.scale);
        if(a.accepted!=b.accepted)return daily_difference(key+"accepted",a.accepted,b.accepted);
        if(a.completed!=b.completed)return daily_difference(key+"completed",a.completed,b.completed);
        if(a.highest_cache!=b.highest_cache)return difference(key+"highest_cache",a.highest_cache,b.highest_cache);
        if(a.quests.size()!=b.quests.size())return difference(key+"quests.count",a.quests.size(),b.quests.size());
        for(size_t q=0;q<a.quests.size();++q){
            const auto& x=a.quests[q];const auto& y=b.quests[q];const auto path=key+"quest["+std::to_string(y.sequence_id)+"].";
            if(x.sequence_id!=y.sequence_id)return difference(key+"quests["+std::to_string(q)+"].sequence_id",x.sequence_id,y.sequence_id);
            if(x.accepted!=y.accepted)return daily_difference(path+"accepted",x.accepted,y.accepted);
            if(x.completed!=y.completed)return daily_difference(path+"completed",x.completed,y.completed);
            if(x.highest_cache!=y.highest_cache)return difference(path+"highest_cache",x.highest_cache,y.highest_cache);
        }
    }
    return "NPC difference unavailable";
}
inline std::string npc_non_pose_difference(const subscriber_quest_state::Npc& actual,const subscriber_quest_state::Npc& required){
    auto observed=actual;
    observed.pose.orientation=required.pose.orientation;observed.pose.position=required.pose.position;observed.pose.scale=required.pose.scale;
    return npc_difference({observed},{required});
}
inline std::string state_difference(const State& actual,const State& required){
    if(actual.day!=required.day){
        if(actual.day.frame.values!=required.day.frame.values)return difference("day.scalars",scalars(actual.day.frame.values),scalars(required.day.frame.values));
        return "day frame/history differs";
    }
    if(actual.additions!=required.additions)return "Subscriber additions receipt differs";
    if(actual.subscribers.size()!=required.subscribers.size())return difference("subscriber.count",actual.subscribers.size(),required.subscribers.size());
    for(size_t i=0;i<actual.subscribers.size();++i){
        const auto& a=actual.subscribers[i];const auto& b=required.subscribers[i];
        const auto key="subscriber["+std::to_string(b.serialized_id)+"].";
        if(a.serialized_id!=b.serialized_id)return difference(key+"id",a.serialized_id,b.serialized_id);
        if(a.pay!=b.pay)return difference(key+"pay",a.pay,b.pay);
        if(a.play_days!=b.play_days)return difference(key+"play_days",a.play_days,b.play_days);
        if(a.needs_bits!=b.needs_bits)return difference(key+"needs_bits",a.needs_bits,b.needs_bits);
        if(a.activity!=b.activity)return difference(key+"activity",subscriber_activity::patterns(a.activity),subscriber_activity::patterns(b.activity));
        if(a.rewards!=b.rewards)return difference(key+"rewards",a.rewards,b.rewards);
        if(a.motion!=b.motion){
            if(a.motion.has_value()!=b.motion.has_value())return difference(key+"online",a.motion.has_value(),b.motion.has_value());
            if(a.motion->region!=b.motion->region)return difference(key+"region",a.motion->region,b.motion->region);
            if(a.motion->position!=b.motion->position)return difference(key+"position",a.motion->position,b.motion->position);
            if(a.motion->orientation!=b.motion->orientation)return difference(key+"orientation",a.motion->orientation,b.motion->orientation);
            return difference(key+"scale",a.motion->scale,b.motion->scale);
        }
        if(a.quest_state!=b.quest_state){
            if(a.quest_state.has_value()!=b.quest_state.has_value())return difference(key+"toon",a.quest_state.has_value(),b.quest_state.has_value());
            const auto& x=*a.quest_state;const auto& y=*b.quest_state;
            if(x.gold!=y.gold)return difference(key+"gold",x.gold,y.gold);
            if(x.xp_bits!=y.xp_bits)return difference(key+"xp_bits",x.xp_bits,y.xp_bits);
            if(x.level!=y.level)return difference(key+"level",x.level,y.level);
            if(x.attributes!=y.attributes)return difference(key+"attributes",x.attributes,y.attributes);
            if(x.seconds_to_level_bits!=y.seconds_to_level_bits)return difference(key+"seconds_to_level_bits",x.seconds_to_level_bits,y.seconds_to_level_bits);
            if(x.quests_completed!=y.quests_completed)return difference(key+"quests_completed",x.quests_completed,y.quests_completed);
            if(x.progress.size()!=y.progress.size())return difference(key+"progress.count",x.progress.size(),y.progress.size());
            for(size_t q=0;q<x.progress.size();++q){
                const auto& left=x.progress[q];const auto& right=y.progress[q];
                const auto path=key+"progress["+std::to_string(q)+"].";
                const auto link=[](const subscriber_quest_state::Link& value){return nlohmann::json{
                    {"state",int(value.state)},{"npc_key",value.npc_key},{"sequence_id",value.sequence_id}};};
                if(left.npc!=right.npc)return difference(path+"npc",link(left.npc),link(right.npc));
                if(left.quest!=right.quest)return difference(path+"quest",link(left.quest),link(right.quest));
                if(left.completed!=right.completed)return difference(path+"completed",left.completed,right.completed);
                if(left.monsters_defeated!=right.monsters_defeated)return difference(path+"monsters_defeated",left.monsters_defeated,right.monsters_defeated);
                if(left.visited!=right.visited)return difference(path+"visited",left.visited,right.visited);
                if(left.action_performed!=right.action_performed)return difference(path+"action_performed",left.action_performed,right.action_performed);
                if(left.complete!=right.complete)return difference(path+"complete",left.complete,right.complete);
            }
            return key+"ordered Quest progress differs";
        }
    }
    if(actual.npcs!=required.npcs)return "NPC pose/counters differ: "+npc_difference(actual.npcs,required.npcs);
    if(actual.loans!=required.loans)return "Loan values differ";
    if(actual.offers!=required.offers)return "Offer values differ";
    return "economy state difference unavailable";
}
inline std::string owner_difference(const Owners& actual,const Owners& required){
    const auto& a=actual.history.pin;const auto& b=required.history.pin;
    if(a.selected!=b.selected)return difference("owner.clock.selected",a.selected,b.selected);
    if(a.pause!=b.pause)return difference("owner.clock.pause",a.pause,b.pause);
    if(a.history_count!=b.history_count)return difference("owner.history.count",a.history_count,b.history_count);
    if(a.history_capacity!=b.history_capacity)return difference("owner.history.capacity",a.history_capacity,b.history_capacity);
    if(actual.history.owners!=required.history.owners)return "owner.History records changed";
    if(actual.subscriber_slots!=required.subscriber_slots)return "owner.Subscriber array slots changed";
    if(actual.subscribers!=required.subscribers)return "owner.Subscriber members changed";
    if(actual.needs_owners!=required.needs_owners)return "owner.Subscriber Brain/NeedSet changed";
    if(actual.loans!=required.loans||actual.offers!=required.offers)return "owner.Loan/Offer members changed";
    if(actual.subscriber_data!=required.subscriber_data)return "owner.Subscriber array data changed";
    if(actual.subscriber_manager!=required.subscriber_manager)return "owner.Subscriber manager changed";
    if(actual.loan_data!=required.loan_data||actual.offer_data!=required.offer_data)return "owner.Loan/Offer array data changed";
    return "owner.native Frame/World/Clock/Business binding changed";
}
}
