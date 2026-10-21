#pragma once
#include "typed_action/action_session.h"
#include "building_history_slice/cash_effect.h"
#include <algorithm>
#include <limits>
#include <memory>

namespace mt2mp::game::npc_history_budget {
inline bool preflight(const action_session::Context& context,const action_session::Request& request,
                      const action_session::NpcHistoryOrigin& origin,const action_session::NpcState& before,
                      const action_session::Ledger& ledger,int32_t cash,uint32_t quests,int32_t completed,
                      std::string& error){
    namespace a=action_session;
    const bool undo=request.action==a::Action::UndoBuilding,moving=origin.action==17;
    const bool restoring=!moving&&((origin.action==16)==undo),removing=!moving&&!restoring;
    auto sample=std::make_unique<a::Message>();sample->kind=a::Kind::HostResult;sample->session=context.session;
    sample->revision=request.host_revision+1;sample->required_mask=context.peer_mask;sample->outcome=a::Outcome::Confirmed;sample->request=request;
    auto& evidence=sample->evidence;evidence.before=ledger;evidence.npc_before=before;evidence.npc_after=before;
    if(!building_history_slice::cash_effect(ledger,cash,evidence.after,error))return false;
    auto& after=evidence.npc_after;after.undo_count=undo?before.undo_count-1:(before.undo_count<20?before.undo_count+1:before.undo_count);
    after.count=restoring?before.count+1:removing?before.count-1:before.count;
    after.created=removing?a::NpcActor{}:restoring?(undo?origin.before:origin.after):before.created;
    if(moving){const auto& destination=undo?origin.before:origin.after;after.created.position=destination.position;after.created.rotation=destination.rotation;after.created.scale=destination.scale;}
    size_t regenerated=0;
    if(restoring){
        auto& records=after.created.quests;
        records.erase(std::remove_if(records.begin(),records.end(),[](const auto& row){return row.action==8;}),records.end());
        after.created.quest_count=int32_t(records.size());
        for(size_t index=0;index<records.size();++index){auto& row=records[index];row.sequence_id=int32_t(index);
            row.description="[Quest description goes here]";row.monster_type=INT32_MIN;row.xp_reward=INT32_MIN;row.gold_reward=INT32_MIN;
            row.accepted={};row.completed={};const auto bound=std::numeric_limits<float>::max();row.rating={-bound,-bound,-bound,false};++regenerated;}
    }
    after.global_random.fill(UINT64_MAX);
    a::NpcHistoryProof proof;proof.origin=origin;proof.cash=cash;
    proof.quests_before=proof.quests_after=proof.references_before=proof.references_after=std::string(64,'a');
    proof.count_before=quests;proof.completed_before=completed;proof.count_after=moving?quests:4096;proof.completed_after=moving?completed:4096;
    evidence.npc_history_proof=std::move(proof);evidence.undo_confirmed=evidence.retirement_confirmed=true;evidence.target_retired=removing;
    std::vector<uint8_t> wire;
    if(!a::encode(*sample,wire,error)){error="NPC history result wire preflight: "+error;return false;}
    // The signed integer sentinels cover regenerated integer widths. Each
    // rating component also reserves 32 bytes beyond the finite float sample,
    // so a different finite native result cannot exceed this existing budget.
    const auto bound=wire.size()+regenerated*96;
    if(bound>a::max_payload){error="NPC history result wire bound="+std::to_string(bound)+" budget="+std::to_string(a::max_payload);return false;}
    error.clear();return true;
}
}
