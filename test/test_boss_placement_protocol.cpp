#include "typed_action/action_session.h"
#include "boss_placement_state.h"
#include <iostream>
#include <stdexcept>
namespace {
namespace a=mt2mp::action_session;namespace state=mt2mp::game::boss_placement_state;namespace actors=mt2mp::game::interventions::actors;
unsigned checks=0;void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
const std::string uuid="01234567-89ab-4cde-8fab-0123456789ab",snapshot="11234567-89ab-4cde-8fab-0123456789ab";
a::Message message(){
    a::Message out;out.kind=a::Kind::HostResult;out.session=1;out.revision=1;out.required_mask=3;out.outcome=a::Outcome::Confirmed;
    auto& request=out.request;request.action=a::Action::PlaceBoss;request.peer=1;request.sequence=1;request.host_revision=1;request.action_uuid=uuid;request.snapshot_uuid=snapshot;
    request.canonical_record_key=state::key(uuid);request.boss_placement.type=4;request.boss_placement.region=17;request.boss_placement.character_sha=std::string(64,'a');
    request.boss_placement.pose={0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};request.boss_placement.point[0]=0x41200000;
    auto& e=out.evidence;e.before.cash=e.before.record_cash=2000;e.before.record_present=1;e.after=e.before;
    e.after.cash-=1000;e.after.record_cash-=1000;e.after.record_expense+=1000;++e.after.line_counts[6];e.after.line_income[6]-=1000;
    a::BossPlacementState before;before.untouched_sha=std::string(64,'b');auto after=before;after.record_sha=std::string(64,'c');actors::Row row;
    row.target={mt2mp::game::interventions::Family::Boss,state::key(uuid)};row.slot=3;row.type=4;row.pose.region=17;row.pose.orientation={0,0,0,1};row.pose.scale={1,1,1};row.lifecycle=actors::Boss{};
    row.clips[48]={std::bit_cast<uint32_t>(3.f),std::bit_cast<uint32_t>(-0.5f),true,std::bit_cast<uint32_t>(1.f)};after.created=row;after.random={1,2,3,4};
    e.boss_placement_before=before;e.boss_placement_after=after;e.undo_confirmed=e.selection_confirmed=e.retirement_confirmed=true;return out;
}
}
int main()try{
    auto complete=message();std::string error;std::vector<uint8_t> payload;a::Message decoded;
    auto roundtrip=[&](const a::Message& source){return a::encode(source,payload,error)&&a::decode(payload.data(),payload.size(),decoded,error)&&decoded.request==source.request&&decoded.evidence==source.evidence;};
    need(a::valid_boss_placement(complete.request.boss_placement),"complete semantic Boss intent rejected");
    need(a::valid_boss_placement_effect(complete.request,complete.evidence),"actual one-birth purchase effect rejected");need(roundtrip(complete),error);
    auto json=nlohmann::json::parse(payload);need(json["schema"]==35&&json["request"].contains("boss_placement")&&!json["request"].contains("building"),"Boss payload must preserve authored type/full pose/release point");
    need(payload.size()<a::max_payload,"complete49 semantic clips exceed existing action packet limit");
    auto bad=[&](auto change){auto value=complete;change(value);error.clear();need(!a::encode(value,payload,error)&&!error.empty(),"malformed Boss effect accepted");};
    bad([](auto& value){value.request.boss_placement.point[1]=0x7fc00000;});
    bad([](auto& value){value.request.boss_placement.pose[3]=0;});
    bad([](auto& value){value.request.boss_placement.character_sha.clear();});
    bad([](auto& value){value.request.boss_placement.type=-1;});
    bad([](auto& value){value.request.canonical_record_key="action:"+uuid;});
    bad([](auto& value){value.request.target_action_uuid=snapshot;});
    bad([](auto& value){value.evidence.boss_placement_before->created=value.evidence.boss_placement_after->created;value.evidence.boss_placement_before->record_sha=std::string(64,'c');});
    bad([](auto& value){value.evidence.boss_placement_after->created.reset();value.evidence.boss_placement_after->record_sha.clear();});
    bad([](auto& value){value.evidence.boss_placement_after->created->slot=4;});
    bad([](auto& value){value.evidence.boss_placement_after->created->type++;});
    bad([](auto& value){value.evidence.boss_placement_after->created->target.key=state::key(snapshot);});
    bad([](auto& value){value.evidence.boss_placement_after->untouched_sha=std::string(64,'d');});
    bad([](auto& value){value.evidence.boss_placement_after->record_sha="partial";});
    bad([](auto& value){value.evidence.boss_placement_after.reset();});
    bad([](auto& value){value.evidence.undo_confirmed=false;});
    bad([](auto& value){value.evidence.selection_confirmed=false;});
    bad([](auto& value){value.evidence.target_retired=true;});
    bad([](auto& value){value.evidence.after.record_income=-1000;value.evidence.after.record_expense=0;});
    bad([](auto& value){value.evidence.after.line_counts[7]++;});
    bad([](auto& value){value.evidence.group_object_before=a::GroupObjectState{};});
    auto request=complete;request.kind=a::Kind::Request;request.revision=0;request.required_mask=0;request.outcome=a::Outcome::Pending;request.request.host_revision=0;request.evidence={};need(roundtrip(request),error);
    json=nlohmann::json::parse(payload);json["request"]["building"]=nlohmann::json::object();auto text=json.dump();
    need(!a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Boss schema accepts foreign extra request fields");
    need(roundtrip(complete),error);json=nlohmann::json::parse(payload);json["evidence"]["boss_placement_after"][4]["clips"][48][3]=nlohmann::json::array({1});text=json.dump();
    need(!a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Boss schema accepts invalid nested semantic clip data");
    need(a::boss_placement_wire_fits(complete.request,complete.evidence,error),error);
    std::cout<<checks<<" Boss placement packet checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<"Boss placement packet failure: "<<exception.what()<<'\n';return 1;}
