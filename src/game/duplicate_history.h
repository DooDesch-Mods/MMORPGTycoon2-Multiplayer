#pragma once
#include "typed_action/action_session.h"
#include "duplicate_transition.h"
#include <map>
namespace mt2mp::game::duplicate_history {
namespace a=action_session;
using Cash=a::DuplicateCash;
inline std::vector<Cash> cash(const a::DuplicateIntent& intent,bool undo){
    std::vector<Cash> result;
    if(intent.rows.size()>9){int64_t amount=0;for(const auto& row:intent.rows)amount+=row.cost;
        result.push_back({int32_t(undo?amount:-amount),intent.cash_position});}
    else for(const auto& row:intent.rows)result.push_back({undo?row.cost:-row.cost,{row.after[4],row.after[5],row.after[6]}});
    return result;
}
inline bool ledger(const a::Ledger& before,const a::Ledger& after,const std::vector<Cash>& nodes){
    auto expected=before;
    auto add=[](int32_t& value,int32_t amount){const auto next=int64_t(value)+amount;
        if(next<INT32_MIN||next>INT32_MAX)return false;value=int32_t(next);return true;};
    // DoCash always calls AddCash, including negative amounts and zero. It
    // updates signed income and one category-six line per original cash node.
    for(const auto& node:nodes){if(!expected.record_present||!add(expected.cash,node.amount)||
        !add(expected.record_cash,node.amount)||!add(expected.record_income,node.amount)||
        !add(expected.line_counts[6],1)||!add(expected.line_income[6],node.amount))return false;}
    return expected==after;
}
inline bool effect(bool undo,const a::DuplicateState& before,const a::DuplicateState& after,std::string& e){
    auto fail=[&](const char* why){e=why;return false;};
    if(bool(before.history)!=bool(after.history)||before.untouched_sha!=after.untouched_sha)
        return fail("Duplicate history changed unrelated full records or lost its native effect witness");
    if(before.history){
        if(before.history->related_keys!=after.history->related_keys||before.sources.size()!=after.sources.size())
            return fail("Duplicate Building history related owner set or source count differs");
        const std::set<std::string> related(before.history->related_keys.begin(),before.history->related_keys.end());
        for(size_t i=0;i<before.sources.size();++i){auto prior=before.sources[i];const auto& current=after.sources[i];
            if(related.contains(prior.key)){if(prior.family!=a::DuplicateFamily::Building)return fail("Duplicate Building history related source is not a Building");
                prior.record_sha=current.record_sha;}
            if(prior!=current)return fail("Duplicate Building history changed an unrelated source field");}
    }else if(before.sources!=after.sources)return fail("Duplicate history changed original source records");
    const auto& live=undo?before:after;const auto& absent=undo?after:before;
    if(live.created.empty()||!absent.created.empty())return fail("Duplicate history complete live/absent direction differs");
    if(before.history&&std::none_of(live.created.begin(),live.created.end(),[](const auto& row){return row.family==a::DuplicateFamily::Building;}))
        return fail("Duplicate Building history witness lacks an original created Building");
    std::map<std::string,const a::DuplicateObject*> rows;
    for(const auto& row:live.created)if(!rows.emplace(row.key,&row).second)return fail("Duplicate history created key repeats");
    for(const auto& row:live.created){
        if(row.family==a::DuplicateFamily::Group){
            if(row.members.empty()||row.key!="group:"+row.members.front())return fail("Duplicate history Group canonical owner differs");
            for(const auto& member:row.members){const auto found=rows.find(member);
                if(found==rows.end()||found->second->family!=a::DuplicateFamily::Scenery||found->second->parent!=row.key||found->second->region!=row.region)
                    return fail("Duplicate history complete reciprocal Group member differs");}
        }else if(!row.parent.empty()){
            const auto parent=rows.find(row.parent);
            if(parent==rows.end()||parent->second->family!=a::DuplicateFamily::Group||
               std::count(parent->second->members.begin(),parent->second->members.end(),row.key)!=1)
                return fail("Duplicate history child has missing or repeated Group membership");
        }
    }
    if(before.release!=after.release)return fail("Duplicate history original graph unexpectedly changed Release edit counts");
    return true;
}
}
