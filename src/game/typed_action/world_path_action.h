#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::action_session {
enum class WorldPathKind:uint8_t {Wall=1,Pass=2};
using WorldPathBits=std::array<uint32_t,3>;
using WorldPathSpline=std::array<uint32_t,12>;
struct WorldPathIntent {
    WorldPathKind kind=WorldPathKind::Wall;
    int32_t start_region=-1,end_region=-1,type=-1,node_type=-1;
    std::string name,start_node,end_node,start_split,end_split;
    WorldPathBits start{},end{};
    uint32_t quoted_length=0;
    std::vector<WorldPathSpline> segments;
    std::vector<WorldPathBits> preview;
    bool preview_valid=false;
    std::string graph_before_sha256;
    bool operator==(const WorldPathIntent&)const=default;
};
struct WorldPathState {
    std::string complete_sha256,world_effect_sha256;
    uint32_t paths=0,nodes=0;
    bool operator==(const WorldPathState&)const=default;
};
inline bool world_path_digest(const std::string& value){
    if(value.size()!=64)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
inline bool world_path_valid(const WorldPathIntent& value){
    auto canonical_key=[](const std::string& key,bool split){
        return key.size()<=128&&key.find('\0')==std::string::npos&&
               (key.empty()||(split&&key=="preview")||key.rfind("snapshot:",0)==0||key.rfind("action:",0)==0);
    };
    auto finite=[](const WorldPathBits& point){for(uint32_t bits:point)if(!std::isfinite(std::bit_cast<float>(bits)))return false;return true;};
    for(const auto& point:value.preview)if(!finite(point))return false;
    for(const auto& segment:value.segments)for(uint32_t bits:segment)
        if(!std::isfinite(std::bit_cast<float>(bits)))return false;
    if(value.start_region<0||value.end_region<0||value.type<0||value.type>=11||
       value.name.size()>256||value.name.find('\0')!=std::string::npos||
       !canonical_key(value.start_node,false)||!canonical_key(value.end_node,false)||
       !canonical_key(value.start_split,true)||!canonical_key(value.end_split,true)||
       !finite(value.start)||!finite(value.end)||
       !world_path_digest(value.graph_before_sha256)||value.preview.size()<2||value.preview.size()>1024)
        return false;
    if(value.kind==WorldPathKind::Wall)
        return value.start_region==value.end_region&&value.node_type>=0&&value.node_type<6&&
            std::isfinite(std::bit_cast<float>(value.quoted_length))&&
            std::bit_cast<float>(value.quoted_length)>0&&value.segments.size()>0&&value.segments.size()<=10;
    if(value.kind==WorldPathKind::Pass)
        return value.start_region!=value.end_region&&value.type==2&&value.node_type==-1&&value.preview_valid&&
            value.preview.size()==2&&
            !value.quoted_length&&value.segments.empty()&&value.start_split.empty()&&value.end_split.empty();
    return false;
}
inline bool world_path_state_valid(const WorldPathState& value){
    return world_path_digest(value.complete_sha256)&&world_path_digest(value.world_effect_sha256)&&
           value.paths<=1000000&&value.nodes<=1000000;
}
}
