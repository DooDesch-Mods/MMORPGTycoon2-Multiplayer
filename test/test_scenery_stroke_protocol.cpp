#include "typed_action/action_session.h"
#include "scenery_stroke_transition.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool okay,const std::string& cause){if(!okay)throw std::runtime_error(cause);}
const std::string fixture_action="11111111-1111-4111-8111-111111111111",fixture_snapshot="22222222-2222-4222-8222-222222222222";
std::string old(unsigned n){return std::string(64,'a')+":world:building:"+std::to_string(n);}
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
DuplicateObject object(std::string key,DuplicateFamily family=DuplicateFamily::Scenery){return {key,"",std::string(64,'b'),family,67,pose(),{}};}
Message message(bool brush=true){
    Message m;m.kind=Kind::HostResult;m.session=4;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=brush?Action::SceneryBrushStroke:Action::SceneryEraseStroke;r.peer=1;r.sequence=1;r.action_uuid=fixture_action;r.snapshot_uuid=fixture_snapshot;
    r.canonical_record_key="scenery-stroke:"+fixture_action;r.scenery_stroke.kind=brush?SceneryStrokeKind::Paint:SceneryStrokeKind::Erase;r.scenery_stroke.region=67;r.scenery_stroke.mode=brush?2:1;
    if(brush){r.scenery_stroke.type="Trees";r.scenery_stroke.variant="oak";}
    SceneryStrokeStamp stamp;stamp.shape={{{0,0},{10,0},{10,10}}};if(brush)stamp.dots={pose(),pose()};r.scenery_stroke.stamps={stamp};
    SceneryStrokeState before;before.props_sha=std::string(64,'c');before.bugs_sha=std::string(64,'d');auto after=before;after.props_sha=std::string(64,'e');
    SceneryStrokeEffect result;result.untouched_sha=std::string(64,'f');
    if(brush){result.created={object("action:"+fixture_snapshot)};after.release[2]=1;after.random[0]=5;after.region_random[2]=7;}
    else{auto removed=object(old(1));removed.parent="group:"+old(1);result.removed={removed};
        auto group=object(removed.parent,DuplicateFamily::Group);group.members={old(1),old(2)};auto changed=group;changed.members={old(2)};changed.record_sha=std::string(64,'e');result.changed={{group,changed}};}
    auto& e=m.evidence;e.scenery_stroke_before=before;e.scenery_stroke_after=after;e.scenery_stroke_result=result;
    e.before.record_present=1;e.before.cash=e.before.record_cash=100;e.after=e.before;e.after.cash=e.after.record_cash=brush?90:110;
    e.after.line_counts[6]=1;e.after.line_income[6]=brush?-10:10;if(brush)e.after.record_expense=10;else e.after.record_income=10;
    e.undo_confirmed=e.selection_confirmed=e.retirement_confirmed=true;return m;
}
int main()try{
    unsigned count=0;std::vector<uint8_t> bytes;std::string e;Message decoded;
    for(bool brush:{false,true}){auto m=message(brush);need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);
        need(scenery_stroke_wire_fits(m.request,m.evidence,e),e);count+=3;}
    const auto source=message();auto bad=[&](auto change){auto m=source;change(m);need(!encode(m,bytes,e),"invalid Scenery stroke accepted");++count;};
    bad([](auto& m){m.request.action=Action::SceneryEraseStroke;});
    bad([](auto& m){m.request.canonical_record_key="scenery-stroke:"+fixture_snapshot;});
    bad([](auto& m){m.request.scenery_stroke.mode=0;});
    bad([](auto& m){m.request.scenery_stroke.stamps.clear();});
    bad([](auto& m){m.request.scenery_stroke.stamps[0].shape[0].pop_back();});
    bad([](auto& m){m.request.scenery_stroke.stamps[0].shape[0][0].x=INT64_MAX;});
    bad([](auto& m){m.request.scenery_stroke.stamps[0].dots[0][7]=0;});
    bad([](auto& m){m.request.scenery_stroke.stamps[0].dots[0][0]=0x7f800000;});
    bad([](auto& m){m.request.scenery_stroke.random_variant=true;});
    bad([](auto& m){m.request.scenery_stroke.finish_position[0]=0x7fc00000;});
    bad([](auto& m){m.evidence.scenery_stroke_after->props_sha=m.evidence.scenery_stroke_before->props_sha;});
    bad([](auto& m){m.evidence.scenery_stroke_after->release[2]=0;});
    bad([](auto& m){m.evidence.scenery_stroke_after->release[1]=1;});
    bad([](auto& m){m.evidence.scenery_stroke_result->created[0].region=68;});
    bad([](auto& m){m.evidence.scenery_stroke_result->created[0].family=DuplicateFamily::Building;});
    bad([](auto& m){m.evidence.scenery_stroke_result->created.push_back(m.evidence.scenery_stroke_result->created[0]);});
    bad([](auto& m){m.evidence.scenery_stroke_result->created[0].key=old(2);});
    bad([](auto& m){m.evidence.scenery_stroke_result->removed={object(old(1))};});
    bad([](auto& m){m.evidence.scenery_stroke_result.reset();});
    bad([](auto& m){m.evidence.retirement_confirmed=false;});
    auto erase=message(false);erase.evidence.scenery_stroke_result->changed[0].after.members={};
    need(!encode(erase,bytes,e),"Group removed a member not destroyed by the original eraser");++count;
    erase=message(false);erase.evidence.scenery_stroke_after->random[0]=1;need(!encode(erase,bytes,e),"eraser changed world RNG");++count;
    erase=message(false);erase.evidence.scenery_stroke_result->changed[0].before.members={old(1)};
    erase.evidence.scenery_stroke_result->changed[0].after.members={};need(encode(erase,bytes,e),e);++count;
    auto random=source;random.request.scenery_stroke.variant.clear();random.request.scenery_stroke.random_variant=true;need(encode(random,bytes,e),e);++count;
    auto request=source;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
    need(encode(request,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==request,e);++count;
    need(encode(source,bytes,e),e);const auto original=nlohmann::json::parse(bytes);
    auto malformed=[&](auto change){auto j=original;change(j);const auto value=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(value.data()),value.size(),decoded,e),"malformed Scenery packet accepted");++count;};
    malformed([](auto& j){j["schema"]=34;});malformed([](auto& j){j["request"]["scenery_stroke"][1]=67.0;});
    malformed([](auto& j){j["request"]["scenery_stroke"][5]=1;});malformed([](auto& j){j["request"]["scenery_stroke"][7][0][1][0][0]=-1;});
    malformed([](auto& j){j["evidence"]["scenery_stroke_result"][1][0].push_back(1);});
    malformed([](auto& j){j["evidence"]["scenery_stroke_result"][3]="";});
    auto history=message(false);history.request.action=Action::UndoBuilding;history.request.target_action_uuid=fixture_action;
    history.request.action_uuid=fixture_snapshot;history.request.scenery_stroke={};
    history.evidence.before=history.evidence.after;history.evidence.after.cash-=10;history.evidence.after.record_cash-=10;
    history.evidence.after.record_income-=10;history.evidence.after.line_income[6]-=10;++history.evidence.after.line_counts[6];
    history.evidence.scenery_stroke_before=history.evidence.scenery_stroke_after;history.evidence.scenery_stroke_after->props_sha=std::string(64,'9');
    auto restored=history.evidence.scenery_stroke_result->removed.front();restored.parent.clear();
    history.evidence.scenery_stroke_result->created={restored};history.evidence.scenery_stroke_result->removed.clear();history.evidence.scenery_stroke_result->changed.clear();
    history.evidence.duplicate_cash={{-10,{}}};
    need(encode(history,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==history,e);++count;
    auto execution=history.evidence;execution.retirement_confirmed=false;need(scenery_stroke_wire_fits(history.request,execution,e),e);++count;
    auto invalid_history=history;invalid_history.evidence.after.record_expense=10;
    need(!encode(invalid_history,bytes,e),"Scenery history accepted SubtractCash instead of native signed AddCash");++count;
    invalid_history=history;invalid_history.evidence.scenery_stroke_after->release[2]=1;
    need(!encode(invalid_history,bytes,e),"Scenery history fabricated another Release edit");++count;
    invalid_history=history;invalid_history.evidence.scenery_stroke_after->bugs_sha=std::string(64,'a');
    need(!encode(invalid_history,bytes,e),"Scenery history changed an unrelated Bug collection");++count;
    invalid_history=history;invalid_history.evidence.duplicate_cash[0].amount=10;
    need(!encode(invalid_history,bytes,e),"Scenery history accepted a wrong signed cash inverse");++count;
    invalid_history=history;invalid_history.request.canonical_record_key="duplicate:"+fixture_action;
    need(!encode(invalid_history,bytes,e),"Scenery history evidence escaped its canonical family");++count;
    history.kind=Kind::Request;history.revision=0;history.required_mask=0;history.outcome=Outcome::Pending;history.evidence={};
    need(encode(history,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==history,e);++count;
    std::cout<<count<<" Scenery complete stroke codec/result cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
