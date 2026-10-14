#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::action_session {
struct Request;struct Evidence;struct Ledger;struct RegionState;
bool path_effect(const Request&,const Evidence&,std::string&);
bool path_receipt_fits(const Request&,const Ledger&,const RegionState&,std::string&);
using PathPoint=std::array<float,3>;
struct PathIntent {
    int32_t region=-1;
    int32_t start_node=-1,end_node=-1;
    std::string graph_before_sha256;
    PathPoint start{},end{};
    float quoted_length=0;
    std::array<float,12> spline{};
    std::vector<PathPoint> preview;
    bool operator==(const PathIntent&)const=default;
};
struct PathDeleteIntent {
    int32_t region=-1,path_index=-1;
    std::string graph_before_sha256;
    bool operator==(const PathDeleteIntent&)const=default;
};
inline bool valid_path_delete(const PathDeleteIntent& p) {
    if(p.region<0||p.path_index<0||p.path_index>=1000000||p.graph_before_sha256.size()!=64)return false;
    for(char c:p.graph_before_sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
bool path_delete_effect(const Request&,const Evidence&,std::string&);
struct PathState {
    uint64_t paths=0,nodes=0;
    std::string graph_sha256;
    int32_t cost=0;
    bool operator==(const PathState&)const=default;
};
inline uint64_t path_new_nodes(const PathIntent& p){return uint64_t(p.start_node<0)+uint64_t(p.end_node<0);}
inline bool path_connected(const PathIntent& p){return p.start_node>=0||p.end_node>=0;}
inline bool valid_path(const PathIntent& p) {
    if(p.start_node < -1||p.end_node < -1||p.start_node>=1000000||p.end_node>=1000000||(p.start_node>=0&&p.start_node==p.end_node))return false;
    if(path_connected(p)){
        if(p.graph_before_sha256.size()!=64)return false;
        for(char c:p.graph_before_sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    }else if(!p.graph_before_sha256.empty())return false;
    if(p.region<0||p.start==p.end||!std::isfinite(p.quoted_length)||p.quoted_length<=0||p.preview.size()<2||p.preview.size()>128)return false;
    for(const auto& point:{p.start,p.end})for(float v:point)if(!std::isfinite(v))return false;
    for(float v:p.spline)if(!std::isfinite(v))return false;
    for(const auto& point:p.preview)for(float v:point)if(!std::isfinite(v))return false;
    return true;
}
}
