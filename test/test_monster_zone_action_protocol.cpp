#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
const char* id="11111111-1111-4111-8111-111111111111";
Message request(){Message m;m.session=5;auto& r=m.request;r.action=Action::PlaceMonsterZone;r.peer=2;r.sequence=1;r.action_uuid=id;r.snapshot_uuid="22222222-2222-4222-8222-222222222222";r.canonical_record_key=std::string("action:")+id;r.zone={67,0,1,{{{8020,0,8550},{{{7988000,8518000},{8051000,8550000},{8020000,8581000}}}}}};return m;}
Message result(unsigned spawned){auto m=request();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;auto& e=m.evidence;e.before.cash=e.after.cash=87900;e.undo_confirmed=e.retirement_confirmed=true;e.zone_before.content_sha256=std::string(64,'a');e.zone_after=e.zone_before;auto& a=e.zone_after;a.zones=1;a.monsters=spawned;a.undo_count=1;a.next_tag=a.created_monsters=int32_t(spawned);a.release_edits[5]=1;a.content_sha256=std::string(64,'b');return m;}
int main(){try{std::string e;std::vector<uint8_t> bytes;Message out;unsigned count=0;
 for(auto m:{request(),result(0),result(4)}){need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e),e);need(out==m,"exact Zone schema25 roundtrip");++count;}
 auto m=request();need(encode(m,bytes,e),e);auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==25,"wrong Zone schema");
 auto bad=[&](nlohmann::json j){auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"bad Zone wire accepted");++count;};
 for(auto v:{nlohmann::json(true),nlohmann::json(1.0),nlohmann::json(-1),nlohmann::json("1")}){auto j=raw;j["request"]["zone"]["region"]=v;bad(j);}
 {auto j=raw;j["schema"]=22;bad(j);} {auto j=raw;j["request"]["action"]=20;bad(j);} {auto j=raw;j["request"]["zone"]["extra"]=0;bad(j);}
 {auto j=raw;j["request"]["zone"]["stamps"]=nlohmann::json::array();bad(j);} {auto j=raw;j["request"]["zone"]["stamps"][0]["paths"][0][0]=true;bad(j);}
 auto invalid=[&](auto change){auto v=result(4);change(v);need(!encode(v,bytes,e),"invalid Zone effect accepted");++count;};
 invalid([](Message& v){++v.evidence.after.cash;});invalid([](Message& v){v.evidence.zone_after.zones=2;});
 invalid([](Message& v){v.evidence.zone_after.next_tag=3;});invalid([](Message& v){v.evidence.zone_after.created_monsters=3;});
 invalid([](Message& v){++v.evidence.zone_after.region_random[0];});invalid([](Message& v){++v.evidence.zone_after.release_edits[4];});
 invalid([](Message& v){v.evidence.zone_after.release_edits[5]=0;});invalid([](Message& v){v.evidence.zone_after.undo_count=0;});
 invalid([](Message& v){v.request.npc.type=1;});invalid([](Message& v){v.evidence.retirement_confirmed=false;});
 std::cout<<count<<" Zone actual schema/effect checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
