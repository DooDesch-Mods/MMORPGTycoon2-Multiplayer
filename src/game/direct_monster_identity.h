#pragma once
#include <cstdint>
#include <map>
#include <string>
namespace mt2mp::game::direct_monster_identity {
struct Scope {
    std::string snapshot;
    uint64_t epoch=0,generation=0;
    uintptr_t root=0,region=0,manager=0;
    bool operator==(const Scope&)const=default;
};
struct Entry {Scope scope;uintptr_t pointer=0;uint64_t uid=0;bool confirmed=false,retired=false;};
class Registry {
    std::map<std::string,Entry> entries_;
public:
    bool stage(const std::string& action,const Scope& scope,uintptr_t pointer,uint64_t uid,std::string& error){
        bool uuid=action.size()==36;
        for(size_t i=0;uuid&&i<action.size();++i){const auto c=action[i];
            uuid=(i==8||i==13||i==18||i==23)?c=='-':((c>='0'&&c<='9')||(c>='a'&&c<='f'));}
        if(!uuid||scope.snapshot.empty()||!scope.epoch||!scope.generation||!scope.root||!scope.region||!scope.manager||!pointer||!uid||
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
    bool tombstone(const std::string& action,const Entry& expected)const{
        auto found=entries_.find(action);return found!=entries_.end()&&found->second.confirmed&&found->second.retired&&
            found->second.scope==expected.scope&&found->second.pointer==expected.pointer&&found->second.uid==expected.uid;
    }
    bool resolve(const Scope& scope,uintptr_t pointer,uint64_t uid,std::string& key,std::string& error,const std::string& pending={})const{
        key.clear();
        for(const auto& [id,v]:entries_)if(v.scope==scope&&v.pointer==pointer&&v.uid==uid&&
            !v.retired&&(v.confirmed||id==pending)){key="action:"+id;return true;}
        error="Direct Monster has no confirmed full-UID/current-world action binding; saved direct admission unsupported";return false;
    }
};
inline Registry registry;
}
