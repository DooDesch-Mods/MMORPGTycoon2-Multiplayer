#pragma once
#include "release_edit_state.h"
#include "general_project_codec.h"
namespace mt2mp::game::release_edit::codec {
using mt2mp::game::general_project::codec::shape;
using mt2mp::game::general_project::codec::i32;
using mt2mp::game::general_project::codec::boolean;
inline std::string string(const nlohmann::json& j){if(!j.is_string())throw std::runtime_error("Release edit string required");auto value=j.get<std::string>();if(!text(value))throw std::runtime_error("Release edit text extent/termination invalid");return value;}
inline std::string sha(const nlohmann::json& j){auto value=string(j);if(!digest(value))throw std::runtime_error("Release edit SHA256 required");return value;}
inline Intent intent(const nlohmann::json& j){shape(j,{"command","type","custom","text","text_sha256"});const auto command=i32(j.at("command"));if(command<1||command>6)throw std::runtime_error("Release edit command outside1..6");Intent out{Command(command),i32(j.at("type")),boolean(j.at("custom")),string(j.at("text")),string(j.at("text_sha256"))};if(!valid(out))throw std::runtime_error("Release edit command field domain invalid");return out;}
template<size_t N>void hashes(const nlohmann::json& j,std::array<std::string,N>& out){if(!j.is_array()||j.size()!=N)throw std::runtime_error("Release edit exact hash array required");for(size_t n=0;n<N;++n)out[n]=sha(j[n]);}
inline Proof proof(const nlohmann::json& j){shape(j,{"version","planned","type","custom","confirmation_required","text_sha256","defaults_sha256","unrelated_sha256"});Proof out;
    for(const auto& name:{"version","planned"}){const auto& list=j.at(name);if(!list.is_array()||list.size()!=3)throw std::runtime_error("Release edit version needs three exact integers");auto& target=name==std::string_view("version")?out.version:out.planned;for(size_t n=0;n<3;++n)target[n]=i32(list[n]);}
    out.type=i32(j.at("type"));out.custom=boolean(j.at("custom"));out.confirmation_required=boolean(j.at("confirmation_required"));hashes(j.at("text_sha256"),out.text_sha256);hashes(j.at("defaults_sha256"),out.defaults_sha256);out.unrelated_sha256=sha(j.at("unrelated_sha256"));
    if(!valid(out))throw std::runtime_error("Release edit complete compact proof invalid");return out;
}
}
