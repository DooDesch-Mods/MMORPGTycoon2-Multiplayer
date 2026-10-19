#include "npc_quest_authoring_fixture.h"
#include "npc_quest_authoring_read.h"
#include <iostream>
namespace qr=mt2mp::game::monster_quest_retirement;
namespace q=mt2mp::npc_quest_records;
namespace read=mt2mp::game::npc_quest_authoring_read;
qr::Witness world(){qr::Witness w;w.manager=100;w.completed=1;
 for(size_t i=0;i<3;++i){const uintptr_t quest=1000+i*100,npc=i<2?5000:6000,destination=7000+i*100;
  auto record=intent().before;record.sequence_id=i<2?int32_t(i):0;if(i)record.name="Survivor"+std::to_string(i);
  const auto key=i<2?owner_key:std::string("action:66666666-6666-4666-8666-666666666666");
  w.quests.push_back(quest);w.rows.push_back({key,record});w.links.push_back({destination,9000+i*10,destination+0x20,npc,npc+50,npc+0x20});
  auto& owner=w.owners[npc];owner.pointer=npc;owner.uid=npc+0x100000000ull;owner.key=key;
  owner.state.records.push_back(record);owner.state.local.push_back({quest,npc,destination,true,true,true,true,q::TaskStatus::Absent});owner.destination_uids.push_back(destination+0x100000000ull);
 }return w;}
qr::Witness changed(qr::Witness w){const auto value=after(before()).record;w.rows[0].record=value;w.links[0][0]=8000;w.links[0][1]=8888;w.links[0][2]=8032;
 auto& owner=w.owners.at(5000);owner.state.records[0]=value;owner.state.local[0].destination=8000;owner.state.local[0].task=q::TaskStatus::Pending;owner.destination_uids[0]=0x100001f40ull;return w;}
int main(){try{unsigned n=0;std::string e;const auto old=world(),now=changed(old);
 auto check=[&](const qr::Witness& v){return read::transition(old,v,1000,8000,0x100001f40ull,intent(),e);};
 need(check(now),e);++n;
 auto invalid=[&](auto mutate){auto v=now;mutate(v);need(!check(v),"changed unrelated Quest or identity accepted");++n;};
 invalid([](auto& v){std::swap(v.quests[0],v.quests[1]);});invalid([](auto& v){++v.completed;});
 invalid([](auto& v){v.rows[1].record.name="Changed survivor";});invalid([](auto& v){++v.links[1][1];});
 invalid([](auto& v){++v.links[0][3];});invalid([](auto& v){v.links[0][0]=7100;});
 invalid([](auto& v){++v.owners.at(5000).uid;});invalid([](auto& v){++v.owners.at(5000).destination_uids[0];});
 invalid([](auto& v){v.owners.at(5000).state.local[0].task=q::TaskStatus::Absent;});
 invalid([](auto& v){v.owners.at(5000).state.local[1].task=q::TaskStatus::Pending;});
 invalid([](auto& v){v.owners.at(6000).state.records[0].description="Changed other owner";});
 invalid([](auto& v){v.owners.at(6000).state.local[0].quest=1201;});
 auto finished=now;finished.owners.at(5000).state.local[0].task=q::TaskStatus::FinishedWaitingTick;need(check(finished),e);++n;
 std::cout<<n<<" actual Quest selected/unaffected owner, row, link and task cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
