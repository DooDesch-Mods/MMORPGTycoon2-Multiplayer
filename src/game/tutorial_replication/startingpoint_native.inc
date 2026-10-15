// Included inside native.cpp's private namespace to reuse its current Context,
// joined-load fence, safe memory port and existing failure latch.
bool same_world(const World& a,const World& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes&&a.world_epoch==b.world_epoch&&a.generation==b.generation;
}
bool startingpoint_code(std::string& e){
    for(const auto& entry:startingpoint_native_entries){std::array<unsigned char,16> actual{};
        if(!memory.copy(image+entry.rva,actual.data(),actual.size(),e))return false;
        if(actual!=entry.bytes)return fail(e,"startingpoint native entry differs RVA="+std::to_string(entry.rva));}
    return true;
}
bool startingpoint_clock(std::string& e){
    native::ClockState clock;if(!native::read_clock(clock,e))return false;
    return clock.time_scale==0&&std::isfinite(clock.pause_scale)&&clock.pause_scale>=0||fail(e,"startingpoint Client requires actual selected Clock0 and finite nonnegative pause scale");
}
DWORD startingpoint_deploy(uintptr_t project){
    __try{reinterpret_cast<Call>(image+0x262f70)(reinterpret_cast<void*>(project));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD startingpoint_destroy(uintptr_t project){
    __try{reinterpret_cast<Call>(image+0x261980)(reinterpret_cast<void*>(project));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct StartingpointCalls {
    const action_session::Context& context;
    const action_session::Binding& binding;
    tutorial_development::StartingpointSnapshot before;
    bool read(tutorial_development::StartingpointSnapshot& out,std::string& e){
        World world;if(!origin(context,binding,world,e))return false;
        nlohmann::json diagnostic;if(!tutorial_development::read_startingpoint(out,diagnostic,e))return false;
        World after;if(!origin(context,binding,after,e))return false;
        if(!same_world(world,out.origin)||!same_world(world,after))return fail(e,"startingpoint full NativeOpen changed during read");
        return startingpoint_clock(e);
    }
    bool same_owners(const tutorial_development::StartingpointSnapshot& after,std::string& e){
        return same_world(before.origin,after.origin)&&before.image==after.image&&before.lounge==after.lounge&&
            before.tech==after.tech&&before.record==after.record&&before.player==after.player&&before.inventory==after.inventory&&
            before.slots==after.slots&&before.release==after.release||fail(e,"startingpoint actual native owners changed");
    }
    bool pin(const startingpoint::Row& expected,bool deploying,std::string& e){
        uintptr_t actual_image=0;uint32_t thread=0;std::string sha;
        if(!native::read_resolver_receipt(actual_image,thread,sha,e))return false;
        if(actual_image!=image||thread!=GetCurrentThreadId()||thread!=mainthread||
           sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
            return fail(e,"startingpoint actual EXE/Mainthread receipt differs");
        if(!verify_commit_entry(image+0x25fb10,e)||!startingpoint_code(e)||!read(before,e))return false;
        if(!before.row||*before.row!=expected||!before.project||!expected.project_present||!before.record)
            return fail(e,"startingpoint native project/record changed before adoption");
        uintptr_t destructor=0;if(!at(image+0x16e48c0,8,destructor,e))return false;
        if(destructor!=image+0x261980)return fail(e,"startingpoint DevProject deleting destructor differs");
        if(deploying){
            if(!before.admitted)return fail(e,"startingpoint adoption preflight rejected: "+before.admission.dump());
            if(std::find(before.items.begin(),before.items.end(),uintptr_t(0))==before.items.end())return fail(e,"startingpoint native inventory has no free slot");
        }
        tutorial_development::StartingpointSnapshot again;
        if(!read(again,e)||!same_owners(again,e))return false;
        if(before.project!=again.project||before.row!=again.row||before.items!=again.items||before.admission!=again.admission)
            return fail(e,"startingpoint native row/owners changed before adoption");
        return true;
    }
    bool project(const startingpoint::Row& target,std::string& e){
        // The native Clock0 gate prevents both DevManager updates. Only the
        // current, already-owned Startingpoint project's Host values are stored.
        return put(before.lounge,0xb0,target.work,e)&&put(before.lounge,0xb4,target.lines,e)&&
            put(before.lounge,0xb8,target.bugs_total,e)&&put(before.lounge,0xbc,target.bugs_known,e)&&put(before.lounge,0xc4,target.desired_workers,e);
    }
    bool deploy(std::string& e){
        const auto code=startingpoint_deploy(before.project);
        if(code)return fail(e,"startingpoint original DevProject Deploy exception="+std::to_string(code));
        tutorial_development::StartingpointSnapshot after;if(!read(after,e)||!same_owners(after,e))return false;
        if(after.project!=before.project||!before.row||!after.row)return fail(e,"startingpoint Deploy changed current project owner");
        auto expected=*before.row;expected.deployed=true;expected.inventory_count=1;expected.release_feature_count=1;
        ++expected.release_change_count;++expected.release_counter;
        if(*after.row!=expected)return fail(e,"startingpoint original Deploy record/inventory/Release readback differs");
        size_t insertions=0;
        for(size_t i=0;i<before.items.size();++i){
            if(before.items[i]==after.items[i])continue;
            if(before.items[i]||!after.items[i])return fail(e,"startingpoint Deploy replaced or removed an existing inventory item");
            ++insertions;
        }
        return insertions==1||fail(e,"startingpoint Deploy did not insert exactly one inventory item");
    }
    bool retire(std::string& e){
        uintptr_t current=0;if(!at(before.lounge,0xa8,current,e))return false;
        if(current!=before.project)return fail(e,"startingpoint project owner changed before native destruction");
        const auto code=startingpoint_destroy(before.project);
        if(code)return fail(e,"startingpoint original project deleting destructor exception="+std::to_string(code));
        // Commit's two Map::AddBugsFromTech calls return before any RNG or map
        // mutation for the required zero counters. Keep its remaining tail;
        // ClearCurrentDevProject would additionally cancel developers.
        return put(before.lounge,0xa8,uintptr_t(0),e)&&put(before.lounge,0xb0,uint64_t(0),e)&&
            put(before.lounge,0xb8,uint64_t(0),e)&&put(before.lounge,0xc4,int32_t(0),e);
    }
    bool readback(const startingpoint::Row& target,std::string& e){
        tutorial_development::StartingpointSnapshot after;if(!read(after,e)||!same_owners(after,e))return false;
        if(after.row&&!after.row->project_present)after.row->commit_bugs=target.commit_bugs;
        if(!after.row||*after.row!=target)return fail(e,"startingpoint full native readback differs from Host row");
        if(target.project_present&&(after.project!=before.project||after.items!=before.items))return fail(e,"startingpoint project adoption changed project/inventory ownership");
        if(!target.project_present&&after.project)return fail(e,"startingpoint native project destructor/tail did not retire project");
        return true;
    }
};
void commit_current(void* lounge){
    // Joined Clients adopt the Host's original Deploy and cleanup below. Their
    // ordinary Commit button must not run a second independent Commit.
    if(load.armed){
        if(GetCurrentThreadId()!=mainthread){block("Client native Commit thread differs");return;}
        if(!client_commit_suppressed){client_commit_suppressed=true;LOG_INFO("[Tutorial] Client native Commit suppressed; Host owns development");}
        return;
    }
    tutorial_development::StartingpointSnapshot before;
    nlohmann::json diagnostic;std::string cause;
    bool observe=false;
    try{
        action_session::Context context;
        if(action_session::selected_session().current(context,cause)&&context.role==action_session::Role::Host&&
           verify_commit_entry(image+0x25fb10,cause)&&tutorial_development::read_startingpoint(before,diagnostic,cause)&&
           before.lounge==reinterpret_cast<uintptr_t>(lounge)&&before.project&&before.row&&before.row->project_present&&!before.row->deployed){
            commit_receipt={before.origin,before.tech,before.record,std::nullopt,{}};observe=true;
        }
    }catch(const std::exception& x){cause=std::string("startingpoint pre-Commit receipt: ")+x.what();}
    // This hook does not authorize or replace any Host operation. Preserve the
    // original native UI/script call even when its optional receipt is unavailable.
    original_commit(lounge);
    if(!observe){if(!cause.empty())LOG_WARN("[Tutorial] Host Commit receipt unavailable cause={}",cause);return;}
    try{
        tutorial_development::StartingpointSnapshot after;
        if(tutorial_development::read_startingpoint(after,diagnostic,cause)&&same_world(before.origin,after.origin)&&
           before.lounge==after.lounge&&before.tech==after.tech&&before.record==after.record&&!after.project&&after.row&&after.row->deployed){
            commit_receipt.bugs=startingpoint::CommitBugs{before.row->bugs_total,before.row->bugs_known};return;
        }
        if(cause.empty())cause="startingpoint original Commit owner/retirement readback differs";
    }catch(const std::exception& x){cause=std::string("startingpoint post-Commit receipt: ")+x.what();}
    commit_receipt.cause=cause;LOG_WARN("[Tutorial] Host Commit receipt rejected cause={}",cause);
}
