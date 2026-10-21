#include "npc_saved_identity.h"

namespace npc_saved_metadata_fixture {
namespace q=mt2mp::game::npc_saved_identity;
namespace opened=mt2mp::game::building_mixed_identity::opened_file;
namespace live_host_state=mt2mp::game::live_host_state;
using Current=q::Current;
uintptr_t image=0;
uint32_t main_thread=0;
bool host_active=false,client_active=false;
std::mutex gate;
std::string uuid,sha,snapshot;
uint64_t common_epoch=0;
opened::CurrentWorld origin;
std::map<int32_t,q::SavedPool> pools;
int scenario=0;
size_t fail_read=0;
struct Read {uintptr_t address=0;size_t size=0;std::vector<unsigned char> bytes;bool operator==(const Read&)const=default;};
std::vector<Read> copies;
std::vector<std::string> events;
std::vector<uint64_t> lookup_ids;
struct Memory {
    bool read(uintptr_t address,void* out,size_t size,std::string& error){
        copies.push_back({address,size,{}});
        if(fail_read&&copies.size()==fail_read)return q::fail(error,"fixture field copy failed");
        MEMORY_BASIC_INFORMATION page{};
        if(t::virtual_query(reinterpret_cast<void*>(address),&page,sizeof page,t::Part::QueryWorldProp)!=sizeof page||
           page.State!=MEM_COMMIT||(page.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return q::fail(error,"fixture field unreadable");
        const auto begin=reinterpret_cast<uintptr_t>(page.BaseAddress);
        if(address<begin||size>page.RegionSize||address-begin>page.RegionSize-size)return q::fail(error,"fixture field extent differs");
        std::memcpy(out,reinterpret_cast<void*>(address),size);
        copies.back().bytes.assign(static_cast<unsigned char*>(out),static_cast<unsigned char*>(out)+size);
        return true;
    }
} memory;
template<class T>void put(size_t offset,T value){std::memcpy(reinterpret_cast<void*>(image+offset),&value,sizeof value);}
bool healthy(std::string& error){return scenario!=1||q::fail(error,"fixture retained observer failure");}
namespace net {
std::string current_snapshot_id(){return snapshot;}
uint64_t current_world_epoch(){return common_epoch;}
}
namespace native_open_runtime {
bool read_current_world(opened::CurrentWorld& out,std::string& error){
    events.push_back("world");out=origin;
    return memory.read(image,&out.world_epoch,sizeof out.world_epoch,error);
}
}
void* find_uid(uint64_t uid){
    lookup_ids.push_back(uid);events.push_back("find:"+std::to_string(uid));
    if(scenario==15)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    uintptr_t found=0;std::string error;
    if(!memory.read(image+0x100+size_t(uid-100)*8,&found,sizeof found,error))return nullptr;
    if(scenario==10&&lookup_ids.size()==2)put(0,uint64_t(42));
    if(scenario==16)found=0;
    return reinterpret_cast<void*>(found);
}
namespace build_actions::private_native::host {
using FindUid=void*(*)(uint64_t);
struct ProcessMemory {};
struct LookupApi {FindUid find=nullptr;};
bool resolve_lookup(ProcessMemory&,uintptr_t actual,const char* digest,LookupApi& api,std::string& error){
    events.push_back("resolve");
    need(actual==image&&std::string(digest)=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",
         "saved reader changed native resolver inputs");
    if(scenario==11)return q::fail(error,"fixture resolver failed");
    uint32_t entry=0;if(!memory.read(image+0x20,&entry,sizeof entry,error))return false;
    api.find=find_uid;return entry==1||q::fail(error,"fixture native entry changed");
}
}
namespace region_saved_identity {
template<class Read>bool resolve(uintptr_t actual,uintptr_t map,uintptr_t root,int32_t id,uintptr_t expected,
                                uintptr_t& region,Read read,std::string& error){
    events.push_back("region");
    need(actual==image&&map==image&&root==map+0x1c0&&id==7&&expected==0,"saved reader changed Region resolver inputs");
    if(scenario==12)return q::fail(error,"fixture Region read failed");
    return read(map,0x1c0,region,error);
}
}
namespace npc_census {
using State=mt2mp::npc_census::State;
using Row=mt2mp::npc_census::Row;
bool read(Memory& source,uintptr_t actual,uintptr_t region,State& out,std::string& error){
    events.push_back("census");need(actual==image&&region==image+0x200,"saved reader changed census inputs");
    if(scenario==13)return q::fail(error,"fixture census failed");
    if(!source.read(region,&out.manager,sizeof out.manager,error))return false;
    for(size_t i=0;i<2;++i){Row row;
        if(!source.read(image+0x300+i*0x20,&row.local_pointer,sizeof row.local_pointer,error)||
           !source.read(image+0x308+i*0x20,&row.local_uid,sizeof row.local_uid,error)||
           !source.read(image+0x310+i*0x20,&row.type,sizeof row.type,error)||
           !source.read(image+0x314+i*0x20,&row.level,sizeof row.level,error))return false;
        out.rows.push_back(row);
    }
    return true;
}
}
// This fixture owns metadata lifetime only. Child ownership and original inverse
// data use test_building_npc_owner; this adapter has no child registry.
namespace building_child_detail {
bool has_region(int32_t){return false;}
template<class Find>bool append_scenes(const opened::CurrentWorld&,Find&,std::vector<Current>& rows,std::string&){
    need(rows.size()==2,"metadata-only fixture expects its complete two root NPCs and no child registry");return true;}
template<class Find>bool append(int32_t,uintptr_t,const npc_census::State&,Find&,std::vector<Current>&,std::string&){return true;}
}
#include "npc_saved_metadata_bodies.inc"
namespace npc_saved_identity_runtime {
bool read_current(std::vector<Current>& out,std::string& error,bool share_metadata){
    need(share_metadata,"actual owner edge omitted explicit metadata opt-in");
    return npc_saved_metadata_fixture::read_current(out,error,share_metadata);
}
}
#include "npc_saved_metadata_edge.inc"
void reset(int kind,size_t failed_copy){
    scenario=kind;fail_read=failed_copy;copies.clear();events.clear();lookup_ids.clear();
    std::memset(reinterpret_cast<void*>(image),0,4096);
    main_thread=GetCurrentThreadId();host_active=client_active=false;uuid=snapshot="owned-fixture";
    sha=std::string(64,'a');common_epoch=3;
    origin={};origin.generation=17;origin.world_epoch=41;
    origin.map=reinterpret_cast<void*>(image);origin.world=origin.map;
    put(0,uint64_t(41));put(0x20,uint32_t(1));put(0x1c0,image+0x200);put(0x200,image+0x280);
    q::SavedPool pool;pool.region=7;pool.pool.manager=image+0x280;
    for(size_t i=0;i<2;++i){const auto pointer=image+0x500+i*0x100;const auto uid=uint64_t(100+i);
        put(0x100+i*8,pointer);put(0x300+i*0x20,pointer);put(0x308+i*0x20,uid);
        put(0x310+i*0x20,int32_t(1));put(0x314+i*0x20,int32_t(7));
        pool.pool.records.push_back({pointer,uid,1,int32_t(i),int32_t(i+1)});
    }
    pools={{7,pool},{8,q::SavedPool{8,{}}}};
    switch(kind){
    case 2:++main_thread;break;
    case 3:host_active=true;break;
    case 4:client_active=true;break;
    case 5:snapshot="foreign";break;
    case 6:common_epoch=0;break;
    case 7:origin.generation=0;break;
    case 8:origin.world_epoch=0;break;
    case 9:put(0,uint64_t(42));break;
    case 14:put(0x310,int32_t(2));break;
    case 17:put(0x200,image+0x288);break;
    default:break;
    }
}
struct Outcome {
    bool result=false;std::string error;std::vector<Current> output;
    std::vector<Read> reads;std::vector<std::string> sequence;std::vector<uint64_t> finds;
    uint64_t os=0,resets=0;
};
Outcome run(bool candidate,bool share,bool parent,int kind=0,size_t failed_copy=0){
    reset(kind,failed_copy);Outcome out;out.output.push_back({"sentinel",-1,{}});
    t::Scope take(t::Part::Take);
    {
        t::PageReadScope scope(parent);auto* caller=t::page_read_scope;
        MEMORY_BASIC_INFORMATION page{};
        if(parent)need(t::virtual_query(reinterpret_cast<void*>(image),&page,sizeof page)==sizeof page,"prime saved-reader parent");
        const auto before=count(t::Part::MemoryVirtualQuery),resets=count(t::Part::ReaderNestedReset);
        if(candidate)out.result=share?actual_owner_edge(out.output,out.error):read_current(out.output,out.error,false);
        else out.result=baseline_read_current(out.output,out.error);
        need(t::page_read_scope==caller,"saved reader changed parent lifetime");
        if(parent)need(t::virtual_query(reinterpret_cast<void*>(image),&page,sizeof page)==sizeof page,"saved-reader post-call parent read");
        out.os=count(t::Part::MemoryVirtualQuery)-before;out.resets=count(t::Part::ReaderNestedReset)-resets;
    }
    out.reads=copies;out.sequence=events;out.finds=lookup_ids;
    need(!t::page_read_scope,"saved-reader fixture leaked scope");return out;
}
void same(const Outcome& before,const Outcome& after){
    need(before.result==after.result&&before.error==after.error,"saved reader original result/cause changed");
    need(before.reads==after.reads&&before.sequence==after.sequence&&before.finds==after.finds,
         "saved reader changed field copies or native lookup order/count");
    need(before.output.size()==after.output.size(),"saved reader output count changed");
    for(size_t i=0;i<before.output.size();++i)
        need(before.output[i].key==after.output[i].key&&before.output[i].saved_region==after.output[i].saved_region&&
             before.output[i].row==after.output[i].row,"saved reader output fields changed");
}
}
void npc_saved_metadata_cases(){
    using namespace npc_saved_metadata_fixture;
    struct Owned {void* page=nullptr;~Owned(){if(page&&!VirtualFree(page,0,MEM_RELEASE))
        std::cerr<<"saved-reader fixture release failed "<<GetLastError()<<'\n';}} owned;
    owned.page=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    need(owned.page!=nullptr,"allocate saved-reader fixture page");image=reinterpret_cast<uintptr_t>(owned.page);
    prefix_fixture::observation.enabled=false;t::totals={};
    const auto before=run(false,false,true),after=run(true,true,true);same(before,after);
    need(after.result&&after.output.size()==2&&after.finds==std::vector<uint64_t>({100,101}),
         "actual saved reader lost current rows or original FindUID calls");
    need(before.resets==2&&after.resets==0&&before.os==2&&after.os==0,
         "opt-in edge failed to retain primed metadata through child and parent continuation");
    for(int kind=1;kind<=17;++kind){
        const auto old=run(false,false,true,kind),now=run(true,true,true,kind);same(old,now);
        need(!now.result&&now.output.empty(),"failed saved read published rows or succeeded");
        if(kind==10)need(now.finds.size()==2&&now.error=="Saved NPC native world generation changed",
                          "post-lookup world change reused admission values");
        if(kind==15)need(now.finds.size()==1&&now.error=="NPC UID lookup exception=3221225477",
                          "actual FindUID SEH was lost or replayed");
    }
    for(size_t failure=1;failure<=before.reads.size();++failure){
        const auto old=run(false,false,true,0,failure),now=run(true,true,true,0,failure);same(old,now);
        need(!now.result&&now.output.empty(),"failed fresh copy published a saved receipt");
    }
    for(bool parent:{false,true}){
        const auto old=run(false,false,parent),now=run(true,false,parent);same(old,now);
        need(old.os==now.os&&old.resets==now.resets,"standalone saved reader boundary changed");
    }
    const auto unscoped=run(true,true,false);same(before,unscoped);
    need(unscoped.resets==0&&unscoped.os==1,"opt-in without parent failed to create local reader scope");
    reset(0,0);t::totals={};
    {t::PageReadScope parent;std::vector<Current> rows;std::string error;
        need(actual_owner_edge(rows,error)&&rows.size()==2,"outside-Take saved owner read failed");
        put(0x314,int32_t(91));
        need(actual_owner_edge(rows,error)&&rows[0].row.level==91&&lookup_ids.size()==4,
             "opt-in cached NPC values or skipped fresh FindUID calls on next read");}
    need(count(t::Part::MemoryVirtualQuery)==0&&!t::page_read_scope,"outside-Take saved reader changed diagnostics/lifetime");
    t::totals={};
}
