namespace boss_placement {
namespace {
bool pristine(Runtime& r,const boss_placement_code::Guard& guard,std::string& error){
    std::array<unsigned char,32> actual{};
    if(!r.memory.read(r.image+guard.rva,actual.data(),actual.size(),error))return false;
    if(actual==guard.bytes)return true;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    return hooks::framework::verify_code(r.image+guard.rva,guard.bytes.data(),guard.bytes.size(),error);
#else
    return q::fail(error,"Boss original native helper differs RVA="+std::to_string(guard.rva));
#endif
}
bool code(Runtime& r,bool enabled,std::string& error){
    for(const auto& helper:boss_placement_code::helpers)if(!pristine(r,helper,error))return false;
    const auto& route=boss_placement_code::release;
    if(!enabled)return pristine(r,route,error);
    if(!installed||!original_release)return q::fail(error,"Boss original release route is not installed");
#ifdef MT2MP_FRAMEWORK_PLUGIN
    auto& backend=hooks::framework::process_backend();MT2FW_MT2_RouteReceipt receipt{};
    if(!backend.read_route(reinterpret_cast<void*>(r.image+route.rva),receipt))return q::fail(error,"Boss owned release route read failed: "+backend.error());
    if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.entry!=r.image+route.rva||receipt.detour!=uintptr_t(&release_entry)||
       receipt.trampoline!=uintptr_t(original_release)||!receipt.generation)return q::fail(error,"Boss current owned release route receipt differs");
    return hooks::framework::verify_code(r.image+route.rva,route.bytes.data(),route.bytes.size(),error);
#else
    std::array<unsigned char,32> live{};std::array<unsigned char,21> trampoline{};unsigned char relay[14]{};
    if(!r.memory.read(r.image+route.rva,live.data(),live.size(),error)||!r.memory.read(uintptr_t(original_release),trampoline.data(),trampoline.size(),error))return false;
    if(live[0]!=0xe9||std::memcmp(live.data()+5,route.bytes.data()+5,route.bytes.size()-5)||std::memcmp(trampoline.data(),route.bytes.data(),7)||
       std::memcmp(trampoline.data()+7,"\xff\x25\0\0\0\0",6))return q::fail(error,"Boss own release/trampoline physical bytes differ");
    uintptr_t returned=0;std::memcpy(&returned,trampoline.data()+13,8);int32_t displacement=0;std::memcpy(&displacement,live.data()+1,4);
    if(returned!=r.image+route.rva+7||!r.memory.read(uintptr_t(int64_t(r.image+route.rva+5)+displacement),relay,sizeof relay,error))
        return q::fail(error,error.empty()?"Boss own original release trampoline return differs":error);
    uintptr_t target=0;std::memcpy(&target,relay+6,8);
    return (!std::memcmp(relay,"\xff\x25\0\0\0\0",6)&&target==uintptr_t(&release_entry))||q::fail(error,"Boss own release relay target differs");
#endif
}
}
bool enable(std::string& error){
    auto* r=published.load(std::memory_order_acquire);
    if(installed)return r&&code(*r,true,error);
    if(install_attempted||!r||!r->ready||r->thread!=GetCurrentThreadId())return q::fail(error,"Boss release requires one installation on the ready Mainthread");
    install_attempted=true;if(!code(*r,false,error)){reject(error);return false;}
    void* original=nullptr;const auto target=reinterpret_cast<void*>(r->image+boss_placement_reader::release_rva);
    if(!hooks::create_hook(target,reinterpret_cast<void*>(&release_entry),&original)||!original){error="Boss original release CreateHook failed";reject(error);return false;}
    original_release=reinterpret_cast<Release>(original);const auto enabled=MH_EnableHook(target);
    if(enabled!=MH_OK){error="Boss original release EnableHook failed: "+std::string(MH_StatusToString(enabled));reject(error);return false;}
    installed=true;
    if(!code(*r,true,error)){const auto disabled=MH_DisableHook(target);installed=false;
        if(disabled!=MH_OK)error+="; Boss release disable failed: "+std::string(MH_StatusToString(disabled));reject(error);return false;}
    capture_cause.clear();return true;
}
}
