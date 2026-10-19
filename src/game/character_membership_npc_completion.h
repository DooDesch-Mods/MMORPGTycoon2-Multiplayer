#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::character_membership_npc_completion {
struct Member {uintptr_t pointer=0;uint64_t uid=0;int32_t type=-1;bool operator==(const Member&)const=default;};
struct Pool {uintptr_t region=0,manager=0;uint32_t next_tag=0;std::vector<Member> rows;bool operator==(const Pool&)const=default;};
struct Receipt {bool attempted=false,confirmed=false;uint32_t calls=0,returned=0;std::string cause;};
// NPC::Command("Destroy") sets +894. Native Manager::Update then removes only
// those rows with the same swap-last/deleting-D0 branch as original RemoveNPC.
// Complete that branch without an unrelated EntityUpdate or a synthetic inverse.
template<class Port>bool finish(Port& port,const std::vector<Pool>& before,const std::vector<Member>& targets,Receipt& out,std::string& error){
    out={};auto fail=[&](const std::string& why){error=out.cause=why;return false;};
    std::set<uint64_t> required,found;std::set<uintptr_t> pointers,managers,regions;
    for(const auto& row:targets)if(!row.pointer||!row.uid||row.type<0||row.type>2||!required.insert(row.uid).second||!pointers.insert(row.pointer).second)
        return fail("Character deferred NPC completion target UID/pointer/type is invalid or repeated");
    std::vector<Pool> current;
    for(const auto& pool:before){if(!pool.region||!pool.manager||!managers.insert(pool.manager).second||!regions.insert(pool.region).second)
            return fail("Character deferred NPC completion Region/manager witness repeats");
        Pool actual;if(!port.read(pool,actual,error))return fail(error.empty()?"Character deferred NPC current pool read failed without cause":error);
        if(actual!=pool)return fail("Character original TypeDestroy changed an unrelated complete NPC pool membership");
        int32_t type=-1;std::set<uint64_t> uids;std::set<uintptr_t> local;
        for(const auto& row:actual.rows){if(row.type<type||row.type>2||!row.pointer||!row.uid||!uids.insert(row.uid).second||!local.insert(row.pointer).second)
                return fail("Character deferred NPC native type array order/identity differs");type=row.type;
            const auto selected=std::find_if(targets.begin(),targets.end(),[&](const auto& value){return value.uid==row.uid;});
            if(selected==targets.end())continue;
            if(*selected!=row||!found.insert(row.uid).second)return fail("Character deferred NPC target belongs to another current pool/lifetime");
            bool marked=false;if(!port.marked(actual,row,marked,error))return fail(error.empty()?"Character deferred NPC pending flag read failed without cause":error);
            if(!marked)return fail("Character original Destroy did not mark its current NPC +894 before completion");
        }current.push_back(std::move(actual));
    }
    if(found!=required)return fail("Character deferred NPC completion omitted a pre-censused target");
    for(auto& pool:current){std::vector<Member> ordered;
        for(const auto& row:pool.rows)if(required.contains(row.uid))ordered.push_back(row);
        for(const auto& row:ordered){bool attempted=false;const bool returned=port.remove(pool,row,attempted,error);
            out.attempted=out.attempted||attempted;if(attempted)++out.calls;
            if(!returned)return fail(error.empty()?"Character original NPC Remove returned failure without cause":error);
            if(!attempted)return fail("Character original NPC Remove claimed a return without an attempted original call");++out.returned;
            const auto removed=std::find(pool.rows.begin(),pool.rows.end(),row);
            const auto last=std::find_if(pool.rows.rbegin(),pool.rows.rend(),[&](const auto& value){return value.type==row.type;});
            if(removed==pool.rows.end()||last==pool.rows.rend())return fail("Character deferred NPC retained type-array witness lost its removed member");
            const auto index=size_t(removed-pool.rows.begin()),tail=size_t(pool.rows.rend()-last-1);
            pool.rows[index]=pool.rows[tail];pool.rows.erase(pool.rows.begin()+ptrdiff_t(tail));
            Pool actual;if(!port.read(pool,actual,error))return fail(error.empty()?"Character original NPC removal readback failed without cause":error);
            if(actual!=pool)return fail("Character original NPC removal differs from its exact swap-last full pool readback");
            bool registered=true;if(!port.registered(row.uid,registered,error))return fail(error.empty()?"Character original NPC UID absence read failed without cause":error);
            if(registered)return fail("Character original NPC deleting destructor did not unregister its full UID");
        }
    }
    out.confirmed=true;error.clear();return true;
}
}
