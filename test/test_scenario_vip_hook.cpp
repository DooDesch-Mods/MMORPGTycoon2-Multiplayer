#include "scenario_vip_authority/native.cpp"
#include <bit>
#include <iostream>
#include <map>
#include <stdexcept>
namespace vip=mt2mp::game::scenario_vip_authority;
namespace fixture {
std::map<uintptr_t,std::vector<unsigned char>> memory;
uintptr_t unreadable=0;
unsigned originals=0,pins=0,publishes=0,creates=0,enables=0;
uintptr_t original_receiver=0;float original_dt=0;
bool active=true,owned=true,reject=false,throw_owner=false,throw_original=false;
vip::Receipt last;
std::vector<int> order;uintptr_t caller_seen=0;
void host(uintptr_t receiver,float dt,uintptr_t caller,vip::HostUpdate update){
    order.push_back(1);caller_seen=caller;update(receiver,dt);order.push_back(2);
}
void original(uintptr_t receiver,float dt){++originals;original_receiver=receiver;original_dt=dt;if(throw_original)throw std::runtime_error("original exception unchanged");}
bool client_active(){return active;}
bool owner(uintptr_t receiver,vip::Receipt& r,std::string& e){++pins;r.receiver=receiver;r.owned=owned;
    if(throw_owner)throw std::runtime_error("actual owner exception");
    if(reject){e="actual owner rejected";return false;}return true;}
void publish(const vip::Receipt& r){++publishes;last=r;}
}
namespace placement_ui_probe {
bool ProcessMemory::copy(uintptr_t address,void* out,size_t size,std::string& error){
    if(address==fixture::unreadable){error="actual hook read failure";return false;}
    const auto at=fixture::memory.find(address);
    if(at==fixture::memory.end()||size>at->second.size()){error="fixture address not supplied";return false;}
    std::memcpy(out,at->second.data(),size);return true;
}
}
MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){++fixture::creates;return MH_ERROR_UNSUPPORTED_FUNCTION;}
MH_STATUS WINAPI MH_EnableHook(LPVOID){++fixture::enables;return MH_ERROR_NOT_CREATED;}
namespace {
unsigned checks=0;
void need(bool okay,const char* cause){++checks;if(!okay)throw std::runtime_error(cause);}
void reset(){
    fixture::memory.clear();fixture::unreadable=0;fixture::originals=fixture::pins=fixture::publishes=fixture::creates=fixture::enables=0;
    fixture::active=fixture::owned=true;fixture::reject=fixture::throw_owner=fixture::throw_original=false;fixture::last={};
    vip::image=0x140000000;vip::ready=true;vip::original_update=fixture::original;
    vip::callbacks={fixture::client_active,fixture::owner,fixture::publish};
    vip::installed={};vip::installed[0]=0xe9;int32_t displacement=0x100;
    std::memcpy(vip::installed.data()+1,&displacement,sizeof displacement);
    vip::relocated.fill(0x42);
    fixture::memory[vip::image+0x4f85b0]={vip::installed.begin(),vip::installed.end()};
    fixture::memory[uintptr_t(vip::original_update)]={vip::relocated.begin(),vip::relocated.end()};
    auto& relay=fixture::memory[vip::image+0x4f85b0+5+0x100];relay.resize(14);relay[0]=0xff;relay[1]=0x25;
    const auto target=uintptr_t(&vip::update);std::memcpy(relay.data()+6,&target,sizeof target);
}
void dispatch(){vip::dispatch(0xabc123,std::bit_cast<float>(uint32_t(0x3eaaaaab)),vip::image+0x4f43f5);}
void suppressed(){need(!fixture::originals&&fixture::publishes==1&&fixture::last.owned&&fixture::last.suppressed&&fixture::last.cause.empty(),"owned Update not suppressed exactly");}
void failed(const char* cause){need(!fixture::originals&&fixture::publishes==1&&fixture::last.owned&&!fixture::last.suppressed&&fixture::last.cause.find(cause)!=std::string::npos,cause);}
}
int main(){try{
    reset();fixture::active=false;dispatch();
    need(fixture::originals==1&&!fixture::pins&&!fixture::publishes,"Host/offline original not exactly once");
    need(fixture::original_receiver==0xabc123&&std::bit_cast<uint32_t>(fixture::original_dt)==0x3eaaaaab,"original arguments changed");
    reset();fixture::owned=false;dispatch();need(fixture::originals==1&&fixture::pins==1&&!fixture::publishes,"retired owner not forwarded exactly once");
    reset();dispatch();suppressed();need(fixture::pins==1&&fixture::last.receiver==0xabc123,"actual owner callback omitted");
    reset();fixture::reject=true;dispatch();failed("actual owner rejected");
    reset();fixture::throw_owner=true;dispatch();failed("actual owner exception");
    reset();vip::dispatch(1,0,vip::image+0x4f43f6);failed("different original ScenarioLog caller");
    reset();vip::ready=false;dispatch();failed("not ready");
    reset();fixture::memory[vip::image+0x4f85b0][7]^=1;dispatch();failed("entry/trampoline changed");
    reset();fixture::memory[uintptr_t(vip::original_update)][8]^=1;dispatch();failed("entry/trampoline changed");
    reset();fixture::memory[vip::image+0x4f85b0+5+0x100][6]^=1;dispatch();failed("relay target differs");
    reset();fixture::unreadable=vip::image+0x4f85b0;dispatch();failed("actual hook read failure");
    reset();fixture::unreadable=uintptr_t(vip::original_update);dispatch();failed("actual hook read failure");
    reset();fixture::unreadable=vip::image+0x4f85b0+5+0x100;dispatch();failed("actual hook read failure");
    reset();fixture::active=false;fixture::throw_original=true;bool caught=false;
    try{dispatch();}catch(const std::runtime_error& e){caught=std::string(e.what())=="original exception unchanged";}
    need(caught&&fixture::originals==1&&!fixture::publishes,"Host original exception changed or replayed");
    reset();std::string error;need(!vip::initialize(vip::image,vip::callbacks,error)&&error=="VIP installation already attempted"&&!fixture::creates,"installation replay admitted");
    reset();vip::image=0;need(!vip::initialize(0,vip::callbacks,error)&&!fixture::creates,"empty image installed");
    reset();vip::image=0;fixture::memory[0x140000000+0x4f4360].resize(1600);
    need(!vip::initialize(0x140000000,vip::callbacks,error)&&error.find("original owner body differs")!=std::string::npos&&!fixture::creates&&!fixture::enables,"wrong shipped owner body enabled hook");
    reset();fixture::active=false;dispatch();fixture::active=true;dispatch();
    need(fixture::originals==1&&fixture::last.suppressed,"Host-to-Client ownership switch cached old decision");
    reset();fixture::order.clear();fixture::active=false;vip::callbacks.host_update=fixture::host;dispatch();
    need(fixture::order==std::vector<int>({1,2})&&fixture::originals==1&&!fixture::pins&&!fixture::publishes,"Host callback surrounds one original call");
    need(fixture::caller_seen==vip::image+0x4f43f5&&fixture::original_receiver==0xabc123&&std::bit_cast<uint32_t>(fixture::original_dt)==0x3eaaaaab,"Host callback preserves caller and exact original arguments");
    reset();fixture::order.clear();vip::callbacks.host_update=fixture::host;dispatch();suppressed();
    need(fixture::order.empty(),"Host callback never runs for owned Client");
    reset();fixture::order.clear();fixture::owned=false;vip::callbacks.host_update=fixture::host;dispatch();
    need(fixture::order.empty()&&fixture::originals==1,"foreign Client original path stays unchanged");
    reset();fixture::order.clear();fixture::active=false;fixture::throw_original=true;vip::callbacks.host_update=fixture::host;caught=false;
    try{dispatch();}catch(const std::runtime_error& e){caught=std::string(e.what())=="original exception unchanged";}
    need(caught&&fixture::order==std::vector<int>({1})&&fixture::originals==1,"Host callback preserves original exception without post/replay");
    std::cout<<checks<<" actual VIP hook dispatch/guard checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
