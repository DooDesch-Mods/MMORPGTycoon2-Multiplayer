#include "scenario_vip_authority/host_witness.cpp"
#define main retained_alert_probe_main
#include "test_vip_alert_probe.cpp"
#undef main
#include <bit>
namespace vip=mt2mp::game::scenario_vip_authority;
namespace action=mt2mp::action_session;
namespace fixture {
Memory memory;
constexpr uintptr_t proxy=0x190000;
action::Context context;
action::Binding binding;
mt2mp::game::building_mixed_identity::opened_file::CurrentWorld world;
uint64_t epoch=7;uint32_t thread=0;
std::string pin_error;
unsigned originals=0;uintptr_t receiver_seen=0;uint32_t dt_seen=0;
enum class Effect {None,Show,Hide,Remove,Throw,Reenter};
Effect effect=Effect::None;
struct Session:action::Session {
    bool current(action::Context& out,std::string& e)override{
        if(!pin_error.empty()){e=pin_error;return false;}out=context;e.clear();return true;
    }
} session;
void setup(){
    memory.setup();memory.put(image+0x1730e90,0xa0,image+0x4f85b0);
    memory.put(scenario,0x38,uint8_t(0));
    memory.put(alert,0x70,proxy);memory.put(proxy,0,toon+0x20);memory.put(proxy,8,int32_t(1));memory.put(toon,0x28,proxy);
    memory.put(image,0x3c,int32_t(0x100));memory.put(image,0x150,uint32_t(0x2000000));
    context={1,1,1,3,action::Role::Host,"12345678-1234-1234-1234-123456789abc"};
    binding={11,13};epoch=7;thread=GetCurrentThreadId();pin_error.clear();originals=0;effect=Effect::None;
    world={};world.game=reinterpret_cast<void*>(0x111);world.mode=reinterpret_cast<void*>(mode);
    world.state=reinterpret_cast<void*>(fixture_state);world.world=reinterpret_cast<void*>(0x222);world.map=reinterpret_cast<void*>(0x333);
    world.world_epoch=binding.native_epoch;world.generation=binding.native_generation;world.bytes=42;
    world.uuid="source-world";world.sha256="source-sha";
    vip::producer={};vip::wrong_thread=false;vip::observation_thread=0;
}
vip::wire::Origin origin(){return {context.session,epoch,binding.native_generation,context.snapshot_uuid};}
void original(uintptr_t receiver,float dt){
    ++originals;receiver_seen=receiver;dt_seen=std::bit_cast<uint32_t>(dt);
    switch(effect){
    case Effect::None:break;
    case Effect::Show:
        memory.put(alert,0x138,uint8_t(1));memory.put(alert,0x130,selector);
        memory.array(manager,0x68,0x140000,{alert});memory.array(list,0x320,0x120000,{selector});memory.array(list,0x300,0x130000,{selector});break;
    case Effect::Hide:
        memory.put(alert,0x138,uint8_t(0));memory.put(alert,0x130,uintptr_t(0));
        memory.array(manager,0x68,0,{});memory.array(list,0x320,0,{});memory.array(list,0x300,0,{});
        memory.unreadable=toon;break;
    case Effect::Remove:memory.array(log_owner,0x20,0,{});memory.unreadable=scenario;break;
    case Effect::Throw:throw std::runtime_error("original Update exception");
    case Effect::Reenter:effect=Effect::None;vip::host_update(receiver,dt,image+0x4f43f5,original);break;
    }
}
void update(){vip::host_update(scenario,std::bit_cast<float>(uint32_t(0x3eaaaaab)),image+0x4f43f5,original);}
void bind(std::string& e){need(vip::host_bind(context,binding,epoch,e),e.c_str());}
}
namespace placement_ui_probe {
bool ProcessMemory::copy(uintptr_t p,void* out,size_t size,std::string& e){return fixture::memory.copy(p,out,size,e);}
}
namespace mt2mp::game::native {
bool read_resolver_receipt(uintptr_t& base,uint32_t& thread,std::string& sha,std::string& e){
    base=image;thread=fixture::thread;sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";e.clear();return true;
}
}
namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string& e){out=fixture::world;e.clear();return true;}
}
namespace mt2mp::action_session {Session& selected_session(){return fixture::session;}}
namespace mt2mp::net {uint64_t current_world_epoch(){return fixture::epoch;}}
int main(){try{
    std::string e;vip::wire::Batch batch;vip::HostSample sample;std::vector<uintptr_t> owners;
    fixture::setup();need(vip::host_read::scenarios(fixture::memory,image,fixture_state,owners,e)&&owners==std::vector<uintptr_t>{scenario},"current ScenarioLog membership");
    need(vip::host_read::sample(fixture::memory,image,scenario,sample,e)&&sample.active&&sample.subscriber==151&&sample.name=="Streamer Name","actual selected shown Weak/Subscriber/name read");
    const auto active=sample;
    fixture::memory.unreadable=log_owner;need(!vip::host_read::scenarios(fixture::memory,image,fixture_state,owners,e)&&e=="original fixture unreadable","ScenarioLog native read cause retained");
    for(unsigned fault=0;fault<12;++fault){fixture::setup();
        switch(fault){
        case 0:fixture::memory.put(scenario,0x98,uint8_t(2));break;
        case 1:fixture::memory.put(alert,0x138,uint8_t(2));break;
        case 2:fixture::memory.put(fixture::proxy,0,uintptr_t(toon+0x21));break;
        case 3:fixture::memory.put(fixture::proxy,8,int32_t(0));break;
        case 4:fixture::memory.put(toon,0x28,uintptr_t(fixture::proxy+8));break;
        case 5:fixture::memory.put(toon,0x940,uintptr_t(subscriber+8));break;
        case 6:fixture::memory.put(subscriber,0x80,uintptr_t(toon+8));break;
        case 7:fixture::memory.put(subscriber,0x20,int32_t(-1));break;
        case 8:fixture::memory.put(alert,0xf0,int32_t(1));break;
        case 9:fixture::memory.text(alert,0x88,0x160000,std::string("a\0b",3));break;
        case 10:fixture::memory.unreadable=alert+0x88;break;
        case 11:fixture::memory.put(image+0x1730e90,0xa0,image+1);break;
        }
        need(!vip::host_read::sample(fixture::memory,image,scenario,sample,e)&&!sample.active&&!e.empty(),"invalid current arrival fails with cause");
        if(fault==10)need(e=="original fixture unreadable","selected native copy cause retained");
    }
    for(unsigned absence=0;absence<5;++absence){fixture::setup();fixture::memory.unreadable=toon;
        switch(absence){
        case 0:fixture::memory.put(alert,0x138,uint8_t(0));break;
        case 1:fixture::memory.put(alert,0x130,uintptr_t(0));break;
        case 2:fixture::memory.put(alert,0x68,uintptr_t(0));break;
        case 3:fixture::memory.put(alert,0x70,uintptr_t(0));break;
        case 4:fixture::memory.put(fixture::proxy,0,uintptr_t(0));break;
        }
        need(vip::host_read::sample(fixture::memory,image,scenario,sample,e)&&!sample.active,"retirement does not touch expired Toon");
    }
    fixture::setup();fixture::bind(e);const auto origin=fixture::origin();
    need(vip::host_capture(origin,batch,e)&&batch.rows.size()==1&&batch.rows[0].active&&batch.rows[0].sequence==1,"bind seeds one existing shown arrival");
    need(vip::host_bind(fixture::context,fixture::binding,fixture::epoch,e)&&vip::host_capture(origin,batch,e)&&batch.rows.size()==1,"same binding does not reseed");
    fixture::effect=fixture::Effect::Hide;fixture::update();
    need(fixture::originals==1&&fixture::receiver_seen==scenario&&fixture::dt_seen==0x3eaaaaab,"witness original once exact arguments");
    need(vip::host_capture(origin,batch,e)&&batch.rows.size()==2&&!batch.rows[1].active&&batch.rows[1].arrival==1&&batch.rows[1].name=="Streamer Name"&&batch.rows[1].subscriber==151,"hidden arrival retires copied payload while pointer remains");
    {vip::alert_probe::Reader reader(fixture::memory,image,0x2000000);vip::alert_probe::Graph graph;
        need(reader.read_ui(mode,fixture_state,graph,e)&&graph.present&&graph.selectors.items.empty()&&graph.active.items.empty(),"original Hide fixture leaves a valid empty UI graph without reading expired Toon");}
    need(vip::host_sent(origin,1,batch,e)&&vip::host_acknowledge(origin,2,1,e)&&vip::host_capture(origin,batch,e)&&batch.rows.empty()&&batch.after==2,"only successful sent/authenticated peer coverage drains transient pair");
    fixture::memory.unreadable=0;fixture::effect=fixture::Effect::Show;fixture::update();
    need(vip::host_capture(origin,batch,e)&&batch.rows.size()==1&&batch.rows[0].sequence==3&&batch.rows[0].arrival==3,"same Subscriber/native pointers may arrive again after retirement");
    need(!vip::host_acknowledge(origin,3,1,e),"non-required peer cannot acknowledge");
    need(!vip::host_reset(origin,e),"reset cannot discard active witness");
    need(vip::host_suspend(origin,e)&&!vip::host_capture(origin,batch,e)&&!vip::host_sent(origin,2,batch,e),"suspend disarms publication and retains origin");
    need(vip::host_reset(origin,e)&&!vip::host_bind(fixture::context,fixture::binding,fixture::epoch,e),"same Origin cannot restart after reset");
    ++fixture::context.session;fixture::bind(e);need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==1&&batch.rows[0].sequence==1,"new session seed excludes old transition history");
    fixture::setup();fixture::memory.put(alert,0x138,uint8_t(0));fixture::bind(e);
    fixture::effect=fixture::Effect::Show;fixture::update();fixture::effect=fixture::Effect::Hide;fixture::update();
    need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==2&&batch.rows[0].active&&!batch.rows[1].active,"arrival and retirement between captures both retained");
    fixture::setup();fixture::bind(e);fixture::effect=fixture::Effect::Remove;fixture::update();
    need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==2&&!batch.rows[1].active,"removed receiver retires without dereferencing freed Scenario");
    fixture::setup();fixture::bind(e);fixture::memory.array(log_owner,0x20,0,{});fixture::memory.unreadable=scenario;
    need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==2,"capture reconciles removal outside Update");
    fixture::setup();fixture::bind(e);fixture::memory.text(alert,0x88,0x160000,"Changed Name");fixture::update();
    need(fixture::originals==1&&!vip::host_capture(fixture::origin(),batch,e)&&e.find("immutable identity/name")!=std::string::npos,"active payload mutation latches error but original still executes");
    need(!vip::host_suspend(fixture::origin(),e)&&e.find("immutable identity/name")!=std::string::npos&&!vip::producer.transitions.armed()&&vip::observation_thread==0,"suspend disarms failed observer and reports retained cause");
    need(vip::host_reset(fixture::origin(),e),"failed observer can reset after suspension without native reads or effects");
    fixture::setup();fixture::bind(e);fixture::pin_error="original Session error";fixture::update();
    need(fixture::originals==1&&!vip::host_capture(fixture::origin(),batch,e)&&e=="original Session error","original pin failure remains visible without suppressing game Update");
    fixture::setup();fixture::bind(e);fixture::world.generation+=1;fixture::update();
    need(fixture::originals==1&&!vip::host_capture(fixture::origin(),batch,e)&&e.find("binding differs")!=std::string::npos,"fresh World binding rejects drift while original executes");
    fixture::setup();fixture::bind(e);vip::host_update(scenario,0,image+0x4f43f6,fixture::original);
    need(fixture::originals==1&&!vip::host_capture(fixture::origin(),batch,e)&&e.find("caller differs")!=std::string::npos,"wrong original caller cannot produce an event");
    fixture::setup();fixture::bind(e);vip::observation_thread=GetCurrentThreadId()+1;fixture::update();
    need(fixture::originals==1&&!vip::host_capture(fixture::origin(),batch,e)&&e.find("off bound Mainthread")!=std::string::npos,"wrong-thread call only forwards and latches failure for Mainthread");
    fixture::setup();fixture::bind(e);fixture::effect=fixture::Effect::Throw;bool thrown=false;
    try{fixture::update();}catch(const std::runtime_error& x){thrown=std::string(x.what())=="original Update exception";}
    need(thrown&&fixture::originals==1&&!vip::host_capture(fixture::origin(),batch,e)&&e.find("original Update threw")!=std::string::npos&&!vip::producer.updating,"original exception propagates once and poisons unconfirmed observation");
    fixture::setup();fixture::bind(e);fixture::effect=fixture::Effect::Reenter;fixture::update();
    need(fixture::originals==2&&!vip::host_capture(fixture::origin(),batch,e)&&e.find("reentrant")!=std::string::npos,"reentrant originals both execute but no event success claimed");
    fixture::setup();fixture::update();need(fixture::originals==1&&!vip::producer.transitions.bound(),"unarmed offline witness only forwards original");
    fixture::setup();fixture::memory.array(list,0x300,0,{});
    need(!vip::host_bind(fixture::context,fixture::binding,fixture::epoch,e)&&!vip::producer.transitions.bound()&&!e.empty(),"seed rejects incomplete full UI graph transactionally");
    vip::HostTransitions transitions;need(transitions.bind(origin,6,e)&&transitions.observe(origin,active,e),"portable witness arrival");
    need(transitions.capture(origin,batch,e)&&transitions.sent(origin,1,batch,e)&&transitions.acknowledge(origin,2,1,e)&&transitions.capture(origin,batch,e)&&batch.rows.size()==1,"one ACK cannot discard another required peer receipt");
    need(transitions.acknowledge(origin,3,1,e)&&transitions.capture(origin,batch,e)&&batch.rows.empty(),"all required peers release prefix");
    auto reused=active;reused.alert+=8;reused.toon+=8;reused.proxy+=8;
    need(transitions.observe(origin,reused,e)&&transitions.capture(origin,batch,e)&&batch.rows.size()==2&&!batch.rows[0].active&&batch.rows[1].active&&batch.rows[1].arrival==3,"replacement retires old arrival before reused Subscriber arrival");
    std::cout<<checks<<" actual Host VIP witness checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<'\n';return 1;}}
