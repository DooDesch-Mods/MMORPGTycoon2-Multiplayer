bool framework_remove_route(Runtime& runtime,std::string& error){
    const auto& routes=framework_runtime::routes();
    MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof(receipt);
    const auto result=routes.code_verify(routes.context,"objects.remove",0,ui::contract::destroy_props_bytes,uint32_t(sizeof(ui::contract::destroy_props_bytes)),&receipt);
    if(result!=MT2FW_MT2_OK||receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.rva!=ui::contract::destroy_props_rva||receipt.entry!=runtime.services.image+ui::contract::destroy_props_rva||
       !receipt.detour||!receipt.trampoline||std::string_view(receipt.owner,9)!="framework"||receipt.owner[9]||
       std::string_view(receipt.game_sha256,64)!=ui::contract::exe_sha256||receipt.game_sha256[64]){
        error="UI Remove Framework shared physical route proof failed result="+std::to_string(result)+" cause="+std::string(receipt.cause,std::find(receipt.cause,receipt.cause+sizeof(receipt.cause),'\0'));return false;
    }
    if(runtime.framework_remove_token&&(receipt.trampoline!=runtime.publication.trampoline||receipt.detour!=runtime.framework_remove_detour||receipt.generation!=runtime.framework_remove_generation)){
        error="UI Remove Framework shared route changed after authority binding";return false;
    }
    if(!ui::validate_own_enabled_route(runtime.services.memory,runtime.services.image,receipt.trampoline,receipt.detour,error))return false;
    runtime.publication.trampoline=receipt.trampoline;runtime.framework_remove_detour=receipt.detour;runtime.framework_remove_generation=receipt.generation;
    error.clear();return true;
}
MT2FW_MT2_RemoveDisposition MT2FW_CALL framework_remove(void* user,const MT2FW_MT2_RemoveCall* call,MT2FW_MT2_RemoveNext next,void* invocation)noexcept {
    if(!user){std::fputs("UI Remove Framework authority has no registered owner\n",stderr);return MT2FW_MT2_REMOVE_UNKNOWN;}
    auto& runtime=*static_cast<Runtime*>(user);auto& publication=runtime.publication;
    publication.inflight.fetch_add(1,std::memory_order_acq_rel);
    struct Exit {ui::Publication& value;~Exit(){value.inflight.fetch_sub(1,std::memory_order_acq_rel);}} exit{publication};
    try {
        if(!call||call->size<sizeof(*call)||!call->invocation||!next||!invocation||!runtime.framework_remove_token||
           call->thread!=runtime.services.thread||GetCurrentThreadId()!=runtime.services.thread||!call->native_return_slot||call->invocation<=runtime.framework_remove_invocation)
            throw std::runtime_error("UI Remove Framework authority identity/thread/native slot differs");
        runtime.framework_remove_invocation=call->invocation;
        uintptr_t actual_return=0;std::string error;
        if(!runtime.services.memory.copy(call->native_return_slot,&actual_return,sizeof(actual_return),error)||actual_return!=call->native_return_address)
            throw std::runtime_error("UI Remove Framework native return slot/readback differs: "+error);
        FrameworkRemoveInvocation current{*call,next,invocation,runtime.services.framework_invocation};
        runtime.services.framework_invocation=&current;
        struct Restore {Services& services;FrameworkRemoveInvocation* previous;~Restore(){services.framework_invocation=previous;}} restore{runtime.services,current.previous};
        publication.last_original_attempted.store(false,std::memory_order_release);publication.last_original_returned.store(false,std::memory_order_release);
        publication.last_disposition.store(uint8_t(ui::Disposition::Rejected),std::memory_order_release);
        try {ui::dispatch_entry(publication,call->manager,static_cast<ui::NativePropArray*>(call->array),call->native_return_slot,uint32_t(call->thread));}
        catch(...){publication.last_original_attempted.store(current.attempted,std::memory_order_release);publication.last_original_returned.store(current.returned,std::memory_order_release);throw;}
        if(current.attempted)return current.returned&&publication.last_original_returned.load(std::memory_order_acquire)?MT2FW_MT2_REMOVE_FORWARDED:MT2FW_MT2_REMOVE_UNKNOWN;
        if(runtime.capture.status().unknown||publication.last_original_attempted.load(std::memory_order_acquire))return MT2FW_MT2_REMOVE_UNKNOWN;
        const auto disposition=ui::Disposition(publication.last_disposition.load(std::memory_order_acquire));
        return disposition==ui::Disposition::IntentCaptured||disposition==ui::Disposition::Rejected?MT2FW_MT2_REMOVE_SUPPRESSED:MT2FW_MT2_REMOVE_UNKNOWN;
    }catch(const std::exception& problem){try{runtime.capture.record_entry_failure(std::string("UI Remove Framework authority: ")+problem.what());}catch(...){std::fputs("UI Remove Framework authority failure reporting threw; outcome unknown\n",stderr);}}
    catch(...){runtime.capture.record_entry_failure("UI Remove Framework authority nonstandard exception");}
    return MT2FW_MT2_REMOVE_UNKNOWN;
}
