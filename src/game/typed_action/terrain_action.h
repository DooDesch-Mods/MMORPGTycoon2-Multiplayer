#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <bit>

namespace mt2mp::action_session {
struct Request;struct Ledger;struct RegionState;struct Evidence;
bool terrain_height_effect(const Request&,const Evidence&,std::string&);
bool terrain_receipt_fits(const Request&,const Ledger&,const RegionState&,std::string&);
struct TerrainPoint {
    int64_t x=0,z=0;
    bool operator==(const TerrainPoint&) const=default;
};
using TerrainPath=std::vector<TerrainPoint>;
using TerrainShape=std::vector<TerrainPath>;
struct TerrainStamp {
    std::array<float,3> position{};
    float radius=0;
    TerrainShape shape;
    bool operator==(const TerrainStamp&) const=default;
};
struct TerrainHeightIntent {
    bool up=false;
    std::array<float,3> hit{},picked{},previous{};
    float radius=0;
    int32_t cell=-1;
    std::string before_sha256;
    bool operator==(const TerrainHeightIntent&)const=default;
};
struct TerrainIntent {
    int32_t id=-1,style=-1;
    std::string start_label;
    float initial_area=0;
    uint32_t reset_after=0;
    uint8_t finish=1; // 0: EndEdit, 1: PaintingFinished, 2: HandleCancel.
    std::vector<TerrainStamp> stamps;
    std::optional<TerrainHeightIntent> height;
    bool operator==(const TerrainIntent&) const=default;
};
struct TerrainState {
    std::array<uint64_t,4> global_random{};
    std::array<int32_t,8> release_edits{};
    std::string contours_sha256,bugs_sha256;
    uint64_t bug_count=0;
    std::string heights_sha256;
    int32_t height_cell=-1;
    uint32_t height_bits=0;
    bool operator==(const TerrainState&) const=default;
};
inline bool same_terrain_content(TerrainState a,TerrainState b) {
    a.global_random={};b.global_random={};return a==b;
}
inline bool valid_terrain(const TerrainIntent& v) {
    if(v.height){
        const auto& h=*v.height;
        if(v.id<0||v.style!=-1||!v.start_label.empty()||v.initial_area!=0||v.reset_after||v.finish!=1||!v.stamps.empty()||h.cell<0||h.cell>=1000000||!std::isfinite(h.radius)||h.radius<=0||h.before_sha256.size()!=64)return false;
        for(char c:h.before_sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
        for(const auto* p:{&h.hit,&h.picked,&h.previous})for(float f:*p)if(!std::isfinite(f))return false;
        return h.picked!=h.previous;
    }
    if(v.id<0||v.style<0||v.style>3||v.start_label.empty()||v.start_label.size()>256||v.start_label.find('\0')!=std::string::npos||v.finish>2||!std::isfinite(v.initial_area)||v.reset_after>v.stamps.size())return false;
    size_t points=0;
    for(const auto& stamp:v.stamps) {
        if(!std::isfinite(stamp.radius)||stamp.radius<=0||stamp.shape.empty())return false;
        for(auto value:stamp.position)if(!std::isfinite(value))return false;
        for(const auto& path:stamp.shape) {
            if(path.size()<3||path.size()>1000000-points)return false;
            points+=path.size();
            // Clipper2 uses the signed 62-bit coordinate domain.
            for(const auto& point:path)if(point.x<-(INT64_MAX>>2)||point.x>(INT64_MAX>>2)||point.z<-(INT64_MAX>>2)||point.z>(INT64_MAX>>2))return false;
        }
    }
    return true;
}
}
