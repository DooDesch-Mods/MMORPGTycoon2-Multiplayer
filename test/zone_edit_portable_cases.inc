#include "typed_action/zone_edit_codec.h"
#include <limits>

namespace zone_edit_portable_fixture {
namespace zone58=mt2mp::action_session;
namespace wire58=zone58::zone_edit_codec;
inline void require(bool okay,const char* cause){if(!okay)throw std::runtime_error(cause);}
inline std::string selected58(){return std::string(64,'a')+":world:zone:17";}
inline zone58::ZoneEditIntent stroke58(int32_t type=1){
    zone58::ZoneEditIntent value;value.region=67;value.zone_type=type;value.character_type=2;value.level=5;
    value.press={std::bit_cast<uint32_t>(8020.f),0x80000000,std::bit_cast<uint32_t>(8550.f)};
    zone58::ZoneStamp stamp;stamp.position={8020.f,std::bit_cast<float>(uint32_t(0x80000000)),8550.f};
    stamp.shape={{{7988000,8518000},{8051000,8550000},{8020000,8581000}},{{8000000,8530000},{8010000,8540000},{8010000,8530000}}};
    value.stamps={stamp};return value;
}
inline zone58::ZoneEditIntent selected_operation58(zone58::ZoneEditKind kind){
    zone58::ZoneEditIntent value;value.kind=kind;value.region=67;value.selected=selected58();
    if(kind==zone58::ZoneEditKind::Apply){value.zone_type=1;value.character_type=2;value.level=5;value.density=0;}
    return value;
}
inline zone58::ZoneEditState state58(char hash){
    zone58::ZoneEditState value;value.values_sha=std::string(64,hash);value.quests_sha=std::string(64,'b');
    value.references_sha=std::string(64,'c');value.counts={1,2,3,4};value.monsters=23;
    value.global_random={0,1,uint64_t(1)<<40,UINT64_MAX};value.region_random={UINT64_MAX,5,8,13};
    value.next_tag=31;value.created_monsters=29;value.release_edits={0,1,2,3,4,5,6,7};return value;
}
inline unsigned run(){
    unsigned checks58=0;
    const auto accept_intent=[&](const zone58::ZoneEditIntent& value){
        require(zone58::valid_zone_edit_intent(value),"Zone58 valid original intent rejected");
        const auto encoded=wire58::intent_json(value);const auto decoded=wire58::intent_read(encoded);
        require(decoded==value&&wire58::intent_json(decoded)==encoded,"Zone58 complete intent/float-bit roundtrip differs");checks58+=2;
    };
    const auto reject_read=[&](const wire58::Json& encoded){
        bool rejected=false;try{static_cast<void>(wire58::intent_read(encoded));}catch(const std::exception&){rejected=true;}
        require(rejected,"Malformed Zone58 intent accepted");++checks58;
    };
    const auto reject_intent=[&](const zone58::ZoneEditIntent& value){
        require(!zone58::valid_zone_edit_intent(value),"Invalid Zone58 typed intent accepted");
        bool rejected=false;try{static_cast<void>(wire58::intent_json(value));}catch(const std::exception&){rejected=true;}
        require(rejected,"Invalid Zone58 typed intent encoded");checks58+=2;
    };
    for(int32_t type58=0;type58<4;++type58){
        auto value=stroke58(type58);accept_intent(value);value.selected=selected58();accept_intent(value);
        value.selected.clear();value.add=false;accept_intent(value);
        value.selected=selected58();reject_intent(value);
    }
    for(int32_t density58=0;density58<4;++density58){auto value=selected_operation58(zone58::ZoneEditKind::Apply);value.density=density58;accept_intent(value);}
    for(const auto kind58:{zone58::ZoneEditKind::Reset,zone58::ZoneEditKind::Respawn,zone58::ZoneEditKind::Delete}){
        auto value=selected_operation58(kind58);accept_intent(value);
        value.character_type=0;reject_intent(value);value=selected_operation58(kind58);value.press[0]=0x80000000;reject_intent(value);
        value=selected_operation58(kind58);value.selected.clear();reject_intent(value);
    }
    auto bad58=stroke58();bad58.kind=zone58::ZoneEditKind(0);reject_intent(bad58);
    bad58=stroke58();bad58.kind=zone58::ZoneEditKind(6);reject_intent(bad58);
    bad58=stroke58();bad58.zone_type=4;reject_intent(bad58);
    bad58=stroke58();bad58.character_type=-1;reject_intent(bad58);
    bad58=stroke58();bad58.region=-1;reject_intent(bad58);
    bad58=stroke58();bad58.level=0;reject_intent(bad58);
    bad58=stroke58();bad58.density=0;reject_intent(bad58);
    bad58=stroke58();bad58.stamps.clear();reject_intent(bad58);
    bad58=stroke58();bad58.stamps.resize(65,bad58.stamps.front());reject_intent(bad58);
    bad58=stroke58();bad58.press[0]=0x7f800000;reject_intent(bad58);
    bad58=stroke58();bad58.stamps[0].position[0]=std::numeric_limits<float>::quiet_NaN();reject_intent(bad58);
    bad58=stroke58();bad58.stamps[0].shape[0].resize(2);reject_intent(bad58);
    bad58=stroke58();bad58.stamps[0].shape[0][0].x=INT64_MAX;reject_intent(bad58);
    bad58=stroke58();bad58.selected="uid:123";reject_intent(bad58);
    for(int32_t density58:{-1,4}){bad58=selected_operation58(zone58::ZoneEditKind::Apply);bad58.density=density58;reject_intent(bad58);}
    bad58=selected_operation58(zone58::ZoneEditKind::Apply);bad58.zone_type=0;reject_intent(bad58);
    bad58=selected_operation58(zone58::ZoneEditKind::Apply);bad58.add=false;reject_intent(bad58);
    auto bounded58=stroke58();bounded58.stamps[0].shape={zone58::TerrainPath(1024,{INT64_MAX>>2,-(INT64_MAX>>2)})};accept_intent(bounded58);
    bounded58.stamps[0].shape[0].push_back({0,0});reject_intent(bounded58);
    bounded58=stroke58();bounded58.stamps.resize(64,bounded58.stamps.front());accept_intent(bounded58);
    const auto canonical58=wire58::intent_json(stroke58());
    for(const auto* field58:{"kind","region","zone_type","character_type","level","density"}){
        for(const auto& wrong58:{wire58::Json(true),wire58::Json(1.0),wire58::Json("1"),wire58::Json(uint64_t(UINT64_MAX))}){
            auto encoded=canonical58;encoded[field58]=wrong58;reject_read(encoded);
        }
    }
    {auto encoded=canonical58;encoded["extra"]=0;reject_read(encoded);}
    {auto encoded=canonical58;encoded.erase("selected");reject_read(encoded);}
    {auto encoded=canonical58;encoded["add"]=1;reject_read(encoded);}
    {auto encoded=canonical58;encoded["press"][0]=uint64_t(UINT32_MAX)+1;reject_read(encoded);}
    {auto encoded=canonical58;encoded["press"][0]=1.0;reject_read(encoded);}
    {auto encoded=canonical58;encoded["press"].push_back(0);reject_read(encoded);}
    {auto encoded=canonical58;encoded["stamps"][0]["position"][0]=-1;reject_read(encoded);}
    {auto encoded=canonical58;encoded["stamps"][0]["position"][0]=0x7f800000;reject_read(encoded);}
    {auto encoded=canonical58;encoded["stamps"][0]["paths"][0].push_back(0);reject_read(encoded);}
    {auto encoded=canonical58;encoded["stamps"][0]["paths"][0][0]=true;reject_read(encoded);}
    {auto encoded=canonical58;encoded["stamps"][0]["paths"][0][0]=uint64_t(UINT64_MAX);reject_read(encoded);}
    auto proof58=state58('a');const auto encoded_state58=wire58::state_json(proof58);
    require(wire58::state_read(encoded_state58)==proof58,"Zone58 complete authored/reference state roundtrip differs");++checks58;
    const auto reject_state58=[&](const wire58::Json& encoded){bool rejected=false;
        try{static_cast<void>(wire58::state_read(encoded));}catch(const std::exception&){rejected=true;}
        require(rejected,"Malformed Zone58 RNG/counter state accepted");++checks58;};
    for(const auto* field58:{"global_random","region_random"}){
        {auto encoded=encoded_state58;encoded.erase(field58);reject_state58(encoded);}
        for(unsigned dimension58:{3,5}){auto encoded=encoded_state58;encoded[field58]=std::vector<int>(dimension58,0);reject_state58(encoded);}
        for(const auto& invalid58:{wire58::Json(-1),wire58::Json(true),wire58::Json(1.0)}){
            auto encoded=encoded_state58;encoded[field58][0]=invalid58;reject_state58(encoded);}
    }
    for(const auto* field58:{"next_tag","created_monsters"}){
        for(const auto& invalid58:{wire58::Json(-1),wire58::Json(true),wire58::Json(1.0),wire58::Json(uint64_t(INT32_MAX)+1)}){
            auto encoded=encoded_state58;encoded[field58]=invalid58;reject_state58(encoded);}
    }
    for(unsigned dimension58:{7,9}){auto encoded=encoded_state58;encoded["release_edits"]=std::vector<int>(dimension58,0);reject_state58(encoded);}
    for(const auto& invalid58:{wire58::Json(true),wire58::Json(1.0),wire58::Json(uint64_t(INT32_MAX)+1)}){
        auto encoded=encoded_state58;encoded["release_edits"][5]=invalid58;reject_state58(encoded);}
    {auto altered58=proof58;altered58.global_random[3]^=uint64_t(1)<<63;
        require(wire58::state_json(altered58)!=encoded_state58,"Zone58 high RNG bits were omitted");++checks58;}
    {auto altered58=proof58;++altered58.region_random[0];
        require(wire58::state_json(altered58)!=encoded_state58,"Zone58 Region RNG was omitted");++checks58;}
    {auto altered58=proof58;altered58.next_tag=-1;require(!zone58::valid_zone_edit_state(altered58),"Zone58 negative tag accepted");++checks58;}
    {auto altered58=proof58;altered58.created_monsters=-1;require(!zone58::valid_zone_edit_state(altered58),"Zone58 negative creation count accepted");++checks58;}
    require(!zone58::valid_zone_edit_state({}),"Zone58 empty state accepted as full capture");++checks58;
    for(const auto* hash58:{"values_sha","quests_sha","references_sha"}){
        auto encoded=encoded_state58;encoded[hash58]=std::string(64,'A');bool rejected=false;
        try{static_cast<void>(wire58::state_read(encoded));}catch(const std::exception&){rejected=true;}
        require(rejected,"Zone58 invalid state witness accepted");++checks58;
    }
    for(unsigned dimension58:{3,5}){auto encoded=encoded_state58;encoded["counts"]=std::vector<int>(dimension58,0);bool rejected=false;
        try{static_cast<void>(wire58::state_read(encoded));}catch(const std::exception&){rejected=true;}
        require(rejected,"Zone58 incomplete four-type census accepted");++checks58;
    }
    {auto encoded=encoded_state58;encoded["monsters"]=uint64_t(UINT32_MAX)+1;bool rejected=false;
        try{static_cast<void>(wire58::state_read(encoded));}catch(const std::exception&){rejected=true;}
        require(rejected,"Zone58 overflowing Monster count accepted");++checks58;}
    zone58::ZoneEditEffect effect58{proof58,state58('d'),std::string(64,'e'),2};
    require(wire58::effect_read(wire58::effect_json(effect58))==effect58,"Zone58 full effect/inverse roundtrip differs");++checks58;
    for(const auto kind58:{zone58::ZoneEditKind::Stroke,zone58::ZoneEditKind::Apply,zone58::ZoneEditKind::Reset,zone58::ZoneEditKind::Respawn,zone58::ZoneEditKind::Delete}){
        const auto intent58=kind58==zone58::ZoneEditKind::Stroke?stroke58():selected_operation58(kind58);
        require(zone58::valid_zone_edit_effect(intent58,effect58),"Zone58 original committed inverse rejected");++checks58;
        auto absent58=effect58;absent58.inverse_sha.clear();absent58.inverse_operations=0;
        require(zone58::valid_zone_edit_effect(intent58,absent58)==(kind58==zone58::ZoneEditKind::Respawn),"Zone58 operation lost its required inverse");++checks58;
    }
    auto broken58=effect58;broken58.inverse_operations=0;require(!zone58::valid_zone_edit_effect(broken58),"Zone58 orphan inverse hash accepted");++checks58;
    broken58=effect58;broken58.inverse_sha.clear();require(!zone58::valid_zone_edit_effect(broken58),"Zone58 graph count without hash accepted");++checks58;
    broken58=effect58;broken58.before.quests_sha.clear();require(!zone58::valid_zone_edit_effect(broken58),"Zone58 omitted Quest witness accepted");++checks58;
    constexpr std::string_view action58="11111111-1111-4111-8111-111111111111",history58="22222222-2222-4222-8222-222222222222";
    const std::string key58=std::string("zone-edit:")+std::string(action58);
    require(wire58::valid_scope(36,58,key58,action58,{}),"Zone58 strict new36 action rejected");++checks58;
    for(uint64_t operation58:{10,11}){require(wire58::valid_scope(36,operation58,key58,history58,action58),"Zone58 matching inverse family rejected");++checks58;}
    for(uint64_t schema58:{25,35,37}){require(!wire58::valid_scope(schema58,58,key58,action58,{}),"Zone58 wrong schema accepted");++checks58;}
    for(uint64_t other58:{23,57,59,60,66}){require(!wire58::valid_scope(36,other58,key58,action58,{}),"Zone58 payload escaped into another action");++checks58;}
    require(!wire58::valid_scope(36,10,key58,action58,action58),"Zone58 inverse self-target accepted");++checks58;
    require(!wire58::valid_scope(36,11,"modular:"+std::string(action58),history58,action58),"Zone58 inverse crossed native history family");++checks58;
    require(!wire58::valid_scope(36,58,key58,action58,history58),"Zone58 forward action carried inverse source");++checks58;
    return checks58;
}
}
inline unsigned zone_edit_portable_cases(){return zone_edit_portable_fixture::run();}
