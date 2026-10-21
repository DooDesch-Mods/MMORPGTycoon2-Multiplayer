#include "dungeon_zone_roster.h"

namespace world_zone_manager_fixture {
namespace live_host_state=mt2mp::game::live_host_state;
namespace dungeon_zone_roster=mt2mp::game::dungeon_zone_roster;
struct Current {
    std::string key,scene;uintptr_t object=0,root=0,region=0,manager=0;uint64_t uid=0;int32_t type=-1;
    bool operator==(const Current&)const=default;
};
namespace q {
struct SavedZone {uintptr_t pointer=0;uint64_t uid=0;};
struct SavedPool {struct {std::vector<SavedZone> zones;} array;};
bool fail(std::string& error,const char* cause){error=cause;return false;}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
std::string zone_key(const SavedPool&,const SavedZone& row){return "monster:"+std::to_string(row.uid);}
}
struct Pool {uintptr_t root=0,region=0;std::string scene;std::map<uint64_t,Current> rows;};
struct Fixture:region_scope_fixture::Fixture {
    bool has_member=false;
    uintptr_t manager(size_t n)const{return uintptr_t(memory)+0x50000+n*0x100;}
    uintptr_t zone()const{return uintptr_t(memory)+0x78000;}
    void setup(bool member=false){reset();has_member=member;
        for(size_t n=0;n<121;++n){put(region(n)+0x968,manager(n));put(manager(n),image+0x16fe170);put(manager(n)+0xf0,region(n));
            for(size_t type=0;type<4;++type){const auto array=manager(n)+0x18+type*0x30;
                put(array,image+0x172e1e0);put(array+8,uintptr_t(0));put(array+0x10,int32_t(0));put(array+0x14,int32_t(0));}}
        if(member){const auto slots=uintptr_t(memory)+0x3000;put(slots,zone());const auto array=manager(0)+0x18;
            put(array+8,slots);put(array+0x10,int32_t(1));put(array+0x14,int32_t(1));
            put(zone(),image+0x1745ce0);put(zone()+0x18,uint64_t(0x1122334455667788));
            put(zone()+0x170,root);put(zone()+0x178,region(0));put(zone()+0x24c,int32_t(0));}}
};
template<bool Scoped,class Find>bool run(Fixture& fixture,Find& exact_uid,std::vector<Current>& out,std::string& error,bool known=true){
    const auto image=fixture.image;
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.read(object,offset,value,cause);};
    std::map<uintptr_t,Pool> world_zone_pools;
    if(known)world_zone_pools[fixture.manager(0)]={fixture.root,fixture.region(0),"root",{{0x1122334455667788,{"zone:fixture","root",fixture.zone(),fixture.root,fixture.region(0),fixture.manager(0),0x1122334455667788,0}}}};
    std::vector<Current> next;std::set<std::string> keys;std::set<uintptr_t> managers,pointers;std::set<uint64_t> uids;
    // Empty confirmed pools have no live alias. The nonempty case admits only
    // the fixture's exact full UID through the actual production alias guards.
    if(!fixture.has_member)world_zone_pools.clear();
    auto visit=[&](auto& add){q::SavedPool monsters;
        for(size_t n=0;n<121;++n)if(!add(fixture.manager(n),fixture.root,fixture.region(n),"root",monsters))return false;
        out=std::move(next);return true;};
    if constexpr(Scoped){
#include "world_zone_manager_scoped.inc"
        return visit(add);
    }else{
#include "world_zone_manager_baseline.inc"
        return visit(add);
    }
}
}

namespace world_zone_manager_fixture {
namespace region_saved_identity=mt2mp::game::region_saved_identity;
template<bool Scoped,class Between>bool region_pair(Fixture& fixture,Between& between,region_saved_identity::Index& region_index,std::string& error){
    const auto image=fixture.image,map=fixture.map,root=fixture.root;
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.read(object,offset,value,cause);};
    if constexpr(Scoped){
#include "world_zone_regions_scoped.inc"
        if(!capture_regions()||!between())return false;
        return verify_regions();
    }else{
#include "world_zone_regions_baseline.inc"
        if(!capture_regions()||!between())return false;
        return verify_regions();
    }
}
}

