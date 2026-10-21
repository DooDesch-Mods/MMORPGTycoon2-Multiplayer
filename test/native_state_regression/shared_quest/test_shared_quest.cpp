#include "mt2mp/hooks/original_body.h"
namespace hooks=mt2mp::hooks;
#include "mt2mp/core/hash.h"
#include "npc_quest_list_ui_codes.h"
#include "framework_backend.cpp"
#include <iostream>
#include <algorithm>
#include <map>

namespace fixture {
constexpr uintptr_t image=0x140000000,checkbox=0x6a1780,other=0x563f50,palette_ui=0x59e990;
unsigned checks=0,reads=0,verifications=0;
bool available=true,physical=true,thread_ok=true,fail_verify=false;
MT2FW_MT2_RouteReceipt observed{};
std::map<uintptr_t,std::vector<unsigned char>> memory;
std::vector<unsigned char> pristine,live;
std::string checkbox_sha,other_sha,palette_sha;
void require(bool yes,const std::string& why){++checks;if(!yes)throw std::runtime_error(why);}
MT2FW_MT2_Result read(void*,const char* name,MT2FW_MT2_RouteReceipt* out){
    ++reads;*out=observed;
    if(!available||!physical||!thread_ok||std::strcmp(name,"mp.management-gm-policy.6a1780"))return MT2FW_MT2_FAILED;
    return MT2FW_MT2_OK;
}
MT2FW_MT2_Result verify(void* context,const char* name,uint32_t offset,const void* expected,uint32_t count,MT2FW_MT2_RouteReceipt* out){
    ++verifications;auto result=read(context,name,out);if(result!=MT2FW_MT2_OK)return result;
    if(fail_verify||offset||count!=153||std::memcmp(expected,pristine.data(),count)||memory.at(image+checkbox)!=live)return MT2FW_MT2_FAILED;
    return MT2FW_MT2_OK;
}
MT2FW_MT2_Result prepare(void*,const char*,void* detour,MT2FW_MT2_RouteReceipt* out){
    observed.state=MT2FW_MT2_ROUTE_PREPARED;observed.generation=9;observed.detour=uintptr_t(detour);observed.trampoline=image+0x900000;
    strcpy_s(observed.owner,"doodesch.multiplayer");*out=observed;return MT2FW_MT2_OK;
}
MT2FW_MT2_Result enable(void*,const char*,MT2FW_MT2_RouteReceipt* out){
    observed.state=MT2FW_MT2_ROUTE_ENABLED;live=pristine;live[0]=0xe9;for(size_t i=1;i<7;++i)live[i]=uint8_t(0x50+i);
    memory[image+checkbox]=live;std::memcpy(observed.current,live.data(),32);*out=observed;return MT2FW_MT2_OK;
}
MT2FW_MT2_Result remove(void*,const char*,MT2FW_MT2_RouteReceipt*){return MT2FW_MT2_FAILED;}
}
struct Memory {
    bool copy(uintptr_t address,void* output,size_t bytes,std::string& error){
        auto found=fixture::memory.find(address);
        if(found==fixture::memory.end()||bytes>found->second.size()){error="fixture memory unavailable";return false;}
        std::memcpy(output,found->second.data(),bytes);return true;
    }
};
struct Code {uintptr_t rva;size_t size;const char* sha;};
namespace npc_quest_list_codes {std::array<Code,2> values{};}
namespace dungeon_palette_codes {std::array<Code,2> helpers{};}
namespace quest_bridge {
bool sha256(const std::string& bytes,std::string& hash,std::string&){hash=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}
}
struct {uintptr_t image=fixture::image;} state;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T> bool field(Memory&,uintptr_t,size_t,T& value,std::string&){value=T(0x437f0000);return true;}
namespace quest_list {
#include "quest_extracted.inc"
}
namespace dungeon_palette {
namespace npc_quest_list_ui_codes=mt2mp::game::npc_quest_list_ui_codes;
void* original=nullptr;uint64_t generation=0;
bool entry(void*,const void*){return false;}
npc_quest_list_ui_codes::Code ui_code{};
#include "palette_extracted.inc"
}

