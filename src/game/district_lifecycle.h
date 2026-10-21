#pragma once
#include "district_saved_identity.h"
#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace mt2mp::game::district_lifecycle {
namespace saved=district_saved_identity;
struct Member {
    std::string key;
    saved::Record native;
    bool operator==(const Member&)const=default;
};
struct Roster {
    uintptr_t manager=0;
    int32_t region=-1;
    std::vector<Member> members;
    bool operator==(const Roster&)const=default;
};
struct Change {
    Roster after;
    std::vector<std::string> born,retired,restored;
    bool operator==(const Change&)const=default;
};
inline bool uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){const auto c=value[i];
        if(i==8||i==13||i==18||i==23){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    }
    return true;
}
inline bool valid_key(const std::string& value){
    if(snapshot_district_key::valid(value))return true;
    constexpr std::string_view prefix="district:action:";
    if(!value.starts_with(prefix)||value.size()<prefix.size()+38||value[prefix.size()+36]!=':'||
       !uuid(value.substr(prefix.size(),36)))return false;
    const auto ordinal=value.substr(prefix.size()+37);
    if(ordinal.empty()||ordinal.size()>6||(ordinal.size()>1&&ordinal.front()=='0'))return false;
    size_t number=0;for(const auto c:ordinal){if(c<'0'||c>'9')return false;number=number*10+size_t(c-'0');}
    return number<saved::maximum_districts;
}
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
inline bool valid(const Roster& roster,std::string& error){
    if(!roster.manager||roster.region<0||roster.members.size()>saved::maximum_districts)
        return fail(error,"District lifecycle manager/Region/member bound invalid");
    std::set<uintptr_t> pointers;std::set<uint64_t> uids;std::set<std::string> keys;
    for(const auto& row:roster.members){
        if(!row.native.pointer||!row.native.uid||!valid_key(row.key)||!pointers.insert(row.native.pointer).second||
           !uids.insert(row.native.uid).second||!keys.insert(row.key).second)
            return fail(error,"District lifecycle full identity/key absent or repeated");
    }
    return true;
}
// Array slots may change when an original removes a different member. A current
// read must still account for every member and preserve the full local identity.
template<class Find>
bool current(const Roster& admitted,const saved::Pool& actual,Find&& find,
             std::vector<saved::Current>& out,std::string& error){
    if(!valid(admitted,error))return false;
    if(actual.manager!=admitted.manager||actual.records.size()!=admitted.members.size())
        return fail(error,"District current manager/count differs from confirmed lifecycle");
    std::map<uint64_t,const Member*> by_uid;
    for(const auto& row:admitted.members)by_uid.emplace(row.native.uid,&row);
    std::set<uint64_t> seen;std::set<uintptr_t> pointers;
    std::vector<saved::Current> result;
    for(const auto& row:actual.records){
        const auto known=by_uid.find(row.uid);uintptr_t pointer=0;
        if(known==by_uid.end()||known->second->native.pointer!=row.pointer||
           !seen.insert(row.uid).second||!pointers.insert(row.pointer).second)
            return fail(error,"District current member lacks confirmed full identity");
        if(!find(row.uid,pointer,error))return false;
        if(pointer!=row.pointer)return fail(error,"District current full UID lookup differs");
        result.push_back({known->second->key,admitted.region,row});
    }
    out=std::move(result);error.clear();return true;
}
template<class Find>
bool seed(const std::string& sha,const saved::SavedPool& snapshot,Find&& find,Roster& out,std::string& error){
    Roster next{snapshot.pool.manager,snapshot.region,{}};
    for(const auto& row:snapshot.pool.records){uintptr_t pointer=0;
        if(!find(row.uid,pointer,error))return false;
        if(pointer!=row.pointer)return fail(error,"District lifecycle saved full UID lookup differs");
        next.members.push_back({saved::key(sha,snapshot.region,row),row});
    }
    if(!valid(next,error))return false;out=std::move(next);error.clear();return true;
}
// Call only after the complete original graph and fresh owner/derived-state
// readback. Replacing a surviving allocation is legal only for an explicit
// original-history restoration with its retained full UID and canonical key.
template<class Find>
bool reconcile(const Roster& before,const saved::Pool& actual,const std::string& operation,
               const std::map<uint64_t,std::string>& restore,Find&& find,Change& out,std::string& error,size_t* next_ordinal=nullptr){
    if(!valid(before,error))return false;
    if(!uuid(operation)||actual.manager!=before.manager||actual.records.size()>saved::maximum_districts)
        return fail(error,"District lifecycle original operation/manager/member bound differs");
    Change next;next.after={before.manager,before.region,{}};
    std::set<uint64_t> live;std::set<uintptr_t> pointers;std::set<std::string> keys;
    size_t born=next_ordinal?*next_ordinal:0;
    for(const auto& row:actual.records){uintptr_t found=0;
        if(!row.pointer||!row.uid||!live.insert(row.uid).second||!pointers.insert(row.pointer).second)
            return fail(error,"District lifecycle actual full UID/pointer absent or repeated");
        if(!find(row.uid,found,error))return false;
        if(found!=row.pointer)return fail(error,"District lifecycle actual full UID is not its registered member");
        const auto prior=std::find_if(before.members.begin(),before.members.end(),[&](const auto& candidate){return candidate.native.uid==row.uid;});
        const auto restored=restore.find(row.uid);std::string key;
        if(prior!=before.members.end()){
            if(prior->native.pointer!=row.pointer||restored!=restore.end())
                return fail(error,"District lifecycle surviving full identity replaced or incorrectly restored");
            key=prior->key;
        }else if(restored!=restore.end()){
            if(!valid_key(restored->second))return fail(error,"District lifecycle restored canonical key invalid");
            key=restored->second;next.restored.push_back(key);
        }else{
            if(born>=saved::maximum_districts)return fail(error,"District lifecycle complete birth ordinal exceeds bound");
            key="district:action:"+operation+":"+std::to_string(born++);next.born.push_back(key);
        }
        if(!keys.insert(key).second)return fail(error,"District lifecycle original output repeats canonical key");
        next.after.members.push_back({std::move(key),row});
    }
    for(const auto& row:before.members)if(!live.contains(row.native.uid)){uintptr_t found=0;
        if(!find(row.native.uid,found,error))return false;
        if(found)return fail(error,"District lifecycle removed full UID remains registered");
        next.retired.push_back(row.key);
    }
    for(const auto& [uid,key]:restore){(void)key;if(!live.contains(uid))return fail(error,"District lifecycle original history omitted a restored full UID");}
    if(!valid(next.after,error))return false;
    if(next_ordinal)*next_ordinal=born;
    out=std::move(next);error.clear();return true;
}
}
