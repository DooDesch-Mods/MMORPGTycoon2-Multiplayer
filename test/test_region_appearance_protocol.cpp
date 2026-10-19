#include "typed_action/action_session.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
Message example(RegionPropertyKind kind){
    Message m;m.session=4;m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::EditRegionProperty;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";
    r.canonical_record_key="region:67";r.region_property.id=67;r.region_property.kind=kind;r.region_property.appearance_value[0]=0.5f;
    auto& e=m.evidence;e.retirement_confirmed=true;e.region_retired_sha256=std::string(64,'a');
    auto& before=e.property_before;before.content.id=67;before.content.preset="forest";before.content.styles={"grass","grass","grass","grass"};
    before.content.scenery_sha256=before.content.grid_sha256=std::string(64,'b');before.name="Region";
    before.type=1;before.status=0;before.level=before.max_level=1;before.owned=true;before.appearance=RegionAppearance{};
    e.property_after=before;auto& after=*e.property_after.appearance;
    switch(kind){
    case RegionPropertyKind::Tint:after.tint[0]=0.5f;break;
    case RegionPropertyKind::Light:after.light[0]=0.5f;break;
    case RegionPropertyKind::Ambient:after.ambient[0]=0.5f;break;
    case RegionPropertyKind::Sky:after.sky[0]=0.5f;r.region_property.appearance_value[7]=-0.75f;after.sky[7]=-0.75f;break;
    case RegionPropertyKind::Particles:r.region_property.appearance_value[0]=6;after.particles=6;after.particles_enabled=true;break;
    case RegionPropertyKind::ParticleColor:after.particle_color[0]=0.5f;break;
    default:throw std::runtime_error("fixture kind");
    }
    return m;
}
int main(){try{unsigned count=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    for(unsigned k=5;k<=10;++k){auto result=example(RegionPropertyKind(k));
        need(encode(result,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(decoded==result,"appearance result roundtrip");++count;
        auto request=result;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
        need(encode(request,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(decoded==request,"appearance request roundtrip");++count;
        auto reject=[&](auto change){auto bad=result;change(bad);need(!encode(bad,bytes,error),"invalid appearance effect accepted");++count;};
        reject([](auto& m){m.evidence.property_after.name="unrelated";});
        reject([](auto& m){++m.evidence.after.cash;});
        reject([](auto& m){++m.evidence.property_after.content.random[0];});
        reject([](auto& m){m.evidence.undo_confirmed=true;});
        reject([](auto& m){m.evidence.property_after.appearance.reset();});
        reject([](auto& m){m.evidence.property_after.appearance->sky[15]+=1;});
        reject([](auto& m){m.request.region_property.appearance_value[0]=std::numeric_limits<float>::infinity();});
        need(encode(request,bytes,error),error);auto j=nlohmann::json::parse(bytes);need(j.at("schema")==35,"appearance schema35 missing");
        j["request"]["region_property"]["appearance_value"].push_back(0);auto raw=j.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),raw.size(),decoded,error),"oversized appearance accepted");++count;
    }
    auto bad=example(RegionPropertyKind::Particles);bad.request.region_property.appearance_value[0]=0.5f;
    need(!encode(bad,bytes,error),"fractional particle type accepted");++count;
    bad=example(RegionPropertyKind::Tint);bad.request.region_property.appearance_value[4]=1;
    need(!encode(bad,bytes,error),"unused appearance tail accepted");++count;
    std::cout<<count<<" actual appearance codec/effect cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
