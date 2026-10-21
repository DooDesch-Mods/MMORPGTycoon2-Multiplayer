#pragma once
#include <array>
#include <bit>
#include <optional>
#include "activation.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::live_host_state::network {
struct Row {
    int32_t region=-1,index=-1;
    std::array<float,4> values{};
    std::array<uint8_t,3> flags{};
    bool operator==(const Row&)const=default;
};
struct PipeRow {
    int32_t region=-1,index=-1;
    std::vector<float> current,smoothing;
    bool operator==(const PipeRow&)const=default;
};
struct ConnectionRow {
    std::array<int32_t,4> key{}; // Region, owner kind (Node0/Pipe1), owner index, static adjacency rank.
    std::array<float,7> values{}; // Pending, RGBA, reconnect timer, activity timer.
    bool operator==(const ConnectionRow&)const=default;
};
struct ManagerRow {int32_t region=-1;std::array<uint32_t,2> values{};bool operator==(const ManagerRow&)const=default;};
struct State {
    std::string other_graph_sha256;
    std::vector<Row> nodes;
    std::vector<PipeRow> pipes;
    std::vector<ConnectionRow> connections;
    std::optional<std::vector<ManagerRow>> managers;
    bool operator==(const State&)const=default;
};
inline bool exact_bits(const State& a,const State& b){
    if(a.managers!=b.managers||a.other_graph_sha256!=b.other_graph_sha256||a.nodes.size()!=b.nodes.size()||a.pipes.size()!=b.pipes.size()||a.connections.size()!=b.connections.size())return false;
    auto floats=[](const auto& left,const auto& right){if(left.size()!=right.size())return false;for(size_t i=0;i<left.size();++i)if(std::bit_cast<uint32_t>(left[i])!=std::bit_cast<uint32_t>(right[i]))return false;return true;};
    for(size_t i=0;i<a.nodes.size();++i)if(a.nodes[i].region!=b.nodes[i].region||a.nodes[i].index!=b.nodes[i].index||a.nodes[i].flags!=b.nodes[i].flags||!floats(a.nodes[i].values,b.nodes[i].values))return false;
    for(size_t i=0;i<a.pipes.size();++i)if(a.pipes[i].region!=b.pipes[i].region||a.pipes[i].index!=b.pipes[i].index||!floats(a.pipes[i].current,b.pipes[i].current)||!floats(a.pipes[i].smoothing,b.pipes[i].smoothing))return false;
    for(size_t i=0;i<a.connections.size();++i)if(a.connections[i].key!=b.connections[i].key||!floats(a.connections[i].values,b.connections[i].values))return false;
    return true;
}
inline bool valid_values(const State& state,bool finished,std::string& e){
    if(state.managers){int32_t prior=-1;for(const auto& manager:*state.managers){if(manager.region<=prior){e="network Manager RegionID order differs";return false;}prior=manager.region;for(auto bits:manager.values)if(!std::isfinite(std::bit_cast<float>(bits))){e="network Manager finite float32 required";return false;}}}
    if(state.other_graph_sha256.size()!=64){e="network state exact graph SHA required";return false;}
    for(char c:state.other_graph_sha256)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f'))){e="network state lowercase graph SHA required";return false;}
    int32_t region=-1,index=-1;
    for(const auto& row:state.nodes){
        if(row.region<0||row.index<0||row.region<region||(row.region==region?row.index!=index+1:row.index!=0)){
            e="network state requires actual sorted region-local node order";return false;}
        for(float value:row.values)if(!std::isfinite(value)){e="network state finite float32 required";return false;}
        for(auto flag:row.flags)if(flag>1){e="network state bool byte required";return false;}
        if(finished&&row.values[0]!=row.values[1]){e="network state Host sample precedes completed native Finish";return false;}
        region=row.region;index=row.index;
    }
    region=-1;index=-1;
    for(const auto& row:state.pipes){
        if(row.region<0||row.index<0||row.region<region||(row.region==region?row.index!=index+1:row.index!=0)){
            e="network state requires actual sorted region-local pipe order";return false;}
        if(row.current.size()>65536||row.current.empty()||(!row.smoothing.empty()&&row.smoothing.size()!=row.current.size())){
            e="network pipe array shape differs from original current/smoothing arrays";return false;}
        for(const auto* values:{&row.current,&row.smoothing})for(float value:*values)if(!std::isfinite(value)){
            e="network pipe finite float32 required";return false;}
        region=row.region;index=row.index;
    }
    std::array<int32_t,4> previous{};bool have=false;
    for(const auto& row:state.connections){
        for(auto value:row.key)if(value<0){e="network connection nonnegative key required";return false;}
        if(row.key[1]>1||(have&&row.key<=previous)){e="network connection sorted unique key required";return false;}
        if((!have||row.key[0]!=previous[0]||row.key[1]!=previous[1]||row.key[2]!=previous[2])?row.key[3]!=0:row.key[3]!=previous[3]+1){e="network connection consecutive owner-local rank required";return false;}
        for(float value:row.values)if(!std::isfinite(value)){e="network connection finite float32 required";return false;}
        previous=row.key;have=true;
    }
    e.clear();return true;
}
inline bool valid(const State& s,std::string& e){return valid_values(s,true,e);}
inline bool valid_action105_effect(const State& s,std::string& e){return valid_values(s,false,e);}
inline nlohmann::json encode(const State& s){
    auto rows=nlohmann::json::array();
    for(const auto& r:s.nodes)rows.push_back(nlohmann::json::array({r.region,r.index,r.values,r.flags}));
    auto pipes=nlohmann::json::array();
    for(const auto& p:s.pipes)pipes.push_back(nlohmann::json::array({p.region,p.index,p.current,p.smoothing}));
    auto connections=nlohmann::json::array();
    for(const auto& c:s.connections)connections.push_back(nlohmann::json::array({c.key,c.values}));
    nlohmann::json out={{"other_graph_sha256",s.other_graph_sha256},{"nodes",rows},{"pipes",pipes},{"connections",connections}};if(s.managers){auto managers=nlohmann::json::array();for(const auto& manager:*s.managers)managers.push_back({manager.region,manager.values});out["managers"]=std::move(managers);}return out;
}
inline bool decode(const nlohmann::json& j,State& out,std::string& e,bool action105_effect=false){
    out={};try{
        if(!j.is_object()||j.size()!=4+size_t(j.contains("managers"))||!j.contains("other_graph_sha256")||!j.contains("nodes")||!j.contains("pipes")||!j.contains("connections")||!j.at("other_graph_sha256").is_string()||!j.at("nodes").is_array()||!j.at("pipes").is_array()||!j.at("connections").is_array())throw std::runtime_error("network exact state fields required");
        State result;result.other_graph_sha256=j.at("other_graph_sha256").get<std::string>();
        auto id=[](const auto& v){if(!v.is_number_integer()||(v.is_number_unsigned()?v.template get<uint64_t>()>INT32_MAX:v.template get<int64_t>()<0||v.template get<int64_t>()>INT32_MAX))throw std::runtime_error("network nonnegative int32 key required");return v.template get<int32_t>();};
        if(j.contains("managers")){const auto& managers=j.at("managers");if(!managers.is_array())throw std::runtime_error("network complete Manager array required");result.managers.emplace();
            for(const auto& item:managers){if(!item.is_array()||item.size()!=2||!item[1].is_array()||item[1].size()!=2)throw std::runtime_error("network exact Manager row required");ManagerRow manager;manager.region=id(item[0]);
                for(size_t n=0;n<2;++n){const auto& word=item[1][n];if(!word.is_number_integer()||(word.is_number_integer()&&!word.is_number_unsigned()&&word.get<int64_t>()<0)||word.get<uint64_t>()>UINT32_MAX)throw std::runtime_error("network Manager float bits require uint32");manager.values[n]=word.get<uint32_t>();}result.managers->push_back(manager);}}
        for(const auto& row:j.at("nodes")){
            if(!row.is_array()||row.size()!=4||!row[2].is_array()||row[2].size()!=4||!row[3].is_array()||row[3].size()!=3)throw std::runtime_error("network exact row fields required");
            Row r;r.region=id(row[0]);r.index=id(row[1]);
            for(size_t i=0;i<4;++i){if(!row[2][i].is_number())throw std::runtime_error("network float value required");r.values[i]=row[2][i].get<float>();}
            for(size_t i=0;i<3;++i){const auto value=id(row[3][i]);if(value>1)throw std::runtime_error("network bool byte required");r.flags[i]=uint8_t(value);}
            result.nodes.push_back(r);
        }
        for(const auto& row:j.at("pipes")){
            if(!row.is_array()||row.size()!=4||!row[2].is_array()||!row[3].is_array()||row[2].size()>65536||row[3].size()>65536)throw std::runtime_error("network exact pipe row required");
            PipeRow p;p.region=id(row[0]);p.index=id(row[1]);
            for(size_t field=2;field<4;++field)for(const auto& value:row[field]){
                if(!value.is_number())throw std::runtime_error("network pipe float required");
                (field==2?p.current:p.smoothing).push_back(value.get<float>());
            }
            result.pipes.push_back(std::move(p));
        }
        for(const auto& row:j.at("connections")){
            if(!row.is_array()||row.size()!=2||!row[0].is_array()||row[0].size()!=4||!row[1].is_array()||row[1].size()!=7)throw std::runtime_error("network exact connection row required");
            ConnectionRow c;for(size_t i=0;i<4;++i)c.key[i]=id(row[0][i]);
            for(size_t i=0;i<7;++i){if(!row[1][i].is_number())throw std::runtime_error("network connection float required");c.values[i]=row[1][i].get<float>();}
            result.connections.push_back(c);
        }
        if(!(action105_effect?valid_action105_effect(result,e):valid(result,e)))return false;out=std::move(result);return true;
    }catch(const std::exception& x){e=std::string("network decode original=")+x.what();return false;}
}
struct ActivationReceipt {int32_t region=-1,index=-1;activation::Receipt event;};
struct PipeEffect {
    int32_t region=-1,index=-1;
    uint32_t resize_attempts=0,resizes=0,arrays_written=0;
    bool display_attempted=false,display_completed=false,queue_attempted=false,queue_completed=false;
};
struct Receipt {
    std::vector<PipeEffect> pipe_effects;
    std::vector<ActivationReceipt> activations;
    bool attempted=false,confirmed=false,unknown=false;
    uint64_t attempted_stores=0,completed_stores=0,notification_attempts=0,notifications=0;
    uint64_t pipe_attempts=0,pipes_completed=0,connection_attempts=0,connection_stores=0;
    State actual;bool actual_present=false;std::string cause;
};
inline nlohmann::json encode_receipt(const Receipt& r){
    nlohmann::json out={{"attempted",r.attempted},{"confirmed",r.confirmed},{"unknown",r.unknown},
        {"attempted_stores",r.attempted_stores},{"completed_stores",r.completed_stores},
        {"notification_attempts",r.notification_attempts},{"notifications",r.notifications},
        {"connection_attempts",r.connection_attempts},{"connection_stores",r.connection_stores},{"pipe_attempts",r.pipe_attempts},{"pipes_completed",r.pipes_completed},{"actual_present",r.actual_present},{"cause",r.cause}};
    out["activations"]=nlohmann::json::array();
    for(const auto& row:r.activations){const auto& a=row.event;out["activations"].push_back({{"region",row.region},{"index",row.index},
        {"attempted",a.attempted},{"flag_written",a.flag_written},{"constructed",a.constructed},{"send_attempted",a.send_attempted},{"sent",a.sent},
        {"cleanup_attempted",a.cleanup_attempted},{"cleaned",a.cleaned},{"confirmed",a.confirmed},{"unknown",a.unknown},
        {"dispatch_enabled",a.dispatch_enabled},{"seh_code",a.seh_code},{"cause",a.cause}});}
    out["pipe_effects"]=nlohmann::json::array();
    for(const auto& p:r.pipe_effects)out["pipe_effects"].push_back({{"region",p.region},{"index",p.index},
        {"resize_attempts",p.resize_attempts},{"resizes",p.resizes},{"arrays_written",p.arrays_written},
        {"display_attempted",p.display_attempted},{"display_completed",p.display_completed},
        {"queue_attempted",p.queue_attempted},{"queue_completed",p.queue_completed}});
    if(r.actual_present)out["actual"]=encode(r.actual);return out;
}
struct Port {
    virtual ~Port()=default;
    virtual bool read(State&,std::string&)=0;
    // Only native providers with a fresh Client/Clock0 census can confirm no effect.
    virtual std::optional<Receipt> confirm_no_effect(const State&,const State&){return std::nullopt;}
    // The native implementation pins all current owners and the actual Client/Clock0 boundary.
    virtual bool preflight(const State& before,const State& target,std::string&)=0;
    virtual bool begin_scalar_stores(std::string&){return true;}
    virtual void end_scalar_stores()noexcept{}
    virtual activation::Receipt activate(size_t){activation::Receipt r;r.cause="native activation service absent";return r;}
    virtual bool store(size_t node,size_t scalar,uint32_t expected,uint32_t target,std::string&)=0;
    virtual bool store_pipe(size_t,const PipeRow&,const PipeRow&,PipeEffect&,std::string& e){e="native pipe service absent";return false;}
    virtual bool store_connection(size_t,size_t,uint32_t,uint32_t,std::string& e){e="native connection service absent";return false;}
    virtual bool store_manager(size_t,size_t,uint32_t,uint32_t,std::string& e){e="native Manager scalar service absent";return false;}
    virtual bool notify_supply(size_t node,std::string&)=0;
};
Receipt apply(Port&,const State& target);
}
