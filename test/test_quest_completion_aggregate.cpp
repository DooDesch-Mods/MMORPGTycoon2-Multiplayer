#include "live_host_state/day_rollover/economy/state_difference.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
namespace quest_state=mt2mp::subscriber_quest_state;
using Pin=mt2mp::game::live_host_state::Pin;
#include "npc_daily_guard_cases.inc"
namespace aggregate_cases {
using namespace daily_guard_test;
constexpr uintptr_t manager=0x50000,table=0x51000,entries=0x52000;
void members(const std::vector<uintptr_t>& quests,int32_t completed){
    put(image+0x13df410,manager);put(manager,image+0x17020b0);put(manager+0x18,table);
    put(manager+0x20,int32_t(2));put(manager+0x24,int32_t(31));put(manager+0x30,entries);
    put(manager+0x38,int32_t(quests.size()));put(manager+0x3c,int32_t(8));put(manager+0x48,completed);
    std::array<uintptr_t,2> heads{};
    for(size_t i=0;i<quests.size();++i){const auto ptr=quests[i],node=uintptr_t(0x53000+i*32);put(entries+i*8,ptr);
        const auto bucket=(uint32_t(ptr)*0x9e3779ffu)>>31;put(node,ptr);put(node+8,uint32_t(ptr));put(node+16,heads[bucket]);heads[bucket]=node;}
    for(size_t i=0;i<2;++i)put(table+i*24+16,heads[i]);
}
std::pair<quest_state::Npc,quest_state::Npc> setup_crossing(){
    auto [old,target]=setup(2);fixture.completion_enabled=true;
    old.quests[1].completed={};target.quests[1].completed.today=11;target.quests[1].completed.total=11;
    daily(quest_address+0x1000,0x170,old.quests[1].completed,false);
    members({quest_address+0x1000,quest_address},1);
    return {old,target};
}
void run(){
    std::string e;Pin p;p.image=image;p.clock=clock_address;
    auto [old,target]=setup_crossing();npc_quest_completion::Snapshot before,after;
    require(quest_completion_current(p,before,e),"actual complete aggregate initial read");
    require(before.completed==1&&before.derived==1&&before.quests.size()==2,"actual first-completion baseline differs");
    require(execute(old,target,false,e),e.c_str());require(quest_completion_current(p,after,e),e.c_str());
    require(after.completed==2&&after.derived==2&&after.quests==before.quests,"first completion count/order not adopted");
    unsigned manager_cas=0;for(const auto& event:fixture.trace)if(event.operation=="cas"&&event.address==manager+0x48){++manager_cas;require(event.before==1&&event.after==2,"aggregate CAS old/target differ");}
    require(manager_cas==1&&fixture.enters==1&&fixture.leaves==1,"aggregate was replayed or left SaveScope");
    for(int mode=0;mode<7;++mode){std::tie(old,target)=setup_crossing();
        if(mode==0)members({quest_address},1);
        if(mode==1)members({quest_address,quest_address},1);
        if(mode==2)put(manager+0x48,int32_t(0));
        if(mode==3)put(manager,image+0x17020b1);
        if(mode==4)fixture.fail_completion_cas=true;
        if(mode==5)fixture.drift_enter=true;
        if(mode==6)put(table+16,uintptr_t(0)),put(table+40,uintptr_t(0));
        e.clear();require(!execute(old,target,false,e)&&!e.empty(),"invalid aggregate/owner/CAS accepted");
        require(fixture.leaves==(mode==5?0u:1u),"SaveScope exit did not preserve exact owner guard");
        if(mode==4)require(e.find("original completion CAS failed")!=std::string::npos,"original CAS cause lost");
    }
    std::tie(old,target)=setup_crossing();fixture.owner.quests.pop_back();
    require(!quest_completion_current(p,after,e),"uncovered global Quest accepted");
    std::tie(old,target)=setup_crossing();put(quest_address+0x1c4,int32_t(99));
    require(quest_completion_current(p,after,e)&&after.derived==1,"multiple completions incorrectly increase active count");
    // The finish predicate cannot compensate for a changed array or an unplanned lifetime-total transition.
    auto invalid=before;invalid.derived=2;invalid.quests.pop_back();bool called=false;
    require(!npc_quest_completion::finish(before,1,[&](auto& out,std::string&,bool){out=invalid;return true;},
        [](std::string&){return true;},[&](uintptr_t,int32_t,int32_t,std::string&){called=true;return true;},e)&&!called,"partial membership committed");
    require(e.find("owner/array/order differs before_count=2 after_count=1")!=std::string::npos,"membership cause lost");
    for(int mode=0;mode<3;++mode){
        auto prior=before;invalid=before;invalid.derived=2;called=false;e.clear();
        if(mode==0)prior.derived=0;
        if(mode==1)invalid.completed=2;
        if(mode==2)invalid.derived=1;
        require(!npc_quest_completion::finish(prior,1,[&](auto& out,std::string&,bool){out=invalid;return true;},
            [](std::string&){return true;},[&](uintptr_t,int32_t,int32_t,std::string&){called=true;return true;},e)&&!called,"invalid aggregate transition committed");
        const char* expected_cause=mode==0?"before aggregate actual=1 derived=0":
            mode==1?"unexpected aggregate change before CAS expected=1 actual=2":
            "derived transition expected=2 actual=1 delta=1 before_completed=1 global_quests=2";
        require(e.find(expected_cause)!=std::string::npos,"aggregate measured cause differs");
    }
}
}
int main()try{aggregate_cases::run();std::cout<<daily_guard_test::assertions<<" actual DailyCount/SaveScope/global completed aggregate cases passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
