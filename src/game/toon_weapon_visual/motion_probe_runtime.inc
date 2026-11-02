bool read_subscriber_motion(int32_t id,nlohmann::json& out,std::string& error){
    out={{"read_only",true},{"serialized_id",id},{"registered_motion",nullptr}};
    try{
        auto* runtime=published_runtime.load(std::memory_order_acquire);
        if(!runtime||!runtime->ordered_steps||runtime->thread!=GetCurrentThreadId())
            return toon_weapon_visual::visual_fail(error,"Subscriber motion requires ordered Mainthread Runtime");
        toon_weapon_visual::Lease before,after;
        if(!weapon_probe_pin(id,before,error))return false;
        MT2FW_MT2_StepReceipt step_before{},step_after{};
        if(!runtime->ordered_steps->diagnostic_read(step_before,error))return false;
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,error)||!world.map||world.uuid!=before.context.snapshot_uuid||
           world.world_epoch!=before.binding.native_epoch||world.generation!=before.binding.native_generation){
            if(error.empty())error="Subscriber motion current World differs";return false;
        }
        const auto map=uintptr_t(world.map),root=map+0x1c0;
        auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return toon_weapon_visual::visual_read(weapon_memory,base,offset,value,cause);};
        region_saved_identity::Index regions;
        if(!region_saved_identity::capture(weapon_image,map,root,regions,read,error))return false;
        auto world_reader=[&](subscriber_passive::MotionOwner& owner,std::string&){owner.map=map;owner.root=root;return true;};
        auto region_reader=[&](const subscriber_passive::MotionOwner&,int32_t saved,uintptr_t expected,uintptr_t& selected,std::string& cause){
            selected=0;for(const auto& [key,pointer]:regions.saved)if(key==saved)selected=pointer;
            uint8_t owned=0;
            if(!selected||selected!=expected||!read(selected,0x850,owned,cause)||owned!=1){
                if(cause.empty())cause="Subscriber motion saved Region is not currently owned";return false;
            }
            return true;
        };
        auto member=[&](subscriber_passive::MotionOwner& owner,std::string& cause){
            std::vector<uintptr_t> toons;
            if(!subscriber_passive::motion_manager(weapon_memory,read,weapon_image,owner.region,owner.manager,toons,cause))return false;
            if(std::count(toons.begin(),toons.end(),owner.toon)!=1){cause="Subscriber motion Toon is not registered once";return false;}
            return true;
        };
        std::optional<subscriber_passive::Motion> pose;
        subscriber_passive::MotionOwner owner;
        if(!subscriber_passive::motion(weapon_memory,read,weapon_image,before.subscriber,world_reader,region_reader,member,pose,error,&owner)||
           !region_saved_identity::verify(weapon_image,map,root,regions,read,error)||!weapon_probe_pin(id,after,error)||before!=after||
           !runtime->ordered_steps->diagnostic_read(step_after,error)){
            if(error.empty())error="Subscriber motion owner changed during read";return false;
        }
        if(step_before.session!=step_after.session||step_before.lease!=step_after.lease||step_before.world_epoch!=step_after.world_epoch||
           step_before.generation!=step_after.generation||step_before.completed_tick!=step_after.completed_tick||step_before.phase!=step_after.phase)
            return toon_weapon_visual::visual_fail(error,"Subscriber motion ordered tick or lifetime changed during read");
        if(pose){
            if(!before.toon||owner.toon!=before.toon)return toon_weapon_visual::visual_fail(error,"Subscriber motion selected Toon differs from bound owner");
            out["registered_motion"]={{"region",pose->region},{"position",pose->position},{"orientation",pose->orientation},{"scale",pose->scale}};
        }
        out["session"]=step_before.session;out["completed_tick"]=step_before.completed_tick;out["phase"]=step_before.phase;
        out["world_epoch"]=step_before.world_epoch;out["generation"]=step_before.generation;
        out["snapshot_uuid"]=before.context.snapshot_uuid;out["native_epoch"]=before.binding.native_epoch;
        out["native_generation"]=before.binding.native_generation;error.clear();return true;
    }catch(const std::exception& exception){error=std::string("Subscriber motion read exception: ")+exception.what();return false;}
    catch(...){error="Subscriber motion read nonstandard exception";return false;}
}
