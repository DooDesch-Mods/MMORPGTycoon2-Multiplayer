#include "gm_ticket_reader.h"
namespace gm_census_scope_fixture {
namespace live=mt2mp::game::live_host_state;
namespace ticket=mt2mp::game::gm_ticket;
namespace a {struct Context {};struct Binding {};}
namespace building_mixed_identity::opened_file {struct CurrentWorld {void* map=nullptr;};}
struct Local {std::vector<uintptr_t> gm_rows;};
struct Runtime {
    uintptr_t image=0,queue=0,root=0,deny=0;Local local;
    size_t reads=0,finds=0;bool world_failure=false,find_failure=false,foreign_uid=false,throws=false,protect_arrow=false;
    uintptr_t protected_page=0;
    std::vector<std::pair<uintptr_t,size_t>> trace;
    template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    void reset(){
        reads=finds=0;deny=0;world_failure=find_failure=foreign_uid=throws=protect_arrow=false;trace.clear();
        if(protected_page){DWORD prior=0;need(VirtualProtect(reinterpret_cast<void*>(protected_page),4096,PAGE_READWRITE,&prior)!=0,"Restore owned GM fixture mapping");protected_page=0;}
    }
    void setup(){
        queue=image+0x20000;root=image+0x21000;local.gm_rows.clear();
        put(image+0x13df360,queue);put(queue,image+0x16fdc60);put(queue+0x18,image+0x16f0720);
        for(size_t i=0;i<50;++i){
            const auto master=image+0x30000+i*4096,arrow=image+0x100000+i*4096;
            local.gm_rows.push_back(master);put(master,image+0x16e5070);put(master+0x18,uint64_t(77+i));put(master+0x4ac,int32_t(i));
            put(master+0x170,root);put(master+0x100,std::array<float,4>{0,0,0,1});put(master+0x11c,std::array<float,3>{1,1,1});
            put(master+0x498,arrow);put(arrow,image+0x16fbbf0);
        }
    }
};
Runtime* current=nullptr;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool gm_world(Runtime& r,const a::Context&,const a::Binding&,building_mixed_identity::opened_file::CurrentWorld& world,std::string& error){
    if(r.world_failure)return fail(error,"original GM world cause");world.map=reinterpret_cast<void*>(r.root-0x1c0);return true;
}
template<class T>bool at(Runtime& r,uintptr_t object,size_t offset,T& out,std::string& error){
    ++r.reads;r.trace.emplace_back(object+offset,sizeof out);
    if(r.throws)throw std::runtime_error("original GM read exception");
    if(object+offset==r.deny)return fail(error,"original GM unreadable field");
    MEMORY_BASIC_INFORMATION info{};
    if(t::virtual_query(reinterpret_cast<LPCVOID>(object+offset),&info,sizeof info,t::Part::QueryPlacement)!=sizeof info||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))
        return fail(error,"original GM unreadable page");
    std::memcpy(&out,reinterpret_cast<const void*>(object+offset),sizeof out);return true;
}
uint32_t uid_lookup(uintptr_t image,uint64_t uid,uintptr_t& object){
    auto& r=*current;++r.finds;
    need(image==r.image&&!t::PageReadScope::reader_active(),"GM UID callback is outside the pure Reader boundary");
    if(r.find_failure)return 5;
    object=(uid>=77&&uid<127)?r.local.gm_rows[size_t(uid-77)]:0;
    if(r.foreign_uid)object=0;
    if(r.protect_arrow&&r.finds==1){
        DWORD prior=0;r.protected_page=r.image+0x100000;
        need(VirtualProtect(reinterpret_cast<void*>(r.protected_page),4096,PAGE_NOACCESS,&prior)!=0,"Change owned arrow mapping at actual UID boundary");
    }
    return 0;
}
#include "gm_census_scope_actual.inc"
void run(){
    prefix_fixture::observation={};
    struct Owned {
        void* value=nullptr;Runtime runtime;
        ~Owned(){runtime.reset();if(value&&!VirtualFree(value,0,MEM_RELEASE))std::cerr<<"GM fixture release failed "<<GetLastError()<<'\n';}
    } owned;
    owned.value=VirtualAlloc(nullptr,32*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    need(owned.value!=nullptr,"GM census fixture allocation");auto& r=owned.runtime;r.image=uintptr_t(owned.value);r.setup();current=&r;
    a::Context context;a::Binding binding;building_mixed_identity::opened_file::CurrentWorld world;ticket::Census prior,actual;std::string error;
    t::Scope take(t::Part::Take);
    const auto baseline_start=count(t::Part::QueryPlacement);
    need(prior_gm_census(r,context,binding,r.local,prior,world,error),"Original GM census fixture");
    const auto baseline_queries=count(t::Part::QueryPlacement)-baseline_start;
    const auto trace=r.trace;need(r.reads==1062&&r.finds==50&&baseline_queries==1062,"Original 50-slot GM census performs every expected field query and UID lookup");
    r.reset();const auto scoped_start=count(t::Part::QueryPlacement);
    need(gm_census(r,context,binding,r.local,actual,world,error)&&actual==prior&&r.trace==trace&&r.finds==50,"Scoped GM census retains complete values, read order and all native UID lookups");
    const auto scoped_queries=count(t::Part::QueryPlacement)-scoped_start;
    need(scoped_queries<=102&&scoped_queries>=51,"GM metadata is reused only between original UID boundaries");
    need(!t::PageReadScope::reader_active(),"GM successful census closes Reader extent");
    std::cout<<"GM50 census reads="<<r.reads<<" original UID calls="<<r.finds<<" OS queries="<<baseline_queries<<"->"<<scoped_queries<<'\n';
    r.reset();r.protect_arrow=true;
    need(!gm_census(r,context,binding,r.local,actual,world,error)&&error=="original GM unreadable page"&&r.finds==1,"GM callback page protection change invalidates pre-call metadata");
    need(!t::PageReadScope::reader_active(),"GM mapping failure closes Reader extent");
    r.reset();r.find_failure=true;
    need(!gm_census(r,context,binding,r.local,actual,world,error)&&error=="GM current FindUID SEH=5"&&r.finds==1,"GM original UID exception cause survives");
    r.reset();r.foreign_uid=true;
    need(!gm_census(r,context,binding,r.local,actual,world,error)&&error=="GM original FindUID does not resolve its current fixed slot","GM current UID identity check remains exact");
    r.reset();r.deny=r.local.gm_rows[0]+0x4c8;
    need(!gm_census(r,context,binding,r.local,actual,world,error)&&error=="original GM unreadable field"&&!t::PageReadScope::reader_active(),"GM field failure preserves cause and closes scope");
    r.reset();r.world_failure=true;
    need(!gm_census(r,context,binding,r.local,actual,world,error)&&error=="original GM world cause"&&r.reads==0,"GM world rejection precedes the Reader and any census reads");
    r.reset();r.throws=true;bool threw=false;try{gm_census(r,context,binding,r.local,actual,world,error);}catch(const std::runtime_error& e){threw=std::string(e.what())=="original GM read exception";}
    need(threw&&!t::PageReadScope::reader_active(),"GM read exception propagates and closes scope");
    r.reset();{t::PageReadScope parent;auto* owner=t::page_read_scope;
        need(gm_census(r,context,binding,r.local,actual,world,error)&&t::page_read_scope==owner,"GM census borrows caller Reader and restores it after every UID call");}
    need(!t::PageReadScope::reader_active(),"GM caller Reader ends at its original boundary");
    current=nullptr;
}
}
