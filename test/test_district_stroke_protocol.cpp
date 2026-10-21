#include "typed_action/action_session.h"
#include "district_stroke_state.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace state=mt2mp::game::district_stroke_state;
int main()try{
    unsigned checks=0;std::string error;std::vector<uint8_t> bytes;
    auto need=[&](bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);};
    auto message=std::make_unique<Message>(),decoded=std::make_unique<Message>();auto& m=*message;
    m.kind=Kind::HostResult;m.session=1;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::DistrictStroke;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";
    r.canonical_record_key="district-stroke:"+r.action_uuid;
    r.district_stroke.region=17;r.district_stroke.geo=2;r.district_stroke.clip={{{0,0},{100,0},{100,100},{0,100}}};
    r.district_stroke.press={0x3f800000,0,0x40000000};r.district_stroke.finish={0x40000000,0,0x40000000};
    r.district_stroke.color={0x3f800000,0x3f000000,0,0x3f800000};r.district_stroke.water_color=r.district_stroke.color;
    r.district_stroke.change_color=true;r.district_stroke.stamps={{0,r.district_stroke.clip},{0,{{{10,10},{50,10},{50,50}}}}};
    m.evidence.district_stroke=DistrictStrokeEffect{{1,std::string(64,'a')},{2,std::string(64,'b')},std::string(64,'c'),5};
    m.evidence.undo_confirmed=true;m.evidence.retirement_confirmed=true;
    auto roundtrip=[&](const Message& value){need(encode(value,bytes,error),error);need(decode(bytes.data(),bytes.size(),*decoded,error),error);need(*decoded==value,"District stroke populated roundtrip differs");};
    auto invalid=[&](auto change){auto copy=std::make_unique<Message>(m);change(*copy);need(!encode(*copy,bytes,error),"invalid District stroke encoded");};
    roundtrip(m);need(district_stroke_wire_fits(r,m.evidence.before,m.evidence.district_stroke->before,error),error);
    need(encode(m,bytes,error),error);const auto original=nlohmann::json::parse(bytes);
    for(auto schema:{1,19,35}){auto bad=original;bad["schema"]=schema;const auto wire=bad.dump();need(!decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),*decoded,error),"new District stroke admitted under old schema");}
    for(const auto field:{"foreign","target_action_uuid"}){auto bad=original;bad["request"][field]="unexpected";const auto wire=bad.dump();need(!decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),*decoded,error),"foreign District request field admitted");}
    invalid([](auto& value){value.request.district_stroke.region=-1;});
    invalid([](auto& value){value.request.district_stroke.geo=3;});
    invalid([](auto& value){value.request.district_stroke.press[0]=0x7fc00000;});
    invalid([](auto& value){value.request.district_stroke.stamps.clear();});
    invalid([](auto& value){value.request.district_stroke.stamps[0].route=2;});
    invalid([](auto& value){value.request.district_stroke.selected="foreign";});
    invalid([](auto& value){value.evidence.district_stroke->inverse_sha="fake";});
    invalid([](auto& value){value.evidence.district_stroke->inverse_operations=0;});
    invalid([](auto& value){value.evidence.undo_confirmed=false;});
    invalid([](auto& value){value.evidence.before.cash=1;});
    invalid([](auto& value){value.request.canonical_record_key="district-stroke:foreign";});
    auto request=std::make_unique<Message>(m);request->kind=Kind::Request;request->revision=0;request->required_mask=0;request->outcome=Outcome::Pending;request->evidence={};roundtrip(*request);
    r.district_stroke.selected=std::string(64,'a')+":world:district:17";r.district_stroke.stamps[1].route=2;roundtrip(m);
    r.district_stroke.kind=DistrictStrokeKind::Erase;r.district_stroke.selected.clear();for(auto& stamp:r.district_stroke.stamps)stamp.route=1;roundtrip(m);
    r.district_stroke={};r.district_stroke.kind=DistrictStrokeKind::Delete;r.district_stroke.region=17;r.district_stroke.selected="district:action:33333333-3333-4333-8333-333333333333:0";roundtrip(m);
    invalid([](auto& value){value.request.district_stroke.color[0]=1;});
    for(auto kind:{Action::UndoBuilding,Action::RedoBuilding}){
        r.action=kind;r.target_action_uuid="11111111-1111-4111-8111-111111111111";r.action_uuid="44444444-4444-4444-8444-444444444444";
        r.canonical_record_key="district-stroke:"+r.target_action_uuid;r.district_stroke={};roundtrip(m);
        need(district_stroke_wire_fits(r,m.evidence.before,m.evidence.district_stroke->before,error),error);
        need(encode(m,bytes,error),error);auto bad=nlohmann::json::parse(bytes);bad["schema"]=35;const auto wire=bad.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),*decoded,error),"new District history admitted under old schema");
    }
    std::cout<<checks<<" District populated stroke/history protocol checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
