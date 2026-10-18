#include "live_host_state/apply_timing.h"
#ifdef _WIN32
#include "live_host_state/memory_query_timing.h"
#endif
#include "npc_quest_daily_apply.h"
#include "subscriber_lifecycle/save_scope.h"
#include <map>

namespace daily_guard_test {
#ifdef _WIN32
namespace apply_timing=mt2mp::game::live_host_state::apply_timing;
unsigned char* metadata_page=nullptr;
#else
// Portable semantic cases do not emulate Windows page metadata.
namespace apply_timing {
enum class PageBoundary {OriginalCall};
struct PageReadScope {PageReadScope()=default;explicit PageReadScope(PageBoundary){}};
}
#endif
constexpr uintptr_t image=0x10000000,npc=0x10000,quest_address=0x20000,clock_address=0x30000;
struct QuestOwner {std::string key;uintptr_t npc;std::vector<std::pair<int32_t,uintptr_t>> quests;};
struct Trace {
    std::string operation;uintptr_t address=0;size_t size=0;int32_t before=0,after=0;
    bool operator==(const Trace&)const=default;
};
struct Fixture {
    std::map<uintptr_t,unsigned char> bytes;
    QuestOwner owner{"npc",npc,{}};
    unsigned fences=0,censuses=0,cas=0,reserves=0,enters=0,leaves=0,pauses=0;
    unsigned fail_fence=0;bool fail_enter=false;bool fail_census=false,fail_reserve=false,drift_reserve=false,drift_quest=false,drift_enter=false,fail_cas=false,fail_read=false;
    std::vector<std::string> events;
    std::vector<Trace> trace;
    unsigned copies=0;
    bool protect_on_reserve=false;
    bool metadata_at_leave=false,metadata_at_pause=false;
    uint64_t queries_before_reserve=0,queries_after_reserve=0;
} fixture;
unsigned assertions=0;
void require(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);}
bool fail(std::string& e,const char* message){e=message;return false;}
bool fail(std::string& e,const std::string& message){e=message;return false;}
template<class T>void put(uintptr_t p,const T& v){const auto* b=reinterpret_cast<const unsigned char*>(&v);for(size_t i=0;i<sizeof v;++i)fixture.bytes[p+i]=b[i];}
struct Memory {bool copy(uintptr_t p,void* out,size_t n,std::string& e){
    ++fixture.copies;fixture.trace.push_back({"read",p,n});
#ifdef _WIN32
    if(metadata_page){
        MEMORY_BASIC_INFORMATION page{};
        if(!apply_timing::virtual_query(metadata_page,&page,sizeof page,apply_timing::Part::QueryPlacement)||
           page.State!=MEM_COMMIT||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
            return fail(e,"original daily metadata page unreadable");
    }
#endif
    if(fixture.fail_read)return fail(e,"original daily byte read failed");
    auto* b=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){const auto found=fixture.bytes.find(p+i);if(found==fixture.bytes.end())return fail(e,"original missing daily byte");b[i]=found->second;}return true;
}} memory;
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){return memory.copy(p+offset,&out,sizeof out,e);}
bool compare_fixed_i32(uintptr_t p,int32_t old,int32_t target,std::string& e){
    ++fixture.cas;fixture.events.push_back("cas");
    fixture.trace.push_back({"cas",p,4,old,target});
    if(fixture.fail_cas)return fail(e,"original daily CAS failed");
    int32_t actual=0;if(!at(p,0,actual,e))return false;
    if(actual!=old)return fail(e,"original daily CAS prior differs");put(p,target);return true;
}
namespace building_mixed_identity::opened_file {struct CurrentWorld {void* mode=nullptr;};}
namespace native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out={};out.mode=reinterpret_cast<void*>(1);return true;}
}
namespace npc_quest_records=mt2mp::npc_quest_records;
namespace npc_quest_daily_apply=mt2mp::npc_quest_daily_apply;
namespace npc_probe::action_runtime {
bool state_owners(const auto&,const auto&,std::vector<QuestOwner>& out,std::string& e){
    apply_timing::PageReadScope pages;
    ++fixture.censuses;fixture.events.push_back("census");
    fixture.trace.push_back({"census"});
    if(fixture.fail_census)return fail(e,"original daily owner census failed");out={fixture.owner};return true;
}
}
bool quest_restore_pause(const Pin&,std::string&){
    ++fixture.pauses;fixture.events.push_back("pause");fixture.trace.push_back({"pause"});
#ifdef _WIN32
    if(metadata_page)fixture.metadata_at_pause=apply_timing::page_read_scope!=nullptr;
#endif
    return true;
}
uint32_t quest_daily_reserve(uintptr_t,uintptr_t array,int32_t capacity){
    ++fixture.reserves;fixture.events.push_back("reserve");
    fixture.trace.push_back({"reserve",array,size_t(capacity)});
    if(fixture.fail_reserve)return 5;
    const uintptr_t data=array+0x2000;put(array+8,data);put(array+0x14,capacity);
    for(int32_t i=0;i<capacity;++i)put(data+size_t(i)*4,int32_t(0));
    if(fixture.drift_reserve)fixture.owner.npc+=8;
    if(fixture.drift_quest)fixture.owner.quests[0].second+=8;
#ifdef _WIN32
    if(metadata_page&&fixture.protect_on_reserve){
        fixture.queries_before_reserve=apply_timing::totals[size_t(apply_timing::Part::MemoryVirtualQuery)].count;
        MEMORY_BASIC_INFORMATION page{};
        if(!apply_timing::virtual_query(metadata_page,&page,sizeof page,apply_timing::Part::QueryPlacement))return 998;
        fixture.queries_after_reserve=apply_timing::totals[size_t(apply_timing::Part::MemoryVirtualQuery)].count;
        DWORD prior=0;if(!VirtualProtect(metadata_page,4096,PAGE_NOACCESS,&prior))return GetLastError();
    }
#endif
    return 0;
}
}

