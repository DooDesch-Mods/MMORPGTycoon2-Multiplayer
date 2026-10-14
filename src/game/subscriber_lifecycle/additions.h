#pragma once
#include "payload.h"
#include <functional>
#include <map>
#include <set>
namespace mt2mp::subscriber_lifecycle::additions {
using Json=nlohmann::json;
struct Retirement {int32_t id=-1,reason=-1;bool operator==(const Retirement&)const=default;};
struct State {
    bool available=false;int32_t next_id=-1;
    std::vector<int32_t> baseline;
    std::vector<std::string> graphs;
    std::vector<Retirement> retired;
    bool operator==(const State&)const=default;
};
bool frame_binding(const Json&,uint64_t session,const std::string& snapshot,uint64_t epoch,std::string&);
bool pack(const Json&,std::string&,std::string&);
bool unpack(const std::string&,Json&,first_append::Payload&,std::string&);
bool valid(const State&,std::string&);
struct Entry {int32_t id=-1;std::string sha,graph;uint8_t acknowledgements=0;uint64_t first_frame=0;};
class Cache {
    bool bound_=false,locked_=false;
    std::vector<int32_t> baseline_;
    std::map<int32_t,Entry> entries_;
    std::map<int32_t,int32_t> retired_;
public:
    using Capture=std::function<bool(int32_t,Json&,std::string&)>;
    using Append=std::function<bool(const Json&,Json&,std::string&)>;
    using Retire=std::function<bool(int32_t,int32_t,Json&,std::string&)>;
    bool host(const std::vector<int32_t>& actual,int32_t next,uint64_t revision,Capture,State&,std::string&,const std::vector<Retirement>& receipts={});
    bool client(const std::vector<int32_t>& actual,const std::vector<int32_t>& target,const State&,Append,Json&,std::string&,Retire={});
    bool acknowledge(uint8_t peer,uint8_t peers,uint64_t revision,const Json& rows,std::string&);
    static Json acknowledgement(const State&);
    void clear(){*this=Cache{};}
    bool locked()const{return locked_;}
    Json status()const{Json retired=Json::array();for(const auto& [id,reason]:retired_)retired.push_back({{"id",id},{"reason",reason}});size_t pending=0,bytes=0;for(const auto& [id,row]:entries_)if(!row.graph.empty()){++pending;bytes+=row.graph.size();}return {{"bound",bound_},{"locked",locked_},{"known_additions",entries_.size()},{"retired_ids",retired_.size()},{"retired",retired},{"pending_graphs",pending},{"pending_graph_bytes",bytes}};}
};
}
