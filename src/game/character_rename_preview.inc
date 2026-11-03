namespace preview {
using Attach=void(*)(void*);
using Select=void(*)(void*,void*);
Attach original_attach=nullptr;
uint64_t attach_generation=0,serial=0,scopes=0,redirected=0,selector_token=0;
MT2FW_MT2_ServiceV2 service{};uint32_t selector_thread=0;
bool enabled=false,ingress_fault=false;
std::array<char,160> selector_cause{};
struct Frame {
    alignas(8) std::array<unsigned char,40> random{};
    a::Context context;a::Binding binding;uintptr_t shared=0;
    unsigned info_depth=0,actor_depth=0;bool failed=false;
};
thread_local Frame* active=nullptr;
MT2FW_MT2_Result MT2FW_CALL select_int(void*,const MT2FW_MT2_CharacterPreviewIntCall*,MT2FW_MT2_CharacterPreviewIntSelection*)noexcept;
void attach_entry(void*);
struct Pin {uintptr_t rva;size_t bytes;const char* sha;};
constexpr Pin pins[]{
    {0x9fc370,96,"7041b7ec325591b8d191a54b3ac8d9bd909ee099283d9d210014af498d160c61"},
    {0x9fc090,192,"7a7ab329d2eb747de62d61f791723eef44f3e850a468747f41708e6d08b981e5"},
    {0x6577d0,1056,"2bee69e1eaea954f0f772cc46dce5d42eea03606285d951b0062c39aa3a15126"},
    {0x905d0,176,"48bd32cca95d9fe85040cc170607b4b7606b240e890ce5afb2522b416b4d64b4"},
    {0x321610,2016,"72e0577cde541f669a8cb7d6c8aaf2f5b355293ccd06cead180ef60d79b4afcc"}};
bool route(Memory& memory,uintptr_t rva,uintptr_t detour,uintptr_t trampoline,uint64_t generation,
           const unsigned char* prefix,size_t copied,std::string& error){
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
    if(!quest_list::read_route(state.image+rva,receipt,error)||receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.detour!=detour||
       receipt.trampoline!=trampoline||receipt.generation!=generation)return fail(error,error.empty()?"Character preview exact RNG route ownership differs":error);
    std::array<unsigned char,22> bytes{};uintptr_t resume=0;
    constexpr std::array<unsigned char,6> jump{0xff,0x25,0,0,0,0};
    if(!memory.copy(trampoline,bytes.data(),copied+14,error))return false;
    std::memcpy(&resume,bytes.data()+copied+6,8);
    return (std::equal(prefix,prefix+copied,bytes.begin())&&std::equal(prefix,prefix+copied,receipt.pristine)&&
        std::equal(jump.begin(),jump.end(),bytes.begin()+copied)&&resume==state.image+rva+copied)||
        fail(error,"Character preview exact retained original instruction boundary differs");
}
bool code(Memory& memory,std::string& error){
    for(const auto& pin:pins){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
           !hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Character preview original body differs RVA="+std::to_string(pin.rva)+" original="+error);}
    constexpr unsigned char attach_prefix[]{0x41,0x57,0x41,0x56,0x41,0x55};
    if(enabled&&(!selector_token||!route(memory,0x6577d0,uintptr_t(&attach_entry),uintptr_t(original_attach),attach_generation,attach_prefix,6,error)))return false;
    return true;
}
uint32_t seed_private(void* object,uint64_t seed)noexcept{
    __try{reinterpret_cast<void(*)(void*,uint64_t)>(state.image+0x9fc090)(object,seed);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t call_attach(Attach original,void* pane)noexcept{
    __try{original(pane);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t call_select(Select original,void* view,void* selected)noexcept{
    __try{original(view,selected);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool current(Frame& frame,Memory& memory,std::string& error){
    a::Context context;a::Binding binding;uintptr_t source=0;
    if(GetCurrentThreadId()!=state.thread||!a::selected_session().current(context,error)||
       !typed_action_runtime::read_binding(frame.context.snapshot_uuid,binding,error)||
       !field(memory,state.image,0x164b250,source,error))return false;
    return (context==frame.context&&binding==frame.binding&&source==frame.shared&&
        net::current_snapshot_id()==context.snapshot_uuid&&Config::instance().is_host==(context.role==a::Role::Host))||
        fail(error,"Character preview current Session/Binding/RNG owner changed");
}
bool prepare(Frame& frame,Memory& memory,uintptr_t owner,std::string& error){
    if(GetCurrentThreadId()!=state.thread||!enabled||!preview::code(memory,error)||
       !a::selected_session().current(frame.context,error)||frame.context.snapshot_uuid!=net::current_snapshot_id()||
       Config::instance().is_host!=(frame.context.role==a::Role::Host)||
       !typed_action_runtime::read_binding(frame.context.snapshot_uuid,frame.binding,error)||
       !field(memory,state.image,0x164b250,frame.shared,error)||!frame.shared)
        return fail(error,error.empty()?"Character preview requires its actual current Mainthread/Session/RNG owner":error);
    const auto seh=seed_private(frame.random.data(),++serial^uint64_t(owner));
    if(seh)return fail(error,"Character preview private native RNG initialization SEH="+std::to_string(seh));
    ++scopes;return true;
}
struct Scope {
    Frame* previous=nullptr;Frame& frame;bool info=false;
    Scope(Frame& value,bool is_info):previous(active),frame(value),info(is_info){active=&frame;if(info)++frame.info_depth;else ++frame.actor_depth;}
    ~Scope(){if(info)--frame.info_depth;else --frame.actor_depth;active=previous;}
};
bool selected(){return enabled&&net::session_wire::connected()&&ordered_mode();}
MT2FW_MT2_Result selector_failure(MT2FW_MT2_CharacterPreviewIntSelection* out,const char* message)noexcept{
    ingress_fault=true;fault=true;if(active)active->failed=true;
    selector_cause.fill(0);size_t count=0;while(count<selector_cause.size()-1&&message[count])++count;std::memcpy(selector_cause.data(),message,count);
    if(out){*out={};out->size=sizeof(*out);std::memcpy(out->cause,selector_cause.data(),selector_cause.size());}
    return MT2FW_MT2_FAILED;
}
MT2FW_MT2_Result MT2FW_CALL select_int(void* user,const MT2FW_MT2_CharacterPreviewIntCall* call,MT2FW_MT2_CharacterPreviewIntSelection* out)noexcept{
    try{
        if(user!=&service||!call||!out||call->size!=sizeof(*call)||call->reserved||call->reserved2)
            return selector_failure(out,"Character preview selector ABI/owner differs");
        *out={};out->size=sizeof(*out);auto* frame=active;
        if(!frame||call->thread!=state.thread||GetCurrentThreadId()!=state.thread)return MT2FW_MT2_UNAVAILABLE;
        const bool category=frame->info_depth&&call->return_rva==0x9061c;
        const bool variant=frame->actor_depth&&call->return_rva==0x321ac0;
        if(!category&&!variant)return MT2FW_MT2_UNAVAILABLE;
        if(ingress_fault||!selector_token)return selector_failure(out,"Character preview selector has a retained ingress failure or no registration");
        Memory memory;std::string error;
        if(!current(*frame,memory,error))return selector_failure(out,error.c_str());
        if(uintptr_t(call->receiver)!=frame->shared)return MT2FW_MT2_UNAVAILABLE;
        if(call->limit<=0)return selector_failure(out,"Character preview selector native range is invalid");
        out->source=frame->random.data();out->source_bytes=MT2FW_MT2_PREVIEW_RANDOM_BYTES;++redirected;return MT2FW_MT2_OK;
    }catch(const std::exception& exception){return selector_failure(out,exception.what());}
    catch(...){return selector_failure(out,"Character preview selector non-standard exception");}
}
bool actor(void* pane,std::string& error){
    if(ingress_fault)return fail(error,"Character preview ingress has a retained failure; original refused");
    Memory memory;uintptr_t ap=0;
    if(!field(memory,uintptr_t(pane),0,ap,error)||ap!=state.image+0x1712310)
        return fail(error,error.empty()?"Character preview weapon owner is not the actual ActorOrbitPane":error);
    Frame own;auto& frame=active?*active:own;
    if(active?!current(frame,memory,error):!prepare(frame,memory,uintptr_t(pane),error))return false;
    Scope scope(frame,false);const auto seh=call_attach(original_attach,pane);
    if(seh||frame.failed){ingress_fault=true;fault=true;return fail(error,"Character preview original AttachWeapon failed SEH="+std::to_string(seh)+" selector="+selector_cause.data());}
    return current(frame,memory,error);
}
void attach_entry(void* pane)try{
    if(!selected()){original_attach(pane);return;}
    std::string error;if(!actor(pane,error)){fault=true;reject(error);}
}catch(const std::exception& exception){fault=true;reject(std::string("Character preview AttachWeapon boundary: ")+exception.what());}
catch(...){fault=true;reject("Character preview AttachWeapon non-standard boundary exception");}
bool enable(std::string& error){
    Memory memory;if(enabled)return code(memory,error);
    if(!code(memory,error))return false;
    if(!selector_token)return fail(error,"Character preview requires the Framework-owned GetInt selector registration");
    const auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+0x6577d0),reinterpret_cast<void*>(&attach_entry),reinterpret_cast<void**>(&original_attach));
    if(status!=MH_OK||!original_attach)return fail(error,"Character preview AttachWeapon hook prepare status="+std::to_string(status));
    const uintptr_t targets[]{state.image+0x6577d0};
    if(!mt2mp::hooks::enable_prepared_targets(targets,"Character preview owners",error))return false;
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
    if(!quest_list::read_route(targets[0],receipt,error))return false;attach_generation=receipt.generation;
    enabled=true;return code(memory,error);
}
bool bind(const MT2FW_MT2_ServiceV2& api,std::string& error){
    if(selector_token||service.v1.context||api.v1.size!=sizeof(api)||api.v1.version!=MT2FW_MT2_PREVIEW_SERVICE_VERSION||
       !api.v1.context||!api.character_preview_int_bind||!api.character_preview_int_unbind)
        return fail(error,"Character preview Framework V2 capability is absent, malformed or already bound");
    service=api;selector_thread=GetCurrentThreadId();
    const auto result=service.character_preview_int_bind(service.v1.context,&select_int,&service,&selector_token);
    if(result!=MT2FW_MT2_OK||!selector_token)return fail(error,"Character preview selector bind failed result="+std::to_string(result));
    return true;
}
bool retire(std::string& error){
    if(!selector_token)return true;
    if(GetCurrentThreadId()!=selector_thread||active)return fail(error,"Character preview selector retirement has a borrowed scope or different thread");
    const auto result=service.character_preview_int_unbind(service.v1.context,selector_token);
    if(result!=MT2FW_MT2_OK)return fail(error,"Character preview selector retirement unconfirmed result="+std::to_string(result));
    selector_token=0;service={};selector_thread=0;return true;
}
}
bool preview_code(Memory& memory,std::string& error){return preview::code(memory,error);}
bool enable_preview(std::string& error){return preview::enable(error);}
