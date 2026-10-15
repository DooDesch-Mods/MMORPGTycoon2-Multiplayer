#include "developer_staff/construction_native.h"
#include "developer_staff/construction_occurrence.h"
#include "remove_diagnostics.h"
#include "mt2mp/game/native_engine.h"
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace action=mt2mp::action_session;
using namespace mt2mp::game;
using namespace mt2mp::game::developer_staff;
namespace fixture {
constexpr uintptr_t image=0x10000000,object=0x20000000,selection=0x30000000,definition=0x40000000,
    text=0x50000000,map=0x60000000,region=0x70000000,proxy=0x80000000;
std::map<uintptr_t,unsigned char> bytes;
uintptr_t rejected_read=0;unsigned target_field_reads=0,checks=0;
bool session_ok=true,record_ok=true,local_ok=true,actor_ok=true,census_ok=true;
std::string local_variant;bool local_scenery=false;
action::Context context;
remove_diagnostics::LocalTarget binding;
native::BuildingState actual;
Capture capture;
job_census::Census queue;
construction::LocalJob link;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
template<class T>void put(uintptr_t p,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);
    for(size_t i=0;i<sizeof(T);++i)bytes[p+i]=data[i];}
void put_text(uintptr_t p,const std::string& value){for(size_t i=0;i<value.size();++i)bytes[p+i]=static_cast<unsigned char>(value[i]);}
struct Memory {
    bool copy(uintptr_t p,void* out,size_t size,std::string& error){
        if(p==object+0x178||p==object+0x170||p==object+0x7e1||p==object+0x7a8)++target_field_reads;
        if(p==rejected_read){error="actual field read failed";return false;}
        for(size_t i=0;i<size;++i)if(!bytes.contains(p+i)){error="unmapped fixture field";return false;}
        for(size_t i=0;i<size;++i)static_cast<unsigned char*>(out)[i]=bytes.at(p+i);
        return true;
    }
} memory;
}
namespace host {using ProcessMemory=fixture::Memory;}
namespace prop_lease {struct Prop{uintptr_t class_addresspoint=0,registry_object=0;uint64_t uid=0;};}
namespace scenery_kiss {
bool classify_scenery(uintptr_t,uintptr_t,uint64_t,const native::BuildingState&,bool& out,std::string&){out=true;return true;}
}
#include "building_type_classify.inc"
namespace mt2mp::action_session {
struct FixtureSession {bool current(Context& out,std::string& e){if(!fixture::session_ok){e="actual Session failed";return false;}out=fixture::context;return true;}};
FixtureSession& selected_session(){static FixtureSession value;return value;}
}
namespace mt2mp::game::remove_diagnostics {
bool classify(LocalTarget& out,std::string& e){
    out=fixture::binding;
    const prop_lease::Prop prop{fixture::image+0x16d93c0,fixture::object,out.uid};
    return classify_normal_building(fixture::memory,fixture::image,prop,fixture::actual,
        out.normal_building,out.normal_scenery,out.kind_cause,e);
}
bool resolve_record_target(const std::string&,const std::string&,LocalTarget& out,std::string& e){
    if(!fixture::record_ok){e="actual record resolver failed";return false;}return classify(out,e);
}
bool resolve_local_target(uintptr_t,uintptr_t,LocalTarget& out,std::string& e){
    if(!fixture::local_ok){e="actual local resolver failed";return false;}
    if(!classify(out,e))return false;
    if(!fixture::local_variant.empty())out.variant=fixture::local_variant;
    if(fixture::local_scenery)out.normal_scenery=true;
    return true;
}
}
namespace mt2mp::game::developer_staff::job_census {
bool read(const Read&,const Capture&,Census& out,std::string& e){
    if(!fixture::census_ok){e="actual census failed";return false;}out=fixture::queue;return true;
}
}
namespace mt2mp::game::developer_staff::construction {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){return fixture::memory.copy(p+offset,&out,sizeof(out),e);}
const job_census::Read copy=[](auto p,auto out,auto size,auto& e){return fixture::memory.copy(p,out,size,e);};
struct Target{remove_diagnostics::LocalTarget binding;uintptr_t region=0,shard=0;uint8_t active=0;int32_t state=0;};
struct Actor{ActorPose pose;};
bool actor(const Capture&,size_t slot,Actor& out,std::string& e){
    if(!fixture::actor_ok){e="actual actor failed";return false;}out.pose.id=static_cast<int32_t>(slot);return true;
}
#include "building_type_binding.inc"
#include "building_type_graph.inc"
}

