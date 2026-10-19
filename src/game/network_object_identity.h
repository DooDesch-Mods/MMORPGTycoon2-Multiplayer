#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::network_object_identity {
enum class Kind:uint8_t {Node,Pipe};
inline const char* name(Kind kind){return kind==Kind::Node?"node":"pipe";}
inline bool uuid(const std::string& s){if(s.size()!=36)return false;for(size_t n=0;n<s.size();++n){if(n==8||n==13||n==18||n==23){if(s[n]!='-')return false;}else if(!((s[n]>='0'&&s[n]<='9')||(s[n]>='a'&&s[n]<='f')))return false;}return true;}
inline bool sha(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool key(const std::string& s){
    std::string tail;if(s.starts_with("action:")&&s.size()>43&&uuid(s.substr(7,36)))tail=s.substr(43);
    else if(s.size()>64&&sha(s.substr(0,64)))tail=s.substr(64);else return false;
    if(tail==":network:node"||tail==":network:pipe")return s.starts_with("action:");
    const auto start=tail.starts_with(":world:network:node:")?20u:tail.starts_with(":world:network:pipe:")?20u:0u;
    if(!start||start>=tail.size()||tail[start]=='0')return false;uint64_t id=0;
    for(size_t n=start;n<tail.size();++n){if(tail[n]<'0'||tail[n]>'9')return false;id=id*10+uint64_t(tail[n]-'0');if(id>INT32_MAX)return false;}return id>0;
}
inline std::string saved(const std::string& digest,Kind kind,int32_t id){return digest+":world:network:"+name(kind)+":"+std::to_string(id);}
inline std::string birth(const std::string& id,Kind kind){return "action:"+id+":network:"+name(kind);}
inline bool matches(const std::string& id,Kind kind){return (kind==Kind::Node||kind==Kind::Pipe)&&key(id)&&id.find(std::string(":network:")+name(kind))!=std::string::npos;}
struct Scope {std::string native_origin;uint64_t epoch=0,generation=0;uintptr_t root=0;bool operator==(const Scope&)const=default;};
struct Entry {Kind kind=Kind::Node;uintptr_t pointer=0,region=0,manager=0;uint64_t uid=0;int32_t region_id=-1;bool retired=false;bool operator==(const Entry&)const=default;};
inline bool valid(const Scope& s){return uuid(s.native_origin)&&s.epoch&&s.generation&&s.root;}
inline bool valid(const Entry& e){return (e.kind==Kind::Node||e.kind==Kind::Pipe)&&e.pointer&&e.region&&e.manager&&e.uid&&e.region_id>=0;}
class Registry {
    Scope scope_;bool sealed_=false;std::map<std::string,Entry> rows_;std::set<std::string> retired_;
    static bool fail(std::string& e,const char* cause){e=cause;return false;}
public:
    bool current(const Scope& scope,std::string& e)const {return (sealed_&&scope==scope_)||fail(e,"Network identity current NativeOrigin/epoch/generation/root differs");}
    bool seal(const Scope& scope,const std::vector<std::pair<std::string,Entry>>& rows,const std::set<std::string>& tombstones,std::string& e){
        if(!valid(scope))return fail(e,"Network identity seal origin invalid");std::map<std::string,Entry> next;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
        for(const auto& [id,row]:rows){if(!matches(id,row.kind)||!valid(row)||row.retired||tombstones.contains(id)||!next.emplace(id,row).second||!pointers.insert(row.pointer).second||!uids.insert(row.uid).second)return fail(e,"Network identity seal key/UID/pointer duplicate, invalid or retired");}
        for(const auto& id:tombstones)if(!key(id))return fail(e,"Network identity tombstone key invalid");
        scope_=scope;rows_=std::move(next);retired_=tombstones;sealed_=true;e.clear();return true;
    }
    bool resolve(const Scope& scope,uintptr_t object,uint64_t uid,std::string& out,std::string& e)const{
        out.clear();if(!current(scope,e))return false;for(const auto& [id,row]:rows_)if(!row.retired&&row.pointer==object&&row.uid==uid){out=id;e.clear();return true;}return fail(e,"Network object has no confirmed canonical identity");
    }
    bool lookup(const Scope& scope,const std::string& id,Entry& out,std::string& e)const{
        out={};if(!current(scope,e))return false;const auto it=rows_.find(id);if(it==rows_.end()||it->second.retired||retired_.contains(id))return fail(e,"Network canonical key absent or terminally retired");out=it->second;e.clear();return true;
    }
    bool append(const Scope& scope,const std::string& id,const Entry& row,std::string& e){
        if(!current(scope,e))return false;if(!matches(id,row.kind)||!valid(row)||row.retired)return fail(e,"Network confirmed birth origin/key/owner invalid");
        if(rows_.contains(id)||retired_.contains(id))return fail(e,"Network birth key already used or retired");
        for(const auto& [old,entry]:rows_)if(!entry.retired&&(entry.pointer==row.pointer||entry.uid==row.uid))return fail(e,"Network birth aliases an existing full UID/pointer");
        rows_.emplace(id,row);e.clear();return true;
    }
    bool retire(const Scope& scope,const std::string& id,uintptr_t object,uint64_t uid,std::string& e){
        Entry actual;if(!lookup(scope,id,actual,e))return false;if(actual.pointer!=object||actual.uid!=uid)return fail(e,"Network retirement current pointer/full UID differs");
        rows_.at(id).retired=true;retired_.insert(id);e.clear();return true;
    }
    bool restore_history(const Scope& scope,const std::string& id,const Entry& actual,std::string& e){
        if(!current(scope,e))return false;const auto old=rows_.find(id);
        if(old==rows_.end()||!old->second.retired||!retired_.contains(id)||!valid(actual)||actual.retired||
           old->second.kind!=actual.kind||old->second.uid!=actual.uid||old->second.region!=actual.region||old->second.manager!=actual.manager||old->second.region_id!=actual.region_id)return fail(e,"Network original history restore lacks exact retired full UID/owner");
        for(const auto& [key,row]:rows_)if(key!=id&&!row.retired&&(row.pointer==actual.pointer||row.uid==actual.uid))return fail(e,"Network original history restore aliases a live object");
        old->second=actual;retired_.erase(id);e.clear();return true;
    }
    bool enumerate(const Scope& scope,std::vector<std::pair<std::string,Entry>>& out,std::set<std::string>& tombstones,std::string& e)const{
        out.clear();tombstones.clear();if(!current(scope,e))return false;for(const auto& row:rows_)if(!row.second.retired)out.push_back(row);tombstones=retired_;e.clear();return true;
    }
    void invalidate(){scope_={};sealed_=false;rows_.clear();retired_.clear();}
};
}
