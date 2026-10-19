#pragma once
#include "duplicate_record_witness.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <stdexcept>
#include "npc_target_key.h"
#include "monster_target_key.h"
#include "snapshot_zone_key.h"
#include "snapshot_boss_key.h"
#include <optional>
#include "control_input/control_motion.h"
#include <string_view>

namespace mt2mp::game::interventions {
enum class Family:uint8_t {Boss=0,Monster=1,Npc=2,Toon=3};
enum class Command:uint8_t {Heal=1,Kill=2,Warn=3,Ban=4,Respawn=5,RaiseLevel=6,LowerLevel=7,ControlAcquire=8,ControlInput=9,ControlRelease=10,ControlActivate=11,ControlDeactivate=12};
struct Target {Family family=Family::Boss;std::string key;bool operator==(const Target&)const=default;};
inline bool control_command(Command value){return value>=Command::ControlAcquire&&value<=Command::ControlDeactivate;}
struct Control {
    std::string lease;uint64_t sequence=0,jump=0;
    control_input::BasisInput input;
    std::optional<Target> selected;
    int32_t ability=-1;std::string prototype;
    bool operator==(const Control&)const=default;
};
struct ControlProof {
    uint64_t peer=0,sequence=0,jump=0;std::string lease;
    bool controlled=false,moving=false;std::optional<Target> selected;
    bool operator==(const ControlProof&)const=default;
};
struct Intent {Target target;Command command=Command::Heal;std::string before_sha;std::optional<Control> control;bool operator==(const Intent&)const=default;};
struct Proof {
    Target target;
    duplicate_records::Witness actor;
    std::optional<duplicate_records::Witness> subscriber;
    std::string actors_sha,zones_sha,economy_sha,gm_sha,marketing_sha,tutorial_sha;
    std::optional<ControlProof> control;
    bool operator==(const Proof&)const=default;
};
inline bool digest(std::string_view value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string_view::npos;}
inline bool boss_key(std::string_view value){
    if(snapshot_boss_key::valid(value))return true;
    if(!value.starts_with("action:"))return false;value.remove_prefix(7);
    return value.size()>42&&monster_target_key::uuid(value.substr(0,36))&&value.substr(36,6)==":boss:"&&snapshot_building_key::number(value.substr(42),INT32_MAX,false);
}
inline bool zone_monster_key(std::string_view value){
    constexpr std::string_view separator=":monster:";const auto split=value.rfind(separator);
    if(split==std::string_view::npos||!snapshot_building_key::number(value.substr(split+separator.size()),INT32_MAX,false))return false;
    const auto zone=value.substr(0,split);return snapshot_zone_key::valid(zone)||(zone.starts_with("action:")&&monster_target_key::uuid(zone.substr(7)));
}
inline bool valid(const Target& value){
    switch(value.family){
    case Family::Boss:return boss_key(value.key);
    case Family::Monster:return monster_target_key::valid(value.key)||zone_monster_key(value.key);
    case Family::Npc:return npc_target_key::valid(value.key);
    case Family::Toon:return value.key.starts_with("subscriber:")&&snapshot_building_key::number(std::string_view(value.key).substr(11),INT32_MAX,false);
    default:return false;}
}
inline const char* original_command(Command command){switch(command){case Command::Heal:return "Heal";case Command::Kill:return "Kill";case Command::Warn:return "Warn";case Command::Ban:return "Ban";case Command::Respawn:return "Respawn";case Command::RaiseLevel:return "Raise Level";case Command::LowerLevel:return "Lower Level";default:return nullptr;}}
inline bool valid(const ControlProof& value){return value.peer<=4&&(!value.selected||valid(*value.selected))&&
    (value.controlled?(value.peer>0&&monster_target_key::uuid(value.lease)):(!value.peer&&value.lease.empty()&&!value.sequence&&!value.jump));}
inline bool valid(const Intent& value){
    if(!valid(value.target))return false;
    if(!control_command(value.command))return !value.control&&digest(value.before_sha)&&original_command(value.command)&&(value.command==Command::Heal||value.command==Command::Kill||value.target.family==Family::Toon);
    if(!value.control||!value.before_sha.empty())return false;const auto& c=*value.control;
    if(!monster_target_key::uuid(c.lease)||!control_input::valid(c.input)||c.input.jump_pressed||(c.selected&&!valid(*c.selected)))return false;
    const bool ability=value.command==Command::ControlActivate||value.command==Command::ControlDeactivate;
    if(ability){if(c.ability<0||c.ability>=4096||c.prototype.empty()||c.prototype.size()>128||c.prototype.find('\0')!=std::string::npos)return false;}
    else if(c.ability!=-1||!c.prototype.empty())return false;
    if(value.command==Command::ControlInput)return c.sequence>0;
    return !c.sequence&&!c.jump&&c.input==control_input::BasisInput{}&&!c.selected;
}
inline bool valid(const duplicate_records::Witness& value){return digest(value.sha)&&value.records&&value.records<=65536&&value.tokens<=262144&&value.links<=65536;}
inline bool valid(const Proof& value){if(value.control)return valid(value.target)&&valid(*value.control)&&value.actor==duplicate_records::Witness{}&&!value.subscriber&&value.actors_sha.empty()&&value.zones_sha.empty()&&value.economy_sha.empty()&&value.gm_sha.empty()&&value.marketing_sha.empty()&&value.tutorial_sha.empty();return valid(value.target)&&valid(value.actor)&&value.subscriber.has_value()==(value.target.family==Family::Toon)&&(!value.subscriber||valid(*value.subscriber))&&digest(value.actors_sha)&&digest(value.zones_sha)&&digest(value.economy_sha)&&digest(value.gm_sha)&&digest(value.marketing_sha)&&digest(value.tutorial_sha);}
// The original command may be a valid no-op, such as Heal at full health or
// Respawn for a living Toon. Its complete native records still need readback.
inline bool effect(const Intent& intent,const Proof& before,const Proof& after){
    if(!valid(intent)||!valid(before)||!valid(after)||intent.target!=before.target||before.target!=after.target)return false;
    if(!control_command(intent.command))return !before.control&&!after.control&&intent.before_sha==before.actor.sha;
    if(!before.control||!after.control)return false;const auto& a=*before.control;const auto& b=*after.control;const auto& c=*intent.control;
    if(intent.command==Command::ControlAcquire)return !a.controlled&&b.controlled&&b.lease==c.lease&&!b.sequence&&!b.jump;
    if(!a.controlled||a.lease!=c.lease)return false;
    if(intent.command==Command::ControlRelease)return !b.controlled;
    if(!b.controlled||b.peer!=a.peer||b.lease!=a.lease)return false;
    if(intent.command==Command::ControlInput)return b.sequence==c.sequence&&b.jump==c.jump&&b.selected==c.selected;
    return b.sequence==a.sequence&&b.jump==a.jump;
}
inline nlohmann::json encode(const Target& value){return {{"family",uint8_t(value.family)},{"key",value.key}};}
inline nlohmann::json encode(const Control& v){return {{"lease",v.lease},{"sequence",v.sequence},{"jump",v.jump},{"basis",{v.input.right.x,v.input.right.z,v.input.forward.x,v.input.forward.z,v.input.axis1,v.input.negative_axis0}},{"selected",v.selected?encode(*v.selected):nlohmann::json(nullptr)},{"ability",v.ability},{"prototype",v.prototype}};}
inline nlohmann::json encode(const ControlProof& v){return {{"peer",v.peer},{"lease",v.lease},{"sequence",v.sequence},{"jump",v.jump},{"controlled",v.controlled},{"moving",v.moving},{"selected",v.selected?encode(*v.selected):nlohmann::json(nullptr)}};}
inline nlohmann::json encode(const Intent& value){if(value.control)return {{"target",encode(value.target)},{"command",uint8_t(value.command)},{"control",encode(*value.control)}};return {{"target",encode(value.target)},{"command",uint8_t(value.command)},{"before_sha",value.before_sha}};}
inline nlohmann::json encode(const duplicate_records::Witness& value){return {{"sha",value.sha},{"records",value.records},{"tokens",value.tokens},{"links",value.links}};}
inline nlohmann::json encode(const Proof& value){if(value.control)return {{"target",encode(value.target)},{"control",encode(*value.control)}};return {{"target",encode(value.target)},{"actor",encode(value.actor)},{"subscriber",value.subscriber?encode(*value.subscriber):nlohmann::json(nullptr)},{"actors_sha",value.actors_sha},{"zones_sha",value.zones_sha},{"economy_sha",value.economy_sha},{"gm_sha",value.gm_sha},{"marketing_sha",value.marketing_sha},{"tutorial_sha",value.tutorial_sha}};}
namespace codec {
using Json=nlohmann::json;
inline void exact(const Json& value,std::initializer_list<const char*> keys){if(!value.is_object()||value.size()!=keys.size())throw std::runtime_error("Intervention exact object fields differ");for(const auto* key:keys)if(!value.contains(key))throw std::runtime_error(std::string("Intervention field absent: ")+key);}
inline uint32_t number(const Json& value,uint32_t maximum){if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>maximum:value.get<int64_t>()<0||uint64_t(value.get<int64_t>())>maximum))throw std::runtime_error("Intervention integer outside domain");return value.get<uint32_t>();}
inline std::string text(const Json& value,size_t maximum){if(!value.is_string())throw std::runtime_error("Intervention string required");const auto result=value.get<std::string>();if(result.size()>maximum||result.find('\0')!=std::string::npos)throw std::runtime_error("Intervention text outside bounded native identity");return result;}
inline Target target(const Json& value){exact(value,{"family","key"});Target out{Family(number(value.at("family"),3)),text(value.at("key"),4096)};if(!valid(out))throw std::runtime_error("Intervention target lacks its canonical family identity");return out;}
inline uint64_t sequence(const Json& value){if(!value.is_number_integer()||(!value.is_number_unsigned()&&value.get<int64_t>()<0))throw std::runtime_error("Control sequence outside unsigned domain");return value.get<uint64_t>();}
inline Control control(const Json& value){exact(value,{"lease","sequence","jump","basis","selected","ability","prototype"});Control out;out.lease=text(value.at("lease"),36);out.sequence=sequence(value.at("sequence"));out.jump=sequence(value.at("jump"));const auto& axes=value.at("basis");if(!axes.is_array()||axes.size()!=6)throw std::runtime_error("Control exact six basis/axis values required");std::array<float,6> n{};for(size_t i=0;i<6;++i){if(!axes[i].is_number())throw std::runtime_error("Control finite numeric input required");n[i]=axes[i].get<float>();}out.input={{n[0],0,n[1]},{n[2],0,n[3]},n[4],n[5],false};if(!value.at("selected").is_null())out.selected=target(value.at("selected"));const auto& ability=value.at("ability");if(!ability.is_number_integer()||ability.get<int64_t>()< -1||ability.get<int64_t>()>4095)throw std::runtime_error("Control ability slot outside domain");out.ability=ability.get<int32_t>();out.prototype=text(value.at("prototype"),128);return out;}
inline ControlProof control_proof(const Json& value){exact(value,{"peer","lease","sequence","jump","controlled","moving","selected"});ControlProof out;out.peer=number(value.at("peer"),4);out.lease=text(value.at("lease"),36);out.sequence=sequence(value.at("sequence"));out.jump=sequence(value.at("jump"));if(!value.at("controlled").is_boolean()||!value.at("moving").is_boolean())throw std::runtime_error("Control native flags must be Boolean");out.controlled=value.at("controlled").get<bool>();out.moving=value.at("moving").get<bool>();if(!value.at("selected").is_null())out.selected=target(value.at("selected"));if(!valid(out))throw std::runtime_error("Control proof ownership invalid");return out;}
inline Intent intent(const Json& value){Intent out;out.target=target(value.at("target"));out.command=Command(number(value.at("command"),12));if(control_command(out.command)){exact(value,{"target","command","control"});out.control=control(value.at("control"));}else{exact(value,{"target","command","before_sha"});out.before_sha=text(value.at("before_sha"),64);}if(!valid(out))throw std::runtime_error("Intervention command/family/before-record invalid");return out;}
inline duplicate_records::Witness witness(const Json& value){exact(value,{"sha","records","tokens","links"});duplicate_records::Witness out{text(value.at("sha"),64),number(value.at("records"),65536),number(value.at("tokens"),262144),number(value.at("links"),65536)};if(!valid(out))throw std::runtime_error("Intervention complete original record witness invalid");return out;}
inline Proof proof(const Json& value){if(value.contains("control")){exact(value,{"target","control"});Proof out;out.target=target(value.at("target"));out.control=control_proof(value.at("control"));if(!valid(out))throw std::runtime_error("Control native proof invalid");return out;}exact(value,{"target","actor","subscriber","actors_sha","zones_sha","economy_sha","gm_sha","marketing_sha","tutorial_sha"});Proof out;out.target=target(value.at("target"));out.actor=witness(value.at("actor"));if(!value.at("subscriber").is_null())out.subscriber=witness(value.at("subscriber"));out.actors_sha=text(value.at("actors_sha"),64);out.zones_sha=text(value.at("zones_sha"),64);out.economy_sha=text(value.at("economy_sha"),64);out.gm_sha=text(value.at("gm_sha"),64);out.marketing_sha=text(value.at("marketing_sha"),64);out.tutorial_sha=text(value.at("tutorial_sha"),64);if(!valid(out))throw std::runtime_error("Intervention complete original owner proof invalid");return out;}
}
}
