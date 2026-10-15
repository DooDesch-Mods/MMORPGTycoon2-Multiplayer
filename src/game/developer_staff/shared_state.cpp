#include "shared_state.h"
#include <cmath>
#include <set>
#include <limits>
#include <regex>
#include <algorithm>
namespace mt2mp::game::developer_staff {
namespace {
using json=nlohmann::json;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
int32_t integer(const json& j){
    if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>uint64_t(INT32_MAX):j.get<int64_t>()<INT32_MIN||j.get<int64_t>()>INT32_MAX))throw std::runtime_error("Developer pool requires exact int32");
    return j.get<int32_t>();
}
bool boolean(const json& j){if(!j.is_boolean())throw std::runtime_error("Developer pool requires exact bool");return j.get<bool>();}
float scalar(const json& j){if(!j.is_number())throw std::runtime_error("Developer pool requires numeric position");const auto n=j.get<double>();if(!std::isfinite(n)||std::abs(n)>double(std::numeric_limits<float>::max()))throw std::runtime_error("Developer pool position outside finite float domain");return float(n);}
}
bool valid_construction(const PoolState& s,std::string& e){
    if(!s.construction)return true;
    const auto& c=*s.construction;
    if(c.actors.size()!=50)return fail(e,"Developer construction requires50 actor poses");
    for(size_t i=0;i<50;++i){const auto& p=c.actors[i];
        if(p.id!=s.rows[i].id||p.region_id< -1)return fail(e,"Developer construction Actor ID/order/region differs");
        float norm=0;for(float v:p.orientation){if(!std::isfinite(v))return fail(e,"Developer construction orientation is not finite");norm+=v*v;}
        if(norm<0.99f||norm>1.01f)return fail(e,"Developer construction orientation is not normalized");
        for(float v:p.scale)if(!std::isfinite(v)||v<=0)return fail(e,"Developer construction scale must be finite positive");
    }
    static const std::regex building_key("^action:[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    std::set<std::string> keys;std::set<int32_t> assigned;
    for(const auto& job:c.jobs){
        if(!std::regex_match(job.building_key,building_key)||!keys.insert(job.building_key).second||job.developer_id< -1||job.developer_id>49)
            return fail(e,"Developer construction Building key/developer invalid or duplicated");
        if(!job.present&&(job.active||job.developer_id!=-1))return fail(e,"Developer restored inactive Building requires absent Job, inactive property and no assigned Developer");
        if(job.developer_id>=0){
            if(!job.present||!assigned.insert(job.developer_id).second)return fail(e,"Developer construction assignment is absent or repeated");
            const auto it=std::find_if(s.rows.begin(),s.rows.end(),[&](const auto& row){return row.id==job.developer_id;});
            if(it==s.rows.end()||!it->job_link||!it->on_map)return fail(e,"Developer construction assignment lacks reciprocal pool Job/map membership");
        }
    }
    for(const auto& row:s.rows)if(row.job_link!=assigned.contains(row.id))return fail(e,"Developer pool Job link lacks exact construction assignment");
    return true;
}
bool valid(const PoolState& s,std::string& e){
    if(s.count<0||s.count>50||s.rows.size()!=50||s.indoor.size()+s.outdoor.size()!=50)return fail(e,"Developer pool requires count0..50 and exactly50 rows/manager entries");
    std::set<int32_t> ids,members,outside;
    for(const auto& values:{&s.indoor,&s.outdoor})for(const auto id:*values){if(id<0||id>49||!members.insert(id).second)return fail(e,"Developer manager IDs duplicate/outside0..49");if(values==&s.outdoor)outside.insert(id);}
    for(size_t slot=0;slot<s.rows.size();++slot){const auto& r=s.rows[slot];
        if(r.id<0||r.id>49||!ids.insert(r.id).second||r.state<0||r.state>7||r.employed!=(slot<size_t(s.count))||r.on_map!=outside.contains(r.id))
            return fail(e,"Developer pool ID/state/employment prefix/manager membership differs");
        for(float value:r.position)if(!std::isfinite(value))return fail(e,"Developer pool position is not finite");
    }
    if(ids!=members)return fail(e,"Developer manager registration union differs from persisted IDs");
    if(!valid_construction(s,e))return false;
    e.clear();return true;
}
json encode(const PoolState& s){
    std::string e;if(!valid(s,e))throw std::runtime_error(e);
    auto rows=json::array();for(const auto& r:s.rows)rows.push_back(json::array({r.id,r.state,r.employed,r.active,r.on_map,r.job_link,r.position[0],r.position[1],r.position[2]}));
    json out={{"count",s.count},{"rows",rows},{"indoor",s.indoor},{"outdoor",s.outdoor}};
    if(s.construction){auto actors=json::array(),jobs=json::array();
        for(const auto& p:s.construction->actors)actors.push_back(json::array({p.id,p.region_id,p.orientation[0],p.orientation[1],p.orientation[2],p.orientation[3],p.scale[0],p.scale[1],p.scale[2]}));
        for(const auto& job:s.construction->jobs)jobs.push_back(json::array({job.building_key,job.present,job.active,job.developer_id}));
        out["construction"]={{"actors",actors},{"jobs",jobs}};
    }
    return out;
}
bool decode(const json& j,PoolState& out,std::string& e){
    out={};try{
        if(!j.is_object()||(j.size()!=4&&j.size()!=5)||(j.size()==5&&!j.contains("construction"))||!j.contains("count")||!j.contains("rows")||!j.contains("indoor")||!j.contains("outdoor"))throw std::runtime_error("Developer pool requires four fields plus optional construction");
        PoolState s;s.count=integer(j.at("count"));const auto& rows=j.at("rows");if(!rows.is_array()||rows.size()!=50)throw std::runtime_error("Developer pool requires exactly50 rows");
        for(const auto& r:rows){if(!r.is_array()||r.size()!=9)throw std::runtime_error("Developer pool row requires exact nine fields");
            s.rows.push_back({integer(r[0]),integer(r[1]),boolean(r[2]),boolean(r[3]),boolean(r[4]),boolean(r[5]),{scalar(r[6]),scalar(r[7]),scalar(r[8])}});}
        for(const auto* name:{"indoor","outdoor"}){const auto& values=j.at(name);if(!values.is_array()||values.size()>50)throw std::runtime_error("Developer manager array exceeds50");
            auto& target=std::string(name)=="indoor"?s.indoor:s.outdoor;for(const auto& v:values)target.push_back(integer(v));}
        if(j.contains("construction")){
            const auto& c=j.at("construction");if(!c.is_object()||c.size()!=2||!c.contains("actors")||!c.contains("jobs"))throw std::runtime_error("Developer construction requires exact actors/jobs");
            const auto& actors=c.at("actors");const auto& jobs=c.at("jobs");
            if(!actors.is_array()||actors.size()!=50||!jobs.is_array())throw std::runtime_error("Developer construction actor/job shape differs");
            construction::State value;
            for(const auto& row:actors){if(!row.is_array()||row.size()!=9)throw std::runtime_error("Developer construction Actor requires exact9 fields");
                value.actors.push_back({integer(row[0]),integer(row[1]),{scalar(row[2]),scalar(row[3]),scalar(row[4]),scalar(row[5])},{scalar(row[6]),scalar(row[7]),scalar(row[8])}});}
            for(const auto& row:jobs){if(!row.is_array()||row.size()!=4||!row[0].is_string())throw std::runtime_error("Developer construction Job requires exact4 fields");
                value.jobs.push_back({row[0].get<std::string>(),boolean(row[1]),boolean(row[2]),integer(row[3])});}
            s.construction=std::move(value);
        }
        if(!valid(s,e))return false;out=std::move(s);e.clear();return true;
    }catch(const std::exception& ex){return fail(e,std::string("Developer pool decode: ")+ex.what());}
}
bool count_effect(bool hire,const PoolState& before,const PoolState& after,std::string& e){
    if(!valid(before,e)||!valid(after,e))return false;
    if((hire&&before.count==50)||(!hire&&before.count==0))return fail(e,"Developer original count is at native boundary");
    auto expected=before;expected.count+=hire?1:-1;
    for(size_t i=0;i<expected.rows.size();++i){auto& r=expected.rows[i];r.employed=i<size_t(expected.count);if(r.employed)r.active=true;}
    if(expected!=after)return fail(e,"Developer original settled Hire/Fire count effect differs from full50 target");
    e.clear();return true;
}
}
