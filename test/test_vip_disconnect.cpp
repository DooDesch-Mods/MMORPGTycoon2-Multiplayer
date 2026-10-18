#include <Windows.h>
#include "scenario_vip_authority/host_transitions.h"
#include "scenario_vip_authority/alert_lifecycle.h"
#include "scenario_vip_authority/disconnect_read.h"
#include "scenario_vip_authority/disconnect_invoke.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;
namespace c=mt2mp::game::scenario_vip_authority::client_alert;
namespace v=c::vip;
namespace vip_host=mt2mp::game::scenario_vip_authority;
namespace probe=vip_host::alert_probe;
namespace messages=vip_host::disconnect_read;
unsigned checks=0;
void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
namespace life_fixture {
#include "disconnect_type8_fixture.inc"
struct MessagePort:Fake {
    unsigned factories=0,fires=0,discards=0;
    bool message_alive=false,message_shown=false,message_owned=false,changed=false;
    std::string text,phase;
    bool create_message(const c::Owner&,const std::string& value,c::MessageCalls& r,std::string& e)override{
        ++factories;r.loc_construct.attempted=r.loc_construct.returned=true;r.create.attempted=true;
        if(phase=="factory"){e="original factory failed";return false;}
        r.create.returned=true;r.create.result=160;
        r.loc_arguments_destroy.attempted=r.loc_arguments_destroy.returned=r.loc_string_destroy.attempted=r.loc_string_destroy.returned=true;
        message_alive=true;text=changed?"expanded different text":value;return true;
    }
    bool inspect_message(const c::Owner&,uintptr_t p,const std::string& expected,int expected_shown,uintptr_t& selector,std::string& e)override{
        if(!message_alive||p!=160||message_shown!=bool(expected_shown)||message_owned!=bool(expected_shown)||text!=expected||phase==(expected_shown?"post":"pre")){
            e="original message ownership/final bytes differ";return false;}
        selector=expected_shown?170:0;return true;
    }
    bool fire_message(const c::Owner&,uintptr_t,c::Call& r,std::string& e)override{
        ++fires;r.attempted=true;message_shown=message_owned=true;
        if(phase=="fire"){e="original FireAndForget uncertain";return false;}r.returned=true;return true;
    }
    bool discard_message(const c::Owner&,uintptr_t,c::Call& r,std::string& e)override{
        ++discards;r.attempted=true;if(message_owned||message_shown){e="manager already owns message";return false;}
        message_alive=false;r.returned=true;return true;
    }
    bool absent(const c::Owner& own,uintptr_t alert,uintptr_t selected,std::string& e)override{
        if(alert==160){if(message_alive||message_owned||message_shown){e="unhanded message remains";return false;}return true;}
        return Fake::absent(own,alert,selected,e);
    }
};
v::Transition message(uint64_t sequence=2){auto r=retirement();r.sequence=sequence;r.message="Streamer has disconnected. Satisfaction: 4. +400 subscribers.";return r;}
void cases(){
    std::string e;const auto o=origin();const auto a=arrival();
    {MessagePort p;c::Lifecycle life;need(life.bind(o,e)&&life.apply(p,o,{a},e),e);auto r=message();
        p.subject_alive=false;need(life.apply(p,o,{r},e),e);
        need(p.hides==1&&p.deletes==1&&p.factories==1&&p.fires==1&&!p.discards&&!life.owned()&&p.message_owned,"retirement did not use original type8 cleanup then manager-owned type9 handoff");
        need(life.last().message.verified&&life.last().message.handed_off&&!life.last().message.create.result,"successful receipt retains unowned raw type9 pointer or lacks readback");
        need(life.apply(p,o,{r},e)&&p.factories==1&&p.fires==1&&!life.result().attempted,"duplicate disconnect replayed");
        need(life.cleanup(p,p.o.mode,e)&&p.deletes==1&&!p.discards&&p.message_alive,"local cleanup destroyed manager-owned type9");
        life.after_deinit(p.o.mode);need(!life.unknown(),"confirmed manager ownership was mistaken for type8 leak");}
    {MessagePort p;p.subject_alive=false;c::Lifecycle life;need(life.bind(o,e)&&life.apply(p,o,{a,message()},e),e);
        need(!p.creates&&!p.shows&&!p.hides&&!p.deletes&&p.factories==1&&p.fires==1,"transient arrival required a stale Toon or lost actual disconnect");}
    {MessagePort p;c::Lifecycle life;need(life.bind(o,e)&&life.apply(p,o,{a},e)&&life.apply(p,o,{retirement()},e),e);
        auto r=message(3);p.subject_alive=false;need(life.apply(p,o,{r},e)&&p.factories==1&&p.deletes==1,"late original EndEvent after Weak retirement missing");
        auto duplicate=r;duplicate.sequence=4;need(!life.preflight(p,o,{duplicate},e)&&p.factories==1,"second disconnect for same arrival accepted");}
    for(const auto& text:std::vector<std::string>{"Literal {name}","Braces {{value}} and $name","Streamer Grüße"}){
        MessagePort p;c::Lifecycle life;auto r=message();r.message=text;
        need(life.bind(o,e)&&life.apply(p,o,{a,r},e)&&p.text==text&&p.fires==1,"copied UTF-8/marker bytes changed");}
    for(const auto& phase:std::vector<std::string>{"factory","pre","fire","post"}){
        MessagePort p;p.phase=phase;c::Lifecycle life;need(life.bind(o,e),e);
        need(!life.apply(p,o,{a,message()},e)&&life.unknown()&&!life.last().confirmed&&!e.empty(),"failed/uncertain message was confirmed");
        need(p.discards==(phase=="pre"?1u:0u),"uncertain manager handoff was manually destroyed");
        const auto factories=p.factories,fires=p.fires;need(!life.apply(p,o,{message()},e)&&p.factories==factories&&p.fires==fires,"Unknown message replayed");}
    {MessagePort p;p.changed=true;c::Lifecycle life;need(life.bind(o,e),e);
        need(!life.apply(p,o,{a,message()},e)&&p.factories==1&&!p.fires&&p.discards==1&&life.unknown(),"LocString re-expansion reached manager handoff or leaked known unhanded allocation");}
}
}
namespace reader_fixture {
#include "disconnect_memory_fixture.inc"
void setup(Memory& m){m.setup();m.put(image+0x1749770,8,image+0x234c70);m.put(alert,0x18,int32_t(9));
    m.text(alert,0xb0,0x200000,"Final Host text {literal}");m.array(manager,0x20,0x210000,{alert});m.put(scenario,0x38,uint8_t(1));}
bool read(Memory& m,probe::Graph& g,std::string& e){probe::Reader reader(m,image,0x2000000);return reader.read_ui(mode,fixture_state,g,e);}
void cases(){Memory m;std::string e,text;uintptr_t selected=0;probe::Graph g;setup(m);need(read(m,g,e),e);
    const auto bytes=m.bytes;const std::string expected="Final Host text {literal}";
    need(messages::inspect(m,image,g,alert,1,&expected,selected,text,e)&&text==expected&&selected==selector&&m.bytes==bytes,"exact owned message reader changed data or lost bytes");
    auto before=g;before.owned={};std::optional<std::string> event;
    need(messages::ended(m,image,scenario,0,before,g,event,e)&&event==expected,"original completed transition lost new type9 message");
    need(messages::ended(m,image,scenario,1,before,g,event,e)&&!event,"retirement alone fabricated EndEvent");
    m.put(scenario,0x38,uint8_t(0));need(messages::ended(m,image,scenario,0,before,g,event,e)&&!event,"new type9 without original completion fabricated EndEvent");m.put(scenario,0x38,uint8_t(1));
    need(!messages::ended(m,image,scenario,0,g,g,event,e)&&!event,"old owned message was reused as current EndEvent");
    auto foreign=g;foreign.manager++;need(!messages::ended(m,image,scenario,0,before,foreign,event,e)&&!event,"replaced owner produced synthetic disconnect");
    for(int fault=0;fault<9;++fault){setup(m);need(read(m,g,e),e);
        switch(fault){case 0:g.owned.items.clear();break;case 1:g.active.items.clear();break;case 2:g.selectors.items.clear();break;
        case 3:g.owned.items.push_back(alert);break;case 4:m.put(selector,0x330,alert+8);break;case 5:m.put(alert,0x18,int32_t(8));break;
        case 6:m.put(image+0x1749770,8,image+0x234c60);break;case 7:m.text(alert,0xb0,0x200000,"Other text");break;case 8:m.drift=alert+0xb8;break;}
        need(!messages::inspect(m,image,g,alert,1,&expected,selected,text,e)&&!e.empty(),"message byte/lifetime/owner corruption accepted");}
    setup(m);m.put(alert,0x130,uintptr_t(0));m.put(alert,0x138,uint8_t(0));
    m.array(manager,0x20,0,{});m.array(manager,0x68,0,{});m.array(list,0x320,0,{});m.array(list,0x300,0,{});
    need(read(m,g,e)&&messages::inspect(m,image,g,alert,0,&expected,selected,text,e)&&!selected,"factory output unowned readback failed");
}
}
namespace abi_fixture {
unsigned constructors=0,factories=0,arguments=0,strings=0;uintptr_t storage=0;int fault=0;
void construct(void* p,const char* text){++constructors;storage=reinterpret_cast<uintptr_t>(p);need(storage%8==0&&std::string(text)=="Final text","LocString constructor ABI/text differs");if(fault==1)RaiseException(0xe0123456,0,0,nullptr);}
uintptr_t factory(int32_t type,const void* p){++factories;need(type==9&&reinterpret_cast<uintptr_t>(p)==storage,"CreateMessageAlert ABI differs");if(fault==2)RaiseException(0xe0123456,0,0,nullptr);return 160;}
void args(void* p){++arguments;need(reinterpret_cast<uintptr_t>(p)==storage+0x20,"LocString args destructor offset differs");if(fault==3)RaiseException(0xe0123456,0,0,nullptr);}
void string(void* p){++strings;need(reinterpret_cast<uintptr_t>(p)==storage,"LocString string destructor receiver differs");if(fault==4)RaiseException(0xe0123456,0,0,nullptr);}
void cases(){for(fault=0;fault<=4;++fault){constructors=factories=arguments=strings=0;c::MessageCalls r;
    c::message_invoke::create({construct,factory,args,string},"Final text",r);
    need(constructors==1&&factories==(fault==1?0u:1u)&&arguments==(fault==1?0u:1u)&&strings==(fault==1?0u:1u),"native LocString lifecycle duplicated or skipped cleanup");
    need(r.loc_construct.returned==(fault!=1)&&r.create.attempted==(fault!=1)&&r.create.returned==(fault!=1&&fault!=2)&&
        r.loc_arguments_destroy.returned==(fault!=1&&fault!=3)&&r.loc_string_destroy.returned==(fault!=1&&fault!=4),"native original call/exception receipt differs");
    if(fault)need((fault==1?r.loc_construct.exception:fault==2?r.create.exception:fault==3?r.loc_arguments_destroy.exception:r.loc_string_destroy.exception)==0xe0123456,"original native exception cause lost");}}
}
namespace portable_fixture {
namespace economy=mt2mp::game::live_host_state::day::economy;
void cases(){std::string e;auto r=life_fixture::message();auto o=life_fixture::origin();
    v::HostRetention host;v::ClientRetention client;need(host.bind(o,2,e)&&client.bind(o,e),e);uint64_t sequence=0;
    need(host.append(o,151,"Streamer",true,0,sequence,e)&&host.append(o,151,"Streamer",false,1,sequence,e),e);
    auto first=host.pending();std::vector<v::Transition> work;need(client.prepare(o,first,work,e)&&client.confirm(o,first,work,e)&&host.sent(o,1,first,e)&&host.acknowledge(o,2,1,e),e);
    need(host.append(o,151,"Streamer",false,1,sequence,e,r.message)&&sequence==3,"late disconnect lost acknowledged retirement identity");
    auto later=host.pending();need(later.after==2&&later.rows.size()==1&&later.rows[0].message==r.message,"late message wire prefix wrong");
    need(client.prepare(o,later,work,e)&&client.confirm(o,later,work,e)&&client.prepare(o,later,work,e)&&work.empty(),"late message confirmed twice");
    need(!host.append(o,151,"Streamer",false,1,sequence,e,r.message),"same arrival accepted two disconnects");
    vip_host::HostTransitions observed;vip_host::HostSample sample{100,200,300,400,true,151,"Streamer"};
    need(observed.bind(o,2,e)&&observed.observe(o,sample,e),e);vip_host::HostSample gone;gone.scenario=100;
    need(observed.observe(o,gone,e)&&observed.observe(o,gone,e,r.message),e);v::Batch batch;need(observed.capture(o,batch,e)&&batch.rows.size()==3&&batch.rows.back().message==r.message,"Host copied retirement identity/message missing");
    {vip_host::HostTransitions removed;need(removed.bind(o,2,e)&&removed.observe(o,sample,e)&&removed.observe(o,gone,e),e);
        removed.forget_removed({100});need(removed.known(100).has_value(),"current retired ScenarioLog receiver lost late EndEvent identity");
        removed.forget_removed({});need(!removed.known(100)&&!removed.observe(o,gone,e,r.message),"removed receiver retained stale arrival identity");}
    economy::State state;auto& f=state.day.frame;f.session=o.session;f.epoch=o.epoch;f.snapshot=o.snapshot;f.source_generation=o.generation;f.revision=1;
    f.selected=f.pause=1;f.values.calendar={1,1,2,1,30,7};f.values.cash=93640;f.values.record={1,93640,0,0,0};f.vips=batch;
    state.day.history={{1,mt2mp::game::live_host_state::day::record(f.values)}};
    std::vector<uint8_t> encoded;need(economy::encode(state,encoded,e),e);auto j=json::parse(encoded);need(j.at("day").at("frame").at("schema")==14,"message did not require schema14");
    economy::State decoded;need(economy::decode(encoded.data(),encoded.size(),decoded,e)&&decoded==state,"actual Economy/Frame message roundtrip lost payload");
    auto downgraded=j;downgraded["day"]["frame"]["schema"]=13;const auto text=downgraded.dump();need(!economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"schema13 accepted disconnect payload");
    for(int fault=0;fault<4;++fault){auto invalid=r;switch(fault){case 0:invalid.active=true;break;case 1:invalid.message="";break;case 2:invalid.message=std::string("a\0b",3);break;case 3:invalid.message=std::string("\xff",1);break;}need(!v::valid({1,{invalid}},e),"unsafe/active disconnect text accepted");}
}
}
int main(){try{life_fixture::cases();reader_fixture::cases();abi_fixture::cases();portable_fixture::cases();std::cout<<checks<<" VIP disconnect message checks passed\n";return 0;}catch(const std::exception& x){std::cerr<<"error after "<<checks<<" checks: "<<x.what()<<'\n';return 1;}}
