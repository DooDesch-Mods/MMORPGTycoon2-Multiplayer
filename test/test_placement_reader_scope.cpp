#include "live_host_state/memory_query_timing.h"
#include "developer_staff/read.h"
#include "mt2mp/core/hash.h"
#include "developer_staff/job_census.h"
#include "developer_staff/construction_types.h"
#include "tutorial_development.h"
#include "region_saved_identity.h"
#include <iostream>
#include <set>
#include <cmath>
#include <stdexcept>

namespace live_host_state=mt2mp::game::live_host_state;
namespace timing=live_host_state::apply_timing;
namespace staff=mt2mp::game::developer_staff;
namespace tutorial_development=mt2mp::game::tutorial_development;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
unsigned checks=0;uint64_t copies=0;unsigned char* allocation=nullptr;
void need(bool v,const std::string& e){++checks;if(!v)throw std::runtime_error(e);}
bool fail(std::string& e,const char* cause){e=cause;return false;}
struct ProcessMemory{bool copy(uintptr_t,void*,size_t,std::string&);};
#include "placement_copy.inc"
ProcessMemory memory;
template<class T>void put(uintptr_t p,size_t offset,const T& v){std::memcpy(reinterpret_cast<void*>(p+offset),&v,sizeof v);}
template<class T>bool at(uintptr_t p,size_t offset,T& v,std::string& e){++copies;return memory.copy(p+offset,&v,sizeof v,e);}
uintptr_t image=0,fixture_state=0,lounge=0,fixture_business=0,inside=0,outside=0,fixture_data=0,fixture_map=0,regions=0,region_data=0;
uintptr_t dev(size_t i){return image+0x40000+i*0x500;}
staff::Pin pinned;int pins=0;bool change_on_pin=false,throw_on_pin=false;
namespace actual_process {
using Capture=staff::Capture;
struct ProcessPort:staff::Port {
    bool resolver(uintptr_t& base,uint32_t& thread,std::string&)override{base=image;thread=7;return true;}
    bool pin(staff::Pin& out,std::string&)override{
        ++pins;if(throw_on_pin)throw std::runtime_error("original pin exception");
        if(change_on_pin&&pins==2)put(dev(0),0x4c4,int32_t(1));out=pinned;return true;
    }
    bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{++copies;return memory.copy(p,out,n,e);}
};
using staff::read;
#include "scoped_process.inc"
}
namespace actual_actor {
using Capture=staff::Capture;using ActorPose=staff::construction::ActorPose;
struct Actor {
    ActorPose pose;uintptr_t shard=0,region=0,quadtree=0,current=0,destination=0,task=0,road=0,node=0;
    uint8_t moving=0,path=0,pending=0;int32_t steps=0,index=0;
    bool operator==(const Actor&)const=default;
};
#include "scoped_actor.inc"
using State=staff::construction::State;
#include "scoped_actor_loops.inc"
}
namespace actual_staff {
struct Developer{uintptr_t address=0;int32_t id=0,job=0,row=-1;uint8_t employed=0,on_map=0,wanted=0;};
struct Owners{tutorial_development::StartingpointSnapshot snapshot;std::vector<Developer> developers;int32_t count=0;};
bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t ap=0;return at(p,0,ap,e)&&(ap==image+rva||fail(e,"tutorial native vtable differs"));}
#include "scoped_staff.inc"
}
uint64_t queries(){return timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count;}
#include "developer_census_scope_cases.inc"
#include "developer_guard_scope_cases.inc"
#include "actor_loop_scope_cases.inc"
void seed(){
    std::memset(allocation,0,0x1800000);image=uintptr_t(allocation);fixture_state=image+0x10000;lounge=image+0x11000;fixture_business=image+0x12000;
    inside=image+0x13000;outside=image+0x14000;fixture_data=image+0x15000;fixture_map=image+0x20000;regions=image+0x80000;region_data=image+0x70000;
    pinned={};pinned.origin.state=reinterpret_cast<void*>(fixture_state);pinned.origin.map=reinterpret_cast<void*>(fixture_map);pinned.origin.world_epoch=1;pinned.origin.generation=1;
    pinned.ledger.cash=1000;pinned.pause_scale=1;
    put(fixture_state,0x180,lounge);put(fixture_state,0x1c0,fixture_business);put(lounge,0,image+0x16de6c0);put(fixture_business,0,image+0x16d97e0);put(image,0x13df290,lounge);
    put(fixture_business,0x4c,int32_t(1));put(lounge,0x310,int32_t(1));put(lounge,0x70,fixture_data);put(lounge,0x78,int32_t(50));put(lounge,0x7c,int32_t(50));put(lounge,0x98,inside);put(lounge,0xa0,outside);
    put(inside,0,image+0x16e47f0);put(outside,0,image+0x16e47f0);put(inside,0x38,fixture_data);put(inside,0x40,int32_t(50));put(inside,0x44,int32_t(50));
    put(image+0x17426e0,0x38,image+0x3b13a0);
    for(size_t i=0;i<50;++i){const auto p=dev(i);put(fixture_data,i*8,p);put(p,0,image+0x17422e0);put(p,0x30,image+0x17426e0);put(p,0x4c0,int32_t((i+7)%50));
        put(p,0x4d8,uint8_t(i==0));put(p,0x4d9,uint8_t(i==0));put(p,0x100,std::array<float,4>{0,0,0,1});put(p,0x11c,std::array<float,3>{1,1,1});
        put(p,0x170,fixture_map+0x1c0);put(p,0x178,regions+67*0x900);}
    put(fixture_map,0,image+0x1742860);put(fixture_map+0x1c0,0,image+0x174ad20);put(fixture_map+0x230,0,image+0x16f04e0);
    put(fixture_map+0x230,8,region_data);put(fixture_map+0x230,0x10,int32_t(70));put(fixture_map+0x230,0x14,int32_t(70));
    put(fixture_map,0x358,region_data);put(fixture_map,0x360,int32_t(1));put(fixture_map,0x364,int32_t(1));
    for(size_t i=0;i<70;++i){const auto region=regions+i*0x900;put(region_data,i*8,region);put(region,0,image+0x1787930);put(region,0x70,fixture_map+0x1c0);put(region,0x834,int32_t(i));}
}
int main(){try{
    allocation=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));need(allocation!=nullptr,"private memory allocation");seed();
    timing::Scope take(timing::Part::Take);std::string e;staff::Capture capture;auto q=queries();auto reads=copies;
    need(actual_process::read_process(capture,e),"actual double census: "+e);need(pins==3&&capture.state.rows.size()==50&&capture.state.rows[0].id==7,"all three pins and persisted IDs preserved");
    need(copies-reads>1000&&queries()-q<10,"all double-census byte reads retained with bounded OS queries");need(timing::page_read_scope==nullptr,"process reader scope closes");
    put(dev(0),0x110,float(72));pins=0;need(actual_process::read_process(capture,e)&&capture.state.rows[0].position[0]==72,"later byte values are fresh");
    change_on_pin=true;pins=0;need(!actual_process::read_process(capture,e)&&e=="Developer owned pool changed during second census","mid-census state change still rejects");change_on_pin=false;put(dev(0),0x4c4,int32_t(0));
    throw_on_pin=true;bool caught=false;try{actual_process::read_process(capture,e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original pin exception";}throw_on_pin=false;
    need(caught&&timing::page_read_scope==nullptr,"pin exception preserved and scope unwinds");pins=0;need(actual_process::read_process(capture,e),e);
    job_scope_cases(capture);
    actor_loop_scope_cases(capture);
    guard_scope_cases(capture);
    actual_actor::Actor actor; q=queries();reads=copies;
    for(size_t i=0;i<50;++i)need(actual_actor::actor(capture,i,actor,e)&&actor.pose.region_id==67&&actor.pose.id==int32_t((i+7)%50),"actual actor and complete Region identity");
    need(copies-reads>14000&&queries()-q<300,"all 50 complete Region scans retained; OS metadata queries reduced");need(timing::page_read_scope==nullptr,"actor scopes close before effects");
    put(dev(0),0x11c,std::array<float,3>{2,1,1});need(actual_actor::actor(capture,0,actor,e)&&actor.pose.scale[0]==2,"actor reads changed values");
    put(regions+69*0x900,0x834,int32_t(67));need(!actual_actor::actor(capture,0,actor,e)&&e.find("saved region address point")!=std::string::npos,"foreign duplicate Region remains rejected");put(regions+69*0x900,0x834,int32_t(69));
    put(dev(0),0x30,uintptr_t(0));need(!actual_actor::actor(capture,0,actor,e)&&!e.empty(),"invalid renderer pointer rejected");put(dev(0),0x30,image+0x17426e0);
    tutorial_development::StartingpointSnapshot snapshot;snapshot.origin=pinned.origin;snapshot.lounge=lounge;snapshot.project=image+0x16000;snapshot.creation.emplace();snapshot.row.emplace();snapshot.row->project_present=true;snapshot.row->purchased=true;
    for(size_t i=0;i<50;++i)snapshot.creation->developers.push_back({dev(i),0});actual_staff::Owners owners;q=queries();reads=copies;
    need(actual_staff::read_owners(snapshot,owners,e)&&owners.developers.size()==50&&owners.developers[0].id==7,"actual Staff owner complete read");
    need(copies-reads>=250&&queries()-q<10,"all Staff fields remain live reads");
    put(dev(1),0x4d9,uint8_t(1));need(actual_staff::read_owners(snapshot,owners,e)&&owners.developers[1].employed==1,"Staff sees changed lifetime");
    put(dev(1),0x4c0,int32_t(7));need(!actual_staff::read_owners(snapshot,owners,e)&&e=="Staff display actual Developer identity/job/employment prefix differs","Staff duplicate ID guard retained");put(dev(1),0x4c0,int32_t(8));
    DWORD old=0;need(VirtualProtect(reinterpret_cast<void*>(dev(0)),4096,PAGE_NOACCESS,&old)!=0,"change protection between readers");e.clear();
    need(!actual_actor::actor(capture,0,actor,e)&&e=="error: native placement UI page unreadable","next actor scope rechecks protection");DWORD ignored=0;
    need(VirtualProtect(reinterpret_cast<void*>(dev(0)),4096,old,&ignored)!=0,"restore private protection");
    need(actual_actor::actor(capture,0,actor,e)&&timing::page_read_scope==nullptr,"read after restored protection is fresh");
    need(VirtualFree(allocation,0,MEM_RELEASE)!=0,"free private allocation");allocation=nullptr;
    std::cout<<checks<<" actual placement reader-scope checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<'\n';if(allocation)VirtualFree(allocation,0,MEM_RELEASE);return 1;}}
