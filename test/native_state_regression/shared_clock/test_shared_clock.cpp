#include "release_ui_owner.h"
#include "mt2mp/core/hash.h"
#include <native_routes.h>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <map>
#include <stdexcept>

unsigned checks=0,receipt_reads=0;
uint32_t fixture_thread=71;
constexpr uintptr_t fixture_image=0x140000000,clock_rva=0x259d70,cycle_rva=0x400000,edit_rva=0x410000;
std::map<uintptr_t,std::vector<unsigned char>> memory_bytes;
std::string fixture_clock_sha,fixture_cycle_sha,fixture_edit_sha;
bool receipt_available=true;
MT2FW_MT2_RouteReceipt observed{};
void require(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
uint32_t fixture_thread_id(){return fixture_thread;}
#define GetCurrentThreadId fixture_thread_id
struct Memory {
    bool copy(uintptr_t address,void* data,size_t count,std::string& error){
        const auto found=memory_bytes.find(address);
        if(found==memory_bytes.end()||count>found->second.size()){error="fixture native memory unavailable";return false;}
        std::memcpy(data,found->second.data(),count);return true;
    }
};
namespace mt2mp::hooks::framework {
struct Backend {
    bool read_route(void* entry,MT2FW_MT2_RouteReceipt& out){++receipt_reads;if(!receipt_available||uintptr_t(entry)!=observed.entry)return false;out=observed;return true;}
    bool verify_code(uintptr_t,const void*,size_t,std::string& error){error="unexpected fixture Release-owned route";return false;}
    std::string error()const{return "original Framework Clock receipt unavailable";}
};
Backend& process_backend(){static Backend backend;return backend;}
}
namespace mt2mp::control_input::runtime {
struct Route {uintptr_t rva;size_t bytes,stolen;const char* sha;void* original=nullptr;game::release_ui_owner::Owner owner;};
struct Runtime {uint32_t thread=71;uintptr_t image=::fixture_image;bool ready=true;Memory memory;std::array<Route,9> routes{};};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "control_clock_extracted.inc"
}
namespace mt2mp::game::release_runtime {
struct Runtime {uintptr_t image=::fixture_image;Memory memory;std::vector<control_input::runtime::Route> routes;release_ui_owner::Owner postresolve_owner;};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "release_clock_extracted.inc"
}
namespace control=mt2mp::control_input::runtime;
namespace release=mt2mp::game::release_runtime;
control::Runtime fixture_control;
release::Runtime fixture_release;
void setup(bool hooked=true,bool publish=true){
    memory_bytes.clear();fixture_thread=71;receipt_available=true;receipt_reads=0;fixture_control={};fixture_release={};
    auto& route=fixture_control.routes[6];route.rva=clock_rva;route.bytes=144;route.stolen=6;
    auto& owner=route.owner;owner.entry=fixture_image+clock_rva;owner.trampoline=fixture_image+0x800000;owner.detour=fixture_image+0x900000;owner.generation=9;owner.stolen=6;
    owner.pristine.resize(144,0x90);for(size_t i=0;i<owner.stolen;++i)owner.pristine[i]=uint8_t(i+10);
    fixture_clock_sha=mt2mp::sha256(owner.pristine);route.sha=fixture_clock_sha.c_str();memory_bytes[owner.entry]=owner.pristine;
    std::vector<unsigned char> trampoline(owner.stolen+28);std::copy_n(owner.pristine.begin(),owner.stolen,trampoline.begin());
    constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};std::memcpy(trampoline.data()+owner.stolen,jump,6);
    const uintptr_t continuation=owner.entry+owner.stolen;std::memcpy(trampoline.data()+owner.stolen+6,&continuation,8);
    std::memcpy(trampoline.data()+owner.stolen+14,jump,6);std::memcpy(trampoline.data()+owner.stolen+20,&owner.detour,8);
    memory_bytes[owner.trampoline]=trampoline;
    if(hooked){auto& body=memory_bytes[owner.entry];body[0]=0xe9;const int32_t delta=int32_t(owner.trampoline+owner.stolen+14-owner.entry-5);std::memcpy(body.data()+1,&delta,4);}
    memory_bytes[fixture_image+cycle_rva]=std::vector<unsigned char>(64,0xcc);fixture_cycle_sha=mt2mp::sha256(memory_bytes[fixture_image+cycle_rva]);
    memory_bytes[fixture_image+edit_rva]=std::vector<unsigned char>(32,0x91);fixture_edit_sha=mt2mp::sha256(memory_bytes[fixture_image+edit_rva]);
    observed={};observed.size=sizeof observed;observed.state=MT2FW_MT2_ROUTE_ENABLED;observed.entry=owner.entry;observed.rva=clock_rva;
    observed.generation=owner.generation;observed.trampoline=owner.trampoline;observed.detour=owner.detour;
    std::memcpy(observed.owner,"doodesch.multiplayer",20);control::published.store(publish?&fixture_control:nullptr);
}
bool verifies(std::string& error){return release::code(fixture_release,error);}
int main()try{
    std::string error;
    setup(false,false);require(verifies(error)&&receipt_reads==0,"Release before Control install keeps pristine Clock pin: "+error);
    memory_bytes[fixture_image+clock_rva][140]^=1;require(!verifies(error),"full 144-byte pristine Clock extent remains enforced");
    setup(true,false);require(!verifies(error)&&error=="Release cycle original helper body differs RVA=2465136","patched Clock without known owner cannot pass Release cycle");
    setup();require(verifies(error)&&receipt_reads==1,"Release cycle delegates exact route6 with six-byte original trampoline: "+error);
    setup();fixture_control.ready=false;require(!verifies(error),"incomplete published Clock owner rejected");
    setup();fixture_thread=72;require(!verifies(error),"wrong Clock owner thread rejected");
    setup();fixture_control.image+=1;require(!verifies(error),"wrong Clock owner image rejected");
    setup();fixture_control.routes[6].rva=0x207820;require(!verifies(error),"ItemActivate contract cannot substitute Clock route6");
    setup();fixture_control.routes[6].bytes=128;require(!verifies(error),"ItemActivate body extent cannot truncate Clock pin");
    setup();fixture_control.routes[6].sha="other fingerprint";require(!verifies(error),"Clock original fingerprint mismatch rejected");
    setup();fixture_control.routes[6].owner.entry+=1;require(!verifies(error),"Clock physical entry mismatch rejected");
    setup();receipt_available=false;require(!verifies(error)&&error.find("original Framework Clock receipt unavailable")!=std::string::npos,"original Clock receipt failure retained");
    setup();observed.owner[0]='x';require(!verifies(error),"foreign Clock owner rejected");
    setup();observed.state=MT2FW_MT2_ROUTE_PRISTINE;require(!verifies(error),"Clock enabled receipt required");
    setup();observed.generation+=1;require(!verifies(error),"Clock generation mismatch rejected");
    setup();observed.detour+=1;require(!verifies(error),"Clock detour receipt mismatch rejected");
    setup();observed.trampoline+=1;require(!verifies(error),"Clock trampoline receipt mismatch rejected");
    setup();memory_bytes[fixture_image+clock_rva][140]^=1;require(!verifies(error),"owned Clock full tail after byte128 cannot change");
    setup();memory_bytes[fixture_image+clock_rva][5]^=1;require(!verifies(error),"sixth stolen Clock prefix byte remains authenticated");
    setup();memory_bytes[fixture_control.routes[6].owner.trampoline][0]^=1;require(!verifies(error),"Clock stolen original bytes authenticated");
    setup();memory_bytes[fixture_control.routes[6].owner.trampoline][12]^=1;require(!verifies(error),"Clock continuation returns after exact six stolen bytes");
    setup();memory_bytes[fixture_control.routes[6].owner.trampoline][26]^=1;require(!verifies(error),"Clock physical relay detour authenticated");
    setup();memory_bytes[fixture_image+cycle_rva][20]^=1;require(!verifies(error)&&error=="Release cycle original helper body differs RVA=4194304","other Release cycle pins remain enforced");
    setup();memory_bytes[fixture_image+edit_rva][20]^=1;require(!verifies(error)&&error=="Release original helper body differs RVA=4259840","Release edit pins remain enforced");
    std::cout<<checks<<" shared Clock Release owner checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