namespace mt2mp::subscriber_lifecycle {
// Exercise the existing SaveScope body and exception/cause plumbing. Only the
// two original native calls use fixture boundaries instead of game addresses.
uint32_t call(SaveScopeCall fn,void*){
    auto& f=daily_guard_test::fixture;
    if(reinterpret_cast<uintptr_t>(fn)==daily_guard_test::image+0x4a03a0){
        f.trace.push_back({"enter"});
        ++f.enters;f.events.push_back("enter");if(f.fail_enter)return 777;if(f.drift_enter)f.owner.npc+=8;
    }else {
        ++f.leaves;f.events.push_back("leave");f.trace.push_back({"leave"});
#ifdef _WIN32
        if(daily_guard_test::metadata_page)f.metadata_at_leave=game::live_host_state::apply_timing::page_read_scope!=nullptr;
#endif
    }
    return 0;
}
#include "npc_save_scope_actual.inc"
}

namespace daily_guard_test {
#include "npc_daily_guard_actual.inc"
#include "npc_daily_metadata_baseline.inc"
#include "npc_daily_guard_original.inc"
void daily(uintptr_t object,size_t offset,const npc_quest_records::DailyCount& row,bool reserve){
    const uintptr_t address=object+offset,data=address+0x3000;
    put(address,image+0x16e4710);put(address+0x38,image+0x1749370);put(address+0x40,data);
    const std::array<int32_t,8> words{int32_t(row.history.size()),reserve?0:10,row.today,row.total,1,10,0,0};
    put(address+0x48,words);put(address+0x68,uint8_t(0));
    for(int i=0;i<10;++i)put(data+size_t(i)*4,i<int(row.history.size())?row.history[size_t(i)]:0);
}
std::pair<quest_state::Npc,quest_state::Npc> setup(unsigned quests=0,bool slots=false,bool reserve=false){
    fixture={};fixture.owner={"npc",npc,{}};put(clock_address+0x44,int32_t(1));
    const std::array<unsigned char,16> code{0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0xb8,0xfe,0xff,0xff,0xff,0xff,0xff,0x1f};put(image+0xe8080,code);
    quest_state::Npc old;old.key="npc";old.type=0;old.level=1;
    if(!reserve)old.accepted.history=old.completed.history={1};
    for(unsigned i=0;i<quests;++i){quest_state::Counters row;row.sequence_id=int32_t(i);row.accepted=old.accepted;row.completed=old.completed;old.quests.push_back(row);fixture.owner.quests.emplace_back(int32_t(i),quest_address+i*0x1000);}
    auto target=old;
    const auto change=[&](auto& a){a.today=1;a.total=1;if(slots||reserve)a.history={2};};
    change(target.accepted);change(target.completed);
    daily(npc,0x898,old.accepted,reserve);daily(npc,0x908,old.completed,reserve);
    for(unsigned i=0;i<quests;++i){change(target.quests[i].accepted);change(target.quests[i].completed);daily(quest_address+i*0x1000,0x100,old.quests[i].accepted,reserve);daily(quest_address+i*0x1000,0x170,old.quests[i].completed,reserve);}
    return {old,target};
}
bool execute(const quest_state::Npc& old,const quest_state::Npc& target,bool original,std::string& e,bool metadata_baseline=false){
    Pin p;p.image=image;p.clock=clock_address;QuestOwner owner{"npc",npc,{}};
    for(size_t i=0;i<old.quests.size();++i)owner.quests.emplace_back(int32_t(i),quest_address+i*0x1000);
    const auto fence=[](std::string& error){++fixture.fences;fixture.events.push_back("fence");fixture.trace.push_back({"fence"});return fixture.fences!=fixture.fail_fence||fail(error,"original daily fence failed");};
    if(metadata_baseline)return store_npc_counters_metadata_baseline(p,owner,old,target,fence,e);
    return original?store_npc_counters_original(p,owner,old,target,fence,e):store_npc_counters(p,owner,old,target,fence,e);
}
unsigned run(){
    std::string e;
    for(unsigned quests:{0u,2u})for(bool slots:{false,true})for(bool reserve:{false,true}){
        auto [old,target]=setup(quests,slots,reserve);e.clear();require(execute(old,target,true,e),"original actual counters fixture failed");const auto original=fixture;
        setup(quests,slots,reserve);e.clear();require(execute(old,target,false,e),"candidate actual counters fixture failed");
        const unsigned k=2*(1+quests),guards=(reserve?5u:(slots?4u:3u))*k+1;
        require(original.censuses==guards&&fixture.censuses==(reserve?k+1:1),"exact original/candidate census reduction differs");
        require(original.fences==fixture.fences&&fixture.fences==guards,"per-guard live fence was skipped");
        require(original.cas==fixture.cas&&original.bytes==fixture.bytes,"original CAS count or exact resulting native bytes changed");
        require(original.reserves==fixture.reserves&&fixture.enters==1&&fixture.leaves==1&&fixture.pauses==1,"original Reserve/SaveScope/pause calls changed");
        require(fixture.events.front()=="enter"&&fixture.events[1]=="fence"&&fixture.events[2]=="census","first owner proof must follow actual SaveScope enter");
    }
    // Compare against the exact immediately preceding production body as well
    // as the older owner-proof baseline retained by the original cases below.
    for(unsigned scenario=0;scenario<14;++scenario){
        const bool reserve=scenario==1||scenario==4||scenario==5||scenario==6;
        const auto configure=[&](quest_state::Npc& target,const quest_state::Npc& old){
            if(scenario==2)fixture.fail_enter=true;
            if(scenario==3)fixture.fail_census=true;
            if(scenario==4)fixture.fail_reserve=true;
            if(scenario==5)fixture.drift_reserve=true;
            if(scenario==6)fixture.drift_quest=true;
            if(scenario==7)fixture.fail_cas=true;
            if(scenario==8)fixture.fail_read=true;
            if(scenario==9)fixture.fail_fence=1;
            if(scenario==10)fixture.fail_fence=4;
            if(scenario==11)target=old;
            if(scenario==12){target.completed=old.completed;target.quests=old.quests;}
            if(scenario==13)fixture.drift_enter=true;
        };
        auto [old,target]=setup(2,true,reserve);configure(target,old);e.clear();
        const bool before=execute(old,target,false,e,true);const auto baseline=fixture;const auto cause=e;
        setup(2,true,reserve);configure(target,old);e.clear();
        const bool after=execute(old,target,false,e);
        require(before==after&&e==cause,"metadata change altered outcome or original failure cause");
        require(baseline.trace==fixture.trace,"metadata change altered exact field/CAS/guard/native ordering");
        require(baseline.bytes==fixture.bytes&&baseline.copies==fixture.copies,"metadata change altered bytes or live copy count");
        require(baseline.censuses==fixture.censuses&&baseline.fences==fixture.fences&&baseline.cas==fixture.cas&&
            baseline.reserves==fixture.reserves&&baseline.enters==fixture.enters&&baseline.leaves==fixture.leaves&&
            baseline.pauses==fixture.pauses,"metadata change altered guard/native/CAS attempt counts");
        if(scenario==11)require(after&&!fixture.enters&&!fixture.cas&&!fixture.copies,"unchanged counters entered a native or read scope");
    }
#ifdef _WIN32
    metadata_page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    require(metadata_page!=nullptr,"allocate owned DailyCount metadata page");
    for(bool reserve:{false,true}){
        auto [old,target]=setup(2,true,reserve);apply_timing::totals={};e.clear();
        bool okay=false;
        {apply_timing::Scope measured(apply_timing::Part::Take);okay=execute(old,target,false,e,true);}
        require(okay,"pre-metadata actual DailyCount body failed");
        const auto baseline=fixture;
        const auto before_queries=apply_timing::totals[size_t(apply_timing::Part::MemoryVirtualQuery)].count;
        setup(2,true,reserve);apply_timing::totals={};e.clear();
        {apply_timing::Scope measured(apply_timing::Part::Take);okay=execute(old,target,false,e);}
        const auto after_queries=apply_timing::totals[size_t(apply_timing::Part::MemoryVirtualQuery)].count;
        require(okay&&fixture.trace==baseline.trace&&fixture.bytes==baseline.bytes,"real metadata scope changed the production body sequence or bytes");
        require(fixture.copies==baseline.copies&&after_queries<before_queries,"real DailyCount metadata reuse must reduce OS queries without dropping copies");
        require(!apply_timing::page_read_scope&&!fixture.metadata_at_leave&&!fixture.metadata_at_pause,
                "DailyCount metadata scope crossed SaveScope leave/pause or survived return");
        std::cout<<"DailyCount metadata queries reserve="<<reserve<<": "<<before_queries<<" -> "<<after_queries
                 <<"; live copies "<<baseline.copies<<" -> "<<fixture.copies<<"\n";
    }
    std::vector<Trace> rejected_trace;std::string rejected_cause;
    for(bool baseline:{true,false}){
        auto [old,target]=setup(0,false,true);fixture.protect_on_reserve=true;
        apply_timing::totals={};e.clear();bool okay=false;
        {apply_timing::Scope measured(apply_timing::Part::Take);okay=execute(old,target,false,e,baseline);}
        require(!okay&&e=="original daily metadata page unreadable","post-Reserve read accepted stale readable metadata");
        require(fixture.queries_after_reserve==fixture.queries_before_reserve+1,"Reserve callback inherited pre-call page metadata");
        require(apply_timing::totals[size_t(apply_timing::Part::MemoryVirtualQuery)].count>fixture.queries_after_reserve,
                "post-Reserve read reused callback page metadata");
        require(fixture.reserves==1&&!fixture.cas&&fixture.enters==1&&fixture.leaves==1&&!fixture.pauses,
                "Reserve readability failure replayed or changed native cleanup");
        require(!apply_timing::page_read_scope,"failed DailyCount left metadata live");
        if(baseline){rejected_trace=fixture.trace;rejected_cause=e;}
        else require(fixture.trace==rejected_trace&&e==rejected_cause,"Reserve page failure changed original field/native sequence or cause");
        DWORD prior=0;require(VirtualProtect(metadata_page,4096,PAGE_READWRITE,&prior)!=0,"restore owned DailyCount metadata page");
    }
    require(VirtualFree(metadata_page,0,MEM_RELEASE)!=0,"release owned DailyCount metadata page");metadata_page=nullptr;
#endif
    for(unsigned fault=1;fault<=7;++fault){auto [old,target]=setup();fixture.fail_fence=fault;e.clear();const bool before=execute(old,target,true,e);const auto original=fixture;const auto cause=e;
        setup();fixture.fail_fence=fault;e.clear();const bool after=execute(old,target,false,e);
        require(before==after&&!after&&e==cause&&fixture.cas==original.cas&&fixture.enters==original.enters&&fixture.leaves==original.leaves,"first/all fence failures lost original cause or effect boundary");}
    {auto [old,target]=setup();fixture.drift_enter=true;e.clear();require(!execute(old,target,false,e)&&fixture.cas==0&&fixture.leaves==0&&e.find("canonical NPC/Quest owners changed")!=std::string::npos,"pre-enter proof survived owner change during enter");}
    for(bool quest_drift:{false,true}){auto [old,target]=setup(1,false,true);fixture.drift_reserve=!quest_drift;fixture.drift_quest=quest_drift;e.clear();
        require(!execute(old,target,false,e)&&fixture.reserves==1&&fixture.cas==0&&fixture.leaves==0&&fixture.censuses==3&&e.find("canonical NPC/Quest owners changed")!=std::string::npos,"Reserve changed owner reached next CAS or cleanup reused old identity");}
    {auto [old,target]=setup(0,false,true);fixture.fail_reserve=true;e.clear();require(!execute(old,target,false,e)&&fixture.reserves==1&&fixture.cas==0&&fixture.censuses==2&&fixture.leaves==1&&e=="DailyCount original Reserve exception=5; no replay","failed Reserve did not invalidate proof or preserve cleanup/cause");}
    {auto [old,target]=setup();fixture.fail_cas=true;e.clear();require(!execute(old,target,false,e)&&fixture.cas==1&&fixture.leaves==1&&e=="original daily CAS failed","CAS failure/SaveScope cleanup changed");}
    {auto [old,target]=setup();fixture.fail_read=true;e.clear();require(!execute(old,target,false,e)&&!fixture.cas&&fixture.leaves==1&&e=="original daily byte read failed","fresh byte failure was hidden");}
    {auto [old,target]=setup();fixture.fail_census=true;e.clear();require(!execute(old,target,false,e)&&!fixture.cas&&!fixture.leaves&&e.find("original daily owner census failed")!=std::string::npos,"owner census failure cause lost");}
    {auto [old,target]=setup();put(npc+0x898+0x50,int32_t(99));e.clear();require(!execute(old,target,false,e)&&!fixture.cas&&e=="DailyCount preflight value differs","fresh actual daily value mismatch accepted");}
    {auto [old,target]=setup();e.clear();require(execute(old,target,false,e),"first complete counters call failed");
        setup();fixture.owner.npc+=8;e.clear();require(!execute(old,target,false,e)&&fixture.censuses==2&&!fixture.cas,"next call reused previous SaveScope owner proof");}
    namespace timed=mt2mp::game::live_host_state::apply_timing;
    for(bool enabled:{false,true})for(bool enter_failure:{false,true}){
        fixture={};fixture.fail_enter=enter_failure;
        const auto count_before=timed::totals[size_t(timed::Part::QuestStoreSaveEnter)].count;
        struct TimedWork {bool enabled;uint64_t before;bool body=false,guard=false;} work{enabled,count_before};
        const auto body=[](void* user,std::string&)->bool{
            auto& data=*static_cast<TimedWork*>(user);data.body=true;fixture.events.push_back("body");
            require(timed::totals[size_t(timed::Part::QuestStoreSaveEnter)].count==data.before+(data.enabled?1u:0u),"enter timer ends before body");return true;
        };
        const auto guard=[](void* user,std::string&)->bool{static_cast<TimedWork*>(user)->guard=true;fixture.events.push_back("guard");return true;};
        mt2mp::subscriber_lifecycle::SaveScopeApi api;
        api.enter=reinterpret_cast<mt2mp::subscriber_lifecycle::SaveScopeCall>(image+0x4a03a0);
        api.leave=reinterpret_cast<mt2mp::subscriber_lifecycle::SaveScopeCall>(image+0x4a03b0);
        mt2mp::subscriber_lifecycle::SaveScopeReceipt receipt;bool okay=false;
        const auto invoke=[&](){okay=mt2mp::subscriber_lifecycle::with_save_scope(api,reinterpret_cast<void*>(1),body,guard,&work,receipt,e);};
        if(enabled){timed::Scope measured(timed::Part::QuestStores);invoke();}else invoke();
        require(timed::totals[size_t(timed::Part::QuestStoreSaveEnter)].count==count_before+(enabled?1u:0u),"exact gated native enter count on success/failure");
        require(receipt.enter_attempted&&fixture.enters==1&&timed::quest_store_depth==0,"one original enter and closed measurement phase");
        if(enter_failure){
            require(!okay&&receipt.enter_exception==777&&receipt.unknown&&!receipt.enter_returned&&e=="native SaveScope Increment exception=777","original enter failure receipt and cause preserved");
            require(!work.body&&!work.guard&&!fixture.leaves&&fixture.events==std::vector<std::string>{"enter"},"failed enter does not execute body guard or leave");
        }else{
            require(okay&&receipt.enter_returned&&receipt.body_attempted&&receipt.exit_guard_confirmed&&receipt.leave_returned&&!receipt.unknown,"successful receipt fields unchanged");
            require(fixture.events==std::vector<std::string>({"enter","body","guard","leave"}),"original enter body guard leave ordering unchanged");
        }
    }
    require(timed::snapshot().contains("quest_store_save_enter"),"native enter category exported through existing snapshot");
    std::cout<<"Daily guard census calls: 7 -> 1 for 2 scalar-only DailyCounts; 25 -> 1 for 6 history-CAS DailyCounts; 31 -> 7 with 6 Reserves. Fence/CAS/callback counts and bytes unchanged.\n";
    std::cout<<"PASS "<<assertions<<" actual DailyCount guard/SaveScope cases\n";
    return assertions;
}
}
