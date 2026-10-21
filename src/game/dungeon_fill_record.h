#pragma once
#include "vendor/nlohmann/json.hpp"
#include "subscriber_lifecycle/record_graph.h"
#include <bit>
#include <cmath>
namespace mt2mp::game::dungeon_fill {
inline bool records(const nlohmann::json& value,std::vector<subscriber_lifecycle::Record>& out,std::string& error)try{
    out.clear();if(!value.is_array()||value.empty()||value.size()>2048){error="Dungeon Fill detached Record resource budget differs";return false;}
    size_t text=0,tokens=0;std::vector<size_t> children(value.size());
    auto token=[&](const nlohmann::json& j,subscriber_lifecycle::Token& out){
        if(!j.is_array()||j.size()!=3||!j[0].is_number_integer()||!j[1].is_number_unsigned()||!j[2].is_string())return false;
        const auto type=j[0].get<int64_t>();const auto bits=j[1].get<uint64_t>();if(type<0||type>4||bits>UINT32_MAX)return false;
        out={int32_t(type),uint32_t(bits),j[2].get<std::string>()};text+=out.text.size();
        return out.text.find('\0')==std::string::npos&&out.text.size()<=8192&&text<=65536&&
            ((type==1||type==2)?bits==0:out.text.empty())&&(type!=3||std::isfinite(std::bit_cast<float>(out.bits)));};
    for(size_t index=0;index<value.size();++index){const auto& j=value[index];subscriber_lifecycle::Record row;
        if(!j.is_array()||j.size()!=4||!j[0].is_number_integer()||!j[1].is_number_unsigned()||!j[3].is_array()||!token(j[2],row.label)){error="Dungeon Fill detached Record exact row/token fields differ";return false;}
        const auto parent=j[0].get<int64_t>();row.child_index=j[1].get<uint64_t>();
        if((!index&&(parent!=-1||row.child_index))||(index&&(parent<0||uint64_t(parent)>=index||row.child_index!=children[size_t(parent)]++))){error="Dungeon Fill detached Record ownership/order invalid";return false;}
        row.parent=index?size_t(parent):SIZE_MAX;tokens+=j[3].size();if(tokens>16384){error="Dungeon Fill detached token budget exceeded";return false;}
        for(const auto& t:j[3]){subscriber_lifecycle::Token item;if(!token(t,item)){error="Dungeon Fill detached token value invalid";return false;}row.tokens.push_back(std::move(item));}out.push_back(std::move(row));
    }
    if(out[0].label.type!=1||out[0].label.text!="mmoFill"){error="Dungeon Fill detached root is not original mmoFill";return false;}error.clear();return true;
}catch(const std::exception& e){error=std::string("Dungeon Fill detached value decode: ")+e.what();return false;}
}
