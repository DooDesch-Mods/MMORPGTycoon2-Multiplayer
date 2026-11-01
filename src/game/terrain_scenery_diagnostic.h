#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vendor/nlohmann/json.hpp>

namespace mt2mp::game::terrain_scenery_diagnostic {
constexpr size_t capture_limit=4096,report_limit=16;
struct Row {
    int32_t category=0,variant=0;
    std::array<uint32_t,10> pose{};
    std::array<uint32_t,6> height{};
    std::array<uint8_t,4> flags{};
    bool auxiliary=false;
};
struct Snapshot {
    size_t total=0,auxiliary_errors=0;
    std::map<uint64_t,Row> rows;
    std::string first_error;
};
template<class Transform,class Read>void capture(Snapshot& snapshot,uint64_t uid,int32_t category,int32_t variant,const Transform& transform,Read&& read,size_t limit=capture_limit){
    ++snapshot.total;if(snapshot.rows.size()>=limit)return;
    Row row;row.category=category;row.variant=variant;
    const std::array<float,10> values{transform.position.x,transform.position.y,transform.position.z,
        transform.rotation[0],transform.rotation[1],transform.rotation[2],transform.rotation[3],transform.scale.x,transform.scale.y,transform.scale.z};
    for(size_t i=0;i<values.size();++i)row.pose[i]=std::bit_cast<uint32_t>(values[i]);
    std::array<uint8_t,0x30> extra{};std::string error;
    if(read(extra.data(),extra.size(),error)){
        constexpr std::array<size_t,6> offsets{0,4,8,12,16,0x28};
        for(size_t i=0;i<offsets.size();++i)std::memcpy(&row.height[i],extra.data()+offsets[i],4);
        row.flags={extra[0x14],extra[0x15],extra[0x16],extra[0x2d]};row.auxiliary=true;
    }else{++snapshot.auxiliary_errors;if(snapshot.first_error.empty())snapshot.first_error=error.empty()?"Scenery diagnostic auxiliary read failed":error;}
    if(!snapshot.rows.emplace(uid,row).second&&snapshot.first_error.empty())snapshot.first_error="Scenery diagnostic duplicate UID";
}
inline nlohmann::json auxiliary(const Row& row){
    if(!row.auxiliary)return nullptr;
    return {{"start_bits",row.height[0]},{"target_bits",row.height[1]},{"current_bits",row.height[2]},
        {"elapsed_bits",row.height[3]},{"duration_bits",row.height[4]},{"height_over_ground_bits",row.height[5]},
        {"active",row.flags[0]},{"ease",row.flags[1]},{"retarget_ease",row.flags[2]},{"automatic",row.flags[3]}};
}
inline nlohmann::json difference(const Snapshot& expected,const Snapshot& actual){
    using json=nlohmann::json;
    constexpr std::array<const char*,10> pose_names{"position_x","position_y","position_z","rotation_x","rotation_y","rotation_z","rotation_w","scale_x","scale_y","scale_z"};
    constexpr std::array<const char*,6> height_names{"tween_start","tween_target","tween_current","tween_elapsed","tween_duration","height_over_ground"};
    constexpr std::array<const char*,4> flag_names{"tween_active","tween_ease","tween_retarget_ease","automatic"};
    json rows=json::array(),counts=json::object();size_t content_rows=0,auxiliary_rows=0,added=0,removed=0,changed=0;
    auto account=[&](const char* name){counts[name]=counts.value(name,size_t(0))+1;};
    for(const auto& [uid,before]:expected.rows){
        const auto found=actual.rows.find(uid);
        if(found==actual.rows.end()){++removed;++changed;if(rows.size()<report_limit)rows.push_back({{"uid",uid},{"state","absent_in_actual_capture"}});continue;}
        const auto& after=found->second;json fields=json::object(),extras=json::object();
        auto integer=[&](const char* name,auto a,auto b,json& target){if(a!=b){target[name]={{"expected",a},{"actual",b}};account(name);}};
        integer("category",before.category,after.category,fields);integer("variant",before.variant,after.variant,fields);
        for(size_t i=0;i<pose_names.size();++i)integer(pose_names[i],before.pose[i],after.pose[i],fields);
        if(before.auxiliary&&after.auxiliary){
            for(size_t i=0;i<height_names.size();++i)integer(height_names[i],before.height[i],after.height[i],extras);
            for(size_t i=0;i<flag_names.size();++i)integer(flag_names[i],before.flags[i],after.flags[i],extras);
        }
        if(!fields.empty())++content_rows;if(!extras.empty())++auxiliary_rows;
        if(!fields.empty()||!extras.empty()){++changed;if(rows.size()<report_limit)rows.push_back({{"uid",uid},{"hashed_fields",fields},{"auxiliary_fields",extras},
            {"expected_auxiliary",auxiliary(before)},{"actual_auxiliary",auxiliary(after)},
            {"expected_pose_bits",before.pose},{"actual_pose_bits",after.pose}});}
    }
    for(const auto& [uid,row]:actual.rows){(void)row;if(expected.rows.contains(uid))continue;++added;++changed;
        if(rows.size()<report_limit)rows.push_back({{"uid",uid},{"state","absent_in_expected_capture"}});}
    const bool complete=expected.total==expected.rows.size()&&actual.total==actual.rows.size();
    return {{"schema_version",1},{"capture_limit",capture_limit},{"report_limit",report_limit},
        {"expected_total",expected.total},{"actual_total",actual.total},{"expected_captured",expected.rows.size()},{"actual_captured",actual.rows.size()},
        {"content_capture_complete",complete},{"auxiliary_capture_complete",complete&&!expected.auxiliary_errors&&!actual.auxiliary_errors&&expected.first_error.empty()&&actual.first_error.empty()},
        {"expected_diagnostic_error",expected.first_error},{"actual_diagnostic_error",actual.first_error},
        {"hashed_rows_changed_in_capture",content_rows},{"auxiliary_rows_changed_in_capture",auxiliary_rows},
        {"added_to_capture",added},{"removed_from_capture",removed},{"field_counts_in_capture",counts},
        {"changed_rows_in_capture",changed},{"rows_truncated",changed>rows.size()},{"rows",rows},
        {"float_encoding","uint32 IEEE-754 bits; auxiliary values do not alter the existing scenery hash"}};
}
}
