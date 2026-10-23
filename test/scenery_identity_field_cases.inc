namespace scenery_identity_field_fixture {
struct Adapter {
    Memory memory;
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){return memory.read(address,out,size,error);}
};
namespace host {
bool resolve_lookup(Adapter& memory,uintptr_t image,const std::string&,int& out,std::string& e){
    uint64_t first=0,second=0;
    if(!memory.copy(image,&first,sizeof first,e)||!memory.copy(image+8,&second,sizeof second,e))return false;
    out=int(first+second);return true;
}
}
bool owners(Adapter& memory,uintptr_t object,uintptr_t& shard,uintptr_t& region,bool scoped,std::string& e){
    if(scoped){
#include "scenery_owner_fields.inc"
    }else{
#include "scenery_owner_fields_baseline.inc"
    }
    return true;
}
bool resolver(Adapter& memory,uintptr_t image,int& api,bool scoped,std::string& e){
    const std::string hash="pinned";
    if(scoped){
#include "scenery_lookup_fields.inc"
    }else{
#include "scenery_lookup_fields_baseline.inc"
    }
    return true;
}
void run(){
    Fixture allocation;Adapter reader;std::string error;uintptr_t shard=0,region=0;int api=0;
    allocation.put(allocation.base+0x170,uintptr_t(17));allocation.put(allocation.base+0x178,uintptr_t(18));
    allocation.put(allocation.base,uint64_t(11));allocation.put(allocation.base+8,uint64_t(12));
    t::Scope measured(t::Part::HostCapture);
    const auto test=[&](auto read){
        reader.memory.reads.clear();auto before=queries();need(read(false),"baseline identity field read failed");
        const auto baseline=reader.memory.reads;need(queries()-before==2,"baseline identity fields did not make both OS queries");
        reader.memory.reads.clear();before=queries();
        {t::PageReadScope original(t::PageBoundary::OriginalCall);auto* boundary=t::page_read_scope;
            need(read(true),"scoped identity fields failed");
            need(t::page_read_scope==boundary&&!t::PageReadScope::reader_active(),"identity fields retained scope at next native call");}
        need(queries()-before==1&&reader.memory.reads==baseline,"scoped identity fields changed read order/bytes or retained duplicate OS query");
        DWORD previous=0;need(VirtualProtect(allocation.allocation,65536,PAGE_NOACCESS,&previous)!=0,"protect identity fixture failed");
        const bool baseline_ok=read(false);const auto cause=error;const bool scoped_ok=read(true);const auto scoped_cause=error;DWORD ignored=0;
        need(VirtualProtect(allocation.allocation,65536,previous,&ignored)!=0,"restore identity fixture failed");
        need(!baseline_ok&&!scoped_ok&&cause==scoped_cause&&!t::page_read_scope,"identity field scope changed unreadable-memory rejection");
    };
    test([&](bool scoped){return owners(reader,allocation.base,shard,region,scoped,error);});
    test([&](bool scoped){return resolver(reader,allocation.base,api,scoped,error);});
    allocation.put(allocation.base+0x170,uintptr_t(31));allocation.put(allocation.base,uint64_t(25));
    need(owners(reader,allocation.base,shard,region,true,error)&&shard==31&&region==18,"next identity read reused stale owner fields");
    need(resolver(reader,allocation.base,api,true,error)&&api==37,"next identity read reused stale resolver bytes");
    std::cout<<"Scenery identity pure fields: same bytes/rejections; owner and resolver OS queries 2 -> 1; native boundaries retained\n";
}
}
