#pragma once
#include "scenery_make_history.h"
#include "terrain_scenery_diagnostic.h"
#include <vector>

namespace mt2mp::game::terrain_history_payload {
using Saved=scenery_make_history::Saved;
using Row=terrain_scenery_diagnostic::Row;
using Json=nlohmann::json;
inline std::array<uint32_t,10> pose(const Saved& value){
    std::array<uint32_t,10> raw{};std::memcpy(raw.data(),value.transform.data(),sizeof raw);
    return {raw[4],raw[5],raw[6],raw[0],raw[1],raw[2],raw[3],raw[7],raw[8],raw[9]};
}
inline std::array<uint32_t,16> cached(const Saved& value){
    std::array<uint32_t,16> out{};std::memcpy(out.data(),value.transform.data()+0x2c,sizeof out);return out;
}
inline Json fields(const Saved& value){
    return {{"object",value.object},{"root",value.root},{"region",value.region},{"uid",value.uid},{"type",value.type},
        {"pose_bits",pose(value)},{"height_over_ground_bits",value.height},{"automatic",value.automatic},
        {"matrix_dirty",value.transform[0x28]},{"padding",{value.transform[0x29],value.transform[0x2a],value.transform[0x2b]}},
        {"matrix_bits",cached(value)}};
}
inline bool row_matches(const Saved& value,const Row& row){
    return row.auxiliary&&value.type==std::array<int32_t,2>{row.category,row.variant}&&pose(value)==row.pose&&
        value.height==row.height[5]&&value.automatic==row.flags[3];
}
struct Report {
    size_t checked=0,failed=0,raw_changed=0,diagnostic_rows=0;
    Json rows=Json::array();
    template<class Matrix>void add(const Saved& expected,const Saved& actual,const Row& before,const Row& after,bool height_proven,Matrix&& matrix){
        ++checked;std::vector<std::string> errors;
        auto differs=[&](bool value,const char* field){if(value)errors.push_back(field);};
        differs(expected.object!=actual.object,"object");differs(expected.root!=actual.root,"root");
        differs(expected.region!=actual.region,"region");differs(expected.uid!=actual.uid,"uid");
        differs(expected.type!=actual.type,"type");differs(expected.height!=actual.height,"height_over_ground");
        differs(expected.automatic!=actual.automatic,"automatic");
        differs(!height_proven,"complete original height/publication proof absent");
        differs(!row_matches(expected,before),"saved payload differs from original complete row");
        differs(!row_matches(actual,after),"restored payload changed after complete row proof");
        const auto old_pose=pose(expected),new_pose=pose(actual);
        for(size_t i=0;i<old_pose.size();++i)if(i!=1&&old_pose[i]!=new_pose[i])errors.push_back("persisted_pose_"+std::to_string(i));
        Json cache_proof=Json::object();
        for(const auto& [name,value]:{std::pair{"expected",&expected},std::pair{"actual",&actual}}){
            // Native GetMatrix consumes the cache only when dirty is zero.
            // Its other branch rebuilds every matrix word from the saved pose.
            if(value->transform[0x28])continue;
            std::array<uint32_t,16> derived{};std::string cause;
            const bool returned=matrix(value->transform,derived,cause);
            cache_proof[name]={{"returned",returned},{"derived_bits",derived},{"cause",cause}};
            if(!returned)errors.push_back(std::string(name)+" original matrix proof failed: "+cause);
            else if(cached(*value)!=derived)errors.push_back(std::string(name)+" clean cached matrix differs from original pose");
        }
        const bool changed=expected!=actual;if(changed)++raw_changed;if(!errors.empty())++failed;
        if(changed||!errors.empty())++diagnostic_rows;
        if((changed||!errors.empty())&&rows.size()<terrain_scenery_diagnostic::report_limit){
            Json offsets=Json::array();for(size_t i=0;i<expected.transform.size();++i)if(expected.transform[i]!=actual.transform[i])offsets.push_back(i);
            rows.push_back({{"uid",expected.uid},{"accepted",errors.empty()},{"errors",errors},{"expected",fields(expected)},
                {"actual",fields(actual)},{"changed_transform_byte_offsets",offsets},{"clean_cache_proof",cache_proof}});
        }
    }
    Json json()const{return {{"schema_version",1},{"checked_rows",checked},{"failed_rows",failed},{"raw_changed_rows",raw_changed},
        {"report_limit",terrain_scenery_diagnostic::report_limit},{"rows_truncated",diagnostic_rows>rows.size()},
        {"rows",rows}};}
};
}
