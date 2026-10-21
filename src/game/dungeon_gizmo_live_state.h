#pragma once
#include "dungeon_gizmo_identity.h"
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace mt2mp::game::dungeon_gizmo_live {
struct Clip {
    std::optional<uint32_t> length,time;
    bool paused=false;uint32_t speed=0;
    bool operator==(const Clip&)const=default;
};
struct Row {
    std::string key;
    bool triggered=false,transition=false,on_map=false,interaction=false;
    int32_t state=0,immediate=0,from=0,to=0,animation=-1;
    // Open, close, unlock and container trigger are fixed native asset slots.
    std::array<Clip,4> clips{};
    bool operator==(const Row&)const=default;
};
struct Scene {
    std::string key;uint32_t unlocked=0;std::vector<Row> rows;
    bool operator==(const Scene&)const=default;
};
using State=std::vector<Scene>;
struct Receipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
inline bool fail(std::string& error,const std::string& why){error="Gizmo live "+why;return false;}
inline bool valid(const State& value,std::string& error){
    std::set<std::string> scenes,keys;
    for(const auto& scene:value){
        if(!dungeon_gizmo_identity::scene(scene.key)||!scenes.insert(scene.key).second)return fail(error,"canonical Scene key missing or repeated");
        for(const auto& row:scene.rows){
            if(!dungeon_gizmo_identity::key(row.key)||!keys.insert(row.key).second||row.state<0||row.state>2||
               row.immediate<0||row.immediate>2||row.from<0||row.from>2||row.to<0||row.to>2||row.animation<-1||row.animation>2)
                return fail(error,"current key or original Door state outside native domain");
            if(!row.transition&&(row.from||row.to||row.animation!=-1))return fail(error,"inactive transition contains unowned cache values");
            for(const auto& clip:row.clips){
                if(clip.time&&!clip.length)return fail(error,"playing clip lacks native asset length");
                if(!clip.time&&(clip.paused||clip.speed))return fail(error,"inactive clip has playing values");
                if((clip.length&&(!std::isfinite(std::bit_cast<float>(*clip.length))||std::bit_cast<float>(*clip.length)<0))||
                   (clip.time&&!std::isfinite(std::bit_cast<float>(*clip.time)))||!std::isfinite(std::bit_cast<float>(clip.speed)))
                    return fail(error,"animation contains a nonfinite native float");
            }
        }
    }error.clear();return true;
}
inline nlohmann::json encode(const State& value){
    auto scenes=nlohmann::json::array();
    for(const auto& scene:value){auto rows=nlohmann::json::array();for(const auto& row:scene.rows){auto clips=nlohmann::json::array();
        for(const auto& clip:row.clips)clips.push_back({clip.length?nlohmann::json(*clip.length):nlohmann::json(nullptr),
            clip.time?nlohmann::json(*clip.time):nlohmann::json(nullptr),clip.paused,clip.speed});
        rows.push_back({{"key",row.key},{"triggered",row.triggered},{"state",row.state},{"immediate",row.immediate},
            {"transition",row.transition},{"from",row.from},{"to",row.to},{"animation",row.animation},
            {"on_map",row.on_map},{"interaction",row.interaction},{"clips",std::move(clips)}});}
        scenes.push_back({{"key",scene.key},{"unlocked",scene.unlocked},{"rows",std::move(rows)}});
    }return scenes;
}
inline bool decode(const nlohmann::json& value,State& out,std::string& error)try{
    using J=nlohmann::json;out.clear();
    const auto exact=[](const J& j,std::initializer_list<const char*> fields){if(!j.is_object()||j.size()!=fields.size())throw std::runtime_error("exact fields required");for(const auto* field:fields)if(!j.contains(field))throw std::runtime_error("field missing");};
    const auto u32=[](const J& j){if(!j.is_number_unsigned()&&(!j.is_number_integer()||j.get<int64_t>()<0))throw std::runtime_error("unsigned word required");const auto n=j.get<uint64_t>();if(n>UINT32_MAX)throw std::runtime_error("word exceeds uint32");return uint32_t(n);};
    const auto i32=[](const J& j){if(!j.is_number_integer())throw std::runtime_error("integer required");if(j.is_number_unsigned()&&j.get<uint64_t>()>INT32_MAX)throw std::runtime_error("integer exceeds int32");const auto n=j.get<int64_t>();if(n<INT32_MIN||n>INT32_MAX)throw std::runtime_error("integer exceeds int32");return int32_t(n);};
    const auto boolean=[](const J& j){if(!j.is_boolean())throw std::runtime_error("boolean required");return j.get<bool>();};
    if(!value.is_array())return fail(error,"complete Scene array required");State next;
    for(const auto& item:value){exact(item,{"key","unlocked","rows"});Scene scene;scene.key=item.at("key").get<std::string>();scene.unlocked=u32(item.at("unlocked"));
        if(!item.at("rows").is_array())return fail(error,"complete Gizmo array required");
        for(const auto& source:item.at("rows")){exact(source,{"key","triggered","state","immediate","transition","from","to","animation","on_map","interaction","clips"});Row row;
            row.key=source.at("key").get<std::string>();row.triggered=boolean(source.at("triggered"));row.transition=boolean(source.at("transition"));
            row.state=i32(source.at("state"));row.immediate=i32(source.at("immediate"));row.from=i32(source.at("from"));row.to=i32(source.at("to"));row.animation=i32(source.at("animation"));row.on_map=boolean(source.at("on_map"));row.interaction=boolean(source.at("interaction"));
            const auto& clips=source.at("clips");if(!clips.is_array()||clips.size()!=4)return fail(error,"four semantic clips required");
            for(size_t n=0;n<4;++n){const auto& fields=clips[n];if(!fields.is_array()||fields.size()!=4)return fail(error,"four clip values required");auto& clip=row.clips[n];
                if(!fields[0].is_null())clip.length=u32(fields[0]);if(!fields[1].is_null())clip.time=u32(fields[1]);clip.paused=boolean(fields[2]);clip.speed=u32(fields[3]);}
            scene.rows.push_back(std::move(row));}
        next.push_back(std::move(scene));}
    if(!valid(next,error))return false;out=std::move(next);return true;
}catch(const std::exception& exception){out.clear();return fail(error,std::string("decode: ")+exception.what());}
inline bool same_members(const State& a,const State& b,std::string& error){
    if(a.size()!=b.size())return fail(error,"complete Scene count differs");
    for(size_t n=0;n<a.size();++n){if(a[n].key!=b[n].key||a[n].rows.size()!=b[n].rows.size())return fail(error,"complete Scene/member order differs");
        for(size_t i=0;i<a[n].rows.size();++i){if(a[n].rows[i].key!=b[n].rows[i].key)return fail(error,"complete canonical member order differs");
            for(size_t c=0;c<4;++c)if(a[n].rows[i].clips[c].length!=b[n].rows[i].clips[c].length)return fail(error,"local semantic asset length differs");}}
    return true;
}
}
