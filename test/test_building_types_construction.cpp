#include "developer_staff/construction_native.h"
#include "developer_staff/construction_occurrence.h"
#include "remove_diagnostics.h"
#include "modular_identity.h"
#include "mt2mp/game/native_engine.h"
#include "live_host_state/memory_query_timing.h"
#include <cstring>
#include <algorithm>
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
bool work_ok=true;
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
bool is_modular_target(const LocalTarget& target,std::string& error){
    const auto read=[](uintptr_t p,size_t offset,auto& out,std::string& cause){return fixture::memory.copy(p+offset,&out,sizeof out,cause);};
    return modular_identity::read(read,fixture::image,target.object,fixture::actual,error);
}
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
bool resolve_identity_destination(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& e){
    LocalTarget expected;
    if(!resolve_record_target(snapshot,key,expected,e)||!resolve_local_target(expected.manager,expected.object,out,e))return false;
    if(expected.type!=out.type||expected.variant!=out.variant){e="actual target metadata differs";return false;}
    return true;
}
}
namespace mt2mp::game::developer_staff::job_census {
bool read(const Read&,const Capture&,Census& out,std::string& e){
    if(!fixture::census_ok){e="actual census failed";return false;}out=fixture::queue;return true;
}
}
namespace mt2mp::game::bug_authority {
bool capture_work(const Capture&,const job_census::Census&,WorkState& out,std::vector<uintptr_t>& bound,std::string& e){
    if(!fixture::work_ok){e="actual Bug-work capture failed";return false;}
    out={};bound.clear();return true;
}
bool has_work(){return false;}
}
namespace mt2mp::game::developer_staff::construction {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){return fixture::memory.copy(p+offset,&out,sizeof(out),e);}
const job_census::Read copy=[](auto p,auto out,auto size,auto& e){return fixture::memory.copy(p,out,size,e);};
struct Target{remove_diagnostics::LocalTarget binding;uintptr_t region=0,shard=0;uint8_t active=0;int32_t state=0;};
struct Actor{ActorPose pose;};
bool actor(const Capture&,size_t slot,Actor& out,std::string& e,bool=false){
    if(!fixture::actor_ok){e="actual actor failed";return false;}out.pose.id=static_cast<int32_t>(slot);return true;
}
#include "building_type_jobs.inc"
#include "building_type_binding.inc"
#include "building_type_graph.inc"
}

