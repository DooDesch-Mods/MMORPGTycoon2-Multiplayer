#pragma once
#include "monetary.h"
#include "state_difference.h"
#include <algorithm>
namespace mt2mp::game::live_host_state::day::economy {
// SaveScope catches up Host simulation. Its confirmed post-capture refresh adopts
// replicated visitor state and counters while keeping object and pool identities fixed.
inline bool capture_readback(const Owners& before,const Owners& after,std::string& error,bool refresh_motion=false){
    auto stable=after.state;
    if(refresh_motion){
        if(!mt2mp::subscriber_quest_state::valid(after.state.npcs,error)){error="capture refreshed NPC counters invalid original="+error;return false;}
        for(size_t i=0;i<after.state.subscribers.size();++i){
            const auto& row=after.state.subscribers[i];
            if(row.motion&&!subscriber_motion::valid(*row.motion,error)){error="capture refreshed motion invalid id="+std::to_string(row.serialized_id)+" original="+error;return false;}
            if(!subscriber_needs::valid(row.needs_bits,error)){error="capture refreshed Needs invalid id="+std::to_string(row.serialized_id)+" original="+error;return false;}
            if(!subscriber_activity::valid(row.activity,error)){error="capture refreshed activity invalid id="+std::to_string(row.serialized_id)+" original="+error;return false;}
            if(row.quest_state){
                if(!mt2mp::subscriber_quest_state::valid(*row.quest_state,error)){error="capture refreshed Quest state invalid id="+std::to_string(row.serialized_id)+" original="+error;return false;}
                for(const auto& progress:row.quest_state->progress){
                    for(const auto* link:{&progress.npc,&progress.quest}){
                        if(link->state!=mt2mp::subscriber_quest_state::LinkState::Live)continue;
                        const auto npc=std::find_if(after.state.npcs.begin(),after.state.npcs.end(),[&](const auto& value){return value.key==link->npc_key;});
                        if(npc==after.state.npcs.end()||(link==&progress.quest&&std::none_of(npc->quests.begin(),npc->quests.end(),[&](const auto& value){return value.sequence_id==link->sequence_id;}))){
                            error="capture refreshed Quest link outside canonical pool id="+std::to_string(row.serialized_id);return false;
                        }
                    }
                }
            }
            if(i<before.state.subscribers.size()){
                stable.subscribers[i].rewards=before.state.subscribers[i].rewards;
                if(row.quest_state&&before.state.subscribers[i].quest_state)
                    stable.subscribers[i].quest_state=before.state.subscribers[i].quest_state;
                stable.subscribers[i].motion=before.state.subscribers[i].motion;
                stable.subscribers[i].needs_bits=before.state.subscribers[i].needs_bits;
                stable.subscribers[i].activity=before.state.subscribers[i].activity;
            }
        }
    }
    if(refresh_motion){
        for(size_t i=0;i<std::min(before.state.npcs.size(),stable.npcs.size());++i){
            const auto& old=before.state.npcs[i];auto& current=stable.npcs[i];
            // Normalize only the comparison copy for SaveScope movement.
            // The const after.state retains the fresh pose published by Coupling.
            current.pose.position=old.pose.position;current.pose.orientation=old.pose.orientation;
            current.accepted=old.accepted;current.completed=old.completed;current.highest_cache=old.highest_cache;
            for(size_t q=0;q<std::min(old.quests.size(),current.quests.size());++q){
                current.quests[q].accepted=old.quests[q].accepted;current.quests[q].completed=old.quests[q].completed;
                current.quests[q].highest_cache=old.quests[q].highest_cache;
            }
        }
    }
    const bool same_owners=before.history.pin==after.history.pin&&before.history.owners==after.history.owners&&before.history.records==after.history.records&&
        before.subscriber_manager==after.subscriber_manager&&before.subscriber_data==after.subscriber_data&&before.loan_data==after.loan_data&&before.offer_data==after.offer_data&&
        before.subscribers==after.subscribers&&before.subscriber_slots==after.subscriber_slots&&before.needs_owners==after.needs_owners&&before.loans==after.loans&&before.offers==after.offers;
    if(before.state==stable&&same_owners&&before.next_id==after.next_id){error.clear();return true;}
    nlohmann::json cause{{"state_changed",before.state!=after.state},{"non_motion_state_changed",before.state!=stable},{"native_owners_changed",!same_owners},{"subscriber_owners_changed",before.subscribers!=after.subscribers},{"subscriber_slots_changed",before.subscriber_slots!=after.subscriber_slots},{"subscriber_needs_owners_changed",before.needs_owners!=after.needs_owners},
        {"next_id_before",before.next_id},{"next_id_after",after.next_id},
        {"subscriber_count_before",before.state.subscribers.size()},{"subscriber_count_after",after.state.subscribers.size()},
        {"owner_count_before",before.subscribers.size()},{"owner_count_after",after.subscribers.size()},
        {"day_changed",before.state.day!=after.state.day},{"additions_changed",before.state.additions!=after.state.additions},
        {"loans_changed",before.state.loans!=after.state.loans},{"offers_changed",before.state.offers!=after.state.offers}};
    const auto& pin_before=before.history.pin;const auto& pin_after=after.history.pin;
    cause["native_owner_parts"]={
        {"pin_context_changed",pin_before.context!=pin_after.context},{"pin_binding_changed",pin_before.binding!=pin_after.binding},{"pin_common_epoch_changed",pin_before.common_epoch!=pin_after.common_epoch},
        {"pin_image_changed",pin_before.image!=pin_after.image},{"pin_world_changed",pin_before.world!=pin_after.world},{"pin_clock_changed",pin_before.clock!=pin_after.clock},
        {"pin_business_changed",pin_before.business!=pin_after.business},{"pin_current_record_changed",pin_before.record!=pin_after.record},{"pin_history_array_changed",pin_before.history!=pin_after.history},
        {"pin_thread_changed",pin_before.thread!=pin_after.thread},{"pin_history_count_before",pin_before.history_count},{"pin_history_count_after",pin_after.history_count},
        {"pin_history_capacity_before",pin_before.history_capacity},{"pin_history_capacity_after",pin_after.history_capacity},
        {"pin_values_changed",pin_before.values!=pin_after.values},{"pin_selected_before",pin_before.selected},{"pin_selected_after",pin_after.selected},
        {"pin_selected_bits_before",std::bit_cast<uint32_t>(pin_before.selected)},{"pin_selected_bits_after",std::bit_cast<uint32_t>(pin_after.selected)},
        {"pin_pause_before",pin_before.pause},{"pin_pause_after",pin_after.pause},
        {"pin_pause_bits_before",std::bit_cast<uint32_t>(pin_before.pause)},{"pin_pause_bits_after",std::bit_cast<uint32_t>(pin_after.pause)},
        {"history_owners_changed",before.history.owners!=after.history.owners},{"history_owner_count_before",before.history.owners.size()},{"history_owner_count_after",after.history.owners.size()},
        {"history_records_changed",before.history.records!=after.history.records},{"history_record_count_before",before.history.records.size()},{"history_record_count_after",after.history.records.size()},
        {"subscriber_manager_changed",before.subscriber_manager!=after.subscriber_manager},{"subscriber_array_changed",before.subscriber_data!=after.subscriber_data},
        {"loan_array_changed",before.loan_data!=after.loan_data},{"offer_array_changed",before.offer_data!=after.offer_data},
        {"loan_owners_changed",before.loans!=after.loans},{"offer_owners_changed",before.offers!=after.offers}};
    if(pin_before.values!=pin_after.values)cause["pin_values_difference"]={
        {"calendar_before",pin_before.values.calendar},{"calendar_after",pin_after.values.calendar},{"cash_before",pin_before.values.cash},{"cash_after",pin_after.values.cash},
        {"current_record_before",pin_before.values.record},{"current_record_after",pin_after.values.record},{"lines_before",pin_before.values.lines},{"lines_after",pin_after.values.lines}};
    for(size_t i=0;i<std::min(before.history.records.size(),after.history.records.size());++i){
        const auto& old=before.history.records[i];const auto& current=after.history.records[i];if(old==current)continue;
        cause["first_history_record_difference"]={{"index",i},{"values_before",old.values},{"values_after",current.values},{"lines_before",old.lines},{"lines_after",current.lines}};break;
    }
    const size_t count=std::min(before.state.subscribers.size(),after.state.subscribers.size());
    for(size_t i=0;i<count;++i){
        const auto& a=before.state.subscribers[i];const auto& b=after.state.subscribers[i];if(a==stable.subscribers[i])continue;
        nlohmann::json row{{"index",i},{"id_before",a.serialized_id},{"id_after",b.serialized_id},
            {"pay_before",a.pay},{"pay_after",b.pay},{"needs_bits_before",a.needs_bits},{"needs_bits_after",b.needs_bits},{"activity_bits_before",subscriber_activity::patterns(a.activity)},{"activity_bits_after",subscriber_activity::patterns(b.activity)},{"play_days_before",a.play_days},{"play_days_after",b.play_days},
            {"online_before",a.motion.has_value()},{"online_after",b.motion.has_value()}};
        auto pose=[](const auto& optional)->nlohmann::json{
            if(!optional)return nullptr;const auto& value=*optional;
            auto bits=[](const auto& values){nlohmann::json result=nlohmann::json::array();for(float f:values)result.push_back(std::bit_cast<uint32_t>(f));return result;};
            return {{"region",value.region},{"position",value.position},{"position_bits",bits(value.position)},
                {"orientation",value.orientation},{"orientation_bits",bits(value.orientation)},{"scale",value.scale},{"scale_bits",bits(value.scale)}};
        };
        row["motion_before"]=pose(a.motion);row["motion_after"]=pose(b.motion);cause["first_subscriber_difference"]=std::move(row);break;
    }
    if(before.state!=stable)cause["state_difference"]=state_difference(stable,before.state);
    error="capture readback differs "+cause.dump();return false;
}
}
