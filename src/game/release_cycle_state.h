#pragma once
#include "release_edit_state.h"
#include "release_graph.h"
#include "general_project_codec.h"
namespace mt2mp::game::release_cycle {
enum class Command:uint8_t {Show=1,Begin=2,Abort=3,Skip=4,Finalize=5};
enum class Phase:uint8_t {Idle=0,Announcement=1,Playing=2,AwaitPublish=3};
inline bool uuid(const std::string& value){if(value.size()!=36)return false;for(size_t n=0;n<36;++n){const auto c=value[n];if((n==8||n==13||n==18||n==23)?c!='-':!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}return true;}
struct Intent {Command command=Command::Show;std::string cycle;bool operator==(const Intent&)const=default;};
inline bool valid(const Intent& value){return value.command>=Command::Show&&value.command<=Command::Finalize&&uuid(value.cycle);}
inline bool phase(Phase value,const std::string& cycle){return value>=Phase::Idle&&value<=Phase::AwaitPublish&&(value==Phase::Idle?cycle.empty():uuid(cycle));}
struct Publication {
    std::string cycle;Phase phase=Phase::Idle;release_graph::Value graph,prepared;std::array<uint64_t,4> random{};
    bool operator==(const Publication&)const=default;
};
inline bool valid(const Publication& value,std::string& e){if(!phase(value.phase,value.cycle)||value.graph.kind!=release_graph::Kind::Manager||value.prepared.kind!=release_graph::Kind::PreparedDemands){e="Release publication active-cycle/phase/typed value domain invalid";return false;}return release_graph::valid(value.graph,e)&&release_graph::valid(value.prepared,e);}
inline nlohmann::json encode(const Publication& value){return {{"cycle",value.cycle},{"phase",uint8_t(value.phase)},{"graph",release_graph::encode(value.graph)},{"prepared",release_graph::encode(value.prepared)},{"random",value.random}};}
struct Proof {
    std::string cycle,graph_sha256;Phase phase=Phase::Idle;
    std::array<int32_t,3> version{},planned{};
    int32_t type=0,state=0,history=0,demands=0,changes=0,features=0,allocated_funds=0,free_points=0;
    std::array<uint64_t,4> random{},predicate_random{};bool allowed=true;
    bool operator==(const Proof&)const=default;
};
inline bool valid(const Proof& value){
    if(!phase(value.phase,value.cycle)||!release_edit::digest(value.graph_sha256)||value.type<0||value.type>4||value.state<0||value.state>3||value.allocated_funds<0||value.free_points<0)return false;
    for(const auto& row:{value.version,value.planned})for(auto n:row)if(n<0)return false;
    for(auto n:{value.history,value.demands,value.changes,value.features})if(n<0||n>4096)return false;return true;
}
template<class Hash>bool proof(const release_edit::State& scalar,const Publication& publication,int32_t free_points,Hash&& hash,Proof& out,std::string& e){
    if(!release_edit::valid(scalar)||!valid(publication,e)||scalar.random!=publication.random){if(e.empty())e="Release scalar/complete graph publication RNG differs";return false;}
    out={};out.cycle=publication.cycle;out.phase=publication.phase;out.graph_sha256=hash(nlohmann::json::array({release_graph::encode(publication.graph),release_graph::encode(publication.prepared)}).dump());
    out.version=scalar.version;out.planned=scalar.planned;out.type=scalar.type;out.state=scalar.state;out.history=scalar.history;out.demands=scalar.demands;out.changes=scalar.changes;out.features=scalar.features;out.allocated_funds=scalar.allocated_funds;out.free_points=free_points;out.random=scalar.random;out.predicate_random=scalar.random;
    if(!valid(out)){e="Release compact actual whole-value proof invalid";return false;}e.clear();return true;
}
template<class Hash>bool publication_matches(const Proof& proof,const Publication& value,Hash&& hash,std::string& e){
    if(!valid(proof)||!valid(value,e)){if(e.empty())e="Release Host after proof/publication is invalid";return false;}
    const auto digest=hash(nlohmann::json::array({release_graph::encode(value.graph),release_graph::encode(value.prepared)}).dump());
    if(proof.cycle!=value.cycle||proof.phase!=value.phase||proof.random!=value.random||proof.graph_sha256!=digest){e="Release Host after graph/prepared/cycle/phase/RNG differs from ordered proof";return false;}
    e.clear();return true;
}
inline bool before(const Intent& intent,const Proof& value,std::string& e){
    if(!valid(intent)||!valid(value)){e="Release original cycle intent/before value invalid";return false;}
    if(intent.command==Command::Show){if(value.phase!=Phase::Idle){e="Release original announcement already active";return false;}}
    else if(value.cycle!=intent.cycle||value.phase==Phase::Idle){e="Release command does not name the actual active cycle";return false;}
    else if(intent.command==Command::Finalize){if(value.phase!=Phase::AwaitPublish){e="Release finalize requires retained original finish/skip/immediate boundary";return false;}}
    else if(intent.command==Command::Begin){if(value.phase!=Phase::Announcement){e="Release Begin requires original announcement phase";return false;}}
    else if(value.phase==Phase::AwaitPublish){e="Release already awaits its ordered original finalize";return false;}
    e.clear();return true;
}
inline bool effect(const Intent& intent,const Proof& a,const Proof& b,std::string& e){
    if(!before(intent,a,e)||!valid(b)){if(e.empty())e="Release original cycle after value invalid";return false;}
    if(intent.command==Command::Finalize){
        const auto points=a.type==3?4:2;
        if(a.history==4096||b.history!=a.history+1||b.version!=a.planned||a.free_points>INT32_MAX-points||b.free_points!=a.free_points+points||b.phase!=Phase::Idle||!b.cycle.empty()||!b.allowed){e="Release original publish version/history/statistics/terminal cycle differs";return false;}
    }else{
        if(a.version!=b.version||a.history!=b.history||a.planned!=b.planned||a.type!=b.type||a.state!=b.state||a.allocated_funds!=b.allocated_funds||a.free_points!=b.free_points||a.changes!=b.changes||a.features!=b.features||a.demands!=b.demands){e="Release pre-publication command changed version/history/type/state/funds/counters";return false;}
        if(intent.command==Command::Show){
            if(b.allowed){if(b.cycle!=intent.cycle||b.phase!=(a.type==4?Phase::AwaitPublish:Phase::Announcement)){e="Release original accepted Show phase/cycle differs";return false;}}
            else if(b.phase!=Phase::Idle||!b.cycle.empty()||a.graph_sha256!=b.graph_sha256||b.random!=b.predicate_random){e="Release denied original predicate changed authored values/cycle or unrecorded RNG";return false;}
        }else{
            if(!b.allowed||a.graph_sha256!=b.graph_sha256||a.changes!=b.changes||a.features!=b.features||a.demands!=b.demands){e="Release original presentation command changed owned gameplay values";return false;}
            const auto target=intent.command==Command::Abort?Phase::Idle:intent.command==Command::Skip?Phase::AwaitPublish:(a.type==2||a.type==3?Phase::Playing:Phase::AwaitPublish);
            if(b.phase!=target||(target==Phase::Idle?!b.cycle.empty():b.cycle!=intent.cycle)){e="Release original presentation command phase/cycle differs";return false;}
        }
    }
    e.clear();return true;
}
inline nlohmann::json encode(const Intent& value){return {{"command",uint8_t(value.command)},{"cycle",value.cycle}};}
inline nlohmann::json encode(const Proof& value){return {{"cycle",value.cycle},{"phase",uint8_t(value.phase)},{"graph_sha256",value.graph_sha256},{"version",value.version},{"planned",value.planned},{"type",value.type},{"state",value.state},{"history",value.history},{"demands",value.demands},{"changes",value.changes},{"features",value.features},{"allocated_funds",value.allocated_funds},{"free_points",value.free_points},{"random",value.random},{"predicate_random",value.predicate_random},{"allowed",value.allowed}};}
namespace codec {
using general_project::codec::shape;using general_project::codec::i32;using general_project::codec::boolean;
inline Phase phase(const nlohmann::json& source){const auto n=i32(source);if(n<0||n>3)throw std::runtime_error("Release phase outside0..3");return Phase(n);}
inline std::string string(const nlohmann::json& source){if(!source.is_string())throw std::runtime_error("Release exact string required");auto out=source.get<std::string>();if(out.size()>64||out.find('\0')!=std::string::npos)throw std::runtime_error("Release identity/hash string domain invalid");return out;}
template<size_t N>inline void random(const nlohmann::json& source,std::array<uint64_t,N>& out){if(!source.is_array()||source.size()!=N)throw std::runtime_error("Release exact RNG state array required");for(size_t n=0;n<N;++n){const auto& value=source[n];if(!value.is_number_integer()||(!value.is_number_unsigned()&&value.get<int64_t>()<0))throw std::runtime_error("Release RNG state requires exact uint64");out[n]=value.get<uint64_t>();}}
inline Intent intent(const nlohmann::json& source){shape(source,{"command","cycle"});const auto n=i32(source.at("command"));if(n<1||n>5)throw std::runtime_error("Release command outside1..5");Intent out{Command(n),string(source.at("cycle"))};if(!valid(out))throw std::runtime_error("Release command/cycle domain invalid");return out;}
inline Publication publication(const nlohmann::json& source){shape(source,{"cycle","phase","graph","prepared","random"});Publication out;out.cycle=string(source.at("cycle"));out.phase=phase(source.at("phase"));out.graph=release_graph::decode(source.at("graph"));out.prepared=release_graph::decode(source.at("prepared"));random(source.at("random"),out.random);std::string e;if(!valid(out,e))throw std::runtime_error(e);return out;}
inline Proof proof(const nlohmann::json& source){shape(source,{"cycle","phase","graph_sha256","version","planned","type","state","history","demands","changes","features","allocated_funds","free_points","random","predicate_random","allowed"});Proof out;
    out.cycle=string(source.at("cycle"));out.phase=phase(source.at("phase"));out.graph_sha256=string(source.at("graph_sha256"));out.allowed=boolean(source.at("allowed"));
    for(const char* name:{"version","planned"}){const auto& values=source.at(name);if(!values.is_array()||values.size()!=3)throw std::runtime_error("Release exact version triple required");auto& target=std::string_view(name)=="version"?out.version:out.planned;for(size_t n=0;n<3;++n)target[n]=i32(values[n]);}
    out.type=i32(source.at("type"));out.state=i32(source.at("state"));out.history=i32(source.at("history"));out.demands=i32(source.at("demands"));out.changes=i32(source.at("changes"));out.features=i32(source.at("features"));out.allocated_funds=i32(source.at("allocated_funds"));out.free_points=i32(source.at("free_points"));random(source.at("random"),out.random);random(source.at("predicate_random"),out.predicate_random);
    if(!valid(out))throw std::runtime_error("Release compact proof domain invalid");return out;
}
}
}
