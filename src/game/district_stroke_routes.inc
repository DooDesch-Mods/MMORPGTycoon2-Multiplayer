namespace district_stroke {
namespace {
struct Route {uintptr_t rva;size_t bytes;const char* sha;void* original=nullptr;void* detour=nullptr;uint64_t generation=0;};
std::array<Route,4> routes{{
#include "district_stroke_route_pins.inc"
}};
bool code(Runtime& runtime,std::string& error){
    for(const auto& route:routes){std::vector<unsigned char> bytes(route.bytes);
        if(!runtime.memory.read(runtime.image+route.rva,bytes.data(),bytes.size(),error))return false;
        if(installed){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
            if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(runtime.image+route.rva),receipt))
                return q::fail(error,"District stroke named route receipt: "+hooks::framework::process_backend().error());
            if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.entry!=runtime.image+route.rva||receipt.trampoline!=uintptr_t(route.original)||
               receipt.detour!=uintptr_t(route.detour)||receipt.generation!=route.generation||std::strcmp(receipt.owner,"doodesch.multiplayer"))
                return q::fail(error,"District stroke route owner/generation/original differs");
        }
        if(!hooks::framework::verify_original_hash(runtime.image+route.rva,bytes.data(),bytes.size(),route.sha,error))return false;
    }
    struct Body{uintptr_t rva;size_t bytes;const char* sha;};const Body helpers[]{
#include "district_stroke_helper_pins.inc"
    };
    for(const auto& body:helpers){std::vector<unsigned char> bytes(body.bytes);
        if(!runtime.memory.read(runtime.image+body.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(runtime.image+body.rva,bytes.data(),bytes.size(),body.sha,error))return false;}
    return true;
}
}
bool enable(std::string& error){
    if(installed)return true;
    if(install_attempted)return q::fail(error,"District stroke previous route installation failed: "+install_cause);
    install_attempted=true;auto* runtime=published.load(std::memory_order_acquire);
    auto failed=[&](){install_cause=error;return false;};
    if(!runtime||!runtime->image||runtime->thread!=GetCurrentThreadId()){
        error="District stroke route install requires bound Mainthread";return failed();}
    if(!code(*runtime,error))return failed();
    const std::array<void*,4> entries{reinterpret_cast<void*>(&normal_entry),reinterpret_cast<void*>(&erase_entry),reinterpret_cast<void*>(&add_entry),reinterpret_cast<void*>(&finish_entry)};
    for(size_t i=0;i<routes.size();++i){auto& route=routes[i];route.detour=entries[i];
        const auto created=MH_CreateHook(reinterpret_cast<void*>(runtime->image+route.rva),route.detour,&route.original);
        if(created!=MH_OK||!route.original){error="District stroke named route create failed status="+std::to_string(created)+" "+hooks::framework::process_backend().error();return failed();}
        originals[i]=route.original;
    }
    for(auto& route:routes){const auto target=reinterpret_cast<void*>(runtime->image+route.rva);
        const auto enabled=MH_EnableHook(target);MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
        if(enabled!=MH_OK||!hooks::framework::process_backend().read_route(target,receipt)){
            error="District stroke named route enable/receipt failed status="+std::to_string(enabled)+" "+hooks::framework::process_backend().error();return failed();}
        route.generation=receipt.generation;
    }
    installed=true;if(!code(*runtime,error)){installed=false;return failed();}return true;
}
}
