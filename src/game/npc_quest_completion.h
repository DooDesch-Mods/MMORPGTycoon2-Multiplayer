#pragma once
#include "npc_quest_retirement.h"
#include <map>
#include <set>

namespace mt2mp::npc_quest_completion {
struct Snapshot {
    uintptr_t manager=0,data=0;int32_t capacity=0,completed=0,derived=0;
    std::vector<uintptr_t> quests;
};
inline bool fail(std::string& e,const std::string& s){e=s;return false;}
template<class Owners,class Rows>
bool read(npc_census::Memory& memory,uintptr_t image,const Owners& owners,const Rows& rows,
          Snapshot& out,std::string& e,bool require_count=true){
    out={};using npc_quest_records::get;
    if(owners.size()!=rows.size())return fail(e,"Quest completion owner/value coverage differs");
    std::map<uintptr_t,int32_t> expected;
    for(size_t i=0;i<owners.size();++i){
        if(owners[i].key!=rows[i].key||owners[i].quests.size()!=rows[i].quests.size())
            return fail(e,"Quest completion canonical owner/sequence coverage differs");
        for(size_t j=0;j<owners[i].quests.size();++j){
            const auto& [sequence,pointer]=owners[i].quests[j];const auto& value=rows[i].quests[j];
            if(!pointer||sequence!=value.sequence_id||value.completed.total<0||!expected.emplace(pointer,value.completed.total).second)
                return fail(e,"Quest completion canonical Quest duplicate or sequence differs");
        }
    }
    npc_quest_retirement::State census;
    if(!npc_quest_retirement::read(memory,image,{},census,e))return false;
    Snapshot value;value.manager=census.manager;
    if(size_t(census.manager_count)!=expected.size())return fail(e,"Quest completion global coverage actual="+
        std::to_string(census.manager_count)+" canonical="+std::to_string(expected.size()));
    if(!get(memory,value.manager,0x30,value.data,"completion array",e)||
       !get(memory,value.manager,0x3c,value.capacity,"completion capacity",e)||
       !get(memory,value.manager,0x48,value.completed,"completion aggregate",e))return false;
    if(value.completed<0||value.completed>census.manager_count)return fail(e,"Quest completion native aggregate outside active Quest count");
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<census.manager_count;++i){
        uintptr_t quest=0,ap=0;int32_t total=-1;
        if(!get(memory,value.data,size_t(i)*8,quest,"completion member",e)||!unique.insert(quest).second||
           !get(memory,quest,0,ap,"completion Quest class",e)||ap!=image+0x174ac10||
           !npc_quest_records::manager_member(memory,image,value.manager,quest,e)||
           !get(memory,quest,0x1c4,total,"completion Quest lifetime total",e))
            return fail(e,e.empty()?"Quest completion global member class/membership differs":e);
        const auto found=expected.find(quest);
        if(found==expected.end()||found->second!=total)return fail(e,"Quest completion global member absent or lifetime total changed");
        value.quests.push_back(quest);if(total>0)++value.derived;
    }
    uintptr_t manager=0,data=0;int32_t count=-1,capacity=-1,completed=-1;
    if(!get(memory,image,0x13df410,manager,"completion manager reread",e)||
       !get(memory,value.manager,0x30,data,"completion array reread",e)||
       !get(memory,value.manager,0x38,count,"completion count reread",e)||
       !get(memory,value.manager,0x3c,capacity,"completion capacity reread",e)||
       !get(memory,value.manager,0x48,completed,"completion aggregate reread",e))return false;
    if(manager!=value.manager||data!=value.data||count!=census.manager_count||capacity!=value.capacity||completed!=value.completed)
        return fail(e,"Quest completion global owner changed during read");
    out=std::move(value);
    if(require_count&&out.completed!=out.derived)return fail(e,"Quest completion aggregate actual="+
        std::to_string(out.completed)+" derived="+std::to_string(out.derived)+" global_quests="+std::to_string(out.quests.size()));
    e.clear();return true;
}
template<class Read,class Guard,class Compare>
bool finish(const Snapshot& before,int32_t delta,Read&& read,Guard&& guard,Compare&& compare,std::string& e){
    if(!guard(e))return false;
    Snapshot after;if(!read(after,e,false))return false;
    const auto expected=int64_t(before.completed)+delta;
    if(before.manager!=after.manager||before.data!=after.data||before.capacity!=after.capacity||before.quests!=after.quests)
        return fail(e,"Quest completion CAS owner/array/order differs before_count="+std::to_string(before.quests.size())+
            " after_count="+std::to_string(after.quests.size()));
    if(before.completed!=before.derived)
        return fail(e,"Quest completion CAS before aggregate actual="+std::to_string(before.completed)+
            " derived="+std::to_string(before.derived));
    if(after.completed!=before.completed)
        return fail(e,"Quest completion CAS unexpected aggregate change before CAS expected="+std::to_string(before.completed)+
            " actual="+std::to_string(after.completed));
    if(expected<0||expected>int64_t(after.quests.size())||after.derived!=expected)
        return fail(e,"Quest completion CAS derived transition expected="+std::to_string(expected)+
            " actual="+std::to_string(after.derived)+" delta="+std::to_string(delta)+
            " before_completed="+std::to_string(before.completed)+" global_quests="+std::to_string(after.quests.size()));
    if(!guard(e)||!compare(before.manager+0x48,before.completed,int32_t(expected),e))return false;
    Snapshot confirmed;
    if(!read(confirmed,e,true))return false;
    if(confirmed.manager!=after.manager||confirmed.data!=after.data||confirmed.capacity!=after.capacity||
       confirmed.quests!=after.quests||confirmed.completed!=expected||confirmed.derived!=expected)
        return fail(e,"Quest completion CAS readback differs; no replay");
    return true;
}
}
