#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <stdexcept>

namespace mt2mp::game::gm_policy {
enum class Field:uint8_t {Enabled,WatchSeconds,CheckLog,IsBannable,BanFavorites,BanInfluencers,WarnsBeforeBan};
inline constexpr std::array<const char*,7> names{"enabled","watchSeconds","checkLog","isBannable","banFavorites","banInfluencers","warnsBeforeBan"};
inline constexpr std::array<size_t,7> offsets{0x18,0x1c,0x20,0x21,0x22,0x23,0x24};
inline constexpr bool integer(Field field){return field==Field::WatchSeconds||field==Field::WarnsBeforeBan;}
inline constexpr int32_t maximum(Field field){return field==Field::WatchSeconds?3600:field==Field::WarnsBeforeBan?30:1;}
struct State {std::array<std::array<int32_t,7>,2> policies{};bool operator==(const State&)const=default;};
struct Intent {uint8_t policy=0;Field field=Field::Enabled;int32_t before=0,value=0;bool operator==(const Intent&)const=default;};
struct Proof {State state;std::string gm_sha256,cascade_sha256,protected_sha256,tutorial_sha256;bool operator==(const Proof&)const=default;};
inline bool valid(const State& state){for(const auto& row:state.policies)for(size_t i=0;i<row.size();++i)if(row[i]<0||row[i]>maximum(Field(i)))return false;return true;}
inline bool valid(const Intent& intent){return intent.policy<2&&uint8_t(intent.field)<7&&intent.before>=0&&intent.before<=maximum(intent.field)&&intent.value>=0&&intent.value<=maximum(intent.field)&&intent.before!=intent.value;}
inline bool digest(const std::string& value){if(value.size()!=64)return false;for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
inline bool ordered(const Proof& proof){return digest(proof.cascade_sha256)&&digest(proof.protected_sha256)&&digest(proof.tutorial_sha256);}
inline bool valid(const Proof& proof){return valid(proof.state)&&digest(proof.gm_sha256)&&((proof.cascade_sha256.empty()&&proof.protected_sha256.empty()&&proof.tutorial_sha256.empty())||ordered(proof));}
inline bool effect(const Intent& intent,const Proof& before,const Proof& after){
    if(!valid(intent)||!valid(before)||!valid(after)||before.state.policies[intent.policy][size_t(intent.field)]!=intent.before)return false;
    auto expected=before.state;expected.policies[intent.policy][size_t(intent.field)]=intent.value;
    return after.state==expected&&((!ordered(before)&&!ordered(after))||(ordered(before)&&ordered(after)&&before.protected_sha256==after.protected_sha256&&before.tutorial_sha256==after.tutorial_sha256));
}
inline nlohmann::json encode(const State& value){return value.policies;}
inline nlohmann::json encode(const Intent& value){return {{"policy",value.policy},{"field",uint8_t(value.field)},{"before",value.before},{"value",value.value}};}
inline nlohmann::json encode(const Proof& value){nlohmann::json out={{"policies",encode(value.state)},{"gm_sha256",value.gm_sha256}};if(ordered(value)){out["cascade_sha256"]=value.cascade_sha256;out["protected_sha256"]=value.protected_sha256;out["tutorial_sha256"]=value.tutorial_sha256;}return out;}
namespace codec {
using json=nlohmann::json;
inline int32_t number(const json& value,int32_t max){if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>uint64_t(max):value.get<int64_t>()<0||value.get<int64_t>()>max))throw std::runtime_error("GM policy integer outside native UI domain");return value.get<int32_t>();}
inline State state(const json& value){State out;if(!value.is_array()||value.size()!=2)throw std::runtime_error("GM policy requires exactly chat and cheat rows");for(size_t p=0;p<2;++p){if(!value[p].is_array()||value[p].size()!=7)throw std::runtime_error("GM policy requires all seven native fields");for(size_t f=0;f<7;++f)out.policies[p][f]=number(value[p][f],maximum(Field(f)));}return out;}
inline Intent intent(const json& value){if(!value.is_object()||value.size()!=4)throw std::runtime_error("GM policy intent requires four exact fields");Intent out;out.policy=uint8_t(number(value.at("policy"),1));out.field=Field(number(value.at("field"),6));out.before=number(value.at("before"),maximum(out.field));out.value=number(value.at("value"),maximum(out.field));if(!valid(out))throw std::runtime_error("GM policy intent has no original change");return out;}
inline Proof proof(const json& value){if(!value.is_object()||(value.size()!=2&&value.size()!=5))throw std::runtime_error("GM policy proof requires exact state and complete GM/cascade digests");Proof out;out.state=state(value.at("policies"));out.gm_sha256=value.at("gm_sha256").get<std::string>();if(value.size()==5){out.cascade_sha256=value.at("cascade_sha256").get<std::string>();out.protected_sha256=value.at("protected_sha256").get<std::string>();out.tutorial_sha256=value.at("tutorial_sha256").get<std::string>();if(!ordered(out))throw std::runtime_error("GM policy ordered proof digests invalid");}if(!valid(out))throw std::runtime_error("GM policy proof invalid");return out;}
}
template<class Read>bool read(Read&& read,uintptr_t image,uintptr_t queue,State& out,std::string& error){
    out={};uintptr_t table=0,singleton=0;if(!read(image,0x13df360,singleton,error)||!read(queue,0,table,error))return false;
    if(!queue||singleton!=queue||table!=image+0x16fdc60){error="GM policy current TicketQueue singleton/class differs";return false;}
    for(size_t p=0;p<2;++p){const auto object=queue+0x48+p*40;if(!read(object,0,table,error))return false;if(table!=image+0x16dab90){error="GM policy embedded current native class differs";return false;}
        for(size_t f=0;f<7;++f){int32_t value=0;if(integer(Field(f))){if(!read(object,offsets[f],value,error))return false;}else {uint8_t byte=0;if(!read(object,offsets[f],byte,error))return false;value=byte;}out.policies[p][f]=value;}}
    if(!valid(out)){error="GM policy actual value exceeds original property UI domain";return false;}error.clear();return true;
}
}
