#include "../monster_target_key.h"
#include "monster_state.h"
#include <bit>
#include <cmath>
#include <climits>
#include <set>
#include <stdexcept>
namespace mt2mp::game::live_host_state::monsters {
namespace {
using Json=nlohmann::json;
bool fail(std::string& e,const std::string& s){e=s;return false;}
void exact(const Json& j,std::initializer_list<const char*> names){if(!j.is_object()||j.size()!=names.size())throw std::runtime_error("Monster frame exact object fields differ");for(auto key:names)if(!j.contains(key))throw std::runtime_error(std::string("Monster frame missing ")+key);}
int32_t integer(const Json& j){if(!j.is_number_integer()||(j.is_number_unsigned()?j.get<uint64_t>()>uint64_t(INT32_MAX):j.get<int64_t>()<INT32_MIN||j.get<int64_t>()>INT32_MAX))throw std::runtime_error("Monster frame int32 required");return j.get<int32_t>();}
uint32_t bits(const Json& j){if(!j.is_number_integer()||(!j.is_number_unsigned()&&j.get<int64_t>()<0)||j.get<uint64_t>()>UINT32_MAX)throw std::runtime_error("Monster frame uint32 bits required");return j.get<uint32_t>();}
bool boolean(const Json& j){if(!j.is_boolean())throw std::runtime_error("Monster frame boolean required");return j.get<bool>();}
template<class T,size_t N,class Read>void array(const Json& j,std::array<T,N>& out,Read&& read){if(!j.is_array()||j.size()!=N)throw std::runtime_error("Monster frame exact array shape differs");for(size_t i=0;i<N;++i)out[i]=read(j[i]);}
float real(const Json& j){if(!j.is_number())throw std::runtime_error("Monster frame numeric pose required");const auto value=j.get<double>();if(!std::isfinite(value)||std::abs(value)>100000000)throw std::runtime_error("Monster frame pose outside finite bound");return float(value);}
bool target(const TargetObservation& t){if(t.state=="empty"||t.state=="expired"||t.state=="unmapped")return t.key.empty();if(t.state!="mapped")return false;if(monster_target_key::valid(t.key))return true;if(t.key.starts_with("subscriber:")){const auto number=t.key.substr(11);if(number.empty()||number.size()>10||(number.size()>1&&number[0]=='0'))return false;uint64_t n=0;for(char c:number){if(c<'0'||c>'9')return false;n=n*10+uint64_t(c-'0');}return n<=INT32_MAX;}return false;}
}
bool valid(const State& s,std::string& e){
    if(s.rows.size()>max_rows||s.retired.size()>max_rows||s.rows.size()+s.retired.size()>max_rows)return fail(e,"Monster frame row/tombstone budget exceeded");
    std::set<std::string> keys;std::string previous;
    for(const auto& r:s.rows){if(!monster_target_key::input(r.birth)||(!previous.empty()&&r.birth<=previous)||!keys.insert(r.birth).second)return fail(e,"Monster frame live birth keys are not unique sorted canonical identities");previous=r.birth;
        const auto identity_error=[&](const std::string& detail){return fail(e,"Monster frame identity/pose invalid birth="+r.birth+" "+detail);};
        if(r.name.empty())return identity_error("member=name reason=empty bytes=0");
        if(r.name.size()>256)return identity_error("member=name reason=too_long bytes="+std::to_string(r.name.size())+" maximum=256");
        if(const auto nul=r.name.find('\0');nul!=std::string::npos)return identity_error("member=name reason=embedded_nul offset="+std::to_string(nul)+" bytes="+std::to_string(r.name.size()));
        for(const auto& field:std::array<std::pair<const char*,int32_t>,4>{{{"rank",r.rank},{"character_slot",r.slot},{"character_type",r.type},{"level",r.level}}})
            if(field.second<0)return identity_error("member="+std::string(field.first)+" actual="+std::to_string(field.second)+" required_min=0");
        if(!subscriber_motion::valid(r.pose,e)){const auto cause=e;return fail(e,cause+" birth="+r.birth+" member=pose actual="+Json({{"region",r.pose.region},{"position",r.pose.position},{"rotation",r.pose.orientation},{"scale",r.pose.scale}}).dump());}
        for(float f:r.spawn)if(!std::isfinite(f)||std::abs(f)>100000000)return fail(e,"Monster frame spawn outside finite bound");
        for(size_t i=0;i<animation_slots;++i){const auto time=r.animation[i],length=r.animation_length[i];
            if(length&&(!std::isfinite(std::bit_cast<float>(*length))||std::bit_cast<float>(*length)<=0))return fail(e,"Monster frame animation length invalid");
            if(time&&(!length||!std::isfinite(std::bit_cast<float>(*time))))return fail(e,"Monster frame animation time invalid");
            if(!std::isfinite(std::bit_cast<float>(r.animation_speed[i]))||(!time&&(r.animation_paused[i]||r.animation_speed[i])))return fail(e,"Monster frame active clip phase invalid");}
        if(!std::isfinite(std::bit_cast<float>(r.lifecycle.respawn_time_bits))||r.lifecycle.action<-1||r.lifecycle.action>=int32_t(animation_slots))return fail(e,"Monster frame lifecycle timer/action invalid");
        const auto& c=r.host_combat;for(const auto& values:{c.values,c.recovery,c.maxima})for(uint32_t value:values)if(!std::isfinite(std::bit_cast<float>(value)))return fail(e,"Monster Host combat observation not finite");
        if(!target(c.target)||!target(c.aggro))return fail(e,"Monster Host target observation invalid");
    }
    previous.clear();for(const auto& id:s.retired){if(!monster_target_key::input(id)||(!previous.empty()&&id<=previous)||!keys.insert(id).second)return fail(e,"Monster frame tombstone duplicate, unsorted or live");previous=id;}
    e.clear();return true;
}
Json encode(const State& s){Json rows=Json::array();for(const auto& r:s.rows){Json animation=Json::array(),length=Json::array();for(const auto& time:r.animation)animation.push_back(time?Json(*time):Json(nullptr));for(const auto& value:r.animation_length)length.push_back(value?Json(*value):Json(nullptr));
    const auto& c=r.host_combat;rows.push_back({{"birth",r.birth},{"name",r.name},{"tag",r.tag},{"rank",r.rank},{"character_id",{r.slot,r.type}},{"level",r.level},
        {"region",r.pose.region},{"position",r.pose.position},{"rotation",r.pose.orientation},{"scale",r.pose.scale},{"spawn",r.spawn},{"on_map",r.on_map},{"animation",animation},{"animation_length",length},{"animation_paused",r.animation_paused},{"animation_speed",r.animation_speed},
        {"lifecycle",{{"ready",r.lifecycle.ready},{"respawn_time_bits",r.lifecycle.respawn_time_bits},{"action",r.lifecycle.action}}},
        {"host_attribute_values",c.values},{"host_attribute_recovery",c.recovery},{"host_attribute_maxima",c.maxima},{"host_dead",c.dead},{"host_ai",c.ai},{"host_status",c.status},
        {"host_target_state",c.target.state},{"host_target_key",c.target.key},{"host_aggro_state",c.aggro.state},{"host_aggro_key",c.aggro.key}});}
    return {{"rows",rows},{"retired",s.retired}};
}
bool decode(const Json& j,State& out,std::string& e){out={};try{exact(j,{"rows","retired"});if(!j["rows"].is_array()||!j["retired"].is_array()||j["rows"].size()+j["retired"].size()>max_rows)throw std::runtime_error("Monster frame row/tombstone bound invalid");State s;
    for(const auto& value:j["rows"]){exact(value,{"birth","name","tag","rank","character_id","level","region","position","rotation","scale","spawn","on_map","animation","animation_length","animation_paused","animation_speed","lifecycle","host_attribute_values","host_attribute_recovery","host_attribute_maxima","host_dead","host_ai","host_status","host_target_state","host_target_key","host_aggro_state","host_aggro_key"});Row r;
        r.birth=value.at("birth").get<std::string>();r.name=value.at("name").get<std::string>();r.tag=integer(value.at("tag"));r.rank=integer(value.at("rank"));std::array<int32_t,2> id{};array(value.at("character_id"),id,integer);r.slot=id[0];r.type=id[1];r.level=integer(value.at("level"));r.pose.region=integer(value.at("region"));
        array(value.at("position"),r.pose.position,real);array(value.at("rotation"),r.pose.orientation,real);array(value.at("scale"),r.pose.scale,real);array(value.at("spawn"),r.spawn,real);r.on_map=boolean(value.at("on_map"));
        for(const auto* key:{"animation","animation_length"}){const auto& values=value.at(key);if(!values.is_array()||values.size()!=animation_slots)throw std::runtime_error("Monster animation array differs");auto& destination=std::string_view(key)=="animation"?r.animation:r.animation_length;for(size_t i=0;i<animation_slots;++i)if(!values[i].is_null())destination[i]=bits(values[i]);}
        array(value.at("animation_paused"),r.animation_paused,boolean);array(value.at("animation_speed"),r.animation_speed,bits);
        const auto& life=value.at("lifecycle");exact(life,{"ready","respawn_time_bits","action"});r.lifecycle={boolean(life.at("ready")),bits(life.at("respawn_time_bits")),integer(life.at("action"))};
        auto& c=r.host_combat;array(value.at("host_attribute_values"),c.values,bits);array(value.at("host_attribute_recovery"),c.recovery,bits);array(value.at("host_attribute_maxima"),c.maxima,bits);c.dead=boolean(value.at("host_dead"));c.ai=integer(value.at("host_ai"));c.status=integer(value.at("host_status"));
        c.target={value.at("host_target_state").get<std::string>(),value.at("host_target_key").get<std::string>()};c.aggro={value.at("host_aggro_state").get<std::string>(),value.at("host_aggro_key").get<std::string>()};s.rows.push_back(std::move(r));}
    for(const auto& id:j["retired"])s.retired.push_back(id.get<std::string>());if(!valid(s,e))return false;out=std::move(s);return true;
    }catch(const std::exception& x){return fail(e,std::string("Monster frame decode: ")+x.what());}}
bool same_identity(const Row& a,const Row& b){return a.birth==b.birth&&a.name==b.name&&a.rank==b.rank&&a.slot==b.slot&&a.type==b.type&&a.level==b.level&&a.pose.region==b.pose.region&&a.spawn[0]==b.spawn[0]&&a.spawn[2]==b.spawn[2];}
bool same_applied(const Row& a,const Row& b){
    return a.spawn==b.spawn&&a.pose==b.pose&&a.on_map==b.on_map&&a.lifecycle==b.lifecycle&&a.animation==b.animation&&
        a.animation_length==b.animation_length&&a.animation_paused==b.animation_paused&&a.animation_speed==b.animation_speed&&
        a.host_combat.values==b.host_combat.values&&a.host_combat.recovery==b.host_combat.recovery&&a.host_combat.maxima==b.host_combat.maxima&&
        a.host_combat.dead==b.host_combat.dead&&a.host_combat.ai==b.host_combat.ai&&a.host_combat.status==b.host_combat.status;
}
bool preflight(const State& local,const State& target,std::string& e){if(!valid(local,e)||!valid(target,e))return false;if(local.retired!=target.retired||local.rows.size()!=target.rows.size())return fail(e,"Monster frame complete confirmed live/tombstone birth sets differ");
    for(size_t i=0;i<local.rows.size();++i){const auto& a=local.rows[i];const auto& b=target.rows[i];if(a.animation_length!=b.animation_length)return fail(e,"Monster frame local semantic clip lengths differ birth="+b.birth);if(!same_identity(a,b)){
            const auto mismatch=[&](const char* member,const Json& local,const Json& required){return fail(e,
                "Monster frame immutable/current birth, spawn or Region differs birth="+b.birth.substr(0,160)+
                " member="+member+" local="+local.dump(-1,' ',false,Json::error_handler_t::replace)+" required="+required.dump(-1,' ',false,Json::error_handler_t::replace));};
            const auto bounded=[](const std::string& value){return Json{{"value",value.substr(0,160)},{"bytes",value.size()}};};
            if(a.birth!=b.birth)return mismatch("birth",bounded(a.birth),bounded(b.birth));
            if(a.name!=b.name)return mismatch("name",bounded(a.name),bounded(b.name));
            if(a.rank!=b.rank)return mismatch("rank",a.rank,b.rank);
            if(a.slot!=b.slot)return mismatch("character_slot",a.slot,b.slot);
            if(a.type!=b.type)return mismatch("character_type",a.type,b.type);
            if(a.level!=b.level)return mismatch("level",a.level,b.level);
            if(a.pose.region!=b.pose.region)return mismatch("region",a.pose.region,b.pose.region);
            if(a.spawn[0]!=b.spawn[0])return mismatch("spawn_x",a.spawn[0],b.spawn[0]);
            if(a.spawn[2]!=b.spawn[2])return mismatch("spawn_z",a.spawn[2],b.spawn[2]);
            return fail(e,"Monster frame immutable/current birth, spawn or Region differs birth="+b.birth.substr(0,160)+" member=unclassified");
        }
        if(a.spawn!=b.spawn&&!(a.host_combat.dead&&!b.host_combat.dead))return fail(e,"Monster spawn height changed outside observed respawn birth="+b.birth);
        if(a.host_combat.maxima!=b.host_combat.maxima)return fail(e,"Monster derived attribute maxima differ birth="+b.birth);}
    e.clear();return true;
}
bool adopted(const State& local,const State& target,std::string& e){if(!preflight(local,target,e))return false;for(size_t i=0;i<local.rows.size();++i)if(!same_applied(local.rows[i],target.rows[i]))return fail(e,"Monster frame current attributes/lifecycle/pose/animation readback differs birth="+target.rows[i].birth);e.clear();return true;}
}
