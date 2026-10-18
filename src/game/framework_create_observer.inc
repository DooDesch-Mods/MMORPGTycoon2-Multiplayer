void reject(Runtime&,const std::string&);
void framework_create_failure(Runtime& runtime,const char* detail)noexcept {
    runtime.framework_create_failed=true;
    try {if(runtime.scope&&!runtime.scope->scenery)runtime.scope->cause=detail;reject(runtime,detail);}
    catch(...){std::fputs("UIBuild Framework observer failure reporting threw; original outcome is unconfirmed\n",stderr);}
}
bool same_create(const MT2FW_MT2_CreateCall& a,const MT2FW_MT2_CreateCall& b)noexcept {
    return a.invocation==b.invocation&&a.thread==b.thread&&a.manager==b.manager&&a.type==b.type&&a.variant==b.variant&&
        a.pose==b.pose&&a.a==b.a&&a.b==b.b&&a.c==b.c&&a.flag==b.flag;
}
void MT2FW_CALL framework_create_before(void* user,const MT2FW_MT2_CreateCall* call)noexcept {
    if(!user){std::fputs("UIBuild Framework before has no registered owner\n",stderr);return;}
    auto& runtime=*static_cast<Runtime*>(user);
    if(GetCurrentThreadId()!=runtime.thread)return;
    if(!call||call->size<sizeof(*call)||call->thread!=runtime.thread||!call->invocation){framework_create_failure(runtime,"Framework CreateBuilding before identity/thread differs");return;}
    if(runtime.framework_create_depth==runtime.framework_creates.size()){
        ++runtime.framework_create_overflow;framework_create_failure(runtime,"Framework CreateBuilding nesting exceeds64; original is not suppressed");return;
    }
    auto* scope=runtime.scope&&!runtime.scope->scenery?runtime.scope:nullptr;
    runtime.framework_creates[runtime.framework_create_depth++]={*call,scope};
    if(scope)++scope->add_attempted;
}
void MT2FW_CALL framework_create_after(void* user,const MT2FW_MT2_CreateCall* call,void* result,uint32_t original_returned)noexcept {
    if(!user){std::fputs("UIBuild Framework after has no registered owner\n",stderr);return;}
    auto& runtime=*static_cast<Runtime*>(user);
    if(GetCurrentThreadId()!=runtime.thread)return;
    if(runtime.framework_create_overflow){--runtime.framework_create_overflow;return;}
    if(!runtime.framework_create_depth){framework_create_failure(runtime,"Framework CreateBuilding after has no entered pair");return;}
    const auto frame=runtime.framework_creates[--runtime.framework_create_depth];
    if(!call||call->size<sizeof(*call)||!same_create(frame.call,*call)||original_returned>1||
       (frame.scope&&runtime.scope!=frame.scope)){
        framework_create_failure(runtime,"Framework CreateBuilding pair arguments/order/Scope differ");return;
    }
    if(frame.scope&&original_returned){++frame.scope->add_returned;frame.scope->building=result;}
    if(frame.scope&&!original_returned)framework_create_failure(runtime,"Framework CreateBuilding original did not return; no replay");
}
bool framework_subscribe_create(Runtime& runtime,std::string& error){
    const auto& routes=framework_runtime::routes();
    if(runtime.framework_create_token||!routes.create_subscribe||!routes.create_unsubscribe||!routes.route_read){error="Framework CreateBuilding paired subscription is unavailable or already owned";return false;}
    MT2FW_MT2_RouteReceipt receipt{};receipt.size=sizeof(receipt);
    if(routes.route_read(routes.context,"objects.create",&receipt)!=MT2FW_MT2_OK||receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.rva!=0x24ed30||receipt.entry!=runtime.image+0x24ed30||!receipt.detour||!receipt.trampoline||
       std::string_view(receipt.owner,9)!="framework"||receipt.owner[9]||std::string_view(receipt.game_sha256,64)!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||receipt.game_sha256[64]){
        error="Framework CreateBuilding current shared route identity differs";return false;
    }
    const auto token=routes.create_subscribe(routes.context,&framework_create_before,&framework_create_after,&runtime);
    if(!token){error="Framework CreateBuilding paired subscription failed";return false;}
    runtime.framework_create_token=token;runtime.created=true;return true;
}
