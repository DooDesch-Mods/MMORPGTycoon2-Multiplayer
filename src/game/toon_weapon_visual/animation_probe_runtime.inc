bool read_subscriber_animation(int32_t id,nlohmann::json& out,std::string& error){
    out={{"read_only",true},{"serialized_id",id},{"actor_present",false},{"active",nlohmann::json::array()}};
    try{
        auto* runtime=published_runtime.load(std::memory_order_acquire);
        if(!runtime||!runtime->ordered_steps||runtime->thread!=GetCurrentThreadId())
            return toon_weapon_visual::visual_fail(error,"Subscriber animation requires ordered Mainthread Runtime");
        toon_weapon_visual::Lease before,after;toon_weapon_visual::Observation observed;
        if(!weapon_probe_pin(id,before,error))return false;
        MT2FW_MT2_StepReceipt first{},last{};
        if(!runtime->ordered_steps->diagnostic_read(first,error))return false;
        if(!before.toon){out["reason"]="Subscriber has no registered Toon";error.clear();return true;}
        if(!toon_weapon_visual::inspect(before.toon,observed,error))return false;
        const auto& plan=observed.plan;
        if(!observed.actor_present){out["reason"]="Toon has no CharacterActor";error.clear();return true;}
        auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return toon_weapon_visual::visual_read(weapon_memory,base,offset,value,cause);};
        auto owners=[&](std::string& cause){uintptr_t character=0,actor=0,skeleton=0;
            if(!read(before.toon,0x6f0,character,cause)||character!=plan.actor||!read(character,0,actor,cause)||actor!=plan.actor_object||
               !read(actor,0x40,skeleton,cause)||skeleton!=plan.skeleton){if(cause.empty())cause="Animation CharacterActor/Actor/skeleton owner changed";return false;}
            return true;};
        struct Strings:mt2mp::npc_census::Memory {
            placement_ui_probe::Memory& memory;
            explicit Strings(placement_ui_probe::Memory& value):memory(value){}
            bool copy(uintptr_t address,void* target,size_t size,std::string& cause)override{return memory.copy(address,target,size,cause);}
        } strings(weapon_memory);
        auto name=[&](uintptr_t definition,std::string& value,std::string& cause){
            return npc_quest_records::read_string(strings,definition,0x18,"Animation definition name",value,cause);};
        toon_weapon_visual::animation_observation::State first_state,last_state;
        uint32_t backlog=0,deadline=0,backlog_after=0,deadline_after=0;
        if(!owners(error)||!read(before.toon,0xad4,backlog,error)||!read(before.toon,0x2b0,deadline,error)||
           !toon_weapon_visual::animation_observation::sample(read,name,plan.actor,plan.actor_object,first_state,error)||
           !owners(error)||!toon_weapon_visual::animation_observation::sample(read,name,plan.actor,plan.actor_object,last_state,error)||
           !read(before.toon,0xad4,backlog_after,error)||!read(before.toon,0x2b0,deadline_after,error)||
           !weapon_probe_pin(id,after,error)||!runtime->ordered_steps->diagnostic_read(last,error))return false;
        if(before!=after||first_state!=last_state||backlog!=backlog_after||deadline!=deadline_after||
           first.session!=last.session||first.lease!=last.lease||first.world_epoch!=last.world_epoch||
           first.generation!=last.generation||first.completed_tick!=last.completed_tick||first.phase!=last.phase){
            error="Subscriber animation owner, native sample or ordered tick changed during read";return false;
        }
        if(!std::isfinite(std::bit_cast<float>(backlog))||!std::isfinite(std::bit_cast<float>(deadline)))
            return toon_weapon_visual::visual_fail(error,"Subscriber animation backlog or deadline is not finite");
        out["actor_present"]=true;out["toon_address"]=before.toon;out["character_actor_address"]=plan.actor;
        out["actor_address"]=plan.actor_object;out["skeleton_address"]=plan.skeleton;
        out["skeleton_visible"]=observed.skeleton_visible!=0;out["backlog_bits"]=backlog;out["deadline_bits"]=deadline;
        out["completed_tick"]=first.completed_tick;out["step_tick"]=first.tick;out["phase"]=first.phase;
        out["ordered_step_dt_bits"]=first.dt_bits;out["session"]=first.session;out["world_epoch"]=first.world_epoch;
        out["generation"]=first.generation;out["native_epoch"]=before.binding.native_epoch;
        out["native_generation"]=before.binding.native_generation;out["snapshot_uuid"]=before.context.snapshot_uuid;
        out["effective_active_index"]=first_state.effective_index;
        for(const auto& row:first_state.active)out["active"].push_back({{"instance_address",row.instance},{"definition_address",row.definition},
            {"name",row.name},{"catalog_indexes",row.catalog_indexes},{"semantic_aliases",row.semantic_aliases},
            {"time_bits",row.time_bits},{"speed_bits",row.speed_bits},{"duration_bits",row.duration_bits},
            {"duration_positive",std::bit_cast<float>(row.duration_bits)>0},{"mode",row.mode},{"playing",bool(row.playing)},
            {"paused",bool(row.paused)},{"direction",row.direction}});
        error.clear();return true;
    }catch(const std::exception& exception){error=std::string("Subscriber animation read exception: ")+exception.what();return false;}
    catch(...){error="Subscriber animation read nonstandard exception";return false;}
}
