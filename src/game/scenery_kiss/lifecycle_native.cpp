#include "mt2mp/hooks/original_body.h"
#include "lifecycle_native.h"
#include "lifecycle_codeguards.h"
#include "ui_contract.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::scenery_kiss {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
bool code(native::Memory& memory,uintptr_t image,size_t first,size_t end,std::string& e){
    if(!image||image>UINTPTR_MAX-0xcf3140)return fail(e,"Scenery lifecycle image range invalid");
    for(size_t i=first;i<end;++i){const auto& c=lifecycle_codes[i];std::vector<unsigned char> actual(c.size);if(!memory.copy(image+c.rva,actual.data(),actual.size(),e))return false;
        if(!hooks::framework::verify_original_bytes(image+c.rva,actual.data(),c.bytes,c.size,e)){e="Scenery lifecycle original full code differs RVA="+std::to_string(c.rva)+" original="+e;return false;}}
    return true;
}
uint32_t region_at(uintptr_t image,uintptr_t root,const void* pose,uintptr_t& region){
    __try{region=uintptr_t(reinterpret_cast<void*(__fastcall*)(void*,const void*,void*)>(image+0x5089b0)(reinterpret_cast<void*>(root),pose,nullptr));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool move_code(native::Memory& memory,uintptr_t image,std::string& e){return code(memory,image,0,3,e);}
bool cancel_code(native::Memory& memory,uintptr_t image,std::string& e){return code(memory,image,3,std::size(lifecycle_codes),e);}
bool region_binding(native::Memory& m,uintptr_t image,uintptr_t root,uintptr_t region,uintptr_t& manager,std::string& e){
    manager=0;if(!image||!root||!region||region>UINTPTR_MAX-0x968)return fail(e,"Scenery destination region address invalid");
    uint8_t owned=0;uintptr_t ap=0,actual_region=0,actual_root=0;
    if(!m.copy(region+0x850,&owned,1,e)||!m.copy(region+0x960,&manager,8,e))return false;
    if(owned!=1||!manager||manager>UINTPTR_MAX-0x910)return fail(e,"Scenery destination region not owned or manager unavailable");
    if(!m.copy(manager,&ap,8,e)||!m.copy(manager+0x900,&actual_region,8,e)||!m.copy(manager+0x908,&actual_root,8,e))return false;
    if(ap!=image+0x17152d0||actual_region!=region||actual_root!=root)return fail(e,"Scenery destination manager reciprocal ownership differs");return true;
}
bool move_destination(native::host::ProcessMemory& memory,uintptr_t image,uintptr_t root,const State& state,std::string& e){
    if(!move_code(memory,image,e)||!valid_state(state,e)||!root)return false;
    const std::array<float,3> pose{state.x,state.y,state.z};uintptr_t region=0,manager=0;const auto seh=region_at(image,root,pose.data(),region);
    if(seh){e="Scenery destination native region lookup SEH="+std::to_string(seh);return false;}
    return region_binding(memory,image,root,region,manager,e);
}
bool cancel_before(native::Memory& memory,uintptr_t image,uintptr_t receiver,std::string& e){
    if(!receiver||receiver>UINTPTR_MAX-0xc58)return fail(e,"Scenery cancel receiver address invalid");
    if(!single_press(memory,image,receiver,e))return false;
    uintptr_t graph=0,edit_flag=0;uint8_t painting=1,pending=1,edit_active=0;int32_t added=-1,removed=-1;
    if(!memory.copy(receiver+0xc50,&graph,8,e)||!memory.copy(receiver+0xb48,&painting,1,e)||!memory.copy(receiver+0x6c1,&pending,1,e)||
       !memory.copy(receiver+0xb80,&added,4,e)||!memory.copy(receiver+0xb88,&removed,4,e))return false;
    if(graph||painting||pending||added||removed)return fail(e,"Scenery cancel has active Paint/commit ownership; no effect admitted");
    if(!memory.copy(receiver+0x360,&edit_flag,8,e))return false;
    if(edit_flag&&!memory.copy(edit_flag,&edit_active,1,e))return false;
    if(edit_active)return fail(e,"Scenery cancel retains native world edit ownership; no EndEdit admitted");
    for(size_t i=0;i<2;++i){uintptr_t child=0,ap=0,dtor=0;
        const auto offset=i?0xb58:0xb50;const auto expected_ap=image+(i?0x1705230:0x16dd870);
        if(!memory.copy(receiver+offset,&child,8,e))return false;
        if(!child)continue;
        if(!memory.copy(child,&ap,8,e))return false;
        if(ap!=expected_ap)return fail(e,"Scenery cancel owned child class differs");
        if(!memory.copy(ap+8,&dtor,8,e))return false;
        if(dtor!=image+scenery_owned_dtor_rvas[i])return fail(e,"Scenery cancel owned child destructor contract differs");}
    return true;
}
bool cancel_after(native::Memory& memory,uintptr_t receiver,std::string& e){
    if(!receiver||receiver>UINTPTR_MAX-0xc58)return fail(e,"Scenery cancel after receiver address invalid");
    uintptr_t graph=1,edit_flag=1;uint8_t painting=1;int32_t render_models=-1,added=-1,removed=-1;
    if(!memory.copy(receiver+0xc50,&graph,8,e)||!memory.copy(receiver+0xb48,&painting,1,e)||!memory.copy(receiver+0x380,&render_models,4,e)||
       !memory.copy(receiver+0xb80,&added,4,e)||!memory.copy(receiver+0xb88,&removed,4,e)||!memory.copy(receiver+0x360,&edit_flag,8,e))return false;
    if(graph||painting||render_models||added||removed||edit_flag)return fail(e,"Scenery cancel native Paint graph, edit ownership or preview render models retained");return true;
}
}
