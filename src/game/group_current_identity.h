#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace mt2mp::game::group_current_identity {
struct Scope {
    std::string snapshot;uint64_t epoch=0,generation=0;uintptr_t root=0;
    bool operator==(const Scope&)const=default;
};
class Registry {
    Scope scope_;
    std::map<uint64_t,std::string> keys_;
public:
    bool bind(const Scope& scope,uint64_t uid,const std::string& derived,std::string& out,std::string& e){
        out.clear();
        if(scope.snapshot.empty()||!scope.epoch||!scope.generation||!scope.root||!uid){e="Group canonical current scope/full UID is incomplete";return false;}
        if(scope!=scope_){scope_=scope;keys_.clear();}
        const auto found=keys_.find(uid);if(found!=keys_.end()){out=found->second;return true;}
        if(!derived.starts_with("group:")||derived.size()<=6||derived.size()>4096){
            e="Group without a retained canonical full UID has no original member identity";return false;}
        for(const auto& [other,key]:keys_)if(other!=uid&&key==derived){e="Group canonical key is already retained by another full UID";return false;}
        keys_.emplace(uid,derived);out=derived;return true;
    }
    bool birth(const Scope& scope,uint64_t uid,const std::string& key,std::string& e){
        std::string actual;if(!bind(scope,uid,key,actual,e))return false;
        if(actual!=key){e="Group original birth full UID already has another canonical identity";return false;}return true;
    }
};
}
