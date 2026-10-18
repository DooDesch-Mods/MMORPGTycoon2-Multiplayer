#include <Windows.h>
#include "marketing_authority/notice_lifecycle.h"
#include "scenario_vip_authority/disconnect_invoke.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
namespace n=mt2mp::game::marketing_authority::notice;
namespace c=mt2mp::game::scenario_vip_authority::client_alert;
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace {
size_t checks=0;void need(bool okay,const std::string& why){++checks;if(!okay)throw std::runtime_error(why);}
n::Origin origin(){return {41,7,9,"11111111-1111-4111-8111-111111111111"};}
struct Port:c::Port {
    unsigned factories=0,fires=0,discards=0;std::string phase;bool alive=false,shown=false;std::string text;
    bool owner(const c::Owner*,bool,c::Owner& out,std::string&)override{out={};return true;}
    bool subject(const c::Owner&,int32_t,const std::string&,c::Subject&,std::string&)override{return false;}
    bool inspect(const c::Owner&,uintptr_t,const c::Subject&,bool,int,uintptr_t&,std::string&)override{return false;}
    bool absent(const c::Owner&,uintptr_t,uintptr_t,std::string&)override{return !alive;}
    bool create(const c::Owner&,const c::Subject&,c::Call&,std::string&)override{return false;}
    bool show(const c::Owner&,uintptr_t,bool,c::Call&,std::string&)override{return false;}
    bool destroy(const c::Owner&,uintptr_t,c::Call&,std::string&)override{return false;}
    bool create_message(const c::Owner&,const std::string& value,c::MessageCalls& r,std::string& e)override{
        ++factories;r.loc_construct.attempted=r.loc_construct.returned=true;r.create.attempted=true;
        if(phase=="factory"){e="original factory failed";return false;}
        r.create.returned=true;r.create.result=160;alive=true;text=phase=="text"?"changed":value;return true;
    }
    bool inspect_message(const c::Owner&,uintptr_t p,const std::string& value,int expected,uintptr_t& selector,std::string& e)override{
        if(!alive||p!=160||shown!=bool(expected)||text!=value||phase==(expected?"post":"pre")){e="message bytes/owner differ";return false;}selector=expected?170:0;return true;
    }
    bool fire_message(const c::Owner&,uintptr_t,c::Call& r,std::string& e)override{++fires;r.attempted=true;shown=true;if(phase=="fire"){e="original Fire unknown";return false;}r.returned=true;return true;}
    bool discard_message(const c::Owner&,uintptr_t,c::Call& r,std::string&)override{++discards;r.attempted=true;if(shown)return false;alive=false;r.returned=true;return true;}
};
void retention(){std::string e;auto o=origin();n::Host host;n::Client client;need(host.bind(o,6,e)&&client.bind(o,e),e);
    need(host.append(o,"Launch finished. 200 impressions, 3 subscribers.",e),e);const auto first=host.pending();need(host.sent(o,1,first,e),e);
    need(host.append(o,"Positive streamer finished. Grüße {literal}",e),e);const auto second=host.pending();need(host.sent(o,2,second,e),e);
    need(client.supersede(o,second,&first,e),e);auto missing=second;missing.rows.pop_back();need(!client.supersede(o,missing,&second,e),"supersession dropped pending message");
    std::vector<n::Row> work;need(client.prepare(o,second,work,e)&&work.size()==2,e);Port port;
    for(const auto& row:work){n::Receipt r;need(n::present(port,row,r,e)&&r.confirmed&&r.calls.handed_off&&!r.calls.create.result,e);port.shown=false;}
    need(client.confirm(o,second,work,e),e);need(client.prepare(o,second,work,e)&&work.empty()&&port.factories==2&&port.fires==2,"duplicate replays native handoff");
    auto conflict=second;conflict.rows[0].message="Other";need(!client.prepare(o,conflict,work,e),"conflicting duplicate accepted");
    need(host.acknowledge(o,2,2,e)&&host.pending()==second,"single peer ACK dropped other peer pending message");
    need(host.acknowledge(o,3,1,e)&&host.pending().after==1&&host.pending().rows.size()==1,"common ACK coverage wrong");
    need(host.acknowledge(o,3,2,e)&&host.pending().after==2&&host.pending().rows.empty(),"full Apply ACK did not release pending payload");
    need(client.prepare(o,host.pending(),work,e)&&work.empty()&&client.confirm(o,host.pending(),work,e),e);
    need(!client.prepare(o,first,work,e),"stale floor reintroduced prior payload");auto foreign=o;++foreign.generation;need(!client.prepare(foreign,second,work,e),"foreign origin accepted");
    need(!host.acknowledge(o,4,2,e)&&!host.acknowledge(o,2,3,e),"foreign/unsent ACK accepted");
    for(int fault=0;fault<5;++fault){auto invalid=first;if(fault==0)invalid.rows[0].sequence=2;if(fault==1)invalid.rows[0].message="";if(fault==2)invalid.rows[0].message=std::string("a\0b",3);if(fault==3)invalid.rows[0].message=std::string("\xff",1);if(fault==4){invalid.after=UINT64_MAX;invalid.rows[0].sequence=0;}need(!n::valid(invalid,e),"unsafe notice admitted");}
    for(const auto& phase:{"factory","pre","text","fire","post"}){Port bad;bad.phase=phase;n::Receipt r;need(!n::present(bad,first.rows[0],r,e)&&r.unknown&&!r.confirmed&&!e.empty(),"failed handoff confirmed");need(bad.factories==1&&bad.discards==unsigned(bad.phase=="pre"||bad.phase=="text"),"uncertain Manager-owned alert disposed");}
}
void packet(){std::string e;economy::State state;auto& f=state.day.frame;auto o=origin();f.session=o.session;f.epoch=o.epoch;f.source_generation=o.generation;f.snapshot=o.snapshot;f.revision=1;f.selected=f.pause=1;
    f.values.calendar={1,1,4,3,2987,7};f.values.cash=81360;f.values.record={3,81360,0,0,0};state.day.history={{3,live::day::record(f.values)}};
    f.marketing=mt2mp::game::marketing_authority::State{{{"awards",n::Json::array()},{"campaigns",n::Json::array()},{"positiveBuzz",0},{"negativeBuzz",0}},n::Batch{0,{{1,"Launch finished. 200 impressions and 3 new subscribers. Grüße {literal}"}}}};
    std::vector<uint8_t> bytes;economy::State decoded;need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"Frame16 complete packet: "+e);
    auto raw=n::Json::parse(bytes);need(raw["day"]["frame"]["schema"]==16,"notice schema not16");
    for(int fault=0;fault<5;++fault){auto bad=raw;auto& frame=bad["day"]["frame"];if(fault==0)frame["schema"]=15;if(fault==1)frame["marketing"].erase("campaignNotices");if(fault==2)frame["marketing"]["campaignNotices"]["rows"][0].push_back(9);if(fault==3)frame["marketing"]["campaignNotices"]["after"]=-1;if(fault==4)frame["marketing"]["campaignNotices"]["rows"][0][1]=false;auto text=bad.dump();need(!economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"malformed/downgraded notice packet accepted");}
    f.marketing->notices.reset();need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"legacy15 changed");
}
unsigned constructs=0,factories=0,arguments=0,strings=0;uintptr_t storage=0;int fault=0;
void construct(void* p,const char*){++constructs;storage=reinterpret_cast<uintptr_t>(p);need(storage%8==0,"LocString alignment");if(fault==1)RaiseException(0xe0123456,0,0,nullptr);}
uintptr_t factory(int32_t type,const void* p){++factories;need(type==0&&reinterpret_cast<uintptr_t>(p)==storage,"type0 factory ABI");if(fault==2)RaiseException(0xe0123456,0,0,nullptr);return 160;}
void args(void* p){++arguments;need(reinterpret_cast<uintptr_t>(p)==storage+0x20,"LocString args receiver");if(fault==3)RaiseException(0xe0123456,0,0,nullptr);}
void dispose(void* p){++strings;need(reinterpret_cast<uintptr_t>(p)==storage,"LocString string receiver");if(fault==4)RaiseException(0xe0123456,0,0,nullptr);}
void abi(){for(fault=0;fault<=4;++fault){constructs=factories=arguments=strings=0;c::MessageCalls r;c::message_invoke::create({construct,factory,args,dispose},"Campaign result",r,0);need(constructs==1&&factories==(fault==1?0u:1u)&&arguments==(fault==1?0u:1u)&&strings==(fault==1?0u:1u),"type0 temporary lifetime count");if(fault)need((fault==1?r.loc_construct.exception:fault==2?r.create.exception:fault==3?r.loc_arguments_destroy.exception:r.loc_string_destroy.exception)==0xe0123456,"SEH cause lost");}}
}
int main(){try{retention();packet();abi();std::cout<<"PASS campaign_notice "<<checks<<" checks\n";return 0;}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<'\n';return 1;}}
