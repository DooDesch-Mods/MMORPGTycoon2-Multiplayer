#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace mt2mp::net {
enum class ActionClaim { Fresh, Cached, Rejected };
template<class Result> class ActionOrder {
    struct Entry { uint64_t sequence; std::string payload; Result result; };
    std::array<uint64_t,5> high_water{};
    std::map<std::string,Entry> cache;
    std::deque<std::string> order;
public:
    void reset() { high_water={}; cache.clear(); order.clear(); }
    ActionClaim claim(uint64_t peer,uint64_t sequence,const std::string& id,const std::string& payload,Result& previous,std::string& error) {
        if(peer<1||peer>4||sequence==0) { error="invalid action peer or sequence"; return ActionClaim::Rejected; }
        auto key=std::to_string(peer)+":"+id;
        if(auto found=cache.find(key);found!=cache.end()) {
            if(found->second.sequence!=sequence||found->second.payload!=payload) { error="conflicting duplicate action"; return ActionClaim::Rejected; }
            previous=found->second.result; return ActionClaim::Cached;
        }
        if(sequence<=high_water[peer]) { error="expired or reordered action"; return ActionClaim::Rejected; }
        high_water[peer]=sequence; return ActionClaim::Fresh;
    }
    void confirm(uint64_t peer,uint64_t sequence,const std::string& id,const std::string& payload,const Result& result) {
        auto key=std::to_string(peer)+":"+id;
        cache.emplace(key,Entry{sequence,payload,result}); order.push_back(key);
        if(order.size()>64) { cache.erase(order.front()); order.pop_front(); }
    }
};
}
