#pragma once
#include "vendor/nlohmann/json.hpp"
#include "../network_object_identity.h"
#include "../subscriber_lifecycle/record_graph.h"
#include <bit>
#include <cmath>
namespace mt2mp::action_session {
struct LogoIntent {std::string before_sha256,desired_sha256;nlohmann::json records=nlohmann::json::array();bool operator==(const LogoIntent&)const=default;};
struct LogoState {bool present=false;std::string sha256;bool operator==(const LogoState&)const=default;};
inline bool logo_records(const nlohmann::json& value,std::vector<subscriber_lifecycle::Record>& out,std::string& error)try{
    out.clear();if(!value.is_array()||value.empty()||value.size()>2048){error="Logo detached Record resource budget differs";return false;}
    size_t text=0,tokens=0;std::vector<size_t> children(value.size());
    auto token=[&](const nlohmann::json& j,subscriber_lifecycle::Token& out){
        if(!j.is_array()||j.size()!=3||!j[0].is_number_integer()||!j[1].is_number_unsigned()||!j[2].is_string())return false;
        const auto type=j[0].get<int64_t>();const auto bits=j[1].get<uint64_t>();if(type<0||type>4||bits>UINT32_MAX)return false;
        out={int32_t(type),uint32_t(bits),j[2].get<std::string>()};text+=out.text.size();
        return out.text.find('\0')==std::string::npos&&out.text.size()<=8192&&text<=65536&&
            ((type==1||type==2)?bits==0:out.text.empty())&&(type!=3||std::isfinite(std::bit_cast<float>(out.bits)));};
    for(size_t index=0;index<value.size();++index){const auto& j=value[index];subscriber_lifecycle::Record row;
        if(!j.is_array()||j.size()!=4||!j[0].is_number_integer()||!j[1].is_number_unsigned()||!j[3].is_array()||!token(j[2],row.label)){error="Logo detached Record exact row/token fields differ";return false;}
        const auto parent=j[0].get<int64_t>();row.child_index=j[1].get<uint64_t>();
        if((!index&&(parent!=-1||row.child_index))||(index&&(parent<0||uint64_t(parent)>=index||row.child_index!=children[size_t(parent)]++))){error="Logo detached Record ownership/order invalid";return false;}
        row.parent=index?size_t(parent):SIZE_MAX;tokens+=j[3].size();if(tokens>16384){error="Logo detached token budget exceeded";return false;}
        for(const auto& t:j[3]){subscriber_lifecycle::Token item;if(!token(t,item)){error="Logo detached token value invalid";return false;}row.tokens.push_back(std::move(item));}out.push_back(std::move(row));
    }
    if(out[0].label.type!=1||out[0].label.text!="mmoLogo"){error="Logo detached root is not original mmoLogo";return false;}error.clear();return true;
}catch(const std::exception& e){error=std::string("Logo detached value decode: ")+e.what();return false;}
inline bool valid_logo(const LogoIntent& intent){std::vector<subscriber_lifecycle::Record> rows;std::string error;return game::network_object_identity::sha(intent.before_sha256)&&game::network_object_identity::sha(intent.desired_sha256)&&logo_records(intent.records,rows,error);}
inline bool valid_logo(const LogoState& state){return game::network_object_identity::sha(state.sha256);}
}