void world_zone_index_scope_cases(){
    using namespace world_zone_manager_fixture;Fixture fixture;std::string error;
    mt2mp::game::region_saved_identity::Index baseline,scoped;
    auto between=[&](){need(!t::page_read_scope,"Region metadata extent closes before World Zone managers and original calls");return true;};
    t::totals={};t::Scope capture(t::Part::HostCapture);fixture.reset();auto start=count(t::Part::HostMemoryVirtualQuery);
    need(region_pair<false>(fixture,between,baseline,error),"baseline World Zone initial/final complete Region scans");
    const auto trace=fixture.events;const auto before_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(trace.size()==1002&&before_queries==1002&&baseline.saved.size()==121,"both original501-field scans remain complete");
    fixture.reset();start=count(t::Part::HostMemoryVirtualQuery);SetLastError(123);
    need(region_pair<true>(fixture,between,scoped,error),"actual World Zone Region extent bodies");
    const auto after_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(fixture.events==trace&&scoped==baseline&&after_queries==2&&!t::page_read_scope,"two separate Region extents retain all1002 values and order with two OS metadata queries");
    need(GetLastError()==123,"Region metadata extents preserve original LastError");
    auto corrupt=[&](){need(!t::page_read_scope,"final Region scan starts after original-boundary extent");fixture.put(fixture.region(120)+0x834,int32_t(1000));return true;};
    fixture.reset();error.clear();need(!region_pair<false>(fixture,corrupt,baseline,error),"baseline final Region identity drift rejected");
    const auto cause=error;const auto drift=fixture.events;
    fixture.reset();need(!region_pair<true>(fixture,corrupt,scoped,error)&&error==cause&&fixture.events==drift&&!t::page_read_scope,"final scoped scan reads changed Region fields and retains exact mismatch cause");
    fixture.reset();fixture.put(fixture.region(120)+0x70,fixture.root+8);error.clear();
    need(!region_pair<false>(fixture,between,baseline,error),"baseline first scan rejects final foreign Region");
    const auto foreign_cause=error;const auto rejected=fixture.events;
    fixture.reset();fixture.put(fixture.region(120)+0x70,fixture.root+8);
    need(!region_pair<true>(fixture,between,scoped,error)&&error==foreign_cause&&fixture.events==rejected&&!t::page_read_scope,"first scoped scan cannot bypass final Region owner guard");
    fixture.reset();DWORD previous=0;bool protected_page=false;
    auto protect=[&](){need(!t::page_read_scope,"Region scope closes before original boundary");
        t::PageReadScope original(t::PageBoundary::OriginalCall);
        protected_page=VirtualProtect(reinterpret_cast<void*>(fixture.region(0)),4096,PAGE_NOACCESS,&previous)!=0;
        need(protected_page,"protect only owned Region page between initial and final scans");return true;};
    const bool okay=region_pair<true>(fixture,protect,scoped,error);DWORD replaced=0;
    need(protected_page&&VirtualProtect(reinterpret_cast<void*>(fixture.region(0)),4096,previous,&replaced)!=0,"restore owned Region page after original boundary");
    need(!okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"final Region scan queries current page protection after original boundary");
    fixture.reset();auto throws=[&](){fixture.throw_address=fixture.region(120)+0x834;return true;};bool caught=false;
    try{region_pair<true>(fixture,throws,scoped,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original region read exception";}
    need(caught&&!t::page_read_scope,"final Region extent unwinds on original read exception");
    std::cout<<"World Zone Region reads="<<trace.size()<<" OS queries="<<before_queries<<"->"<<after_queries<<" separate extents=2\n";
    t::totals={};
}

void world_zone_manager_scope_cases(){
    world_zone_index_scope_cases();
    using namespace world_zone_manager_fixture;Fixture fixture;std::string error;std::vector<Current> baseline,scoped;
    unsigned finds=0;auto find=[&](uint64_t uid,uintptr_t& found)->uint32_t{++finds;found=uid==0x1122334455667788?fixture.zone():0;return 0;};
    t::totals={};t::Scope capture(t::Part::HostCapture);fixture.setup();auto start=count(t::Part::HostMemoryVirtualQuery);
    need(run<false>(fixture,find,baseline,error),"baseline complete121-Region World Zone manager census");
    const auto trace=fixture.events;const auto before_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(trace.size()>3000&&before_queries==trace.size()&&baseline.empty()&&finds==0,"empty World Zones retain every four-array and reciprocal owner read");
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(run<true>(fixture,find,scoped,error),"actual scoped World Zone manager body");
    const auto after_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(fixture.events==trace&&scoped==baseline&&after_queries<=242&&after_queries<before_queries&&!t::page_read_scope,"scoped World Zone census preserves all read bytes/order with bounded metadata queries");
    fixture.setup(true);finds=0;need(run<false>(fixture,find,baseline,error)&&baseline.size()==1&&finds==2,"baseline current Zone retains both original full UID calls");const auto member_trace=fixture.events;
    fixture.setup(true);finds=0;auto checked_find=[&](uint64_t uid,uintptr_t& found)->uint32_t{
        need(t::page_read_scope&&!t::PageReadScope::reader_active(),"actual World Zone FindUID cannot borrow reader metadata");return find(uid,found);};
    need(run<true>(fixture,checked_find,scoped,error)&&scoped==baseline&&fixture.events==member_trace&&finds==2&&!t::page_read_scope,"current non-Monster Zone alias, UID calls and complete field order remain exact");
    fixture.setup(true);error.clear();need(!run<false>(fixture,find,baseline,error,false),"baseline foreign Zone alias rejected");const auto cause=error;const auto rejected=fixture.events;
    fixture.setup(true);need(!run<true>(fixture,find,scoped,error,false)&&error==cause&&fixture.events==rejected&&!t::page_read_scope,"scoped census retains foreign alias rejection and original cause");
    fixture.setup();fixture.put(fixture.manager(120)+0xf0,fixture.region(0));need(!run<false>(fixture,find,baseline,error),"baseline final Region reciprocal owner fails");const auto owner_cause=error;const auto owner_trace=fixture.events;
    fixture.setup();fixture.put(fixture.manager(120)+0xf0,fixture.region(0));need(!run<true>(fixture,find,scoped,error)&&error==owner_cause&&fixture.events==owner_trace&&!t::page_read_scope,"last Region is fully checked after metadata reuse");
    fixture.setup(true);DWORD previous=0;bool protected_page=false;
    auto protect=[&](uint64_t uid,uintptr_t& found)->uint32_t{const auto result=find(uid,found);protected_page=VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,PAGE_NOACCESS,&previous)!=0;need(protected_page,"protect only owned World Zone manager page inside original FindUID");return result;};
    const bool okay=run<true>(fixture,protect,scoped,error);DWORD replaced=0;
    need(protected_page&&VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,previous,&replaced)!=0,"restore owned World Zone manager protection");
    need(!okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"post-FindUID array reads requery current page protection");
    fixture.setup(true);auto failed=[](uint64_t,uintptr_t&)->uint32_t{return 123;};
    need(!run<true>(fixture,failed,scoped,error)&&error=="World Zone current FindUID failed exception=123"&&!t::page_read_scope,"original World Zone FindUID failure remains visible");
    auto throws=[](uint64_t,uintptr_t&)->uint32_t{throw std::runtime_error("original World Zone FindUID exception");};bool caught=false;
    try{run<true>(fixture,throws,scoped,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original World Zone FindUID exception";}
    need(caught&&!t::page_read_scope,"World Zone reader and original-call scopes unwind on exception");
    std::cout<<"World Zone121 manager reads="<<trace.size()<<" OS queries="<<before_queries<<"->"<<after_queries<<" current Zone original FindUID calls=2\n";
    t::totals={};
}