void reset(const std::string& type="SmallInn",const std::string& variant="base",uint32_t flags=UINT32_MAX){
    using namespace fixture;bytes.clear();rejected_read=0;target_field_reads=0;
    session_ok=record_ok=local_ok=actor_ok=census_ok=work_ok=true;local_scenery=false;local_variant.clear();
    context={};context.snapshot_uuid="11111111-1111-4111-8111-111111111111";
    capture={};capture.image=image;capture.pin.origin.map=reinterpret_cast<void*>(map);
    capture.pin.origin.world_epoch=7;capture.pin.origin.generation=9;
    binding={selection,object,3176,7,9,context.snapshot_uuid,"action:22222222-2222-4222-8222-222222222222",type,variant};
    actual={};actual.type=type;actual.variant=variant;actual.prop.uid=3176;
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


namespace registry_failure_fixture {
#include "scenery_registry_guard.inc"
void run(){
    for(const char* stage:{"ActualRoot.bind","read_action_prop"}){
        std::string error;
        fixture::need(!require_registry(9478,0,0,stage,error)&&error==std::string("scenery_action_prop_registry_failed SEH=0 uid=9478 stage=")+stage,
            "actual absent registry failure preserves original cause and exact queried UID/stage");
        fixture::need(!require_registry(10864,0,0xc0000005u,stage,error)&&error==std::string("scenery_action_prop_registry_failed SEH=3221225477 uid=10864 stage=")+stage,
            "actual registry exception remains distinct from absent UID and retains the native SEH");
        error="earlier upstream cause";
        fixture::need(require_registry(10864,0x123456,0,stage,error)&&error=="earlier upstream cause",
            "existing successful registry condition and upstream cause are unchanged");
    }
}
}
#include "duplicate_construction_native_cases.inc"
void modular_construction_target_cases(){using namespace fixture;std::string error;construction::State out;
    const auto setup=[](){reset("Scenery","base",0);put(object,image+0x16d93c0);put(object+0x18,uint64_t(3176));put(object+0xcba,uint8_t(0));
        put(object+0x7d0,uintptr_t(0xb0000000));put(object+0x7d8,uintptr_t(0xc0000000));put(uintptr_t(0xb0000000),image+0x1712a60);put(uintptr_t(0xc0000000),image+0x16fea40);put(object+0x7a8,int32_t(0));};
    setup();need(graph(out,error)&&out.jobs.size()==1&&out.jobs[0].present&&!out.jobs[0].active,"Exact Modular original queued Job rejected: "+error);
    for(int fault=0;fault<5;++fault){setup();error.clear();if(fault==0)put(object+0xcba,uint8_t(1));if(fault==1)put(object+0x7d0,uintptr_t(0));if(fault==2)put(uintptr_t(0xc0000000),image+0x16fea48);if(fault==3)put(proxy,uintptr_t(0));if(fault==4){auto foreign=queue.jobs[0];foreign.pointer++;queue.jobs.push_back(foreign);}
        need(!graph(out,error)&&!error.empty(),"Modular construction admitted art/foreign Layout/Plan/proxy/unknown Job");
        if(fault==4)need(error=="Construction queue contains a Job outside retained original placement occurrences","Modular admission weakened complete unknown-Job guard");}
    setup();put(object+0x7a8,int32_t(2));put(object+0x7e1,uint8_t(1));queue.jobs.clear();need(graph(out,error)&&!out.jobs[0].present&&out.jobs[0].active,"Modular completed Job lost authoritative activation");
}
int main(){try{duplicate_construction_native_fixture::run();registry_failure_fixture::run();modular_construction_target_cases();using namespace fixture;std::string e;construction::State out;
    for(const auto& [type,variant]:std::vector<std::pair<std::string,std::string>>{
        {"SmallInn","base"},{"Inn","boat"},{"Blacksmith","elf"},{"Tavern","base"},{"PotionShop","base"},
        {"Shop","base"},{"Landmark","lighthouse"},{"Respawn","base"},{"StartingPoint","base"}}){
        reset(type,variant);
        const bool bound=graph(out,e);
        need(bound&&out.jobs.size()==1&&out.jobs[0].building_key==binding.record_key&&
            out.jobs[0].present&&!out.jobs[0].active&&out.jobs[0].developer_id==-1,
            type+" original normal Job binding rejected: "+e);
        put(object+0x7a8,int32_t(0));
        const bool pending_bound=graph(out,e);
        need(pending_bound&&out.jobs.size()==1&&out.jobs[0].present&&!out.jobs[0].active,
            type+" original queued constructor state rejected: "+e);
        put(object+0x7a8,int32_t(2));
        put(object+0x7e1,uint8_t(1));queue.jobs.clear();
        need(graph(out,e)&&!out.jobs[0].present&&out.jobs[0].active,type+" completed activation readback rejected: "+e);
        put(object+0x7e1,uint8_t(0));
        need(!graph(out,e)&&e=="Construction original Job disappeared without authoritative Building activation",
            type+" missing Job became an activated Building");
    }
    for(const auto& [type,flags,cause]:std::vector<std::tuple<std::string,uint32_t,std::string>>{
        {"Scenery",0,"unmapped fixture field"},
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
        if(wrong==6)put(object+0x170,map+0x1c1);if(wrong==7)put(object+0x7a8,int32_t(3));
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
    for(unsigned fault=0;fault<24;++fault){
        reset();put(object+0x7a8,int32_t(0));e.clear();
        if(fault==0)queue.jobs.clear();
        if(fault==1)++queue.jobs[0].pointer;
        if(fault==2)link.retired=true;
        if(fault==3)link.target_alive=false;
        if(fault==4)link.target_restored=true;
        if(fault==5)put(proxy,uintptr_t(0));
        if(fault==6)put(proxy,object+0x28);
        if(fault==7)++queue.jobs[0].building;
        if(fault==8)++queue.jobs[0].building_proxy;
        if(fault==9)++queue.jobs[0].region;
        if(fault==10)++binding.uid;
        if(fault==11)put(object+0x170,map+0x1c1);
        if(fault==12)put(object+0x7e1,uint8_t(1));
        if(fault==13)put(object+0x7e1,uint8_t(2));
        if(fault==14)queue.jobs[0].valid=0;
        if(fault==15)queue.jobs[0].type=2;
        if(fault==16){queue.jobs[0].building=0;queue.jobs[0].building_proxy=0;}
        if(fault==17){queue.jobs[0].developer=0xa0000000;queue.jobs[0].reciprocal=false;}
        if(fault==18)queue.jobs.push_back(queue.jobs[0]);
        if(fault==19)++binding.manager;
        if(fault==20)++binding.world_generation;
        if(fault==21)++link.region;
        if(fault==22)++binding.world_epoch;
        if(fault==23)binding.snapshot_id+="foreign";
        construction::Target pending_target;
        const bool accepted=construction::building(capture,link,pending_target,e);
        need(!accepted&&!e.empty(),"pending constructor state admitted a stale/foreign occurrence; fault="+std::to_string(fault));
        if(fault==11)need(e=="Construction target root-shard owner differs","root owner failure became a lifecycle failure");
        if(fault==12)need(e=="Construction target state=0 active=1 requires state 1 or 2, or state 0/active 0 with retained live original Job",
            "invalid lifecycle failure lost actual and required values");
        if(fault==13)need(e=="Construction target active=2 requires Boolean 0 or 1","non-Boolean failure lost its value");
    }
    reset();put(object+0x7a8,int32_t(0));census_ok=false;e.clear();construction::Target pending_target;
    need(!construction::building(capture,link,pending_target,e)&&e=="actual census failed",
         "pending constructor admission replaced original queue read failure");
    reset();put(object+0x7a8,int32_t(0));rejected_read=proxy;e.clear();
    need(!construction::building(capture,link,pending_target,e)&&e=="actual field read failed",
         "pending constructor admission replaced original weak read failure");
    reset("Blacksmith","base");put(object+0x7a8,int32_t(1));
    need(graph(out,e)&&out.jobs.size()==1&&out.jobs[0].present,
        "original maintenance Building lost its retained construction occurrence: "+e);
    reset("Blacksmith","base");work_ok=false;
    need(!graph(out,e)&&e=="actual Bug-work capture failed",
        "upstream Bug-work failure became a successful construction graph");
    std::cout<<"PASS "<<checks<<" actual classifier/binding/construction graph cases; no native calls\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
