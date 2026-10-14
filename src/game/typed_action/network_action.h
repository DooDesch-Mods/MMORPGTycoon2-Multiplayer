#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
namespace mt2mp::action_session {
struct Request;struct Evidence;
struct NetworkIntent {
    int32_t region=-1;
    std::string type,key;
    std::array<float,3> position{};
    int32_t quoted_cost=0;
    bool operator==(const NetworkIntent&)const=default;
};
struct NetworkState {
    uint64_t nodes=0,pipes=0,connections=0;
    std::string graph_sha256;
    bool operator==(const NetworkState&)const=default;
};
inline bool valid_network(const NetworkIntent& p) {
    if(p.region<0||p.type.empty()||p.key.empty()||p.type.size()>128||p.key.size()>128||p.quoted_cost<0||
       p.type.find('\0')!=std::string::npos||p.key.find('\0')!=std::string::npos)return false;
    for(float v:p.position)if(!std::isfinite(v))return false;
    return true;
}
bool network_effect(const Request&,const Evidence&,std::string&);
}
