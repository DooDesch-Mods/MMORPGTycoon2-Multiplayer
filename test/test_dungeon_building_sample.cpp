#include "dungeon_building_sample.h"
#include <iostream>
#include <stdexcept>
namespace dungeon_building_sample_fixture {
using namespace mt2mp::game;
void need(bool ok,const char* cause){if(!ok)throw std::runtime_error(cause);}
const std::string exterior="action:11111111-1111-4111-8111-111111111111";
dungeon_building_sample::Scene scene(const std::string& suffix){dungeon_building_sample::Scene row;
    row.authored.key=exterior+suffix;row.authored.dungeon=exterior;
    row.authored.canvas_sha=std::string(64,'a');row.authored.design_sha=std::string(64,'b');
    row.zones=std::string(64,'c');row.buildings=std::string(64,'d');return row;}
struct Snapshot {int value=0;};
struct CapturePort {
    int reads=0;bool first_fail=false,last_fail=false,drift=false;
    bool read(Snapshot& out,std::string& error,const Snapshot* previous){++reads;
        if((reads==1&&first_fail)||(reads==2&&last_fail)){error=reads==1?"exact first ReadMemory cause":"exact second owner cause";return false;}
        if(reads==2)need(previous&&previous->value==12,"Retained previous sample not passed to final read");
        out.value=12+(drift&&reads==2?1:0);return true;}
    bool same(const Snapshot& before,const Snapshot& after,std::string& error){if(before.value!=after.value){error="exact current owner drift";return false;}return true;}
};
struct SeedPort {
    std::string order;int failure=0;
    bool before(std::string& error){order+='b';if(failure==1){error="exact pending/current-world cause";return false;}return true;}
    bool write(const std::array<uint64_t,4>& words,std::string& error){order+='w';need(words==std::array<uint64_t,4>{1,2,3,4},"Host RNG words changed");
        if(failure==2){error="exact seed SEH";return false;}return true;}
    bool after(const std::array<uint64_t,4>&,std::string& error){order+='a';if(failure==3){error="exact seed readback cause";return false;}return true;}
};
}
int main()try{using namespace dungeon_building_sample_fixture;unsigned checks=0;std::string error;
    const auto check=[&](bool value,const char* cause){++checks;need(value,cause);};
    std::map<std::string,std::string> output,config;
    check(dungeon_building_sample::compose({},config,output,error)&&output.empty(),"Zero Dungeons before first native exterior must be valid");
    config[exterior]=std::string(64,'e');std::vector rows{scene("/template"),scene("/instance/7")};
    check(dungeon_building_sample::compose(rows,config,output,error)&&output.size()==1,"Complete template and nonordinal Instance owner failed");
    const auto complete=output;
    auto changed=rows;changed[1].buildings=std::string(64,'f');
    check(dungeon_building_sample::compose(changed,config,output,error)&&output!=complete,"Contained Building authored change omitted");
    changed=rows;changed[1].zones=std::string(64,'f');
    check(dungeon_building_sample::compose(changed,config,output,error)&&output!=complete,"Complete Zone authored values omitted");
    changed=rows;changed[0].authored.design_sha=changed[1].authored.design_sha=std::string(64,'f');
    check(dungeon_building_sample::compose(changed,config,output,error)&&output!=complete,"Original Grid/Layout/Plan authored value omitted");
    changed=rows;changed[0].authored.canvas_sha=changed[1].authored.canvas_sha=std::string(64,'f');
    check(dungeon_building_sample::compose(changed,config,output,error)&&output!=complete,"Original Canvas authored value omitted");
    changed=rows;changed.push_back(rows[0]);check(!dungeon_building_sample::compose(changed,config,output,error),"Duplicate current scene accepted");
    changed={rows[1]};check(!dungeon_building_sample::compose(changed,config,output,error),"Instance without actual template accepted");
    changed=rows;changed[1].zones="missing";check(!dungeon_building_sample::compose(changed,config,output,error),"Missing full Zone proof accepted");
    changed=rows;changed[1].buildings.clear();check(!dungeon_building_sample::compose(changed,config,output,error),"Missing full Building proof accepted");
    changed=rows;changed[1].authored.dungeon="foreign";check(!dungeon_building_sample::compose(changed,config,output,error),"Foreign original Dungeon owner accepted");
    auto extra=config;extra["action:22222222-2222-4222-8222-222222222222"]=std::string(64,'e');
    check(!dungeon_building_sample::compose(rows,extra,output,error),"Unobserved exterior settings accepted");
    check(!dungeon_building_sample::compose({},config,output,error),"Missing original Dungeon scenes accepted");
    monster_quest_retirement::Witness witness;monster_quest_retirement::Owner owner;
    owner.pointer=0x1000;owner.uid=77;owner.key="building-npc:"+exterior;
    mt2mp::npc_quest_records::Record record;record.sequence_id=0;record.name="Native authored quest";
    mt2mp::npc_quest_records::Local native;native.quest=0x2000;native.owner=owner.pointer;native.destination=0x3000;
    native.npc_list_member=native.owner_weak_confirmed=native.destination_weak_confirmed=native.manager_member=true;
    owner.state.records={record};owner.state.local={native};witness.owners.emplace(owner.pointer,owner);
    witness.quests={native.quest};witness.links={std::array<uintptr_t,6>{native.destination,0x4000,0x3020,owner.pointer,0x5000,0x1020}};
    witness.rows={{owner.key,record}};std::string quest_sha,changed_sha;
    check(dungeon_building_sample::quests(witness,quest_sha,error),"Complete authored Quest witness rejected");
    auto tasks=witness;tasks.owners.at(owner.pointer).state.local[0].task=mt2mp::npc_quest_records::TaskStatus::FinishedWaitingTick;
    check(dungeon_building_sample::quests(tasks,changed_sha,error)&&changed_sha==quest_sha,"Private original task cleanup became shared authored identity");
    auto authored_quest=witness;authored_quest.owners.at(owner.pointer).state.records[0].name="Changed authored title";authored_quest.rows[0].record.name="Changed authored title";
    check(dungeon_building_sample::quests(authored_quest,changed_sha,error)&&changed_sha!=quest_sha,"Full authored Quest title omitted");
    auto membership=witness;membership.owners.at(owner.pointer).state.local[0].destination_weak_confirmed=false;
    check(dungeon_building_sample::quests(membership,changed_sha,error)&&changed_sha!=quest_sha,"Actual weak Quest membership omitted");
    auto broken=witness;broken.rows[0].record.name="Foreign row";
    check(!dungeon_building_sample::quests(broken,changed_sha,error),"Global Quest record not owned by its current list accepted");
    for(int mode=0;mode<4;++mode){CapturePort port;port.first_fail=mode==1;port.last_fail=mode==2;port.drift=mode==3;Snapshot out{99};error.clear();
        const bool ok=dungeon_building_sample::capture(port,out,error,static_cast<const Snapshot*>(nullptr));
        check(ok==(mode==0),"Double-read sample confirmed failure/drift");
        check(out.value==(mode==0?12:0),"Failed sample leaked success-shaped partial result");
        check(port.reads==(mode==1?1:2),"Sample repeated failed original read");
        if(mode)check(error==(mode==1?"exact first ReadMemory cause":mode==2?"exact second owner cause":"exact current owner drift"),"Sample lost original cause");
    }
    for(int failure=0;failure<4;++failure){SeedPort port;port.failure=failure;error.clear();
        check(dungeon_building_sample::seed(port,{1,2,3,4},error)==(failure==0),"RNG result disagrees with actual guarded call/readback");
        check(port.order==(failure==1?"b":failure==2?"bw":"bwa"),"RNG native attempt occurred before owner check or was replayed");
        if(failure)check(error==(failure==1?"exact pending/current-world cause":failure==2?"exact seed SEH":"exact seed readback cause"),"RNG lost original native cause");
    }
    std::cout<<checks<<" Dungeon Building original authored sample/cause/owner/RNG cases passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
