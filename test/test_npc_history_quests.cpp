#include "npc_history_quests.h"
#include <iostream>
#include <stdexcept>
namespace npc_history_quest_fixture {
namespace qr=mt2mp::game::monster_quest_retirement;
namespace actual=mt2mp::game::npc_history_quests;
unsigned checks=0;
void need(bool ok,const std::string& error){++checks;if(!ok)throw std::runtime_error(error);}
qr::Owner owner(uintptr_t pointer,const std::string& key){qr::Owner value;value.pointer=pointer;value.uid=0x100000002;value.key=key;return value;}
void run(){
    std::string error;qr::Witness original,before,after;original.manager=before.manager=after.manager=1000;
    const std::string key="action:11111111-1111-4111-8111-111111111111";
    original.owners.emplace(100,owner(100,key));
    auto& prior=original.owners.at(100);mt2mp::npc_quest_records::Record record;record.sequence_id=0;record.type=1;record.name="Visit";record.action=2;record.destination_key="zone:a";
    prior.state.records={record};prior.destination_uids={99};mt2mp::npc_quest_records::Local old;old.quest=200;old.owner=100;old.destination=900;prior.state.local={old};
    original.quests={200};original.links={{{900,908,932,100,108,132}}};original.rows={{key,record}};
    after.owners.emplace(300,owner(300,key));auto& current=after.owners.at(300);current.state.records={record};
    current.state.records[0].xp_reward=100;current.state.records[0].accepted.today=0;
    auto restored=old;restored.quest=400;restored.owner=300;current.state.local={restored};current.destination_uids={99};
    after.quests={400};after.links={{{900,908,932,300,308,332}}};after.rows={{key,current.state.records[0]}};
    need(actual::restored(original,before,after,key,100,300,0x100000002,{0x100000002},error),"fresh original full UID owner/Quest restore failed: "+error);
    need(actual::restored(original,before,after,key,100,300,0x100000002,{0x100000002,0x100000002},error),"multiple retained snapshots of one original owner rejected: "+error);
    for(unsigned mutation=0;mutation<6;++mutation){auto bad=after;
        if(mutation==0)bad.owners.at(300).uid^=1ull<<32;if(mutation==1)bad.owners.at(300).key="foreign";
        if(mutation==2)bad.owners.at(300).state.records[0].destination_key="zone:foreign";
        if(mutation==3)bad.owners.at(300).state.local[0].owner=100;
        if(mutation==4)bad.quests.push_back(400);if(mutation==5)bad.completed=1;
        need(!actual::restored(original,before,bad,key,100,300,0x100000002,{0x100000002},error)&&!error.empty(),"invalid complete Quest restoration accepted");
    }
    const auto seed=actual::seed(original,key,300,0x100000002);
    need(!seed.owners.contains(100)&&seed.owners.at(300).pointer==300&&seed.owners.at(300).state.records.empty(),"seed retained freed owner pointer or reused stale records");
    const auto retired_seed=actual::seed(original,key,0,0x100000002);need(retired_seed.owners.empty(),"retired owner is still read by previous seed");
    qr::Witness empty_original,empty_before,empty_after;empty_original.manager=empty_before.manager=empty_after.manager=1000;
    empty_after.owners.emplace(300,owner(300,key));
    need(actual::restored(empty_original,empty_before,empty_after,key,100,300,0x100000002,{0x100000002},error),"original empty Quest owner restore failed: "+error);
    empty_after.owners.clear();need(actual::restored(empty_original,empty_before,empty_after,key,100,300,0x100000002,{0x100000002},error,false),"non-Quest NPC empty true SetNPCQuests restore failed: "+error);
    auto foreign=before;foreign.owners.emplace(300,owner(300,"foreign"));
    need(!actual::restored(original,foreign,after,key,100,300,0x100000002,{0x100000002},error),"restored pointer collision accepted");
    auto selected=original;selected.removed={200};auto retired=before;
    need(actual::removed(selected,retired,key,{},error),"complete original own-Quest retirement rejected: "+error);
    need(!actual::removed(selected,original,key,{},error),"retired NPC Quest owner retained");
    auto malformed=retired;malformed.quests={200};need(!actual::removed(selected,malformed,key,{},error),"retirement accepted incomplete global Quest rows");
}
}
int main()try{npc_history_quest_fixture::run();std::cout<<npc_history_quest_fixture::checks<<" NPC original Quest restoration and freed-owner seed checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
