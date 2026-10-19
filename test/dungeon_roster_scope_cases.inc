#include "dungeon_scene_roster.h"

namespace dungeon_scope_fixture {
namespace dungeon_scene_roster=mt2mp::game::dungeon_scene_roster;
namespace live_host_state=mt2mp::game::live_host_state;
struct Exterior {uint64_t uid=0;std::string record;};
struct Fixture:region_scope_fixture::Fixture {
    uintptr_t manager(size_t n)const{return uintptr_t(memory)+0x50000+n*0x500;}
    uintptr_t building(size_t n)const{return uintptr_t(memory)+0x78000+n*0x1000;}
    uintptr_t dungeon()const{return uintptr_t(memory)+0x7b000;}
    void setup(bool scene=false){reset();
        for(size_t n=0;n<121;++n){put(region(n)+0x950,manager(n));put(manager(n),image+0x17183c0);put(manager(n)+0x450,root);put(manager(n)+0x458,region(n));
            put(manager(n)+0x408,image+0x172d7c0);put(manager(n)+0x410,n?uintptr_t(0):uintptr_t(memory)+0x3000);put(manager(n)+0x418,int32_t(n?0:3));put(manager(n)+0x41c,int32_t(n?0:3));}
        for(size_t n=0;n<3;++n){put(uintptr_t(memory)+0x3000+n*8,building(n));put(building(n),image+0x16d93c0);}
        if(scene){const auto shard=uintptr_t(memory)+0x7c000,owner=uintptr_t(memory)+0x7d000;
            put(building(0)+0xa38,dungeon());put(building(0)+0x18,uint64_t(0x200000012));put(building(0)+0x170,root);
            put(dungeon(),image+0x16c0560);put(dungeon()+0x18,uint64_t(0x100000012));put(dungeon()+0x48,shard);put(dungeon()+0x50,owner);put(dungeon()+0x6f8,building(0));
            put(shard,image+0x174ad20);put(shard+0x48,int32_t(1));put(shard+0x18,dungeon());put(owner,image+0x1787930);put(owner+0x70,shard);}}
};
using Find=std::function<uint32_t(uint64_t,uintptr_t&)>;
using Typed=std::function<uint32_t(uintptr_t,uintptr_t,uintptr_t,uintptr_t&)>;
template<class Call>uint32_t original_find(Call& call,uint64_t uid,uintptr_t& found){return call(uid,found);}
template<class Call>uint32_t original_cast(Call& call,uintptr_t object,uintptr_t source,uintptr_t target,uintptr_t& found){return call(object,source,target,found);}
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Api {Find find;Typed cast;};
template<class Read,class Lookup,class Cast>
bool current_roster(Read& at,Lookup& lookup,Cast& cast,Fixture& fixture,bool scoped,std::vector<dungeon_scene_roster::Scene>& ready,std::string& error){
    const auto image=fixture.image,map=fixture.map;Api api{lookup,cast};
#include "dungeon_current_find.inc"
#include "dungeon_current_typed.inc"
    auto identify=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& e){uintptr_t registered=0;if(!find(uid,registered,e))return false;
        if(registered!=object)return fail(e,"fixture exterior full UID changed");key=std::string(64,'a')+":world:building:17";return true;};
    if(!scoped)return dungeon_scene_roster::read(at,typed,find,identify,image,map,map+0x1c0,ready,error);
#include "dungeon_roster_scoped_call.inc"
}
template<class Read,class Lookup,class Cast>
bool census_roster(Read& at,Lookup& lookup,Cast& cast,Fixture& fixture,bool scoped,std::vector<dungeon_scene_roster::Scene>& ready,std::string& error){
    const auto image=fixture.image,map=fixture.map;Api api{lookup,cast};
#include "dungeon_census_find.inc"
#include "dungeon_census_typed.inc"
    auto identify=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& e){uintptr_t registered=0;if(!find(uid,registered,e))return false;
        if(registered!=object)return fail(e,"fixture exterior full UID changed");key=std::string(64,'a')+":world:building:17";return true;};
    if(!scoped)return dungeon_scene_roster::read(at,typed,find,identify,image,map,map+0x1c0,ready,error);
