#include "region_saved_identity.h"

namespace region_scope_fixture {
namespace live=mt2mp::game::live_host_state;
namespace live_host_state=mt2mp::game::live_host_state;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
struct Fixture {
    void* memory=VirtualAlloc(nullptr,0x80000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uintptr_t image=0,map=0,root=0,data=0,owned=0,mutation_address=0,throw_address=0;
    unsigned mutation_hits=0;
    std::vector<definition_scope_fixture::ReadEvent> events;
    Fixture(){need(memory!=nullptr,"owned121-region allocation");const auto base=uintptr_t(memory);
        image=base-0x1740000;map=base+0x1000;root=map+0x1c0;data=base+0x2000;owned=base+0x2800;reset();}
    ~Fixture(){if(memory&&!VirtualFree(memory,0,MEM_RELEASE))std::cerr<<"region fixture release failed "<<GetLastError()<<'\n';}
    template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    uintptr_t region(size_t n)const{return uintptr_t(memory)+0x4000+n*0x900;}
    void reset(){std::memset(memory,0,0x80000);events.clear();mutation_address=throw_address=0;mutation_hits=0;
        put(map,image+0x1742860);put(root,image+0x174ad20);put(root+0x48,int32_t(0));
        put(map+0x230,image+0x16f04e0);put(map+0x238,data);put(map+0x240,int32_t(121));put(map+0x244,int32_t(121));
        put(map+0x358,owned);put(map+0x360,int32_t(1));put(map+0x364,int32_t(1));put(owned,region(0));
        for(size_t n=0;n<121;++n){put(data+n*8,region(n));put(region(n),image+0x1787930);put(region(n)+0x70,root);put(region(n)+0x834,int32_t(n));}}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& error){
        const auto address=object+offset;
        if(address==throw_address)throw std::runtime_error("original region read exception");
        if(address==mutation_address&&++mutation_hits==2)put(map+0x240,int32_t(120));
        if(!mt2mp::game::root_saved_building::read(reinterpret_cast<void*>(object),offset,value,error))return false;
        const auto* bytes=reinterpret_cast<const unsigned char*>(&value);events.push_back({address,{bytes,bytes+sizeof value}});return true;}
};
template<class Read,class Visit>bool boss_candidate(Read& at,uintptr_t image,uintptr_t map,uintptr_t root,Visit& add,std::string& error){
    const auto fail=[](std::string& cause,const std::string& value){cause=value;return false;};
#include "census_reader_extent.inc"
#include "boss_region_index.inc"
    return true;
}
template<class Read,class Visit>bool release_candidate(Read& read,uintptr_t image,uintptr_t map,uintptr_t root,Visit& add,std::string& e){
    struct Runtime {uintptr_t image;};const Runtime r{image};struct World {uintptr_t map;};const World world{map};
#include "census_reader_extent.inc"
#include "release_region_index.inc"
    return true;
}
}

