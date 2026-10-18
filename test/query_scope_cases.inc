#include <functional>
#include <optional>
#include <vector>

namespace loop_probe {
namespace apply_timing=t;
struct Subscriber {std::optional<int> quest_state;};
struct Npc {int value=0;};
bool same_npc_non_pose(const Npc& a,const Npc& b){return a.value==b.value;}
struct State {std::vector<Subscriber> subscribers;std::vector<Npc> npcs;};
struct Receipt {bool attempted=false,unknown=false,confirmed=false;size_t attempted_stores=0,stores=0;std::string cause;};
struct Port {
    const State* expected=nullptr;std::vector<std::string> calls;std::string failure;bool throws=false;
    size_t begun=0,ended=0,closed=0;bool begin_ok=true,close_ok=true;
    bool with_quest_stores(const State&,const State&,const std::function<bool(std::string&)>& body,std::string& error){
        if(!body(error))return false;
        ++closed;if(!close_ok){error="original phase close failure";return false;}return true;
    }
    bool begin_quest_stores(const State& actual,std::string& error){
        need(&actual==expected,"actual Quest phase forwards retained owner receipt");++begun;
        if(!begin_ok){error="original Quest phase admission cause";return false;}return true;
    }
    void end_quest_stores()noexcept{++ended;}
    bool call(const State& actual,size_t index,const char* kind,std::string& error){
        need(&actual==expected,"actual loop forwards retained owner receipt");
        calls.push_back(std::string(kind)+std::to_string(index));
        if(throws)throw std::runtime_error("original loop callback exception");
        if(calls.back()==failure){error="original loop callback cause";return false;}return true;
    }
    bool store_quest_state(const State& actual,size_t index,const std::optional<int>& old,const std::optional<int>& after,std::string& error){
        need(old==expected->subscribers.at(index).quest_state&&old!=after,"actual Quest loop forwards exact before/changed target");
        return call(actual,index,"q",error);
    }
    bool store_npc_state(const State& actual,size_t index,const Npc& old,const Npc& after,std::string& error){
        need(old.value==expected->npcs.at(index).value&&old.value!=after.value,"actual NPC loop forwards exact before/changed target");
        need(closed>0&&close_ok,"NPC starts only after explicit successful Quest phase close");
        return call(actual,index,"n",error);
    }
};
Receipt run(Port& port,const State& expected,const State& target){
    Receipt receipt;const auto& before=expected;
    const auto stop=[&](const std::string& stage){receipt.unknown=receipt.attempted;receipt.cause=stage+" original="+receipt.cause+"; no replay";};
#include "quest_store_loop.inc"
#include "npc_store_loop.inc"
    receipt.confirmed=true;return receipt;
}
void cases(){
    State before{{{1},{2},{std::nullopt}},{{7},{8}}},target{{{1},{3},{4}},{{9},{8}}};
    Port port;port.expected=&before;
    const auto q=count(t::Part::QuestStores),n=count(t::Part::NpcStores);
    auto result=run(port,before,target);
    need(result.confirmed&&result.attempted_stores==3&&result.stores==3&&port.calls==std::vector<std::string>{"q1","q2","n0"},"actual loops preserve order/skip/attempted/completed stores");
    need(port.begun==1&&port.ended==1,"actual Quest scope ends once on success");
    need(count(t::Part::QuestStores)==q+1&&count(t::Part::NpcStores)==n+1,"one timer per actual loop");
    port.calls.clear();result=run(port,before,before);
    need(result.confirmed&&!result.attempted&&port.calls.empty(),"unchanged authoritative state causes no stores");
    port.failure="q1";port.calls.clear();const auto npc_before=count(t::Part::NpcStores);result=run(port,before,target);
    need(!result.confirmed&&result.unknown&&result.attempted_stores==1&&result.stores==0&&port.calls==std::vector<std::string>{"q1"},"Quest failure remains once with no NPC fallthrough");
    need(port.begun==3&&port.ended==3,"actual Quest scope ends once on ordinary store failure");
    need(result.cause=="actual Toon Quest state Unknown original=original loop callback cause; no replay"&&count(t::Part::NpcStores)==npc_before,"Quest original cause and skipped NPC timer retained");
    port.failure="n0";port.calls.clear();result=run(port,before,target);
    need(!result.confirmed&&result.unknown&&result.attempted_stores==3&&result.stores==2&&result.cause=="actual NPC state Unknown original=original loop callback cause; no replay","NPC failure retains earlier stores and original cause");
    port.failure.clear();port.throws=true;port.calls.clear();const auto quest_before=count(t::Part::QuestStores);bool caught=false;
    try{run(port,before,target);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original loop callback exception";}
    need(caught&&port.calls==std::vector<std::string>{"q1"}&&count(t::Part::QuestStores)==quest_before+1,"exception keeps original cause and closes actual loop timer");
    need(port.begun==5&&port.ended==5,"actual Quest scope ends once on thrown store");
    port.throws=false;port.close_ok=false;port.calls.clear();result=run(port,before,target);
    need(!result.confirmed&&result.unknown&&result.stores==2&&result.attempted_stores==2&&port.calls==std::vector<std::string>{"q1","q2"},"failed explicit Quest close never reaches NPC and retains attempted stores");
    need(result.cause=="actual Toon Quest state Unknown original=original phase close failure; no replay","phase close cause remains visible");
    port.close_ok=true;port.begun=5;port.ended=5;
    port.throws=false;port.begin_ok=false;port.calls.clear();result=run(port,before,target);
    need(!result.confirmed&&!result.attempted&&!result.unknown&&port.calls.empty()&&port.begun==6&&port.ended==5,
        "failed Quest phase admission runs no stores or unentered destructor");
    need(result.cause=="actual Toon Quest phase admission Unknown original=original Quest phase admission cause; no replay",
        "failed Quest phase admission preserves original cause");
}
}
void scope_cases(){
    void* allocation=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    need(allocation!=nullptr,"scope attribution owned page");
    try{
        {
            t::Scope take(t::Part::Take);MEMORY_BASIC_INFORMATION info{};
            need(t::virtual_query(allocation,&info,sizeof info,t::Part::QueryPlacement)==sizeof info,"unscoped original query");
            need(count(t::Part::PlacementUnscopedOs)==1&&count(t::Part::PlacementScopedOs)==0,"unscoped OS query classification");
            {
                t::PageReadScope parent;
                need(t::virtual_query(allocation,&info,sizeof info,t::Part::QueryPlacement)==sizeof info,"scoped original query");
                need(t::virtual_query(allocation,&info,sizeof info,t::Part::QueryPlacement)==sizeof info,"scoped metadata hit");
                need(count(t::Part::PlacementScopedOs)==1,"cache hits are not OS samples");
                {t::PageReadScope child;need(count(t::Part::ReaderNestedReset)==1,"pure nested entry counted");}
                need(count(t::Part::ReaderNestedReset)==2,"pure nested exit counted");
                {t::PageReadScope child(t::PageBoundary::OriginalCall);need(count(t::Part::OriginalNestedReset)==1,"original nested entry counted");}
                need(count(t::Part::OriginalNestedReset)==2,"original nested exit counted");
                {t::PageReadScope disabled(false,t::PageBoundary::OriginalCall);}
                need(count(t::Part::OriginalNestedReset)==2&&count(t::Part::ReaderNestedReset)==2,"disabled helper does not count reset");
                need(t::virtual_query(allocation,&info,sizeof info,t::Part::QueryPlacement)==sizeof info&&count(t::Part::PlacementScopedOs)==2,"nested boundaries still invalidate parent metadata");
                DWORD old=0;
                {t::PageReadScope boundary(t::PageBoundary::OriginalCall);
                    need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&old)!=0,"original boundary protection change");}
                need(t::virtual_query(allocation,&info,sizeof info,t::Part::QueryPlacement)==sizeof info&&info.Protect==PAGE_NOACCESS,"tagged boundary keeps fresh guard failure metadata");
                SetLastError(731);MEMORY_BASIC_INFORMATION direct{};const auto direct_result=VirtualQuery(allocation,&direct,3);const auto direct_error=GetLastError();SetLastError(731);
                need(t::virtual_query(allocation,&info,3,t::Part::QueryPlacement)==direct_result&&GetLastError()==direct_error,"classified native failure keeps exact LastError");
            }
            const auto& all=t::totals[size_t(t::Part::QueryPlacement)];
            const auto& scoped=t::totals[size_t(t::Part::PlacementScopedOs)];const auto& unscoped=t::totals[size_t(t::Part::PlacementUnscopedOs)];
            need(all.count==scoped.count+unscoped.count&&all.total_ns==scoped.total_ns+unscoped.total_ns,"scope counts and exact same timer samples partition Placement");
            need(t::totals[size_t(t::Part::ReaderNestedReset)].total_ns==0&&t::totals[size_t(t::Part::OriginalNestedReset)].total_ns==0,"reset events are counts without invented timings");
            loop_probe::cases();
            const auto diagnostic=t::snapshot();for(const char* key:{"placement_scoped_os","placement_unscoped_os","reader_nested_reset","original_nested_reset","quest_stores","npc_stores"})need(diagnostic.contains(key),"existing status exports diagnostic entry");
            partition();
        }
        need(t::page_read_scope==nullptr&&t::take_depth==0,"all diagnostic scopes unwind");
    }catch(...){if(!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"owned diagnostic allocation release failed\n";throw;}
    need(VirtualFree(allocation,0,MEM_RELEASE)!=0,"owned diagnostic allocation release");
    t::totals={};
}
