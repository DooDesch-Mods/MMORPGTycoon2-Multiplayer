#pragma once
#include "dungeon_scene_key.h"
#include <algorithm>
#include <map>
#include <set>
#include <string_view>

namespace mt2mp::game::dungeon_gizmo_identity {
inline bool uuid(std::string_view s){
    if(s.size()!=36)return false;
    for(size_t i=0;i<s.size();++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;}return true;
}
inline bool digest(std::string_view s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string_view::npos;}
inline bool take_number(std::string_view& s,std::string_view suffix){
    const auto end=s.find(suffix);if(end==std::string_view::npos||!snapshot_building_key::number(s.substr(0,end),INT32_MAX,false))return false;
    s.remove_prefix(end+suffix.size());return true;
}
// The path is the original saved root Building child, then the original template
// or saved Instance index and Gizmo child. It never contains a process UID.
inline std::string record(const std::string& sha,int32_t region,int32_t building,int32_t instance,int32_t child){
    return sha+":world:dungeon:gizmo:region{"+std::to_string(region)+"}/building["+std::to_string(building)+"]"+
        (instance<0?"/template/":"/instance["+std::to_string(instance)+"]/")+"gizmo["+std::to_string(child)+"]";
}
inline bool saved(std::string_view s){
    constexpr std::string_view prefix=":world:dungeon:gizmo:region{";
    if(s.size()<=64+prefix.size()||!digest(s.substr(0,64))||s.substr(64,prefix.size())!=prefix)return false;
    s.remove_prefix(64+prefix.size());if(!take_number(s,"}/building[")||!take_number(s,"]"))return false;
    if(s.starts_with("/template/gizmo["))s.remove_prefix(16);
    else if(s.starts_with("/instance[")){s.remove_prefix(10);if(!take_number(s,"]/gizmo["))return false;}
    else return false;
    return s.ends_with(']')&&snapshot_building_key::number(s.substr(0,s.size()-1),INT32_MAX,false);
}
inline std::string birth(const std::string& id,uint32_t ordinal){return "action:"+id+":dungeon:gizmo:"+std::to_string(ordinal);}
inline bool key(std::string_view s){
    if(saved(s))return true;
    constexpr std::string_view middle=":dungeon:gizmo:";
    return s.starts_with("action:")&&s.size()>43+middle.size()&&uuid(s.substr(7,36))&&s.substr(43,middle.size())==middle&&
        snapshot_building_key::number(s.substr(43+middle.size()),INT32_MAX,false);
}
struct Scope {std::string snapshot,sha;uint64_t epoch=0,generation=0;uintptr_t root=0;bool operator==(const Scope&)const=default;};
struct Member {std::string scene;uintptr_t object=0,manager=0,shard=0,region=0;uint64_t uid=0;bool operator==(const Member&)const=default;};
struct Saved {std::string record;Member member;};
struct Alias {std::string record,key;bool operator==(const Alias&)const=default;};
inline bool valid(const Scope& s){return uuid(s.snapshot)&&digest(s.sha)&&s.epoch&&s.generation&&s.root;}
inline bool scene(std::string_view s){
    if(s.ends_with("/template"))return dungeon_scene_key::building_key(std::string(s.substr(0,s.size()-9)));
    const auto split=s.rfind("/instance/");return split!=std::string_view::npos&&
        dungeon_scene_key::building_key(std::string(s.substr(0,split)))&&snapshot_building_key::number(s.substr(split+10),INT32_MAX,false);
}
inline bool valid(const Member& m){return scene(m.scene)&&m.object&&m.manager&&m.shard&&m.region&&m.uid;}
class Registry {
    Scope scope_;bool sealed_=false;std::map<std::string,Member> rows_;std::set<std::string> retired_;
    static bool fail(std::string& e,const char* cause){e=cause;return false;}
public:
    bool operator==(const Registry&)const=default;
    bool current(const Scope& s,std::string& e)const{return (sealed_&&s==scope_)||fail(e,"Dungeon Gizmo identity NativeOpen snapshot/epoch/generation/root differs");}
    bool seal(const Scope& s,const std::vector<Saved>& actual,const std::vector<Alias>* aliases,std::string& e,const std::set<std::string>& tombstones={}){
        if(!valid(s))return fail(e,"Dungeon Gizmo identity snapshot scope invalid");
        std::map<std::string,std::string> declared;std::set<std::string> alias_keys;
        if(aliases)for(const auto& a:*aliases)if(!saved(a.record)||a.record.substr(0,64)!=s.sha||!key(a.key)||
            !declared.emplace(a.record,a.key).second||!alias_keys.insert(a.key).second)return fail(e,"Dungeon Gizmo snapshot alias record/key invalid or duplicated");
        for(const auto& id:tombstones)if(!key(id)||alias_keys.contains(id))return fail(e,"Dungeon Gizmo snapshot retired key invalid or aliases a live member");
        std::map<std::string,Member> next;std::set<std::string> records;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
        for(const auto& row:actual){
            if(!saved(row.record)||row.record.substr(0,64)!=s.sha||!valid(row.member)||!records.insert(row.record).second||
               !pointers.insert(row.member.object).second||!uids.insert(row.member.uid).second)return fail(e,"Dungeon Gizmo native saved roster record/pointer/full UID invalid or duplicated");
            const auto found=declared.find(row.record);if(aliases&&found==declared.end())return fail(e,"Dungeon Gizmo companion lacks an actual saved native record");
            const auto& id=aliases?found->second:row.record;
            if(tombstones.contains(id)||!next.emplace(id,row.member).second)return fail(e,"Dungeon Gizmo saved canonical identity aliases another member or a retired key");
        }
        if(aliases&&declared.size()!=actual.size())return fail(e,"Dungeon Gizmo companion contains a foreign saved native record");
        scope_=s;rows_=std::move(next);retired_=tombstones;sealed_=true;e.clear();return true;
    }
    bool resolve(const Scope& s,uintptr_t object,uint64_t uid,std::string& out,std::string& e)const{
        out.clear();if(!current(s,e))return false;
        for(const auto& [id,m]:rows_)if(m.object==object&&m.uid==uid&&!retired_.contains(id)){out=id;e.clear();return true;}
        return fail(e,"Dungeon Gizmo has no confirmed current saved/birth identity");
    }
    bool lookup(const Scope& s,const std::string& id,Member& out,std::string& e)const{
        out={};if(!current(s,e))return false;const auto found=rows_.find(id);
        if(found==rows_.end()||retired_.contains(id))return fail(e,"Dungeon Gizmo key absent or terminally retired");out=found->second;e.clear();return true;
    }
    bool capture(const Scope& s,const std::string& next_sha,const std::vector<Saved>& actual,std::vector<Alias>& out,std::string& e)const{
        out.clear();if(!current(s,e))return false;if(!digest(next_sha))return fail(e,"Dungeon Gizmo capture native SHA invalid");
        std::vector<Alias> next;std::set<std::string> records,canonical;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
        for(const auto& row:actual){std::string id;Member current_member;
            if(!saved(row.record)||row.record.substr(0,64)!=next_sha||!records.insert(row.record).second||
               !pointers.insert(row.member.object).second||!uids.insert(row.member.uid).second)return fail(e,"Dungeon Gizmo capture native saved roster invalid or duplicated");
            if(!resolve(s,row.member.object,row.member.uid,id,e)||!lookup(s,id,current_member,e))return false;
            if(!valid(row.member)||current_member.object!=row.member.object||current_member.uid!=row.member.uid||
               current_member.manager!=row.member.manager||current_member.shard!=row.member.shard||current_member.region!=row.member.region||
               !canonical.insert(id).second)return fail(e,"Dungeon Gizmo capture current scene/owner identity differs");next.push_back({row.record,id});
        }
        size_t alive=0;for(const auto& [id,member]:rows_){(void)member;if(!retired_.contains(id))++alive;}
        if(next.size()!=alive)return fail(e,"Dungeon Gizmo capture omitted a live confirmed member");
        std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.record<b.record;});out=std::move(next);e.clear();return true;
    }
    bool append(const Scope& s,const std::string& id,const Member& m,std::string& e){
        if(!current(s,e))return false;if(!key(id)||!id.starts_with("action:")||!valid(m)||rows_.contains(id)||retired_.contains(id))return fail(e,"Dungeon Gizmo confirmed birth key/owner invalid or reused");
        for(const auto& [old,row]:rows_)if(!retired_.contains(old)&&(row.object==m.object||row.uid==m.uid))return fail(e,"Dungeon Gizmo confirmed birth aliases an existing native full UID/pointer");
        rows_.emplace(id,m);e.clear();return true;
    }
    bool verify_roster(const Scope& s,const std::vector<Member>& actual,std::string& e)const{
        if(!current(s,e))return false;std::set<std::string> seen;
        for(const auto& member:actual){std::string id;Member expected;
            if(!resolve(s,member.object,member.uid,id,e)||!lookup(s,id,expected,e))return false;
            if(member!=expected||!seen.insert(id).second)return fail(e,"Dungeon Gizmo current roster canonical owner duplicated or changed");}
        size_t alive=0;for(const auto& [id,member]:rows_){(void)member;if(!retired_.contains(id))++alive;}
        return actual.size()==alive||fail(e,"Dungeon Gizmo current roster omitted a live confirmed identity");
    }
    bool retire(const Scope& s,const std::string& id,const Member& actual,std::string& e){
        Member m;if(!lookup(s,id,m,e))return false;if(m!=actual)return fail(e,"Dungeon Gizmo retirement full UID/current owner differs");retired_.insert(id);e.clear();return true;
    }
    bool restore_retired(const Scope& s,const std::string& id,const Member& old,const Member& actual,std::string& e){
        if(!current(s,e))return false;const auto found=rows_.find(id);
        if(found==rows_.end()||!retired_.contains(id)||found->second!=old||!valid(actual)||actual.scene!=old.scene||
           actual.manager!=old.manager||actual.shard!=old.shard||actual.region!=old.region)
            return fail(e,"Dungeon Gizmo original restoration requires its exact retired canonical scene owner");
        for(const auto& [key,row]:rows_)if(key!=id&&(row.uid==actual.uid||(!retired_.contains(key)&&row.object==actual.object)))
            return fail(e,"Dungeon Gizmo restored native return aliases another confirmed identity");
        found->second=actual;retired_.erase(id);e.clear();return true;
    }
    bool tombstones(const Scope& s,std::set<std::string>& out,std::string& e)const{out.clear();if(!current(s,e))return false;out=retired_;e.clear();return true;}
    void invalidate(){scope_={};sealed_=false;rows_.clear();retired_.clear();}
};
}
