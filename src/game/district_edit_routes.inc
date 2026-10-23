namespace {
struct Route {uintptr_t rva;size_t bytes;const char* sha;void* original=nullptr;void* detour=nullptr;uint64_t generation=0;};
std::array<Route,10> routes{{
#include "district_edit_route_pins.inc"
}};
bool code(Runtime& runtime,std::string& error){
    for(const auto& route:routes){
        std::vector<unsigned char> bytes(route.bytes);
        if(!runtime.memory.read(runtime.image+route.rva,bytes.data(),bytes.size(),error))return false;
        if(installed){
            MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
            if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(runtime.image+route.rva),receipt)){
                error="District property named route receipt: "+hooks::framework::process_backend().error();return false;}
            if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.entry!=runtime.image+route.rva||receipt.trampoline!=uintptr_t(route.original)||
               receipt.detour!=uintptr_t(route.detour)||receipt.generation!=route.generation||std::strcmp(receipt.owner,"doodesch.multiplayer")){
                error="District property route owner/generation/original differs";return false;}
        }
        if(!hooks::framework::verify_original_hash(runtime.image+route.rva,bytes.data(),bytes.size(),route.sha,error))return false;
    }
    struct Body{uintptr_t rva;size_t bytes;const char* sha;};
    const Body helpers[]{
#include "district_edit_helper_pins.inc"
    };
    for(const auto& body:helpers){std::vector<unsigned char> bytes(body.bytes);
        if(!runtime.memory.read(runtime.image+body.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(runtime.image+body.rva,bytes.data(),bytes.size(),body.sha,error))return false;}
    return true;
}
}
bool enable(std::string& error){
    if(installed)return true;
    if(install_attempted){error="District property previous route installation failed: "+install_cause;return false;}
    install_attempted=true;auto* runtime=published.load(std::memory_order_acquire);
    auto fail_install=[&](){install_cause=error;return false;};
    if(!runtime||!runtime->image||runtime->thread!=GetCurrentThreadId()){error="District property install requires bound Mainthread";return fail_install();}
    if(!code(*runtime,error))return fail_install();
    const std::array<void*,10> entries{reinterpret_cast<void*>(&begin_entry),reinterpret_cast<void*>(&end_entry),reinterpret_cast<void*>(&cancel_entry),
        reinterpret_cast<void*>(&spinner_entry),reinterpret_cast<void*>(&color_entry),reinterpret_cast<void*>(&spinner_float_entry),
        reinterpret_cast<void*>(&spinner_drag_thunk),reinterpret_cast<void*>(&spinner_click_thunk),reinterpret_cast<void*>(&spinner_click_entry),reinterpret_cast<void*>(&spinner_drag_entry)};
    for(size_t i=0;i<routes.size();++i){auto& route=routes[i];route.detour=entries[i];
        const auto target=reinterpret_cast<void*>(runtime->image+route.rva);
        const auto created=MH_CreateHook(target,route.detour,&route.original);
        if(created!=MH_OK||!route.original){error="District named route create failed status="+std::to_string(created)+" "+hooks::framework::process_backend().error();return fail_install();}
    }
    begin_original=reinterpret_cast<Notice>(routes[0].original);end_original=reinterpret_cast<Notice>(routes[1].original);cancel_original=reinterpret_cast<Notice>(routes[2].original);
    spinner_original=reinterpret_cast<Spinner>(routes[3].original);color_original=reinterpret_cast<Color>(routes[4].original);
    for(size_t i=0;i<additional_spinners.size();++i)additional_spinners[i]=routes[i+5].original;
    const bool batch=hooks::framework::process_backend().batch_available();
    if(batch&&!hooks::enable_prepared_routes(runtime->image,routes,"District named route",error))return fail_install();
    for(auto& route:routes){const auto target=reinterpret_cast<void*>(runtime->image+route.rva);
        if(!batch){const auto result=MH_EnableHook(target);if(result!=MH_OK){
            error="District named route enable failed status="+std::to_string(result)+" "+hooks::framework::process_backend().error();return fail_install();}}
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
        if(!hooks::framework::process_backend().read_route(target,receipt)){
            error="District named route receipt failed: "+hooks::framework::process_backend().error();return fail_install();}
        route.generation=receipt.generation;
    }
    installed=true;if(!code(*runtime,error)){installed=false;return fail_install();}return true;
}
