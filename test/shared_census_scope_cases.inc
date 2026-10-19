#include "intervention_monster_roster.h"
#include "character_boss_retirement_read.h"

namespace observed_building_identity {
inline std::vector<definition_scope_fixture::ReadEvent>* events=nullptr;
template<class T>bool read(const void* object,size_t offset,T& out,std::string& error){
    if(!mt2mp::game::root_saved_building::read(object,offset,out,error))return false;
    const auto* bytes=reinterpret_cast<const unsigned char*>(&out);events->push_back({uintptr_t(object)+offset,{bytes,bytes+sizeof out}});return true;}
#include "shared_building_type.inc"
#include "shared_building_baseline.inc"
#include "shared_building_candidate.inc"
}
namespace shared_census_fixture {
namespace live_host_state=mt2mp::game::live_host_state;
namespace interventions=mt2mp::game::interventions;
namespace monster_zone_read=mt2mp::game::monster_zone_read;
namespace character_boss_retirement_read=mt2mp::game::character_boss_retirement_read;
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
inline std::function<uint32_t(uint64_t,uintptr_t&)> boss_lookup;
inline uint32_t lookup(uintptr_t,uint64_t uid,uintptr_t& found){return boss_lookup(uid,found);}
struct Find {uintptr_t image;
#include "shared_boss_find.inc"
};
struct BossFixture:party_pool_scope_fixture::Fixture {
    void setup_boss(bool member=false){setup();
        for(size_t n=0;n<121;++n){put(region(n)+0x990,manager(n));put(manager(n),image+0x16fa780);put(manager(n)+0x68,region(n));put(manager(n)+0x38,image+0x172dfa0);}
        if(member){put(manager(0)+0x40,uintptr_t(memory)+0x3100);put(manager(0)+0x48,int32_t(1));put(manager(0)+0x4c,int32_t(1));put(uintptr_t(memory)+0x3100,party());
            put(party(),image+0x1743040);put(party()+0x18,uint64_t(0x1122334455667788));put(party()+0x170,root);put(party()+0x178,region(0));put(image+0x1743040+0x438,image+0x3976f0);}}
};
template<class Read>bool boss_candidate(Read& at,Find& find,uintptr_t image,uintptr_t shard,uintptr_t region,character_boss_retirement_read::Pool& out,std::string& error){
    struct Pool {character_boss_retirement_read::Pool& native;};Pool pool{out};
#include "shared_boss_call.inc"
}
struct MonsterFixture {
    void* allocation=VirtualAlloc(nullptr,0x500000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uintptr_t image=0,map=0,root=0,mutate_address=0;unsigned hits=0;std::function<void()> mutation;
    std::vector<definition_scope_fixture::ReadEvent> events;
    MonsterFixture(){need(allocation!=nullptr,"owned complete Monster pool allocation");image=uintptr_t(allocation)-0x1640000;map=uintptr_t(allocation)+0x1000;root=map+0x1c0;setup();}
    ~MonsterFixture(){if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"Monster pool fixture release failed "<<GetLastError()<<'\n';}
    template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    uintptr_t region(size_t n)const{return uintptr_t(allocation)+0x100000+n*0x1000;}
    uintptr_t building_manager(size_t n)const{return uintptr_t(allocation)+0x200000+n*0x1000;}
    uintptr_t zone_manager(size_t n)const{return uintptr_t(allocation)+0x280000+n*0x1000;}
    uintptr_t monster_manager(size_t n)const{return uintptr_t(allocation)+0x300000+n*0x1000;}
    uintptr_t monster()const{return uintptr_t(allocation)+0x480000;}
    void setup(bool member=false){std::memset(allocation,0,0x500000);events.clear();mutate_address=0;hits=0;mutation={};
        put(image+0x164b250,uintptr_t(allocation)+0x400000);
        for(size_t n=0;n<121;++n){put(region(n)+0x70,root);put(region(n)+0x950,building_manager(n));put(region(n)+0x968,zone_manager(n));put(region(n)+0x988,monster_manager(n));
            put(building_manager(n),image+0x17183c0);put(building_manager(n)+0x450,root);put(building_manager(n)+0x458,region(n));
            put(zone_manager(n),image+0x16fe170);put(zone_manager(n)+0xf0,region(n));put(zone_manager(n)+0x48,image+0x172e1e0);
            put(monster_manager(n),image+0x1714100);put(monster_manager(n)+0x80,region(n));put(monster_manager(n)+0x38,image+0x16e8b60);
            put(region(n)+0x160,uintptr_t(allocation)+0x400100+n*0x40);}
        if(member){const auto object=monster();put(monster_manager(0)+0x40,uintptr_t(allocation)+0x481000);put(monster_manager(0)+0x48,int32_t(1));put(monster_manager(0)+0x4c,int32_t(1));
            put(uintptr_t(allocation)+0x481000,object);put(object,image+0x16c0770);put(object+0x18,uint64_t(0x100000001));put(object+0x170,root);put(object+0x178,region(0));}}
    struct Memory {MonsterFixture& fixture;
        bool read(uintptr_t address,void* out,size_t size,std::string& error){auto& f=fixture;if(address==f.mutate_address&&++f.hits==2)f.mutation();
            if(!mt2mp::game::root_saved_building::bytes(reinterpret_cast<void*>(address),0,out,size,error))return false;
            const auto* bytes=static_cast<const unsigned char*>(out);f.events.push_back({address,{bytes,bytes+size}});return true;}
        bool copy(uintptr_t address,void* out,size_t size,std::string& error){return read(address,out,size,error);}
    } memory{*this};
};
template<class Memory>bool monster_candidate(Memory& memory,uintptr_t image,uintptr_t map,const mt2mp::game::boss_identity::Scope& scope,monster_zone_read::State& monsters,std::string& error){
    using interventions::monster_roster;
#include "shared_monster_call.inc"
}
bool monster_run(MonsterFixture& fixture,bool scoped,std::vector<monster_zone_read::State>& out,std::string& error){out.clear();
    for(size_t n=0;n<121;++n){mt2mp::game::boss_identity::Scope scope{"12345678-1234-4234-8234-123456789012","world",1,1,fixture.root,fixture.root,fixture.region(n),fixture.monster_manager(n)};
        monster_zone_read::State value;const bool okay=scoped?monster_candidate(fixture.memory,fixture.image,fixture.map,scope,value,error):interventions::monster_roster(fixture.memory,fixture.image,fixture.map,scope,value,error);
        if(!okay)return false;out.push_back(value);}return true;}
}

void shared_census_scope_cases(){
    using namespace shared_census_fixture;std::string error;t::totals={};t::Scope capture(t::Part::HostCapture);
    {
        MonsterFixture fixture;std::vector<monster_zone_read::State> baseline,scoped;auto start=count(t::Part::HostMemoryVirtualQuery);
        need(monster_run(fixture,false,baseline,error),"baseline121 complete Monster pools");const auto trace=fixture.events;const auto queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(trace.size()==3751&&queries==3751&&baseline.size()==121,"complete Monster pool has31 actual reads including both RNG states and final manager array");
        fixture.setup();start=count(t::Part::HostMemoryVirtualQuery);need(monster_run(fixture,true,scoped,error),"actual complete Monster pool scopes");const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        std::cout<<"Monster scope diagnostic reads="<<fixture.events.size()<<" expected="<<trace.size()<<" trace_equal="<<(fixture.events==trace)<<" output_equal="<<(baseline==scoped)<<" OS queries="<<scoped_queries<<" expected=242 scope_open="<<(t::page_read_scope!=nullptr)<<'\n';
        need(fixture.events==trace,"all3751 Monster read addresses/order/bytes remain exact");
        need(baseline==scoped&&!t::page_read_scope,"all121 Monster outputs remain exact and metadata scope closes");
        need(scoped_queries==242,"each Monster pool queries its Region suffix then its lower RNG singleton allocation prefix");
        fixture.setup(true);need(monster_run(fixture,false,baseline,error),"baseline actual direct Monster member");const auto member_trace=fixture.events;
        need(member_trace.size()==3767,"one direct Monster adds all16 current field reads");
        fixture.setup(true);need(monster_run(fixture,true,scoped,error)&&fixture.events==member_trace&&baseline==scoped,"direct Monster transform/owner/full UID and RNG values remain exact");
        fixture.put(fixture.monster()+0x18,uint64_t(0x900000001));need(monster_run(fixture,true,scoped,error)&&scoped[0].monsters[0].uid==0x900000001,"next Monster extent reads current full UID");
        auto mutate=[&]{fixture.mutate_address=fixture.monster_manager(120)+0x48;fixture.mutation=[&]{fixture.put(fixture.monster_manager(120)+0x48,int32_t(1));};};
        fixture.setup();mutate();need(!monster_run(fixture,false,baseline,error),"baseline final Monster array guard rejects change");const auto rejected=fixture.events;const auto cause=error;
        fixture.setup();mutate();need(!monster_run(fixture,true,scoped,error)&&fixture.events==rejected&&error==cause&&!t::page_read_scope,"Monster final guard retains same current reads and failure cause");
        std::cout<<"Monster121 pool reads="<<trace.size()<<" OS queries="<<queries<<"->"<<scoped_queries<<'\n';
    }
    {
        BossFixture fixture;fixture.setup_boss();Find find{fixture.image};unsigned finds=0;boss_lookup=[&](uint64_t uid,uintptr_t& found)->uint32_t{++finds;found=uid==0x1122334455667788?fixture.party():0;return 0;};
        auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.reader.read(object+offset,&value,sizeof value,cause);};
        auto run=[&](bool scoped,std::vector<character_boss_retirement_read::Pool>& out){out.clear();for(size_t n=0;n<121;++n){character_boss_retirement_read::Pool value;
            const bool okay=scoped?boss_candidate(at,find,fixture.image,fixture.root,fixture.region(n),value,error):character_boss_retirement_read::census(fixture.image,fixture.root,fixture.region(n),at,find,value,error);
            if(!okay)return false;out.push_back(value);}return true;};
        std::vector<character_boss_retirement_read::Pool> baseline,scoped;auto start=count(t::Part::HostMemoryVirtualQuery);
        need(run(false,baseline),"baseline121 complete Boss pools");const auto trace=fixture.events;const auto queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(trace.size()==1452&&queries==1452&&finds==0,"complete empty Boss pools retain12 reads each");
        fixture.setup_boss();start=count(t::Part::HostMemoryVirtualQuery);need(run(true,scoped),"actual Boss complete pool scopes");const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(fixture.events==trace&&baseline==scoped&&scoped_queries==121&&!t::page_read_scope,"all1452 Boss reads and121 current pools stay exact");
        fixture.setup_boss(true);finds=0;need(run(false,baseline),"baseline complete Boss with current native member");const auto member_trace=fixture.events;need(member_trace.size()==1458&&finds==1,"one Boss adds6 current reads and one Find");
        fixture.setup_boss(true);finds=0;uint64_t resets=0;{t::Scope take(t::Part::Take);start=count(t::Part::OriginalNestedReset);need(run(true,scoped),"actual Boss member callback boundary");resets=count(t::Part::OriginalNestedReset)-start;}
        need(fixture.events==member_trace&&baseline==scoped&&finds==1&&resets==2&&!t::page_read_scope,"Boss full UID/member checks retain one Find and entry/return invalidation");
        fixture.setup_boss(true);DWORD previous=0;bool protected_page=false;boss_lookup=[&](uint64_t,uintptr_t& found)->uint32_t{found=fixture.party();protected_page=VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,PAGE_NOACCESS,&previous)!=0;need(protected_page,"protect only owned Boss manager inside Find");return 0;};
        const bool okay=run(true,scoped);DWORD replaced=0;need(protected_page&&VirtualProtect(reinterpret_cast<void*>(fixture.manager(0)),4096,previous,&replaced)!=0,"restore owned Boss manager after Find");
        need(!okay&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"Boss final manager guard reads current protection after callback");
        fixture.setup_boss(true);boss_lookup=[](uint64_t,uintptr_t&)->uint32_t{return 123;};need(!run(true,scoped)&&error=="Boss current full UID lookup SEH=123"&&!t::page_read_scope,"original Boss Find cause remains visible");
        boss_lookup=[](uint64_t,uintptr_t&)->uint32_t{throw std::runtime_error("original Boss Find exception");};bool caught=false;
        try{run(true,scoped);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original Boss Find exception";}
        need(caught&&!t::page_read_scope,"original Boss Find exception unwinds native and reader metadata extents");
        boss_lookup={};std::cout<<"Boss121 pool reads="<<trace.size()<<" OS queries="<<queries<<"->"<<scoped_queries<<'\n';
    }
    {
        party_pool_scope_fixture::Fixture fixture;fixture.setup();const auto manager=fixture.manager(0);
        fixture.put(fixture.region(0)+0x950,manager);fixture.put(manager,fixture.image+0x17183c0);fixture.put(manager+0x450,fixture.root);fixture.put(manager+0x458,fixture.region(0));
        observed_building_identity::events=&fixture.events;
        auto read=[&](bool scoped,int32_t& id){auto map=reinterpret_cast<void*>(fixture.map),region=reinterpret_cast<void*>(fixture.region(0)),owner=reinterpret_cast<void*>(manager),root=reinterpret_cast<void*>(fixture.root);
            return scoped?observed_building_identity::instrumented_region_id(fixture.image,map,region,owner,root,id,error):observed_building_identity::unscoped_region_id(fixture.image,map,region,owner,root,id,error);};
        int32_t before=-1,after=-1;auto start=count(t::Part::HostMemoryVirtualQuery);need(read(false,before),"baseline complete root Building Region identity");const auto queries=count(t::Part::HostMemoryVirtualQuery)-start;const auto trace=fixture.events;
        need(queries==504&&trace.size()==504&&before==0,"Building identity keeps504 original native field reads for121 saved Regions and one owned subset member");
        fixture.events.clear();
        start=count(t::Part::HostMemoryVirtualQuery);need(read(true,after)&&after==before,"actual pure complete Building Region identity");const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
        need(scoped_queries==1&&fixture.events==trace&&!t::page_read_scope,"Building identity preserves all504 read addresses/order/bytes with one metadata query");
        need(mt2mp::game::root_saved_building::region_id(fixture.image,reinterpret_cast<void*>(fixture.map),reinterpret_cast<void*>(fixture.region(0)),reinterpret_cast<void*>(manager),reinterpret_cast<void*>(fixture.root),after,error)&&after==before,"production Building identity returns same selected saved Region ID");
        fixture.put(fixture.region(120)+0x834,int32_t(0));need(!read(true,after)&&error.find("ID repeated")!=std::string::npos&&!t::page_read_scope,"Building saved Region duplicate-ID guard reads fresh values on next invocation");
        std::cout<<"Building Region identity native reads/OS queries="<<queries<<"->"<<scoped_queries<<'\n';
    }
    t::totals={};
}
