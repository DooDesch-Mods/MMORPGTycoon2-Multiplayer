#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace vip=live::vip;
namespace economy=live::day::economy;
namespace {
size_t checks=0;
void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
vip::Origin origin(){return {41,7,9,"11111111-1111-4111-8111-111111111111"};}
economy::State fixture(const vip::Batch& b){
    economy::State s;auto& f=s.day.frame;const auto o=origin();
    f.session=o.session;f.snapshot=o.snapshot;f.epoch=o.epoch;f.source_generation=o.generation;
    f.revision=3;f.selected=1;f.pause=1;f.values.calendar={1,1,2,1,30,7};
    f.values.record={1,93640,0,0,0};f.values.cash=93640;f.vips=b;
    s.day.history={{1,live::day::record(f.values)}};return s;
}
bool raw_decode(const nlohmann::json& j,economy::State& out,std::string& e){
    const auto text=j.dump();return economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e);
}
}
int main(){try{
    std::string e;const auto o=origin();vip::HostRetention host;vip::ClientRetention client;
    need(host.bind(o,6,e)&&client.bind(o,e),"bind: "+e);uint64_t first=0,retired=0;
    need(host.append(o,151,"Visitor",true,0,first,e)&&first==1,"arrival: "+e);
    need(host.append(o,151,"Visitor",false,first,retired,e)&&retired==2,"retirement before capture: "+e);
    const auto first_batch=host.pending();need(first_batch.rows.size()==2,"transient arrival lost");
    need(host.sent(o,10,first_batch,e),"first frame: "+e);
    uint64_t reused=0;need(host.append(o,151,"Visitor",true,0,reused,e)&&reused==3,"reused Subscriber is distinct arrival");
    const auto newer=host.pending();need(newer.rows.size()==3&&newer.rows[0]==first_batch.rows[0],"superseding frame lost older transition");
    need(host.sent(o,11,newer,e),"newer frame: "+e);
    std::vector<vip::Transition> work;
    need(client.prepare(o,newer,work,e)&&work==newer.rows&&client.committed()==0,"prepare mutated committed receipt");
    auto partial=work;partial.pop_back();need(!client.confirm(o,newer,partial,e)&&client.committed()==0,"partial native receipt confirmed");
    need(client.confirm(o,newer,work,e)&&client.committed()==3,"confirm: "+e);
    need(client.prepare(o,newer,work,e)&&work.empty(),"same batch replayed UI work");
    auto conflicting=newer;conflicting.rows[0].name="Other";
    need(!client.prepare(o,conflicting,work,e),"conflicting duplicate accepted");
    need(host.acknowledge(o,2,11,e)&&host.pending()==newer,"one peer pruned unACKed events");
    need(!host.acknowledge(o,3,12,e),"future ACK accepted");
    need(!host.acknowledge(o,1,11,e),"foreign peer accepted");
    need(host.acknowledge(o,3,10,e)&&host.pending().after==2&&host.pending().rows.size()==1,"ACK coverage did not retain later arrival");
    need(host.acknowledge(o,3,11,e)&&host.pending().after==3&&host.pending().rows.empty(),"all-peer ACK did not prune");
    need(host.acknowledge(o,3,10,e)&&host.pending().after==3,"old ACK regressed floor");
    need(client.prepare(o,host.pending(),work,e)&&work.empty()&&client.confirm(o,host.pending(),work,e),"pruned suffix rejected");
    need(host.append(o,151,"Visitor",false,reused,retired,e)&&retired==4,"retire ACKed arrival: "+e);
    need(client.prepare(o,host.pending(),work,e)&&work.size()==1&&client.confirm(o,host.pending(),work,e),"retirement lost persistent arrival identity");
    need(!host.append(o,151,"Visitor",false,reused,retired,e),"duplicate retirement accepted");
    for(int field=0;field<4;++field){auto foreign=o;if(field==0)++foreign.session;if(field==1)++foreign.epoch;if(field==2)++foreign.generation;if(field==3)foreign.snapshot[0]='2';
        need(!client.prepare(foreign,host.pending(),work,e),"foreign full origin accepted");
        need(!host.acknowledge(foreign,2,11,e),"foreign ACK origin accepted");}
    auto bad=host.pending();bad.after=5;bad.rows.clear();need(!client.prepare(o,bad,work,e),"missing prefix accepted");
    bad={3,{{4,1,151,"Wrong name",false}}};vip::ClientRetention unknown;need(unknown.bind(o,e),"bind unknown");
    need(!unknown.prepare(o,bad,work,e),"retirement without arrival accepted");
    const vip::Batch arrival{0,{{1,1,151,"Visitor",true}}};
    need(unknown.prepare(o,arrival,work,e)&&unknown.confirm(o,arrival,work,e),"selected arrival setup");
    for(int field=0;field<3;++field){bad={1,{{2,1,151,"Visitor",false}}};
        if(field==0)bad.rows[0].subscriber=152;if(field==1)bad.rows[0].name="Changed";if(field==2)bad.rows[0].arrival=2;
        need(!unknown.prepare(o,bad,work,e)&&unknown.committed()==1,"retirement owner/value drift accepted");}
    bad={0,{{1,1,151,"A",true},{3,3,151,"B",true}}};need(!vip::valid(bad,e),"sequence gap accepted");
    bad={UINT64_MAX,{{0,0,151,"A",true}}};need(!vip::valid(bad,e),"sequence overflow accepted");
    bad={0,{{1,1,151,std::string(257,'x'),true}}};need(!vip::valid(bad,e),"oversize name accepted");
    bad.rows[0].name=std::string("A\0B",3);need(!vip::valid(bad,e),"embedded NUL accepted");
    bad.rows[0].name=std::string(1,static_cast<char>(0xff));need(!vip::valid(bad,e),"invalid UTF-8 accepted");
    auto raw=vip::encode(first_batch);vip::Batch parsed;
    need(vip::decode(raw,parsed,e)&&parsed==first_batch,"component roundtrip");
    for(int column=0;column<5;++column){auto malformed=raw;malformed["rows"][0][column]=nullptr;need(!vip::decode(malformed,parsed,e),"partial row accepted");}
    raw=vip::encode(first_batch);raw["extra"]=0;need(!vip::decode(raw,parsed,e),"unknown field accepted");
    auto state=fixture(first_batch);std::vector<uint8_t> bytes;economy::State decoded;
    need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"whole Economy/Day/Frame roundtrip: "+e);
    auto frame=nlohmann::json::parse(bytes);need(frame["day"]["frame"]["schema"]==13,"VIP did not require schema13");
    auto duplicate=frame.dump();const auto at=duplicate.find("\"after\":0");need(at!=std::string::npos,"duplicate-key fixture marker absent");
    duplicate.replace(at,9,"\"after\":0,\"after\":1");
    need(!economy::decode(reinterpret_cast<const uint8_t*>(duplicate.data()),duplicate.size(),decoded,e),"conflicting duplicate batch field accepted");
    auto old=frame;old["day"]["frame"]["schema"]=12;need(!raw_decode(old,decoded,e),"schema12 silently accepted VIP");
    old=frame;old["day"]["frame"].erase("vips");need(!raw_decode(old,decoded,e),"schema13 silently lost VIP");
    old=frame;old["day"]["frame"]["vips"]=nullptr;need(!raw_decode(old,decoded,e),"schema13 null component accepted");
    state.day.frame.vips.reset();need(economy::encode(state,bytes,e),"legacy encode");
    need(nlohmann::json::parse(bytes)["day"]["frame"]["schema"]==12,"inactive schema changed");
    need(economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"inactive roundtrip");
    auto changed=fixture(first_batch).day.frame;auto original=changed;changed.vips->rows[0].name="Conflict";
    live::Context context{41,2,1,3,mt2mp::action_session::Role::Client,o.snapshot};
    need(live::admit(1,context,7,9,&original,changed,0,0,false,e)==live::Admission::Rejected,"same Frame revision conflict admitted");
    client.reset();host.reset();need(!client.prepare(o,first_batch,work,e)&&!host.sent(o,12,first_batch,e),"reset retained previous origin");
    vip::HostRetention alone;uint64_t single=0;
    need(alone.bind(o,0,e),"Host-only world cannot bind before a friend joins");
    need(alone.append(o,151,"Visitor",true,0,single,e)&&single==1,"Host-only arrival identity lost");
    const auto solo=alone.pending();
    need(solo.rows.size()==1&&!alone.sent(o,0,solo,e)&&alone.pending()==solo,"failed Host-only send pruned history");
    need(alone.sent(o,1,solo,e)&&alone.pending().after==1&&alone.pending().rows.empty(),"no-remote successful publication retained unnecessary history");
    need(!alone.acknowledge(o,2,1,e),"Host-only world accepted a foreign ACK");
    need(alone.append(o,151,"Visitor",false,1,single,e)&&single==2,"Host-only publication lost active retirement identity");
    need(alone.sent(o,2,alone.pending(),e)&&alone.pending().after==2,"Host-only retirement publication failed");
    alone.reset();need(!alone.append(o,151,"Visitor",true,0,single,e),"reset Host-only retention accepted a stale origin");
    std::cout<<"PASS "<<checks<<" actual VIP codec/retention checks\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
