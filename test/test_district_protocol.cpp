#include "typed_action/action_session.h"
#include <iostream>
#include <memory>
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
    unsigned cases=0;auto m_storage=std::make_unique<Message>(sample());auto& m=*m_storage;std::string e;std::vector<uint8_t> bytes;auto out_storage=std::make_unique<Message>();auto& out=*out_storage;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==m,e);++cases;
    need(district_wire_fits(m.request,*m.evidence.district_before,m.evidence.before,e),e);++cases;
    auto roundtrip=[&](const Message& v){need(encode(v,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==v,e);++cases;};
    auto empty_storage=std::make_unique<Message>(m);auto& empty=*empty_storage;empty.request.district_text.text="";empty.evidence.district_after->name="{district_unnamed}";roundtrip(empty);
    auto moved_storage=std::make_unique<Message>(m);auto& moved=*moved_storage;moved.evidence.district_before->flags[11]=1;moved.evidence.district_before->manual_title_inside=false;
    roundtrip(moved);need(district_wire_fits(moved.request,*moved.evidence.district_before,moved.evidence.before,e),e);++cases;
    auto description_storage=std::make_unique<Message>(m);auto& description=*description_storage;description.request.district_text.kind=DistrictTextKind::Description;description.request.district_text.text="";
    description.evidence.district_after=description.evidence.district_before;description.evidence.district_after->description="";roundtrip(description);
    auto request_storage=std::make_unique<Message>(m);auto& request=*request_storage;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};roundtrip(request);
    auto reject=[&](auto edit){auto bad_storage=std::make_unique<Message>(m);auto& bad=*bad_storage;edit(bad);need(!encode(bad,bytes,e),"invalid District effect accepted");++cases;};
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

    namespace property=mt2mp::game::district_property;
    for(const auto& field:property::fields){
        auto edit_storage=std::make_unique<Message>(sample());auto& edit=*edit_storage;edit.request.action=Action::EditDistrict;edit.request.district_text={};
        edit.request.district_property.kind=field.kind;
        auto& before=*edit.evidence.district_before;before.geo=1;
        before.values[8]=std::bit_cast<uint32_t>(10.f);before.values[9]=std::bit_cast<uint32_t>(8.f);
        if(field.type==property::Type::Flag)edit.request.district_property.bits[0]=1;
        else if(field.type==property::Type::Color)edit.request.district_property.bits.fill(std::bit_cast<uint32_t>(0.5f));
        else edit.request.district_property.bits[0]=std::bit_cast<uint32_t>(field.minimum>0?field.minimum+1.f:1.f);
        DistrictState after;need(property::expected(before,edit.request.district_property,0,after,e),e);edit.evidence.district_after=after;
        edit.evidence.undo_confirmed=field.original_history;
        roundtrip(edit);need(district_property_wire_fits(edit.request,before,edit.evidence.before,0,e),e);++cases;
        need(encode(edit,bytes,e),e);auto property_json=nlohmann::json::parse(bytes);need(property_json["schema"]==36,"new District packet uses legacy schema");++cases;
        for(int old_schema:{19,35}){auto bad=property_json;bad["schema"]=old_schema;const auto wire=bad.dump();need(!decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),out,e),"District edit accepted under old schema");++cases;}
        auto bad_storage=std::make_unique<Message>(edit);auto& bad=*bad_storage;bad.evidence.district_after->constructed_day++;need(!encode(bad,bytes,e),"property changed unrelated constructed day");++cases;
        bad=edit;bad.evidence.undo_confirmed=!field.original_history;need(!encode(bad,bytes,e),"property invented or dropped original history");++cases;
        bad=edit;bad.evidence.after.line_counts[0]++;need(!encode(bad,bytes,e),"property changed Ledger");++cases;
        auto intent_storage=std::make_unique<Message>(edit);auto& intent=*intent_storage;intent.kind=Kind::Request;intent.revision=0;intent.required_mask=0;intent.outcome=Outcome::Pending;intent.evidence={};roundtrip(intent);
        auto born_storage=std::make_unique<Message>(edit);auto& born=*born_storage;born.request.canonical_record_key="district:action:33333333-3333-4333-8333-333333333333:0";roundtrip(born);
        if(field.original_history){
            auto inverse_storage=std::make_unique<Message>(edit);auto& inverse=*inverse_storage;inverse.request.action=Action::UndoBuilding;inverse.request.target_action_uuid=edit.request.action_uuid;
            inverse.request.action_uuid="44444444-4444-4444-8444-444444444444";inverse.request.canonical_record_key="district-edit:"+inverse.request.target_action_uuid;
            inverse.request.district_property={};std::swap(inverse.evidence.district_before,inverse.evidence.district_after);roundtrip(inverse);
            need(district_history_wire_fits(inverse.request,inverse.evidence,e),e);++cases;
            need(encode(inverse,bytes,e),e);auto old=nlohmann::json::parse(bytes);old["schema"]=35;const auto wire=old.dump();
            need(!decode(reinterpret_cast<const uint8_t*>(wire.data()),wire.size(),out,e),"new District history accepted under schema35");++cases;
            inverse.request.action=Action::RedoBuilding;std::swap(inverse.evidence.district_before,inverse.evidence.district_after);roundtrip(inverse);
        }
    }
    {
        auto prior=*sample().evidence.district_before;prior.geo=2;prior.flags[7]=prior.flags[8]=prior.flags[5]=1;
        prior.values[23]=std::bit_cast<uint32_t>(-5.f);prior.values[25]=std::bit_cast<uint32_t>(-6.f);
        property::Value value{property::Kind::Spread,{std::bit_cast<uint32_t>(2.f),0,0,0}};DistrictState after;
        need(property::expected(prior,value,std::bit_cast<uint32_t>(1.f),after,e),e);++cases;
        need(after.values[23]==std::bit_cast<uint32_t>(-1.f)&&after.values[25]==after.values[23]&&after.values[26]==after.values[24]&&after.flags[9]==1&&after.flags[10]==0,"original clamp/lock/obstruction order differs");++cases;
        auto wrong=after;wrong.values[25]=prior.values[25];need(!property::effect(prior,value,std::bit_cast<uint32_t>(1.f),wrong,e),"missing original derived water clamp accepted");++cases;
        value.bits[0]=0x7fc00000;need(!property::valid(value),"nonfinite property accepted");++cases;
        value={property::Kind::ShowTitle,{2,0,0,0}};need(!property::valid(value),"nonboolean property accepted");++cases;
    }

    std::cout<<cases<<" District complete state/protocol cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
