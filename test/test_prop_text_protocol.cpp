#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
Message sample(PropTextKind kind,bool scenery=false){
    Message m;m.session=7;m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::EditPropText;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";
    r.canonical_record_key="action:33333333-3333-4333-8333-333333333333";r.prop_text={kind,"New text"};
    PropTextState a;a.object.type=scenery?"cactus":"SmallInn";a.object.variant="variant";a.object.alive=true;
    a.scenery=scenery;a.name="Old name";a.description=scenery?"":"Old description";
    auto b=a;if(kind==PropTextKind::Name)b.name=r.prop_text.text;else b.description=r.prop_text.text;
    m.evidence.prop_text_before=a;m.evidence.prop_text_after=b;m.evidence.retirement_confirmed=true;return m;
}
int main(){try{unsigned count=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    for(auto kind:{PropTextKind::Name,PropTextKind::Description}){
        auto value=sample(kind);need(encode(value,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);
        need(decoded==value,"Prop text result roundtrip");++count;
        auto request=value;request.kind=Kind::Request;request.revision=0;request.required_mask=0;request.outcome=Outcome::Pending;request.evidence={};
        need(encode(request,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(decoded==request,"Prop text request roundtrip");++count;
        auto reject=[&](auto change){auto bad=value;change(bad);need(!encode(bad,bytes,error),"invalid Prop text result accepted");++count;};
        reject([](auto& m){m.evidence.prop_text_after->object.x=1;});
        reject([](auto& m){m.evidence.prop_text_after->object.alive=false;});
        reject([](auto& m){m.evidence.prop_text_after->object.variant="different";});
        reject([](auto& m){m.evidence.prop_text_after->scenery=true;});
        reject([](auto& m){m.evidence.prop_text_after.reset();});
        reject([](auto& m){++m.evidence.after.cash;});
        reject([](auto& m){m.evidence.undo_confirmed=true;});
        reject([](auto& m){m.request.npc_property.name="unrelated";});
        reject([](auto& m){m.request.canonical_record_key="region:67";});
        reject([](auto& m){m.request.prop_text.text=std::string("bad\0text",8);});
        reject([](auto& m){m.request.prop_text.text=std::string(1,char(-1));});
        need(encode(request,bytes,error),error);auto raw=nlohmann::json::parse(bytes);raw["request"]["prop_text"]["pointer"]=123;
        auto text=raw.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"unknown Prop field accepted");++count;
    }
    auto scene=sample(PropTextKind::Name,true);need(encode(scene,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(scene==decoded,"Scenery text roundtrip");++count;
    auto empty=sample(PropTextKind::Description);empty.request.prop_text.text.clear();empty.evidence.prop_text_after->description.clear();need(encode(empty,bytes,error),error);++count;
    auto bad=sample(PropTextKind::Description,true);need(!encode(bad,bytes,error),"Scenery description accepted");++count;
    auto mixed=sample(PropTextKind::Name);mixed.request.action=Action::EditNpcProperty;mixed.request.prop_text={};need(!encode(mixed,bytes,error),"foreign action Prop evidence accepted");++count;
    auto dungeon=sample(PropTextKind::Name);dungeon.evidence.prop_text_before->dungeon=true;dungeon.evidence.prop_text_after->dungeon=true;
    dungeon.evidence.prop_text_before->dungeon_name="Dungeon";dungeon.evidence.prop_text_before->dungeon_region_name="Region";
    dungeon.evidence.prop_text_after->dungeon_name=dungeon.request.prop_text.text;dungeon.evidence.prop_text_after->dungeon_region_name=dungeon.request.prop_text.text;
    need(encode(dungeon,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==dungeon,error);++count;
    dungeon.evidence.prop_text_after->dungeon_region_name="Region";need(!encode(dungeon,bytes,error),"Dungeon Region rename missing");++count;
    std::cout<<count<<" actual Prop text protocol cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
