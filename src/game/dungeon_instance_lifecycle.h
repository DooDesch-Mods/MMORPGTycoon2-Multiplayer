#pragma once
#include "dungeon_scene_key.h"
#include "party_identity.h"
#include "marketing_authority/notices.h"
#include <algorithm>
#include <limits>

namespace mt2mp::game::dungeon_instance_lifecycle {
namespace journal=marketing_authority::notice;
enum class Effect:uint8_t {Make=1,Destroy=2,Reset=3,ForcePlayersOut=4};
struct Event {
    Effect effect=Effect::Make;
    std::string exterior,operation;
    uint32_t ordinal=0;
    bool action=false;
    int32_t index=-1,counter_before=-1,counter_after=-1;
    std::vector<int32_t> before,after;
    bool operator==(const Event&)const=default;
};
inline bool fail(std::string& error,const std::string& cause){error="Dungeon Instance lifecycle "+cause;return false;}
inline bool indices(const std::vector<int32_t>& values){
    std::set<int32_t> seen;for(auto value:values)if(value<0||!seen.insert(value).second)return false;return true;
}
inline bool valid(const Event& event,std::string& error){
    if(!dungeon_scene_key::building_key(event.exterior)||!party_identity::uuid(event.operation)||!event.ordinal||
       event.index<0||event.counter_before<0||event.counter_after<0||!indices(event.before)||!indices(event.after))
        return fail(error,"original operation/owner/index/counter values invalid");
    if(event.effect==Effect::Make){
        if(event.counter_before==INT32_MAX||event.index!=event.counter_before||event.counter_after!=event.counter_before+1||
           event.after.size()!=event.before.size()+1||event.after.back()!=event.index||
           !std::equal(event.before.begin(),event.before.end(),event.after.begin()))return fail(error,"original Make must append its actual next index once");
    }else if(event.effect==Effect::Destroy){
        auto after=event.before;const auto found=std::find(after.begin(),after.end(),event.index);
        if(found==after.end())return fail(error,"original Destroy target absent from its owner");
        after.erase(found);
        if(after!=event.after||event.counter_before!=event.counter_after)return fail(error,"original Destroy must preserve remaining order and next index");
    }else if(event.effect==Effect::Reset||event.effect==Effect::ForcePlayersOut){
        if(event.before!=event.after||event.counter_before!=event.counter_after||
           std::find(event.before.begin(),event.before.end(),event.index)==event.before.end())
            return fail(error,"original reset/force-out changes owner membership");
    }else return fail(error,"unknown original effect");
    error.clear();return true;
}
inline nlohmann::json encode(const Event& event){return {{"effect",uint8_t(event.effect)},{"exterior",event.exterior},
    {"operation",event.operation},{"ordinal",event.ordinal},{"action",event.action},{"index",event.index},
    {"counter_before",event.counter_before},{"counter_after",event.counter_after},{"before",event.before},{"after",event.after}};}
inline uint32_t number(const nlohmann::json& value,uint32_t limit){
    const auto result=live_host_state::vip::unsigned_value(value);
    if(result>limit)throw std::runtime_error("Dungeon Instance lifecycle integer outside domain");return uint32_t(result);
}
inline Event decode_event(const nlohmann::json& value){
    static constexpr std::array<const char*,10> fields{"effect","exterior","operation","ordinal","action","index","counter_before","counter_after","before","after"};
    if(!value.is_object()||value.size()!=fields.size())throw std::runtime_error("Dungeon Instance lifecycle exact fields required");
    for(const auto* field:fields)if(!value.contains(field))throw std::runtime_error("Dungeon Instance lifecycle field missing");
    Event event;event.effect=Effect(number(value.at("effect"),4));event.exterior=value.at("exterior").get<std::string>();
    event.operation=value.at("operation").get<std::string>();event.ordinal=number(value.at("ordinal"),UINT32_MAX);
    if(!value.at("action").is_boolean())throw std::runtime_error("Dungeon Instance lifecycle action flag requires bool");
    event.action=value.at("action").get<bool>();event.index=int32_t(number(value.at("index"),INT32_MAX));
    event.counter_before=int32_t(number(value.at("counter_before"),INT32_MAX));event.counter_after=int32_t(number(value.at("counter_after"),INT32_MAX));
    for(const auto* field:{"before","after"}){const auto& values=value.at(field);if(!values.is_array())throw std::runtime_error("Dungeon Instance lifecycle indices require array");
        auto& target=field[0]=='b'?event.before:event.after;for(const auto& item:values)target.push_back(int32_t(number(item,INT32_MAX)));}
    std::string error;if(!valid(event,error))throw std::runtime_error(error);return event;
}
inline Event event(const journal::Row& row){
    unsigned depth=0;const auto guard=[&](int level,nlohmann::json::parse_event_t,nlohmann::json&){depth=std::max(depth,unsigned(level));return level<=2;};
    const auto value=nlohmann::json::parse(row.message.begin(),row.message.end(),guard);
    if(depth>2)throw std::runtime_error("Dungeon Instance lifecycle nesting exceeds2");
    const auto decoded=decode_event(value);if(encode(decoded).dump()!=row.message)throw std::runtime_error("Dungeon Instance lifecycle payload is not canonical");return decoded;
}
inline bool valid(const journal::Batch& batch,std::string& error){
    if(!journal::valid(batch,error))return false;
    try{for(const auto& row:batch.rows)(void)event(row);error.clear();return true;}
    catch(const std::exception& e){return fail(error,e.what());}
}
inline bool append(journal::Host& host,const journal::Origin& origin,const Event& value,std::string& error){
    if(!valid(value,error))return false;return host.append(origin,encode(value).dump(),error);
}
struct Member {
    int32_t index=-1;uintptr_t instance=0,shard=0,region=0;
    bool operator==(const Member&)const=default;
};
struct Pin {
    journal::Origin origin;
    uint64_t local_epoch=0,local_generation=0,exterior_uid=0;
    uintptr_t root=0,exterior=0,dungeon=0;
    std::string key;
    int32_t counter=-1;
    std::vector<Member> members;
};
inline std::vector<int32_t> indices(const Pin& pin){std::vector<int32_t> out;for(const auto& row:pin.members)out.push_back(row.index);return out;}
inline bool same_owner(const Pin& before,const Pin& after){return before.origin==after.origin&&
    before.local_epoch==after.local_epoch&&before.local_generation==after.local_generation&&before.root==after.root&&
    before.exterior_uid==after.exterior_uid&&before.exterior==after.exterior&&before.dungeon==after.dungeon&&before.key==after.key;}
inline bool valid(const Pin& pin,std::string& error){
    if(!live_host_state::vip::valid_origin(pin.origin)||!pin.local_epoch||!pin.local_generation||!pin.root||
       !pin.exterior||!pin.exterior_uid||!pin.dungeon||!dungeon_scene_key::building_key(pin.key)||pin.counter<0)
        return fail(error,"native current world/exterior owner invalid");
    std::set<uintptr_t> instances,shards,regions;
    for(const auto& row:pin.members)if(row.index<0||!row.instance||!row.shard||!row.region||
        !instances.insert(row.instance).second||!shards.insert(row.shard).second||!regions.insert(row.region).second)
        return fail(error,"native current Instance/Shard/Region membership invalid");
    if(!indices(indices(pin)))return fail(error,"native current Instance indices repeat");error.clear();return true;
}
struct Receipt {
    bool attempted=false,returned=false,confirmed=false,observed_action=false,unknown=false;
    uintptr_t made_instance=0;
    uint32_t calls=0,counter_stores=0;
    std::string cause;
};
struct Port {
    virtual ~Port()=default;
    virtual bool current(const std::string& exterior,Pin&,std::string&)=0;
    // True requires a retained actual original-action hook receipt and fresh
    // bound world. The owning building may already be gone after that action.
    // Matching values cannot prove that Reset or ForcePlayersOut already ran.
    virtual bool observed(const Event&,bool&,std::string&)=0;
    // Adopt the Host's genuine pre-call next-index scalar before the original.
    // DesiredInstances can reset it outside these four calls; that simulation
    // does not run independently on Client. Preserve all surviving instances.
    virtual bool invoke(const Pin&,const Event&,uintptr_t target,Receipt&,std::string&)=0;
};
class Executor {
    Port& native_;
    std::string failure_;
public:
    explicit Executor(Port& native):native_(native){}
    bool apply(const Event& value,Receipt& receipt,std::string& error){
        receipt={};if(!failure_.empty())return fail(error,failure_);
        if(!valid(value,error))return false;
        bool observed=false;
        if(value.action&&!native_.observed(value,observed,error))return false;
        const auto reject=[&](const std::string& cause){receipt.unknown=receipt.attempted;receipt.cause=cause;
            if(receipt.unknown)failure_=cause;return fail(error,cause);};
        if(observed){
            receipt.observed_action=true;receipt.confirmed=true;error.clear();return true;
        }
        Pin before;if(!native_.current(value.exterior,before,error)||!valid(before,error))return false;
        if(before.key!=value.exterior)return reject("resolved exterior key differs");
        if(indices(before)!=value.before)return reject("actual original membership pre-state differs");
        uintptr_t target=0;
        if(value.effect!=Effect::Make){const auto found=std::find_if(before.members.begin(),before.members.end(),[&](const auto& row){return row.index==value.index;});
            if(found==before.members.end())return reject("original target missing");target=found->instance;}
        receipt.attempted=true;++receipt.calls;
        if(!native_.invoke(before,value,target,receipt,error)||!receipt.returned)return reject(error.empty()?"original invocation failed":error);
        Pin after;if(!native_.current(value.exterior,after,error)||!valid(after,error)||!same_owner(before,after)||
            indices(after)!=value.after||after.counter!=value.counter_after)return reject(error.empty()?"original result/owner readback differs":error);
        if(value.effect==Effect::Make){
            const auto& born=after.members.back();if(!receipt.made_instance||born.instance!=receipt.made_instance||born.index!=value.index)
                return reject("original Make return is not its actual appended Instance");
            for(const auto& row:before.members)if(std::find(after.members.begin(),after.members.end(),row)==after.members.end())
                return reject("original Make replaced an existing Instance");
        }else{
            auto expected=before.members;if(value.effect==Effect::Destroy)std::erase_if(expected,[&](const auto& row){return row.index==value.index;});
            if(expected!=after.members)return reject("original effect replaced or reordered a surviving native Instance");
        }
        receipt.confirmed=true;error.clear();return true;
    }
};
}