void reset(const std::string& type="SmallInn",const std::string& variant="base",uint32_t flags=UINT32_MAX){
    using namespace fixture;bytes.clear();rejected_read=0;target_field_reads=0;
    session_ok=record_ok=local_ok=actor_ok=census_ok=true;local_scenery=false;local_variant.clear();
    context={};context.snapshot_uuid="11111111-1111-4111-8111-111111111111";
    capture={};capture.image=image;capture.pin.origin.map=reinterpret_cast<void*>(map);
    capture.pin.origin.world_epoch=7;capture.pin.origin.generation=9;
    binding={selection,object,3176,7,9,context.snapshot_uuid,"action:22222222-2222-4222-8222-222222222222",type,variant};
    actual={};actual.type=type;actual.variant=variant;
    if(flags==UINT32_MAX){const std::map<std::string,uint32_t> known{{"SmallInn",1},{"Inn",1},{"Shop",2},
        {"StartingPoint",4},{"Respawn",8},{"Tavern",16},{"Blacksmith",64},{"Landmark",128},{"PotionShop",512}};
        flags=known.at(type);}
    put(image+0x13df630,selection);put(selection,image+0x17205d0);
    put(object+0x390,definition);put(definition,image+0x172ae50);put(definition+0x18,text);
    put(definition+0x20,uint64_t(type.size()));put_text(text,type);put(definition+0x16c,flags);
    put(object+0x178,region);put(object+0x170,map+0x1c0);put(object+0x7e1,uint8_t(0));put(object+0x7a8,int32_t(2));
    put(proxy,object+0x20);
    link={binding.record_key,0x90000000,3176,true,false,object,proxy,region};
    queue={};job_census::Job job;job.pointer=link.job;job.type=1;job.valid=1;
    job.building=object;job.building_proxy=proxy;job.region=region;queue.jobs.push_back(job);
}
bool graph(construction::State& out,std::string& e){job_census::Census jobs;std::vector<construction::Actor> actors;
    return construction::graph(fixture::capture,{fixture::link},out,jobs,actors,e);}

int main(){try{using namespace fixture;std::string e;construction::State out;
    for(const auto& [type,variant]:std::vector<std::pair<std::string,std::string>>{
        {"SmallInn","base"},{"Inn","boat"},{"Blacksmith","elf"},{"Tavern","base"},{"PotionShop","base"},
        {"Shop","base"},{"Landmark","lighthouse"},{"Respawn","base"},{"StartingPoint","base"}}){
        reset(type,variant);
        const bool bound=graph(out,e);
        need(bound&&out.jobs.size()==1&&out.jobs[0].building_key==binding.record_key&&
            out.jobs[0].present&&!out.jobs[0].active&&out.jobs[0].developer_id==-1,
            type+" original normal Job binding rejected: "+e);
        put(object+0x7e1,uint8_t(1));queue.jobs.clear();
        need(graph(out,e)&&!out.jobs[0].present&&out.jobs[0].active,type+" completed activation readback rejected: "+e);
        put(object+0x7e1,uint8_t(0));
        need(!graph(out,e)&&e=="Construction original Job disappeared without authoritative Building activation",
            type+" missing Job became an activated Building");
    }
    for(const auto& [type,flags,cause]:std::vector<std::tuple<std::string,uint32_t,std::string>>{
        {"Scenery",0,"native UI target is a separate Scenery definition"},
        {"Inn",1u<<13,"native UI target is a separate Scenery definition"},
        {"DungeonExterior",1u<<16,"native UI target is a separate dungeon definition"},
        {"DungeonPortal",1u<<17,"native UI target is a separate dungeon definition"},
        {"StartingPoint",0,"native UI target StartingPoint type/feature differs"},
        {"Inn",1u<<2,"native UI target StartingPoint type/feature differs"}}){
        reset(type,"base",flags);need(!graph(out,e)&&e==cause&&target_field_reads==0,
            "unsupported native branch lost its original cause or read activation: "+e);
    }
    for(int membership=0;membership<2;++membership){reset("Blacksmith","elf");
        actual.dungeon_membership=membership==0;actual.dungeon_instance_membership=membership==1;
        need(!graph(out,e)&&e=="native UI target belongs to a separate dungeon world"&&target_field_reads==0,
            "dungeon member entered root-shard activation");}
    for(int wrong=0;wrong<11;++wrong){reset("Inn","boat");
        if(wrong==0)binding.uid++;if(wrong==1)binding.record_key+="x";if(wrong==2)binding.snapshot_id+="x";
        if(wrong==3)binding.world_epoch++;if(wrong==4)binding.world_generation++;if(wrong==5)binding.manager++;
        if(wrong==6)put(object+0x170,map+0x1c1);if(wrong==7)put(object+0x7a8,int32_t(1));
        if(wrong==8)put(object+0x7e1,uint8_t(2));if(wrong==9)local_variant="elf";if(wrong==10)local_scenery=true;
        need(!graph(out,e)&&!e.empty(),"wrong Building identity/variant/lifecycle entered construction");
    }
    for(int failure=0;failure<7;++failure){reset("Blacksmith","elf");std::string expected;
        if(failure==0){session_ok=false;expected="actual Session failed";}
        if(failure==1){record_ok=false;expected="actual record resolver failed";}
        if(failure==2){local_ok=false;expected="actual local resolver failed";}
        if(failure==3){rejected_read=definition+0x16c;expected="actual field read failed";}
        if(failure==4){rejected_read=object+0x7e1;expected="actual field read failed";}
        if(failure==5){actor_ok=false;expected="actual actor failed";}
        if(failure==6){census_ok=false;expected="actual census failed";}
        need(!graph(out,e)&&e==expected,"upstream original cause was replaced: "+e);
    }
    for(int wrong=0;wrong<4;++wrong){reset("Inn","boat");
        if(wrong==0)queue.jobs[0].pointer++;if(wrong==1)queue.jobs[0].region++;
        if(wrong==2)put(proxy,uintptr_t(0));if(wrong==3){queue.jobs[0].developer=0xa0000000;queue.jobs[0].reciprocal=false;}
        need(!graph(out,e)&&!e.empty(),"foreign occurrence or broken native weak/backlink admitted");
    }
    std::cout<<"PASS "<<checks<<" actual classifier/binding/construction graph cases; no native calls\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
