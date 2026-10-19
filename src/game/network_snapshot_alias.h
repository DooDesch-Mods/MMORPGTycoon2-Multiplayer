#pragma once
#include "network_object_identity.h"
#include "vendor/nlohmann/json.hpp"
#include <tuple>
namespace mt2mp::game::network_snapshot_alias {
using Kind=network_object_identity::Kind;
struct Row {Kind kind=Kind::Node;int32_t link=0;std::string key;bool operator==(const Row&)const=default;};
struct Manifest {bool present=false;std::vector<Row> rows;std::set<std::string> retired;bool operator==(const Manifest&)const=default;};
inline bool valid(const Manifest& m,std::string& e){
    if(!m.present&&(!m.rows.empty()||!m.retired.empty())){e="Network aliases outside promised manifest";return false;}
    if(m.rows.size()>131072||m.retired.size()>131072){e="Network alias manifest resource budget exceeded";return false;}
    std::set<int32_t> links;std::set<std::string> keys;std::pair<uint8_t,int32_t> previous{};bool first=true;
    for(const auto& r:m.rows){const auto order=std::pair{uint8_t(r.kind),r.link};
        if((r.kind!=Kind::Node&&r.kind!=Kind::Pipe)||r.link<=0||!network_object_identity::key(r.key)||r.key.find(std::string(":network:")+network_object_identity::name(r.kind))==std::string::npos||
           (!first&&order<=previous)||!links.insert(r.link).second||!keys.insert(r.key).second||m.retired.contains(r.key)){e="Network alias link/kind/key duplicate, unsorted or retired";return false;}
        previous=order;first=false;
    }
    for(const auto& id:m.retired)if(!network_object_identity::key(id)){e="Network alias tombstone key invalid";return false;}
    e.clear();return true;
}
inline nlohmann::json encode(const Manifest& m){auto rows=nlohmann::json::array();for(const auto& r:m.rows)rows.push_back({{"kind",network_object_identity::name(r.kind)},{"link",r.link},{"key",r.key}});return {{"rows",rows},{"retired",m.retired}};}
inline bool decode(const nlohmann::json& j,Manifest& out,std::string& e)try{
    out={};if(!j.is_object()||j.size()!=2||!j.at("rows").is_array()||j.at("rows").size()>131072||!j.at("retired").is_array()||j.at("retired").size()>131072){e="Network alias exact manifest fields/bounds invalid";return false;}
    Manifest m;m.present=true;for(const auto& r:j.at("rows")){
        if(!r.is_object()||r.size()!=3||!r.at("kind").is_string()||!r.at("link").is_number_integer()||r.at("link").get<int64_t>()<=0||r.at("link").get<uint64_t>()>INT32_MAX){e="Network alias exact row fields invalid";return false;}
        const auto kind=r.at("kind").get<std::string>();if(kind!="node"&&kind!="pipe"){e="Network alias original object kind invalid";return false;}
        m.rows.push_back({kind=="node"?Kind::Node:Kind::Pipe,r.at("link").get<int32_t>(),r.at("key").get<std::string>()});
    }
    for(const auto& id:j.at("retired")){const auto text=id.get<std::string>();if(!m.retired.insert(text).second){e="Network alias duplicate tombstone";return false;}}
    if(!valid(m,e))return false;out=std::move(m);e.clear();return true;
}catch(const std::exception& x){e=std::string("Network alias decode: ")+x.what();return false;}
}
