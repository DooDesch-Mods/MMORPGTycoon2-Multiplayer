#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace bug=mt2mp::game::bug_authority;
namespace {
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
economy::State fixture(){
    economy::State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";
    f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;
    bug::Row row;row.parent_key=std::string(64,'a')+":world:building:31";
    row.birth={bug::BirthKind::Publication,f.snapshot,3,0};row.value.variant=0;row.value.reproducibility=.5f;
    f.bugs=bug::State{{row}};s.day.history={{1,live::day::record(f.values)}};return s;
}
bool decode(const nlohmann::json& j,economy::State& out,std::string& e){const auto bytes=j.dump();return economy::decode(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),out,e);}
}
int main(){try{
    auto s=fixture();std::string e;std::vector<uint8_t> bytes;economy::State actual;
    need(economy::encode(s,bytes,e)&&economy::decode(bytes.data(),bytes.size(),actual,e)&&actual==s,"full Economy/Day/HostFrame Bug roundtrip: "+e);
    const auto raw=nlohmann::json::parse(bytes);need(raw["day"]["frame"]["schema"]==12,"Host schema not12");
    auto bad=raw;bad["day"]["frame"]["bugs"][0][11]=2;need(!decode(bad,actual,e)&&actual==economy::State{},"malformed fullframe accepted");
    for(int schema:{9,10}){auto old=raw;old["day"]["frame"]["schema"]=schema;old["day"]["frame"].erase("bugs");if(schema==9)old["day"]["frame"].erase("developers");need(decode(old,actual,e)&&!actual.day.frame.bugs,"legacy absence lost: "+e);}
    auto old=raw;old["day"]["frame"]["schema"]=10;need(!decode(old,actual,e),"mixed schema accepted");
    s.day.frame.bugs=bug::State{};need(economy::encode(s,bytes,e)&&economy::decode(bytes.data(),bytes.size(),actual,e)&&actual==s,"empty authoritative Bugs lost");
    auto f=fixture().day.frame;auto previous=f;f.bugs->rows[0].value.toons=1;
    live::Context c;c.role=mt2mp::action_session::Role::Client;c.host_peer=1;c.local_peer=2;c.peer_mask=3;c.session=f.session;c.snapshot_uuid=f.snapshot;
    need(live::admit(1,c,f.epoch,f.source_generation,&previous,f,0,0,false,e)==live::Admission::Rejected,"same revision conflicting Bugs accepted");
    f=previous;f.bugs->rows[0].birth.sequence=4;need(!live::valid(f,e),"future publication accepted");
    f=previous;f.bugs->rows[0].birth.source="22222222-2222-4222-8222-222222222222";need(!live::valid(f,e),"foreign publication snapshot accepted");
    s=fixture();for(uint64_t i=1;i<10000;++i){auto row=s.day.frame.bugs->rows[0];row.birth.occurrence=i;s.day.frame.bugs->rows.push_back(row);}need(!economy::encode(s,bytes,e)&&e.find("1048576")!=std::string::npos,"existing transport cap bypassed: "+e);
    std::cout<<"PASS 11 complete-frame compatibility/domain/admission/cap cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
