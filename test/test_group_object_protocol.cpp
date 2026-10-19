#include "typed_action/action_session.h"
#include <bit>
#include <iostream>
#include <stdexcept>
namespace {
namespace a=mt2mp::action_session;unsigned checks=0;
void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
const std::string uuid="01234567-89ab-4cde-8fab-0123456789ab",second="11234567-89ab-4cde-8fab-0123456789ab",snapshot="21234567-89ab-4cde-8fab-0123456789ab";
const std::string member="action:"+uuid,group="group:"+member;
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
a::GroupObjectState state(){a::GroupObjectState v;const auto p=pose();v.ownership.groups.push_back({group,67,p,{{member,p}}});v.ownership.props.push_back({member,group,67,p});
    v.records.sources={{member,group,std::string(64,'a'),a::DuplicateFamily::Scenery,67,p,{}},{group,"",std::string(64,'b'),a::DuplicateFamily::Group,67,p,{member}}};v.records.untouched_sha=std::string(64,'c');return v;}
a::Ledger money(){a::Ledger v;v.cash=v.record_cash=1000;v.record_present=1;return v;}
a::Ledger cash(a::Ledger v,int32_t amount){v.cash+=amount;v.record_cash+=amount;v.record_income+=amount;++v.line_counts[6];v.line_income[6]+=amount;return v;}
a::Message message(bool moving){a::Message m;m.kind=a::Kind::HostResult;m.session=1;m.revision=1;m.outcome=a::Outcome::Confirmed;m.required_mask=3;
    auto& r=m.request;r.action=moving?a::Action::MoveGroup:a::Action::RemoveGroup;r.action_uuid=uuid;r.snapshot_uuid=snapshot;r.peer=1;r.sequence=1;r.host_revision=1;
    r.canonical_record_key=group;r.group_object={group,pose(),{}, {member}};if(moving){r.group_object.after=pose();r.group_object.after[4]=std::bit_cast<uint32_t>(10.f);}
    auto& e=m.evidence;e.before=money();e.group_object_before=state();e.group_object_after=state();e.undo_confirmed=e.selection_confirmed=e.retirement_confirmed=true;
    if(moving){auto& after=*e.group_object_after;after.ownership.groups[0].pose=r.group_object.after;after.ownership.props[0].pose=r.group_object.after;
        for(auto& row:after.records.sources){row.pose=r.group_object.after;row.record_sha=std::string(64,'d');}e.after=e.before;
    }else{auto& after=*e.group_object_after;after.ownership.groups.clear();after.ownership.props.clear();after.records.sources.clear();e.after=cash(e.before,100);e.target_retired=true;e.duplicate_cash={{-100,{0,0,0}}};}
    return m;
}
}
int main()try{
    std::string e;std::vector<uint8_t> bytes;a::Message read;
    auto roundtrip=[&](const a::Message& m){return a::encode(m,bytes,e)&&a::decode(bytes.data(),bytes.size(),read,e)&&read.request==m.request&&read.evidence==m.evidence;};
    for(bool moving:{true,false}){auto m=message(moving);need(a::valid_group_object(moving,m.request.group_object),"valid complete Group intent rejected");
        need(a::valid_group_object_effect(m.request,m.evidence),"valid complete original Group effect rejected");need(roundtrip(m),e);
        const auto j=nlohmann::json::parse(bytes);need(j.at("schema")==35u&&j["request"].contains("group_object")&&!j["request"].contains("building"),"Group action reused an incomplete normal-prop packet");
        auto bad=[&](auto change){auto x=m;change(x);e.clear();need(!a::encode(x,bytes,e)&&!e.empty(),"invalid Group complete packet accepted");};
        bad([](auto& x){x.request.group_object.members.push_back(member);});bad([](auto& x){x.request.group_object.key="group:foreign";});
        bad([](auto& x){x.evidence.group_object_after->records.untouched_sha=std::string(64,'e');});bad([](auto& x){x.evidence.group_object_after->records.release[0]++;});
        bad([](auto& x){x.evidence.group_object_after.reset();});bad([](auto& x){x.evidence.group_object_before->records.sources[0].parent.clear();});
        bad([](auto& x){x.evidence.group_before=x.evidence.group_object_before->ownership;});
        auto request=m;request.kind=a::Kind::Request;request.outcome=a::Outcome::Pending;request.evidence={};request.revision=0;request.required_mask=0;request.request.host_revision=0;need(roundtrip(request),e);
        auto json=nlohmann::json::parse(bytes);json["request"]["building"]=nlohmann::json::object();const auto text=json.dump();
        need(!a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),read,e),"foreign extra fields accepted in Group schema35");
    }
    auto destroy=message(false);auto undo=destroy;undo.request.action=a::Action::UndoBuilding;undo.request.action_uuid=second;undo.request.target_action_uuid=uuid;
    undo.request.canonical_record_key="group-object:"+uuid;undo.request.group_object={};undo.evidence.group_object_before=destroy.evidence.group_object_after;
    undo.evidence.group_object_after=destroy.evidence.group_object_before;undo.evidence.group_object_after->ownership.random={1,2,3,4};undo.evidence.group_object_after->records.random={1,2,3,4};
    undo.evidence.before=destroy.evidence.after;undo.evidence.after=cash(undo.evidence.before,-100);undo.evidence.target_retired=false;undo.evidence.duplicate_cash={{100,{0,0,0}}};
    need(a::valid_group_object_effect(undo.request,undo.evidence),"native Group restoration RNG/Record effects rejected");need(roundtrip(undo),e);
    auto redo=destroy;redo.request=undo.request;redo.request.action=a::Action::RedoBuilding;redo.evidence.group_object_before=undo.evidence.group_object_after;
    redo.evidence.group_object_after=undo.evidence.group_object_before;redo.evidence.group_object_after->ownership.random={1,2,3,4};redo.evidence.group_object_after->records.random={1,2,3,4};
    redo.evidence.before=undo.evidence.after;redo.evidence.after=cash(redo.evidence.before,100);need(roundtrip(redo),e);
    auto zero=message(false);zero.evidence.after=cash(zero.evidence.before,0);zero.evidence.duplicate_cash[0].amount=0;need(roundtrip(zero),e);
    auto move=message(true);auto move_undo=move;move_undo.request=undo.request;std::swap(move_undo.evidence.group_object_before,move_undo.evidence.group_object_after);need(roundtrip(move_undo),e);
    auto nochange=move;nochange.request.group_object.after=nochange.request.group_object.before;need(!a::encode(nochange,bytes,e),"unchanged Group Move accepted");
    auto wrong_cash=destroy;wrong_cash.evidence.duplicate_cash[0].amount=100;need(!a::encode(wrong_cash,bytes,e),"Group inverse refund sign accepted");
    std::cout<<checks<<" actual Group object packet/complete-state cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
