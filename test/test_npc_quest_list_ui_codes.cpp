#include "npc_quest_list_ui_codes.h"
#include "npc_quest_info_refresh_cases.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace ui=mt2mp::game::npc_quest_list_ui_codes;
struct Fixture {
    ui::Code code=ui::npc;
    uintptr_t image=0x140000000;
    ui::Owned owned{0x71000000,0x72000000,19};
    MT2FW_MT2_RouteReceipt route{sizeof route};
    std::string pristine,live;
    std::array<unsigned char,19> trampoline{};
    bool route_ok=true,verify_ok=true,read_ok=true;
    size_t reads=0,verified=0;
    explicit Fixture(ui::Code value=ui::npc):code(value),pristine(code.bytes,'x'){
        std::memcpy(pristine.data(),code.prefix.data(),32);
        live=pristine;const unsigned char opcode=0xe9;std::memcpy(live.data(),&opcode,1);
        route.state=MT2FW_MT2_ROUTE_ENABLED;route.generation=owned.generation;
        route.entry=image+code.rva;route.rva=code.rva;route.detour=owned.detour;route.trampoline=owned.original;
        std::memcpy(route.owner,"doodesch.multiplayer",sizeof "doodesch.multiplayer");
        std::memcpy(route.game_sha256,ui::game_sha,sizeof ui::game_sha);
        std::memcpy(route.pristine,pristine.data(),32);std::memcpy(route.current,live.data(),32);
        std::memcpy(trampoline.data(),code.prefix.data(),5);
        trampoline[5]=0xff;trampoline[6]=0x25;
        const auto resume=image+code.rva+5;std::memcpy(trampoline.data()+11,&resume,sizeof resume);
    }
    bool copy(uintptr_t at,void* output,size_t size,std::string& e){
        if(!read_ok){e="original memory read cause";return false;}
        if(at==owned.original&&size==trampoline.size()){
            std::memcpy(output,trampoline.data(),size);return true;}
        if(at!=image+code.rva||size!=live.size())throw std::runtime_error("unbounded test read");
        std::memcpy(output,live.data(),size);return true;
    }
    bool run(std::string& e,uintptr_t command=0){
        // Hashing is a seam here: the tests exercise full-body comparison and
        // route admission. They do not replace or test SHA256 itself.
        auto hash=[&](const std::string& bytes,std::string& out,std::string&){out=bytes==pristine?code.sha:"different";return true;};
        auto read=[&](uintptr_t entry,MT2FW_MT2_RouteReceipt& out,std::string& error){
            ++reads;if(entry!=image+code.rva)throw std::runtime_error("wrong named target");
            if(!route_ok){error="original route read cause";return false;}out=route;return true;};
        auto verify=[&](uintptr_t entry,const void* bytes,size_t size,std::string& error){
            ++verified;
            if(!verify_ok){error="original Framework verify cause";return false;}
            if(entry!=route.entry||size!=pristine.size()||std::memcmp(bytes,pristine.data(),size))throw std::runtime_error("original body differs");
            return true;};
        if(command)return ui::verify_npc_command(*this,image,command,owned,read,verify,hash,e);
        return ui::verify(*this,image,code,owned,read,verify,hash,e);
    }
};
int main(){
    size_t checks=0;
    auto expect=[&](bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);};
    std::string e;
    for(auto code:{ui::npc,ui::selector,ui::selector_with_alignment}){
        Fixture exact(code);expect(exact.run(e)&&exact.verified==1,"exact owned route");
        Fixture pristine(code);pristine.live=pristine.pristine;pristine.owned={};
        expect(pristine.run(e)&&!pristine.reads&&!pristine.verified,"pristine original");
        Fixture tail(code);tail.live.back()='y';expect(!tail.run(e)&&!tail.verified,"immutable body/alignment");
    }
    auto rejects=[&](auto change,const char* label){Fixture fixture;change(fixture);e.clear();expect(!fixture.run(e)&&!e.empty(),label);};
    rejects([](auto& f){std::memcpy(f.route.owner,"different.plugin",sizeof "different.plugin");},"wrong owner");
    rejects([](auto& f){++f.route.generation;},"wrong generation");
    rejects([](auto& f){++f.route.detour;},"wrong detour");
    rejects([](auto& f){++f.route.trampoline;},"wrong original");
    rejects([](auto& f){++f.route.entry;},"wrong entry");
    rejects([](auto& f){++f.route.rva;},"wrong RVA");
    rejects([](auto& f){--f.route.size;},"wrong receipt ABI size");
    rejects([](auto& f){f.route.state=MT2FW_MT2_ROUTE_UNKNOWN;},"unknown outcome");
    rejects([](auto& f){f.route.state=MT2FW_MT2_ROUTE_PREPARED;},"disabled original");
    rejects([](auto& f){f.route.game_sha256[0]='0';},"wrong EXE");
    rejects([](auto& f){std::memset(f.route.owner,'x',sizeof f.route.owner);},"unterminated owner");
    rejects([](auto& f){f.route.pristine[17]^=1;},"forged pristine prefix");
    rejects([](auto& f){f.live[1]^=1;},"jump changed after readback");
    rejects([](auto& f){f.owned={};},"arbitrary jump without exact owned hook");
    rejects([](auto& f){f.live=f.pristine;},"owned hook unexpectedly gone");
    rejects([](auto& f){f.owned.generation=0;},"missing retained generation");
    rejects([](auto& f){f.trampoline[0]^=1;},"original instructions changed");
    rejects([](auto& f){f.trampoline[5]^=1;},"original continuation jump changed");
    rejects([](auto& f){f.trampoline[11]^=1;},"original resume address changed");
    Fixture route_failure;route_failure.route_ok=false;
    expect(!route_failure.run(e)&&e=="original route read cause","retain route failure cause");
    Fixture verify_failure;verify_failure.verify_ok=false;
    expect(!verify_failure.run(e)&&e=="original Framework verify cause","retain live verification cause");
    Fixture memory_failure;memory_failure.read_ok=false;
    expect(!memory_failure.run(e)&&e=="original memory read cause","retain memory failure cause");
    Fixture vtable;expect(!vtable.run(e,vtable.image+ui::npc.rva+1)&&!vtable.reads,"wrong vtable target");
    Fixture exact_vtable;expect(exact_vtable.run(e,exact_vtable.image+ui::npc.rva),"exact original vtable target");
    checks+=quest_info_test::run();
    std::cout<<"PASS "<<checks<<" Q01 owned-menu/Info refresh comparisons\n";
}
