namespace scenery_stroke {
namespace {
std::array<uintptr_t,4> detours(){return {uintptr_t(&brush_paint_entry),uintptr_t(&brush_finish_entry),uintptr_t(&erase_paint_entry),uintptr_t(&erase_finish_entry)};}
bool original_bytes(Runtime& r,const scenery_stroke_code::Guard& guard,std::string& e){
    std::vector<unsigned char> actual(guard.size);
    if(!r.memory.read(r.image+guard.rva,actual.data(),actual.size(),e))return false;
    if(!std::memcmp(actual.data(),guard.bytes,guard.size))return true;
    return hooks::framework::verify_code(r.image+guard.rva,guard.bytes,guard.size,e);
}
bool verify_routes(Runtime& r,bool enabled,std::string& e){
    const auto targets=detours();
    for(size_t index=0;index<std::size(scenery_stroke_code::routes);++index){const auto& route=scenery_stroke_code::routes[index];
        if(!enabled){if(!original_bytes(r,route,e))return false;continue;}
        if(!originals[index])return q::fail(e,"Scenery original route trampoline is absent index="+std::to_string(index));
        auto& backend=hooks::framework::process_backend();MT2FW_MT2_RouteReceipt receipt{};
        if(!backend.read_route(reinterpret_cast<void*>(r.image+route.rva),receipt))return q::fail(e,"Scenery owned route read failed: "+backend.error());
        if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.entry!=r.image+route.rva||receipt.detour!=targets[index]||
           receipt.trampoline!=originals[index]||!receipt.generation)
            return q::fail(e,"Scenery current owned route receipt differs index="+std::to_string(index));
        if(!hooks::framework::verify_code(r.image+route.rva,route.bytes,route.size,e))return false;
    }
    for(const auto& guard:scenery_stroke_code::replay_helpers)if(!original_bytes(r,guard,e))return false;
    return true;
}
}
bool enable(std::string& e){
    auto* r=published.load(std::memory_order_acquire);
    if(installed)return r&&verify_routes(*r,true,e);
    if(installation_attempted||!r||!r->ready||r->thread!=GetCurrentThreadId())return q::fail(e,"Scenery stroke routes require one installation on the ready Mainthread");
    installation_attempted=true;
    if(!verify_routes(*r,false,e)){reject(e);return false;}
    const auto targets=detours();std::array<bool,4> enabled{};
    auto failed=[&](){
        for(size_t index=0;index<enabled.size();++index)if(enabled[index]){
            const auto result=MH_DisableHook(reinterpret_cast<void*>(r->image+scenery_stroke_code::routes[index].rva));
            if(result!=MH_OK)e+="; Scenery owned route disable index="+std::to_string(index)+": "+MH_StatusToString(result);
        }
        reject(e);return false;
    };
    for(size_t index=0;index<targets.size();++index){void* original=nullptr;const auto target=reinterpret_cast<void*>(r->image+scenery_stroke_code::routes[index].rva);
        if(!hooks::create_hook(target,reinterpret_cast<void*>(targets[index]),&original)||!original){e="Scenery stroke CreateHook failed index="+std::to_string(index);return failed();}
        originals[index]=uintptr_t(original);
        const auto result=MH_EnableHook(target);
        if(result!=MH_OK){e="Scenery stroke EnableHook failed index="+std::to_string(index)+": "+MH_StatusToString(result);return failed();}
        enabled[index]=true;
    }
    if(!verify_routes(*r,true,e))return failed();installed=true;cause.clear();return true;
}
bool bulk(uintptr_t receiver,bool eraser,bool& handled,std::string& e){
    handled=false;auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||r->thread!=GetCurrentThreadId()||!installed)return true;
    return scenery_stroke_capture::bulk(r->memory,r->image,receiver,eraser,handled,e);
}
}
