namespace info_refresh=npc_quest_info_refresh;
uint32_t update_info_contents(uintptr_t window)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+0x758d90)(reinterpret_cast<void*>(window));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct InfoRefreshCalls {
    Operation& op;
    Memory memory;
    explicit InfoRefreshCalls(Operation& value):op(value){}
    bool pin(info_refresh::Snapshot& out,bool& eligible,std::string& skip,std::string& e){
        Pin p;action_runtime::ui::LocalBinding local;
        if(GetCurrentThreadId()!=state.thread)return fail(e,"Quest Info refresh requires original Mainthread");
        if(!bind_owner(memory,op.binding,op.request.snapshot_uuid,op.request.canonical_record_key,
                       op.request.npc_quest_list.region,local,p,e))return false;
        if(local.object!=op.owner||!same_world(p.world,op.pin.world))
            return fail(e,"Quest Info refresh current NPC/World differs from confirmed action");
        const auto text=[&](uintptr_t object,size_t offset,std::string& value,std::string& error){
            return npc_quest_records::read_string(memory,object,offset,"Info presentation",value,error);};
        if(!info_refresh::read(memory,text,state.image,local.manager,local.object,
                              int32_t(op.evidence.quest_list_after.records.size()),out,eligible,skip,e))return false;
        if(!eligible)return true;
        native::CameraState camera;
        if(!native::read_camera_state(camera,e))return false;
        const std::array<float,20> values{camera.x,camera.y,camera.z,camera.focus_x,camera.focus_y,camera.focus_z,
            camera.distance,camera.minimum_distance,camera.maximum_distance,camera.desired_distance,camera.zoom_table_value,
            camera.zoom_lower_neighbor,camera.representable_target,camera.edit_weight,camera.focus_blend,camera.focus_end_blend,
            camera.target_x,camera.target_y,camera.target_z,camera.target_distance};
        for(size_t i=0;i<values.size();++i)out.owner.camera[i]=std::bit_cast<uint32_t>(values[i]);
        out.owner.camera[20]=uint32_t(camera.zoom_index);out.owner.camera[21]=uint32_t(camera.target_zoom_index);
        out.owner.camera[22]=camera.focus_pending?1u:0u;out.owner.camera[23]=camera.focus_target_available?1u:0u;
        out.owner.keyboard_focus=reinterpret_cast<uintptr_t>(SDL_GetKeyboardFocus());
        return true;
    }
    bool verify(std::string& e){
        for(const auto& code:npc_quest_list_codes::info_refresh){std::string bytes(code.size,'\0'),sha;
            if(!memory.copy(state.image+code.rva,bytes.data(),bytes.size(),e)||!quest_bridge::sha256(bytes,sha,e))return false;
            if(sha!=code.sha)return fail(e,"Quest Info refresh original body differs RVA="+std::to_string(code.rva));}
        return true;
    }
    bool refresh(uintptr_t window,std::string& e){
        const auto seh=update_info_contents(window);
        return !seh||fail(e,"Quest Info original UpdateContents SEH="+std::to_string(seh));
    }
};
json info_refresh_status(const info_refresh::Receipt& r){
    return {{"checked",r.checked},{"attempted",r.attempted},{"returned",r.returned},{"confirmed",r.confirmed},
        {"skipped",r.skipped},{"skip_reason",r.skip_reason},{"count_before",r.count_before},{"count_after",r.count_after},
        {"displayed_before",r.displayed_before},{"displayed_after",r.displayed_after},
        {"presentation_unchanged",r.presentation_unchanged},{"cause",r.cause}};
}
void refresh_info(Operation& op){
    std::string e;
    try{InfoRefreshCalls calls{op};if(info_refresh::apply(calls,op.info_refresh,e))return;}
    catch(const std::exception& x){e=std::string("Quest Info refresh boundary: ")+x.what();}
    catch(...){e="Quest Info refresh non-standard exception";}
    op.info_refresh.checked=true;
    op.info_refresh.cause=e.empty()?"Quest Info refresh failed without cause":e;
    // The world action is already confirmed. Retain a separate UI failure and
    // never replay its original command to repair a local presentation cache.
    LOG_WARN("[NPCQuestList] action={} local Info refresh failed: {}",op.request.action_uuid,op.info_refresh.cause);
}
