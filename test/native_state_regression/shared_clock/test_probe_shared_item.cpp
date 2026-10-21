#include "mt2mp/hooks/original_body.h"
#include "shared_owner_fixture.inc"
#include "placement_ui_probe/ui_probe.h"
#include "placement_ui_probe/codeguards.h"
#include "placement_ui_probe/scenery_probe_guards.h"
#include "placement_ui_probe/scene_mode_guards.h"

void* fixture_module(const wchar_t*){return reinterpret_cast<void*>(fixture_image);}
#define GetModuleHandleW fixture_module
namespace placement_ui_probe {
bool fail(std::string& error,const char* cause){error=cause;return false;}
bool ProcessMemory::copy(uintptr_t pointer,void* data,size_t count,std::string& error){::Memory memory;return memory.copy(pointer,data,count,error);}
#include "probe_initialize_extracted.inc"
}
#undef GetModuleHandleW
namespace probe=placement_ui_probe;
std::string fixture_item_sha;
void setup_probe(bool hooked=true,bool publish=true){
    setup(false,publish);
    const auto put=[](const auto& guard){auto& bytes=memory_bytes[fixture_image+guard.rva];if(bytes.size()<guard.size)bytes.assign(guard.bytes,guard.bytes+guard.size);};
    for(const auto& guard:probe::guards)put(guard);
    for(const auto& guard:probe::scenery_guards)put(guard);
    for(const auto& guard:probe::scene_mode_guards)put(guard);
    auto& route=fixture_control.routes[7];route.rva=0x207820;route.bytes=128;route.stolen=5;
    auto& owner=route.owner;owner.entry=fixture_image+route.rva;owner.trampoline=fixture_image+0xa00000;owner.detour=fixture_image+0xb00000;owner.generation=19;owner.stolen=5;
    owner.pristine=memory_bytes.at(owner.entry);fixture_item_sha=mt2mp::sha256(owner.pristine);route.sha=fixture_item_sha.c_str();
    std::vector<unsigned char> trampoline(owner.stolen+28);std::copy_n(owner.pristine.begin(),owner.stolen,trampoline.begin());
    constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};std::memcpy(trampoline.data()+owner.stolen,jump,6);
    const uintptr_t continuation=owner.entry+owner.stolen;std::memcpy(trampoline.data()+owner.stolen+6,&continuation,8);
    std::memcpy(trampoline.data()+owner.stolen+14,jump,6);std::memcpy(trampoline.data()+owner.stolen+20,&owner.detour,8);memory_bytes[owner.trampoline]=trampoline;
    if(hooked){auto& body=memory_bytes[owner.entry];body[0]=0xe9;const int32_t delta=int32_t(owner.trampoline+owner.stolen+14-owner.entry-5);std::memcpy(body.data()+1,&delta,4);}
    observed.entry=owner.entry;observed.rva=route.rva;observed.generation=owner.generation;observed.trampoline=owner.trampoline;observed.detour=owner.detour;
}
probe::Context probe_context(){return {fixture_image,0x5000,1,1,71};}
bool probe_initializes(std::string& error){probe::Probe value;return value.initialize(probe_context(),error);}
int main()try{
    std::string error;
    setup_probe(false,false);require(probe_initializes(error)&&receipt_reads==0,"complete pristine Probe guard tables still pass: "+error);
    require(fixture_item_sha=="f58f023d2a949f8307da8ba029e649767573240ea3395d832f2ebdd8dfa01b7c","Probe exact original ItemActivate bytes match Control fingerprint");
    setup_probe();require(probe_initializes(error)&&receipt_reads==1,"late Probe accepts actual owned ItemActivate with every other guard unchanged: "+error);
    setup_probe(true,false);require(!probe_initializes(error)&&error=="error: placement UI original body differs at RVA=2127904","late Probe rejects patched ItemActivate without known owner");
    setup_probe();receipt_available=false;require(!probe_initializes(error),"late Probe requires current authenticated route receipt");
    setup_probe();observed.owner[0]='x';require(!probe_initializes(error),"late Probe rejects foreign ItemActivate owner");
    setup_probe();observed.generation+=1;require(!probe_initializes(error),"late Probe rejects stale owner generation");
    setup_probe();observed.detour+=1;require(!probe_initializes(error),"late Probe rejects changed detour receipt");
    setup_probe();observed.trampoline+=1;require(!probe_initializes(error),"late Probe rejects changed original trampoline receipt");
    setup_probe();memory_bytes[fixture_image+0x207820][100]^=1;require(!probe_initializes(error),"late Probe still verifies complete original ItemActivate body");
    setup_probe();memory_bytes[fixture_control.routes[7].owner.trampoline][11]^=1;require(!probe_initializes(error),"late Probe rejects changed original continuation");
    setup_probe();memory_bytes[fixture_image+probe::guards[0].rva][20]^=1;require(!probe_initializes(error),"late Probe preserves unrelated Building guard");
    setup_probe();memory_bytes[fixture_image+probe::scenery_guards[0].rva][20]^=1;require(!probe_initializes(error),"late Probe preserves Scenery guard");
    setup_probe();memory_bytes[fixture_image+probe::scene_mode_guards[0].rva][20]^=1;require(!probe_initializes(error),"late Probe preserves mode-selector guard");
    setup_probe();fixture_thread=72;require(!probe_initializes(error),"late Probe preserves bound native-thread requirement");
    setup_probe();probe::Probe value;auto invalid=probe_context();invalid.world=0;require(!value.initialize(invalid,error),"late Probe preserves actual world requirement");
    require(value.initialize(probe_context(),error),"rejected Probe initialization does not commit context");
    require(!value.initialize(probe_context(),error),"confirmed Probe initialization remains one-shot");
    std::cout<<checks<<" late Probe shared ItemActivate checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
