#include <memory>
#include "typed_action/zone_edit_codec.h"
inline unsigned zone_edit_message_cases(){
    using namespace mt2mp::action_session;
    unsigned checks=0;std::string error;std::vector<uint8_t> bytes;
    auto message=std::make_unique<Message>(),decoded=std::make_unique<Message>();
    auto& value=*message;
    const auto check=[&](bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause);};
    const std::string operation="11111111-1111-4111-8111-111111111111",snapshot="22222222-2222-4222-8222-222222222222";
    const std::string prior="33333333-3333-4333-8333-333333333333";
    const auto setup=[&](ZoneEditKind kind,int32_t type){
        value=*std::make_unique<Message>();value.session=5;
        auto& request=value.request;request.action=Action::EditZone;request.peer=2;request.sequence=1;
        request.action_uuid=operation;request.snapshot_uuid=snapshot;request.canonical_record_key="zone-edit:"+operation;
        auto& intent=request.zone_edit;intent.kind=kind;intent.region=67;
        if(kind==ZoneEditKind::Stroke){
            intent.zone_type=type;intent.character_type=0;intent.level=1;
            intent.stamps={{{8020,0,8550},{{{7988000,8518000},{8051000,8550000},{8020000,8581000}},
                {{8000000,8540000},{8001000,8540000},{8001000,8541000}}}}};
        }else{
            intent.selected="action:"+prior;
            if(kind==ZoneEditKind::Apply){intent.zone_type=1;intent.character_type=3;intent.level=7;intent.density=2;}
        }
    };
    const auto roundtrip=[&](){
        check(encode(value,bytes,error),"Zone58 encode: "+error);
        check(decode(bytes.data(),bytes.size(),*decoded,error),"Zone58 decode: "+error);
        check(value==*decoded,"Zone58 populated Message lost a field");
        check(nlohmann::json::parse(bytes).at("schema")==36,"Zone58 wire used a foreign schema");
    };
    const auto result=[&](){
        value.kind=Kind::HostResult;value.revision=1;value.required_mask=3;value.outcome=Outcome::Confirmed;
        auto& evidence=value.evidence;evidence.before.cash=evidence.after.cash=87900;evidence.retirement_confirmed=true;
        ZoneEditEffect effect;effect.before.values_sha=std::string(64,'a');effect.before.quests_sha=std::string(64,'b');
        effect.before.references_sha=std::string(64,'c');effect.before.counts={4,7,2,1};effect.before.monsters=34;
        effect.before.global_random={UINT64_MAX,1,2,3};effect.before.region_random={4,5,6,UINT64_MAX};
        effect.before.next_tag=44;effect.before.created_monsters=83;effect.before.release_edits={1,2,3,4,5,6,7,8};
        effect.after=effect.before;effect.after.values_sha=std::string(64,'d');++effect.after.release_edits[5];
        effect.after.global_random[0]=0;
        if(value.request.zone_edit.kind!=ZoneEditKind::Respawn){effect.inverse_sha=std::string(64,'e');effect.inverse_operations=19;evidence.undo_confirmed=true;}
        evidence.zone_edit=effect;
    };
    const auto rejected=[&](nlohmann::json wire){
        const auto text=wire.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Malformed Zone58 Message was accepted");
        check(!error.empty(),"Zone58 malformed wire lost its failure cause");
    };
    for(int type=0;type<4;++type){
        setup(ZoneEditKind::Stroke,type);roundtrip();
        value.request.zone_edit.add=false;roundtrip();
        value.request.zone_edit.add=true;value.request.zone_edit.selected="action:"+prior;roundtrip();
        result();roundtrip();
    }
    for(auto kind:{ZoneEditKind::Apply,ZoneEditKind::Reset,ZoneEditKind::Respawn,ZoneEditKind::Delete}){
        setup(kind,1);roundtrip();result();roundtrip();
        value.kind=Kind::ApplyAck;value.ack_peer=2;roundtrip();
    }
    setup(ZoneEditKind::Apply,1);result();
    for(auto action:{Action::UndoBuilding,Action::RedoBuilding}){
        value.request.action=action;value.request.zone_edit={};value.request.target_action_uuid=prior;
        value.request.canonical_record_key="zone-edit:"+prior;roundtrip();
    }
    setup(ZoneEditKind::Stroke,1);result();roundtrip();const auto wire=nlohmann::json::parse(bytes);
    for(auto schema:{25,35,37}){auto bad=wire;bad["schema"]=schema;rejected(bad);}
    {auto bad=wire;bad["request"]["action"]=57;rejected(bad);}
    {auto bad=wire;bad["request"]["canonical_record_key"]="zone-edit:"+prior;rejected(bad);}
    {auto bad=wire;bad["request"]["zone_edit"]["unexpected"]=1;rejected(bad);}
    {auto bad=wire;bad["evidence"]["zone_edit"]["after"]["region_random"][1]=-1;rejected(bad);}
    {auto bad=wire;bad["evidence"]["zone_edit"]["inverse_operations"]=0;rejected(bad);}
    {auto bad=wire;bad["evidence"]["zone_edit"]=nullptr;rejected(bad);}
    {auto bad=wire;bad["evidence"]["district_stroke"]=nullptr;rejected(bad);}
    value.request.npc.type=1;check(!encode(value,bytes,error),"Foreign request payload accepted by Zone58");value.request.npc={};
    value.evidence.after.cash++;check(!encode(value,bytes,error),"Zone58 changed zero-cost Ledger");value.evidence.after=value.evidence.before;
    check(zone_edit_wire_fits(value.request,value.evidence.before,value.evidence.zone_edit->before,error),"Zone58 populated proof preflight: "+error);
    auto foreign=std::make_unique<Message>();foreign->session=5;foreign->request=value.request;
    foreign->request.action=Action::StartStartingpoint;foreign->request.canonical_record_key="tech:startingpoint";foreign->request.zone_edit={};
    foreign->evidence.zone_edit=value.evidence.zone_edit;
    check(!encode(*foreign,bytes,error),"Foreign action accepted Zone58 evidence");
    return checks;
}
