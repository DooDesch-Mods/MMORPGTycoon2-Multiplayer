#include "party_saved_identity.h"
#include "dungeon_party_census.h"

namespace party_pool_scope_fixture {
namespace live_host_state=mt2mp::game::live_host_state;
namespace dungeon_party_census=mt2mp::game::dungeon_party_census;
namespace q=mt2mp::game::party_saved_identity;
struct Fixture:region_scope_fixture::Fixture {
    uintptr_t mutate_address=0;unsigned mutate_hits=0;std::function<void()> mutation;
    uintptr_t manager(size_t n)const{return uintptr_t(memory)+0x50000+n*0x100;}
    uintptr_t party()const{return uintptr_t(memory)+0x78000;}
    void array(size_t region_index,size_t offset,uintptr_t ap,uintptr_t slots_address,int32_t count){const auto owner=manager(region_index)+offset;
        put(owner,image+ap);put(owner+8,slots_address);put(owner+0x10,count);put(owner+0x14,count);}
    void setup(bool member=false){reset();mutate_address=0;mutate_hits=0;mutation={};
        for(size_t n=0;n<121;++n){put(region(n)+0x9d0,manager(n));put(manager(n),image+dungeon_party_census::manager_ap);put(manager(n)+0xa8,region(n));
            array(n,0x18,dungeon_party_census::parties_ap,0,0);array(n,0x58,dungeon_party_census::pending_ap,0,0);array(n,0x80,dungeon_party_census::pending_ap,0,0);}
        if(member){const auto slots=uintptr_t(memory)+0x3000;put(slots,party());
            array(0,0x18,dungeon_party_census::parties_ap,slots,1);array(0,0x58,dungeon_party_census::pending_ap,slots,1);array(0,0x80,dungeon_party_census::pending_ap,slots,1);
            put(party(),image+dungeon_party_census::party_ap);put(party()+0x18,uint64_t(0x1122334455667788));put(party()+0x170,root);put(party()+0x178,region(1));put(party()+0x620,region(1));}}
    struct Memory {Fixture& fixture;
        bool read(uintptr_t address,void* out,size_t size,std::string& error){auto& f=fixture;
            if(address==f.throw_address)throw std::runtime_error("Party region read exception");
            if(address==f.mutate_address&&++f.mutate_hits==2)f.mutation();
            if(!mt2mp::game::root_saved_building::bytes(reinterpret_cast<void*>(address),0,out,size,error))return false;
            const auto* bytes=static_cast<const unsigned char*>(out);f.events.push_back({address,{bytes,bytes+size}});return true;}
    } reader{*this};
};
struct Ready {std::vector<dungeon_party_census::Pool> pools;};
bool region_headers(Fixture& fixture,bool scoped,q::Bounds& regions,std::vector<std::pair<uintptr_t,uintptr_t>>& owners,std::string& error){
    auto& memory=fixture.reader;const auto image=fixture.image,map=fixture.map;
    struct Current {uintptr_t root;};const Current current{fixture.root};owners.clear();regions={};
    if(scoped){
#include "party_region_loop.inc"
    }else{
#include "party_region_baseline.inc"
    }
    return true;
}
template<class Call>bool run(Fixture& fixture,Call& exact_uid,bool scoped,Ready& ready,std::map<uint64_t,dungeon_party_census::Member>& members,std::string& error){
    auto& memory=fixture.reader;const auto image=fixture.image;
    std::vector<std::pair<uintptr_t,uintptr_t>> owners;for(size_t n=0;n<121;++n)owners.emplace_back(fixture.root,fixture.region(n));
    std::set<uintptr_t> managers;ready={};members.clear();
#include "party_pool_find.inc"
    if(scoped){
#include "party_pool_loop.inc"
    }else{
#include "party_pool_baseline.inc"
    }
    return true;
}
}

