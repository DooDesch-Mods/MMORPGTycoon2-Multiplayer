#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <utility>
#include <set>
#include "monster_target_key.h"
#include "dungeon_gizmo_identity.h"
namespace mt2mp::game::direct_monster_identity {
struct Scope {
    std::string snapshot;
    uint64_t epoch=0,generation=0;
    uintptr_t root=0,region=0,manager=0;
    std::string scene="world";
    bool operator==(const Scope&)const=default;
};
inline bool valid_scene(const Scope& scope){return scope.scene=="world"||dungeon_gizmo_identity::scene(scope.scene);}
inline bool root_owned(const Scope& scope){return scope.scene=="world";}
struct Entry {Scope scope;uintptr_t pointer=0;uint64_t uid=0;bool confirmed=false,retired=false;bool operator==(const Entry&)const=default;};
class Registry {
    std::map<std::string,Entry> entries_;
public:
    bool operator==(const Registry&)const=default;
    bool capture_key(const Scope& scope,uintptr_t pointer,uint64_t uid,const std::string& saved,std::string& key,std::string& error)const{
        key.clear();if(!pointer||!uid||!snapshot_monster_key::valid(saved)){error="Monster capture local identity/saved key invalid";return false;}
        for(const auto& [id,v]:entries_)if(v.scope==scope&&(v.pointer==pointer||v.uid==uid)){
            if(!v.confirmed||v.retired||v.pointer!=pointer||v.uid!=uid){error="Monster capture pending/retired/replaced current identity";return false;}
            key=monster_target_key::canonical(id);error.clear();return true;
        }
        key=saved;error.clear();return true;
    }
    // Restoration receives only freshly resolved local entries. The complete
    // original-record census and companion comparison precede this commit.
    bool restore(const Scope& world,const std::vector<std::pair<std::string,Entry>>& rows,std::string& error){
        if(world.snapshot.empty()||!world.epoch||!world.generation||!world.root||rows.size()>4096){
            error="Monster restoration requires bounded completed current world";return false;}
        std::map<std::string,Entry> ready;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
        const auto current=[&](const Scope& s){return s.snapshot==world.snapshot&&s.epoch==world.epoch&&s.generation==world.generation&&s.root==world.root;};
        for(const auto& [key,value]:rows){
            const auto id=monster_target_key::argument(key);
            if(!monster_target_key::valid(key)||!current(value.scope)||!valid_scene(value.scope)||!value.scope.region||!value.scope.manager||
               !value.pointer||!value.uid||!value.confirmed||value.retired||!pointers.insert(value.pointer).second||
               !uids.insert(value.uid).second||!ready.emplace(id,value).second){
                error="Monster restored key/full UID/local owner duplicate or invalid";return false;}
        }
        for(const auto& [id,old]:entries_)if(current(old.scope)){
            if(!old.confirmed){error="Monster restore cannot replace a pending current birth";return false;}
            const auto found=ready.find(id);
            if(old.retired){
                if(found!=ready.end()){error="Monster restore cannot revive a current-world tombstone";return false;}
                ready.emplace(id,old);
            }else if(found==ready.end()||found->second!=old){error="Monster restore changes a current confirmed binding";return false;}
        }
        if(ready.size()>4096){error="Monster restored live/tombstone budget exceeded";return false;}
        entries_.swap(ready);error.clear();return true;
    }
    bool stage(const std::string& action,const Scope& scope,uintptr_t pointer,uint64_t uid,std::string& error){
        bool uuid=action.size()==36;
        for(size_t i=0;uuid&&i<action.size();++i){const auto c=action[i];
            uuid=(i==8||i==13||i==18||i==23)?c=='-':((c>='0'&&c<='9')||(c>='a'&&c<='f'));}
        if(!uuid||!valid_scene(scope)||scope.snapshot.empty()||!scope.epoch||!scope.generation||!scope.root||!scope.region||!scope.manager||!pointer||!uid||
           entries_.size()>=4096||entries_.contains(action)){error="Monster action identity invalid, reused or exhausted";return false;}
        for(const auto& [id,v]:entries_)if(v.scope==scope&&((!v.retired&&v.pointer==pointer)||v.uid==uid)){
            error="Monster action identity reuses a current pointer/full UID";return false;}
        entries_.emplace(action,Entry{scope,pointer,uid,false});return true;
    }
    bool confirm(const std::string& action,std::string& error){
        const auto found=entries_.find(action);if(found==entries_.end()||found->second.confirmed){error="Monster identity confirmation is absent/replayed";return false;}
        found->second.confirmed=true;return true;
    }
    bool lookup(const std::string& action,const Scope& scope,Entry& out,std::string& error)const{
        out={};const auto found=entries_.find(action);
        if(found==entries_.end()||!found->second.confirmed||found->second.retired||found->second.scope!=scope){
            error="Monster birth identity is absent, pending, retired or outside current world/manager";return false;}
        out=found->second;return true;
    }
    bool retire(const std::string& action,const Entry& expected,std::string& error){
        auto found=entries_.find(action);
        if(found==entries_.end()||!found->second.confirmed||found->second.retired||
           found->second.scope!=expected.scope||found->second.pointer!=expected.pointer||found->second.uid!=expected.uid){
            error="Monster birth retirement requires exact live confirmed identity";return false;}
        found->second.retired=true;return true;
    }
    bool restore_retired(const std::string& action,const Entry& expected,const Entry& actual,std::string& error){
        const auto found=entries_.find(action);
        if(found==entries_.end()||!found->second.confirmed||!found->second.retired||found->second.scope!=expected.scope||
           found->second.pointer!=expected.pointer||found->second.uid!=expected.uid||actual.scope!=expected.scope||
           actual.uid!=expected.uid||!actual.pointer||!actual.confirmed||actual.retired){
            error="Monster original restoration requires its exact retired full UID and scene owner";return false;}
        for(const auto& [key,row]:entries_)if(key!=action&&!row.retired&&(row.uid==actual.uid||row.pointer==actual.pointer)){
            error="Monster original restored full UID or pointer aliases another current binding";return false;}
        found->second=actual;error.clear();return true;
    }
    bool restore_scene(const std::string& action,const Entry& expected,const Entry& actual,
                       const std::string& exterior,std::string& error){
        const auto found=entries_.find(action);const auto& old=expected.scope;const auto& fresh=actual.scope;
        const bool belongs=old.scene==exterior+"/template"||old.scene.starts_with(exterior+"/instance/");
        if(!dungeon_scene_key::building_key(exterior)||!belongs||found==entries_.end()||!found->second.retired||
           !found->second.confirmed||found->second.scope!=old||found->second.pointer!=expected.pointer||found->second.uid!=expected.uid||
           !valid_scene(fresh)||fresh.scene!=old.scene||fresh.snapshot!=old.snapshot||fresh.epoch!=old.epoch||
           fresh.generation!=old.generation||fresh.root!=old.root||!fresh.region||!fresh.manager||!actual.pointer||
           !actual.uid||!actual.confirmed||actual.retired){
            error="Monster scene restoration requires exact retired identity and original exterior";return false;}
        for(const auto& [key,row]:entries_)if(key!=action&&(row.uid==actual.uid||(!row.retired&&row.pointer==actual.pointer))){
            error="Monster restored scene member aliases another native identity";return false;}
        found->second=actual;error.clear();return true;
    }
    bool tombstone(const std::string& action,const Entry& expected)const{
        auto found=entries_.find(action);return found!=entries_.end()&&found->second.confirmed&&found->second.retired&&
            found->second.scope==expected.scope&&found->second.pointer==expected.pointer&&found->second.uid==expected.uid;
    }
    bool enumerate(const std::string& snapshot,uint64_t epoch,uint64_t generation,uintptr_t root,
                   std::vector<std::pair<std::string,Entry>>& out,std::string& error)const{
        out.clear();if(snapshot.empty()||!epoch||!generation||!root){error="Monster identity enumeration requires exact current world";return false;}
        for(const auto& [id,entry]:entries_)if(entry.scope.snapshot==snapshot&&entry.scope.epoch==epoch&&entry.scope.generation==generation&&entry.scope.root==root){
            if(!entry.confirmed){out.clear();error="Monster live identity contains an unconfirmed birth";return false;}out.emplace_back(id,entry);}
        error.clear();return true;
    }
    bool resolve(const Scope& scope,uintptr_t pointer,uint64_t uid,std::string& key,std::string& error,const std::string& pending={})const{
        key.clear();
        for(const auto& [id,v]:entries_)if(v.scope==scope&&v.pointer==pointer&&v.uid==uid&&
            !v.retired&&(v.confirmed||id==pending)){key=monster_target_key::canonical(id);return true;}
        error="Direct Monster has no confirmed full-UID/current-world canonical binding";return false;
    }
};
inline Registry registry;
}
