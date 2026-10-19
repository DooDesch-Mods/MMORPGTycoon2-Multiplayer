#include "typed_action/action_session.h"
#include "region_history_key.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
using json=nlohmann::json;
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
Message sample(Action direction){
    Message m;m.kind=Kind::HostResult;m.session=1;m.revision=5;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=direction;r.peer=2;r.sequence=1;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.target_action_uuid="22222222-2222-4222-8222-222222222222";r.snapshot_uuid="33333333-3333-4333-8333-333333333333";r.canonical_record_key="region:56";
    auto& e=m.evidence;e.undo_confirmed=e.retirement_confirmed=true;
    auto& before=e.property_before;before.content.id=56;before.content.preset="Forest";before.content.styles={"Forest","Forest","Forest","Forest"};
    before.content.scenery_sha256=before.content.grid_sha256=std::string(64,'a');before.name="Test";before.type=1;before.status=0;before.owned=true;
    before.level=direction==Action::UndoBuilding?4:1;before.max_level=direction==Action::UndoBuilding?6:10;
    before.actors=RegionLevelState{};before.actors->counts={0,2,0};before.actors->roster_sha256=std::string(64,'a');
    before.actors->levels_sha256=std::string(64,'b');before.actors->expected_sha256=std::string(64,'c');
    e.property_after=before;e.property_after.level=direction==Action::UndoBuilding?1:4;e.property_after.max_level=direction==Action::UndoBuilding?10:6;
    e.property_after.actors->levels_sha256=e.property_after.actors->expected_sha256=std::string(64,'c');
    e.region_retired_sha256="4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945";return m;
}
int main(){try{unsigned cases=0;std::vector<uint8_t> bytes;std::string e;Message decoded;
    for(auto direction:{Action::UndoBuilding,Action::RedoBuilding}){
        const auto m=sample(direction);need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);++cases;
        auto request=m;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
        need(encode(request,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==request,e);++cases;
        auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,e),"invalid Region history accepted");++cases;};
        reject([](auto& v){v.request.target_action_uuid=v.request.action_uuid;});
        reject([](auto& v){v.request.canonical_record_key="region:57";});
        reject([](auto& v){v.request.canonical_record_key="region:056";});
        reject([](auto& v){v.request.canonical_record_key="region:-1";});
        reject([](auto& v){v.request.region_property.id=56;});
        reject([](auto& v){v.evidence.property_before.actors.reset();});
        reject([](auto& v){v.evidence.property_after.actors.reset();});
        reject([](auto& v){v.evidence.property_after.actors->counts[1]++;});
        reject([](auto& v){v.evidence.property_after.actors->roster_sha256=std::string(64,'b');});
        reject([](auto& v){v.evidence.property_after.actors->levels_sha256=std::string(64,'d');});
        reject([](auto& v){v.evidence.property_after.actors->expected_sha256=std::string(64,'d');});
        reject([](auto& v){v.evidence.property_after.name="Other";});
        reject([](auto& v){v.evidence.property_after.content.grid_sha256=std::string(64,'b');});
        reject([](auto& v){v.evidence.property_after.level=v.evidence.property_before.level;v.evidence.property_after.max_level=v.evidence.property_before.max_level;});
        reject([](auto& v){v.evidence.property_after.level=0;});
        reject([](auto& v){v.evidence.after.cash++;});
        reject([](auto& v){v.evidence.undo_confirmed=false;});
        reject([](auto& v){v.evidence.selection_confirmed=true;});
        reject([](auto& v){v.evidence.target_retired=true;});
        reject([](auto& v){v.evidence.region_retired_sha256.clear();});
        need(encode(m,bytes,e),e);auto old=json::parse(bytes);old["schema"]=uint64_t{10};const auto body=old.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(body.data()),body.size(),decoded,e),"Region history accepted single-object schema");++cases;
    }
    for(auto direction:{Action::UndoBuilding,Action::RedoBuilding})for(bool owned:{false,true})for(int32_t price:{0,500}){
        auto m=sample(direction);auto& value=m.evidence;auto& before=value.property_before;auto& after=value.property_after;
        before.owned=owned;before.status=owned?1:0;before.level=owned?4:0;before.max_level=owned?6:0;
        after.owned=!owned;after.status=owned?0:1;after.level=owned?0:4;after.max_level=owned?0:6;
        before.appearance=after.appearance=RegionAppearance{};
        value.before.record_present=true;value.before.cash=value.before.record_cash=10000;value.before.flat_day=value.before.record_day=7;
        value.after=value.before;const auto amount=owned?price:-price;
        if(price){value.after.cash+=amount;value.after.record_cash+=amount;value.after.record_income+=amount;
            value.after.line_counts[6]++;value.after.line_income[6]+=amount;}
        need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);++cases;
        auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,e),"invalid ownership history accepted");++cases;};
        reject([](auto& bad){bad.evidence.property_after.actors.reset();});
        reject([](auto& bad){bad.evidence.property_before.appearance.reset();});
        reject([](auto& bad){bad.evidence.property_after.appearance->tint[0]=0.5f;});
        reject([](auto& bad){bad.evidence.property_after.actors->roster_sha256=std::string(64,'d');});
        reject([](auto& bad){bad.evidence.property_after.building_count++;});
        reject([](auto& bad){bad.evidence.property_after.status=9;});
        reject([](auto& bad){bad.evidence.property_after.owned=bad.evidence.property_before.owned;});
        reject([](auto& bad){bad.evidence.after.record_expense++;});
        if(price){
            reject([](auto& bad){bad.evidence.after.line_counts[7]++;bad.evidence.after.line_counts[6]--;});
            reject([](auto& bad){bad.evidence.after.record_income=bad.evidence.before.record_income;});
            reject([](auto& bad){auto& v=bad.evidence;const auto delta=v.after.cash-v.before.cash;
                v.after.cash-=2*delta;v.after.record_cash-=2*delta;v.after.record_income-=2*delta;v.after.line_income[6]-=2*delta;});
        }
    }
    for(const auto* key:{"region:","region:+1","region:00","region:-1","region:2147483648","region:1x","region: 1","region:1/2","selection:1"}){
        need(!mt2mp::game::region_history_key::parse(key),"malformed canonical Region key accepted");++cases;}
    for(const auto* key:{"region:0","region:56","region:2147483647"}){need(mt2mp::game::region_history_key::parse(key).has_value(),"valid canonical Region key rejected");++cases;}
    std::cout<<cases<<" Region original-history protocol/key/clamp cases passed\n";return 0;
}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}}