void region_identity_scope_cases(){
    using namespace region_scope_fixture;Fixture fixture;std::string error;uintptr_t selected=0;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.read(object,offset,value,cause);};
    std::vector<uintptr_t> visited;std::vector<std::string> keys;
    std::function<bool(size_t)> visit;
    auto boss_add=[&](uintptr_t shard,uintptr_t region,const char* scene){
        need(shard==fixture.root&&std::string(scene)=="world","actual Boss loop keeps root scene ownership");
        visited.push_back(region);return !visit||visit(visited.size()-1);};
    auto release_add=[&](uintptr_t region,const char* family,const std::string& key){
        need(std::string(family)=="region","actual Release loop keeps canonical family");
        visited.push_back(region);keys.push_back(key);return !visit||visit(visited.size()-1);};
    auto boss=[&]{return boss_candidate(read,fixture.image,fixture.map,fixture.root,boss_add,error);};
    auto release=[&]{return release_candidate(read,fixture.image,fixture.map,fixture.root,release_add,error);};
    auto reset=[&]{fixture.reset();visited.clear();keys.clear();visit={};error.clear();};
    t::totals={};t::Scope capture(t::Part::HostCapture);
    auto start=count(t::Part::HostMemoryVirtualQuery);std::vector<uintptr_t> expected;
    for(int32_t id=0;id<121;++id){need(region_saved_identity::resolve(fixture.image,fixture.map,fixture.root,id,fixture.region(size_t(id)),selected,read,error),"original single-target validation remains available");expected.push_back(selected);}
    const auto original_reads=fixture.events.size();const auto original_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(original_reads==60621&&original_queries==60621,"historical outer-loop121 full scans retained as comparison");
    reset();start=count(t::Part::HostMemoryVirtualQuery);
    need(boss()&&visited==expected,"actual Boss index loop visits every original physical Region once in native order");
    need(fixture.events.size()==1002&&count(t::Part::HostMemoryVirtualQuery)-start==1&&!t::page_read_scope,"Boss index phase performs two complete501-read validations and one OS query");
    reset();start=count(t::Part::HostMemoryVirtualQuery);
    need(release()&&visited==expected,"actual Release index loop visits every original physical Region once");
    need(fixture.events.size()==1002&&count(t::Part::HostMemoryVirtualQuery)-start==1&&!t::page_read_scope,"Release index phase performs two complete validations and one OS query");
    for(size_t n=0;n<keys.size();++n)need(keys[n]==std::to_string(n),"Release canonical Region ID remains exact");

    const auto rejected=[&](const std::function<void()>& corrupt){
        reset();corrupt();
        need(!region_saved_identity::resolve(fixture.image,fixture.map,fixture.root,0,fixture.region(0),selected,read,error),"historical resolver rejects corrupt complete Region ownership");
        const auto cause=error;const auto trace=fixture.events;
        reset();corrupt();need(!boss()&&error==cause&&fixture.events==trace&&visited.empty()&&!t::page_read_scope,"Boss rejects before traversal with same complete guard and original cause");
        reset();corrupt();need(!release()&&error==cause&&fixture.events==trace&&visited.empty()&&!t::page_read_scope,"Release rejects before publication with same complete guard and original cause");
    };
    rejected([&]{fixture.put(fixture.region(120)+0x834,int32_t(0));});
    rejected([&]{fixture.put(fixture.data+120*8,fixture.region(0));});
    rejected([&]{fixture.put(fixture.region(120),uintptr_t(7));});
    rejected([&]{fixture.put(fixture.region(120)+0x70,fixture.root+8);});
    rejected([&]{fixture.put(fixture.region(120)+0x834,int32_t(-1));});
    rejected([&]{fixture.put(fixture.root+0x48,int32_t(1));});
    rejected([&]{fixture.put(fixture.map+0x230,uintptr_t(7));});
    rejected([&]{fixture.put(fixture.map+0x240,int32_t(4097));});
    rejected([&]{fixture.put(fixture.owned,fixture.region(120)+8);});
    rejected([&]{fixture.put(fixture.map+0x360,int32_t(2));fixture.put(fixture.map+0x364,int32_t(2));fixture.put(fixture.owned+8,fixture.region(0));});
    rejected([&]{fixture.mutation_address=fixture.map+0x240;});

    const auto drift=[&](const std::function<void()>& mutate){
        for(const bool use_boss:{true,false}){
            reset();visit=[&](size_t n){if(n==60)mutate();return true;};
            need(!(use_boss?boss():release())&&error=="saved region complete index changed during pure traversal"&&!t::page_read_scope,
                 "valid but changed ordered index is rejected by final complete revalidation");
        }
    };
    drift([&]{fixture.put(fixture.region(120)+0x834,int32_t(999));});
    drift([&]{fixture.put(fixture.owned,fixture.region(120));});
    drift([&]{fixture.put(fixture.map+0x244,int32_t(122));});
    drift([&]{fixture.put(fixture.data,fixture.region(120));fixture.put(fixture.data+120*8,fixture.region(0));});

    reset();visit=[&](size_t n){if(n!=60)return true;error="original native callback failure";return false;};
    need(!boss()&&error=="original native callback failure"&&visited.size()==61&&fixture.events.size()==501&&!t::page_read_scope,
         "failed original traversal stops once without final validation or later native calls");
    reset();visit=[](size_t)->bool{throw std::runtime_error("original traversal exception");};bool caught=false;
    try{release();}catch(const std::runtime_error& exception){caught=std::string(exception.what())=="original traversal exception";}
    need(caught&&visited.size()==1&&!t::page_read_scope,"original traversal exception unwinds complete metadata extent");

    reset();DWORD previous=0;bool protected_page=false;
    visit=[&](size_t n){if(n==60){t::PageReadScope original(t::PageBoundary::OriginalCall);
        protected_page=VirtualProtect(fixture.memory,0x80000,PAGE_NOACCESS,&previous)!=0;need(protected_page,"protect only owned fixture in original callback");}return true;};
    const bool readable=boss();DWORD replaced=0;
    need(protected_page&&VirtualProtect(fixture.memory,0x80000,previous,&replaced)!=0,"restore only owned fixture protection");
    need(!readable&&error.find("unreadable")!=std::string::npos&&!t::page_read_scope,"final validation observes native callback protection change");

    reset();
    {t::PageReadScope parent;
        need(boss(),"bounded census can borrow a pure parent extent");
        fixture.put(fixture.region(120)+0x834,int32_t(999));visited.clear();
        need(release()&&keys.back()=="999","next complete census reads changed identity values without retaining the previous index");
        {t::PageReadScope original(t::PageBoundary::OriginalCall);
            need(!t::PageReadScope::reader_active(),"original-call body is not a borrowable reader");
            MEMORY_BASIC_INFORMATION metadata{};start=count(t::Part::HostMemoryVirtualQuery);
            need(t::virtual_query(fixture.memory,&metadata,sizeof metadata)==sizeof metadata&&t::virtual_query(fixture.memory,&metadata,sizeof metadata)==sizeof metadata,
                 "mixed original body reads actual page metadata");
            need(count(t::Part::HostMemoryVirtualQuery)-start==2,"mixed original body has no implicit page cache");
            visited.clear();keys.clear();start=count(t::Part::HostMemoryVirtualQuery);need(boss()&&release(),"reader reentry inside original body owns separate extents");
            need(count(t::Part::HostMemoryVirtualQuery)-start==2,"separate original-body reader calls do not share metadata");
        }
        visited.clear();keys.clear();start=count(t::Part::HostMemoryVirtualQuery);need(boss()&&count(t::Part::HostMemoryVirtualQuery)==start+1,"pure parent queries current metadata after original return");
    }
    need(!t::page_read_scope,"no Region index metadata survives its bounded extent");
    std::cout<<"Region121 index phase value reads="<<original_reads<<"->1002 OS queries="<<original_queries<<"->1 per Boss/Release phase\n";
    t::totals={};
}
