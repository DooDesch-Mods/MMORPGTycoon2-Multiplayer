#include <array>
#include <vector>

namespace prefix_fixture {
enum class Probe {Normal,Failure,ChangedError,Overflow,NoAccess,Guard,Short,WrongAllocation,WrongEnd,WrongProtection};
struct Observation {
    bool enabled=false;
    LPCVOID probe_address=nullptr,request_failure=nullptr;
    Probe probe=Probe::Normal;
    DWORD type=0;
    std::vector<LPCVOID> addresses;
    std::vector<DWORD> incoming;
};
inline Observation observation;
}
// Only this test translation unit redirects the production OS call. Ordinary
// cases call the real OS; fault cases alter one selected response, not the cache.
SIZE_T observed_query(LPCVOID address,PMEMORY_BASIC_INFORMATION out,SIZE_T length){
    auto& o=prefix_fixture::observation;
    if(!o.enabled)return ::VirtualQuery(address,out,length);
    const auto incoming=GetLastError();
    o.addresses.push_back(address);o.incoming.push_back(incoming);
    SetLastError(incoming);
    if(address==o.request_failure){out->State=MEM_FREE;SetLastError(ERROR_ACCESS_DENIED);return 0;}
    if(address==o.probe_address&&o.probe==prefix_fixture::Probe::Failure){
        out->State=MEM_FREE;SetLastError(ERROR_INVALID_ADDRESS);return 0;
    }
    const auto result=::VirtualQuery(address,out,length);
    if(o.type&&result==sizeof *out)out->Type=o.type;
    if(address!=o.probe_address||result!=sizeof *out)return result;
    using prefix_fixture::Probe;
    switch(o.probe){
    case Probe::ChangedError:SetLastError(ERROR_INVALID_DATA);break;
    case Probe::Overflow:out->RegionSize=UINTPTR_MAX;break;
    case Probe::NoAccess:out->Protect=PAGE_NOACCESS;break;
    case Probe::Guard:out->Protect|=PAGE_GUARD;break;
    case Probe::Short:return sizeof *out-1;
    case Probe::WrongAllocation:out->AllocationBase=nullptr;break;
    case Probe::WrongEnd:out->RegionSize-=4096;break;
    case Probe::WrongProtection:out->Protect=PAGE_READONLY;break;
    default:break;
    }
    return result;
}
void prefix_cases(){
    using namespace prefix_fixture;
    struct Owned {
        std::array<void*,2> regions{};
        ~Owned(){for(auto p:regions)if(p&&!VirtualFree(p,0,MEM_RELEASE))
            std::cerr<<"prefix fixture allocation release failed "<<GetLastError()<<'\n';}
    } owned;
    SYSTEM_INFO system{};GetSystemInfo(&system);const size_t page=system.dwPageSize;
    for(auto& p:owned.regions){p=VirtualAlloc(nullptr,page*32,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        need(p!=nullptr,"prefix fixture owned allocation");}
    const auto at=[&](size_t row,size_t index){return static_cast<unsigned char*>(owned.regions[row])+index*page+17;};
    const auto begin=[&]{observation={};observation.enabled=true;};
    const auto exact=[&](size_t row,size_t index){
        MEMORY_BASIC_INFORMATION expected{},actual{};
        std::memset(&expected,0xa5,sizeof expected);std::memset(&actual,0xa5,sizeof actual);
        SetLastError(713);const auto result=::VirtualQuery(at(row,index),&expected,sizeof expected);
        const auto error=GetLastError();if(observation.type)expected.Type=observation.type;
        SetLastError(713);
        need(t::virtual_query(at(row,index),&actual,sizeof actual,t::Part::QueryPlacement)==result,
             "prefix wrapper retains requested-page return value");
        need(std::memcmp(&expected,&actual,sizeof actual)==0&&GetLastError()==error,
             "prefix wrapper retains every native metadata byte and LastError");
    };
    {
        t::Scope take(t::Part::Take);
        begin();const auto ascending=count(t::Part::MemoryVirtualQuery);
        {t::PageReadScope scope;for(size_t i=0;i<32;++i)exact(0,i);}
        need(observation.addresses==std::vector<LPCVOID>{at(0,0)}&&
             count(t::Part::MemoryVirtualQuery)==ascending+1,"ascending pages retain one requested-address OS query");

        begin();const auto descending=count(t::Part::MemoryVirtualQuery);
        {t::PageReadScope scope;for(size_t i=32;i>0;--i)exact(0,i-1);}
        need(observation.addresses==std::vector<LPCVOID>{at(0,31),owned.regions[0]}&&
             count(t::Part::MemoryVirtualQuery)==descending+2,"descending 32 pages use requested suffix then actual allocation-base query");
        need(observation.incoming==std::vector<DWORD>{713,713},"both successful OS queries receive original LastError");

        begin();
        {t::PageReadScope scope;exact(0,31);exact(1,31);exact(0,30);exact(1,30);exact(0,5);exact(1,5);}
        need(observation.addresses==std::vector<LPCVOID>{at(0,31),at(1,31),owned.regions[0],at(1,30),at(1,5)},
             "alternating allocations retain all suffixes and use only one widening attempt per lifetime");

        for(const auto mode:{Probe::Failure,Probe::ChangedError,Probe::Overflow,Probe::NoAccess,Probe::Guard,
                             Probe::Short,Probe::WrongAllocation,Probe::WrongEnd,Probe::WrongProtection}){
            begin();observation.probe_address=owned.regions[0];observation.probe=mode;
            const auto os=count(t::Part::MemoryVirtualQuery),miss=count(t::Part::PageScopeMiss);
            {t::PageReadScope scope;exact(0,31);exact(0,20);exact(0,10);exact(0,25);}
            need(observation.addresses==std::vector<LPCVOID>{at(0,31),owned.regions[0],at(0,20),at(0,10)},
                 "failed or unsuitable prefix falls back once and does not retry");
            need(observation.incoming==std::vector<DWORD>{713,713,713,713},
                 "prefix failure restores incoming LastError before original request");
            need(count(t::Part::MemoryVirtualQuery)==os+4&&count(t::Part::PageScopeMiss)==miss+4,
                 "prefix and fallback calls both remain in existing OS and miss counters");
        }

        begin();observation.probe_address=owned.regions[0];observation.probe=Probe::Failure;
        {t::PageReadScope scope;exact(0,31);observation.request_failure=at(0,20);
            MEMORY_BASIC_INFORMATION actual{},expected{};std::memset(&actual,0x5a,sizeof actual);expected=actual;expected.State=MEM_FREE;
            SetLastError(713);
            need(t::virtual_query(at(0,20),&actual,sizeof actual)==0&&GetLastError()==ERROR_ACCESS_DENIED&&
                 std::memcmp(&actual,&expected,sizeof actual)==0,"requested query failure retains its exact output and error after failed prefix");}

        // A real split protection prevents the allocation prefix from describing
        // the later writable suffix. The requested-page query remains decisive.
        DWORD old=0;
        need(VirtualProtect(owned.regions[0],page*8,PAGE_READONLY,&old)!=0,"create real readable split prefix");
        begin();
        {t::PageReadScope scope;exact(0,31);exact(0,20);exact(0,10);exact(0,4);}
        need(observation.addresses==std::vector<LPCVOID>{at(0,31),owned.regions[0],at(0,20),at(0,10),at(0,4)},
             "real split prefix never extends known suffix across a protection boundary");
        need(VirtualProtect(owned.regions[0],page*8,PAGE_NOACCESS,&old)!=0,"create real unreadable prefix");
        begin();
        {t::PageReadScope scope;exact(0,31);exact(0,20);exact(0,4);exact(0,4);}
        need(observation.addresses==std::vector<LPCVOID>{at(0,31),owned.regions[0],at(0,20),at(0,4),at(0,4)},
             "unreadable prefix is not cached and each unreadable requested page stays fresh");
        need(VirtualProtect(owned.regions[0],page*8,PAGE_READWRITE,&old)!=0,"restore owned prefix protection");

        for(const auto type:{DWORD(MEM_IMAGE),DWORD(MEM_MAPPED)}){
            begin();observation.type=type;
            {t::PageReadScope scope;exact(0,31);exact(0,20);exact(0,10);}
            need(observation.addresses==std::vector<LPCVOID>{at(0,31),at(0,20),at(0,10)},
                 "image and mapped opportunities retain requested-page queries without spending a prefix attempt");
        }

        for(const auto boundary:{t::PageBoundary::Reader,t::PageBoundary::OriginalCall}){
            begin();
            {t::PageReadScope scope;exact(0,31);exact(0,20);
                {t::PageReadScope nested(boundary);
                    need(VirtualProtect(static_cast<unsigned char*>(owned.regions[0])+page*16,page,PAGE_NOACCESS,&old)!=0,
                         "native or reader boundary changes owned page readability");}
                exact(0,16);exact(0,16);
                {t::PageReadScope nested(boundary);
                    need(VirtualProtect(static_cast<unsigned char*>(owned.regions[0])+page*16,page,PAGE_READWRITE,&old)!=0,
                         "native or reader boundary restores owned page");}
                exact(0,31);exact(0,20);}
            need(observation.addresses==std::vector<LPCVOID>{at(0,31),owned.regions[0],at(0,16),at(0,16),at(0,31),owned.regions[0]},
                 "both boundaries discard expanded metadata and reset the bounded attempt");
        }
        partition();observation.enabled=false;
    }
    const auto outside=count(t::Part::MemoryVirtualQuery),miss=count(t::Part::PageScopeMiss);
    begin();{t::PageReadScope scope;for(size_t i=32;i>0;--i)exact(0,i-1);}
    need(observation.addresses==std::vector<LPCVOID>{at(0,31),owned.regions[0]}&&
         count(t::Part::MemoryVirtualQuery)==outside&&count(t::Part::PageScopeMiss)==miss,
         "outside Take uses the same bounded algorithm without diagnostic events");
    observation.enabled=false;
    need(t::page_read_scope==nullptr,"prefix cases leave no reader scope active");
    t::totals={};
}
