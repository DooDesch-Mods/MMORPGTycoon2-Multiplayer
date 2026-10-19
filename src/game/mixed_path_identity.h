#pragma once
#include "mixed_path_graph.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::mixed_path_identity {
using Census=mixed_path_native::Census;
using Member=mixed_path_native::Member;
using Ref=mixed_path_native::Ref;
struct Birth {std::string key;uint64_t local_uid=0;int32_t region=-1,type=-1;bool path=false;};
inline bool reject(std::string& error,const std::string& cause){error="mixed Path identity: "+cause;return false;}
inline bool uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){const char c=value[i];
        if(i==8||i==13||i==18||i==23){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
    return true;
}
inline std::string initial(const std::string& snapshot,bool path,const Member& value){
    return "snapshot:"+snapshot+(path?":path:":":node:")+std::to_string(value.ref.region)+":"+
           std::to_string(value.ref.type)+":"+std::to_string(value.ref.index);
}
inline std::string born(const std::string& action,bool path,int32_t type,size_t ordinal){
    return "action:"+action+(path?":path:":":node:")+std::to_string(type)+":"+std::to_string(ordinal);
}
class Registry {
    std::string snapshot_;
    std::map<uint64_t,std::string> keys_;
    std::set<std::string> used_;
public:
    const std::map<uint64_t,std::string>& keys()const{return keys_;}
    bool seed(const std::string& snapshot,const Census& census,std::string& error){
        if(!snapshot_.empty()||!uuid(snapshot))return reject(error,"initial snapshot UUID absent or already seeded");
        std::map<uint64_t,std::string> keys;std::set<std::string> used;
        auto add=[&](const Member& member,bool path){
            if(!member.uid||member.ref.region<0||member.ref.type<0||member.ref.index<0)return false;
            auto key=initial(snapshot,path,member);
            return used.insert(key).second&&keys.emplace(member.uid,std::move(key)).second;};
        for(const auto& row:census.paths)if(!add(row.member,true))return reject(error,"initial Path full UID or bucket reference differs");
        for(const auto& row:census.nodes)if(!add(row.member,false))return reject(error,"initial Intersection full UID or bucket reference differs");
        snapshot_=snapshot;keys_=std::move(keys);used_=std::move(used);return true;
    }
    // Commit only after the native action and the complete after-census are
    // confirmed. Retired UIDs stay recorded for original Undo/Redo rebinding.
    bool births(const std::string& action,const Census& before,const Census& after,
                std::vector<Birth>& out,std::string& error){
        if(snapshot_.empty()||!uuid(action))return reject(error,"birth action lacks seeded World or UUID");
        std::set<uint64_t> prior,current;
        for(const auto& row:before.paths)prior.insert(row.member.uid);
        for(const auto& row:before.nodes)prior.insert(row.member.uid);
        std::map<uint64_t,std::string> next=keys_;std::set<std::string> used=used_;
        std::map<std::pair<bool,int32_t>,size_t> ordinals;
        std::vector<Birth> created;
        auto add=[&](const Member& member,bool path){
            if(!member.uid||!current.insert(member.uid).second)return false;
            if(prior.contains(member.uid))return next.contains(member.uid);
            const auto key=born(action,path,member.ref.type,ordinals[{path,member.ref.type}]++);
            if(!used.insert(key).second||!next.emplace(member.uid,key).second)return false;
            created.push_back({key,member.uid,member.ref.region,member.ref.type,path});return true;};
        for(const auto& row:after.paths)if(!add(row.member,true))return reject(error,"new Path UID/key or retained identity differs");
        for(const auto& row:after.nodes)if(!add(row.member,false))return reject(error,"new Intersection UID/key or retained identity differs");
        if(created.empty())return reject(error,"original Wall/Pass produced no native Path or Intersection birth");
        keys_=std::move(next);used_=std::move(used);out=std::move(created);return true;
    }
    // Original Undo/Redo can allocate a new full UID for an existing canonical
    // birth. Bind only expected Region/type/bucket positions; the caller then
    // compares the complete topology and original Records before confirmation.
    bool restore(const mixed_path_graph::Snapshot& expected,const Census& current,std::string& error){
        if(snapshot_.empty())return reject(error,"history restore has no seeded World");
        std::map<std::pair<bool,Ref>,std::string> slots;
        for(const auto& row:expected.paths)
            if(!slots.emplace(std::make_pair(true,row.local_ref),row.key).second)
                return reject(error,"expected Path history bucket repeated");
        for(const auto& row:expected.nodes)
            if(!slots.emplace(std::make_pair(false,row.local_ref),row.key).second)
                return reject(error,"expected Intersection history bucket repeated");
        std::map<uint64_t,std::string> next=keys_;
        std::set<std::string> live;
        auto bind=[&](const Member& member,bool path){
            const auto found=slots.find(std::make_pair(path,member.ref));
            if(found==slots.end()||found->second.empty())return false;
            const auto existing=next.find(member.uid);
            if(existing!=next.end()&&existing->second!=found->second)return false;
            if(existing==next.end())next.emplace(member.uid,found->second);
            return live.insert(found->second).second;
        };
        for(const auto& row:current.paths)if(!bind(row.member,true))
            return reject(error,"original Path history UID/bucket/key cannot rebind");
        for(const auto& row:current.nodes)if(!bind(row.member,false))
            return reject(error,"original Intersection history UID/bucket/key cannot rebind");
        if(live.size()!=slots.size())return reject(error,"original history restored incomplete topology");
        keys_=std::move(next);return true;
    }
};
}
