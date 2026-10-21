namespace dungeon_gizmo {
namespace {
struct RenameText{const char* data=nullptr;size_t size=0;std::array<uint8_t,16> storage{};};
uint32_t rename_original(uintptr_t object,const RenameText& value,bool& returned)noexcept{
    returned=false;__try{reinterpret_cast<void(*)(void*,const RenameText*)>(state.image+0x4cf710)(reinterpret_cast<void*>(object),&value);returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool rename_code(Memory& memory,std::string& error){
    std::array<unsigned char,16> original{};
    return memory.copy(state.image+0x4cf710,original.data(),original.size(),error)&&
        hooks::framework::verify_original_hash(state.image+0x4cf710,original.data(),original.size(),
            "a09973ac6e34d4b78efa50878d5da6dfd7004a87abee0fec1014e20f7119aeaf",error);
}
}
bool rename_target(uintptr_t object,std::string& key,std::string& scene,std::string& error){
    Memory memory;dungeon_gizmo_record::Window owner;dungeon_gizmo_identity::Member member;
    if(!ready||fault||GetCurrentThreadId()!=state.thread||!code(memory,error)||!rename_code(memory,error)||!window(memory,owner,error)||
       owner.target!=object||!owner.uid||!dungeon_scene_identity_runtime::resolve(object,owner.uid,key,error)||
       !dungeon_scene_identity_runtime::lookup(key,member,error)||member.object!=object||member.uid!=owner.uid)
        return fail(error,error.empty()?"Gizmo Rename requires its exact current Info Window/full UID/scene owner":error);
    scene=member.scene;return true;
}
bool rename_read(const a::Binding& binding,const std::string& snapshot,const std::string& key,const std::string& scene,
                 a::DungeonGizmoState& value,dungeon_gizmo_identity::Member& member,a::Ledger& money,
                 build_actions::private_native::UndoWitness& undo,std::string& error){
    Memory memory;Sample current;a::DungeonGizmoIntent target;target.key=key;target.scene=scene;
    if(!ready||fault||GetCurrentThreadId()!=state.thread||!code(memory,error)||!rename_code(memory,error)||
       !sample(memory,binding,snapshot,target,current,error))return fail(error,error.empty()?"Gizmo Rename current native read is unavailable":error);
    value=std::move(current.value);member=current.target;money=current.dungeon.dungeon.dungeon.dungeon.money;
    undo=current.dungeon.dungeon.dungeon.dungeon.undo;return true;
}
bool rename_apply(const a::Binding& binding,const std::string& snapshot,const std::string& key,const std::string& scene,
                  const dungeon_gizmo_identity::Member& expected,const a::DungeonGizmoState& before,const a::Ledger& money,
                  const build_actions::private_native::UndoWitness& undo,const std::string& text,bool& returned,std::string& error){
    returned=false;a::DungeonGizmoState current;dungeon_gizmo_identity::Member member;a::Ledger actual_money;
    build_actions::private_native::UndoWitness actual_undo;
    if(!rename_read(binding,snapshot,key,scene,current,member,actual_money,actual_undo,error))return false;
    if(member!=expected||current!=before||actual_money!=money||actual_undo!=undo)
        return fail(error,"Gizmo Rename complete native target/scene/Undo/Ledger changed before original assignment");
    const RenameText value{text.data(),text.size(),{}};const auto seh=rename_original(member.object,value,returned);
    if(seh||!returned)return fail(error,"Gizmo original Prop Rename did not return SEH="+std::to_string(seh));
    uintptr_t optional_window=0;const auto lookup_seh=find_window(optional_window);
    if(lookup_seh)return fail(error,"Gizmo Rename current Window lookup SEH="+std::to_string(lookup_seh));
    if(!optional_window)return true;
    Memory memory;dungeon_gizmo_record::Window selected_window;
    if(!window(memory,selected_window,error))return false;
    if(selected_window.target!=member.object)return true;
    if(selected_window.uid!=member.uid)return fail(error,"Gizmo Rename current local Header full UID changed");
    const auto* old_record=selected(before,key);if(!old_record)return fail(error,"Gizmo Rename complete before target record is absent");
    auto refreshed=*old_record;refreshed.name=text;bool door=false;
    const auto door_seh=is_door(member.object,door);if(door_seh)return fail(error,"Gizmo Rename current Header IsDoor SEH="+std::to_string(door_seh));
    replaying=true;preparation=&refreshed;preparation_door=door;replay_receiver=selected_window.object;original_calls=0;
    const auto refresh_seh=refresh_window(selected_window.object);const bool failed=callback_failed;reset_replay();
    if(refresh_seh||failed)return fail(error,refresh_seh?"Gizmo Rename original Header refresh SEH="+std::to_string(refresh_seh):cause);
    dungeon_gizmo_record::Window final_window;
    return window(memory,final_window,error)&&
        (final_window.object==selected_window.object&&final_window.target==member.object&&final_window.uid==member.uid||
         fail(error,"Gizmo Rename local Header owner changed after original refresh"));
}
}
