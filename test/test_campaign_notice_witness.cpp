#include <Windows.h>
#include "marketing_authority/notices.h"
#include "scenario_vip_authority/disconnect_read.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace n=mt2mp::game::marketing_authority::notice;
namespace probe=mt2mp::game::scenario_vip_authority::alert_probe;
namespace message=mt2mp::game::scenario_vip_authority::disconnect_read;
namespace graph=probe;
size_t checks=0;void need(bool okay,const std::string& why){++checks;if(!okay)throw std::runtime_error(why);}
namespace fixture {
#include "campaign_memory.inc"
}
namespace host_case {
using Json=n::Json;
fixture::Memory memory;
namespace native {struct ClockState{int32_t flat_day=3;};bool read_clock(ClockState& out,std::string&){out.flat_day=3;return true;}}
namespace notice_hook {using Original=bool(*)(uintptr_t);bool healthy(std::string&){return true;}bool verify(uintptr_t,std::string&){return true;}}
struct Sample{uintptr_t marketing=0;std::vector<uintptr_t> members;std::array<int32_t,6> fields{};uint8_t active=0;int32_t day=0;bool operator==(const Sample&)const=default;};
struct Observer{virtual ~Observer()=default;virtual bool observe(uintptr_t,notice_hook::Original)=0;};
struct Harness:Observer{
    uintptr_t image=fixture::image;bool updating=false,called=false;unsigned calls=0;std::string failure,phase;n::Host host;
    n::Origin bound_origin{41,7,9,"11111111-1111-4111-8111-111111111111"};Json last_witness=nullptr;graph::Graph actual;
    void latch(const std::string& e){if(failure.empty())failure=e.empty()?"fixture failure":e;}
    bool pin(std::string&){return true;}
    bool sample(uintptr_t receiver,Sample& out,bool& owned,std::string& e){out={};owned=phase!="foreign";out.marketing=fixture::manager;out.members={receiver};out.day=3;
        for(size_t i=0;i<out.fields.size();++i)if(!message::at(memory,receiver,0x50+i*4,out.fields[i],e))return false;
        return message::at(memory,receiver,0x68,out.active,e);
    }
    bool read_graph(graph::Graph& out,std::string& e){if(phase=="before"&&!called){e="original graph unreadable";return false;}out=actual;if(!called)out.owned={};if(phase=="owner"&&called)++out.manager;return true;}
#include "campaign_witness.inc"
};
Harness* current=nullptr;constexpr uintptr_t campaign=0x310000;
bool original(uintptr_t receiver){need(current!=nullptr&&receiver==campaign,"original receiver changed");++current->calls;current->called=true;
    if(current->phase=="throw")throw std::runtime_error("original exception");
    if(current->phase=="future")return true;
    if(current->phase!="active")memory.put(receiver,0x68,uint8_t(0));
    if(current->phase=="type")memory.put(fixture::alert,0x18,int32_t(9));
    return false;
}
void setup(Harness& h){memory.setup();memory.put(fixture::image+0x1749770,8,fixture::image+0x234c70);memory.put(fixture::alert,0x18,int32_t(0));
    memory.text(fixture::alert,0xb0,0x200000,"Launch finished. 3 new subscribers. {literal}");memory.array(fixture::manager,0x20,0x210000,{fixture::alert});
    for(size_t i=0;i<6;++i)memory.put(campaign,0x50+i*4,int32_t(i==0?6:i==1?1:i==2?2:0));memory.put(campaign,0x68,uint8_t(1));
    std::string e;probe::Reader reader(memory,fixture::image,0x2000000);need(reader.read_ui(fixture::mode,fixture::fixture_state,h.actual,e),e);need(h.host.bind(h.bound_origin,2,e),e);current=&h;
}
void cases(){for(const auto& phase:{"okay","before","active","type","owner","foreign","future","throw"}){
    Harness h;setup(h);h.phase=phase;if(h.phase=="future")memory.put(campaign,0x58,int32_t(5));bool result=false,threw=false;
    try{result=h.observe(campaign,original);}catch(const std::exception& x){need(std::string(x.what())=="original exception","original exception hidden");threw=true;}
    need(h.calls==1,"original IsActive duplicated/skipped");need(result==(h.phase=="future")&&threw==(h.phase=="throw"),"original return/exception changed");
    if(h.phase=="okay"){need(h.failure.empty()&&h.host.pending().rows.size()==1&&h.host.pending().rows[0].message=="Launch finished. 3 new subscribers. {literal}","actual type0 full payload absent");need(!h.observe(campaign,original)&&h.calls==2&&h.host.pending().rows.size()==1,"inactive repeat emitted message");}
    else if(h.phase=="future"||h.phase=="foreign")need(h.failure.empty()&&h.host.pending().rows.empty(),"unrelated original call fabricated event");
    else need(!h.failure.empty()&&h.host.pending().rows.empty(),"unconfirmed native event published");
}}
}
namespace mt2mp {std::string sha256(const std::vector<unsigned char>&){return "fixture-original-hash";}}
namespace hook_case {
constexpr uintptr_t rva=0x518290;constexpr size_t extent=5968;constexpr const char* sha="fixture-original-hash";
uintptr_t image=fixture::image;bool ready=true,attempted=true;std::string installation_error;
bool trampoline_call(uintptr_t){return false;}using Original=bool(*)(uintptr_t);Original original=trampoline_call;
bool dispatch(uintptr_t){return false;}
fixture::Memory memory;std::array<unsigned char,16> installed{};std::array<unsigned char,64> relocated{};std::vector<unsigned char> pristine(extent,0x90);
bool fail(std::string& e,const std::string& text){e=text;return false;}
#include "campaign_hook_verify.inc"
void setup(){memory.bytes.clear();std::vector<unsigned char> body=pristine;body[0]=0xe9;const int32_t displacement=0x10000;std::memcpy(body.data()+1,&displacement,4);
    std::copy_n(body.begin(),installed.size(),installed.begin());relocated.fill(0xcc);
    for(size_t i=0;i<body.size();++i)memory.put(image+rva,i,body[i]);
    for(size_t i=0;i<relocated.size();++i)memory.put(reinterpret_cast<uintptr_t>(original),i,relocated[i]);
    std::array<unsigned char,14> relay{0xff,0x25,0,0,0,0};auto target=reinterpret_cast<uintptr_t>(&dispatch);std::memcpy(relay.data()+6,&target,sizeof target);
    for(size_t i=0;i<relay.size();++i)memory.put(image+rva+5+displacement,i,relay[i]);
}
void cases(){std::string e;setup();need(verify(image,e),e);
    for(int fault=0;fault<5;++fault){setup();switch(fault){case 0:memory.put(image+rva,0,uint8_t(0x90));break;case 1:memory.put(image+rva,100,uint8_t(0));break;case 2:memory.put(reinterpret_cast<uintptr_t>(original),12,uint8_t(0));break;case 3:memory.put(image+rva+5+0x10000,6,uintptr_t(0));break;case 4:memory.unreadable=image+rva;break;}need(!verify(image,e)&&!e.empty(),"owned hook entry/tail/trampoline/relay/read failure accepted");}
}
}
int main(){try{host_case::cases();hook_case::cases();std::cout<<"PASS campaign_notice_witness "<<checks<<" checks\n";return 0;}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<'\n';return 1;}}
