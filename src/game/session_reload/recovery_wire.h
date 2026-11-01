#pragma once
#include "recovery_protocol.h"
#include "mt2mp/net/snapshot_world_path.h"
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::game::session_reload::recovery {
using Json=nlohmann::json;
enum class Kind:uint64_t {Begin=1,Held=2,Published=3,Retired=4,Failed=5};
struct Packet {
    Kind kind=Kind::Begin;uint64_t session=0,sender=0,process=0,interruption=0,lease=0,world=0,generation=0,tick=0,retirement=0;
    std::string token,cause;Cohort original{},survivors{};Json file=nullptr;
};
inline std::string hex(const unsigned char (&bytes)[32]){
    constexpr char digits[]="0123456789abcdef";std::string value;value.reserve(64);
    for(auto byte:bytes){value+=digits[byte>>4];value+=digits[byte&15];}return value;
}
inline bool hash(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos&&value.find_first_not_of('0')!=std::string::npos;}
inline bool digest(const std::string& text,unsigned char (&out)[32]){
    if(!hash(text))return false;const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
    for(size_t i=0;i<32;++i)out[i]=static_cast<unsigned char>((nibble(text[2*i])<<4)|nibble(text[2*i+1]));return true;
}
inline bool file_valid(const Json& file){
    if(!file.is_object()||file.size()!=7)return false;
    for(const char* key:{"snapshot_id","world_name","sha256","companion_sha256","witness_sha256"})if(!file.contains(key)||!file[key].is_string())return false;
    for(const char* key:{"bytes","companion_bytes"})if(!file.contains(key)||!file[key].is_number_unsigned()||file[key].get<uint64_t>()==0)return false;
    return nonce(file["snapshot_id"].get<std::string>())&&net::snapshot_world_path::valid_folder(file["world_name"].get<std::string>())&&
        hash(file["sha256"].get<std::string>())&&hash(file["companion_sha256"].get<std::string>())&&hash(file["witness_sha256"].get<std::string>());
}
inline bool valid(const Packet& p){
    uint8_t original=0,survivors=0;
    if(!p.session||p.sender<1||p.sender>4||!p.process||!p.interruption||!p.lease||!p.world||!p.generation||!nonce(p.token)||
       !valid(p.original,original)||!valid(p.survivors,survivors)||!(original&1)||!(survivors&1)||!(survivors&bit(p.sender))||
       (survivors&original)!=survivors||survivors==original||p.process!=p.survivors.process_nonces[p.sender-1])return false;
    for(size_t i=0;i<4;++i)if(p.survivors.physical_ids[i]&&(p.survivors.physical_ids[i]!=p.original.physical_ids[i]||p.survivors.process_nonces[i]!=p.original.process_nonces[i]))return false;
    if(p.kind<Kind::Begin||p.kind>Kind::Failed||((p.kind==Kind::Begin||p.kind==Kind::Published)&&p.sender!=1))return false;
    if((p.kind==Kind::Retired)!=(p.retirement!=0))return false;
    if(p.kind==Kind::Failed){if(p.cause.empty()||p.cause.size()>512)return false;}else if(!p.cause.empty())return false;
    return p.kind==Kind::Published?file_valid(p.file):p.file.is_null();
}
inline Json encode(const Packet& p){
    if(!valid(p))throw std::runtime_error("Recovery packet has invalid original process/attempt evidence");
    return {{"recovery",uint64_t(1)},{"kind",uint64_t(p.kind)},{"session",p.session},{"sender",p.sender},{"process",p.process},
        {"interruption",p.interruption},{"lease",p.lease},{"world",p.world},{"generation",p.generation},{"tick",p.tick},{"retirement",p.retirement},
        {"token",p.token},{"original",{{"physical",p.original.physical_ids},{"process",p.original.process_nonces}}},
        {"survivors",{{"physical",p.survivors.physical_ids},{"process",p.survivors.process_nonces}}},{"file",p.file},{"cause",p.cause}};
}
inline bool decode(const Json& value,Packet& out,std::string& error)try{
    out={};
    if(!value.is_object()||value.size()!=16)throw std::runtime_error("Recovery packet exact field count differs");
    for(const char* field:{"recovery","kind","session","sender","process","interruption","lease","world","generation","tick","retirement"})
        if(!value.contains(field)||!value[field].is_number_unsigned())throw std::runtime_error("Recovery packet integer field differs");
    if(value["recovery"]!=1||!value.at("token").is_string()||!value.at("cause").is_string())throw std::runtime_error("Recovery packet revision/text differs");
    const auto cohort=[](const Json& source,Cohort& target){
        if(!source.is_object()||source.size()!=2)throw std::runtime_error("Recovery cohort exact fields differ");
        for(const char* field:{"physical","process"}){
            if(!source.at(field).is_array()||source.at(field).size()!=4)throw std::runtime_error("Recovery cohort requires all four slots");
            for(size_t i=0;i<4;++i){if(!source[field][i].is_number_unsigned())throw std::runtime_error("Recovery process identity is not uint64");
                (field[0]=='p'&&field[1]=='h'?target.physical_ids:target.process_nonces)[i]=source[field][i].get<uint64_t>();}
        }
    };
    out.kind=Kind(value["kind"].get<uint64_t>());out.session=value["session"];out.sender=value["sender"];out.process=value["process"];
    out.interruption=value["interruption"];out.lease=value["lease"];out.world=value["world"];out.generation=value["generation"];out.tick=value["tick"];out.retirement=value["retirement"];
    out.token=value["token"];out.cause=value["cause"];out.file=value.at("file");cohort(value.at("original"),out.original);cohort(value.at("survivors"),out.survivors);
    if(!valid(out))throw std::runtime_error("Recovery packet original process/attempt evidence differs");
    error.clear();return true;
}catch(const std::exception& cause){error=cause.what();return false;}
inline bool same_attempt(const Packet& a,const Packet& b){return a.session==b.session&&a.token==b.token&&same(a.original,b.original)&&same(a.survivors,b.survivors);}
}
