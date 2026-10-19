#pragma once
#include "subscriber_lifecycle/record_graph.h"
#include "subscriber_lifecycle/release_record_kind.h"
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <set>
#include <map>
#include <bit>
#include <cmath>
namespace mt2mp::game::release_graph {
namespace capture=subscriber_lifecycle;
constexpr size_t maximum_records=4096,maximum_tokens=32768,maximum_text=262144,maximum_links=4096;
struct External {int32_t id=0;std::string key;bool operator==(const External&)const=default;};
using Kind=capture::ReleaseRecordKind;
struct Value {std::vector<capture::Record> records;std::vector<External> external;Kind kind=Kind::Manager;bool operator==(const Value&)const;};
inline bool text(const std::string& value){return value.size()<=maximum_text&&value.find('\0')==std::string::npos;}
inline bool token(const capture::Token& value){
    if(!text(value.text))return false;
    // Preserve the engine's scalar token kinds. Only active data is canonical.
    if(value.type==1||value.type==2)return value.bits==0;
    if(value.type==4)return value.text.empty();
    if(value.type==3)return value.text.empty()&&std::isfinite(std::bit_cast<float>(value.bits));
    return value.type==0&&value.bits==0&&value.text.empty();
}
inline bool valid(const Value& value,std::string& e){
    if((value.kind!=Kind::Manager&&value.kind!=Kind::PreparedDemands&&value.kind!=Kind::GameMasterTickets)||value.records.empty()||value.records.size()>maximum_records||value.external.size()>maximum_links){e="Original typed Record graph kind/extent invalid";return false;}
    std::vector<size_t> children(value.records.size());size_t tokens=0,bytes=0;std::set<int32_t> ids;std::set<std::string> keys;
    for(size_t i=0;i<value.records.size();++i){const auto& row=value.records[i];
        const auto root=capture::release_record_root(value.kind);
        const auto reject=[&](const char* reason,size_t expected_child){
            e="Release original Record typed root/parent/child order invalid: reason="+std::string(reason)+
                " kind="+std::to_string(uint8_t(value.kind))+" records="+std::to_string(value.records.size())+
                " row="+std::to_string(i)+" parent="+(row.parent==SIZE_MAX?std::string("root"):std::to_string(row.parent))+
                " child="+std::to_string(row.child_index)+" expected_child="+std::to_string(expected_child)+
                " label_type="+std::to_string(row.label.type)+" label_bits="+std::to_string(row.label.bits)+
                " label_bytes="+std::to_string(row.label.text.size())+" label="+nlohmann::json(row.label.text.substr(0,96)).dump(-1,' ',false,nlohmann::json::error_handler_t::replace)+
                " expected_root="+root;return false;
        };
        if(i==0&&(row.parent!=SIZE_MAX||row.child_index!=0))return reject("root ancestry",0);
        if(i==0&&(row.label.type!=1||row.label.text!=root))return reject("typed root",0);
        if(i&&row.parent>=i)return reject("parent must precede child",0);
        if(i&&row.child_index!=children[row.parent])return reject("child order",children[row.parent]);
        if(row.label.type>2||!token(row.label))return reject("label scalar",i?children[row.parent]:0);
        if(i)++children[row.parent];
        tokens+=row.tokens.size()+1;bytes+=row.label.text.size();for(const auto& t:row.tokens){if(!token(t)){e="Release original Record scalar kind/active value invalid";return false;}bytes+=t.text.size();}
        if(tokens>maximum_tokens||bytes>maximum_text){e="Release original Record token/text budget exceeded";return false;}
        if(row.label.text=="linkId"&&(row.label.type!=1||row.tokens.size()!=1||row.tokens[0].type!=4||!row.tokens[0].bits||row.tokens[0].bits>INT32_MAX||!ids.insert(int32_t(row.tokens[0].bits)).second)){e="Release inline linkId definition invalid/duplicate";return false;}
    }
    for(const auto& link:value.external)if(link.id<=0||link.key.empty()||link.key.size()>512||!text(link.key)||!ids.insert(link.id).second||!keys.insert(link.key).second){e="Release original external link identity duplicated/invalid";return false;}
    e.clear();return true;
}
inline std::vector<int32_t> inline_ids(const Value& value){std::vector<int32_t> out;for(const auto& row:value.records)if(row.label.text=="linkId")out.push_back(int32_t(row.tokens[0].bits));return out;}
inline nlohmann::json encode(const Value& value){auto rows=nlohmann::json::array(),links=nlohmann::json::array();
    const auto scalar=[](const capture::Token& t){return nlohmann::json::array({t.type,t.bits,t.text});};
    for(const auto& row:value.records){auto tokens=nlohmann::json::array();for(const auto& t:row.tokens)tokens.push_back(scalar(t));rows.push_back(nlohmann::json::array({row.parent==SIZE_MAX?int64_t(-1):int64_t(row.parent),row.child_index,scalar(row.label),tokens}));}
    for(const auto& link:value.external)links.push_back(nlohmann::json::array({link.id,link.key}));return {{"kind",uint8_t(value.kind)},{"records",rows},{"external",links}};
}
inline Value decode(const nlohmann::json& source){
    if(!source.is_object()||source.size()!=3||!source.contains("kind")||!source.contains("records")||!source.contains("external"))throw std::runtime_error("Release graph requires exact kind/records/external fields");
    const auto& rows=source.at("records");const auto& links=source.at("external");
    if(!rows.is_array()||rows.empty()||rows.size()>maximum_records||!links.is_array()||links.size()>maximum_links)throw std::runtime_error("Release graph wire extent invalid");
    const auto integer=[](const nlohmann::json& j,int64_t low,int64_t high){
        if(!j.is_number_integer()||(j.is_number_unsigned()&&j.get<uint64_t>()>uint64_t(INT64_MAX)))throw std::runtime_error("Release graph exact integer required");
        const auto v=j.get<int64_t>();if(v<low||v>high)throw std::runtime_error("Release graph integer outside domain");return v;
    };
    const auto scalar=[&](const nlohmann::json& j){
        if(!j.is_array()||j.size()!=3||!j[2].is_string())throw std::runtime_error("Release graph exact scalar triple required");
        capture::Token out{int32_t(integer(j[0],0,4)),uint32_t(integer(j[1],0,UINT32_MAX)),j[2].get<std::string>()};
        if(!token(out))throw std::runtime_error("Release graph active scalar value invalid");return out;
    };
    Value out;out.kind=Kind(integer(source.at("kind"),0,2));size_t count=0,bytes=0;
    for(const auto& row:rows){
        if(!row.is_array()||row.size()!=4||!row[3].is_array())throw std::runtime_error("Release graph exact Record tuple required");
        capture::Record result;const auto parent=integer(row[0],-1,int64_t(maximum_records-1));result.parent=parent<0?SIZE_MAX:size_t(parent);
        result.child_index=size_t(integer(row[1],0,int64_t(maximum_records-1)));result.label=scalar(row[2]);
        count+=row[3].size()+1;bytes+=result.label.text.size();if(count>maximum_tokens)throw std::runtime_error("Release graph token budget exceeded");
        for(const auto& value:row[3]){auto t=scalar(value);bytes+=t.text.size();if(bytes>maximum_text)throw std::runtime_error("Release graph text budget exceeded");result.tokens.push_back(std::move(t));}
        out.records.push_back(std::move(result));
    }
    for(const auto& link:links){
        if(!link.is_array()||link.size()!=2||!link[1].is_string())throw std::runtime_error("Release graph exact external link pair required");
        out.external.push_back({int32_t(integer(link[0],1,INT32_MAX)),link[1].get<std::string>()});
    }
    std::string e;if(!valid(out,e))throw std::runtime_error(e);return out;
}
inline bool same(const Value& a,const Value& b){return encode(a)==encode(b);}
inline bool Value::operator==(const Value& other)const{return same(*this,other);}
template<class Key>bool capture_value(const capture::Graph& source,Key&& key,Value& out,std::string& e,Kind kind=Kind::Manager){
    out={};out.kind=kind;out.records=source.records;std::set<int32_t> ids;
    for(const auto& link:source.private_links){if(link.saved_link_id<=0||!link.object||!ids.insert(link.saved_link_id).second){e="Release actual private context link pointer/ID duplicate";return false;}
        if(link.has_inline_definition)continue;std::string portable;if(!key(link.object,portable,e)){if(e.empty())e="Release external owner has no current canonical binding";return false;}out.external.push_back({link.saved_link_id,std::move(portable)});}
    std::sort(out.external.begin(),out.external.end(),[](const auto& a,const auto& b){return a.id<b.id;});return valid(out,e);
}
}
