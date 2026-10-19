#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
using Json=nlohmann::json;
void need(bool value,const std::string& error){if(!value)throw std::runtime_error(error);}
Message sample(Action direction,bool removing,int32_t price=100){
    Message m;m.kind=Kind::HostResult;m.session=1;m.revision=5;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=direction;r.peer=2;r.sequence=1;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.target_action_uuid="22222222-2222-4222-8222-222222222222";r.snapshot_uuid="33333333-3333-4333-8333-333333333333";
    r.canonical_record_key="path:"+r.target_action_uuid;
    auto& e=m.evidence;e.undo_confirmed=e.retirement_confirmed=true;
    e.before.cash=e.before.record_cash=1000;e.before.record_present=1;e.before.flat_day=e.before.record_day=1;e.after=e.before;
    e.region_before.id=56;e.region_before.preset="Forest";e.region_before.styles={"Forest","Forest","Forest","Forest"};
    e.region_before.scenery_sha256=e.region_before.grid_sha256=std::string(64,'a');e.region_after=e.region_before;
    e.terrain_before.contours_sha256=e.terrain_before.bugs_sha256=std::string(64,'a');e.terrain_after=e.terrain_before;
    e.path_before={3,4,std::string(64,'a'),0};e.path_after={removing?2u:4u,removing?3u:5u,std::string(64,'b'),price};
    if(price){const auto amount=removing?price:-price;e.after.cash+=amount;e.after.record_cash+=amount;
        e.after.record_income+=amount;
        e.after.line_counts[6]++;e.after.line_income[6]+=amount;}
    return m;
}
int main(){try{
    unsigned cases=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    for(auto direction:{Action::UndoBuilding,Action::RedoBuilding})for(bool removing:{false,true}){
        const auto m=sample(direction,removing);
        need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==m,error);++cases;
        auto request=m;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
        need(encode(request,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==request,error);++cases;
        const auto free=sample(direction,removing,0);
        need(encode(free,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==free,error);++cases;
        auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,error),"invalid Path history accepted");++cases;};
        reject([](auto& v){v.request.target_action_uuid=v.request.action_uuid;});
        reject([](auto& v){v.request.canonical_record_key="path:"+v.request.action_uuid;});
        reject([](auto& v){v.request.path.region=56;});
        reject([](auto& v){v.evidence.path_before.cost=1;});
        reject([](auto& v){v.evidence.path_after.cost=-1;});
        reject([](auto& v){v.evidence.path_after.paths=v.evidence.path_before.paths;});
        reject([](auto& v){v.evidence.path_after.nodes=100;});
        reject([](auto& v){v.evidence.path_after.graph_sha256="wrong";});
        reject([](auto& v){v.evidence.after.cash++;});
        reject([](auto& v){v.evidence.after.record_cash++;});
        reject([](auto& v){v.evidence.after.record_income++;});
        reject([](auto& v){v.evidence.after.record_expense++;});
        if(!removing)reject([](auto& v){
            v.evidence.after.record_income=v.evidence.before.record_income;
            v.evidence.after.record_expense+=v.evidence.path_after.cost;
        });
        reject([](auto& v){v.evidence.after.line_counts[7]++;});
        reject([](auto& v){v.evidence.after.line_income[6]++;});
        reject([](auto& v){v.evidence.after.record_day++;});
        reject([](auto& v){v.evidence.terrain_after.bugs_sha256=std::string(64,'b');});
        reject([](auto& v){v.evidence.terrain_after.release_edits[3]++;});
        reject([](auto& v){v.evidence.region_after.grid_sha256=std::string(64,'b');});
        reject([](auto& v){v.evidence.region_retired_sha256=std::string(64,'a');});
        reject([](auto& v){v.evidence.undo_confirmed=false;});
        reject([](auto& v){v.evidence.selection_confirmed=true;});
        reject([](auto& v){v.evidence.target_retired=true;});
        reject([](auto& v){v.evidence.property_before.name="other";});
        need(encode(m,bytes,error),error);auto old=Json::parse(bytes);old["schema"]=10;const auto body=old.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(body.data()),body.size(),decoded,error),"Path history accepted old single-object schema");++cases;
    }
    std::cout<<cases<<" Path original-history protocol/effect comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
