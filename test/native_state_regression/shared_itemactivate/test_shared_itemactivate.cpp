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
constexpr uintptr_t fixture_image=0x140000000,item_rva=0x207820,other_rva=0x400000;
std::map<uintptr_t,std::vector<unsigned char>> memory_bytes;
std::string item_sha,other_sha;
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
    std::string error()const{return "original Framework receipt unavailable";}
};
Backend& process_backend(){static Backend backend;return backend;}
}
namespace mt2mp::control_input::runtime {
struct Route {uintptr_t rva;size_t bytes,stolen;const char* sha;void* original=nullptr;game::release_ui_owner::Owner owner;};
struct Runtime {uint32_t thread=71;uintptr_t image=::fixture_image;bool ready=true;Memory memory;std::array<Route,9> routes{};};
std::atomic<Runtime*> published{nullptr};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "control_itemactivate_extracted.inc"
}
namespace mt2mp::game::npc_probe::monster_action_runtime {
struct Runtime {uintptr_t image=::fixture_image;void(*release)(void*,void*,const float*)=nullptr;} state;
namespace elite_probe_codes {struct Code {uintptr_t rva;size_t size;std::string sha;};std::vector<Code> values;}
namespace monster_hook_contract {constexpr std::array<unsigned char,5> prefix{};const char* tail_sha="unused fixture route";}
namespace zone_action_runtime {bool verify_owned_entry(uintptr_t,std::string& error){error="unexpected Zone fixture route";return false;}}
namespace quest_bridge {bool sha256(const std::string& bytes,std::string& sha,std::string&){sha=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool field(Memory& memory,uintptr_t base,size_t offset,uintptr_t& value,std::string& error){return memory.copy(base+offset,&value,sizeof value,error);}
void release_entry(void*,void*,const float*){}
#include "monster_itemactivate_extracted.inc"
}
namespace control=mt2mp::control_input::runtime;
namespace monster=mt2mp::game::npc_probe::monster_action_runtime;
control::Runtime runtime;
void setup(bool hooked=true,bool publish=true){
    memory_bytes.clear();fixture_thread=71;receipt_available=true;receipt_reads=0;runtime={};
    auto& route=runtime.routes[7];route.rva=item_rva;route.bytes=128;route.stolen=5;
    auto& owner=route.owner;owner.entry=fixture_image+item_rva;owner.trampoline=fixture_image+0x800000;owner.detour=fixture_image+0x900000;owner.generation=9;owner.stolen=5;
    owner.pristine.resize(128,0x90);for(size_t i=0;i<5;++i)owner.pristine[i]=uint8_t(i+10);
    item_sha=mt2mp::sha256(owner.pristine);route.sha=item_sha.c_str();
    memory_bytes[owner.entry]=owner.pristine;
    std::vector<unsigned char> trampoline(owner.stolen+28);
    std::copy_n(owner.pristine.begin(),owner.stolen,trampoline.begin());
    constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};
    std::memcpy(trampoline.data()+owner.stolen,jump,6);
    const uintptr_t continuation=owner.entry+owner.stolen;
    std::memcpy(trampoline.data()+owner.stolen+6,&continuation,8);
    std::memcpy(trampoline.data()+owner.stolen+14,jump,6);
    std::memcpy(trampoline.data()+owner.stolen+20,&owner.detour,8);
    memory_bytes[owner.trampoline]=trampoline;
    if(hooked){auto& body=memory_bytes[owner.entry];body[0]=0xe9;const int32_t delta=int32_t(owner.trampoline+owner.stolen+14-owner.entry-5);std::memcpy(body.data()+1,&delta,4);}
    std::vector<unsigned char> other(64,0xcc);other_sha=mt2mp::sha256(other);memory_bytes[fixture_image+other_rva]=other;
    monster::elite_probe_codes::values={{item_rva,128,item_sha},{other_rva,64,other_sha}};
    const uintptr_t release=fixture_image+0x5bf3c0;std::vector<unsigned char> ap(sizeof release);std::memcpy(ap.data(),&release,sizeof release);memory_bytes[fixture_image+0x1739500+0xe8]=ap;
    observed={};observed.size=sizeof observed;observed.state=MT2FW_MT2_ROUTE_ENABLED;observed.entry=owner.entry;observed.rva=item_rva;
    observed.generation=owner.generation;observed.trampoline=owner.trampoline;observed.detour=owner.detour;
    std::memcpy(observed.owner,"doodesch.multiplayer",20);
    control::published.store(publish?&runtime:nullptr);
}
bool verifies(std::string& error){Memory memory;return monster::code(memory,false,error);}
int main()try{
    std::string error;
    setup(false,false);require(verifies(error),"pristine prehook Monster pin stays valid: "+error);require(receipt_reads==0,"pristine owner absence retains original raw full-body validation");
    memory_bytes[fixture_image+item_rva][64]^=1;require(!verifies(error),"unowned pristine body corruption rejected");
    setup(true,false);require(!verifies(error)&&error=="Monster native body differs RVA=2127904","patched entry without Control owner fails original pin");
    setup();require(verifies(error)&&receipt_reads==1,"correct Control-owned hook passes one current receipt and complete restored-body verification: "+error);
    setup();runtime.ready=false;require(!verifies(error),"published incomplete Control installation rejected");
    setup();fixture_thread=72;require(!verifies(error),"foreign thread cannot claim Control ownership");
    setup();runtime.image+=1;require(!verifies(error),"foreign image cannot claim Control ownership");
    setup();runtime.routes[7].rva+=1;require(!verifies(error),"different Control route cannot substitute ItemActivate");
    setup();runtime.routes[7].bytes-=1;require(!verifies(error),"different original extent rejected");
    setup();runtime.routes[7].sha="different original hash";require(!verifies(error),"different original fingerprint rejected");
    setup();runtime.routes[7].owner.entry+=1;require(!verifies(error),"physical owner entry differs from exact shared RVA");
    setup();receipt_available=false;require(!verifies(error)&&error.find("original Framework receipt unavailable")!=std::string::npos,"original missing route receipt cause retained");
    setup();observed.state=MT2FW_MT2_ROUTE_PRISTINE;require(!verifies(error),"unexpected route lifecycle state rejected");
    setup();observed.generation+=1;require(!verifies(error),"changed generation rejected");
    setup();observed.trampoline+=1;require(!verifies(error),"changed receipt trampoline rejected");
    setup();observed.detour+=1;require(!verifies(error),"changed receipt detour rejected");
    setup();observed.owner[0]='x';require(!verifies(error),"different Framework owner rejected");
    setup();memory_bytes[runtime.routes[7].owner.entry][64]^=1;require(!verifies(error),"modified immutable original tail rejected after detour");
    setup();memory_bytes[runtime.routes[7].owner.entry][0]=0x90;require(!verifies(error),"removed or foreign entry jump rejected");
    setup();memory_bytes[runtime.routes[7].owner.trampoline][0]^=1;require(!verifies(error),"mutated stolen original instruction rejected");
    setup();memory_bytes[runtime.routes[7].owner.trampoline][11]^=1;require(!verifies(error),"wrong original continuation rejected");
    setup();memory_bytes[runtime.routes[7].owner.trampoline][25]^=1;require(!verifies(error),"wrong physical relay detour rejected");
    setup();memory_bytes[fixture_image+other_rva][32]^=1;require(!verifies(error)&&error=="Monster native body differs RVA=4194304","unrelated Monster body pins remain enforced");
    setup();memory_bytes[fixture_image+0x1739500+0xe8][0]^=1;require(!verifies(error),"Monster primary Release class slot remains enforced");
    std::cout<<checks<<" shared ItemActivate owner checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
