bool proposal_owner(Memory& memory,uintptr_t live,std::string& error){
    a::Context context;a::Binding binding;character_definition_identity::Census definitions;character_membership_reader::Users users;
    if(!a::selected_session().current(context,error)||!typed_action_runtime::read_binding(context.snapshot_uuid,binding,error)||
       !character_actor_records::read_current(context,binding,definitions,users,error))return false;
    unsigned owners=0;
    for(const auto& definition:definitions.definitions)if(!definition.deleted){
        uintptr_t data=0;int32_t count=0,capacity=0;
        if(!field(memory,definition.object,0x130,data,error)||!field(memory,definition.object,0x138,count,error)||
           !field(memory,definition.object,0x13c,capacity,error))return false;
        if(count<0||count>10000||capacity<count||(count&&(!data||size_t(count)>(UINTPTR_MAX-data)/8)))
            return fail(error,"Ability old proposal canonical owner array bounds differ");
        for(int32_t i=0;i<count;++i){uintptr_t member=0;if(!field(memory,data,size_t(i)*8,member,error))return false;if(member==live)++owners;}
    }
    return owners==1||fail(error,"Ability old proposal live target is not uniquely owned by the current Character census");
}
bool proposal_release(uintptr_t outer,std::string& error){
    if(!ready||GetCurrentThreadId()!=state.thread)return fail(error,"Ability old proposal requires its ready bound Mainthread");
    Memory memory;uintptr_t ap=0,draft=0,live=0,proxy=0,target=0,reciprocal=0;int32_t refs=0;uint64_t uid=0;
    if(!field(memory,outer,0,ap,error)||ap!=state.image+0x1720f30||!field(memory,outer,0x510,draft,error))
        return fail(error,error.empty()?"Ability old proposal window class differs":error);
    if(!draft)return true;
    if(!field(memory,outer,0x520,live,error)||!field(memory,outer,0x528,proxy,error)||!live||!proxy||
       !field(memory,proxy,0,target,error)||target!=live+0x38||!field(memory,proxy,8,refs,error)||refs<=0||
       !field(memory,live,0x40,reciprocal,error)||reciprocal!=proxy||!field(memory,live,0x18,uid,error))
        return fail(error,error.empty()?"Ability old proposal live weak lease differs":error);
    // SetupForCharacterType has already stored the new owner at +530. Resolve the
    // previous owner from the old weak live target instead of using that new binding.
    if(!code(memory,error)||!proposal_owner(memory,live,error))return false;
    const auto status=proposal_clear_copy(draft,live,uid);
    return !status||fail(error,"Ability old proposal full UID/map ownership failed code="+std::to_string(status));
}
