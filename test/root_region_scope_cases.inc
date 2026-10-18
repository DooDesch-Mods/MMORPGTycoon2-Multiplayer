namespace root_region_scope {
struct Memory {
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
struct Origin {void* map=nullptr;};
struct WorldState {Origin origin;uintptr_t root=0;};
#include "root_region_helpers.inc"
bool original(Memory& memory_,const WorldState& world,uintptr_t image_,uintptr_t region,std::string& e){
#include "root_region_original.inc"
    return root_region(region);
}
bool scoped(Memory& memory_,const WorldState& world,uintptr_t image_,uintptr_t region,std::string& e){
#include "root_region_scoped.inc"
    return root_region(region);
}
enum class Fault {None,Failure,Exception,Owner,List,ListValues};
template<class T>void put(uintptr_t pointer,size_t offset,T value){std::memcpy(reinterpret_cast<void*>(pointer+offset),&value,sizeof value);}
struct Owned {
    unsigned char* allocation=nullptr;
    Owned(){allocation=static_cast<unsigned char*>(VirtualAlloc(nullptr,32768,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));need(allocation!=nullptr,"owned root-region fixture allocation");}
    ~Owned(){if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"error: root-region fixture VirtualFree failed Win32="<<GetLastError()<<'\n';}
};
struct CountingMemory final:Memory {
    host_copy::ProcessMemory actual;
    size_t copies=0,header_reads=0;
    Fault fault=Fault::None;
    uintptr_t map=0,first=0,slots=0;
    bool copy(uintptr_t address,void* out,size_t bytes,std::string& e)override{
        ++copies;
        if(fault==Fault::Failure&&copies==5){e="original root fixture read failure";return false;}
        if(fault==Fault::Exception&&copies==5)throw std::runtime_error("original root fixture exception");
        if(fault==Fault::Owner&&address==first+0x70)put(first,0x70,uintptr_t(0));
        if(address==map+0x358&&++header_reads==2&&fault==Fault::List)put(map,0x360,int32_t(7));
        if(address==map+0x358&&header_reads==3&&fault==Fault::ListValues){put(slots,0,first+128);put(slots,8,first);}
        return actual.copy(address,out,bytes,e);
    }
};
uint64_t queries(){return timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count;}
void run(bool baseline_only){
    Owned owned;const auto base=reinterpret_cast<uintptr_t>(owned.allocation);
    constexpr uintptr_t image=0x10000000;constexpr size_t count=8;
    const auto array=base+4096,first=base+8192;
    const WorldState world{{reinterpret_cast<void*>(base)},base+0x1c0};
    const auto reset=[&](){
        std::memset(owned.allocation,0,32768);
        put(base,0x358,array);put(base,0x360,int32_t(count));put(base,0x364,int32_t(count));
        for(size_t i=0;i<count;++i){const auto region=first+i*128;put(array,i*8,region);put(region,0,image+0x1787930);put(region,0x70,world.root);}
    };
    const auto memory=[&](Fault fault=Fault::None){CountingMemory value;value.map=base;value.first=first;value.slots=array;value.fault=fault;return value;};
    reset();std::string e;auto before=queries();auto baseline=memory();
    need(original(baseline,world,image,first,e),"actual baseline root census succeeds");
    const auto baseline_queries=queries()-before;
    need(baseline.copies==12+2*((count+63)/64)+2*count,"actual baseline retains exactly two full list/value reads");
    need(baseline_queries==baseline.copies,"baseline reproduces per-value OS query overhead");
    need(timing::page_read_scope==nullptr,"baseline retains no metadata");
    if(baseline_only){std::cout<<"Root-region baseline: "<<baseline.copies<<" fresh copies / "<<baseline_queries<<" OS queries\n";return;}
    before=queries();auto candidate=memory();SetLastError(123);e.clear();
    need(scoped(candidate,world,image,first,e),"actual scoped root census succeeds");
    const auto scoped_queries=queries()-before;
    need(candidate.copies==baseline.copies,"scoped census preserves every actual fresh copy");
    need(scoped_queries==1,"one homogeneous owned allocation needs one OS metadata query");
    need(GetLastError()==123&&timing::page_read_scope==nullptr,"root scope ends with LastError preserved");
    before=queries();for(int i=0;i<3;++i){auto fresh=memory();e.clear();need(scoped(fresh,world,image,first,e),"each next census succeeds with fresh metadata");}
    need(queries()-before==3,"root metadata does not survive between invocations");
    // The actual second list and per-region value guards still read changed bytes.
    for(const auto fault:{Fault::Owner,Fault::List,Fault::ListValues,Fault::Failure,Fault::Exception}){
        reset();auto failed=memory(fault);e.clear();bool result=false,caught=false;
        try{result=scoped(failed,world,image,first,e);}
        catch(const std::runtime_error& problem){caught=std::string(problem.what())=="original root fixture exception";}
        need(!result&&timing::page_read_scope==nullptr,"every root failure/exception closes the exact scope");
        if(fault==Fault::Owner)need(e=="remove_root_region_census_owner","changed shard owner is read fresh and rejected");
        if(fault==Fault::List)need(e=="remove_reader_array_changed_during_copy","changed list header is reread and rejected");
        if(fault==Fault::ListValues)need(e=="remove_root_regions_changed","second list values are read fresh and compared");
        if(fault==Fault::Failure)need(e=="original root fixture read failure","upstream copy failure retained");
        if(fault==Fault::Exception)need(caught,"upstream exception propagates unchanged");
    }
    reset();put(first,0,image+0x1787931);auto changed=memory();e.clear();
    need(!scoped(changed,world,image,first,e)&&e.find("remove_reader_vtable expected_rva=")==0,"changed class field is read fresh between invocations");
    reset();auto missing=memory();e.clear();
    need(!scoped(missing,world,image,first+128*count,e)&&e=="remove_region_absent_root_map","membership guard is unchanged");
    reset();auto safe=memory();e.clear();need(scoped(safe,world,image,first,e),"prime completed census before mapping change");
    DWORD old=0,unused=0;
    need(timing::page_read_scope==nullptr,"no root metadata at caller native callback boundary");
    need(VirtualProtect(reinterpret_cast<void*>(array),4096,PAGE_NOACCESS,&old)!=0,"owned callback changes list page protection");
    auto protected_memory=memory();e.clear();
    need(!scoped(protected_memory,world,image,first,e)&&e=="remove_memory_page_unreadable"&&timing::page_read_scope==nullptr,"next census rejects changed mapping with original copy cause");
    need(VirtualProtect(reinterpret_cast<void*>(array),4096,old,&unused)!=0,"restore only owned fixture page");
    reset();
    {timing::PageReadScope outer;host_copy::ProcessMemory reader;uintptr_t value=0;e.clear();
        need(reader.copy(array,&value,sizeof value,e),"prime existing parent metadata");
        auto nested=memory();need(scoped(nested,world,image,first,e)&&timing::page_read_scope==&outer,"root scope restores its parent after local completion");
        need(VirtualProtect(reinterpret_cast<void*>(array),4096,PAGE_NOACCESS,&old)!=0,"caller effect follows closed root scope");e.clear();
        need(!reader.copy(array,&value,sizeof value,e),"root exit cleared stale parent mapping metadata");
        need(VirtualProtect(reinterpret_cast<void*>(array),4096,old,&unused)!=0,"restore owned parent-boundary test page");
    }
    need(timing::page_read_scope==nullptr,"all root and parent scopes are closed");
    std::cout<<"Root-region OS queries: "<<baseline_queries<<" -> "<<scoped_queries<<"; fresh copies unchanged at "<<candidate.copies<<"\n";
}
}
