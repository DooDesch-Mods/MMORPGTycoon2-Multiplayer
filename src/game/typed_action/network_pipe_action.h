#pragma once
#include "network_action.h"
#include <vector>
namespace mt2mp::action_session {
using NetworkPoint=std::array<float,3>;
struct NetworkPipeIntent {
    int32_t region=-1,start_node=-1,end_node=-1;
    std::string type,key,node_type,graph_before_sha256;
    NetworkPoint start{},end{};
    float quoted_length=0;
    int32_t quoted_cost=0;
    std::vector<NetworkPoint> preview;
    std::vector<std::array<float,12>> splines;
    bool operator==(const NetworkPipeIntent&)const=default;
};
inline bool valid_network_pipe(const NetworkPipeIntent& p){
    if(p.region<0||p.start_node<0||p.end_node< -1||p.start_node==p.end_node||p.type!="Cable"||p.key!="item_cable"||p.node_type!="Junction"||
       p.graph_before_sha256.size()!=64||p.quoted_cost<0||!std::isfinite(p.quoted_length)||p.quoted_length<=0||p.preview.size()<2||p.preview.size()>2048||p.splines.size()!=1)return false;
    for(char ch:p.graph_before_sha256)if(!((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')))return false;
    for(const auto& point:{p.start,p.end})for(float v:point)if(!std::isfinite(v))return false;
    for(const auto& point:p.preview)for(float v:point)if(!std::isfinite(v))return false;
    for(const auto& spline:p.splines)for(float v:spline)if(!std::isfinite(v))return false;
    return p.start!=p.end;
}
bool network_pipe_effect(const Request&,const Evidence&,std::string&);
}
