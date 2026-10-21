#include "quest_cascade_read.h"
#include "npc_quest_cascade.h"
#include "npc_remove_effect.h"
#include <iostream>
#include <stdexcept>
namespace {
namespace cascade=mt2mp::game::quest_cascade_read;
namespace qr=mt2mp::game::monster_quest_retirement;
using json=nlohmann::json;
int checks=0;
void expect(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
qr::Witness witness(){
    qr::Witness out;out.manager=1000;out.completed=2;
    const auto add=[&](uintptr_t quest,uintptr_t owner,uintptr_t destination,const char* key,uint64_t uid,bool completed){
        auto& target=out.owners[owner];target.pointer=owner;target.uid=owner+1000;target.key="npc:"+std::to_string(owner);
        mt2mp::npc_quest_records::Record record;record.destination_key=key;record.sequence_id=int32_t(target.state.records.size());
        record.completed.total=completed?1:0;target.state.records.push_back(record);target.destination_uids.push_back(uid);
        mt2mp::npc_quest_records::Local local;local.quest=quest;local.owner=owner;local.destination=destination;
        local.task=mt2mp::npc_quest_records::TaskStatus::Absent;target.state.local.push_back(local);
        out.quests.push_back(quest);out.links.push_back({destination,destination+8,destination+0x20,owner,owner+8,owner+0x20});
        out.rows.push_back({target.key,record});
    };
    add(11,101,301,"zone:a",901,true);add(12,101,302,"zone:b",902,false);
    add(13,102,303,"zone:c",903,true);add(14,101,304,"zone:d",904,false);return out;
}
json progress(const char* key){return {{"npc",{{"state",2},{"key","npc:101"}}},{"quest",{{"state",2},{"key",key}}},
    {"completed",4},{"monsters_defeated",3},{"visited",true},{"action_performed",false},{"complete",true}};}
json references(){return json::array({{{"id",40},{"online",true},{"gold",100},{"xp_bits",123},{"level",3},{"quests_completed",4},
    {"progress",json::array({progress("npc:101/quest/0"),progress("npc:101/quest/1"),progress("npc:102/quest/0"),progress("npc:101/quest/2")})}},
    {{"id",41},{"online",false},{"progress",json::array()}}});}
json retired(const json& before){auto after=before;
    for(const size_t slot:{size_t(0),size_t(2)}){after[0]["progress"][slot]["quest"]={{"state",1},{"key",""}};after[0]["progress"][slot]["complete"]=false;}
    after[0]["progress"][1]["quest"]["key"]="npc:101/quest/0";
    after[0]["progress"][3]["quest"]["key"]="npc:101/quest/1";return after;
}
void run(){std::string error;auto before=witness();
    expect(cascade::select(before,{{"zone:a",901},{"zone:c",903}},error),"multiple exact destinations");
    expect(before.removed==std::vector<uintptr_t>{11,13},"original global selection order");
    expect(qr::inverse_owners(before)==std::vector<uintptr_t>{101,102},"include owner losing final Quest");
    auto bad=witness();bad.removed={14};
    expect(!cascade::select(bad,{{"zone:a",999}},error)&&error.find("full UID")!=std::string::npos,"canonical key must match full UID");
    expect(bad.removed==std::vector<uintptr_t>{14},"failed selection does not replace retained state");
    bad=witness();expect(!cascade::select(bad,{{"zone:a",901},{"zone:c",901}},error),"reject aliased destination UIDs");
    expect(!cascade::select(bad,{{"",901}},error),"reject empty destination key");
    expect(!cascade::select(bad,{{"zone:a",0}},error),"reject empty destination UID");
    bad=witness();bad.rows.pop_back();expect(!cascade::select(bad,{{"zone:a",901}},error),"reject incomplete global witness");
    bad=witness();bad.owners.erase(101);expect(!cascade::select(bad,{{"zone:a",901}},error),"reject absent owner");
    bad=witness();bad.owners.at(101).state.local[0].destination=777;
    expect(!cascade::select(bad,{{"zone:a",901}},error),"reject changed owner destination");
    bad=witness();expect(cascade::select(bad,{{"zone:unreferenced",999}},error)&&bad.removed.empty(),"unreferenced removed actor needs no synthetic Quest");
    auto after=before;after.completed=0;after.quests={14,12};after.rows={before.rows[3],before.rows[1]};after.links={before.links[3],before.links[1]};after.removed.clear();
    auto& owner=after.owners.at(101);owner.state.records={before.owners.at(101).state.records[1],before.owners.at(101).state.records[2]};
    owner.state.records[0].sequence_id=0;owner.state.records[1].sequence_id=1;
    owner.state.local={before.owners.at(101).state.local[1],before.owners.at(101).state.local[2]};owner.destination_uids={902,904};
    after.owners.at(102).state={};after.owners.at(102).destination_uids.clear();
    expect(qr::survivors(before,after,error),"native swap-last plus complete owner-slot retirement");
    auto changed=after;changed.owners.erase(102);expect(!qr::survivors(before,changed,error),"empty former owner still needs readback");
    changed=after;changed.completed=1;expect(!qr::survivors(before,changed,error),"completed aggregate must match removed records");
    changed=after;std::swap(changed.quests[0],changed.quests[1]);expect(!qr::survivors(before,changed,error),"global order cannot be sorted for comparison");
    const auto old=references(),now=retired(old);
    expect(cascade::references(before,old,now,error),"active references retire and survivors shift together");
    auto actual=now;actual[0]["progress"][0]["quest"]["state"]=0;
    expect(cascade::references(before,old,actual,error),"native empty weak link permitted alongside expired");
    actual=now;actual[0]["progress"][0]["quest"]={{"state",2},{"key","npc:101/quest/0"}};
    expect(!cascade::references(before,old,actual,error),"retired Quest cannot stay live");
    actual=now;actual[0]["progress"][3]["quest"]["key"]="npc:101/quest/2";
    expect(!cascade::references(before,old,actual,error),"multiple slot retirement must renumber survivors");
    actual=now;actual[0]["gold"]=101;expect(!cascade::references(before,old,actual,error),"retirement cannot award gold");
    actual=now;actual[0]["quests_completed"]=5;expect(!cascade::references(before,old,actual,error),"retirement cannot increment completed count");
    actual=now;actual[0]["progress"][2]["npc"]["key"]="npc:102";
    expect(!cascade::references(before,old,actual,error),"unrelated NPC weak link must stay exact");
    actual=now;actual[0]["progress"][0]["visited"]=false;expect(!cascade::references(before,old,actual,error),"retired progress keeps other fields");
    actual=now;actual[0]["progress"].erase(0);expect(!cascade::references(before,old,actual,error),"progress entry is not silently removed");
    actual=now;actual.erase(1);expect(!cascade::references(before,old,actual,error),"offline Subscribers remain in cohort");
    actual=now;actual[0]["progress"][0]["quest"].erase("state");
    expect(!cascade::references(before,old,actual,error)&&error.find("payload")!=std::string::npos,"malformed weaklink retains JSON cause");
    bad=before;bad.removed.push_back(11);expect(!cascade::references(bad,old,now,error),"no repeated retirement");
    bad=before;bad.removed.push_back(99);expect(!cascade::references(bad,old,now,error),"no foreign retired Quest");
}
void npc_retirement(){
    namespace npc=mt2mp::game::npc_quest_cascade;
    auto before=witness();std::string error;
    for(size_t index:{size_t(1),size_t(2)}){
        const auto pointer=before.quests[index];const auto owner=before.links[index][3];
        auto& record=before.rows[index].record;record.destination_key="npc:101";
        before.links[index][0]=101;
        auto& local=before.owners.at(owner);
        for(size_t slot=0;slot<local.state.local.size();++slot)if(local.state.local[slot].quest==pointer){
            local.state.local[slot].destination=101;local.state.records[slot]=record;local.destination_uids[slot]=1101;
        }
    }
    expect(npc::select(before,101,1101,"npc:101",error),"NPC selects incoming and owned Quests");
    expect(before.removed==std::vector<uintptr_t>{12,13,11,14},"NPC original incoming-before-owned retirement order");
    const auto seed=npc::survivor_seed(before,"npc:101");
    expect(!seed.owners.contains(101)&&seed.owners.contains(102),"post-destruction seed never rereads removed owner");
    auto after=seed;after.manager=before.manager;after.completed=0;
    after.owners.at(102).state={};after.owners.at(102).destination_uids.clear();
    expect(npc::survivors(before,after,"npc:101",error),"NPC complete global retirement retains empty survivor owner");
    auto bad=after;bad.owners.emplace(101,before.owners.at(101));
    expect(!npc::survivors(before,bad,"npc:101",error),"removed NPC cannot remain an owner");
    bad=before;expect(!npc::select(bad,101,1102,"npc:101",error),"NPC canonical target requires full UID");
    auto old=references();old[0]["progress"][2]["npc"]["key"]="npc:102";
    auto current=old;
    for(size_t slot=0;slot<4;++slot){
        current[0]["progress"][slot]["quest"]={{"state",1},{"key",""}};
        current[0]["progress"][slot]["complete"]=false;
        if(slot!=2)current[0]["progress"][slot]["npc"]={{"state",1},{"key",""}};
    }
    expect(npc::references(before,old,current,"npc:101",error),"NPC and Quest weak lifetimes expire together");
    auto changed=current;changed[0]["progress"][1]["npc"]=old[0]["progress"][1]["npc"];
    expect(!npc::references(before,old,changed,"npc:101",error),"deleted NPC cannot remain Live in progress");
    changed=current;changed[0]["progress"][2]["npc"]={{"state",1},{"key",""}};
    expect(!npc::references(before,old,changed,"npc:101",error),"another NPC reference cannot expire");
    changed=current;changed[0]["gold"]=101;
    expect(!npc::references(before,old,changed,"npc:101",error),"NPC deletion cannot award gold");
    changed=current;changed[0]["progress"].erase(0);
    expect(!npc::references(before,old,changed,"npc:101",error),"NPC deletion cannot silently drop progress");
    changed=current;changed[0]["progress"][0]["npc"].erase("state");
    expect(!npc::references(before,old,changed,"npc:101",error)&&error.find("payload")!=std::string::npos,"NPC malformed weak lifetime reports original JSON cause");
}
void npc_survivor_census(){
    namespace effect=mt2mp::game::npc_probe::remove_effect;
    mt2mp::npc_census::State before,after;before.manager=after.manager=1000;
    mt2mp::npc_census::Row removed,owner,unrelated;removed.local_uid=1;removed.type=1;
    owner.local_uid=2;owner.type=0;owner.quest_count=3;owner.name="Surviving owner";
    unrelated.local_uid=3;unrelated.type=2;unrelated.quest_count=0;
    before.rows={removed,owner,unrelated};after.rows={unrelated,owner};after.rows[1].quest_count=1;
    const std::map<uint64_t,int32_t> counts{{2,1}};std::string error;
    expect(effect::removed_one(before,after,1,error,&counts),"NPC removal admits only complete witness-derived survivor Quest count");
    expect(!effect::removed_one(before,after,1,error),"NPC removal rejects changed Quest count without complete proof");
    auto bad=after;bad.rows[1].name="Changed";
    expect(!effect::removed_one(before,bad,1,error,&counts),"Quest cascade cannot rename a surviving NPC");
    bad=after;bad.rows[1].position[0]=1;
    expect(!effect::removed_one(before,bad,1,error,&counts),"Quest cascade cannot move a surviving NPC");
    bad=after;bad.rows[1].quest_count=2;
    expect(!effect::removed_one(before,bad,1,error,&counts),"Quest cascade requires exact witnessed survivor count");
    bad=after;bad.rows[0].quest_count=1;
    expect(!effect::removed_one(before,bad,1,error,&counts),"Quest cascade cannot change an unrelated NPC count");
}
void npc_owned_destruction_order(){
    namespace npc=mt2mp::game::npc_quest_cascade;
    auto before=witness();std::string error;
    auto row=before.rows[2];row.record.sequence_id=1;row.record.completed.total=0;
    auto local=before.owners.at(102).state.local.front();local.quest=15;
    before.owners.at(102).state.local.push_back(local);before.owners.at(102).state.records.push_back(row.record);
    before.owners.at(102).destination_uids.push_back(before.owners.at(102).destination_uids.front());
    before.quests.push_back(15);before.rows.push_back(row);before.links.push_back(before.links[2]);
    const auto original=before;
    const size_t order[]{1,3,2,0,4};
    for(size_t i=0;i<5;++i){before.quests[i]=original.quests[order[i]];before.rows[i]=original.rows[order[i]];before.links[i]=original.links[order[i]];}
    expect(npc::select(before,101,1101,"npc:101",error),"NPC destructor binds its complete owned list");
    expect(before.removed==std::vector<uintptr_t>{11,14,12},"NPC shrinking-loop then member-array destruction order");
    auto after=npc::survivor_seed(before,"npc:101");after.manager=before.manager;after.completed=1;
    after.quests={13,15};after.rows={original.rows[2],original.rows[4]};after.links={original.links[2],original.links[4]};
    expect(npc::survivors(before,after,"npc:101",error),"NPC destructor preserves actual unrelated global swap-last order");
    auto bad=after;std::swap(bad.quests[0],bad.quests[1]);
    expect(!npc::survivors(before,bad,"npc:101",error),"Forward owned deletion cannot replace original destructor order");
}
}
int main(){try{run();npc_retirement();npc_survivor_census();npc_owned_destruction_order();std::cout<<checks<<" Quest cascade checks passed\n";return 0;}
catch(const std::exception& exception){std::cerr<<"Quest cascade: "<<exception.what()<<'\n';return 1;}}
