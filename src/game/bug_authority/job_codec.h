#pragma once
#include "job_state.h"
#include "codec.h"
#include <limits>
#include <map>

namespace mt2mp::game::bug_authority {
namespace work_codec {
using Json=nlohmann::json;
inline bool fail(std::string& e,const char* cause){e=cause;return false;}
inline bool identity(const BugIdentity& id,std::string& e){
    Row row;row.parent_kind=id.parent_kind;row.parent_key=id.parent_key;row.birth=id.birth;row.value.variant=0;
    return valid(State{{row}},e);
}
inline bool birth(const Birth& value,std::string& e){
    BugIdentity id;id.parent_key="action:11111111-1111-4111-8111-111111111111";id.birth=value;
    if(!identity(id,e))return false;
    if(value.kind==BirthKind::Snapshot&&(!value.occurrence||value.occurrence>static_cast<uint64_t>(INT32_MAX)))return fail(e,"Bug Job Snapshot requires positive int32 serialized linkId");
    return true;
}
inline float scalar(const Json& j){
    if(!j.is_number())throw std::runtime_error("Bug Work numeric scalar required");
    const double n=j.get<double>();if(!std::isfinite(n)||std::abs(n)>(std::numeric_limits<float>::max)())throw std::runtime_error("Bug Work finite float required");
    return static_cast<float>(n);
}
inline bool counter(const JobCounter& c,std::string& e){
    if(c.value.history.size()>10||c.value.today<0||c.value.total<0||c.highest<0||c.day<0)return fail(e,"Bug Job counter domain/history bound invalid");
    for(auto n:c.value.history)if(n<0)return fail(e,"Bug Job counter negative history");
    return true;
}
inline Json counter_json(const JobCounter& c){
    auto out=Json::array({c.value.today,c.value.total,c.highest,c.day});for(auto n:c.value.history)out.push_back(n);return out;
}
inline JobCounter counter_read(const Json& j){
    if(!j.is_array()||j.size()<4||j.size()>14)throw std::runtime_error("Bug Job counter requires4..14 flat values");
    JobCounter c;c.value.today=codec_detail::i32(j[0]);c.value.total=codec_detail::i32(j[1]);c.highest=codec_detail::i32(j[2]);c.day=codec_detail::i32(j[3]);
    for(size_t i=4;i<j.size();++i)c.value.history.push_back(codec_detail::i32(j[i]));return c;
}
inline bool valid(const WorkState& s,std::string& e){
    using BirthKey=std::tuple<BirthKind,std::string,uint64_t,uint64_t>;
    std::set<BirthKey> jobs;std::set<std::pair<ParentKind,std::string>> parents;std::set<int32_t> assigned;
    std::map<BirthKey,std::pair<ParentKind,std::string>> bug_parents;
    if(!counter(s.created,e)||!counter(s.fixed,e))return false;
    for(const auto& j:s.jobs){
        const bool absent=j.type==0&&!j.valid&&j.developer_id>=0&&j.birth.kind==BirthKind::Snapshot&&
            j.bug.parent_kind==ParentKind::Building&&j.bug.parent_key.empty()&&j.bug.birth==Birth{};
        if((!absent&&!identity(j.bug,e))||!birth(j.birth,e))return false;
        if(!absent&&j.bug.birth.kind!=BirthKind::Snapshot){
            const auto& b=j.bug.birth;const BirthKey key{b.kind,b.source,b.sequence,b.occurrence};
            const auto parent=std::pair{j.bug.parent_kind,j.bug.parent_key};
            const auto [it,inserted]=bug_parents.emplace(key,parent);
            if(!inserted&&it->second!=parent)return fail(e,"Bug Work global birth has conflicting parents");
        }
        if(!jobs.emplace(j.birth.kind,j.birth.source,j.birth.sequence,j.birth.occurrence).second)return fail(e,"Bug Job occurrence duplicated");
        if(!((j.type==7&&j.valid)||(j.type==0&&!j.valid&&j.developer_id>=0))||j.developer_id< -1||j.developer_id>49||j.region_id< -1)
            return fail(e,"Bug Job type/valid/assignment/region invalid");
        if(j.developer_id>=0&&!assigned.insert(j.developer_id).second)return fail(e,"Bug Job Developer assignment duplicated");
        for(float n:j.position)if(!std::isfinite(n))return fail(e,"Bug Job position not finite");
    }
    for(const auto& p:s.parents){
        Row owner;owner.parent_kind=p.kind;owner.parent_key=p.key;
        if(!codec_detail::parent(owner)||!parents.emplace(p.kind,p.key).second||p.state<0||p.state>2||!std::isfinite(p.bandwidth)||p.bandwidth<0)return fail(e,"Bug parent visual owner/state/bandwidth invalid or duplicated");
        for(float n:p.tint)if(!std::isfinite(n))return fail(e,"Bug parent tint not finite");
        if(p.kind==ParentKind::Building&&(p.tint_override||p.tint!=std::array<float,4>{}))return fail(e,"Bug Building parent cannot carry NPC tint");
        if(p.kind==ParentKind::Npc&&(p.bandwidth!=0||p.user_set))return fail(e,"Bug NPC parent cannot carry Building bandwidth");
    }
    e.clear();return true;
}
inline void encode(const WorkState& s,Json& construction){
    std::string e;if(!valid(s,e))throw std::runtime_error(e);
    auto jobs=Json::array(),parents=Json::array();
    for(const auto& j:s.jobs){const auto& b=j.bug.birth;const auto& k=j.birth;
        jobs.push_back(Json::array({static_cast<int>(j.bug.parent_kind),j.bug.parent_key,static_cast<int>(b.kind),b.source,b.sequence,b.occurrence,
            static_cast<int>(k.kind),k.source,k.sequence,k.occurrence,j.type,j.valid,j.developer_id,j.region_id,j.position[0],j.position[1],j.position[2]}));}
    for(const auto& p:s.parents)parents.push_back(Json::array({static_cast<int>(p.kind),p.key,p.state,p.bandwidth,p.user_set,p.tint_override,p.tint[0],p.tint[1],p.tint[2],p.tint[3]}));
    construction["bug_jobs"]=std::move(jobs);construction["bug_parents"]=std::move(parents);
    construction["job_created"]=counter_json(s.created);construction["job_fixed"]=counter_json(s.fixed);
}
inline bool decode(const Json& c,WorkState& out,std::string& e){
    out={};try{
        WorkState s;const auto& jobs=c.at("bug_jobs");const auto& parents=c.at("bug_parents");
        if(!jobs.is_array()||!parents.is_array())throw std::runtime_error("Bug Work Job/parent arrays required");
        for(const auto& row:jobs){if(!row.is_array()||row.size()!=17)throw std::runtime_error("Bug Job requires exact17 fields");
            JobRow j;j.bug.parent_kind=static_cast<ParentKind>(codec_detail::i32(row[0]));j.bug.parent_key=row[1].get<std::string>();
            j.bug.birth={static_cast<BirthKind>(codec_detail::i32(row[2])),row[3].get<std::string>(),codec_detail::u64(row[4]),codec_detail::u64(row[5])};
            j.birth={static_cast<BirthKind>(codec_detail::i32(row[6])),row[7].get<std::string>(),codec_detail::u64(row[8]),codec_detail::u64(row[9])};
            j.type=codec_detail::i32(row[10]);j.valid=codec_detail::boolean(row[11]);j.developer_id=codec_detail::i32(row[12]);j.region_id=codec_detail::i32(row[13]);
            j.position={scalar(row[14]),scalar(row[15]),scalar(row[16])};s.jobs.push_back(std::move(j));}
        for(const auto& row:parents){if(!row.is_array()||row.size()!=10)throw std::runtime_error("Bug parent requires exact10 fields");
            ParentVisual p;p.kind=static_cast<ParentKind>(codec_detail::i32(row[0]));p.key=row[1].get<std::string>();p.state=codec_detail::i32(row[2]);
            p.bandwidth=scalar(row[3]);p.user_set=codec_detail::boolean(row[4]);p.tint_override=codec_detail::boolean(row[5]);
            p.tint={scalar(row[6]),scalar(row[7]),scalar(row[8]),scalar(row[9])};s.parents.push_back(std::move(p));}
        s.created=counter_read(c.at("job_created"));s.fixed=counter_read(c.at("job_fixed"));if(!valid(s,e))return false;
        out=std::move(s);e.clear();return true;
    }catch(const std::exception& x){e=std::string("Bug Work decode: ")+x.what();return false;}
}
inline bool publication(const WorkState& s,const std::string& snapshot,uint64_t revision,std::string& e){
    for(const auto& j:s.jobs)for(const auto* b:{&j.bug.birth,&j.birth})
        if(b->kind==BirthKind::Publication&&(b->source!=snapshot||b->sequence>revision))return fail(e,"Bug Work publication snapshot/revision differs");
    return true;
}
}
}
