#include "typed_action/action_session.h"
#include "service_membership_fixture.h"
#include <iostream>
using namespace service_fixture;
a::Message fixture(a::ServiceMembershipIntent intent){a::Message m;m.session=7;m.kind=a::Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=a::Outcome::Confirmed;
    auto& r=m.request;r.action=a::Action::EditServiceMembership;r.peer=2;r.sequence=1;r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";r.canonical_record_key="action:33333333-3333-4333-8333-333333333333";r.service_membership=intent;
    auto mem=memory();a::ServiceMembershipState before,after;reader::Local local;std::string e;need(read(mem,before,local,e),e);need(a::service_membership_expected(intent,before,after),"fixture expected");m.evidence.service_membership_before=before;m.evidence.service_membership_after=after;m.evidence.retirement_confirmed=true;return m;}
int main(){try{unsigned cases=0;std::vector<uint8_t> bytes;std::string e;a::Message decoded;
    for(auto intent:{a::ServiceMembershipIntent{a::ServiceMembershipKind::AddSpawn,-1,-1},a::ServiceMembershipIntent{a::ServiceMembershipKind::AddSpawn,0,-1},a::ServiceMembershipIntent{a::ServiceMembershipKind::Remove,-1,1}}){auto m=fixture(intent);
        need(a::encode(m,bytes,e),e);need(a::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);++cases;
        auto request=m;request.kind=a::Kind::Request;request.revision=0;request.required_mask=0;request.outcome=a::Outcome::Pending;request.evidence={};need(a::encode(request,bytes,e),e);need(a::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==request,e);++cases;
        auto bad=[&](auto change){auto copy=m;change(copy);need(!a::encode(copy,bytes,e),"invalid membership result accepted");++cases;};
        bad([](auto& v){v.evidence.service_membership_after->rows[0].price[1]++;});
        bad([](auto& v){v.evidence.service_membership_after->rows[0].counts[0].total++;});
        bad([](auto& v){v.evidence.service_membership_after->characters[0].name="Other";});
        bad([](auto& v){v.evidence.service_membership_after->rows.pop_back();});
        bad([](auto& v){v.evidence.service_membership_after->config_present=true;});
        bad([](auto& v){v.evidence.after.cash++;});bad([](auto& v){v.evidence.undo_confirmed=true;});
        bad([](auto& v){v.request.prop_text.text="foreign";});bad([](auto& v){v.evidence.service_membership_before.reset();});
        need(a::encode(request,bytes,e),e);auto raw=nlohmann::json::parse(bytes);raw["request"]["service_membership"]["pointer"]=8;auto text=raw.dump();need(!a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"unknown field accepted");++cases;
    }
    auto m=fixture({a::ServiceMembershipKind::AddSpawn,-1,-1});auto& before=*m.evidence.service_membership_before;before.rows.push_back(before.rows[0]);a::ServiceMembershipState after;need(a::service_membership_expected(m.request.service_membership,before,after),"duplicate replacement expected");m.evidence.service_membership_after=after;need(a::encode(m,bytes,e),e);++cases;
    m.evidence.service_membership_after->rows.push_back(before.rows[0]);need(!a::encode(m,bytes,e),"old duplicate survived replacement");++cases;
    m=fixture({});need(a::service_membership_wire_fits(m.request,*m.evidence.service_membership_before,m.evidence.before,e),e);++cases;
    auto large=*m.evidence.service_membership_before;large.characters.resize(a::max_payload/(4096*2)+2);
    for(size_t i=0;i<large.characters.size();++i){large.characters[i].id=int32_t(i);large.characters[i].name=std::string(4096,'c');}
    need(large.characters.size()*4096*2>a::max_payload,"oversize fixture must exceed the hard packet limit");
    need(!a::service_membership_wire_fits(m.request,large,m.evidence.before,e)&&e=="action payload exceeds "+std::to_string(a::max_payload)+" bytes","oversized complete result accepted before effect or rejected for another cause: "+e);++cases;
    std::cout<<cases<<" actual Service membership codec cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