int main()try{
    using namespace fixture;
    pristine.resize(153);for(size_t i=0;i<pristine.size();++i)pristine[i]=uint8_t(i*17+11);
    live=pristine;memory[image+checkbox]=live;checkbox_sha=mt2mp::sha256(pristine);
    memory[image+other]=std::vector<unsigned char>(480,0x91);other_sha=mt2mp::sha256(memory[image+other]);
    memory[image+palette_ui]=std::vector<unsigned char>(308,0x81);palette_sha=mt2mp::sha256(memory[image+palette_ui]);
    npc_quest_list_codes::values={Code{checkbox,153,checkbox_sha.c_str()},Code{other,480,other_sha.c_str()}};
    dungeon_palette_codes::helpers=npc_quest_list_codes::values;
    dungeon_palette::ui_code={palette_ui,308,palette_sha.c_str(),{}};
    observed.size=sizeof observed;observed.state=MT2FW_MT2_ROUTE_PRISTINE;observed.entry=image+checkbox;observed.rva=checkbox;
    strcpy_s(observed.game_sha256,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6");
    std::memcpy(observed.pristine,pristine.data(),32);std::memcpy(observed.current,live.data(),32);
    MT2FW_MT2_RoutesV1 api{};api.size=sizeof api;api.version=MT2FW_MT2_ROUTES_VERSION;api.context=&observed;
    api.hook_prepare=prepare;api.hook_enable=enable;api.hook_remove=remove;api.route_read=read;api.code_verify=verify;
    auto& backend=mt2mp::hooks::framework::process_backend();
    require(backend.bind(api,image,"doodesch.multiplayer",GetCurrentThreadId(),[](void*,uintptr_t,std::string&){return true;},[](void*,const char*,const std::string&){},nullptr),"actual Backend bind");
    Memory m;std::string error;
    require(quest_list::inner_codes(m,error),"pristine Quest body");
    require(dungeon_palette::code(m,error),"pristine palette body");
    require(verifications==2,"both pristine consumers use full Framework verification");
    void* original=nullptr;
    require(backend.create(reinterpret_cast<void*>(image+checkbox),reinterpret_cast<void*>(image+0xa00000),&original)==MH_OK,"actual Backend prepare");
    require(backend.enable(reinterpret_cast<void*>(image+checkbox))==MH_OK,"actual Backend enable");
    require(quest_list::inner_codes(m,error),"owned Quest full original body");
    require(dungeon_palette::code(m,error),"owned palette full original body");
    const auto good=observed;
    auto rejected=[&](const char* why){error.clear();require(!quest_list::inner_codes(m,error)&&!error.empty(),why);observed=good;};
    available=false;rejected("missing route rejects");available=true;
    physical=false;rejected("invalid physical route rejects");physical=true;
    strcpy_s(observed.owner,"foreign");rejected("actual Backend rejects foreign owner");
    observed.generation++;rejected("actual Backend rejects stale generation");
    observed.detour++;rejected("actual Backend rejects wrong detour");
    observed.trampoline++;rejected("actual Backend rejects wrong trampoline");
    observed.entry++;rejected("actual Backend rejects wrong entry");
    observed.rva++;rejected("actual Backend rejects wrong RVA");
    observed.game_sha256[0]='0';rejected("actual Backend rejects wrong image hash");
    observed.state=MT2FW_MT2_ROUTE_UNKNOWN;rejected("actual Backend rejects unknown state");
    observed.current[10]^=1;rejected("live prefix must match receipt");
    observed.pristine[10]^=1;rejected("forged pristine prefix fails full expected-byte verification");
    memory[image+checkbox][152]^=1;rejected("body corruption beyond restored prefix rejects");memory[image+checkbox]=live;
    fail_verify=true;rejected("Framework full-body failure propagates");fail_verify=false;
    npc_quest_list_codes::values[0].sha=other_sha.c_str();rejected("unchanged consumer SHA remains mandatory");npc_quest_list_codes::values[0].sha=checkbox_sha.c_str();
    memory[image+other][479]^=1;rejected("unrelated Quest pin remains mandatory");
    require(!dungeon_palette::code(m,error),"unrelated palette pin remains mandatory");memory[image+other][479]^=1;
    memory[image+palette_ui][307]^=1;require(!dungeon_palette::code(m,error),"palette outer body remains mandatory");memory[image+palette_ui][307]^=1;
    observed.generation++;require(!dungeon_palette::code(m,error),"palette uses authenticated owner receipt too");observed=good;
    std::string bytes(160,'x');require(!quest_list::checkbox_original(image+checkbox,bytes,error),"160-byte owner guard cannot replace 153-byte consumer extent");
    bytes.resize(153);require(!quest_list::checkbox_original(image+other,bytes,error),"helper cannot admit unrelated entry");
    require(quest_list::inner_codes(m,error)&&dungeon_palette::code(m,error),"both consumers recover after fixture corruption removed");
    std::cout<<"PASS "<<checks<<" Quest/palette authenticated-body checks with actual Framework Backend\n";return 0;
}catch(const std::exception& problem){std::cerr<<problem.what()<<'\n';return 1;}
