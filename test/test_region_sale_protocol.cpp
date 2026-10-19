#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
Message sample(){
    Message m;m.session=7;m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::EditRegionProperty;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";
    r.canonical_record_key="region:67";r.region_property.id=67;r.region_property.kind=RegionPropertyKind::Sale;
    auto& e=m.evidence;e.retirement_confirmed=true;e.undo_confirmed=true;e.region_retired_sha256=std::string(64,'a');
    auto& before=e.property_before;before.content.id=67;before.content.preset="forest";before.content.styles={"grass","grass","grass","grass"};
    before.content.scenery_sha256=before.content.grid_sha256=std::string(64,'b');before.name="Region";
    before.type=1;before.status=0;before.level=before.max_level=2;before.owned=true;before.appearance=RegionAppearance{};
    before.actors=RegionLevelState{{2,1,1},std::string(64,'c'),std::string(64,'d'),std::string(64,'e'),{}};
    before.building_count=3;before.path_count=4;before.path_node_count=5;before.network_node_count=6;before.network_pipe_count=7;
    e.property_after=before;auto& after=e.property_after;after.owned=false;after.level=after.max_level=0;
    after.actors->levels_sha256=after.actors->expected_sha256;
    e.before.cash=e.before.record_cash=1000;e.before.record_present=1;e.after=e.before;
    e.after.cash+=500;e.after.record_cash+=500;e.after.record_income+=500;e.after.line_counts[7]++;e.after.line_income[7]+=500;
    return m;
}
int main(){try{unsigned count=0;std::string error;std::vector<uint8_t> bytes;Message decoded;auto result=sample();
    need(encode(result,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(result==decoded,"sale roundtrip");++count;
    auto request=result;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
    need(encode(request,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(request==decoded,"sale request roundtrip");++count;
    auto reject=[&](auto change){auto bad=result;change(bad);need(!encode(bad,bytes,error),"invalid sale accepted");++count;};
    reject([](auto& m){m.evidence.property_after.owned=true;});
    reject([](auto& m){m.evidence.property_before.owned=false;});
    reject([](auto& m){m.evidence.property_after.level=1;});
    reject([](auto& m){m.evidence.property_after.actors->levels_sha256=std::string(64,'f');});
    reject([](auto& m){m.evidence.property_after.actors->counts[1]++;});
    reject([](auto& m){m.evidence.property_after.building_count--;});
    reject([](auto& m){m.evidence.property_after.path_node_count--;});
    reject([](auto& m){m.evidence.property_after.network_pipe_count--;});
    reject([](auto& m){m.evidence.property_after.appearance->tint[0]=1;});
    reject([](auto& m){m.evidence.property_after.actors.reset();});
    reject([](auto& m){m.evidence.after.line_income[7]--;});
    reject([](auto& m){m.evidence.after.record_income--;});
    reject([](auto& m){m.evidence.after.record_expense++;});
    reject([](auto& m){m.evidence.undo_confirmed=false;});
    auto free=result;free.evidence.after=free.evidence.before;free.evidence.after.line_counts[7]++;
    need(encode(free,bytes,error),error);++count;
    MonsterActor monster;monster.name="Level zero";monster.character_slot=1;monster.character_type=0;monster.rank=1;monster.status=3;
    monster.scale={1,1,1};monster.rotation={0,0,0,1};monster.level=0;
    need(valid_monster_actor(monster),"native sold Region Monster level0 rejected");++count;
    monster.level=-1;need(!valid_monster_actor(monster),"negative Monster level accepted");++count;
    std::cout<<count<<" actual region sale codec/effect cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
