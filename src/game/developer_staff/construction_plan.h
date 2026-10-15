#pragma once
#include "construction_types.h"
#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::developer_staff::construction {
inline bool expand_pending(const std::vector<JobState>& actual,const std::vector<JobState>& pending,const std::vector<LocalJob>& links,
                           std::vector<JobState>& out,std::string& error){
    out.clear();std::set<std::string> keys,used;
    if(actual.size()!=links.size()){error="Construction local occurrence list differs";return false;}
    for(const auto& job:pending)if((!job.present&&(job.active||job.developer_id!=-1))||!keys.insert(job.building_key).second){
        error="Construction wire requires unique pending or restored-inactive occurrences";return false;}
    for(size_t i=0;i<actual.size();++i){const auto& local=actual[i];
        if(local.building_key!=links[i].building_key){out.clear();error="Construction local occurrence key order differs";return false;}
        const auto found=std::find_if(pending.begin(),pending.end(),[&](const auto& job){return job.building_key==local.building_key;});
        if(found==pending.end()){
            if(links[i].target_alive&&links[i].target_restored&&!local.active){
                out.clear();error="Construction restored inactive Building receipt is missing";return false;}
            out.push_back({local.building_key,false,links[i].target_alive,-1});
        }else {
            if(!found->present&&(!links[i].target_alive||!links[i].target_restored)){
                out.clear();error="Construction inactive absence has no confirmed live Redo target";return false;}
            used.insert(found->building_key);out.push_back(*found);}
    }
    if(used.size()!=pending.size()){
        out.clear();error="Construction frame names an occurrence absent from original ordered placement";return false;}
    return true;
}
struct Membership {int32_t id;bool outside;bool reorder=false;};
// Native Lounge transitions preserve retained order and append new members.
inline bool membership_plan(const std::vector<int32_t>& inside,const std::vector<int32_t>& outside,
                            const std::vector<int32_t>& target_inside,const std::vector<int32_t>& target_outside,
                            std::vector<Membership>& out,std::string& error){
    out.clear();std::set<int32_t> old_ids,new_ids;
    for(const auto* list:{&inside,&outside})for(auto id:*list)if(id<0||!old_ids.insert(id).second){error="Construction current manager IDs duplicate or invalid";return false;}
    for(const auto* list:{&target_inside,&target_outside})for(auto id:*list)if(id<0||!new_ids.insert(id).second){error="Construction target manager IDs duplicate or invalid";return false;}
    if(old_ids!=new_ids){error="Construction manager actor sets differ";return false;}
    const auto side=[&](const auto& before,const auto& target,bool is_outside){
        std::vector<int32_t> after;
        for(auto id:before)if(std::find(target.begin(),target.end(),id)!=target.end())after.push_back(id);
        for(auto id:target)if(std::find(before.begin(),before.end(),id)==before.end()){
            out.push_back({id,is_outside,false});after.push_back(id);}
        // A completed trip can be absent from intermediate frames. Preserve the
        // longest target prefix already ordered, then reappend only the rest.
        size_t keep=0;
        for(auto id:after)if(keep<target.size()&&id==target[keep])++keep;
        for(size_t i=keep;i<target.size();++i)out.push_back({target[i],is_outside,true});
    };
    side(outside,target_outside,true);side(inside,target_inside,false);
    return true;
}
}
