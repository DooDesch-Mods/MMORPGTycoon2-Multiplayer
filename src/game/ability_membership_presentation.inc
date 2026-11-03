bool presentation_owner(Memory& memory,uintptr_t owner,uintptr_t prototype,std::string& error){
    a::Context context;a::Binding binding;character_definition_identity::Census definitions;character_membership_reader::Users users;
    if(!a::selected_session().current(context,error)||!typed_action_runtime::read_binding(context.snapshot_uuid,binding,error)||
       !character_actor_records::read_current(context,binding,definitions,users,error))return false;
    const auto found=std::find_if(definitions.definitions.begin(),definitions.definitions.end(),[&](const auto& row){return row.object==owner&&!row.deleted&&row.uid;});
    if(found==definitions.definitions.end())return fail(error,"Ability presentation owner is absent from the current Character definitions");
    uintptr_t data=0,ap=0;int32_t count=0,capacity=0;uint64_t uid=0;
    if(!field(memory,owner,0x130,data,error)||!field(memory,owner,0x138,count,error)||!field(memory,owner,0x13c,capacity,error))return false;
    if(count<=0||count>10000||capacity<count||!data||size_t(count)>(UINTPTR_MAX-data)/8)return fail(error,"Ability presentation owner array bounds differ");
    unsigned matches=0;
    for(int32_t i=0;i<count;++i){uintptr_t member=0;if(!field(memory,data,size_t(i)*8,member,error))return false;if(member==prototype)++matches;}
    if(matches!=1||!field(memory,prototype,0,ap,error)||ap!=state.image+0x171c930||!field(memory,prototype,0x18,uid,error)||!uid)
        return fail(error,error.empty()?"Ability presentation prototype is not one current owned native definition":error);
    return true;
}
uint32_t presentation_original(void* receiver,void* button,float seconds,bool animate)noexcept{
    __try{original_dock(receiver,button,seconds,animate);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t presentation_restore(uintptr_t prototype,uintptr_t proxy,int32_t display_slot,bool& restored)noexcept{
    restored=false;
    __try{
        if(*reinterpret_cast<const uintptr_t*>(prototype)!=state.image+0x171c930||
           *reinterpret_cast<const uintptr_t*>(prototype+0x40)!=proxy||
           *reinterpret_cast<const uintptr_t*>(proxy)!=prototype+0x38)return 0;
        *reinterpret_cast<int32_t*>(prototype+0x208)=display_slot;
        restored=*reinterpret_cast<const int32_t*>(prototype+0x208)==display_slot;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool presentation_dock(Memory& memory,void* receiver,void* button,float seconds,bool animate,
    uintptr_t owner,uintptr_t prototype,bool prefab,std::string& error){
    if(!ready||fault||GetCurrentThreadId()!=state.thread||prefab||!code(memory,error)||!presentation_owner(memory,owner,prototype,error))
        return fail(error,error.empty()?"Ability presentation docking requires its current native owner/thread/code":error);
    int32_t level=-1,slot_level=-1,display_slot=-2;uintptr_t proxy=0;
    if(!field(memory,prototype,0x200,level,error)||!field(memory,uintptr_t(receiver),0x34c,slot_level,error)||
       !field(memory,prototype,0x208,display_slot,error)||!field(memory,prototype,0x40,proxy,error))return false;
    if(level<0||level!=slot_level||display_slot< -1||!proxy)return fail(error,"Ability presentation docking would change the native ability level");
    // Stock SetupFor docks fresh icons through this exact caller. Its display-slot
    // write is local presentation work; retain the canonical serialized value.
    const auto original_exception=presentation_original(receiver,button,seconds,animate);
    bool restored=false;const auto cleanup_exception=presentation_restore(prototype,proxy,display_slot,restored);
    if(original_exception||cleanup_exception||!restored)return fail(error,"Ability presentation docking/cleanup failed original_seh="+
        std::to_string(original_exception)+" cleanup_seh="+std::to_string(cleanup_exception)+" display_slot_restored="+std::to_string(restored));
    uintptr_t docked=0;int32_t actual_level=-1,actual_slot=-2;
    if(!field(memory,uintptr_t(receiver),0x380,docked,error)||docked!=uintptr_t(button)||
       !field(memory,prototype,0x200,actual_level,error)||actual_level!=level||
       !field(memory,prototype,0x208,actual_slot,error)||actual_slot!=display_slot)
        return fail(error,error.empty()?"Ability presentation original docking/readback differs":error);
    return true;
}
