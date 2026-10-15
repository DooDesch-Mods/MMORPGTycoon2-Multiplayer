#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
economy::State frame(){
    economy::State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";
    f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;
    staff::PoolState pool;pool.count=2;
    for(int32_t i=0;i<50;++i){staff::PoolRow r;r.id=49-i;r.active=r.employed=i<2;pool.rows.push_back(r);pool.indoor.push_back(r.id);}
    f.developers=pool;s.day.history={{1,live::day::record(f.values)}};return s;
}
bool decode(const nlohmann::json& raw,economy::State& out,std::string& e){const auto b=raw.dump();return economy::decode(reinterpret_cast<const uint8_t*>(b.data()),b.size(),out,e);}
}
int main(){try{
    const auto state=frame();std::string e;std::vector<uint8_t> bytes;economy::State actual;
    need(economy::encode(state,bytes,e),"complete Economy encode: "+e);
    need(economy::decode(bytes.data(),bytes.size(),actual,e)&&actual==state,"complete Economy11/Day/HostState10 full50 roundtrip: "+e);
    need(bytes.size()<live::max_frame_payload,"full50 pool exceeded the existing Channel9 bound");
    auto raw=nlohmann::json::parse(bytes);auto bad=raw;bad["day"]["frame"]["developers"]["rows"][0][0]=-1;
    need(!decode(bad,actual,e)&&actual==economy::State{}&&e.find("Developer")!=std::string::npos,"malformed inner Developer row produced a complete frame");
    bad=raw;bad["day"]["frame"]["developers"]["indoor"].erase(0);
    need(!decode(bad,actual,e)&&actual==economy::State{},"missing manager member passed complete decoder");
    bad=raw;bad["day"]["frame"]["developers"]["rows"][1][0]=bad["day"]["frame"]["developers"]["rows"][0][0];
    need(!decode(bad,actual,e),"duplicate persisted ID passed complete decoder");
    auto old=raw;old["day"]["frame"]["schema"]=9;old["day"]["frame"].erase("developers");
    need(decode(old,actual,e)&&!actual.day.frame.developers,"schema9 compatibility fabricated pool ownership");
    old["day"]["frame"]["developers"]=nullptr;
    need(!decode(old,actual,e),"mixed schema9/new pool shape accepted");
    auto c=mt2mp::action_session::Context{};c.role=mt2mp::action_session::Role::Client;c.local_peer=2;c.host_peer=1;c.peer_mask=3;c.session=41;c.snapshot_uuid=state.day.frame.snapshot;
    auto before=state.day.frame;before.base_action_revision=4;before.for_action="22222222-2222-4222-8222-222222222222";
    mt2mp::action_session::Message result;result.session=41;result.kind=mt2mp::action_session::Kind::HostResult;result.outcome=mt2mp::action_session::Outcome::Confirmed;result.revision=5;
    result.request.snapshot_uuid=c.snapshot_uuid;result.request.action_uuid=before.for_action;result.evidence.before=live::ledger(before.values);result.evidence.developers_before=before.developers;
    need(live::admit_before_result(1,c,7,9,nullptr,before,result,4,0,e)==live::Admission::New,"matching full native BEFORE pool rejected: "+e);
    before.developers->rows[0].active=false;
    need(live::admit_before_result(1,c,7,9,nullptr,before,result,4,0,e)==live::Admission::Rejected&&e.find("Developer BEFORE")!=std::string::npos,"cash-equal but mismatched full50 BEFORE pool admitted");
    before.developers.reset();need(live::admit_before_result(1,c,7,9,nullptr,before,result,4,0,e)==live::Admission::Rejected,"missing Host-before pool admitted a Hire/Fire result");
    std::cout<<"PASS "<<checks<<" complete-frame Developer checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
