namespace zone_edit_runtime {
namespace {
struct Route {uintptr_t rva;size_t bytes;const char* sha;void* original=nullptr;void* detour=nullptr;uint64_t generation=0;};
std::array<Route,2> routes{{
#include "zone_edit_route_pins.inc"
#include "zone_edit_additional_route_pins.inc"
}};
bool code(Memory& memory,std::string& error){
    if(!za::all_code(memory,true,error))return false;
    uintptr_t refund=0;
    if(!field(memory,za::state.image+0x1745ce0,0x278,refund,error)||refund!=za::state.image+0xe3f510)
        return fail(error,error.empty()?"Zone original zero-refund virtual target differs":error);
    for(const auto& route:routes){
        std::vector<unsigned char> bytes(route.bytes);
        if(!memory.copy(za::state.image+route.rva,bytes.data(),bytes.size(),error))return false;
        if(ready){
            MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
            if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(za::state.image+route.rva),receipt))
                return fail(error,"Zone named route receipt: "+hooks::framework::process_backend().error());
            if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.entry!=za::state.image+route.rva||
               receipt.trampoline!=uintptr_t(route.original)||receipt.detour!=uintptr_t(route.detour)||
               receipt.generation!=route.generation||std::strcmp(receipt.owner,"doodesch.multiplayer"))
                return fail(error,"Zone named route owner/generation/original differs");
        }
        if(!hooks::framework::verify_original_hash(za::state.image+route.rva,bytes.data(),bytes.size(),route.sha,error))return false;
    }
    struct Body{uintptr_t rva;size_t bytes;const char* sha;};
    const Body helpers[]{
#include "zone_edit_helper_pins.inc"
#include "zone_edit_additional_helper_pins.inc"
#include "zone_edit_execution_pins.inc"
    };
    for(const auto& body:helpers){
        std::vector<unsigned char> bytes(body.bytes);
        if(!memory.copy(za::state.image+body.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(za::state.image+body.rva,bytes.data(),bytes.size(),body.sha,error))return false;
    }
    return true;
}
}
bool enable(std::string& error){
    if(ready){Memory memory;return !capture_fault&&!captured&&!queued&&code(memory,error);}
    if(install_attempted)return fail(error,"Zone previous route installation failed: "+install_cause);
    install_attempted=true;
    auto failed=[&](){install_cause=error;return false;};
    if(!za::state.ready||!za::state.image||za::state.thread!=GetCurrentThreadId()){
        error="Zone edit route install requires existing Zone hook owner on bound Mainthread";return failed();}
    Memory memory;if(!code(memory,error))return failed();
    const std::array<void*,2> entries{reinterpret_cast<void*>(&command_entry),reinterpret_cast<void*>(&right_entry)};
    for(size_t index=0;index<routes.size();++index){
        auto& route=routes[index];route.detour=entries[index];
        const auto result=MH_CreateHook(reinterpret_cast<void*>(za::state.image+route.rva),route.detour,&route.original);
        if(result!=MH_OK||!route.original){
            error="Zone named route create failed status="+std::to_string(result)+" "+hooks::framework::process_backend().error();return failed();}
    }
    original_command=reinterpret_cast<Command>(routes[0].original);
    original_right=reinterpret_cast<zone_edit_calls::Press>(routes[1].original);
    const bool batch=hooks::framework::process_backend().batch_available();
    if(batch&&!mt2mp::hooks::enable_prepared_routes(za::state.image,routes,"Zone named routes",error))return failed();
    for(auto& route:routes){
        auto* entry=reinterpret_cast<void*>(za::state.image+route.rva);
        if(!batch){const auto result=MH_EnableHook(entry);if(result!=MH_OK){
            error="Zone named route enable failed status="+std::to_string(result)+" "+hooks::framework::process_backend().error();return failed();}}
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
        if(!hooks::framework::process_backend().read_route(entry,receipt)){
            error="Zone named route receipt failed: "+hooks::framework::process_backend().error();return failed();}
        route.generation=receipt.generation;
    }
    ready=true;if(!code(memory,error)){ready=false;return failed();}return true;
}
}
