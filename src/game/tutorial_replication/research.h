#pragma once
#include "startingpoint.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mt2mp::game::tutorial_replication::research {
struct Row {std::string key;startingpoint::Row value;bool operator==(const Row&)const=default;};
// Release AddFeature advances this process-local scan stamp even for an existing
// feature. Peer/state observations compare every semantic field; native call
// admission and before/after effects continue to use exact Row equality/plan.
inline bool same_state(const std::optional<Row>& a,const std::optional<Row>& b){
    if(!a||!b)return a==b;
    auto compared=*b;compared.value.release_counter=a->value.release_counter;
    return *a==compared;
}
enum class Plan {None,Project,Deploy,Create};
namespace detail {
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
inline int32_t number(const nlohmann::json& j){
    if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>uint64_t(INT32_MAX):j.get<int64_t>()<0||j.get<int64_t>()>INT32_MAX))
        throw std::runtime_error("Research integer outside0..INT32_MAX");
    return j.get<int32_t>();
}
}
using detail::fail;
using detail::number;
inline bool valid(const Row& row,std::string& e){
    const auto& r=row.value;
    if(row.key!="bridges")return fail(e,"Research key must be bridges");
    if(r.commit_bugs&&(!r.deployed||r.project_present||r.commit_bugs->total<0||r.commit_bugs->known<0||r.commit_bugs->known>r.commit_bugs->total))return fail(e,"Research native Commit receipt invalid");
    if(!std::isfinite(r.work)||r.work<0||r.lines<0||r.desired_workers<0||r.bugs_total<0||r.bugs_known<0||r.bugs_known>r.bugs_total||
       r.inventory_count<0||r.inventory_count>200||r.release_change_count<0||r.release_change_count>4096||
       r.release_feature_count<0||r.release_feature_count>r.release_change_count)
        return fail(e,"Research row numeric bounds invalid");
    if(r.custom||(r.project_present&&r.deployed)||((r.project_present||r.deployed)&&!r.purchased))
        return fail(e,"Research row requires native unowned record, purchased project or deployed record");
    if(!r.project_present&&!r.deployed&&(r.inventory_count||r.release_feature_count))
        return fail(e,"Research no-project baseline already has native deployment effects");
    if(r.deployed&&(r.work!=0||r.lines||r.desired_workers||r.bugs_total||r.bugs_known))
        return fail(e,"Research retired project counters differ from native Commit tail");
    e.clear();return true;
}
inline nlohmann::json encode(const Row& row){
    const auto& r=row.value;
    return {{"key",row.key},{"project_present",r.project_present},{"purchased",r.purchased},{"deployed",r.deployed},{"custom",r.custom},
        {"work",r.work},{"lines",r.lines},{"desired_workers",r.desired_workers},{"bugs_total",r.bugs_total},{"bugs_known",r.bugs_known},
        {"inventory_count",r.inventory_count},{"release_change_count",r.release_change_count},
        {"release_feature_count",r.release_feature_count},{"release_counter",r.release_counter},
        {"commit_bugs",r.commit_bugs?nlohmann::json{{"total",r.commit_bugs->total},{"known",r.commit_bugs->known}}:nlohmann::json(nullptr)}};
}
inline bool decode(const nlohmann::json& j,Row& out,std::string& e){
    out={};try{
        if(!j.is_object()||j.size()!=15)throw std::runtime_error("Research exact15 fields required");
        Row row;row.key=j.at("key").get<std::string>();auto& r=row.value;
        for(auto key:{"project_present","purchased","deployed","custom"})if(!j.at(key).is_boolean())throw std::runtime_error("Research exact bool required");
        if(!j.at("work").is_number_float())throw std::runtime_error("Research work exact float required");
        r.project_present=j.at("project_present").get<bool>();r.purchased=j.at("purchased").get<bool>();r.deployed=j.at("deployed").get<bool>();r.custom=j.at("custom").get<bool>();r.work=j.at("work").get<float>();
        r.lines=number(j.at("lines"));r.desired_workers=number(j.at("desired_workers"));r.bugs_total=number(j.at("bugs_total"));r.bugs_known=number(j.at("bugs_known"));
        r.inventory_count=number(j.at("inventory_count"));r.release_change_count=number(j.at("release_change_count"));r.release_feature_count=number(j.at("release_feature_count"));
        const auto& counter=j.at("release_counter");
        if(!counter.is_number_integer()||(counter.is_number_unsigned()?counter.get<uint64_t>()>UINT32_MAX:counter.get<int64_t>()<0||uint64_t(counter.get<int64_t>())>UINT32_MAX))throw std::runtime_error("Research release counter outside uint32");
        r.release_counter=counter.get<uint32_t>();
        const auto& bugs=j.at("commit_bugs");if(!bugs.is_null()){
            if(!bugs.is_object()||bugs.size()!=2)throw std::runtime_error("Research Commit receipt exact2 fields required");
            r.commit_bugs=startingpoint::CommitBugs{number(bugs.at("total")),number(bugs.at("known"))};
        }
        if(!valid(row,e))return false;out=std::move(row);return true;
    }catch(const std::exception& x){e=std::string("Research decode: ")+x.what();return false;}
}
inline bool plan(const std::optional<Row>& local,const std::optional<Row>& target,Plan& out,std::string& e){
    out=Plan::None;
    if(local&&!valid(*local,e))return false;if(target&&!valid(*target,e))return false;
    if(local==target){e.clear();return true;}
    if(!local||!target||local->key!=target->key)return fail(e,"Research adoption requires matching explicit records on both sides");
    const auto& a=local->value;const auto& b=target->value;
    if(!a.project_present&&!a.deployed){
        if(!b.project_present||!b.purchased||b.deployed)return fail(e,"Research creation requires Host purchased current project");
        if(b.inventory_count!=a.inventory_count||b.release_change_count!=a.release_change_count||b.release_feature_count!=a.release_feature_count||b.release_counter!=a.release_counter)
            return fail(e,"Research creation frame changed native deployment effects");
        out=Plan::Create;e.clear();return true;
    }
    if(!a.project_present||a.deployed||a.inventory_count||a.release_feature_count)return fail(e,"Research adoption requires current unowned project");
    if(b.project_present){
        if(b.inventory_count!=a.inventory_count||b.release_change_count!=a.release_change_count||b.release_feature_count!=a.release_feature_count||b.release_counter!=a.release_counter)
            return fail(e,"Research project frame changed native deployment effects");
        out=Plan::Project;
    }else{
        if(!b.deployed||!b.purchased)return fail(e,"Research Deploy requires retired deployed purchased record");
        if(!b.commit_bugs||b.commit_bugs->total!=a.bugs_total||b.commit_bugs->known!=a.bugs_known)return fail(e,"Research Deploy requires exact original Host Commit bug receipt");
        if(b.inventory_count!=1||b.release_feature_count!=1||a.release_change_count==4096||b.release_change_count!=a.release_change_count+1||
           a.release_counter==UINT32_MAX||b.release_counter!=a.release_counter+1)
            return fail(e,"Research deployment requires exact one native item/Release feature/counter increment");
        out=Plan::Deploy;
    }
    e.clear();return true;
}
}
