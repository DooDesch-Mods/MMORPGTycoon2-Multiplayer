#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
Message sample(){
    Message m;m.kind=Kind::HostResult;m.session=1;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::EditDistrictText;r.peer=2;r.sequence=1;r.action_uuid="11111111-1111-4111-8111-111111111111";
    r.snapshot_uuid="22222222-2222-4222-8222-222222222222";r.canonical_record_key=std::string(64,'a')+":world:district:17";r.district_text.text="Village";
    DistrictState before;before.region=67;before.geo=0;before.name="Old";before.description="Unchanged";before.pose[3]=before.pose[7]=before.pose[8]=before.pose[9]=0x3f800000;
    before.shape={{{0,0},{100,0},{100,100},{0,100}}};m.evidence.district_before=m.evidence.district_after=before;m.evidence.district_after->name="Village";
    m.evidence.retirement_confirmed=true;return m;
}
int main()try{
    unsigned cases=0;auto m=sample();std::string e;std::vector<uint8_t> bytes;Message out;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==m,e);++cases;
    need(district_wire_fits(m.request,*m.evidence.district_before,m.evidence.before,e),e);++cases;
    auto roundtrip=[&](Message v){need(encode(v,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==v,e);++cases;};
    auto empty=m;empty.request.district_text.text="";empty.evidence.district_after->name="{district_unnamed}";roundtrip(empty);
    auto moved=m;moved.evidence.district_before->flags[11]=1;moved.evidence.district_before->manual_title_inside=false;
    roundtrip(moved);need(district_wire_fits(moved.request,*moved.evidence.district_before,moved.evidence.before,e),e);++cases;
    auto description=m;description.request.district_text.kind=DistrictTextKind::Description;description.request.district_text.text="";
    description.evidence.district_after=description.evidence.district_before;description.evidence.district_after->description="";roundtrip(description);
    auto request=m;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};roundtrip(request);
    auto reject=[&](auto edit){auto bad=m;edit(bad);need(!encode(bad,bytes,e),"invalid District effect accepted");++cases;};
    reject([](auto& v){v.request.canonical_record_key="action:11111111-1111-4111-8111-111111111111";});
    reject([](auto& v){v.request.district_text.text=std::string("x\0y",3);});
    reject([](auto& v){v.evidence.district_after->geo=1;});
    reject([](auto& v){v.evidence.district_after->values[17]++;});
    reject([](auto& v){v.evidence.district_after->flags[11]=1;});
    reject([](auto& v){v.evidence.district_after->shape[0][1].x++;});
    reject([](auto& v){v.evidence.district_after->pose[4]++;});
    reject([](auto& v){v.evidence.district_after->region++;});
    reject([](auto& v){v.evidence.district_after->saved_version++;});
    reject([](auto& v){v.evidence.district_after->constructed_day++;});
    reject([](auto& v){v.evidence.after.line_counts[0]++;});
    reject([](auto& v){v.evidence.undo_confirmed=true;});
    reject([](auto& v){v.evidence.selection_confirmed=true;});
    reject([](auto& v){v.evidence.target_retired=true;});
    reject([](auto& v){v.evidence.district_after.reset();});
    reject([](auto& v){v.request.prop_text.text="foreign";});
    need(encode(m,bytes,e),e);auto j=nlohmann::json::parse(bytes);
    auto malformed=[&](auto edit){auto bad=j;edit(bad);const auto wire=bad.dump();need(!decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),out,e),"malformed District packet accepted");++cases;};
    malformed([](auto& v){v["evidence"]["district_before"]["flags"][0]=2;});
    malformed([](auto& v){v["evidence"]["district_before"]["values"][0]=1.0;});
    malformed([](auto& v){v["evidence"]["district_before"]["local_pointer"]=9;});
    malformed([](auto& v){v["evidence"]["district_before"]["shape"][0].push_back(5);});
    malformed([](auto& v){v["request"]["district_text"]["kind"]=3;});
    malformed([](auto& v){v["schema"]=34;});
    auto large=*m.evidence.district_before;large.shape[0].resize(10000,{999999999999,999999999999});
    need(!district_wire_fits(m.request,large,{},e),"oversized complete District receipt admitted");++cases;
    std::cout<<cases<<" District complete state/protocol cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