void party_pool_scope_cases(){
    using namespace party_pool_scope_fixture;Fixture fixture;Ready baseline,scoped;std::map<uint64_t,dungeon_party_census::Member> before,after;std::string error;
    unsigned finds=0;auto find=[&](uint64_t uid,uintptr_t& found)->uint32_t{++finds;found=uid==0x1122334455667788?fixture.party():0;return 0;};
    t::totals={};t::Scope capture(t::Part::HostCapture);fixture.setup();auto start=count(t::Part::HostMemoryVirtualQuery);
    q::Bounds initial,actual;std::vector<std::pair<uintptr_t,uintptr_t>> original_owners,current_owners;
    need(region_headers(fixture,false,initial,original_owners,error),"baseline Party Region headers and pointers");const auto region_trace=fixture.events;const auto header_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);
    need(region_headers(fixture,true,actual,current_owners,error),"actual scoped Party Region headers and pointers");const auto region_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(region_trace.size()==126&&fixture.events==region_trace&&initial==actual&&original_owners==current_owners&&current_owners.size()==121,"Party Region scope preserves all126 field reads and121 owner pairs");
    need(header_queries==126&&region_queries==1&&!t::page_read_scope,"Party Region metadata scope closes before scene roster boundary");
    fixture.put(fixture.data+120*8,fixture.region(0));need(region_headers(fixture,true,actual,current_owners,error)&&current_owners[120].second==fixture.region(0),"new Region extent observes changed current pointer");
    fixture.put(fixture.map+0x240,int32_t(120));need(!q::unchanged(fixture.reader,fixture.map+0x230,4096,actual,error)&&error=="quest target array changed during read","final Region bounds proof still rejects drift after scoped enumeration");
    for(bool wrong_class:{false,true}){
        const auto corrupt=[&]{if(wrong_class)fixture.put(fixture.map,uintptr_t(0));else fixture.put(fixture.map+0x244,int32_t(120));};
        fixture.setup();corrupt();need(!region_headers(fixture,false,initial,original_owners,error),"baseline Party Region invalid header");const auto invalid_trace=fixture.events;const auto original_cause=error;
        fixture.setup();corrupt();need(!region_headers(fixture,true,actual,current_owners,error)&&fixture.events==invalid_trace&&error==original_cause&&!t::page_read_scope,"scoped Party Region invalid header retains original cause and reads");
    }
    fixture.setup();fixture.throw_address=fixture.data+120*8;bool region_threw=false;
    try{region_headers(fixture,true,actual,current_owners,error);}catch(const std::runtime_error& e){region_threw=std::string(e.what())=="Party region read exception";}
    need(region_threw&&!t::page_read_scope,"Party Region read exception closes metadata extent");
    fixture.setup();DWORD region_previous=0,region_replaced=0;
    need(VirtualProtect(reinterpret_cast<void*>(fixture.data),4096,PAGE_NOACCESS,&region_previous)!=0,"protect owned Region pointer array");
    const bool region_okay=region_headers(fixture,true,actual,current_owners,error);
    need(VirtualProtect(reinterpret_cast<void*>(fixture.data),4096,region_previous,&region_replaced)!=0,"restore owned Region pointer array");
    need(!region_okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"Party Region pointer read failure remains visible and closes extent");
    std::cout<<"Party121 Region header reads="<<region_trace.size()<<" OS queries="<<header_queries<<"->"<<region_queries<<'\n';
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);
    need(run(fixture,find,false,baseline,before,error),"baseline complete121-Region current Party loop");const auto trace=fixture.events;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(trace.size()==3993&&baseline_queries==3993&&baseline.pools.size()==121&&before.empty()&&finds==0,"empty managers retain33 reads each,121 pools and all three initial/final arrays");
    fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(run(fixture,find,true,scoped,after,error),"extracted production complete Party pool scope");const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(fixture.events==trace&&scoped.pools==baseline.pools&&after==before&&scoped_queries==1&&!t::page_read_scope,"all3993 values/order/bytes and121 complete pools preserved with one metadata query");
    fixture.put(fixture.manager(120)+0xb0,uint8_t(1));need(run(fixture,find,true,scoped,after,error)&&scoped.pools[120].updating==1,"next extent reads current manager updating flag");
    fixture.setup(true);finds=0;need(run(fixture,find,false,baseline,before,error),"baseline Party transfer with pending removal and release arrays");const auto member_trace=fixture.events;
    need(member_trace.size()==4009&&finds==1,"transfer fixture retains4009 actual reads and one original Find");
    fixture.setup(true);finds=0;uint64_t resets=0;
    {t::Scope take(t::Part::Take);start=count(t::Part::OriginalNestedReset);need(run(fixture,find,true,scoped,after,error),"scoped transfer with original callback");resets=count(t::Part::OriginalNestedReset)-start;}
    need(fixture.events==member_trace&&baseline.pools==scoped.pools&&before==after&&finds==1&&resets==2&&!t::page_read_scope,"transfer owner values and both pending arrays stay exact; Find invalidates metadata twice");
    need(scoped.pools[0].members[0].region==fixture.region(1)&&scoped.pools[0].members[0].manager_region==fixture.region(1)&&scoped.pools[0].removals.members.size()==1&&scoped.pools[0].releases.members.size()==1,"current transferred owner and nonempty pending queues remain visible");
    auto mutate=[&]{fixture.mutate_address=fixture.manager(120)+0x28;fixture.mutation=[&]{fixture.put(fixture.manager(120)+0x28,int32_t(1));};};
    fixture.setup();mutate();need(!run(fixture,find,false,baseline,before,error),"baseline final Party array reread rejects changed count");const auto rejected=fixture.events;const auto cause=error;
    fixture.setup();mutate();need(!run(fixture,find,true,scoped,after,error)&&fixture.events==rejected&&error==cause&&!t::page_read_scope,"complete scoped final array reread preserves fresh bytes and original cause");
    fixture.setup();fixture.put(fixture.manager(120)+0xa8,fixture.region(0));need(!run(fixture,find,true,scoped,after,error)&&error=="Party census manager class/reciprocal Region/update flag differs"&&!t::page_read_scope,"last Region reciprocal owner guard remains active");
    fixture.setup(true);DWORD previous=0;bool protected_page=false;
    auto protect=[&](uint64_t uid,uintptr_t& found)->uint32_t{const auto result=find(uid,found);protected_page=VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,PAGE_NOACCESS,&previous)!=0;need(protected_page,"protect only owned Party manager page inside original Find");return result;};
    const bool okay=run(fixture,protect,true,scoped,after,error);DWORD replaced=0;
    need(protected_page&&VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,previous,&replaced)!=0,"restore owned Party manager page after original Find");
    need(!okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"post-Find final arrays query current page protection");
    fixture.setup(true);auto failed=[](uint64_t,uintptr_t&)->uint32_t{return 123;};
    need(!run(fixture,failed,true,scoped,after,error)&&error=="Party original full UID lookup SEH=123"&&!t::page_read_scope,"original Party Find failure remains visible");
    auto throws=[](uint64_t,uintptr_t&)->uint32_t{throw std::runtime_error("original Party Find exception");};bool caught=false;
    try{run(fixture,throws,true,scoped,after,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original Party Find exception";}
    need(caught&&!t::page_read_scope,"original Party Find exception unwinds callback and reader scope");
    std::cout<<"Party121 pool reads="<<trace.size()<<" OS queries="<<baseline_queries<<"->"<<scoped_queries<<" transfer reads="<<member_trace.size()<<" original Find calls=1\n";
    t::totals={};
}
