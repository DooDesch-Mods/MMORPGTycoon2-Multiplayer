#include "region_saved_identity.h"
#include "character_definition_identity.h"
#include <functional>
namespace region_saved_identity=mt2mp::game::region_saved_identity;
namespace definitions=mt2mp::game::character_definition_identity;
namespace {
struct RegionFixture {
    void* allocation=VirtualAlloc(nullptr,0x80000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    uintptr_t base=uintptr_t(allocation),image=base-0x1740000,map=base+0x1000,root=map+0x1c0,data=base+0x2000,owned=base+0x2800;
    Memory memory;std::map<uintptr_t,std::string> owners;std::function<void(size_t)> visit;
    RegionFixture(){need(allocation!=nullptr,"owned region allocation failed");reset();}
    ~RegionFixture(){if(allocation&&!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"owned region release failed "<<GetLastError()<<'\n';}
    template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    uintptr_t region(size_t n)const{return base+0x4000+n*0x900;}
    void reset(){std::memset(allocation,0,0x80000);memory.reads.clear();owners.clear();visit={};
        put(map,image+0x1742860);put(root,image+0x174ad20);put(root+0x48,int32_t(0));
        put(map+0x230,image+0x16f04e0);put(map+0x238,data);put(map+0x240,int32_t(121));put(map+0x244,int32_t(121));
        put(map+0x358,owned);put(map+0x360,int32_t(1));put(map+0x364,int32_t(1));put(owned,region(0));
        for(size_t n=0;n<121;++n){put(data+n*8,region(n));put(region(n),image+0x1787930);put(region(n)+0x70,root);put(region(n)+0x834,int32_t(n));}
    }
    bool run(bool indexed,std::string& error){
        const auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.read(object+offset,&value,sizeof value,cause);};
        const auto owner_add=[&](auto& rows,uintptr_t object,const std::string& key,std::string& cause){
            if(!object||key.empty()){cause="Character original Record owner is empty";return false;}
            const auto [found,inserted]=rows.emplace(object,key);
            if(!inserted&&found->second!=key){cause="Character original Record owner aliases two canonical keys";return false;}
            if(visit)visit(rows.size());return true;
        };
        struct State {uintptr_t image;};const State state{image};
        struct Out {struct World {uintptr_t root;}world;};const Out out{{root}};
        if(indexed){
#include "character_region_owners.inc"
        }else{
            definitions::Array regions;if(!definitions::array(at,map+0x230,regions,error))return false;
            for(const auto region:regions.members){int32_t id=-1;uintptr_t selected=0;
                if(!at(region,0x834,id,error)||!region_saved_identity::resolve(state.image,map,out.world.root,id,region,selected,at,error)||
                   !owner_add(owners,region,"region:"+std::to_string(id),error))return false;}
            if(!definitions::unchanged(at,regions,error))return false;
        }
        return true;
    }
};
void region_owners_cases(){
    RegionFixture f;std::string error;t::Scope measured(t::Part::HostCapture);auto start=queries();
    need(f.run(false,error),"original per-region complete ownership scan failed");const auto before=queries()-start;
    const auto original_reads=f.memory.reads.size();const auto expected=f.owners;
    f.reset();start=queries();need(f.run(true,error)&&f.owners==expected,"indexed actual owner block changed canonical owner map");
    const auto after=queries()-start;const auto indexed_reads=f.memory.reads.size();
    need(original_reads==60990&&indexed_reads==1002&&after==1,"indexed owner block did not retain exactly two complete validations");
    const auto rejected=[&](const std::function<void()>& corrupt){f.reset();corrupt();need(!f.run(false,error),"baseline accepted invalid native ownership");
        f.reset();corrupt();need(!f.run(true,error)&&!error.empty(),"indexed block accepted invalid native ownership");};
    rejected([&]{f.put(f.region(120)+0x834,int32_t(0));});
    rejected([&]{f.put(f.data+120*8,f.region(0));});
    rejected([&]{f.put(f.region(120),uintptr_t(7));});
    rejected([&]{f.put(f.region(120)+0x70,f.root+8);});
    rejected([&]{f.put(f.region(120)+0x834,int32_t(-1));});
    rejected([&]{f.put(f.root+0x48,int32_t(1));});
    rejected([&]{f.put(f.map,uintptr_t(7));});
    rejected([&]{f.put(f.root,uintptr_t(7));});
    rejected([&]{f.put(f.map+0x230,uintptr_t(7));});
    rejected([&]{f.put(f.map+0x244,int32_t(120));});
    rejected([&]{f.put(f.owned,f.region(120)+8);});
    rejected([&]{f.put(f.map+0x360,int32_t(2));f.put(f.map+0x364,int32_t(2));f.put(f.owned+8,f.region(0));});
    rejected([&]{f.owners.emplace(f.region(0),"foreign-owner");});
    const auto drift=[&](const std::function<void()>& change){f.reset();f.visit=[&](size_t count){if(count==60)change();};
        need(!f.run(true,error)&&error=="saved region complete index changed during pure traversal","indexed block did not reject native drift before publication");};
    drift([&]{f.put(f.region(120)+0x834,int32_t(999));});
    drift([&]{f.put(f.owned,f.region(120));});
    drift([&]{f.put(f.map+0x244,int32_t(122));});
    drift([&]{f.put(f.data,f.region(120));f.put(f.data+120*8,f.region(0));});
    f.reset();need(f.run(true,error),"initial owner pass failed");
    {t::PageReadScope original(t::PageBoundary::OriginalCall);f.put(f.region(120)+0x834,int32_t(999));}
    f.owners.clear();need(f.run(true,error)&&f.owners.at(f.region(120))=="region:999","next native call reused prior semantic owner index");
    f.reset();DWORD previous=0;need(VirtualProtect(f.allocation,0x80000,PAGE_NOACCESS,&previous)!=0,"protect owned region fixture failed");
    const auto accepted=f.run(true,error);DWORD ignored=0;need(VirtualProtect(f.allocation,0x80000,previous,&ignored)!=0,"restore owned region fixture failed");
    need(!accepted&&error=="record fixture unreadable","indexed reader accepted unreadable current Region memory");
    need(!t::page_read_scope,"region owner block retained metadata past its pure traversal");
    std::cout<<"character Region owners: identical121 keys, native field reads "<<original_reads<<" -> "<<indexed_reads<<", OS queries "<<before<<" -> "<<after<<'\n';
}
}
