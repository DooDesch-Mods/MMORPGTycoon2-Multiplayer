#include <limits>
namespace world_prop_array_fixture {
struct Memory {
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
struct ProcessMemory final:Memory {bool copy(uintptr_t,void*,size_t,std::string&)override;};
#include "world_prop_array_bodies.inc"

struct ScriptedMemory final:Memory {
    static constexpr uintptr_t address=0x20000,header=0x10000;
    std::vector<uintptr_t> values;
    size_t calls=0,largest=0,header_reads=0;
    uintptr_t unreadable=0;
    int changed_header=0;
    bool empty_cause=false,throws=false,bulk_failure=false;
    ScriptedMemory(){for(uintptr_t i=0;i<130;++i)values.push_back(0x40000+i*16);}
    bool copy(uintptr_t p,void* out,size_t bytes,std::string& e)override{
        ++calls;largest=std::max(largest,bytes);
        if(throws)throw std::runtime_error("original array read exception");
        if(p==header||p==header+8||p==header+12){
            ++header_reads;const bool second=header_reads>3;
            if(p==header){const uintptr_t value=address+(second&&changed_header==1?8:0);std::memcpy(out,&value,bytes);}
            else {const int32_t value=int32_t(values.size())+(second&&changed_header==(p==header+8?2:3)?1:0);std::memcpy(out,&value,bytes);}
            return true;
        }
        if((bulk_failure&&bytes>sizeof(uintptr_t))||p<address||p-address>values.size()*sizeof(uintptr_t)||
           bytes>values.size()*sizeof(uintptr_t)-(p-address)||
           (unreadable&&p<=unreadable&&bytes>unreadable-p)){
            if(!empty_cause)e=bytes>sizeof(uintptr_t)?"speculative bulk read failure":"original slot read failure";
            return false;
        }
        std::memcpy(out,reinterpret_cast<const unsigned char*>(values.data())+(p-address),bytes);return true;
    }
};
void equivalent(const ScriptedMemory& seed,uintptr_t data,int32_t count_value,int32_t capacity,const char* reason,
                const std::string& initial_error={}){
    auto old_memory=seed,new_memory=seed;
    std::vector<uintptr_t> old_out{99},new_out{99};std::string old_error=initial_error,new_error=initial_error;
    const bool old_ok=original_array(old_memory,data,count_value,capacity,old_out,old_error);
    const bool new_ok=array(new_memory,data,count_value,capacity,new_out,new_error);
    need(old_ok==new_ok&&old_out==new_out&&old_error==new_error,reason);
    need(new_memory.largest<=64*sizeof(uintptr_t),"corrupt native counts never enlarge the temporary read");
}
struct Owned {
    unsigned char* bytes=nullptr;
    size_t page=0;
    Owned(){SYSTEM_INFO info{};GetSystemInfo(&info);page=info.dwPageSize;
        bytes=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*3,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        need(bytes!=nullptr,"array fixture owns three pages");}
    ~Owned(){if(bytes&&!VirtualFree(bytes,0,MEM_RELEASE))std::cerr<<"error: array fixture release failed Win32="<<GetLastError()<<'\n';}
};
struct CountingMemory final:Memory {
    ProcessMemory actual;
    size_t calls=0,largest=0;
    bool copy(uintptr_t p,void* out,size_t bytes,std::string& e)override{
        ++calls;largest=std::max(largest,bytes);return actual.copy(p,out,bytes,e);
    }
};
}
void world_prop_array_cases(){
    namespace f=world_prop_array_fixture;
    f::ScriptedMemory seed;
    for(const int32_t n:{0,1,63,64,65,130})
        f::equivalent(seed,n?f::ScriptedMemory::address:0,n,n,"actual array retains complete ordered slots at chunk boundaries");
    for(const auto pair:{std::pair{-1,0},std::pair{3,2},std::pair{0,-1}})
        f::equivalent(seed,f::ScriptedMemory::address,pair.first,pair.second,"actual count/capacity rejection unchanged");
    f::equivalent(seed,0,1,1,"nonempty null array rejected before reading");
    f::equivalent(seed,UINTPTR_MAX-7,1,1,"end-address overflow rejected before reading");
    f::equivalent(seed,UINTPTR_MAX-8,1,1,"last nonoverflowing address keeps original copy failure");
    f::equivalent(seed,UINTPTR_MAX-15,2,2,"full-array overflow rejected before chunking");
    {
        auto unreadable=seed;unreadable.values.clear();
        f::equivalent(unreadable,f::ScriptedMemory::address,(std::numeric_limits<int32_t>::max)(),
                      (std::numeric_limits<int32_t>::max)(),"corrupt huge count fails without allocating native count storage");
    }
    for(const size_t slot:{size_t(0),size_t(63),size_t(64),size_t(129)}){
        auto null_value=seed;null_value.values[slot]=0;
        f::equivalent(null_value,f::ScriptedMemory::address,130,130,"null slot preserves exact original partial output");
    }
    for(const size_t slot:{size_t(1),size_t(64),size_t(129)}){
        auto duplicate=seed;duplicate.values[slot]=duplicate.values[0];
        f::equivalent(duplicate,f::ScriptedMemory::address,130,130,"duplicate across chunks preserves original rejection and prefix");
    }
    for(const size_t slot:{size_t(0),size_t(20),size_t(64),size_t(129)}){
        auto unreadable=seed;unreadable.unreadable=f::ScriptedMemory::address+slot*sizeof(uintptr_t);
        f::equivalent(unreadable,f::ScriptedMemory::address,130,130,"failed chunk retains original single-slot cause and prefix");
        unreadable.empty_cause=true;
        f::equivalent(unreadable,f::ScriptedMemory::address,130,130,"empty upstream failure cause remains empty");
    }
    {
        auto precedence=seed;precedence.values[3]=0;precedence.unreadable=f::ScriptedMemory::address+20*sizeof(uintptr_t);
        f::equivalent(precedence,f::ScriptedMemory::address,130,130,"later unreadable slot cannot replace earlier null cause");
        f::equivalent(precedence,f::ScriptedMemory::address,130,130,"speculative read cannot replace retained upstream cause","retained prior cause");
        precedence.values[3]=precedence.values[0];
        f::equivalent(precedence,f::ScriptedMemory::address,130,130,"later unreadable slot cannot replace earlier duplicate cause");
    }
    {
        auto changed=seed;std::vector<uintptr_t> out;std::string e;
        need(f::list(changed,f::ScriptedMemory::header,0,out,e)&&out==seed.values&&changed.header_reads==6,
             "actual list rereads every header field after all slots");
        need(changed.calls==9,"130 slots require three copies plus six unchanged header reads");
        for(int field=1;field<=3;++field){changed=seed;changed.changed_header=field;e.clear();
            need(!f::list(changed,f::ScriptedMemory::header,0,out,e)&&e=="remove_reader_array_changed_during_copy",
                 "fresh list data/count/capacity mutation remains rejected");}
        changed=seed;changed.values[64]+=1;e.clear();
        need(f::list(changed,f::ScriptedMemory::header,0,out,e)&&out[64]==changed.values[64],
             "new census reads changed slot values instead of retaining a cache");
    }
    {
        auto throwing=seed;throwing.throws=true;bool caught=false;std::vector<uintptr_t> out;std::string e;
        try{f::array(throwing,f::ScriptedMemory::address,130,130,out,e);}
        catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original array read exception";}
        need(caught,"actual array propagates copy exception without replay");
        auto failed=seed;failed.bulk_failure=true;e.clear();
        need(!f::array(failed,f::ScriptedMemory::address,130,130,out,e)&&e=="speculative bulk read failure",
             "a failed chunk cannot become a successful census if fallback reads recover");
    }
    {
        f::Owned owned;const auto start=reinterpret_cast<uintptr_t>(owned.bytes+owned.page-2*sizeof(uintptr_t));
        std::memcpy(reinterpret_cast<void*>(start),seed.values.data(),seed.values.size()*sizeof(uintptr_t));
        DWORD old=0,unused=0;
        need(VirtualProtect(owned.bytes+owned.page,owned.page,PAGE_READONLY,&old)!=0,"owned second page becomes a separate readable region");
        f::CountingMemory old_memory,new_memory;std::vector<uintptr_t> old_out,new_out;std::string old_error,new_error;
        need(f::original_array(old_memory,start,130,130,old_out,old_error)&&f::array(new_memory,start,130,130,new_out,new_error)&&
             old_out==new_out&&new_out==seed.values,"actual ProcessMemory reads a chunk crossing readable protection regions");
        need(old_memory.calls==130&&new_memory.calls==3&&new_memory.largest==512,
             "actual successful array reduces 130 adapter calls to three bounded copies");
        need(VirtualProtect(owned.bytes+owned.page,owned.page,PAGE_NOACCESS,&unused)!=0,"owned second page becomes unreadable");
        old_error.clear();new_error.clear();
        need(!f::original_array(old_memory,start,130,130,old_out,old_error)&&!f::array(new_memory,start,130,130,new_out,new_error)&&
             old_out==new_out&&new_out.size()==2&&old_error==new_error&&new_error=="remove_memory_page_unreadable",
             "actual page-crossing failure retains original two-slot prefix and adapter cause");
        const uintptr_t null_slot=0;std::memcpy(reinterpret_cast<void*>(start),&null_slot,sizeof null_slot);
        old_error.clear();new_error.clear();
        need(!f::original_array(old_memory,start,130,130,old_out,old_error)&&!f::array(new_memory,start,130,130,new_out,new_error)&&
             old_out==new_out&&new_out.empty()&&old_error==new_error&&new_error=="remove_reader_array_null_or_duplicate",
             "actual later inaccessible page cannot hide earlier null validation");
        need(VirtualProtect(owned.bytes+owned.page,owned.page,old,&unused)!=0,"restore only owned page protection");
    }
}