#include "dungeon_roster_scoped_call.inc"
}
namespace remove_diagnostics {
inline std::function<bool(uintptr_t,std::string&)> key_callback;
inline bool resolve_current_building_key(const std::string&,uintptr_t building,uint64_t,std::string& record,std::string& error){
    record=std::string(64,'a')+":world:building:17";return key_callback(building,error);}
}
template<class Read>bool exteriors(Read& at,Fixture& fixture,bool scoped,std::vector<Exterior>& exteriors,std::string& error){
    struct World {uintptr_t map;};const World world{fixture.map};const std::string snapshot="owned-fixture";
    if(scoped){
#include "dungeon_exterior_loop.inc"
    }else{
#include "dungeon_exterior_baseline.inc"
    }
    return true;
}
}

void dungeon_roster_scope_cases(){
    using namespace dungeon_scope_fixture;Fixture fixture;std::string error;std::vector<dungeon_scene_roster::Scene> ready;
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.read(object,offset,value,cause);};
    unsigned finds=0,casts=0;
    auto lookup=[&](uint64_t uid,uintptr_t& found)->uint32_t{++finds;found=uid==0x100000012?fixture.dungeon():uid==0x200000012?fixture.building(0):0;return 0;};
    auto cast=[&](uintptr_t object,uintptr_t,uintptr_t,uintptr_t& found)->uint32_t{++casts;found=object;return 0;};
    t::totals={};t::Scope capture(t::Part::HostCapture);fixture.setup();auto start=count(t::Part::HostMemoryVirtualQuery);
    need(current_roster(at,lookup,cast,fixture,false,ready,error)&&ready.empty(),"unscoped complete121-Region three-Building roster");
    const auto roster_trace=fixture.events;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(roster_trace.size()==1956&&baseline_queries==1956&&!finds&&!casts,"empty Dungeon roster retains1956 actual field reads without native callbacks");
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(current_roster(at,lookup,cast,fixture,true,ready,error),"actual current roster scope");
    const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(fixture.events==roster_trace&&scoped_queries==1&&!t::page_read_scope,"current roster preserves1956 read addresses/order/bytes with one OS query");
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(census_roster(at,lookup,cast,fixture,true,ready,error)&&fixture.events==roster_trace&&count(t::Part::HostMemoryVirtualQuery)-start==1,"both census roster invocations preserve same pure complete traversal");
    remove_diagnostics::key_callback=[](uintptr_t,std::string&){return true;};std::vector<Exterior> exterior;
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(exteriors(at,fixture,false,exterior,error)&&exterior.empty(),"unscoped complete current exterior traversal");
    const auto exterior_trace=fixture.events;const auto exterior_baseline=count(t::Part::HostMemoryVirtualQuery)-start;
    need(exterior_trace.size()==1104&&exterior_baseline==1104,"current exterior traversal retains1104 actual field reads");
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(exteriors(at,fixture,true,exterior,error)&&fixture.events==exterior_trace&&count(t::Part::HostMemoryVirtualQuery)-start==1&&!t::page_read_scope,"actual exterior loop retains1104 exact reads with one OS query");
    fixture.setup();fixture.put(fixture.region(120)+0x834,int32_t(0));need(!current_roster(at,lookup,cast,fixture,true,ready,error)&&error.find("Shard/ID differs")!=std::string::npos,"fresh complete Region ID guard remains active between calls");
    fixture.setup();fixture.mutation_address=fixture.map+0x240;need(!current_roster(at,lookup,cast,fixture,false,ready,error),"baseline final Region array guard rejects changed count");const auto rejected=fixture.events;const auto cause=error;
    fixture.setup();fixture.mutation_address=fixture.map+0x240;need(!current_roster(at,lookup,cast,fixture,true,ready,error)&&fixture.events==rejected&&error==cause&&!t::page_read_scope,"scoped final Region array guard preserves fresh reads and original cause");
    fixture.setup();fixture.mutation_address=fixture.map+0x240;need(!exteriors(at,fixture,false,exterior,error),"baseline exterior final Region array guard rejects changed count");const auto exterior_rejected=fixture.events;const auto exterior_cause=error;
    fixture.setup();fixture.mutation_address=fixture.map+0x240;need(!exteriors(at,fixture,true,exterior,error)&&fixture.events==exterior_rejected&&error==exterior_cause&&!t::page_read_scope,"exterior final Region array guard preserves fresh reads and cause");
    fixture.setup(true);finds=casts=0;uint64_t resets=0;
    {t::Scope take(t::Part::Take);start=count(t::Part::OriginalNestedReset);need(current_roster(at,lookup,cast,fixture,true,ready,error)&&ready.size()==1,"actual template roster traverses original callback barriers");resets=count(t::Part::OriginalNestedReset)-start;}
    need(finds==3&&casts==2&&resets==10&&!t::page_read_scope,"three actual Find and two RTTI calls invalidate enclosing metadata on entry and return");
    fixture.setup(true);DWORD previous=0;bool protected_page=false;
    auto protect=[&](uint64_t uid,uintptr_t& found)->uint32_t{const auto result=lookup(uid,found);if(!protected_page){protected_page=VirtualProtect(reinterpret_cast<void*>(fixture.map),4096,PAGE_NOACCESS,&previous)!=0;need(protected_page,"protect only owned Map page inside original Find");}return result;};
    const bool okay=current_roster(at,protect,cast,fixture,true,ready,error);DWORD replaced=0;
    need(protected_page&&VirtualProtect(reinterpret_cast<void*>(fixture.map),4096,previous,&replaced)!=0,"restore only owned Map page after original Find");
    need(!okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"final Region array read queries changed protection after original Find");
    fixture.setup(true);bool key_protected=false;remove_diagnostics::key_callback=[&](uintptr_t,std::string&){key_protected=VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,PAGE_NOACCESS,&previous)!=0;need(key_protected,"protect only owned manager page inside original key resolver");return true;};exterior.clear();
    const bool exterior_okay=exteriors(at,fixture,true,exterior,error);
    need(key_protected&&VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,previous,&replaced)!=0,"restore owned manager page after original key resolver");
    need(!exterior_okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"key callback barrier preserves fresh final Building array protection check");
    fixture.setup(true);auto failed=[](uint64_t,uintptr_t&)->uint32_t{return 123;};
    need(!current_roster(at,failed,cast,fixture,true,ready,error)&&error=="Dungeon current scene full UID lookup SEH=123"&&!t::page_read_scope,"original native Find failure remains visible after metadata unwind");
    fixture.setup(true);auto foreign=[](uintptr_t,uintptr_t,uintptr_t,uintptr_t& found)->uint32_t{found=0;return 0;};
    need(!census_roster(at,lookup,foreign,fixture,true,ready,error)&&error=="Dungeon scene original RTTI receiver differs SEH=0"&&!t::page_read_scope,"original RTTI receiver guard remains visible");
    fixture.setup(true);auto throws=[](uint64_t,uintptr_t&)->uint32_t{throw std::runtime_error("original Dungeon Find exception");};bool caught=false;
    try{census_roster(at,throws,cast,fixture,true,ready,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original Dungeon Find exception";}
    need(caught&&!t::page_read_scope,"original Dungeon Find exception unwinds callback and complete reader scopes");
    remove_diagnostics::key_callback={};std::cout<<"Dungeon121-Region roster reads="<<roster_trace.size()<<" OS queries="<<baseline_queries<<"->"<<scoped_queries<<" exterior reads/queries="<<exterior_baseline<<"->1\n";
    t::totals={};
}
