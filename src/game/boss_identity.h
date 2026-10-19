#pragma once
#include "snapshot_boss_key.h"
#include "character_boss_retirement_read.h"
#include <map>
#include <set>
#include <vector>
namespace mt2mp::game::boss_identity {
inline bool uuid(std::string_view value){if(value.size()!=36)return false;for(size_t i=0;i<value.size();++i){const auto c=value[i];if((i==8||i==13||i==18||i==23)?c!='-':!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}return true;}
inline bool valid_key(std::string_view key){if(snapshot_boss_key::valid(key))return true;
    constexpr std::string_view prefix="action:";if(!key.starts_with(prefix))return false;key.remove_prefix(prefix.size());
    if(key.size()<43||!uuid(key.substr(0,36))||key.substr(36,6)!=":boss:")return false;
    return snapshot_building_key::number(key.substr(42),INT32_MAX,false);
}
struct Scope {std::string snapshot,scene;uint64_t epoch=0,generation=0;uintptr_t root=0,shard=0,region=0,manager=0;bool operator==(const Scope&)const=default;};
struct Entry {Scope scope;uintptr_t pointer=0;uint64_t uid=0;bool confirmed=false,retired=false;bool operator==(const Entry&)const=default;};
inline bool valid_world(const Scope& scope){return uuid(scope.snapshot)&&scope.epoch&&scope.generation&&scope.root;}
inline bool valid(const Scope& scope){return valid_world(scope)&&!scope.scene.empty()&&scope.shard&&scope.region&&scope.manager;}
inline bool same_world(const Scope& a,const Scope& b){return a.snapshot==b.snapshot&&a.epoch==b.epoch&&a.generation==b.generation&&a.root==b.root;}
class Registry {
    std::map<std::string,Entry> entries_;
public:
    bool restore(const Scope& world,const std::vector<std::pair<std::string,Entry>>& rows,std::string& error){
        if(!valid_world(world)||rows.size()>4096){error="Boss restore requires bounded actual current world";return false;}
        std::map<std::string,Entry> next;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
        for(const auto& [key,row]:rows)if(!valid_key(key)||!valid(row.scope)||!same_world(world,row.scope)||!row.pointer||!row.uid||!row.confirmed||row.retired||!pointers.insert(row.pointer).second||!uids.insert(row.uid).second||!next.emplace(key,row).second){error="Boss restore repeats or lacks canonical current full-UID ownership";return false;}
        for(const auto& [key,row]:entries_)if(same_world(world,row.scope)){
            if(!row.confirmed){error="Boss restore cannot replace an unconfirmed original birth";return false;}
            const auto found=next.find(key);if(row.retired){if(found!=next.end()){error="Boss restore cannot revive a current tombstone";return false;}next.emplace(key,row);}
            else if(found==next.end()||found->second!=row){error="Boss restore changes a confirmed current binding";return false;}}
        if(next.size()>4096){error="Boss live/tombstone identity budget exceeded";return false;}entries_.swap(next);error.clear();return true;
    }
    bool stage(const std::string& key,const Entry& row,std::string& error){
        if(!valid_key(key)||!key.starts_with("action:")||!valid(row.scope)||!row.pointer||!row.uid||row.confirmed||row.retired||entries_.size()>=4096||entries_.contains(key)){error="Boss original birth key/owner invalid, reused or exhausted";return false;}
        for(const auto& [id,current]:entries_)if(same_world(current.scope,row.scope)&&((!current.retired&&current.pointer==row.pointer)||current.uid==row.uid)){error="Boss original birth aliases an existing native pointer/full UID";return false;}
        entries_.emplace(key,row);error.clear();return true;
    }
    bool confirm(const std::string& key,const Entry& expected,std::string& error){const auto found=entries_.find(key);if(found==entries_.end()||found->second!=expected||expected.confirmed||expected.retired){error="Boss birth confirmation lacks its exact unconfirmed original receipt";return false;}found->second.confirmed=true;error.clear();return true;}
    bool lookup(const std::string& key,const Scope& current,Entry& out,std::string& error)const{out={};const auto found=entries_.find(key);if(found==entries_.end()||found->second.scope!=current||!found->second.confirmed||found->second.retired){error="Boss canonical key is absent, pending, retired or outside actual scene";return false;}out=found->second;error.clear();return true;}
    bool identify(const Scope& current,uintptr_t object,uint64_t uid,std::string& key,std::string& error)const{key.clear();if(!valid(current)||!object||!uid){error="Boss identify requires current scene/object/full UID";return false;}for(const auto& [candidate,row]:entries_)if(row.scope==current&&row.pointer==object&&row.uid==uid){if(!row.confirmed||row.retired){error="Boss original identity is pending or retired";return false;}key=candidate;error.clear();return true;}error="Boss has no confirmed saved/original-birth canonical identity";return false;}
    bool retire(const std::string& key,const Entry& expected,std::string& error){const auto found=entries_.find(key);if(found==entries_.end()||found->second!=expected||!expected.confirmed||expected.retired){error="Boss retirement requires its exact confirmed live original binding";return false;}found->second.retired=true;error.clear();return true;}
    bool enumerate(const Scope& world,std::vector<std::pair<std::string,Entry>>& out,std::string& error)const{out.clear();if(!valid_world(world)){error="Boss enumeration requires actual current world";return false;}for(const auto& row:entries_)if(same_world(row.second.scope,world)){if(!row.second.confirmed){out.clear();error="Boss current world contains an unconfirmed original birth";return false;}out.push_back(row);}error.clear();return true;}
    template<class Find>bool retire_removed(const Scope& current,const character_boss_retirement_read::Pool& before,const character_boss_retirement_read::Pool& after,const std::vector<character_boss_retirement_read::Member>& selected,Find& find,std::string& error){
        if(!valid(current)||before.root!=current.shard||before.region!=current.region||before.manager!=current.manager){error="Boss retirement original census differs from its canonical current scene";return false;}
        if(!character_boss_retirement_read::removed(before,after,selected,error))return false;
        Registry next=*this;
        for(const auto& row:selected){std::string key;uintptr_t registered=0;
            if(!identify(current,row.pointer,row.uid,key,error)||!find(row.uid,registered,error))return false;
            if(registered){error="Boss original destruction has not cleared its selected full UID registration";return false;}
            const auto entry=next.entries_.find(key);if(entry==next.entries_.end()||!next.retire(key,entry->second,error))return false;}
        entries_.swap(next.entries_);error.clear();return true;
    }
    bool capture_key(const Scope& current,uintptr_t object,uint64_t uid,const std::string& saved,std::string& out,std::string& error)const{
        const bool first_capture=!current.epoch&&!current.generation&&uuid(current.snapshot)&&!current.scene.empty()&&current.root&&current.shard&&current.region&&current.manager;
        if(!snapshot_boss_key::valid(saved)||(!valid(current)&&!first_capture)||!object||!uid){error="Boss saved capture key/current native owner invalid";return false;}
        for(const auto& [key,row]:entries_)if(row.scope==current&&(row.pointer==object||row.uid==uid)){if(row.pointer!=object||row.uid!=uid||!row.confirmed||row.retired){error="Boss saved capture changed a current native binding";return false;}out=key;error.clear();return true;}out=saved;error.clear();return true;
    }
};
inline Registry registry;
using Member=character_boss_retirement_read::Member;
inline bool born(const character_boss_retirement_read::Pool& before,const character_boss_retirement_read::Pool& after,uintptr_t returned,Member& out,std::string& error){
    out={};if(!returned||before.root!=after.root||before.region!=after.region||before.manager!=after.manager||after.members.size()!=before.members.size()+1){error="Boss original birth changed owning scene or did not append exactly one member";return false;}
    for(size_t i=0;i<before.members.size();++i)if(before.members[i]!=after.members[i]){error="Boss original birth changed an existing complete manager member";return false;}
    const auto added=after.members.back();if(!added.uid||added.pointer!=returned){error="Boss original birth appended member differs from its actual original return/full UID";return false;}
    for(const auto& old:before.members)if(old.pointer==added.pointer||old.uid==added.uid){error="Boss original birth reused an existing live object/full UID";return false;}
    out=added;error.clear();return true;
}
// The scope comes from fresh NativeOpen root and Dungeon scene owners. Array
// positions are membership witnesses only and never become canonical keys.
template<class Read,class Find>bool read_members(Read& read,Find& find,uintptr_t image,const Scope& scope,std::vector<Member>& out,std::string& error){
    out.clear();if(!valid(scope)){error="Boss current native scene scope invalid";return false;}
    character_boss_retirement_read::Pool current;
    if(!character_boss_retirement_read::census(image,scope.shard,scope.region,read,find,current,error))return false;
    if(current.manager!=scope.manager){error="Boss canonical scene manager differs from actual complete census";return false;}
    out=std::move(current.members);error.clear();return true;
}
}
