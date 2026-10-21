#ifdef MT2MP_ROOT_REGION_SCOPE_ONLY
#include "root_region_scope_entry.inc"
#else
#include "live_host_state/memory_query_timing.h"
#include "subscriber_quest_state.h"
#include "npc_quest_records.h"
#include "npc_quest_runtime_apply.h"
#include "npc_quest_visitor_reader.h"
#include "live_host_state/day_rollover/economy/monetary.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace timing=mt2mp::game::live_host_state::apply_timing;
namespace live_host_state=mt2mp::game::live_host_state;
namespace apply_timing=timing;
namespace quest_state=mt2mp::subscriber_quest_state;
namespace npc_quest_records=mt2mp::npc_quest_records;
namespace npc_quest_runtime=mt2mp::npc_quest_runtime;
namespace npc_quest_visitor_reader=mt2mp::npc_quest_visitor_reader;
int checks=0;
void need(bool okay,const char* message){++checks;if(!okay)throw std::runtime_error(message);}
bool fail(std::string& e,const char* message){e=message;return false;}
bool fail(std::string& e,const std::string& message){e=message;return false;}
namespace host_copy {
bool fail(std::string& e,const char* message){if(e.empty())e=message;return false;}
struct ProcessMemory {bool copy(uintptr_t,void*,size_t,std::string&);};
#include "host_copy.inc"
}
namespace root_copy {
#include "root_copy.inc"
}
namespace placement_copy {
struct ProcessMemory {bool copy(uintptr_t,void*,size_t,std::string&);};
#include "placement_copy.inc"
}
unsigned char* allocation=nullptr;
uint64_t queries(){return timing::totals[size_t(timing::Part::MemoryVirtualQuery)].count;}
bool read_bytes(int backend,uintptr_t address,void* out,size_t count,std::string& e){
    if(backend==0){host_copy::ProcessMemory m;return m.copy(address,out,count,e);}
    if(backend==1)return root_copy::bytes(reinterpret_cast<void*>(address),0,out,count,e);
    placement_copy::ProcessMemory m;return m.copy(address,out,count,e);
}
int backend=0;
bool inject_failure=false,inject_exception=false,fixture_changed=false;
bool scan(std::string& e){
    if(inject_failure)return fail(e,"original owner cause");
    if(inject_exception)throw std::runtime_error("original owner exception");
    for(size_t i=0;i<1000;++i){unsigned char actual=0;if(fixture_changed&&i==500)allocation[0]=77;
        if(!read_bytes(backend,uintptr_t(allocation),&actual,1,e))return false;
        if(actual!=allocation[0])return fail(e,"stale byte value");}
    return true;
}
namespace owner_entry {
namespace action {struct Context {int id;};struct Binding {int id;};}
struct StateOwner {int id;};
int calls=0;
bool state_owners_impl(const action::Context& context,const action::Binding& binding,
                      std::vector<StateOwner>& out,std::string& e){
    ++calls;if(context.id!=11||binding.id!=22)return fail(e,"original identity guard");
    if(!scan(e))return false;out={{33}};return true;
}
#include "state_owners.inc"
}
struct Pin {uintptr_t image;};
struct QuestOwner {std::string key;uintptr_t npc;std::vector<std::pair<int32_t,uintptr_t>> quests;};
size_t value_reads=0;
struct ValueMemory:placement_copy::ProcessMemory {
 bool copy(uintptr_t address,void* out,size_t size,std::string& e){++value_reads;return placement_copy::ProcessMemory::copy(address,out,size,e);}
} memory;
class QuestMemory final:public mt2mp::npc_census::Memory {
public:bool copy(uintptr_t address,void* out,size_t count,std::string& e)override{return memory.copy(address,out,count,e);}
};
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){return memory.copy(p+offset,&out,sizeof out,e);}
bool ap(uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){uintptr_t actual=0;return at(p,0,actual,e)&&(actual==image+expected||fail(e,"original NPC class differs"));}
#include "quest_npcs.inc"
bool progress_pointers_scoped(const Pin& p,uintptr_t toon,std::vector<uintptr_t>& rows,std::string& e){
    constexpr bool scalar_only=false;
#include "progress_pointers_scoped.inc"
    return pointers(rows);
}
bool progress_pointers_unscoped(const Pin& p,uintptr_t toon,std::vector<uintptr_t>& rows,std::string& e){
#include "progress_pointers_unscoped.inc"
    return pointers(rows);
}
template<class T>void put(uintptr_t p,size_t offset,const T& value){std::memcpy(reinterpret_cast<void*>(p+offset),&value,sizeof value);}
void daily(uintptr_t object,size_t offset,uintptr_t image,uintptr_t data){
    put(object,offset,image+0x16e4710);put(object,offset+0x38,image+0x1749370);
    put(object,offset+0x40,data);put(object,offset+0x48,int32_t(2));put(object,offset+0x4c,int32_t(2));
    put(object,offset+0x50,int32_t(3));put(object,offset+0x54,int32_t(8));
}
#include "motion_double_scope_cases.inc"
int main(){try{
    allocation=static_cast<unsigned char*>(VirtualAlloc(nullptr,32768,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));need(allocation!=nullptr,"owned allocation");
    timing::Scope measured(timing::Part::Take);std::string e;
    for(backend=0;backend<3;++backend){
        allocation[0]=31;auto start=queries();need(scan(e),"uncached actual adapter");need(queries()-start==1000,"uncached queries remain fresh");
        std::vector<owner_entry::StateOwner> rows;const auto before_calls=owner_entry::calls;
        start=queries();SetLastError(123);need(owner_entry::state_owners({11},{22},rows,e),"actual owner entry");
        need(queries()-start<=2,"scoped adapter avoids repeated OS query");need(GetLastError()==123,"LastError preserved");
        need(rows.size()==1&&rows[0].id==33&&owner_entry::calls==before_calls+1,"arguments output and call count preserved");
        need(timing::page_read_scope==nullptr,"owner read ends before mutation");
        fixture_changed=true;need(owner_entry::state_owners({11},{22},rows,e)&&allocation[0]==77,"changed bytes read live");fixture_changed=false;
        inject_failure=true;e.clear();need(!owner_entry::state_owners({11},{22},rows,e)&&e=="original owner cause","original failure retained");inject_failure=false;
        inject_exception=true;bool caught=false;try{owner_entry::state_owners({11},{22},rows,e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original owner exception";}inject_exception=false;
        need(caught&&timing::page_read_scope==nullptr,"exception and unwind preserved");
        need(!owner_entry::state_owners({12},{22},rows,e)&&e=="original identity guard","identity rejection retained");
        DWORD old=0;need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&old)!=0,"owned page inaccessible");e.clear();
        need(!owner_entry::state_owners({11},{22},rows,e)&&!e.empty(),"next reader checks changed protection");DWORD unused=0;
        need(VirtualProtect(allocation,4096,old,&unused)!=0,"restore owned page");
        {timing::PageReadScope outer;unsigned char value=0;need(read_bytes(backend,uintptr_t(allocation),&value,1,e),"prime outer metadata");
            {timing::PageReadScope original_lookup;need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&old)!=0,"original boundary changes mapping");}
            e.clear();need(!read_bytes(backend,uintptr_t(allocation),&value,1,e),"post-original mapping is rechecked");
            need(VirtualProtect(allocation,4096,old,&unused)!=0,"restore callback test page");}
    }
    const uintptr_t npc=uintptr_t(allocation),region=npc+4096,quest=npc+8192,data=npc+12288,image=0x10000000;
    std::memset(allocation,0,16384);put(npc,0,image+0x1742980);put(npc,0x178,region);put(region,0x834,int32_t(67));
    put(npc,0x6f8,int32_t(2));put(npc,0x80c,int32_t(0));put(data,0,int32_t(4));put(data,4,int32_t(5));
    daily(npc,0x898,image,data);daily(npc,0x908,image,data);daily(quest,0x100,image,data);daily(quest,0x170,image,data);
    put(quest,0,image+0x174ac10);put(quest,0x1e0,image+0x1787870);
    const npc_quest_runtime::Value runtime{{std::bit_cast<uint32_t>(0.25f),std::bit_cast<uint32_t>(0.5f),std::bit_cast<uint32_t>(0.75f)},true,12,7};
    put(quest,0x1f8,runtime.rating_bits);put(quest,0x204,uint8_t(runtime.rating_set));put(quest,0x88,runtime.xp_reward);put(quest,0x8c,runtime.gold_reward);
    const std::vector<QuestOwner> owners={{"snapshot:fixture",npc,{{7,quest}}}};
    std::vector<quest_state::Npc> rows;auto start=queries();e.clear();
    need(quest_npcs({image},owners,rows,e),"actual complete NPC and Quest counter reader");
    need(queries()-start<10,"actual NPC counter reader caches page metadata");
    need(rows.size()==1&&rows[0].level==2&&rows[0].pose.region==67&&rows[0].accepted.history==std::vector<int32_t>({4,5})&&rows[0].quests[0].sequence_id==7,"actual complete values retained");
    need(rows[0].quests[0].runtime==runtime,"actual Quest sampler lost complete RuntimeValue");
    put(npc,0x6f8,int32_t(3));put(data,4,int32_t(9));need(quest_npcs({image},owners,rows,e)&&rows[0].level==3&&rows[0].quests[0].completed.history[1]==9,"next NPC read sees changed data");
    const uintptr_t second_npc=npc+16384,second_quest=npc+20480;
    std::memcpy(reinterpret_cast<void*>(second_npc),reinterpret_cast<void*>(npc),0x980);
    std::memcpy(reinterpret_cast<void*>(second_quest),reinterpret_cast<void*>(quest),0x220);
    std::vector<QuestOwner> pair=owners;pair.push_back({"snapshot:second",second_npc,{{7,second_quest}}});
    value_reads=0;need(quest_npcs({image},pair,rows,e)&&rows.size()==2,"full pair value reader");
    const auto full_reads=value_reads;const auto expected_pair=rows;
    quest_state::Npc selected;value_reads=0;
    need(quest_npc({image},pair[1],selected,e)&&selected==expected_pair[1],"selected second row equals full reader values");
    need(value_reads*2==full_reads,"two equally sized NPC rows require exactly half the value reads");
    const auto selected_reads=value_reads;
    put(second_npc,0x6f8,int32_t(14));need(quest_npc({image},pair[1],selected,e)&&selected.level==14,"selected changed level remains fresh");
    put(second_quest,0x100+0x54,int32_t(19));need(quest_npc({image},pair[1],selected,e)&&selected.quests[0].accepted.total==19,"selected changed Quest counter remains fresh");
    put(second_quest,0x88,int32_t(33));need(quest_npc({image},pair[1],selected,e)&&selected.quests[0].runtime.xp_reward==33,"selected RuntimeValue remained cached");
    put(second_quest,0x1e0,uintptr_t(0));e.clear();need(!quest_npc({image},pair[1],selected,e)&&e=="Quest rating AP, bool or finite values differ","selected RuntimeValue rating class guard missing");
    put(second_quest,0x1e0,image+0x1787870);
    put(second_npc,0,uintptr_t(0));e.clear();need(!quest_npc({image},pair[1],selected,e)&&e=="original NPC class differs","selected class mismatch rejected");
    put(second_npc,0,image+0x1742980);
    put(second_quest,0x100+0x38,uintptr_t(0));e.clear();
    need(!quest_npc({image},pair[1],selected,e)&&e=="Quest accepted history array AP differs","selected DailyCount guard retained");
    daily(second_quest,0x100,image,data);
    put(npc,0,uintptr_t(0));value_reads=0;e.clear();
    need(quest_npc({image},pair[1],selected,e)&&value_reads==selected_reads,"unselected value class not reread at selected store");
    e.clear();need(!quest_npcs({image},pair,rows,e)&&e=="original NPC class differs","unchanged final full reader still rejects unselected corruption");
    put(npc,0,image+0x1742980);e.clear();
    need(quest_npcs({image},pair,rows,e)&&rows[1]==selected,"full reader sees selected state after simulated effects");
    put(second_npc,0x80c,int32_t(1));e.clear();
    need(!quest_npc({image},pair[1],selected,e)&&e=="NPC state non-QuestGiver owns Quest rows","selected type/Quest mismatch retained");
    put(second_npc,0x80c,int32_t(0));
    DWORD second_protect=0,second_restored=0;
    need(VirtualProtect(reinterpret_cast<void*>(second_npc),4096,PAGE_NOACCESS,&second_protect)!=0,"selected page unavailable");e.clear();
    need(!quest_npc({image},pair[1],selected,e)&&e=="error: native placement UI page unreadable","selected read failure preserves upstream cause");
    need(VirtualProtect(reinterpret_cast<void*>(second_npc),4096,second_protect,&second_restored)!=0,"restore selected page");
    std::cout<<"NPC value reads per selected store: "<<full_reads<<" -> "<<selected_reads<<" for two rows\n";
    put(quest,0x100+0x38,uintptr_t(0));need(!quest_npcs({image},owners,rows,e)&&e=="Quest accepted history array AP differs","actual counter class guard retained");
    need(timing::page_read_scope==nullptr,"no retained cache after NPC failure");
    // All fields share one owned homogeneous region. Compare the exact
    // original body with the candidate; only the local metadata scope differs.
    const uintptr_t toon=npc,array=region,first=quest;constexpr size_t progress_count=8;
    std::memset(allocation,0,16384);put(toon,0xaf0,image+0x16ee0e0);put(toon,0xaf8,array);
    put(toon,0xb00,int32_t(progress_count));put(toon,0xb04,int32_t(progress_count));
    std::vector<uintptr_t> expected_pointers;
    for(size_t i=0;i<progress_count;++i){auto pointer=first+i*64;put(array,i*8,pointer);put(pointer,0,image+0x172d130);expected_pointers.push_back(pointer);}
    std::vector<uintptr_t> pointers_before,pointers_after;start=queries();e.clear();
    need(progress_pointers_unscoped({image},toon,pointers_before,e),"original full Progress pointer reader");
    const auto original_queries=queries()-start;need(original_queries==4+2*progress_count,"original pointer reader performs four header and two reads per row");
    start=queries();SetLastError(123);need(progress_pointers_scoped({image},toon,pointers_after,e),"scoped actual Progress pointer reader");
    const auto scoped_queries=queries()-start;need(scoped_queries==1,"homogeneous region requires one OS query inside pure pointer reader");
    need(pointers_before==expected_pointers&&pointers_after==expected_pointers,"all exact current ordered pointers retained");
    need(GetLastError()==123&&timing::page_read_scope==nullptr,"pointer scope closes and preserves LastError before caller effects");
    start=queries();for(int i=0;i<3;++i)need(progress_pointers_scoped({image},toon,pointers_after,e),"each invocation freshly admits pointer metadata");
    need(queries()-start==3,"no metadata survives between pointer reader calls");
    put(array,0,expected_pointers.back());
    need(progress_pointers_scoped({image},toon,pointers_after,e)&&pointers_after[0]==expected_pointers.back(),"changed pointer bytes remain fresh");
    put(array,0,expected_pointers.front());put(toon,0xb04,int32_t(7));e.clear();
    need(!progress_pointers_scoped({image},toon,pointers_after,e)&&e=="Quest apply Progress array header differs"&&timing::page_read_scope==nullptr,"header failure keeps original cause and closes scope");
    put(toon,0xb04,int32_t(8));put(first,0,uintptr_t(0));e.clear();
    need(!progress_pointers_scoped({image},toon,pointers_after,e)&&e=="original NPC class differs"&&timing::page_read_scope==nullptr,"AP failure keeps original helper cause and closes scope");
    put(first,0,image+0x172d130);
    need(timing::page_read_scope==nullptr,"no pointer metadata at scalar CAS boundary");
    put(toon,0x850,int32_t(11));need(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(toon+0x850),12,11)==11,"caller scalar CAS occurs outside pointer metadata scope");
    // A native callback can change page protection only after the reader exits.
    need(timing::page_read_scope==nullptr,"no pointer metadata at native callback boundary");
    DWORD old_protect=0,restored=0;need(VirtualProtect(reinterpret_cast<void*>(array),4096,PAGE_NOACCESS,&old_protect)!=0,"owned callback changes array protection");e.clear();
    need(!progress_pointers_scoped({image},toon,pointers_after,e)&&e=="error: native placement UI page unreadable"&&timing::page_read_scope==nullptr,"next invocation rejects changed callback protection with original cause");
    need(VirtualProtect(reinterpret_cast<void*>(array),4096,old_protect,&restored)!=0,"restore owned pointer array protection");
    std::cout<<"Progress pointer OS queries: "<<original_queries<<" -> "<<scoped_queries<<" for "<<progress_count<<" rows in one homogeneous region\n";
    const auto owner_before=timing::totals[size_t(timing::Part::QuestCanonicalOwners)].count;
    std::vector<owner_entry::StateOwner> timed_rows;
    {timing::Scope quest_phase(timing::Part::QuestStores);
        need(owner_entry::state_owners({11},{22},timed_rows,e),"timed actual owner entry succeeds");
        inject_failure=true;e.clear();
        need(!owner_entry::state_owners({11},{22},timed_rows,e)&&e=="original owner cause","timed owner failure retains original cause");inject_failure=false;
        inject_exception=true;bool owner_caught=false;
        try{owner_entry::state_owners({11},{22},timed_rows,e);}catch(const std::runtime_error& error){owner_caught=std::string(error.what())=="original owner exception";}
        inject_exception=false;need(owner_caught,"timed owner exception remains visible");
    }
    need(timing::totals[size_t(timing::Part::QuestCanonicalOwners)].count==owner_before+3&&timing::quest_store_depth==0&&timing::page_read_scope==nullptr,"owner timings count all exits without retaining phase or metadata");
    need(owner_entry::state_owners({11},{22},timed_rows,e)&&timing::totals[size_t(timing::Part::QuestCanonicalOwners)].count==owner_before+3,"outside Quest phase owner work remains fresh and uncounted");
    motion_double_scope::run();
    need(VirtualFree(allocation,0,MEM_RELEASE)!=0,"free owned allocation");allocation=nullptr;
    std::cout<<checks<<" actual Quest reader-scope checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<'\n';if(allocation)VirtualFree(allocation,0,MEM_RELEASE);return 1;}}

#endif
