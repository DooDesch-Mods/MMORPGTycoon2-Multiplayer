#include "npc_quest_authoring_fixture.h"
#include "live_host_state/monster_state.inc"
#include <iostream>
namespace monsters=mt2mp::game::live_host_state::monsters;
int main()try{
    const auto saved=std::string(64,'a')+":world:monster:inline:region{67}/monsterManager/monster[1]";
    std::string error;std::vector<uint8_t> bytes;Message decoded;unsigned cases=0;
    auto roundtrip=[&](const Message& value){need(encode(value,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==value,"saved Monster request/result roundtrip differs");++cases;};
    auto quest=request();quest.request.npc_quest.destination_key=saved;quest.request.npc_quest.before.destination_key=saved;
    roundtrip(quest);
    auto committed=result();committed.request=quest.request;committed.evidence.quest_before.record=quest.request.npc_quest.before;
    committed.evidence.quest_after.record.destination_key=saved;roundtrip(committed);
    for(const auto action:{Action::MoveMonster,Action::RemoveMonster}){
        auto value=request();auto& r=value.request;r.action=action;r.npc_quest={};r.canonical_record_key=saved;r.target_action_uuid.clear();
        if(action==Action::MoveMonster)r.monster_move={67,{8020,70,8550}};else r.monster_remove={67};
        roundtrip(value);
        r.target_action_uuid=target_key.substr(7);need(!encode(value,bytes,error),"saved key accepted with unrelated birth UUID");++cases;
        r.target_action_uuid.clear();r.canonical_record_key+="junk";need(!encode(value,bytes,error),"malformed saved target admitted");++cases;
    }
    monsters::Row row;row.birth=saved;row.name="Original saved Monster";row.rank=1;row.slot=1;row.type=0;row.level=1;
    row.pose.region=67;row.pose.orientation={0,0,0,1};row.pose.scale={1,1,1};row.host_combat.target={"mapped",saved};
    monsters::State state{{row},{}},actual;
    need(monsters::decode(monsters::encode(state),actual,error)&&actual==state,"actual live codec lost saved identity/target");++cases;
    const auto valid_state=state;
    auto invalid=[&](auto change,const std::string& detail){auto bad=valid_state;change(bad.rows[0]);error="stale prior error";
        need(!monsters::valid(bad,error)&&error.find(detail)!=std::string::npos&&error.find(saved)!=std::string::npos,"actual validator omitted exact failure: "+error);
        need(!monsters::decode(monsters::encode(bad),actual,error)&&actual.rows.empty()&&error.find(detail)!=std::string::npos,"actual decoder omitted failure or published partial row: "+error);++cases;};
    invalid([](auto& r){r.name.clear();},"member=name reason=empty bytes=0");
    invalid([](auto& r){r.name=std::string(257,'x');},"member=name reason=too_long bytes=257 maximum=256");
    invalid([](auto& r){r.name=std::string("a\0b",3);},"member=name reason=embedded_nul offset=1 bytes=3");
    for(int32_t tag:{-11752269,INT32_MIN,-1,0,INT32_MAX}){auto local=valid_state;local.rows[0].tag=tag;
        need(monsters::decode(monsters::encode(local),actual,error)&&actual==local,"raw signed serial roundtrip differs");
        need(monsters::preflight(actual,valid_state,error)&&monsters::adopted(actual,valid_state,error),"local serial became shared identity: "+error);++cases;}
    auto duplicates=valid_state;duplicates.rows.push_back(row);duplicates.rows[0].tag=duplicates.rows[1].tag=-11752269;
    duplicates.rows[1].birth=std::string(64,'b')+":world:monster:38";
    need(monsters::decode(monsters::encode(duplicates),actual,error)&&actual==duplicates,"duplicate local serial rejected distinct canonical identities");++cases;
    duplicates.rows[1].birth=duplicates.rows[0].birth;
    need(!monsters::valid(duplicates,error),"duplicate canonical identity admitted");++cases;
    invalid([](auto& r){r.rank=-1;},"member=rank actual=-1 required_min=0");
    invalid([](auto& r){r.slot=-2;},"member=character_slot actual=-2 required_min=0");
    invalid([](auto& r){r.type=-3;},"member=character_type actual=-3 required_min=0");
    invalid([](auto& r){r.level=-4;},"member=level actual=-4 required_min=0");
    invalid([](auto& r){r.pose.scale[0]=-1;},"member=pose actual=");
    error="stale prior error";need(monsters::valid(valid_state,error)&&error.empty(),"valid zero tag changed or stale cause survived");++cases;
    auto mismatch=[&](auto change,const std::string& detail){auto local=valid_state;change(local.rows[0]);
        need(monsters::valid(local,error),"fixture must reach actual immutable comparison: "+error);
        monsters::State decoded_local,decoded_host;
        need(monsters::decode(monsters::encode(local),decoded_local,error)&&monsters::decode(monsters::encode(valid_state),decoded_host,error),error);
        need(!monsters::preflight(decoded_local,decoded_host,error)&&error.find(detail)!=std::string::npos,
            "actual preflight omitted local/required member: "+error);++cases;};
    mismatch([](auto& r){r.birth=std::string(64,'b')+":world:monster:37";},"member=birth local=");
    mismatch([](auto& r){r.name=std::string(256,'x');},"member=name local={\"bytes\":256,\"value\":\""+std::string(160,'x')+"\"}");
    const auto split_name=std::string(159,'x')+"ä";
    const auto rendered_name=std::string("{\"bytes\":161,\"value\":\"")+std::string(159,'x')+"\xef\xbf\xbd\"}";
    mismatch([&](auto& r){r.name=split_name;},"member=name local="+rendered_name);
    auto required_utf8=valid_state;required_utf8.rows[0].name=split_name;
    monsters::State required_decoded;
    need(monsters::decode(monsters::encode(required_utf8),required_decoded,error),error);
    need(!monsters::preflight(valid_state,required_decoded,error)&&error.find("member=name local=")!=std::string::npos&&
        error.find(" required="+rendered_name)!=std::string::npos,"required UTF-8 diagnostic lost original comparison cause: "+error);++cases;
    mismatch([](auto& r){r.rank=2;},"member=rank local=2 required=1");
    mismatch([](auto& r){r.slot=2;},"member=character_slot local=2 required=1");
    mismatch([](auto& r){r.type=2;},"member=character_type local=2 required=0");
    mismatch([](auto& r){r.level=2;},"member=level local=2 required=1");
    mismatch([](auto& r){r.pose.region=68;},"member=region local=68 required=67");
    mismatch([](auto& r){r.spawn[0]=1.25f;},"member=spawn_x local=1.25 required=0.0");
    mismatch([](auto& r){r.spawn[2]=2.5f;},"member=spawn_z local=2.5 required=0.0");
    mismatch([](auto& r){r.tag=5;r.rank=2;r.spawn[0]=3;},"member=rank local=2 required=1");
    need(monsters::preflight(valid_state,valid_state,error)&&error.empty(),"unchanged immutable row rejected");++cases;
    state.rows.clear();state.retired={saved};need(monsters::decode(monsters::encode(state),actual,error)&&actual==state,"actual live codec lost saved tombstone");++cases;
    state.retired[0]+="junk";need(!monsters::valid(state,error),"invalid saved tombstone admitted");++cases;
    std::cout<<cases<<" saved Monster actual Quest/Move/Remove/live codec cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
