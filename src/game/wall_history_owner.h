#pragma once
#include <map>
#include <set>
#include <string>
#include <cstdint>
#include <vector>

namespace mt2mp::game::wall_history_owner {
struct Receipt {uintptr_t root=0;std::set<std::string> keys;};
inline bool reject(std::string& error,const std::string& cause){error="Wall history shard owner: "+cause;return false;}
template<class Native>
bool capture(Native& native,uintptr_t root,const std::map<std::string,uintptr_t>& paths,Receipt& out,std::string& error){
    if(!root||paths.empty())return reject(error,"source root or born Wall keys absent");
    if(!native.verify(error))return false;
    Receipt value;value.root=root;std::set<uintptr_t> objects;
    for(const auto& [key,path]:paths){
        uintptr_t shard=0;
        if(key.empty()||!path||!objects.insert(path).second)return reject(error,"source Wall key or unique native object differs");
        if(!native.read(path,shard,error))return reject(error,"source owner read failed: "+error);
        if(shard!=root)return reject(error,"source Wall does not belong to the bound root shard");
        value.keys.insert(key);
    }
    out=std::move(value);return true;
}
template<class Native>
bool restore(Native& native,uintptr_t root,const std::map<std::string,uintptr_t>& paths,const Receipt& expected,std::string& error){
    if(!root||root!=expected.root||expected.keys.empty()||paths.size()!=expected.keys.size())
        return reject(error,"source root or complete restored Wall key count differs");
    if(!native.verify(error))return false;
    std::vector<uintptr_t> missing;std::set<uintptr_t> objects;
    for(const auto& [key,path]:paths){
        if(!expected.keys.contains(key)||!path||!objects.insert(path).second)
            return reject(error,"restored Wall key or unique native object differs");
        uintptr_t shard=0;if(!native.read(path,shard,error))return false;
        if(shard&&shard!=root)return reject(error,"restored Wall has a foreign shard owner");
        if(!shard)missing.push_back(path);
    }
    // Preflight every restored member before completing any omitted owner.
    for(uintptr_t path:missing){
        if(!native.set(path,root,error))return false;
        uintptr_t shard=0;if(!native.read(path,shard,error))return false;
        if(shard!=root)return reject(error,"original SetShard returned without exact owner readback");
    }
    return true;
}
}
