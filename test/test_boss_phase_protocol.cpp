#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
Message sample(int32_t source,int32_t boundary){
    Message m;m.session=7;m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::ReorderBossPhase;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";
    r.canonical_record_key="character:3:0";r.boss_phase={0,source,boundary};
    BossPhaseState before;BossTypePhases boss;boss.id=0;boss.name="Boss";boss.definition="definition";boss.ability_names={"Attack","Attack","Heal"};
    boss.phases={{"Alpha","fire",20,{1,0,0,1},{0,1}},{"Beta","ice",40,{0,1,0,1},{2}},{"Gamma","",60,{0,0,1,1},{1,0,1}}};
    before.bosses={boss};boss.id=1;boss.name="Unrelated";before.bosses.push_back(boss);
    auto after=before;auto& phases=after.bosses[0].phases;auto moved=phases[size_t(source)];phases.erase(phases.begin()+source);
    phases.insert(phases.begin()+(boundary>source?boundary-1:boundary),moved);
    m.evidence.boss_phase_before=before;m.evidence.boss_phase_after=after;m.evidence.retirement_confirmed=true;return m;
}
int main(){try{std::string e;std::vector<uint8_t> bytes;Message decoded;unsigned cases=0;
    for(auto indices:{std::array<int32_t,2>{0,3},{2,0},{1,0},{0,2}}){auto m=sample(indices[0],indices[1]);
        need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e),e);need(decoded==m,"Boss phase result roundtrip");++cases;
        auto request=m;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
        need(encode(request,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e),e);need(decoded==request,"Boss phase request roundtrip");++cases;
        auto bad=[&](auto mutate){auto value=m;mutate(value);need(!encode(value,bytes,e),"Invalid Boss phase result accepted");++cases;};
        bad([](auto& v){++v.evidence.boss_phase_after->bosses[1].phases[0].health;});
        bad([](auto& v){v.evidence.boss_phase_after->bosses[0].phases[0].abilities.push_back(4);});
        bad([](auto& v){v.evidence.boss_phase_after->bosses[0].phases.pop_back();});
        bad([](auto& v){v.evidence.boss_phase_after->bosses[0].edited=true;});
        bad([](auto& v){++v.evidence.boss_phase_after->release_edits;});
        bad([](auto& v){++v.evidence.after.cash;});
        bad([](auto& v){v.request.canonical_record_key="character:4:0";});
        bad([](auto& v){v.request.boss_phase.boundary=4;});
        bad([](auto& v){v.request.prop_text.text="foreign";});
        need(encode(request,bytes,e),e);auto raw=nlohmann::json::parse(bytes);raw["request"]["boss_phase"]["pointer"]=42;auto text=raw.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"Boss pointer field accepted");++cases;
    }
    for(auto indices:{std::array<int32_t,2>{1,1},{1,2}}){auto m=sample(indices[0],indices[1]);need(!encode(m,bytes,e),"No-op phase order accepted");++cases;}
    for(auto kind:{BossPhaseKind::Name,BossPhaseKind::Health,BossPhaseKind::Vfx,BossPhaseKind::Color}){
        auto m=sample(0,2);m.evidence.boss_phase_after=m.evidence.boss_phase_before;auto& intent=m.request.boss_phase;
        intent.boundary=-1;intent.kind=kind;auto& phase=m.evidence.boss_phase_after->bosses[0].phases[0];
        if(kind==BossPhaseKind::Name)phase.name=intent.text="New phase";
        if(kind==BossPhaseKind::Health)phase.health=intent.health=150;
        if(kind==BossPhaseKind::Vfx)phase.vfx=intent.text="";
        if(kind==BossPhaseKind::Color)phase.color=intent.color={0.1f,0.2f,0.3f,0.4f};
        need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"Property codec result differs");++cases;
        auto broken=m;broken.evidence.boss_phase_after->bosses[0].phases[1].health++;need(!encode(broken,bytes,e),"Unrelated phase edit accepted");++cases;
        broken=m;broken.request.boss_phase.boundary=0;need(!encode(broken,bytes,e),"Property stale reorder payload accepted");++cases;
        broken=m;broken.evidence.boss_phase_after=broken.evidence.boss_phase_before;need(!encode(broken,bytes,e),"Property no effect accepted");++cases;
        broken=m;broken.evidence.boss_phase_after->bosses[0].phases[0].abilities.clear();need(!encode(broken,bytes,e),"Property lost ability ownership accepted");++cases;
    }
    for(auto kind:{BossPhaseKind::RemoveAbility,BossPhaseKind::AddAbility}){
        auto m=sample(0,2);m.evidence.boss_phase_after=m.evidence.boss_phase_before;auto& intent=m.request.boss_phase;
        intent.boundary=-1;intent.kind=kind;auto& phase=m.evidence.boss_phase_after->bosses[0].phases[0];
        if(kind==BossPhaseKind::RemoveAbility){intent.element=0;phase.abilities.erase(phase.abilities.begin());}
        else{intent.text="Attack";intent.ability_indices={0,1};phase.abilities.insert(phase.abilities.end(),{0,1});}
        need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);++cases;
        auto bad=m;bad.evidence.boss_phase_after->bosses[0].ability_names[0]="Changed";need(!encode(bad,bytes,e),"Ability prototype renamed by phase membership");++cases;
        bad=m;bad.evidence.boss_phase_after->bosses[1].phases[0].abilities.clear();need(!encode(bad,bytes,e),"Unrelated owner membership lost");++cases;
        bad=m;if(kind==BossPhaseKind::RemoveAbility)bad.request.boss_phase.element=999;else bad.request.boss_phase.ability_indices={0};
        need(!encode(bad,bytes,e),"Wrong element or incomplete duplicate-name match accepted");++cases;
    }
    for(auto kind:{BossPhaseKind::Create,BossPhaseKind::Destroy}){
        auto m=sample(0,2);m.evidence.boss_phase_before->default_color={1,0,0,1};m.evidence.boss_phase_after=m.evidence.boss_phase_before;
        auto& intent=m.request.boss_phase;intent.boundary=-1;intent.kind=kind;auto& phases=m.evidence.boss_phase_after->bosses[0].phases;
        if(kind==BossPhaseKind::Create){intent.phase=3;intent.text="{design_boss_defaultphasename} 4";phases.push_back({intent.text,"",100,{1,0,0,1},{}});}
        else{intent.phase=0;phases.erase(phases.begin());}
        need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);++cases;
        auto bad=m;bad.evidence.boss_phase_after->bosses[0].phases[0].name="Changed survivor";need(!encode(bad,bytes,e),"Phase membership changed survivor");++cases;
        bad=m;bad.evidence.boss_phase_after->default_color[0]=0;need(!encode(bad,bytes,e),"Phase global constructor color changed");++cases;
        bad=m;if(kind==BossPhaseKind::Create)bad.evidence.boss_phase_after->bosses[0].phases.back().health=0;else bad.evidence.boss_phase_after->bosses[0].phases.clear();
        need(!encode(bad,bytes,e),"Phase constructor or exact deletion differs");++cases;
    }
    std::cout<<cases<<" actual Boss phase protocol cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
