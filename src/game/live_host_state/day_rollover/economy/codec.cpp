#include "codec.h"
#include <set>
namespace mt2mp::game::live_host_state::day::economy {
namespace {
using Json=nlohmann::json;
void exact(const Json& row,std::initializer_list<const char*> names){
    if(!row.is_object()||row.size()!=names.size())throw std::runtime_error("economy exact object fields required");
    for(auto name:names)if(!row.contains(name))throw std::runtime_error(std::string("economy missing field=")+name);
}
int32_t i32(const Json& value){
    if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>uint64_t(INT32_MAX):value.get<int64_t>()<INT32_MIN||value.get<int64_t>()>INT32_MAX))throw std::runtime_error("economy int32 required");
    return value.get<int32_t>();
}
uint32_t u32(const Json& value){
    if(!value.is_number_integer()||(value.is_number_unsigned()?value.get<uint64_t>()>UINT32_MAX:value.get<int64_t>()<0||value.get<int64_t>()>UINT32_MAX))throw std::runtime_error("economy uint32 required");
    return value.get<uint32_t>();
}
template<size_t N>void integers(const Json& values,std::array<int32_t,N>& out){
    if(!values.is_array()||values.size()!=N)throw std::runtime_error("economy exact native scalar array required");
    for(size_t i=0;i<N;++i)out[i]=i32(values.at(i));
}
template<class T>void native_rows(const Json& rows,std::vector<T>& out){
    if(!rows.is_array())throw std::runtime_error("economy native rows array required");
    for(const auto& row:rows){
        exact(row,{"text","amounts"});T target;const auto& text=row.at("text");
        if(!text.is_array()||text.size()!=3)throw std::runtime_error("economy exact three native strings required");
        for(size_t i=0;i<3;++i){if(!text.at(i).is_string())throw std::runtime_error("economy native string required");target.text[i]=text.at(i).get<std::string>();}
        integers(row.at("amounts"),target.amounts);out.push_back(std::move(target));
    }
}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "quest_codec.inc"
}
bool encode(const State& state,std::vector<uint8_t>& out,std::string& error){
    out.clear();if(!valid(state,error)||!mt2mp::subscriber_lifecycle::additions::valid(state.additions,error))return false;
    std::vector<uint8_t> day_bytes;if(!day::encode(state.day,day_bytes,error))return false;
    try{
        Json subscribers=Json::array(),loans=Json::array(),offers=Json::array(),retired=Json::array();
        for(const auto& row:state.additions.retired)retired.push_back({{"id",row.id},{"reason",row.reason}});
        for(const auto& row:state.subscribers){Json pose=nullptr;if(row.motion)pose={{"region",row.motion->region},{"orientation",row.motion->orientation},{"position",row.motion->position},{"scale",row.motion->scale}};subscribers.push_back({{"id",row.serialized_id},{"pay",row.pay},{"play_days",row.play_days},{"needs_bits",row.needs_bits},{"satisfaction",row.activity.satisfaction},{"addiction",row.activity.addiction},{"logged_in_time_bits",row.activity.logged_in_time_bits},{"motion",pose},{"rewards",row.rewards},{"quest_state",toon_json(row.quest_state)}});}
        for(const auto& row:state.loans)loans.push_back({{"text",row.text},{"amounts",row.amounts}});
        for(const auto& row:state.offers)offers.push_back({{"text",row.text},{"amounts",row.amounts}});
        const auto text=Json{{"schema",11},{"kind","host_state_economy"},{"day",Json::parse(day_bytes)},
            {"subscribers",subscribers},{"npcs",npcs_json(state.npcs)},{"loans",loans},{"offers",offers},{"additions",{{"available",state.additions.available},{"next_id",state.additions.next_id},{"baseline",state.additions.baseline},{"graphs",state.additions.graphs},{"retired",retired}}}}.dump();
        if(text.size()>max_frame_payload)return fail(error,"economy frame exceeds 65536-byte Channel9 transport bound bytes="+std::to_string(text.size()));
        out.assign(text.begin(),text.end());error.clear();return true;
    }catch(const std::exception& x){return fail(error,std::string("economy encode original=")+x.what());}
}
bool decode(const uint8_t* bytes,size_t size,State& out,std::string& error){
    out={};if(!bytes||!size||size>max_frame_payload)return fail(error,"economy frame length invalid");
    try{
        std::vector<std::set<std::string>> fields;
        const auto json=Json::parse(bytes,bytes+size,[&](int depth,Json::parse_event_t event,Json& value){
            if(depth>10)throw std::runtime_error("economy nesting exceeds10");
            if(event==Json::parse_event_t::object_start)fields.emplace_back();
            if(event==Json::parse_event_t::key&&!fields.back().insert(value.get<std::string>()).second)throw std::runtime_error("duplicate economy JSON field");
            if(event==Json::parse_event_t::object_end)fields.pop_back();return true;
        });
        exact(json,{"schema","kind","day","subscribers","npcs","loans","offers","additions"});
        if(i32(json.at("schema"))!=11||json.at("kind")!="host_state_economy"||!json.at("subscribers").is_array())throw std::runtime_error("economy exact schema11 required");
        State target;const auto day_text=json.at("day").dump();
        if(!day::decode(reinterpret_cast<const uint8_t*>(day_text.data()),day_text.size(),target.day,error))return false;
        for(const auto& row:json.at("subscribers")){
            exact(row,{"id","pay","play_days","needs_bits","satisfaction","addiction","logged_in_time_bits","motion","rewards","quest_state"});Subscriber subscriber;
            subscriber.serialized_id=i32(row.at("id"));integers(row.at("pay"),subscriber.pay);subscriber.play_days=u32(row.at("play_days"));
            subscriber.activity={i32(row.at("satisfaction")),i32(row.at("addiction")),u32(row.at("logged_in_time_bits"))};
            bit_patterns(row.at("rewards"),subscriber.rewards);subscriber.quest_state=toon_read(row.at("quest_state"));
            const auto& needs=row.at("needs_bits");if(!needs.is_array()||needs.size()!=7)throw std::runtime_error("Subscriber Needs exact seven uint32 patterns required");
            for(size_t i=0;i<7;++i)subscriber.needs_bits[i]=u32(needs.at(i));
            if(!row.at("motion").is_null()){
                const auto& pose=row.at("motion");exact(pose,{"region","orientation","position","scale"});subscriber_motion::Pose motion;motion.region=i32(pose.at("region"));
                auto floats=[](const Json& values,auto& out){if(!values.is_array()||values.size()!=out.size())throw std::runtime_error("Subscriber motion exact float array required");for(size_t i=0;i<out.size();++i){if(!values.at(i).is_number())throw std::runtime_error("Subscriber motion float required");out[i]=values.at(i).template get<float>();}};
                floats(pose.at("orientation"),motion.orientation);floats(pose.at("position"),motion.position);floats(pose.at("scale"),motion.scale);subscriber.motion=motion;
            }
            target.subscribers.push_back(subscriber);
        }
        const auto& add=json.at("additions");exact(add,{"available","next_id","baseline","graphs","retired"});
        if(!add.at("available").is_boolean()||!add.at("baseline").is_array()||!add.at("graphs").is_array()||!add.at("retired").is_array())throw std::runtime_error("Subscriber additions fields invalid");
        target.additions.available=add.at("available").get<bool>();target.additions.next_id=i32(add.at("next_id"));
        for(const auto& id:add.at("baseline"))target.additions.baseline.push_back(i32(id));
        for(const auto& graph:add.at("graphs")){if(!graph.is_string())throw std::runtime_error("Subscriber graph compact text required");target.additions.graphs.push_back(graph.get<std::string>());}
        for(const auto& row:add.at("retired")){exact(row,{"id","reason"});target.additions.retired.push_back({i32(row.at("id")),i32(row.at("reason"))});}
        native_rows(json.at("loans"),target.loans);native_rows(json.at("offers"),target.offers);
        npcs_read(json.at("npcs"),target.npcs);
        if(!valid(target,error)||!mt2mp::subscriber_lifecycle::additions::valid(target.additions,error))return false;out=std::move(target);error.clear();return true;
    }catch(const std::exception& x){return fail(error,std::string("economy decode original=")+x.what());}
}
}
