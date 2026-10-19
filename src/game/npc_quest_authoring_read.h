#pragma once
#include "monster_quest_retirement.h"
#include "typed_action/npc_quest_action.h"

namespace mt2mp::game::npc_quest_authoring_read {
namespace qr=monster_quest_retirement;
inline bool transition(const qr::Witness& before,const qr::Witness& after,uintptr_t quest,
                       uintptr_t destination,uint64_t destination_uid,const action_session::NpcQuestIntent& intent,std::string& e){
    if(before.manager!=after.manager||before.completed!=after.completed||before.quests!=after.quests||
       before.owners.size()!=after.owners.size()||before.rows.size()!=after.rows.size()||before.links.size()!=after.links.size())
        return qr::fail(e,"Quest edit changed manager, membership, order or completed count");
    size_t changed=0;
    for(size_t i=0;i<before.quests.size();++i){
        if(before.quests[i]!=quest){
            if(before.rows[i]!=after.rows[i]||before.links[i]!=after.links[i])return qr::fail(e,"Quest edit changed unrelated row or weak link");
            continue;
        }
        ++changed;
        if(before.rows[i].owner_key!=after.rows[i].owner_key||before.rows[i].record!=intent.before||
           !action_session::npc_quest_record_transition(intent,after.rows[i].record)||
           after.links[i][0]!=destination||!after.links[i][1]||after.links[i][2]!=destination+0x20||
           !std::equal(before.links[i].begin()+3,before.links[i].end(),after.links[i].begin()+3))
            return qr::fail(e,"Quest edit selected row or exact owner/destination weak binding differs");
    }
    if(changed!=1)return qr::fail(e,"Quest edit selected pointer is not exactly one global member");
    for(const auto& [pointer,old]:before.owners){const auto found=after.owners.find(pointer);
        if(found==after.owners.end()||found->second.uid!=old.uid||found->second.key!=old.key||
           found->second.state.local.size()!=old.state.local.size()||found->second.destination_uids.size()!=old.destination_uids.size())
            return qr::fail(e,"Quest edit changed owner identity or slot count");
        const auto& now=found->second;
        for(size_t i=0;i<old.state.local.size();++i){const auto& a=old.state.local[i];const auto& b=now.state.local[i];
            if(a.quest!=b.quest||a.owner!=b.owner)return qr::fail(e,"Quest edit changed owner slot pointer");
            if(a.quest==quest){
                if(b.destination!=destination||now.destination_uids[i]!=destination_uid||b.task==npc_quest_records::TaskStatus::Absent)
                    return qr::fail(e,"Quest edit destination full UID or original recalculation task absent");
            }else if(old.state.records[i]!=now.state.records[i]||a.destination!=b.destination||a.task!=b.task||old.destination_uids[i]!=now.destination_uids[i])
                return qr::fail(e,"Quest edit changed unrelated owner slot or task");
        }
    }
    return true;
}
}
