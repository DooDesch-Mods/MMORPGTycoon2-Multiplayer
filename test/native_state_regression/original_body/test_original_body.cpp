#include "mt2mp/core/hash.h"
#include "mt2mp/hooks/framework_backend.h"
namespace mt2mp {inline unsigned body_hash_calls=0;inline std::string counted_body_sha256(const std::vector<unsigned char>& bytes){++body_hash_calls;return sha256(bytes);}}
#define sha256 counted_body_sha256
#include "mt2mp/hooks/original_body.h"
#undef sha256
#include "framework_backend.cpp"
#include "duplicate_codeguards.h"
#include "scenery_kiss/lifecycle_codeguards.h"
#include <iostream>
#include <map>
#include <array>
#include "live_host_state/memory_query_timing.h"
namespace live_host_state=mt2mp::game::live_host_state;

namespace fixture {
constexpr uintptr_t image=0x140000000;
unsigned checks=0,reads=0,verifies=0;
bool available=true,physical=true,verification=true;
uintptr_t denied_read=0;
void* query_page=nullptr;
std::vector<std::pair<uintptr_t,size_t>> byte_reads;
std::map<uintptr_t,std::vector<unsigned char>> memory;
struct Service {MT2FW_MT2_RouteReceipt receipt{};std::vector<unsigned char> pristine;};
std::map<std::string,Service> services;
void require(bool yes,const std::string& why){++checks;if(!yes)throw std::runtime_error(why);}
Service& service(uintptr_t rva){for(auto& [name,row]:services){(void)name;if(row.receipt.rva==rva)return row;}throw std::runtime_error("fixture route missing");}
MT2FW_MT2_Result read(void*,const char* name,MT2FW_MT2_RouteReceipt* output){
    ++reads;auto it=services.find(name);if(it==services.end())return MT2FW_MT2_UNAVAILABLE;
    *output=it->second.receipt;return available&&physical?MT2FW_MT2_OK:MT2FW_MT2_FAILED;
}
MT2FW_MT2_Result verify(void* context,const char* name,uint32_t offset,const void* expected,uint32_t length,MT2FW_MT2_RouteReceipt* output){
    ++verifies;const auto result=read(context,name,output);if(result!=MT2FW_MT2_OK)return result;
    auto& row=services.at(name);const auto& live=memory.at(row.receipt.entry);
    if(!verification||offset||!length||length>row.pristine.size()||
       std::memcmp(expected,row.pristine.data(),length)||std::memcmp(live.data(),row.receipt.current,std::min<size_t>(32,live.size()))||
       (length>32&&std::memcmp(live.data()+32,row.pristine.data()+32,length-32)))return MT2FW_MT2_FAILED;
    return MT2FW_MT2_OK;
}
MT2FW_MT2_Result prepare(void*,const char* name,void* detour,MT2FW_MT2_RouteReceipt* output){
    auto& row=services.at(name);auto& receipt=row.receipt;receipt.state=MT2FW_MT2_ROUTE_PREPARED;
    receipt.generation=receipt.rva;receipt.detour=uintptr_t(detour);receipt.trampoline=receipt.entry+0x2000000;
    strcpy_s(receipt.owner,"doodesch.multiplayer");*output=receipt;return MT2FW_MT2_OK;
}
MT2FW_MT2_Result enable(void*,const char* name,MT2FW_MT2_RouteReceipt* output){
    auto& row=services.at(name);auto& receipt=row.receipt;receipt.state=MT2FW_MT2_ROUTE_ENABLED;
    auto& live=memory.at(receipt.entry);live[0]=0xe9;for(size_t i=1;i<5;++i)live[i]=uint8_t(i+0x70);
    std::memcpy(receipt.current,live.data(),std::min<size_t>(32,live.size()));*output=receipt;return MT2FW_MT2_OK;
}
MT2FW_MT2_Result remove(void*,const char*,MT2FW_MT2_RouteReceipt*){return MT2FW_MT2_FAILED;}
void route(uintptr_t rva,size_t length,const char* name){
    auto& bytes=memory[image+rva];const auto old=bytes.size();bytes.resize(length);
    for(size_t i=old;i<length;++i)bytes[i]=uint8_t((rva+i*11)%251);
    auto& row=services[name];row.pristine=bytes;auto& receipt=row.receipt;
    receipt.size=sizeof receipt;receipt.state=MT2FW_MT2_ROUTE_PRISTINE;receipt.entry=image+rva;receipt.rva=rva;
    strcpy_s(receipt.game_sha256,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6");
    std::memcpy(receipt.pristine,bytes.data(),std::min<size_t>(32,bytes.size()));std::memcpy(receipt.current,receipt.pristine,32);
}
void hook(uintptr_t rva){auto& backend=mt2mp::hooks::framework::process_backend();void* original=nullptr;
    require(backend.create(reinterpret_cast<void*>(image+rva),reinterpret_cast<void*>(image+rva+0x3000000),&original)==MH_OK,"actual Backend prepare");
    require(backend.enable(reinterpret_cast<void*>(image+rva))==MH_OK,"actual Backend enable");
}
}
struct Memory {
    bool copy(uintptr_t address,void* out,size_t count,std::string& error){auto it=fixture::memory.find(address);
        fixture::byte_reads.emplace_back(address,count);
        if(fixture::query_page){MEMORY_BASIC_INFORMATION info{};
            if(!live_host_state::apply_timing::virtual_query(fixture::query_page,&info,sizeof info,live_host_state::apply_timing::Part::QueryWorldProp)||
               (info.Protect&(PAGE_NOACCESS|PAGE_GUARD))){error="fixture unreadable memory";return false;}}
        if(address==fixture::denied_read||it==fixture::memory.end()||count>it->second.size()){error="fixture unreadable memory";return false;}
        std::memcpy(out,it->second.data(),count);return true;}
    bool read(uintptr_t address,void* out,size_t count,std::string& error){return copy(address,out,count,error);}
};
namespace hooks=mt2mp::hooks;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct {uintptr_t image=fixture::image;} state;
struct Pin {uintptr_t rva;size_t bytes;const char* sha;};
std::array<std::string,7> pin_hashes;
namespace character_membership_codes {std::array<Pin,5> helpers{};}
namespace dungeon_cells_codes {std::array<Pin,2> helpers{},graph_helpers{};constexpr int drag=0,release=1,create=2;}
namespace quest_bridge {bool sha256(const std::string& bytes,std::string& hash,std::string&){hash=mt2mp::counted_body_sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}}
namespace quest_list {
bool read_route(uintptr_t entry,MT2FW_MT2_RouteReceipt& receipt,std::string& error){auto& backend=hooks::framework::process_backend();if(backend.read_route(reinterpret_cast<void*>(entry),receipt))return true;error=backend.error();return false;}
bool verify_code(uintptr_t entry,const void* bytes,size_t size,std::string& error){return hooks::framework::process_backend().verify_code(entry,bytes,size,error);}
}
namespace character {
void confirm_entry(void*){}
void(*original)(void*)=nullptr;uint64_t generation=0;
#include "character_extracted.inc"
}
namespace cells {
void drag_entry(){}void release_entry(){}void create_entry(){}
void* original_drag=nullptr;void* original_release=nullptr;void* original_create=nullptr;
uint64_t drag_generation=0,release_generation=0,create_generation=0;
bool route(Memory&,int,uintptr_t,uintptr_t,uint64_t,size_t,std::string&){return true;}
#include "cells_extracted.inc"
}
namespace policy {
struct Body {uintptr_t rva;size_t size;const char* sha;};
std::array<Body,2> bodies{};Memory memory;
namespace hud_time_runtime {bool verify_window_show_hook(uintptr_t,std::string&){return true;}}
#include "policy_extracted.inc"
}
namespace mt2mp::scenery_kiss {
namespace native {using Memory=::Memory;}
#include "scenery_extracted.inc"
}

namespace control_input::runtime {
bool verify_item_activate(uintptr_t,size_t,const char*,bool&,std::string& error){error="unexpected ItemActivate fixture route";return false;}
}
namespace monster_cursor {
struct Runtime {uintptr_t image=fixture::image;void(*release)(void*,void*,const float*)=nullptr;} state;
namespace elite_probe_codes {struct Code {uintptr_t rva;size_t size;const char* sha;};std::array<Code,2> values;}
namespace monster_hook_contract {constexpr std::array<unsigned char,5> prefix{};const char* tail_sha="unused fixture Release";}
namespace zone_action_runtime {bool verify_owned_entry(uintptr_t,std::string& error){error="unexpected Zone fixture route";return false;}}
bool field(Memory& memory,uintptr_t base,size_t offset,uintptr_t& value,std::string& error){return memory.copy(base+offset,&value,sizeof value,error);}
void release_entry(void*,void*,const float*){}
#include "monster_cursor_extracted.inc"
}
namespace npc_history {
std::array<std::string,6> npc_history_hashes;
#include "npc_history_extracted.inc"
}
#include "gizmo_code_digest_cases.inc"

int main()try{
    using namespace fixture;
#include "duplicate_fixture_guards.inc"
    for(const auto& item:mt2mp::scenery_kiss::lifecycle_codes)memory[image+item.rva]=std::vector<unsigned char>(item.bytes,item.bytes+item.size);
    route(0x5cfdd0,memory.at(image+0x5cfdd0).size(),"mp.scenery-brush-finish.5cfdd0");
    route(0x3c4c10,699,"mp.npc-level-command.3c4c10");
    route(0x3c8d50,801,"mp.interventions.npc-entity-update.3c8d50");
    route(0x689e80,4212,"mp.character-rename-ui.689e80");
    route(0x53b260,649,"mp.building-and-scenery-capture.53b260");
    route(0x6a1780,160,"mp.management-gm-policy.6a1780");
    route(0x54d80,16,"mp.economic-authority-fence.54d80");
    route(0x684250,276,"mp.character-membership-confirm.684250");
    constexpr uintptr_t unrelated=0x987654;memory[image+unrelated]=std::vector<unsigned char>(64,0x37);
    const std::array<uintptr_t,7> rvas{0x3c4c10,0x3c8d50,0x689e80,0x53b260,0x6a1780,unrelated,0x684250};
    for(size_t i=0;i<rvas.size();++i)pin_hashes[i]=mt2mp::sha256(memory.at(image+rvas[i]));
    auto pin=[&](size_t i){return Pin{rvas[i],memory.at(image+rvas[i]).size(),pin_hashes[i].c_str()};};
    character_membership_codes::helpers={pin(0),pin(1),pin(2),pin(5),pin(6)};
    dungeon_cells_codes::helpers={pin(3),pin(5)};dungeon_cells_codes::graph_helpers={pin(5),pin(5)};
    policy::bodies={policy::Body{rvas[4],160,pin_hashes[4].c_str()},policy::Body{unrelated,64,pin_hashes[5].c_str()}};
    MT2FW_MT2_RoutesV1 api{};api.size=sizeof api;api.version=MT2FW_MT2_ROUTES_VERSION;api.context=&services;
    api.hook_prepare=prepare;api.hook_enable=enable;api.hook_remove=remove;api.route_read=read;api.code_verify=verify;
    auto& backend=hooks::framework::process_backend();
    require(backend.bind(api,image,"doodesch.multiplayer",GetCurrentThreadId(),[](void*,uintptr_t,std::string&){return true;},[](void*,const char*,const std::string&){},nullptr),"actual Backend bind");
    Memory m;std::string error;
    error="retained earlier failure";
    require(hooks::framework::verify_original_hash(image+rvas[0],memory.at(image+rvas[0]).data(),699,pin_hashes[0].c_str(),error)&&error.empty(),"pristine hash success clears stale error");
    error="retained earlier failure";
    require(hooks::framework::verify_original_bytes(image+rvas[0],memory.at(image+rvas[0]).data(),service(rvas[0]).pristine.data(),699,error)&&error.empty(),"pristine byte success clears stale error");
    require(character::code(m,error)&&cells::code(m,error)&&policy::code(image,error)&&mt2mp::game::duplicate_code::check(m,image,error),"all pristine actual consumers");
    require(reads==0&&verifies==0,"raw original success keeps existing path");
    require(mt2mp::scenery_kiss::code(m,image,0,std::size(mt2mp::scenery_kiss::lifecycle_codes),error),"pristine exact Scenery lifecycle guards");
    for(auto rva:{0x3c4c10,0x3c8d50,0x689e80,0x53b260,0x6a1780,0x54d80,0x5cfdd0})hook(uintptr_t(rva));
    require(character::code(m,error),"Character both shared hooks and4212byte original body accepted");
    error="retained earlier failure";
    require(hooks::framework::verify_original_hash(image+rvas[0],memory.at(image+rvas[0]).data(),699,pin_hashes[0].c_str(),error)&&error.empty(),"authenticated hash success clears stale error");
    error="retained earlier failure";
    require(hooks::framework::verify_original_bytes(image+rvas[0],memory.at(image+rvas[0]).data(),service(rvas[0]).pristine.data(),699,error)&&error.empty(),"authenticated byte success clears stale error");
    require(cells::code(m,error),"Dungeon649byte helper after build hook accepted");
    require(policy::code(image,error),"late Bug-C160byte body accepted");
    require(mt2mp::game::duplicate_code::check(m,image,error),"Duplicate both16byte hooked helpers accepted");
    require(mt2mp::scenery_kiss::code(m,image,3,std::size(mt2mp::scenery_kiss::lifecycle_codes),error),"Scenery cancel after brush Finish hook accepted");
    memory[image+0x5cfdd0].back()^=1;
    require(!mt2mp::scenery_kiss::code(m,image,3,std::size(mt2mp::scenery_kiss::lifecycle_codes),error),"Scenery complete original Finish tail still rejects");
    memory[image+0x5cfdd0].back()^=1;
    auto& target=service(0x3c4c10);const auto good=target.receipt;
    auto rejected=[&](const char* why){error.clear();require(!character::code(m,error)&&!error.empty(),why);target.receipt=good;};
    available=false;rejected("missing receipt rejects");available=true;
    physical=false;rejected("invalid owned physical hook rejects");physical=true;
    strcpy_s(target.receipt.owner,"foreign");rejected("actual Backend rejects foreign owner");
    target.receipt.generation++;rejected("actual Backend rejects stale generation");
    target.receipt.detour++;rejected("actual Backend rejects wrong detour");
    target.receipt.trampoline++;rejected("actual Backend rejects wrong trampoline");
    target.receipt.state=MT2FW_MT2_ROUTE_UNKNOWN;rejected("actual Backend rejects unknown state");
    target.receipt.current[12]^=1;rejected("current prefix mismatch rejects");
    target.receipt.pristine[12]^=1;rejected("pristine prefix authentication rejects");
    verification=false;rejected("SDK code verification failure remains failure");verification=true;
    memory[image+0x3c4c10][698]^=1;rejected("full body tail corruption rejects");memory[image+0x3c4c10][698]^=1;
    memory[image+0x689e80][4211]^=1;rejected("long tail beyond4096 still rejects");memory[image+0x689e80][4211]^=1;
    memory[image+unrelated][63]^=1;rejected("unrelated uncatalogued pin still rejects");
    require(!cells::code(m,error)&&!policy::code(image,error),"unrelated cell and policy pins still reject");memory[image+unrelated][63]^=1;
    memory[image+0x53b260][648]^=1;require(!cells::code(m,error),"Dungeon complete helper tail rejects");memory[image+0x53b260][648]^=1;
    memory[image+0x54d80][15]^=1;require(!mt2mp::game::duplicate_code::check(m,image,error),"Duplicate complete16byte guard rejects");memory[image+0x54d80][15]^=1;
    hook(0x684250);character::original=reinterpret_cast<void(*)(void*)>(service(0x684250).receipt.trampoline);character::generation=service(0x684250).receipt.generation;
    require(!character::code(m,error)&&error.find("ownership")!=std::string::npos,"existing Character Confirm-specific detour guard preserved");character::original=nullptr;
    require(character::code(m,error)&&cells::code(m,error)&&policy::code(image,error)&&mt2mp::game::duplicate_code::check(m,image,error),"actual consumers recover after fixture changes removed");
    route(0x5854e0,272,"mp.dungeon.build-cursor-behaviour.5854e0");
    const auto cursor_sha=mt2mp::sha256(memory.at(image+0x5854e0));
    monster_cursor::elite_probe_codes::values={monster_cursor::elite_probe_codes::Code{0x5854e0,272,cursor_sha.c_str()},
        monster_cursor::elite_probe_codes::Code{unrelated,64,pin_hashes[5].c_str()}};
    const uintptr_t monster_release=image+0x5bf3c0;auto& release_slot=memory[image+0x1739500+0xe8];release_slot.resize(sizeof monster_release);
    std::memcpy(release_slot.data(),&monster_release,sizeof monster_release);
    require(monster_cursor::code(m,false,error),"Monster first activation accepts pristine Cursor body");
    hook(0x5854e0);
    require(monster_cursor::code(m,true,error),"Monster repeated activation accepts actual Building-owned Cursor hook");
    auto& cursor_service=service(0x5854e0);const auto cursor_receipt=cursor_service.receipt;
    const auto cursor_reject=[&](const char* cause){error.clear();require(!monster_cursor::code(m,true,error)&&error.find("RVA=5788896")!=std::string::npos,cause);cursor_service.receipt=cursor_receipt;};
    available=false;cursor_reject("Monster missing Cursor receipt rejected");available=true;
    physical=false;cursor_reject("Monster invalid physical Cursor owner rejected");physical=true;
    strcpy_s(cursor_service.receipt.owner,"foreign");cursor_reject("Monster foreign Cursor owner rejected");
    cursor_service.receipt.generation++;cursor_reject("Monster stale Cursor generation rejected");
    cursor_service.receipt.trampoline++;cursor_reject("Monster changed Cursor trampoline rejected");
    cursor_service.receipt.detour++;cursor_reject("Monster changed Cursor detour rejected");
    cursor_service.receipt.current[12]^=1;cursor_reject("Monster mismatched live Cursor prefix rejected");
    verification=false;cursor_reject("Monster original Cursor prefix authentication failure rejected");verification=true;
    memory[image+0x5854e0][271]^=1;cursor_reject("Monster complete Cursor tail corruption rejected");memory[image+0x5854e0][271]^=1;
    memory[image+unrelated][63]^=1;error.clear();require(!monster_cursor::code(m,true,error)&&error=="Monster native body differs RVA="+std::to_string(unrelated),"Monster unrelated raw body pin retained");memory[image+unrelated][63]^=1;
    release_slot[0]^=1;error.clear();require(!monster_cursor::code(m,true,error)&&error=="Monster primary Release address point differs","Monster Release address point guard retained");release_slot[0]^=1;
    require(monster_cursor::code(m,true,error),"Monster current owned Cursor recovers after invalid evidence removed");
    {
#include "npc_history_fixture_pins.inc"
        for(size_t i=0;i<npc_history_pins.size();++i){const auto [rva,size]=npc_history_pins[i];
            memory[image+rva]=std::vector<unsigned char>(size,0x41);
            npc_history::npc_history_hashes[i]=mt2mp::sha256(memory.at(image+rva));}
        const std::array<int32_t,3> costs{1000,100,500};auto& cost_bytes=memory[image+0x1493b28];cost_bytes.resize(sizeof costs);
        std::memcpy(cost_bytes.data(),costs.data(),sizeof costs);
        require(npc_history::code(m,error),"NPC retirement accepts all pristine full bodies");
        route(0x55ed80,704,"objects.move");auto& move=service(0x55ed80);auto& receipt=move.receipt;
        receipt.state=MT2FW_MT2_ROUTE_ENABLED;receipt.generation=1;receipt.detour=image+0x2000000;receipt.trampoline=image+0x3000000;
        strcpy_s(receipt.owner,"framework");memory[image+0x55ed80][0]=0xe9;
        std::memcpy(receipt.current,memory.at(image+0x55ed80).data(),32);
        const auto saved=receipt;
        require(npc_history::code(m,error),"NPC retirement accepts authenticated shared Framework objects.move");
        const auto rejects=[&](const char* why){error.clear();require(!npc_history::code(m,error)&&error.find("RVA=5631360")!=std::string::npos,why);receipt=saved;};
        available=false;rejects("NPC missing MoveDo receipt rejects");available=true;
        physical=false;rejects("NPC unauthenticated physical MoveDo rejects");physical=true;
        strcpy_s(receipt.owner,"foreign");rejects("NPC foreign shared owner rejects");
        receipt.generation=0;rejects("NPC absent shared generation rejects");
        receipt.trampoline=0;rejects("NPC absent shared trampoline rejects");
        receipt.detour=0;rejects("NPC absent shared detour rejects");
        receipt.current[12]^=1;rejects("NPC live prefix mismatch rejects");
        verification=false;rejects("NPC original prefix authentication failure rejects");verification=true;
        memory[image+0x55ed80][703]^=1;rejects("NPC complete704byte tail corruption rejects");memory[image+0x55ed80][703]^=1;
        for(const auto [rva,size]:npc_history_pins)if(rva!=0x55ed80){memory[image+rva][size-1]^=1;error.clear();
            require(!npc_history::code(m,error)&&error=="NPC history original full body differs RVA="+std::to_string(rva),"NPC unrelated raw full body pin rejects");memory[image+rva][size-1]^=1;}
        cost_bytes[0]^=1;require(!npc_history::code(m,error)&&error=="NPC history original TypeCost table differs","NPC TypeCost guard retained");cost_bytes[0]^=1;
        require(npc_history::code(m,error),"NPC retirement guard recovers after invalid evidence removed");
    }
    gizmo_code_digest_fixture::run();
    std::cout<<"PASS "<<checks<<" original-body and actual consumer/Backend checks\n";return 0;
}catch(const std::exception& problem){std::cerr<<problem.what()<<'\n';return 1;}
