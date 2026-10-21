#include "zone_quest_history.h"
namespace zone_quest_history_fixture {
namespace zh=mt2mp::game::zone_quest_history;
namespace qr=mt2mp::game::monster_quest_retirement;
namespace q=mt2mp::npc_quest_records;
using json=nlohmann::json;
struct Item {uintptr_t quest=0,owner=0,destination=0;uint64_t uid=0;std::string name,key;int32_t completed=0;};
inline qr::Witness witness(const std::vector<Item>& local,const std::vector<uintptr_t>& global){
    qr::Witness result;result.manager=900;
    for(const auto pointer:{uintptr_t(100),uintptr_t(200)}){qr::Owner owner;owner.pointer=pointer;owner.uid=pointer*10;
        owner.key=pointer==100?"owner-a":"owner-b";result.owners.emplace(pointer,std::move(owner));}
    for(const auto& item:local){auto& owner=result.owners.at(item.owner);q::Record row;
        row.sequence_id=int32_t(owner.state.records.size());row.type=1;row.action=2;row.name=item.name;
        row.destination_key=item.key;row.monsters_to_kill=3;row.completed.total=item.completed;
        owner.state.records.push_back(row);owner.destination_uids.push_back(item.uid);
        owner.state.local.push_back({item.quest,item.owner,item.destination,true,true,true,true,q::TaskStatus::Absent});}
    for(auto pointer:global){bool found=false;
        for(const auto& [owner_pointer,owner]:result.owners){
            for(size_t slot=0;slot<owner.state.local.size();++slot){const auto& link=owner.state.local[slot];if(link.quest!=pointer)continue;
                result.quests.push_back(pointer);result.rows.push_back({owner.key,owner.state.records[slot]});
                result.links.push_back({link.destination,link.destination+8,link.destination+0x20,owner_pointer,owner_pointer+8,owner_pointer+0x20});
                if(owner.state.records[slot].completed.total>0)++result.completed;found=true;}}
        if(!found)throw std::runtime_error("fixture global Quest lacks local record");}
    return result;
}
inline std::vector<Item> initial(){return {{10,100,400,4000,"Zone quest","zone",1},
    {20,100,500,5000,"Monster quest","monster",0},{30,200,600,6000,"Unrelated","other",0}};}
inline json references(){const auto progress=[](const char* key){return json{{"quest",{{"state",2},{"key",key}}},{"complete",true},{"reward",17}};};
    return json::array({{{"id",1},{"npc","owner-a"},{"progress",json::array({progress("owner-a/quest/0"),progress("owner-a/quest/1"),progress("owner-b/quest/0")})}}});}
inline json expired(json value){for(size_t slot=0;slot<2;++slot){auto& row=value[0]["progress"][slot];row["quest"]={{"state",int(slot)},{"key",""}};row["complete"]=false;}return value;}
}
inline unsigned zone_quest_history_cases(){using namespace zone_quest_history_fixture;
    unsigned checks=0;std::string error;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why+" case="+std::to_string(checks)+" cause="+error);};
    const auto source=witness(initial(),{10,30,20});const auto old_refs=references(),new_refs=expired(old_refs);
    const std::vector<zh::Step> mixed{{zh::Kind::Destroy,"zone",4000,400},{zh::Kind::Make,"zone",4000,0},
        {zh::Kind::Quests,"owner-a",1000,100},{zh::Kind::Destroy,"monster",5000,500},{zh::Kind::EditZone,"zone",4000,0}};
    auto values=initial();values.erase(values.begin()+1);values[0].quest=11;values[0].completed=0;
    auto after=witness(values,{30,11});qr::Witness retired;
    check(zh::verify(source,source,after,mixed,old_refs,new_refs,retired,error),"Mixed native graph original restoration rejected");
    check(retired.removed==std::vector<uintptr_t>{10,20},"Retired original Quest lifetimes or order lost");
    for(unsigned change=0;change<12;++change){auto bad=after;auto steps=mixed;auto refs=new_refs;
        if(change==0){std::reverse(bad.quests.begin(),bad.quests.end());std::reverse(bad.rows.begin(),bad.rows.end());std::reverse(bad.links.begin(),bad.links.end());}
        if(change==1){bad.owners.at(100).state.records[0].name="Unexpected";bad.rows[1].record.name="Unexpected";}
        if(change==2)bad.owners.at(100).destination_uids[0]++;
        if(change==3)bad.owners.at(200).state.local[0].task=q::TaskStatus::Pending;
        if(change==4){bad.owners.at(200).state.records[0].name="Changed unrelated";bad.rows[0].record.name="Changed unrelated";}
        if(change==5)bad.completed++;
        if(change==6)bad.owners.at(100).uid++;
        if(change==7)steps[0].receiver++;
        if(change==8)steps[3].uid++;
        if(change==9)steps[2].receiver++;
        if(change==10)refs[0]["progress"][0]["quest"]={{"state",2},{"key","owner-a/quest/0"}};
        if(change==11)refs[0]["progress"][2]["reward"]=18;
        error.clear();check(!zh::verify(source,source,bad,steps,old_refs,refs,retired,error)&&!error.empty(),"Changed mixed graph effect accepted");}
    // Original Set recomputes runtime values. Authored definitions and exact
    // complete peer readback remain mandatory; old runtime caches are not copied.
    {auto computed=after;auto& record=computed.owners.at(100).state.records[0];record.completed.total=1;
        computed.completed=1;computed.rows[1].record=record;
        check(zh::verify(source,source,computed,mixed,old_refs,new_refs,retired,error),"Original recomputed runtime record rejected");}
    {auto steps=mixed;std::swap(steps[2],steps[3]);
        check(!zh::verify(source,source,after,steps,old_refs,new_refs,retired,error),"Native dependency execution order ignored");}
    {const auto set_after=witness({{11,100,400,4000,"Zone quest","zone",0},{21,100,500,5000,"Monster quest","monster",0},initial()[2]},{30,11,21});
        const std::vector<zh::Step> steps{{zh::Kind::Quests,"owner-a",1000,100},{zh::Kind::Quests,"owner-a",1000,100}};
        check(zh::verify(source,source,set_after,steps,old_refs,new_refs,retired,error),"Repeated original whole-owner Set order rejected");
        check(retired.removed==std::vector<uintptr_t>{10,20},"Intermediate new Quests were treated as initial old lifetimes");
        auto empty=witness({initial()[2]},{30});
        check(zh::verify(source,empty,set_after,{{zh::Kind::Make,"zone",4000,0},{zh::Kind::Quests,"owner-a",1000,100}},new_refs,new_refs,retired,error),"Owner whose last Quest disappeared could not restore");
        empty.owners.erase(100);
        check(!zh::verify(source,empty,set_after,steps,new_refs,new_refs,retired,error)&&error.find("seeded")!=std::string::npos,"Missing prior empty-owner readback accepted");}
    {auto historical=witness({initial()[0],initial()[1]},{10,20});historical.owners.erase(200);
        check(zh::verify(historical,source,after,mixed,old_refs,new_refs,retired,error),"Later unrelated current Quest owner was rejected");
        auto bad=after;bad.owners.at(200).state.local[0].task=q::TaskStatus::Pending;
        check(!zh::verify(historical,source,bad,mixed,old_refs,new_refs,retired,error),"Later unrelated current Quest owner task changed");}
    {auto survivor=witness({{20,100,500,5000,"Monster quest","monster",0},initial()[2]},{20,30});
        auto refs=old_refs;refs[0]["progress"][0]["quest"]={{"state",1},{"key",""}};refs[0]["progress"][0]["complete"]=false;
        refs[0]["progress"][1]["quest"]["key"]="owner-a/quest/0";
        check(zh::verify(source,source,survivor,{{zh::Kind::Destroy,"zone",4000,400}},old_refs,refs,retired,error),"Destroy survivor swap-last and active slot renumber failed");
        survivor.owners.at(100).state.local[0].destination_weak_confirmed=false;
        check(!zh::verify(source,source,survivor,{{zh::Kind::Destroy,"zone",4000,400}},old_refs,refs,retired,error),"Changed surviving weak ownership accepted");}
    {check(zh::verify(source,source,source,{{zh::Kind::EditZone,"zone",4000,0}},old_refs,old_refs,retired,error),"EditZone invented Quest regeneration");
        check(!zh::verify(source,source,source,{{zh::Kind(99),"zone",4000,0}},old_refs,old_refs,retired,error),"Unsupported effect kind accepted");}
    return checks;
}
