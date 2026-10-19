#pragma once
#include "boss_placement_reader.h"
#include "mt2mp/core/hash.h"
#include <algorithm>

namespace mt2mp::game::boss_placement_state {
namespace actors=interventions::actors;
using State=action_session::BossPlacementState;
inline bool valid(const State& value){
    if(!boss_placement_reader::sha(value.untouched_sha)||value.created.has_value()!=!value.record_sha.empty())return false;
    if(!value.created)return true;
    std::string error;
    return boss_placement_reader::sha(value.record_sha)&&value.created->target.family==interventions::Family::Boss&&actors::valid({*value.created},error);
}
inline std::string key(const std::string& uuid){return "action:"+uuid+":boss:0";}
inline bool born(const action_session::BossPlacementIntent& intent,const std::string& uuid,const State& value){
    if(!boss_placement_reader::valid(intent)||!valid(value)||!value.created)return false;
    const auto& row=*value.created;
    if(row.target.key!=key(uuid)||row.slot!=3||row.type!=intent.type||row.pose.region!=intent.region)return false;
    std::array<uint32_t,10> pose{};
    for(size_t i=0;i<4;++i)pose[i]=std::bit_cast<uint32_t>(row.pose.orientation[i]);
    for(size_t i=0;i<3;++i){pose[4+i]=std::bit_cast<uint32_t>(row.pose.position[i]);pose[7+i]=std::bit_cast<uint32_t>(row.pose.scale[i]);}
    return pose==intent.pose;
}
// Purchase calls SubtractCash(1000,6), not the Undo system's signed AddCash.
template<class Ledger>bool payment(const Ledger& before,const Ledger& after){
    if(!before.record_present)return false;auto expected=before;
    auto add=[](int32_t& value,int32_t delta){const auto result=int64_t(value)+delta;if(result<INT32_MIN||result>INT32_MAX)return false;value=int32_t(result);return true;};
    return add(expected.cash,-1000)&&add(expected.record_cash,-1000)&&add(expected.record_expense,1000)&&
        add(expected.line_counts[6],1)&&add(expected.line_income[6],-1000)&&expected==after;
}
inline bool effect(const action_session::BossPlacementIntent& intent,const std::string& uuid,const State& before,const State& after){
    return boss_placement_reader::valid(intent)&&valid(before)&&!before.created&&born(intent,uuid,after)&&before.untouched_sha==after.untouched_sha;
}
// Read the complete current actor census, keeping the new original Boss row
// separate. The same full State is then used by the existing actor writer.
inline bool split(const actors::State& complete,const std::string& created,State& out,std::string& error){
    if(!actors::valid(complete,error))return false;actors::State untouched;out.created.reset();
    for(const auto& row:complete){if(row.target.key==created){if(out.created)return npc_quest_targets::fail(error,"Boss current canonical Birth row repeats");out.created=row;}else untouched.push_back(row);}
    const auto text=actors::encode(untouched).dump();out.untouched_sha=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));
    return boss_placement_reader::sha(out.untouched_sha)||npc_quest_targets::fail(error,"Boss complete untouched Actor digest failed");
}
inline bool adoption(const actors::State& current,const State& authority,actors::State& target,std::string& error){
    target={};if(!valid(authority)||!authority.created)return npc_quest_targets::fail(error,"Boss authoritative native Birth witness is absent");
    State local;if(!split(current,authority.created->target.key,local,error))return false;
    if(!local.created||local.untouched_sha!=authority.untouched_sha)return npc_quest_targets::fail(error,"Boss authoritative adoption current complete survivors or Birth identity differs");
    target=current;
    for(auto& row:target)if(row.target==authority.created->target){row=*authority.created;return actors::valid(target,error);}
    return npc_quest_targets::fail(error,"Boss authoritative new row has no current native alias");
}
}
