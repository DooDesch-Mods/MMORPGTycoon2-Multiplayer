#include "quest_history_read.h"
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;
namespace q=mt2mp::game::monster_quest_retirement;
namespace history=mt2mp::game::duplicate_quest_history;
namespace read=mt2mp::game::quest_history_read;
void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
void add(q::Witness& value,uintptr_t owner,uint64_t uid,const std::string& key,uintptr_t quest,uintptr_t target,const std::string& title,int32_t completed=0){
    auto& current=value.owners[owner];current.pointer=owner;current.uid=uid;current.key=key;
    mt2mp::npc_quest_records::Record row;row.sequence_id=int32_t(current.state.records.size());row.type=1;row.action=2;
    row.name=title;row.destination_key="target:"+std::to_string(target);row.monsters_to_kill=3;row.completed.total=completed;
    current.state.records.push_back(row);current.destination_uids.push_back(target);
    mt2mp::npc_quest_records::Local local;local.quest=quest;local.owner=owner;local.destination=target;local.task=mt2mp::npc_quest_records::TaskStatus::Pending;
    current.state.local.push_back(local);value.quests.push_back(quest);value.rows.push_back({key,row});value.links.push_back({target,target+1,target+2,owner,owner+1,owner+2});
    if(completed>0)++value.completed;
}
int main()try{
    unsigned cases=0;std::string error;const std::string owner="owner:a";
    q::Witness before;before.manager=0x100;
    add(before,10,110,owner,101,1000,"Current",4);add(before,20,120,"owner:b",103,2000,"Unrelated");add(before,10,110,owner,102,2000,"Second");
    auto original=before;original.owners.at(10).state.records.resize(1);original.owners.at(10).state.local.resize(1);original.owners.at(10).destination_uids.resize(1);
    original.owners.at(10).state.records[0].name="Restored";
    q::Witness after;after.manager=before.manager;add(after,20,120,"owner:b",103,2000,"Unrelated");add(after,10,110,owner,104,1000,"Restored");
    need(history::restored(original,before,after,{10},error),error);++cases;
    auto reject=[&](auto change){auto bad=after;change(bad);need(!history::restored(original,before,bad,{10},error),"invalid whole Quest restore accepted");++cases;};
    reject([](auto& bad){std::swap(bad.quests[0],bad.quests[1]);});
    reject([](auto& bad){bad.owners.at(10).state.records[0].name="Wrong authored title";});
    reject([](auto& bad){bad.owners.at(10).uid++;});
    reject([](auto& bad){bad.owners.at(20).state.records[0].name="Changed unrelated quest";});
    reject([](auto& bad){bad.links[0][1]++;});
    reject([](auto& bad){bad.completed++;});
    reject([](auto& bad){bad.owners.at(20).state.local[0].task=mt2mp::npc_quest_records::TaskStatus::Absent;});
    auto no_quests=original;no_quests.owners.at(10).state={};no_quests.owners.at(10).destination_uids.clear();
    auto empty=after;empty.quests.pop_back();empty.rows.pop_back();empty.links.pop_back();empty.owners.at(10).state={};empty.owners.at(10).destination_uids.clear();
    need(history::restored(no_quests,before,empty,{10},error),"empty NPC list did not retain its current owner");++cases;
    json progress=json::array();
    for(const auto& key:{owner+"/quest/0",owner+"/quest/1",std::string("owner:b/quest/0")})
        progress.push_back({{"npc",{{"state",2},{"key",key.starts_with(owner)?owner:"owner:b"}}},{"quest",{{"state",2},{"key",key}}},{"complete",true},{"completed",2}});
    json refs=json::array({{{"id",17},{"gold",14},{"xp_bits",20},{"quests_completed",4},{"progress",progress}}});
    for(int expired:{0,1}){
        auto current=refs;for(size_t i=0;i<2;++i){current[0]["progress"][i]["quest"]={{"state",expired},{"key",""}};current[0]["progress"][i]["complete"]=false;}
        need(read::references(refs,current,owner,error),error);++cases;
        auto invalid=[&](auto change){auto bad=current;change(bad);need(!read::references(refs,bad,owner,error),"invalid replaced Quest reference accepted");++cases;};
        invalid([&](auto& bad){bad[0]["progress"][0]["quest"]={{"state",2},{"key",owner+"/quest/0"}};});
        invalid([](auto& bad){bad[0]["progress"][1]["complete"]=true;});
        invalid([](auto& bad){bad[0]["gold"]=15;});
        invalid([](auto& bad){bad[0]["progress"][0]["completed"]=3;});
        invalid([](auto& bad){bad[0]["progress"][0]["npc"]["state"]=0;});
        invalid([](auto& bad){bad[0]["progress"][2]["quest"]={{"state",1},{"key",""}};});
    }
    need(!read::references(refs,refs,owner,error),"old Live references survived whole-list replacement");++cases;
    std::cout<<cases<<" Quest original global order, ownership and visitor reference cases passed\n";return 0;
}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}
