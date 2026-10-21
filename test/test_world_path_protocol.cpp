#include "typed_action/action_session.h"
#include <bit>
#include <iostream>
#include <stdexcept>

namespace a=mt2mp::action_session;
namespace {
unsigned checks=0;
void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
const std::string uuid="01234567-89ab-4cde-8fab-0123456789ab";
const std::string snapshot="11234567-89ab-4cde-8fab-0123456789ab";
a::Message pass(){
    a::Message out;out.kind=a::Kind::HostResult;out.session=1;out.revision=1;
    out.required_mask=3;out.outcome=a::Outcome::Confirmed;
    auto& r=out.request;r.action=a::Action::PlacePass;r.peer=1;r.sequence=1;r.host_revision=1;
    r.action_uuid=uuid;r.snapshot_uuid=snapshot;r.canonical_record_key="action:"+uuid;
    auto& p=r.world_path;p.kind=a::WorldPathKind::Pass;p.start_region=17;p.end_region=18;p.type=2;
    p.name="Pass";p.start={0,0,0};p.end={std::bit_cast<uint32_t>(10.f),0,0};
    p.preview={p.start,p.end};p.preview_valid=true;p.graph_before_sha256=std::string(64,'a');
    auto& e=out.evidence;e.before.record_present=1;e.after=e.before;
    e.world_path_before=a::WorldPathState{std::string(64,'a'),std::string(64,'b'),5,7};
    e.world_path_after=a::WorldPathState{std::string(64,'c'),std::string(64,'d'),6,9};
    e.undo_confirmed=e.retirement_confirmed=true;
    return out;
}
bool roundtrip(const a::Message& source,std::vector<uint8_t>& wire,std::string& error){
    a::Message decoded;
    return a::encode(source,wire,error)&&a::decode(wire.data(),wire.size(),decoded,error)&&
           decoded.request==source.request&&decoded.evidence==source.evidence;
}
}
int main()try{
    std::string error;std::vector<uint8_t> wire;auto original=pass();
    need(a::world_path_valid(original.request.world_path),"Pass native intent rejected");
    need(a::valid_world_path_effect(original.request,original.evidence),"Pass native effect rejected");
    need(roundtrip(original,wire,error),"Pass HostResult roundtrip: "+error);
    need(wire.size()<a::max_payload,"Pass packet exceeds action transport bound");
    auto j=nlohmann::json::parse(wire);
    need(j.at("schema")==35&&j.at("request").contains("world_path")&&
         j.at("evidence").contains("world_path_after"),"Pass original graph receipt omitted");
    auto wall=original;wall.request.action=a::Action::PlaceWall;
    auto& p=wall.request.world_path;p.kind=a::WorldPathKind::Wall;p.end_region=17;p.type=4;
    p.node_type=0;p.name="Wall";p.quoted_length=std::bit_cast<uint32_t>(10.f);
    p.segments.push_back({});p.preview_valid=false;
    need(roundtrip(wall,wire,error),"Wall HostResult roundtrip: "+error);
    auto request=original;request.kind=a::Kind::Request;request.revision=0;
    request.required_mask=0;request.outcome=a::Outcome::Pending;
    request.request.host_revision=0;request.evidence={};
    need(roundtrip(request,wire,error),"Pass Request roundtrip: "+error);
    auto undo=original;undo.request.action=a::Action::UndoBuilding;
    undo.request.action_uuid="21234567-89ab-4cde-8fab-0123456789ab";
    undo.request.canonical_record_key="path:"+uuid;undo.request.target_action_uuid=uuid;
    undo.request.world_path={};
    std::swap(undo.evidence.world_path_before,undo.evidence.world_path_after);
    need(roundtrip(undo,wire,error),"mixed Path Undo roundtrip: "+error);
    auto undo_request=undo;undo_request.kind=a::Kind::Request;undo_request.revision=0;
    undo_request.required_mask=0;undo_request.outcome=a::Outcome::Pending;
    undo_request.request.host_revision=0;undo_request.evidence={};
    need(roundtrip(undo_request,wire,error),"mixed Path Undo Request roundtrip: "+error);
    auto redo=undo;redo.request.action=a::Action::RedoBuilding;
    redo.request.action_uuid="31234567-89ab-4cde-8fab-0123456789ab";
    std::swap(redo.evidence.world_path_before,redo.evidence.world_path_after);
    need(roundtrip(redo,wire,error),"mixed Path Redo roundtrip: "+error);
    auto removal=original;removal.request.action=a::Action::RemoveWorldPath;
    removal.request.world_path={};auto& remove_intent=removal.request.world_path;
    remove_intent.kind=a::WorldPathKind::Remove;remove_intent.start_region=17;
    remove_intent.end_region=18;remove_intent.type=2;
    remove_intent.target_path="snapshot:"+snapshot+":path:17:2:0";
    remove_intent.graph_before_sha256=std::string(64,'a');
    removal.evidence.world_path_after=a::WorldPathState{std::string(64,'c'),std::string(64,'b'),4,6};
    removal.evidence.target_retired=removal.evidence.selection_confirmed=true;
    need(roundtrip(removal,wire,error),"mixed Path Remove roundtrip: "+error);
    auto removal_request=removal;removal_request.kind=a::Kind::Request;removal_request.revision=0;
    removal_request.required_mask=0;removal_request.outcome=a::Outcome::Pending;
    removal_request.request.host_revision=0;removal_request.evidence={};
    need(roundtrip(removal_request,wire,error),"mixed Path Remove Request roundtrip: "+error);
    auto invalid_remove=[&](auto change){auto value=removal;change(value);error.clear();
        need(!a::encode(value,wire,error)&&!error.empty(),"invalid mixed Path Remove accepted");};
    invalid_remove([](auto& value){value.request.world_path.target_path.clear();});
    invalid_remove([](auto& value){value.request.world_path.target_path="pointer:123";});
    invalid_remove([](auto& value){value.request.world_path.preview.push_back({});});
    invalid_remove([](auto& value){value.evidence.target_retired=false;});
    invalid_remove([](auto& value){value.evidence.selection_confirmed=false;});
    invalid_remove([](auto& value){value.evidence.world_path_after->paths=3;});
    invalid_remove([](auto& value){value.evidence.world_path_after->nodes=3;});
    invalid_remove([](auto& value){value.request.action=a::Action::PlacePass;});
    auto remove_undo=undo;remove_undo.evidence.world_path_before=removal.evidence.world_path_after;
    remove_undo.evidence.world_path_after=removal.evidence.world_path_before;
    need(roundtrip(remove_undo,wire,error),"mixed Path Remove original Undo roundtrip: "+error);
    auto remove_redo=remove_undo;remove_redo.request.action=a::Action::RedoBuilding;
    std::swap(remove_redo.evidence.world_path_before,remove_redo.evidence.world_path_after);
    need(roundtrip(remove_redo,wire,error),"mixed Path Remove original Redo roundtrip: "+error);
    auto bad=[&](auto change){auto value=original;change(value);error.clear();
        need(!a::encode(value,wire,error)&&!error.empty(),"malformed Wall/Pass packet accepted");};
    bad([](auto& value){value.request.world_path.preview[1][1]=0x7fc00000;});
    bad([](auto& value){value.request.world_path.start_region=value.request.world_path.end_region;});
    bad([](auto& value){value.request.world_path.preview_valid=false;});
    bad([](auto& value){value.request.world_path.graph_before_sha256.clear();});
    bad([](auto& value){value.request.world_path.start_node="local-pointer:123";value.request.world_path.start_node.append(130,'x');});
    bad([](auto& value){value.evidence.world_path_after.reset();});
    bad([](auto& value){value.evidence.world_path_after->complete_sha256=value.evidence.world_path_before->complete_sha256;});
    bad([](auto& value){value.evidence.undo_confirmed=false;});
    bad([](auto& value){value.request.action=a::Action::PlaceWall;});
    bad([](auto& value){value.evidence.boss_placement_before=a::BossPlacementState{};});
    auto bad_wall=wall;bad_wall.request.world_path.segments[0][2]=0x7fc00000;
    error.clear();need(!a::encode(bad_wall,wire,error)&&!error.empty(),"Wall spline accepted nonfinite component");
    need(roundtrip(original,wire,error),"Pass roundtrip before strict decoder: "+error);
    j=nlohmann::json::parse(wire);j["request"]["path"]=nlohmann::json::object();
    auto text=j.dump();a::Message decoded;
    need(!a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),
         "Pass decoder accepted foreign Road field");
    need(roundtrip(original,wire,error),"Pass roundtrip before nested decoder: "+error);
    j=nlohmann::json::parse(wire);j["request"]["world_path"][14][0][0]=0x7fc00000;
    text=j.dump();
    need(!a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),
         "Pass decoder accepted nonfinite preview bit pattern");
    std::cout<<checks<<" Wall/Pass transport and native receipt checks passed\n";
    return 0;
}catch(const std::exception& cause){std::cerr<<"Wall/Pass protocol failure: "<<cause.what()<<'\n';return 1;}
