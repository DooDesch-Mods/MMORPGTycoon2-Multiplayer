#pragma once
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include "frame_limits.h"
#include <string>
#include <vector>

namespace mt2mp::game::live_host_state::vip {
struct Origin {
    uint64_t session=0,epoch=0,generation=0;
    std::string snapshot;
    bool operator==(const Origin&)const=default;
};
struct Transition {
    uint64_t sequence=0,arrival=0;
    int32_t subscriber=-1;
    std::string name;
    bool active=false;
    std::optional<std::string> message;
    bool operator==(const Transition&)const=default;
};
struct Batch {
    uint64_t after=0;
    std::vector<Transition> rows;
    bool operator==(const Batch&)const=default;
};
inline bool fail(std::string& e,const char* s){e=s;return false;}
inline bool valid_origin(const Origin& o){
    if(!o.session||!o.epoch||!o.generation||o.snapshot.size()!=36)return false;
    for(size_t i=0;i<o.snapshot.size();++i){const char c=o.snapshot[i];
        if(i==8||i==13||i==18||i==23){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}
    return true;
}
inline bool has_messages(const Batch& b){return std::any_of(b.rows.begin(),b.rows.end(),[](const auto& row){return row.message.has_value();});}
inline bool valid(const Batch& b,std::string& e){
    uint64_t previous=b.after;
    for(const auto& r:b.rows){
        if(previous==UINT64_MAX||r.sequence!=previous+1||!r.arrival||r.arrival>r.sequence||
            r.subscriber<0||r.name.size()>256||r.name.find('\0')!=std::string::npos||
            (r.active?r.arrival!=r.sequence:r.arrival==r.sequence))
            return fail(e,"VIP transition sequence/arrival/Subscriber/name differs");
        try{(void)nlohmann::json(r.name).dump();}catch(const std::exception& x){e=std::string("VIP name UTF-8 invalid: ")+x.what();return false;}
        if(r.message){
            if(r.active||r.message->empty()||r.message->size()>max_logical_frame_payload||r.message->find('\0')!=std::string::npos)
                return fail(e,"VIP disconnect message is absent/active/unsafe or exceeds Frame bound");
            try{(void)nlohmann::json(*r.message).dump();}catch(const std::exception& x){e=std::string("VIP message UTF-8 invalid: ")+x.what();return false;}
        }
        previous=r.sequence;
    }
    e.clear();return true;
}
inline nlohmann::json encode(const Batch& b){
    nlohmann::json rows=nlohmann::json::array();
    for(const auto& r:b.rows){nlohmann::json row={r.sequence,r.arrival,r.subscriber,r.name,r.active};if(r.message)row.push_back(*r.message);rows.push_back(std::move(row));}
    return {{"after",b.after},{"rows",std::move(rows)}};
}
inline uint64_t unsigned_value(const nlohmann::json& j){
    if(!j.is_number_integer()||(!j.is_number_unsigned()&&j.get<int64_t>()<0))throw std::runtime_error("VIP unsigned integer required");
    return j.get<uint64_t>();
}
inline bool decode(const nlohmann::json& j,Batch& out,std::string& e){
    out={};try{
        if(!j.is_object()||j.size()!=2||!j.contains("after")||!j.contains("rows")||!j.at("rows").is_array())
            return fail(e,"VIP exact batch fields required");
        Batch b;b.after=unsigned_value(j.at("after"));
        for(const auto& row:j.at("rows")){
            if(!row.is_array()||(row.size()!=5&&row.size()!=6)||!row.at(3).is_string()||!row.at(4).is_boolean()||(row.size()==6&&!row.at(5).is_string()))
                return fail(e,"VIP exact five-field transition or six-field disconnect required");
            const auto id=unsigned_value(row.at(2));if(id>INT32_MAX)return fail(e,"VIP Subscriber ID outside int32");
            Transition decoded{unsigned_value(row.at(0)),unsigned_value(row.at(1)),static_cast<int32_t>(id),row.at(3).get<std::string>(),row.at(4).get<bool>()};
            if(row.size()==6)decoded.message=row.at(5).get<std::string>();b.rows.push_back(std::move(decoded));
        }
        if(!valid(b,e))return false;out=std::move(b);return true;
    }catch(const std::exception& x){e=std::string("VIP decode: ")+x.what();return false;}
}
inline bool transition(std::map<uint64_t,Transition>& active,const Transition& row,std::string& e){
    if(row.active){if(!active.emplace(row.arrival,row).second)return fail(e,"VIP arrival already active");}
    else {const auto it=active.find(row.arrival);
        if(it==active.end()||it->second.subscriber!=row.subscriber||it->second.name!=row.name)
            return fail(e,"VIP retirement does not match active arrival");
        if(!it->second.active&&(!row.message||it->second.message))
            return fail(e,"VIP retirement or disconnect already confirmed");
        // A Weak expiry can precede the original EndEvent. Keep the copied
        // arrival identity so its later, actually witnessed notice is accepted.
        it->second=row;}
    return true;
}

// Frame ACKs enter here only after the existing sender/origin/row authentication.
// Native UI success must precede that ACK; transport receipt is insufficient.
class HostRetention {
    Origin origin_;
    uint32_t peers_=0;
    uint64_t next_=0,last_frame_=0;
    Batch pending_;
    std::array<uint64_t,4> acknowledged_{};
    std::map<uint64_t,uint64_t> coverage_;
    std::map<uint64_t,Transition> active_;
public:
    bool bind(const Origin& o,uint32_t peers,std::string& e){
        if(!valid_origin(o)||(peers&~15u))return fail(e,"VIP Host origin/peer mask invalid");
        *this={};origin_=o;peers_=peers;e.clear();return true;
    }
    void reset(){*this={};}
    const Batch& pending()const{return pending_;}
    bool append(const Origin& o,int32_t id,const std::string& name,bool active,uint64_t arrival,uint64_t& sequence,std::string& e,const std::optional<std::string>& message=std::nullopt){
        sequence=0;if(o!=origin_||!valid_origin(origin_)||next_==UINT64_MAX)return fail(e,"VIP Host origin/sequence unavailable");
        Transition row{next_+1,active?next_+1:arrival,id,name,active,message};
        if(active&&arrival)return fail(e,"VIP arrival cannot reference earlier arrival");
        Batch single{next_,{row}};if(!valid(single,e))return false;
        auto owners=active_;if(!transition(owners,row,e))return false;
        active_=std::move(owners);pending_.rows.push_back(row);sequence=++next_;e.clear();return true;
    }
    bool sent(const Origin& o,uint64_t frame,const Batch& captured,std::string& e){
        if(o!=origin_||!valid_origin(origin_)||!frame||frame<=last_frame_||captured!=pending_)
            return fail(e,"VIP sent frame origin/order/captured transitions differ");
        coverage_.emplace(frame,next_);last_frame_=frame;
        // No remote consumer exists in a Host-only world. Keep active identities
        // for retirement while releasing history after successful publication.
        if(!peers_){pending_.after=next_;pending_.rows.clear();coverage_.clear();}
        e.clear();return true;
    }
    bool acknowledge(const Origin& o,uint32_t peer,uint64_t frame,std::string& e){
        if(o!=origin_||peer<1||peer>4||!(peers_&(1u<<(peer-1)))||!frame||frame>last_frame_)
            return fail(e,"VIP ACK origin/peer/frame differs");
        auto& prior=acknowledged_[peer-1];
        if(frame<=prior){e.clear();return true;}
        if(!coverage_.contains(frame))return fail(e,"VIP ACK does not name a sent frame");
        prior=frame;uint64_t floor=UINT64_MAX;
        for(uint32_t i=0;i<4;++i)if(peers_&(1u<<i))floor=std::min(floor,acknowledged_[i]);
        if(floor){const auto covered=coverage_.find(floor);
            if(covered==coverage_.end())return fail(e,"VIP retained ACK coverage missing");
            pending_.after=covered->second;
            std::erase_if(pending_.rows,[&](const Transition& r){return r.sequence<=pending_.after;});
            coverage_.erase(coverage_.begin(),covered);}
        e.clear();return true;
    }
};

// Prepare is read-only. Confirm must be called after the native consumer has
// completed every returned transition, in order, within this exact Frame.
class ClientRetention {
    Origin origin_;
    uint64_t committed_=0,floor_=0;
    std::map<uint64_t,Transition> seen_,active_;
public:
    bool bind(const Origin& o,std::string& e){if(!valid_origin(o))return fail(e,"VIP Client origin invalid");*this={};origin_=o;e.clear();return true;}
    void reset(){*this={};}
    uint64_t committed()const{return committed_;}
    bool prepare(const Origin& o,const Batch& b,std::vector<Transition>& work,std::string& e)const{
        work.clear();if(o!=origin_||!valid_origin(o))return fail(e,"VIP Client origin differs");
        if(!valid(b,e))return false;
        if(b.after<floor_||b.after>committed_||b.after+b.rows.size()<committed_)
            return fail(e,"VIP batch loses retained/confirmed transition prefix");
        auto owners=active_;
        for(const auto& row:b.rows){
            if(row.sequence<=committed_){const auto it=seen_.find(row.sequence);
                if(it==seen_.end()||it->second!=row)return fail(e,"VIP conflicting duplicate transition");}
            else {if(!transition(owners,row,e))return false;work.push_back(row);}
        }
        e.clear();return true;
    }
    bool confirm(const Origin& o,const Batch& b,const std::vector<Transition>& applied,std::string& e){
        std::vector<Transition> expected;if(!prepare(o,b,expected,e))return false;
        if(applied!=expected)return fail(e,"VIP native applied transition receipt differs");
        for(const auto& row:expected){if(!transition(active_,row,e))return false;seen_.emplace(row.sequence,row);committed_=row.sequence;}
        floor_=b.after;seen_.erase(seen_.begin(),seen_.upper_bound(floor_));e.clear();return true;
    }
};
}
