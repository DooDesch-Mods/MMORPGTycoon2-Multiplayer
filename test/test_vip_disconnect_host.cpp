#include "disconnect_host_fixture.inc"
namespace disconnect_fixture {
constexpr uintptr_t message_alert=0x210000,message_selector=0x220000,message_text=0x230000;
std::string text="Streamer Name disconnected: final Host bytes {literal}";
enum class End {Message,RetirementOnly,MissingMessage,WorldLoss};
End effect=End::Message;
void original(uintptr_t,float){
    ++fixture::originals;auto& memory=fixture::memory;
    memory.put(scenario,0xa0,uintptr_t(0));
    memory.array(manager,0x68,0,{});memory.array(list,0x320,0,{});memory.array(list,0x300,0,{});
    if(effect==End::WorldLoss){fixture::world.world=reinterpret_cast<void*>(0x999);return;}
    if(effect==End::RetirementOnly)return;
    memory.put(scenario,0x38,uint8_t(1));if(effect==End::MissingMessage)return;
    memory.type(message_alert,0x1749770);memory.put(image+0x1749770,8,image+0x234c70);
    memory.put(message_alert,0x18,int32_t(9));memory.put(message_alert,0x130,message_selector);memory.put(message_alert,0x138,uint8_t(1));
    memory.text(message_alert,0xb0,message_text,text);memory.type(message_selector,0x1704fa0);
    memory.put(message_selector,0x318,list);memory.put(message_selector,0x330,message_alert);
    memory.put(message_selector,0x278,list);memory.put(message_selector,0x280,window);
    memory.array(manager,0x20,0x240000,{message_alert});memory.array(manager,0x68,0x250000,{message_alert});
    memory.array(list,0x320,0x260000,{message_selector});memory.array(list,0x300,0x270000,{message_selector});
}
void setup(std::string& e){fixture::setup();fixture::bind(e);}
void update(){vip::host_update(scenario,0.25f,image+0x4f43f5,original);}
}
int main(){try{
    namespace d=disconnect_fixture;std::string e;vip::wire::Batch batch;
    d::setup(e);d::effect=d::End::Message;d::update();
    need(vip::host_capture(fixture::origin(),batch,e),e.c_str());
    need(fixture::originals==1&&batch.rows.size()==2&&batch.rows.back().message==d::text&&batch.rows.back().arrival==1&&batch.rows.back().subscriber==151,"exact original EndEvent notice did not retain arrival identity/final native bytes");
    need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==2&&fixture::originals==1,"receipt observation invoked original Update or appended notice twice");

    d::setup(e);d::effect=d::End::RetirementOnly;d::update();
    need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==2&&!batch.rows.back().message,"cleanup retirement fabricated EndEvent text");
    // The original later EndEvent still has the scenario and copied arrival
    // identity, although its Toon Weak and type8 UI may already be gone.
    d::effect=d::End::Message;d::update();
    need(vip::host_capture(fixture::origin(),batch,e)&&batch.rows.size()==3&&batch.rows.back().message==d::text&&batch.rows.back().arrival==1&&fixture::originals==2,"late original EndEvent after type8 retirement lost its once-only notice");

    d::setup(e);d::effect=d::End::MissingMessage;d::update();
    need(!vip::host_capture(fixture::origin(),batch,e)&&e.find("exactly one new original owned type9")!=std::string::npos&&fixture::originals==1,"completed flag alone confirmed unobserved message effect");
    d::setup(e);d::effect=d::End::WorldLoss;d::update();
    need(!vip::host_capture(fixture::origin(),batch,e)&&fixture::originals==1,"world loss produced synthetic message or replayed original Update");
    std::cout<<checks<<" original Host disconnect witness checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"error: "<<x.what()<<'\n';return 1;}}
