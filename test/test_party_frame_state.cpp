#include "party_frame_state.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace frame=mt2mp::game::party_frame;
namespace party=mt2mp::game::party_live;
namespace identity=mt2mp::game::party_identity;
namespace original=mt2mp::subscriber_lifecycle;
using Json=nlohmann::json;
const std::string operation="aaaaaaaa-bbbb-cccc-dddd-123456789abc";
const std::string exterior="action:"+operation;
unsigned checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
party::Record record(uint32_t ordinal=0){
    party::Record row;row.key=identity::birth(operation,ordinal);
    row.shard="root_shard";row.region=row.manager_region="region:0";
    row.managers={{"region:0",false,false},{"region:1",true,true}};
    row.records={
        {SIZE_MAX,0,{1,0,"mmoParty"},{}},
        {0,0,{1,0,"linkId"},{{4,1,{}}}},
        {0,1,{1,0,"mode"},{{4,3,{}}}},
        {0,2,{1,0,"member"},{{4,2,{}}}},
        {0,3,{1,0,"member"},{{4,3,{}}}},
        {0,4,{1,0,"ordinary_integer"},{{4,2,{}},{4,UINT32_MAX,{}}}},
        {0,5,{1,0,"native_extra_field"},{{3,0x3f812345,{}},{2,0,"quoted \"value\"\nnext"},{0,0,{}}}}};
    row.root_link=1;row.local_ids={1};row.external={{2,"toon:1"},{3,"toon:2"}};row.toons={"toon:1","toon:2"};
    std::string error;if(!party::record_detail::summarize(row,row.sha,error))throw std::runtime_error(error);
    return row;
}
bool rejects_record(const Json& value){try{(void)frame::decode_record(value);return false;}catch(const std::exception&){return true;}}
bool rejects_wire(const std::string& wire){try{(void)frame::record(wire);return false;}catch(const std::exception&){return true;}}
frame::State state(){
    frame::State value;value.parties={frame::encode_record(record()).dump()};
    value.pools={{exterior,1,{9,2}},{"action:bbbbbbbb-bbbb-cccc-dddd-123456789abc",0,{}}};
    mt2mp::game::dungeon_instance_live::State first;
    first.exterior=exterior;first.index=2;first.scene=exterior+"/instance/2";first.party=record().key;
    first.online_fraction=0.5f;first.timeout=-0.25f;first.completion=2.0f;
    first.wants_online=true;first.timing_out=true;first.used=true;first.completed=false;
    auto second=first;second.index=9;second.scene=exterior+"/instance/9";second.party.clear();
    value.instances={first,second};return value;
}
int main()try{
    std::string error;const auto original=record();const auto encoded=frame::encode_record(original);
    const auto wire=encoded.dump();const auto roundtrip=frame::record(wire);
    require(party::validate(roundtrip,error),"complete original Record validates after canonical round trip");
    require(frame::encode_record(roundtrip)==encoded,"complete original fields, owner manifests and tokens survive codec");
    require(roundtrip.records[5].tokens[0].bits==2&&roundtrip.records[3].tokens[0].bits==2,
        "ordinary integer equal to link ID remains an unchanged original integer");
    require(roundtrip.records[5].tokens[1].bits==UINT32_MAX,"signed native integer retains all original bits");
    require(roundtrip.records[6].tokens[0].bits==0x3f812345,"float bit pattern remains exact");
    require(roundtrip.managers==original.managers&&roundtrip.toons==original.toons,"ordered member and complete manager flags survive");
    {
        auto bad=encoded;bad["sha"]=std::string(64,'0');require(rejects_record(bad),"corrupt full Record checksum rejected");
        bad=encoded;bad["records"][5][3][0][1]=uint32_t(7);require(rejects_record(bad),"uninterpreted original integer cannot evade checksum");
        bad=encoded;bad["external"][0][1]="toon:other";require(rejects_record(bad),"external canonical pointer manifest is checksum-bound");
        bad=encoded;bad["local_ids"]=Json::array();require(rejects_record(bad),"local inline manifest cannot disappear");
        bad=encoded;bad["external"].push_back(bad["external"][0]);require(rejects_record(bad),"duplicate saved link IDs rejected");
        bad=encoded;bad["toons"].push_back("toon:1");require(rejects_record(bad),"duplicate ordered Party member rejected");
        bad=encoded;bad["toons"]={"toon:missing"};require(rejects_record(bad),"Party member must have actual external reference");
        bad=encoded;bad["managers"].push_back(bad["managers"][0]);require(rejects_record(bad),"duplicate actual manager owner rejected");
        bad=encoded;bad["extra"]=0;require(rejects_record(bad),"unrecognized Record field rejected");
        bad=encoded;bad.erase("root_link");require(rejects_record(bad),"missing Record field rejected");
    }
    require(rejects_wire("{\"sha\":"+Json(original.sha).dump()+","+wire.substr(1)),"duplicate JSON field is rejected before normal JSON overwrite");
    require(rejects_wire(" "+wire),"noncanonical whitespace cannot create a second original receipt encoding");
    require(rejects_wire("{}"),"incomplete original Record wire rejected");
    for(const Json integer:{Json(-1),Json(uint64_t(INT32_MAX)+1),Json(1.0),Json(true)}){
        auto bad=encoded;bad["root_link"]=integer;require(rejects_record(bad),"root link signed/range/type domain enforced");
        bad=encoded;bad["local_ids"][0]=integer;require(rejects_record(bad),"local saved ID signed/range/type domain enforced");
        bad=encoded;bad["external"][0][0]=integer;require(rejects_record(bad),"external saved ID signed/range/type domain enforced");
    }
    for(const Json integer:{Json(-1),Json(uint64_t(UINT32_MAX)+1),Json(1.0),Json(true)}){
        auto bad=encoded;bad["records"][5][3][1][1]=integer;require(rejects_record(bad),"token bit domain cannot truncate or convert signed input");
    }
    for(const Json integer:{Json(-1),Json(5),Json(1.0),Json(true)}){
        auto bad=encoded;bad["records"][5][3][0][0]=integer;require(rejects_record(bad),"original token type domain enforced");
    }
    {
        auto bad=encoded;bad["records"][1][0]=-1;require(rejects_record(bad),"negative record parent rejected");
        bad=encoded;bad["records"][1][1]=-1;require(rejects_record(bad),"negative original child index rejected");
        bad=encoded;bad["managers"][0][1]=1;require(rejects_record(bad),"manager Boolean cannot accept integer");
    }
    const auto valid=state();frame::State decoded;
    require(frame::valid(valid,error),"complete pools use actual native order/index and permit reset counter below surviving indices");
    require(frame::decode(frame::encode(valid),decoded,error)&&decoded==valid,"full Party/Instance state round trip");
    require(decoded.instances[0].timeout==-0.25f,"negative original timeout is preserved");
    {
        auto bad=valid;bad.parties.push_back(bad.parties[0]);require(!frame::valid(bad,error),"duplicate complete Party key rejected");
        bad=valid;bad.parties={frame::encode_record(record(1)).dump(),valid.parties[0]};require(!frame::valid(bad,error),"unordered complete Party keys rejected");
        bad=valid;bad.parties.clear();require(!frame::valid(bad,error),"assigned Party must exist in complete Party state");
        bad=valid;bad.pools.erase(bad.pools.begin());require(!frame::valid(bad,error),"Instance cannot exist without its actual pool");
        bad=valid;bad.instances.pop_back();require(!frame::valid(bad,error),"complete pool cannot omit saved Instance values");
        bad=valid;bad.pools[0].indices.pop_back();require(!frame::valid(bad,error),"saved Instance cannot appear outside pool membership");
        bad=valid;bad.pools[0].indices={2,2};require(!frame::valid(bad,error),"native indices cannot repeat in one pool");
        bad=valid;bad.pools[0].counter=-1;require(!frame::valid(bad,error),"negative actual pool counter rejected");
        bad=valid;std::reverse(bad.pools.begin(),bad.pools.end());require(!frame::valid(bad,error),"unordered complete owner pools rejected");
        bad=valid;bad.instances[1]=bad.instances[0];require(!frame::valid(bad,error),"duplicate saved Instance scene rejected");
        bad=valid;std::reverse(bad.instances.begin(),bad.instances.end());require(!frame::valid(bad,error),"saved Instance canonical scene order enforced");
        bad=valid;bad.instances[0].index=9;require(!frame::valid(bad,error),"native index and canonical scene cannot diverge");
        bad=valid;bad.instances[0].online_fraction=std::numeric_limits<float>::infinity();require(!frame::valid(bad,error),"nonfinite native scalar rejected");
    }
    for(const Json integer:{Json(-1),Json(uint64_t(INT32_MAX)+1),Json(1.0),Json(true)}){
        auto bad=frame::encode(valid);bad["pools"][0][1]=integer;
        require(!frame::decode(bad,decoded,error)&&decoded==frame::State{},"pool counter wire domain rejects without partial output");
        bad=frame::encode(valid);bad["pools"][0][2][0]=integer;
        require(!frame::decode(bad,decoded,error)&&decoded==frame::State{},"pool index wire domain rejects without partial output");
        bad=frame::encode(valid);bad["instances"][0]["index"]=integer;
        require(!frame::decode(bad,decoded,error)&&decoded==frame::State{},"Instance index wire domain rejects without partial output");
    }
    auto bad=frame::encode(valid);bad["instances"][0]["used"]=1;
    require(!frame::decode(bad,decoded,error),"Instance Boolean cannot accept integer");
    bad=frame::encode(valid);bad["instances"][0]["timeout"]="0";
    require(!frame::decode(bad,decoded,error),"Instance float cannot accept string");
    bad=frame::encode(valid);bad["extra"]=0;
    require(!frame::decode(bad,decoded,error),"Frame fields remain exact");
    {
        auto mapped=valid;mt2mp::game::scene_clone::Mapping mapping;
        mapping.operation="10000000-0000-4000-8000-000000000001";
        mapping.scenes.push_back({mapped.instances[0].scene,1,0,0});
        const auto key=mt2mp::game::scene_clone::birth_key(mapping.operation,mt2mp::game::scene_clone::Family::Gizmo,0);
        mapping.members.push_back({mapping.scenes[0].scene,key,mt2mp::game::scene_clone::Family::Gizmo,0,0,mapping.operation});
        mapping.scenes[0].buildings=1;
        const auto building_key=mt2mp::game::scene_clone::birth_key(mapping.operation,mt2mp::game::scene_clone::Family::Building,1);
        mapping.members.push_back({mapping.scenes[0].scene,building_key,mt2mp::game::scene_clone::Family::Building,0,1,mapping.operation});
        mapping.scenes[0].scenery=1;
        const auto scenery_key=mt2mp::game::scene_clone::birth_key(mapping.operation,mt2mp::game::scene_clone::Family::Scenery,2);
        mapping.members.push_back({mapping.scenes[0].scene,scenery_key,mt2mp::game::scene_clone::Family::Scenery,0,2,mapping.operation});
        mapped.scenes=mapping;
        require(frame::valid(mapped,error),"complete native scene mapping admitted with Party state");
        require(frame::decode(frame::encode(mapped),decoded,error)&&decoded==mapped,"full clone birth provenance survives Party wire round trip");
        auto corrupt=frame::encode(mapped);corrupt["scenes"]["members"][0][4]="invalid";
        require(!frame::decode(corrupt,decoded,error),"corrupt nested clone key cannot enter Party Frame");
    }
    std::cout<<checks<<" Party Frame state checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
