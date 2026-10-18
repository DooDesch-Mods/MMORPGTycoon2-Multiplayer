namespace npc_find_scope_test {
using Find=void*(*)(uint64_t);
inline unsigned calls=0;inline uint64_t seen=0;inline uint32_t lookup_seh_code=0;
void* find(uint64_t uid){seen=uid;return reinterpret_cast<void*>(uintptr_t(uid+16));}
uint32_t lookup(Find fn,uint64_t uid,uintptr_t& out){++calls;if(lookup_seh_code)return lookup_seh_code;out=reinterpret_cast<uintptr_t>(fn(uid));return 0;}
uint32_t find_call(Find fn,uint64_t uid,uintptr_t& out){return lookup(fn,uid,out);}
#include "npc_find_calls.inc"
}
void npc_find_scope_cases(){
    using namespace npc_find_scope_test;
    for(auto invoke:{&action_find,&saved_find}){
        for(bool historical:{false,true}){
            t::totals={};calls=0;seen=0;lookup_seh_code=0;
            t::Scope take(t::Part::Take);t::PageReadScope outer;
            int live_value=17;MEMORY_BASIC_INFORMATION information{};SetLastError(0);
            need(t::virtual_query(&live_value,&information,sizeof information,t::Part::QueryPlacement)==sizeof information,"Find parent first metadata query failed");
            uintptr_t result=0;
            if(historical){t::PageReadScope original(t::PageBoundary::OriginalCall);need(invoke(find,41,result)==0,"historical Find failed");}
            else need(invoke(find,41,result)==0,"pure Find failed");
            need(calls==1&&seen==41&&result==57,"actual Find call arguments/result/once changed");
            live_value=18;
            need(t::virtual_query(&live_value,&information,sizeof information,t::Part::QueryPlacement)==sizeof information&&live_value==18,"Find scope retained game values instead of metadata");
            need(count(t::Part::MemoryVirtualQuery)==uint64_t(historical?2:1),"pure Find failed to retain active parent metadata");
            need(count(t::Part::OriginalNestedReset)==uint64_t(historical?2:0),"Find reset count differs");
            lookup_seh_code=0xc0000005;result=99;
            need(invoke(find,42,result)==0xc0000005&&result==99&&calls==2,"original lookup SEH result changed");
        }
        t::totals={};lookup_seh_code=0;calls=0;
        {t::Scope take(t::Part::Take);uintptr_t result=0;need(invoke(find,43,result)==0&&result==59&&calls==1&&t::page_read_scope==nullptr,"unscoped Find changed caller lifetime");}
    }
    t::totals={};calls=0;seen=0;lookup_seh_code=0;
}
