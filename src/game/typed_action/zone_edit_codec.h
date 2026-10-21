#pragma once
#include "zone_edit_action.h"
#include "vendor/nlohmann/json.hpp"
#include <initializer_list>
#include <stdexcept>

namespace mt2mp::action_session::zone_edit_codec {
using Json=nlohmann::json;
inline void exact(const Json& value,std::initializer_list<const char*> names){
    if(!value.is_object()||value.size()!=names.size())throw std::runtime_error("Zone edit exact object fields differ");
    for(const auto* name:names)if(!value.contains(name))throw std::runtime_error("Zone edit required field missing");
}
inline uint64_t unsigned_integer(const Json& value){
    if(value.is_number_unsigned())return value.get<uint64_t>();
    if(value.is_number_integer()){const auto number=value.get<int64_t>();if(number>=0)return uint64_t(number);}
    throw std::runtime_error("Zone edit requires an unsigned integer");
}
inline uint32_t word(const Json& value){
    const auto number=unsigned_integer(value);
    if(number>UINT32_MAX)throw std::runtime_error("Zone edit word exceeds uint32");
    return uint32_t(number);
}
inline int64_t signed_integer(const Json& value){
    if(value.is_number_unsigned()){const auto number=value.get<uint64_t>();
        if(number<=uint64_t(INT64_MAX))return int64_t(number);
    }else if(value.is_number_integer())return value.get<int64_t>();
    throw std::runtime_error("Zone edit requires an int64 integer");
}
inline int32_t integer(const Json& value){
    const auto number=signed_integer(value);
    if(number<INT32_MIN||number>INT32_MAX)throw std::runtime_error("Zone edit integer exceeds int32");
    return int32_t(number);
}
template<size_t Size>std::array<uint32_t,Size> words(const Json& value){
    if(!value.is_array()||value.size()!=Size)throw std::runtime_error("Zone edit exact word-array extent differs");
    std::array<uint32_t,Size> result{};
    for(size_t index=0;index<Size;++index)result[index]=word(value[index]);
    return result;
}
inline Json intent_json(const ZoneEditIntent& value){
    if(!valid_zone_edit_intent(value))throw std::runtime_error("Zone edit intent outside original domain");
    auto stamps=Json::array();
    for(const auto& stamp:value.stamps){
        auto paths=Json::array();
        for(const auto& path:stamp.shape){auto points=Json::array();
            for(const auto& point:path){points.push_back(point.x);points.push_back(point.z);}
            paths.push_back(std::move(points));
        }
        std::array<uint32_t,3> position{};
        for(size_t index=0;index<3;++index)position[index]=std::bit_cast<uint32_t>(stamp.position[index]);
        stamps.push_back({{"position",position},{"paths",std::move(paths)}});
    }
    return {{"kind",uint8_t(value.kind)},{"region",value.region},{"selected",value.selected},
        {"zone_type",value.zone_type},{"character_type",value.character_type},{"level",value.level},{"density",value.density},
        {"add",value.add},{"press",value.press},{"stamps",std::move(stamps)}};
}
inline ZoneEditIntent intent_read(const Json& source){
    exact(source,{"kind","region","selected","zone_type","character_type","level","density","add","press","stamps"});
    const auto kind=unsigned_integer(source.at("kind"));
    if(kind<1||kind>5)throw std::runtime_error("Zone edit original operation differs");
    ZoneEditIntent value;value.kind=ZoneEditKind(kind);value.region=integer(source.at("region"));
    value.selected=source.at("selected").get<std::string>();value.zone_type=integer(source.at("zone_type"));
    value.character_type=integer(source.at("character_type"));value.level=integer(source.at("level"));value.density=integer(source.at("density"));
    if(!source.at("add").is_boolean())throw std::runtime_error("Zone edit add requires a boolean");
    value.add=source.at("add").get<bool>();value.press=words<3>(source.at("press"));
    const auto& stamps=source.at("stamps");
    if(!stamps.is_array()||stamps.size()>64)throw std::runtime_error("Zone edit stamp budget differs");
    size_t remaining=1024;
    for(const auto& row:stamps){
        exact(row,{"position","paths"});ZoneStamp stamp;const auto position=words<3>(row.at("position"));
        for(size_t index=0;index<3;++index)stamp.position[index]=std::bit_cast<float>(position[index]);
        const auto& paths=row.at("paths");
        if(!paths.is_array()||paths.empty())throw std::runtime_error("Zone edit clipped shape requires paths");
        for(const auto& path:paths){
            if(!path.is_array()||path.size()<6||path.size()%2||path.size()/2>remaining)
                throw std::runtime_error("Zone edit path exceeds exact int64 pair budget");
            remaining-=path.size()/2;TerrainPath decoded;
            for(size_t index=0;index<path.size();index+=2)decoded.push_back({signed_integer(path[index]),signed_integer(path[index+1])});
            stamp.shape.push_back(std::move(decoded));
        }
        value.stamps.push_back(std::move(stamp));
    }
    if(!valid_zone_edit_intent(value))throw std::runtime_error("Zone edit intent outside original domain");
    return value;
}
inline Json state_json(const ZoneEditState& value){
    if(!valid_zone_edit_state(value))throw std::runtime_error("Zone edit complete state hashes missing");
    return {{"values_sha",value.values_sha},{"quests_sha",value.quests_sha},{"references_sha",value.references_sha},
        {"counts",value.counts},{"monsters",value.monsters},{"global_random",value.global_random},{"region_random",value.region_random},
        {"next_tag",value.next_tag},{"created_monsters",value.created_monsters},{"release_edits",value.release_edits}};
}
inline ZoneEditState state_read(const Json& source){
    exact(source,{"values_sha","quests_sha","references_sha","counts","monsters","global_random","region_random","next_tag","created_monsters","release_edits"});ZoneEditState value;
    value.values_sha=source.at("values_sha").get<std::string>();value.quests_sha=source.at("quests_sha").get<std::string>();
    value.references_sha=source.at("references_sha").get<std::string>();value.counts=words<4>(source.at("counts"));value.monsters=word(source.at("monsters"));
    for(const auto* name:{"global_random","region_random"}){
        const auto& encoded=source.at(name);
        if(!encoded.is_array()||encoded.size()!=4)throw std::runtime_error("Zone edit RNG requires four uint64 words");
        auto& destination=std::string_view(name)=="global_random"?value.global_random:value.region_random;
        for(size_t index=0;index<4;++index)destination[index]=unsigned_integer(encoded[index]);
    }
    value.next_tag=integer(source.at("next_tag"));value.created_monsters=integer(source.at("created_monsters"));
    const auto& edits=source.at("release_edits");
    if(!edits.is_array()||edits.size()!=8)throw std::runtime_error("Zone edit Release requires eight int32 counters");
    for(size_t index=0;index<8;++index)value.release_edits[index]=integer(edits[index]);
    if(!valid_zone_edit_state(value))throw std::runtime_error("Zone edit complete state hashes missing");
    return value;
}
inline Json effect_json(const ZoneEditEffect& value){
    if(!valid_zone_edit_effect(value))throw std::runtime_error("Zone edit complete inverse proof differs");
    return {{"before",state_json(value.before)},{"after",state_json(value.after)},
        {"inverse_sha",value.inverse_sha},{"inverse_operations",value.inverse_operations}};
}
inline ZoneEditEffect effect_read(const Json& source){
    exact(source,{"before","after","inverse_sha","inverse_operations"});ZoneEditEffect value;
    value.before=state_read(source.at("before"));value.after=state_read(source.at("after"));
    value.inverse_sha=source.at("inverse_sha").get<std::string>();value.inverse_operations=word(source.at("inverse_operations"));
    if(!valid_zone_edit_effect(value))throw std::runtime_error("Zone edit complete inverse proof differs");
    return value;
}
inline bool valid_scope(uint64_t schema,uint64_t action,std::string_view key,std::string_view action_uuid,std::string_view target_uuid){
    if(schema!=36||!game::monster_target_key::uuid(action_uuid))return false;
    if(action==58)return target_uuid.empty()&&key==std::string("zone-edit:")+std::string(action_uuid);
    return (action==10||action==11)&&game::monster_target_key::uuid(target_uuid)&&action_uuid!=target_uuid&&
        key==std::string("zone-edit:")+std::string(target_uuid);
}
}
