namespace world_quest_history {
bool references(const action_session::Binding& binding,const std::string& snapshot,
                const monster_quest_retirement::Witness& witness,json& out,std::string& error){
    const auto& runtime=monster_action_runtime::state;
    if(!runtime.ready||runtime.thread!=GetCurrentThreadId())
        return fail(error,"World Quest references require bound Mainthread");
    Memory memory;host::LookupApi api;host::WorldState before,after;host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,runtime.image,exe_sha,api,error))return false;
    host::Reader reader(memory.process,origin,runtime.image,runtime.thread,api);
    monster_action_runtime::quest_list::References current;
    if(!reader.current(before,true,error)||!action_runtime::bound(before,binding,snapshot,error)||
       !monster_action_runtime::quest_list::references(memory,binding,snapshot,witness,current,error)||
       !reader.current(after,true,error)||!action_runtime::same_world(before,after))
        return fail(error,error.empty()?"World Quest references changed current full World":error);
    out=std::move(current.rows);error.clear();return true;
}
bool read(const action_session::Binding& binding,const std::string& snapshot,monster_quest_retirement::Witness& out,
          std::string& e,const monster_quest_retirement::Witness* previous){
    const auto& runtime=monster_action_runtime::state;
    if(!runtime.ready||runtime.thread!=GetCurrentThreadId())return fail(e,"World history Quest read requires bound Mainthread");
    Memory memory;host::LookupApi api;host::WorldState before,after;host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,runtime.image,exe_sha,api,e))return false;
    host::Reader reader(memory.process,origin,runtime.image,runtime.thread,api);
    if(!reader.current(before,true,e)||!action_runtime::bound(before,binding,snapshot,e)||
       !monster_action_runtime::read_remove_quests(memory,binding,snapshot,0,"",out,e,previous)||
       !reader.current(after,true,e)||!action_runtime::same_world(before,after))
        return fail(e,e.empty()?"World history Quest read current full World changed":e);
    return true;
}
}
