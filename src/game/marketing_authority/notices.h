#pragma once
#include "live_host_state/vip_transitions.h"

namespace mt2mp::game::marketing_authority::notice {
using Origin=live_host_state::vip::Origin;
using Json=nlohmann::json;
inline bool fail(std::string& e,const std::string& s){e="Campaign notice "+s;return false;}
struct Row {uint64_t sequence=0;std::string message;bool operator==(const Row&)const=default;};
struct Batch {uint64_t after=0;std::vector<Row> rows;bool operator==(const Batch&)const=default;};
inline bool valid(const Batch& b,std::string& e){
    uint64_t sequence=b.after;size_t bytes=0;
    for(const auto& r:b.rows){
        if(sequence==UINT64_MAX||r.sequence!=++sequence||r.message.empty()||r.message.find('\0')!=std::string::npos||
           r.message.size()>live_host_state::max_logical_frame_payload-bytes)return fail(e,"sequence/text/resource bound differs");
        bytes+=r.message.size();
        try{(void)Json(r.message).dump();}catch(const std::exception& x){return fail(e,std::string("UTF-8: ")+x.what());}
    }
    e.clear();return true;
}
inline Json encode(const Batch& b){Json rows=Json::array();for(const auto& r:b.rows)rows.push_back({r.sequence,r.message});return {{"after",b.after},{"rows",std::move(rows)}};}
inline bool decode(const Json& j,Batch& out,std::string& e){
    out={};try{
        if(!j.is_object()||j.size()!=2||!j.contains("after")||!j.contains("rows")||!j.at("rows").is_array())return fail(e,"exact batch fields required");
        Batch b;b.after=live_host_state::vip::unsigned_value(j.at("after"));
        for(const auto& r:j.at("rows")){
            if(!r.is_array()||r.size()!=2||!r[1].is_string())return fail(e,"exact sequence/message row required");
            b.rows.push_back({live_host_state::vip::unsigned_value(r[0]),r[1].get<std::string>()});
        }
        if(!valid(b,e))return false;out=std::move(b);return true;
    }catch(const std::exception& x){return fail(e,std::string("decode: ")+x.what());}
}
// Coverage names existing fully applied Frame ACKs, never packet delivery.
class Host {
    Origin origin_;uint32_t peers_=0;uint64_t next_=0,last_frame_=0;
    Batch pending_;std::array<uint64_t,4> acknowledged_{};std::map<uint64_t,uint64_t> coverage_;
public:
    bool bind(const Origin& o,uint32_t peers,std::string& e){
        if(!live_host_state::vip::valid_origin(o)||(peers&~15u))return fail(e,"Host origin/peers invalid");
        *this={};origin_=o;peers_=peers;e.clear();return true;
    }
    const Batch& pending()const{return pending_;}
    bool append(const Origin& o,const std::string& message,std::string& e){
        if(o!=origin_||!live_host_state::vip::valid_origin(o)||next_==UINT64_MAX)return fail(e,"Host origin/sequence differs");
        auto next=pending_;next.rows.push_back({next_+1,message});if(!valid(next,e))return false;
        pending_=std::move(next);++next_;return true;
    }
    bool sent(const Origin& o,uint64_t frame,const Batch& captured,std::string& e){
        if(o!=origin_||!live_host_state::vip::valid_origin(o)||!frame||frame<=last_frame_||captured!=pending_)return fail(e,"sent Frame origin/order/payload differs");
        coverage_.emplace(frame,next_);last_frame_=frame;
        if(!peers_){pending_.after=next_;pending_.rows.clear();coverage_.clear();}e.clear();return true;
    }
    bool acknowledge(const Origin& o,uint32_t peer,uint64_t frame,std::string& e){
        if(o!=origin_||!live_host_state::vip::valid_origin(o)||peer<1||peer>4||!(peers_&(1u<<(peer-1)))||!frame||frame>last_frame_)return fail(e,"ACK origin/peer/frame differs");
        auto& prior=acknowledged_[peer-1];if(frame<=prior){e.clear();return true;}
        if(!coverage_.contains(frame))return fail(e,"ACK does not name sent Frame");
        prior=frame;uint64_t floor=UINT64_MAX;
        for(uint32_t i=0;i<4;++i)if(peers_&(1u<<i))floor=std::min(floor,acknowledged_[i]);
        if(floor){const auto covered=coverage_.find(floor);if(covered==coverage_.end())return fail(e,"ACK coverage missing");
            pending_.after=covered->second;std::erase_if(pending_.rows,[&](const Row& r){return r.sequence<=pending_.after;});coverage_.erase(coverage_.begin(),covered);}
        e.clear();return true;
    }
};
class Client {
    Origin origin_;uint64_t committed_=0,floor_=0;std::map<uint64_t,Row> seen_;
public:
    bool bind(const Origin& o,std::string& e){if(!live_host_state::vip::valid_origin(o))return fail(e,"Client origin invalid");*this={};origin_=o;e.clear();return true;}
    uint64_t committed()const{return committed_;}
    bool prepare(const Origin& o,const Batch& b,std::vector<Row>& work,std::string& e)const{
        work.clear();if(o!=origin_||!live_host_state::vip::valid_origin(o))return fail(e,"Client origin invalid");if(!valid(b,e))return false;
        if(b.after<floor_||b.after>committed_||b.rows.size()>UINT64_MAX-b.after||b.after+b.rows.size()<committed_)return fail(e,"batch loses confirmed prefix");
        for(const auto& r:b.rows){if(r.sequence<=committed_){const auto it=seen_.find(r.sequence);if(it==seen_.end()||it->second!=r)return fail(e,"conflicting duplicate");}else work.push_back(r);}
        e.clear();return true;
    }
    bool supersede(const Origin& o,const Batch& next,const Batch* prior,std::string& e)const{
        std::vector<Row> work;if(!prepare(o,next,work,e))return false;
        if(prior)for(const auto& r:prior->rows){if(r.sequence<=committed_)continue;
            if(r.sequence<=next.after||r.sequence-next.after>next.rows.size()||next.rows[size_t(r.sequence-next.after-1)]!=r)return fail(e,"queued unconfirmed notice lost/conflicting");}
        return true;
    }
    bool confirm(const Origin& o,const Batch& b,const std::vector<Row>& applied,std::string& e){
        std::vector<Row> expected;if(!prepare(o,b,expected,e))return false;if(expected!=applied)return fail(e,"native receipts differ from planned notices");
        for(const auto& r:expected){seen_.emplace(r.sequence,r);committed_=r.sequence;}floor_=b.after;seen_.erase(seen_.begin(),seen_.upper_bound(floor_));e.clear();return true;
    }
};
}
