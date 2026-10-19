bool gm_route_code(Runtime& r,std::string& error){
    for(const auto& route:r.gm_routes){const auto& owner=route.owner;MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"GM route read original="+hooks::framework::process_backend().error());
        if(actual.state!=MT2FW_MT2_ROUTE_ENABLED||actual.generation!=owner.generation||actual.entry!=owner.entry||actual.trampoline!=owner.trampoline||actual.detour!=owner.detour||std::strcmp(actual.owner,"doodesch.multiplayer"))return fail(error,"GM actual named route owner/generation differs");
        if(!release_ui_owner::verify(owner,[&](auto address,auto out,auto bytes,auto& cause){return r.memory.copy(address,out,bytes,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,error)||
           !hooks::framework::process_backend().verify_code(owner.entry,owner.pristine.data(),owner.pristine.size(),error))return false;
    }return true;
}
bool gm_hook_owner(Runtime& r,uintptr_t object,std::string& error){
    if(GetCurrentThreadId()!=r.thread||!r.gm_context||!r.gm_binding)return fail(error,"GM Client hook has no saved Mainthread Context/Binding");
    a::Context current;if(!a::selected_session().current(current,error)||current!=*r.gm_context)return fail(error,error.empty()?"GM Client hook Session changed":error);
    uintptr_t lounge=0,data=0,member=0,root=0;int32_t count=0,slot=-1;uint64_t uid=0;
    if(!ap(r,object,0x16e5070,error)||!at(r,object,0x18,uid,error)||!uid||!at(r,object,0x4ac,slot,error)||slot<0||slot>=50||
       !at(r,r.image,0x13df5e0,lounge,error)||!ap(r,lounge,0x171e600,error)||!at(r,lounge,0x40,data,error)||!at(r,lounge,0x48,count,error)||count!=50||
       !at(r,data,size_t(slot)*8,member,error)||member!=object||!at(r,object,0x170,root,error))return fail(error,error.empty()?"GM Client hook actual fixed-slot owner differs":error);
    uintptr_t registered=0;const auto exception=uid_lookup(r.image,uid,registered);
    if(exception||registered!=object)return fail(error,"GM Client hook FindUID differs SEH="+std::to_string(exception));
    if(root&&root!=r.gm_root)return fail(error,"GM Client hook current native world root differs");return true;
}
void gm_hook_entry(size_t index,void* receiver,float delta){auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->gm_routes[index].original)throw std::runtime_error("GM original hook was not published");
    if(!r->gm_client){r->gm_routes[index].original(receiver,delta);return;}
    if(r->gm_suspended||r->gm_failed)return;
    std::string error;if(!std::isfinite(delta)||delta<0||!gm_hook_owner(*r,uintptr_t(receiver),error)){
        r->gm_failed=true;r->gm_cause=error.empty()?"GM Client native update delta is invalid":error;reject(*r,r->gm_cause);return;}
    if(index==0)return;
    const auto exception=gm_ticket::native::presentation(r->image,uintptr_t(receiver),delta);
    if(exception){r->gm_failed=true;r->gm_cause="GM Client original FlyingArrow presentation SEH="+std::to_string(exception);reject(*r,r->gm_cause);}
}
void gm_tick_entry(void* receiver,float delta){gm_hook_entry(0,receiver,delta);}
void gm_entity_entry(void* receiver,float delta){gm_hook_entry(1,receiver,delta);}
bool gm_enable(Runtime& r,std::string& error){const GmUpdate detours[2]{&gm_tick_entry,&gm_entity_entry};
    for(size_t i=0;i<2;++i){auto& route=r.gm_routes[i];auto& owner=route.owner;owner.entry=r.image+route.rva;owner.detour=uintptr_t(detours[i]);owner.stolen=route.stolen;owner.pristine.resize(route.bytes);
        if(!r.memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),error)||mt2mp::sha256(owner.pristine)!=route.sha)return fail(error,error.empty()?"GM complete original function body differs":error);
        const auto prepared=MH_CreateHook(reinterpret_cast<void*>(owner.entry),reinterpret_cast<void*>(owner.detour),reinterpret_cast<void**>(&route.original));
        if(prepared!=MH_OK||!route.original)return fail(error,"GM named route prepare failed status="+std::to_string(prepared)+" original="+hooks::framework::process_backend().error());owner.trampoline=uintptr_t(route.original);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK)return fail(error,"GM named route enable failed status="+std::to_string(enabled)+" original="+hooks::framework::process_backend().error());
        MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"GM named route receipt failed original="+hooks::framework::process_backend().error());owner.generation=actual.generation;
    }return gm_route_code(r,error);
}
