#include "character_definition_identity.h"
#include "root_saved_building.h"

namespace definition_scope_fixture {
namespace character_definition_identity=mt2mp::game::character_definition_identity;
namespace live_host_state=mt2mp::game::live_host_state;
struct ReadEvent {uintptr_t address=0;std::vector<unsigned char> bytes;bool operator==(const ReadEvent&)const=default;};
struct Owned {
    void* memory=VirtualAlloc(nullptr,65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    ~Owned(){if(memory&&!VirtualFree(memory,0,MEM_RELEASE))std::cerr<<"definition fixture release failed "<<GetLastError()<<'\n';}
};
struct Fixture {
    Owned owned;uintptr_t image=0,state=0,manager=0,data=0,first=0,second=0;
    std::vector<ReadEvent> events;bool change_uid=false,throw_read=false;unsigned uid_reads=0;
    Fixture(){need(owned.memory!=nullptr,"owned pure-definition allocation");const auto base=uintptr_t(owned.memory);
        image=base-0x13df700;state=base+0x1000;manager=base+0x2000;data=base+0x3000;first=base+0x4000;second=base+0x5000;reset();}
    template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    void reset(){std::memset(owned.memory,0,65536);events.clear();change_uid=false;throw_read=false;uid_reads=0;
        put(image+0x13df7a0,manager);put(state+0x168,manager);put(manager,image+0x1732430);
        for(size_t kind=0;kind<5;++kind){const auto array=manager+0x80+kind*0x50;
            put(array+8,kind==1?data:uintptr_t(0));put(array+0x10,int32_t(kind==1?2:0));put(array+0x14,int32_t(kind==1?2:0));}
        put(data,first);put(data+8,second);
        for(size_t n=0;n<2;++n){const auto object=n?second:first;put(object,image+0x1705990);put(object+0x18,uint64_t(0x100000001+n));
            put(object+0x2c,int32_t(1));put(object+0x30,int32_t(n));}
    }
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& error){
        if(throw_read)throw std::runtime_error("original definition read exception");
        if(object+offset==first+0x18&&++uid_reads==2&&change_uid)put(first+0x18,uint64_t(0x900000001));
        if(!mt2mp::game::root_saved_building::read(reinterpret_cast<void*>(object),offset,value,error))return false;
        const auto* bytes=reinterpret_cast<const unsigned char*>(&value);events.push_back({object+offset,{bytes,bytes+sizeof value}});return true;
    }
};
template<class Read>bool candidate(Read& at,uintptr_t image,uintptr_t state,uintptr_t manager,character_definition_identity::Census& current,std::string& error){
    struct Owner {uintptr_t image;};const Owner owner{image};
    struct Before {struct Origin {uintptr_t state;}origin;};const Before before{{state}};
#include "character_definition_pure_call.inc"
}
}

void character_definition_scope_cases(){
    using namespace definition_scope_fixture;Fixture fixture;std::string error;
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return fixture.read(object,offset,value,cause);};
    character_definition_identity::Census baseline,scoped;t::totals={};t::Scope capture(t::Part::HostCapture);
    const auto unscoped=count(t::Part::HostMemoryVirtualQuery);
    need(character_definition_identity::read(at,fixture.image,fixture.state,fixture.manager,baseline,error),"unscoped actual definition reader");
    const auto trace=fixture.events;const auto baseline_queries=count(t::Part::HostMemoryVirtualQuery)-unscoped;
    need(trace.size()==77&&baseline_queries==77,"baseline complete reader performs77 value reads and fresh OS queries");
    fixture.reset();const auto start=count(t::Part::HostMemoryVirtualQuery);
    need(candidate(at,fixture.image,fixture.state,fixture.manager,scoped,error),"extracted production pure-definition extent");
    const auto scoped_queries=count(t::Part::HostMemoryVirtualQuery)-start;
    need(fixture.events==trace&&scoped.definitions.size()==baseline.definitions.size(),"all77 actual value reads preserve order and bytes");
    need(scoped_queries==1&&!t::page_read_scope,"only page metadata is shared and scope ends at call return");
    need(scoped.definitions[0].uid==baseline.definitions[0].uid&&scoped.definitions[1].key==baseline.definitions[1].key,"complete full-UID/global identity output retained");
    fixture.put(fixture.first+0x18,uint64_t(0x700000001));fixture.events.clear();
    need(candidate(at,fixture.image,fixture.state,fixture.manager,scoped,error)&&scoped.definitions[0].uid==0x700000001,"next reader sees fresh current bytes after previous return");
    fixture.reset();fixture.change_uid=true;error.clear();
    need(!character_definition_identity::read(at,fixture.image,fixture.state,fixture.manager,baseline,error),"unscoped final current UID guard rejects change");
    const auto rejected_trace=fixture.events;const auto rejected_error=error;
    fixture.reset();fixture.change_uid=true;error.clear();
    need(!candidate(at,fixture.image,fixture.state,fixture.manager,scoped,error)&&error==rejected_error&&fixture.events==rejected_trace,"scoped complete rereads detect same UID change with same read count and cause");
    fixture.reset();fixture.throw_read=true;bool caught=false;
    try{candidate(at,fixture.image,fixture.state,fixture.manager,scoped,error);}
    catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original definition read exception";}
    need(caught&&!t::page_read_scope,"original read exception remains visible and closes metadata scope");fixture.throw_read=false;
    {
        t::PageReadScope parent;MEMORY_BASIC_INFORMATION info{};
        need(t::virtual_query(fixture.owned.memory,&info,sizeof info)==sizeof info,"warm only owned parent metadata");
        need(candidate(at,fixture.image,fixture.state,fixture.manager,scoped,error),"nested pure definition reader");
        DWORD previous=0;
        {t::PageReadScope original(t::PageBoundary::OriginalCall);
            need(VirtualProtect(fixture.owned.memory,65536,PAGE_NOACCESS,&previous)!=0,"protect only owned fixture inside original callback boundary");}
        uint64_t value=0;const auto readable=mt2mp::game::root_saved_building::read(reinterpret_cast<void*>(fixture.first),0x18,value,error);
        DWORD replaced=0;need(VirtualProtect(fixture.owned.memory,65536,previous,&replaced)!=0,"restore only owned fixture protection");
        need(!readable&&error.find("unreadable")!=std::string::npos,"original callback invalidates shared metadata before the next value read");
    }
    fixture.events.clear();need(candidate(at,fixture.image,fixture.state,fixture.manager,scoped,error)&&fixture.events.size()==77,"fresh scope recovers after protection restore");
    std::cout<<"pure definition reader value reads="<<trace.size()<<" baseline OS queries="<<baseline_queries<<" scoped OS queries="<<scoped_queries<<'\n';
    t::totals={};
}
