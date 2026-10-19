#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& cause){if(!ok)throw std::runtime_error(cause);}
Message sample(){Message m;m.kind=Kind::HostResult;m.session=1;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::MoveSelection;r.peer=2;r.sequence=1;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.snapshot_uuid="22222222-2222-4222-8222-222222222222";r.canonical_record_key="selection:"+r.action_uuid;
    for(const auto* key:{"action:33333333-3333-4333-8333-333333333333","action:44444444-4444-4444-8444-444444444444"}){
        BuildingState before;before.alive=true;before.type="SmallInn";before.variant="inn";before.x=100;before.z=100;
        auto after=before;after.x+=10;r.selection_move.rows.push_back({key,before,after});}
    auto& e=m.evidence;e.retirement_confirmed=e.undo_confirmed=e.selection_confirmed=true;e.selection_move_before=SelectionMoveState{};e.selection_move_after=SelectionMoveState{};
    for(const auto& row:r.selection_move.rows){e.selection_move_before->rows.push_back(row.before);e.selection_move_after->rows.push_back(row.after);}return m;
}
int main(){try{unsigned cases=0;auto m=sample();std::vector<uint8_t> bytes;std::string e;Message decoded;
    need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e),e);need(decoded==m,"full result roundtrip");++cases;
    auto request=m;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
    need(encode(request,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==request,e);++cases;
    need(selection_move_wire_fits(m.request,m.evidence.before,e),e);++cases;
    auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,e),"invalid multi target effect accepted");++cases;};
    reject([](auto& v){v.request.selection_move.rows.pop_back();});
    reject([](auto& v){v.request.selection_move.rows[1].key=v.request.selection_move.rows[0].key;});
    reject([](auto& v){v.request.canonical_record_key=v.request.selection_move.rows[0].key;});
    reject([](auto& v){v.request.selection_move.rows[0].after.type="Other";});
    reject([](auto& v){v.evidence.selection_move_after->rows[1].x+=1;});
    reject([](auto& v){v.evidence.selection_move_before->rows[1].x+=1;});
    reject([](auto& v){v.evidence.selection_move_after->rows[0].alive=false;});
    reject([](auto& v){v.evidence.undo_confirmed=false;});
    reject([](auto& v){v.evidence.selection_confirmed=false;});
    reject([](auto& v){v.evidence.after.line_counts[0]++;});
    reject([](auto& v){v.evidence.target_retired=true;});
    reject([](auto& v){v.evidence.selection_move_after.reset();});
    reject([](auto& v){v.request.service_membership.ordinal=7;});
    auto huge=m.request;
    for(unsigned i=0;i<1000;++i){auto row=huge.selection_move.rows[0];char id[64];std::snprintf(id,sizeof id,"action:12345678-1234-4234-8234-%012u",i);row.key=id;huge.selection_move.rows.push_back(row);}
    need(!selection_move_wire_fits(huge,{},e),"oversized complete result allowed before mutation");++cases;
    for(auto direction:{Action::UndoBuilding,Action::RedoBuilding}){
        auto history=sample();const auto original=history.request.action_uuid;
        history.request.action=direction;history.request.action_uuid="55555555-5555-4555-8555-555555555555";
        history.request.target_action_uuid=original;history.request.canonical_record_key="selection:"+original;
        history.request.selection_move={};history.evidence.selection_confirmed=false;
        if(direction==Action::UndoBuilding)std::swap(history.evidence.selection_move_before,history.evidence.selection_move_after);
        need(encode(history,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==history,e);++cases;
        auto pending_history=history;pending_history.kind=Kind::Request;pending_history.revision=0;pending_history.required_mask=0;pending_history.outcome=Outcome::Pending;pending_history.evidence={};
        need(encode(pending_history,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==pending_history,e);++cases;
        auto history_reject=[&](auto change){auto bad=history;change(bad);need(!encode(bad,bytes,e),"invalid group history accepted");++cases;};
        history_reject([](auto& v){v.request.target_action_uuid=v.request.action_uuid;});
        history_reject([](auto& v){v.request.canonical_record_key="selection:"+v.request.action_uuid;});
        history_reject([](auto& v){v.request.selection_move=sample().request.selection_move;});
        history_reject([](auto& v){v.evidence.selection_move_before->rows.pop_back();});
        history_reject([](auto& v){v.evidence.selection_move_after->rows[1].type="Other";});
        history_reject([](auto& v){v.evidence.selection_move_after->rows[1].alive=false;});
        history_reject([](auto& v){v.evidence.undo_confirmed=false;});
        history_reject([](auto& v){v.evidence.selection_confirmed=true;});
        history_reject([](auto& v){v.evidence.after.cash++;});
        history_reject([](auto& v){v.evidence.selection_move_after=v.evidence.selection_move_before;});
        need(encode(history,bytes,e),e);
        auto old_schema=nlohmann::json::parse(bytes);old_schema["schema"]=uint64_t{10};auto old_bytes=old_schema.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(old_bytes.data()),old_bytes.size(),decoded,e),"group history admitted legacy single-object schema");++cases;
    }
    std::cout<<cases<<" Selection Move protocol cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
