#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::character_boss_retirement_read {
struct Member {
    uintptr_t pointer=0;
    uint64_t uid=0;
    bool operator==(const Member&)const=default;
};
struct Pool {
    uintptr_t root=0,region=0,manager=0;
    std::vector<Member> members;
    bool operator==(const Pool&)const=default;
};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class Read,class Find>
bool census(uintptr_t image,uintptr_t root,uintptr_t region,Read& read,Find& find,Pool& out,std::string& error){
    Pool value;value.root=root;value.region=region;
    uintptr_t ap=0,actual_root=0,actual_region=0,data=0;
    int32_t count=-1,capacity=-1;
    if(!root||!region)return fail(error,"Character Boss root or Region is absent");
    if(!read(region,0x70,actual_root,error)||!read(region,0x990,value.manager,error)||
       !read(value.manager,0,ap,error))return false;
    if(actual_root!=root||ap!=image+0x16fa780)return fail(error,"Character Boss Region/manager class differs");
    if(!read(value.manager,0x68,actual_region,error)||!read(value.manager+0x38,0,ap,error)||
       !read(value.manager,0x40,data,error)||!read(value.manager,0x48,count,error)||!read(value.manager,0x4c,capacity,error))return false;
    if(actual_region!=region||ap!=image+0x172dfa0||count<0||count>100000||capacity<count||
       (count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))return fail(error,"Character Boss complete manager ownership/span differs");
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;
    for(int32_t i=0;i<count;++i){Member row;uintptr_t found=0,notify=0;
        if(!read(data,size_t(i)*8,row.pointer,error)||!read(row.pointer,0,ap,error)||
           !read(row.pointer,0x18,row.uid,error)||!read(row.pointer,0x170,actual_root,error)||
           !read(row.pointer,0x178,actual_region,error)||!read(ap,0x438,notify,error)||!find(row.uid,found,error))return false;
        if(!row.pointer||!row.uid||ap!=image+0x1743040||notify!=image+0x3976f0||actual_root!=root||
           actual_region!=region||found!=row.pointer||!pointers.insert(row.pointer).second||!uids.insert(row.uid).second)
            return fail(error,"Character Boss member class/full UID/Notify/reciprocal owner differs");
        value.members.push_back(row);
    }
    uintptr_t again_data=0,again_manager=0;int32_t again_count=-1,again_capacity=-1;
    if(!read(region,0x990,again_manager,error)||!read(value.manager,0x40,again_data,error)||
       !read(value.manager,0x48,again_count,error)||!read(value.manager,0x4c,again_capacity,error))return false;
    if(again_manager!=value.manager||again_data!=data||again_count!=count||again_capacity!=capacity)
        return fail(error,"Character Boss manager changed during census");
    out=std::move(value);return true;
}
// Original RemoveBoss uses swap-last. Notification order belongs to the native
// Character collection; require the exact survivors without guessing that order.
inline bool removed(const Pool& before,const Pool& after,const std::vector<Member>& selected,std::string& error){
    if(before.root!=after.root||before.region!=after.region||before.manager!=after.manager)
        return fail(error,"Character Boss manager/Region ownership changed after original destruction");
    std::map<uint64_t,uintptr_t> expected;
    for(const auto& row:before.members)if(!row.uid||!row.pointer||!expected.emplace(row.uid,row.pointer).second)
        return fail(error,"Character Boss prepared census contains invalid/duplicate identity");
    std::set<uint64_t> ids;
    for(const auto& row:selected){const auto found=expected.find(row.uid);
        if(!ids.insert(row.uid).second||found==expected.end()||found->second!=row.pointer)
            return fail(error,"Character Boss selected destruction member is absent or duplicated");
        expected.erase(found);
    }
    if(after.members.size()!=expected.size())return fail(error,"Character Boss original destruction changed an unrelated member count");
    for(const auto& row:after.members){const auto found=expected.find(row.uid);
        if(found==expected.end()||found->second!=row.pointer)return fail(error,"Character Boss original destruction changed surviving full identity");
        expected.erase(found);
    }
    return expected.empty()||fail(error,"Character Boss original destruction omitted a surviving member");
}
}
