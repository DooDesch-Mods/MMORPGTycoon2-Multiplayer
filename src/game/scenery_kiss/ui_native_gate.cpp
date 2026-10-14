#include "ui_native_gate.h"
#include "ui_codeguards.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include <Windows.h>
#include <cstring>
#include <limits>
namespace mt2mp::scenery_kiss {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool equals(native::Memory& m,uintptr_t p,const unsigned char* expected,size_t n,std::string& e){
    std::vector<unsigned char> actual(n);if(!m.copy(p,actual.data(),n,e))return false;
    return std::memcmp(actual.data(),expected,n)==0||fail(e,"scenery UI exact native bytes differ");
}
bool allocation(uintptr_t address,void*& owner,std::string& e){
    MEMORY_BASIC_INFORMATION info{};
    if(VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof info)!=sizeof info||info.State!=MEM_COMMIT||info.Type!=MEM_PRIVATE||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return fail(e,"scenery UI original allocation is not retained executable memory");
    owner=info.AllocationBase;return true;
}
uint32_t permission_calls(uintptr_t image,uintptr_t art,const void* filename,uintptr_t shard,const void* position,int32_t category,bool& locked,uintptr_t& region,int32_t& cost){
    __try{
        locked=reinterpret_cast<bool(__fastcall*)(void*,const void*)>(image+0x3cbe0)(reinterpret_cast<void*>(art),filename);
        if(locked)return 0;
        region=uintptr_t(reinterpret_cast<void*(__fastcall*)(void*,const void*,void*)>(image+0x5089b0)(reinterpret_cast<void*>(shard),position,nullptr));
        cost=reinterpret_cast<int32_t(__fastcall*)(int32_t)>(image+0x4470b0)(category);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool validate_ui_bytes(native::Memory& memory,uintptr_t image,const std::array<uintptr_t,6>& originals,const std::array<uintptr_t,6>& entries,bool enabled,std::string& e){
    if(!image||image>(std::numeric_limits<uintptr_t>::max)()-0x17396b0)return fail(e,"scenery UI image range invalid");
    for(size_t i=0;i<6;++i){const auto& c=ui_codes[i];const auto target=image+c.rva;
        if(!originals[i]||!equals(memory,originals[i],c.bytes,c.span,e))return false;
        constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};uintptr_t continuation=0;
        if(!equals(memory,originals[i]+c.span,jump,sizeof jump,e)||!memory.copy(originals[i]+c.span+6,&continuation,8,e))return false;
        if(continuation!=target+c.span)return fail(e,"scenery UI trampoline continuation differs");
        if(!enabled){if(!equals(memory,target,c.bytes,c.size,e))return false;continue;}
        std::array<unsigned char,5> route{};if(!memory.copy(target,route.data(),route.size(),e))return false;
        if(route[0]!=0xe9)return fail(e,"scenery UI physical target route differs");
        int32_t displacement=0;std::memcpy(&displacement,route.data()+1,4);
        const auto relay=uintptr_t(int64_t(target+5)+displacement);uintptr_t entry=0;
        if(!equals(memory,relay,jump,sizeof jump,e)||!memory.copy(relay+6,&entry,8,e))return false;
        if(!entries[i]||entry!=entries[i])return fail(e,"scenery UI relay owner differs");
        if(!equals(memory,target+5,c.bytes+5,c.size-5,e))return false;
    }
    for(size_t i=6;i<std::size(ui_codes);++i){const auto& c=ui_codes[i];if(!equals(memory,image+c.rva,c.bytes,c.size,e))return false;}
    return true;
}
bool validate_ui_originals(uintptr_t image,std::string& e){
    native::host::ProcessMemory memory;for(const auto& c:ui_codes)if(!equals(memory,image+c.rva,c.bytes,c.size,e))return false;return true;
}
bool validate_ui_trampolines(uintptr_t image,const std::array<uintptr_t,6>& originals,std::string& e){
    native::host::ProcessMemory memory;for(const auto original:originals){void* owner=nullptr;if(!allocation(original,owner,e))return false;}
    return validate_ui_bytes(memory,image,originals,{},false,e);
}
bool validate_ui_routes(uintptr_t image,const std::array<uintptr_t,6>& originals,const std::array<uintptr_t,6>& entries,std::string& e){
    native::host::ProcessMemory memory;if(!validate_ui_bytes(memory,image,originals,entries,true,e))return false;
    for(size_t i=0;i<6;++i){int32_t displacement=0;void *first=nullptr,*second=nullptr;
        if(!memory.copy(image+ui_codes[i].rva+1,&displacement,4,e))return false;
        const auto relay=uintptr_t(int64_t(image+ui_codes[i].rva+5)+displacement);
        if(!allocation(originals[i],first,e)||!allocation(relay,second,e))return false;
        if(first!=second)return fail(e,"scenery UI relay and original allocation owner differ");}
    return true;
}
bool create_permission(native::host::ProcessMemory& memory,uintptr_t image,uintptr_t shard,const State& state,int32_t category,uintptr_t& manager,int32_t& cost,std::string& e){
    manager=0;cost=-1;if(!shard||category<0||category>=47)return fail(e,"scenery Create actual world or category invalid");
    if(!valid_state(state,e))return false;
    for(size_t i=6;i<std::size(ui_codes);++i){const auto& c=ui_codes[i];if(!equals(memory,image+c.rva,c.bytes,c.size,e))return false;}
    uintptr_t art=0;if(!memory.copy(image+0x13df2d0,&art,8,e))return false;
    if(!art)return fail(e,"scenery actual ArtLibrary unavailable");
    struct String {const char* data;size_t size;unsigned char rest[16];} filename{state.variant.data(),state.variant.size(),{}};
    const std::array<float,3> position{state.x,state.y,state.z};bool locked=true;uintptr_t region=0;
    const auto seh=permission_calls(image,art,&filename,shard,position.data(),category,locked,region,cost);
    if(seh){e="scenery native permission failed SEH="+std::to_string(seh);return false;}
    if(locked)return fail(e,"scenery actual variant locked");
    if(!region||cost<0)return fail(e,"scenery actual region or price unavailable");
    return permission_binding(memory,image,shard,region,art,manager,e);
}
bool permission_binding(native::Memory& memory,uintptr_t image,uintptr_t shard,uintptr_t region,uintptr_t art,uintptr_t& manager,std::string& e){
    manager=0;if(!image||!shard||!region||!art)return fail(e,"scenery actual permission binding missing");
    unsigned char owned=0;uintptr_t ap=0,manager_region=0,manager_shard=0,art_after=0;
    if(!memory.copy(region+0x850,&owned,1,e)||!memory.copy(region+0x960,&manager,8,e))return false;
    if(owned!=1||!manager)return fail(e,"scenery actual region not owned or manager missing");
    if(!memory.copy(manager,&ap,8,e)||!memory.copy(manager+0x900,&manager_region,8,e)||!memory.copy(manager+0x908,&manager_shard,8,e)||!memory.copy(image+0x13df2d0,&art_after,8,e))return false;
    if(ap!=image+0x17152d0||manager_region!=region||manager_shard!=shard||art_after!=art)return fail(e,"scenery actual permission ownership changed");
    return true;
}
}
