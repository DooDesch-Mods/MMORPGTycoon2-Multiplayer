namespace lifecycle_native {
DWORD start_call(uintptr_t log,uintptr_t asset){__try{reinterpret_cast<void(*)(void*,void*,void*)>(image+0x4f3b00)(reinterpret_cast<void*>(log),reinterpret_cast<void*>(asset),nullptr);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD complete_call(uintptr_t log,uintptr_t asset){__try{reinterpret_cast<void(*)(void*,void*)>(image+0x4f4130)(reinterpret_cast<void*>(log),reinterpret_cast<void*>(asset));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD set_version_call(uintptr_t software){__try{reinterpret_cast<void(*)(void*,int32_t,int32_t,int32_t)>(image+0x525780)(reinterpret_cast<void*>(software+0x70),0,0,100);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool pin(const action_session::Context& c,const action_session::Binding& b,const World& expected,const std::string& id,Owners& owner,std::string& e,bool catalogue=true){
    World actual;if(!origin(c,b,actual,e)||!same_world(expected,actual))return fail(e,e.empty()?"tutorial lifecycle full NativeOpen changed":e);
    return catalogue?owners(actual,id,owner,e):log_owners(actual,id,owner,e);
}
bool create(const action_session::Context& c,const action_session::Binding& b,const World& world,size_t index,const State& target,bool& attempted,std::string& e){
    Owners before;NativeCursor cursor;bool present=false;const std::string id=lifecycle::name(index);
    if(!pin(c,b,world,id,before,e)||!owned_cursor(before,id,cursor,present,e))return false;
    if(present||before.completed||creation)return fail(e,"tutorial StartScenario requires absent, incomplete and unentered fixed scenario");
    Creation pending{world,c,b,before.log,before.asset,0,id,false};
    struct Reset {~Reset(){creation=nullptr;}} reset;creation=&pending;
    attempted=true;const auto code=start_call(before.log,before.asset);
    if(code)return fail(e,"tutorial original StartScenario exception="+std::to_string(code));
    if(!fault.empty())return fail(e,fault);
    Owners after;if(!pin(c,b,world,id,after,e)||!owned_cursor(after,id,cursor,present,e))return false;
    if(!present||!pending.entered||!pending.sequence||cursor.sequence!=pending.sequence||cursor.value.next!=1||cursor.value.complete||after.completed||
       after.log!=before.log||after.library!=before.library||after.asset!=before.asset||after.instances.size()!=before.instances.size()+1||
       !std::equal(before.instances.begin(),before.instances.end(),after.instances.begin())||after.instances.back()!=cursor.instance)
        return fail(e,"tutorial original StartScenario appended instance/first Advance readback differs");
    if(!bind_cache(world,c,e))return false;
    cache->proof.lifecycle_order[index][0]=target.lifecycle_order[index][0];
    cache->serial=std::max(cache->serial,target.lifecycle_order[index][0]);
    if(index==1)cache->proof.leveltwo_initial_next=target.leveltwo_initial_next;
    load.quarantined.erase(cursor.sequence);
    return true;
}
bool forward(const action_session::Context& c,const action_session::Binding& b,const World& world,size_t index,int32_t next,const State& target,bool terminal,bool& attempted,std::string& e){
    Owners owner;NativeCursor n;bool present=false;const auto* final=lifecycle::desired(target,index);
    if(!final||!pin(c,b,world,lifecycle::name(index),owner,e,false)||!owned_cursor(owner,lifecycle::name(index),n,present,e))return false;
    if(!present||!load.sequences.contains(n.sequence))return fail(e,"tutorial forward scenario has no early native fence");
    if(next<n.value.next)return fail(e,"tutorial forward checkpoint moved backward after planning");
    Cursor wanted=*final;wanted.next=next;wanted.complete=terminal;
    if(!interval(n,wanted,lifecycle::technology(target),e))return false;
    uintptr_t tech_step=0,version_step=0;
    if(index==0&&n.value.next<=93&&next>93&&lifecycle::technology(target)){if(!at(n.steps,93*8,tech_step,e))return false;}
    if(index==0&&n.value.next<=100&&next>100){if(!at(n.steps,100*8,version_step,e))return false;}
    Scope scope(n.sequence,false,tech_step,version_step);
    for(auto position=n.value.next;position<next;++position){
        if(index==0&&position==100){
            const auto expected=target.version?target.version:target.terminal_version;
            uintptr_t software=0;std::array<int32_t,3> before{},after{};World current;
            if(!expected||!origin(c,b,current,e)||!same_world(world,current)||!version(world,software,before,e))return fail(e,e.empty()?"tutorial step100 requires actual version receipt":e);
            if(before!=*expected){
                if(before[0]>0||*expected!=std::array<int32_t,3>{0,0,100})return fail(e,"tutorial step100 only admits original Set(0,0,100)");
                attempted=true;const auto code=set_version_call(software);if(code)return fail(e,"tutorial original Version Set exception="+std::to_string(code));
            }
            uintptr_t after_owner=0;
            if(!origin(c,b,current,e)||!same_world(world,current)||!version(world,after_owner,after,e))return false;
            if(software!=after_owner||after!=*expected)return fail(e,"tutorial original Version Set full owner/value readback differs");
        }
        attempted=true;const auto code=advance_call(n.sequence);if(code)return fail(e,"tutorial Advance exception="+std::to_string(code));
        if(!fault.empty())return fail(e,fault);int32_t actual=0;
        if(!at(n.sequence,0x50,actual,e)||actual!=position+1)return fail(e,e.empty()?"tutorial Advance index readback differs":e);
    }
    if(terminal){uint8_t complete=0;if(!at(n.sequence,0x54,complete,e))return false;
        if(!complete){attempted=true;const auto code=advance_call(n.sequence);if(code)return fail(e,"tutorial final Stop/Advance exception="+std::to_string(code));if(!fault.empty())return fail(e,fault);}
        int32_t actual=0;if(!at(n.sequence,0x54,complete,e)||!at(n.sequence,0x50,actual,e))return false;
        if(complete!=1||actual!=next)return fail(e,"tutorial final Stop/Advance completion/index readback differs");
    }
    if(next==final->next){
        if(n.value.history!=final->history||n.value.last!=final->last){attempted=true;if(!history(n,*final,e))return false;}
        if(!terminal){ProgressCalls presentation;if(!message_progress::apply(memory,image,n.sequence,next,final->message_progress,presentation,attempted,e))return false;}
    }
    Owners after;if(!pin(c,b,world,lifecycle::name(index),after,e,false))return false;
    return owner.log==after.log&&owner.library==after.library&&owner.asset==after.asset&&owner.instances==after.instances&&owner.completed==after.completed||fail(e,"tutorial owners changed during forward transition");
}
bool complete(const action_session::Context& c,const action_session::Binding& b,const World& world,size_t index,const State& target,bool& attempted,std::string& e){
    if(!forward(c,b,world,index,lifecycle::desired(target,index)->next,target,true,attempted,e))return false;
    Owners before;NativeCursor cursor;bool present=false;const std::string id=lifecycle::name(index);
    if(!pin(c,b,world,id,before,e)||!owned_cursor(before,id,cursor,present,e))return false;
    if(!present||before.completed||!cursor.value.complete||cursor.value.next!=cursor.value.count||cursor.value!=*lifecycle::find(target.terminal,index))return fail(e,"tutorial CompleteScenario requires exact final cursor/history and one active instance");
    attempted=true;const auto code=complete_call(before.log,before.asset);
    if(code)return fail(e,"tutorial original CompleteScenario exception="+std::to_string(code));
    Owners after;NativeCursor absent;if(!pin(c,b,world,id,after,e)||!owned_cursor(after,id,absent,present,e))return false;
    auto expected=before.instances;expected.erase(std::find(expected.begin(),expected.end(),cursor.instance));
    if(present||after.completed!=1||before.log!=after.log||before.library!=after.library||before.asset!=after.asset||after.instances!=expected)
        return fail(e,"tutorial original CompleteScenario removal/completed-ID readback differs");
    load.sequences.erase(cursor.sequence);
    if(!bind_cache(world,c,e))return false;
    cache->proof.terminal.push_back(*lifecycle::find(target.terminal,index));cache->proof.lifecycle_order[index]=target.lifecycle_order[index];
    cache->serial=std::max(cache->serial,target.lifecycle_order[index][1]);
    if(index==0){cache->proof.terminal_startingpoint=target.terminal_startingpoint;cache->proof.terminal_version=target.terminal_version;}
    return true;
}
bool prepare(const action_session::Context& c,const action_session::Binding& b,const State& local,const State& target,
             lifecycle::Plan& plan,std::optional<startingpoint::Row>& local_technology,std::string& e){
    if(!lifecycle::plan(local,target,plan,e))return false;
    if(local==target){local_technology=lifecycle::technology(local);return true;}
    if((plan.version_set||std::any_of(plan.operations.begin(),plan.operations.end(),[](const lifecycle::Operation& op){return op.kind!=lifecycle::Kind::Forward;}))&&!verify_transition_code(e))return false;
    World world;if(!origin(c,b,world,e)||!startingpoint_clock(e))return false;
    for(size_t i=0;i<2;++i){if(!lifecycle::desired(target,i)&&!lifecycle::find(local.active,i))continue;
        const bool transition=std::any_of(plan.operations.begin(),plan.operations.end(),[&](const lifecycle::Operation& op){return op.index==i&&op.kind!=lifecycle::Kind::Forward;});
        Owners owner;NativeCursor n;bool present=false;
        if(!(transition?owners(world,lifecycle::name(i),owner,e):log_owners(world,lifecycle::name(i),owner,e))||!owned_cursor(owner,lifecycle::name(i),n,present,e))return false;
        const auto* desired=lifecycle::desired(target,i);
        if(present&&desired&&!interval(n,*desired,lifecycle::technology(target),e))return false;
        const auto* current=lifecycle::find(local.active,i);
        if(current&&desired&&current->next==desired->next&&current->message_progress.has_value()!=desired->message_progress.has_value())
            return fail(e,"tutorial Host/local current Meter presence differs");
        if(!present&&!lifecycle::completed(local,i)&&desired&&desired->count!=(i==0?107:17))return fail(e,"tutorial creation Host cursor has wrong fixed asset count");
        if(present&&desired&&desired->message_progress){uintptr_t step=0;if(!at(n.steps,size_t(desired->next-1)*8,step,e)||!type(step,0x16fda00,e))return false;}
    }
    local_technology=lifecycle::technology(local);
    if(!local_technology&&lifecycle::technology(target)){
        tutorial_development::StartingpointSnapshot actual;nlohmann::json diagnostic;
        if(!tutorial_development::read_startingpoint(actual,diagnostic,e,true)||!same_world(world,actual.origin))return fail(e,e.empty()?"tutorial creation Startingpoint full Origin differs":e);
        local_technology=actual.row;
    }
    const auto desired_technology=lifecycle::technology(target);
    startingpoint::Plan operation;if(!startingpoint::plan(local_technology,desired_technology,operation,e))return false;
    if(operation!=startingpoint::Plan::None){StartingpointCalls calls{c,b,{}};if(!calls.pin(*local_technology,operation==startingpoint::Plan::Deploy,e))return false;}
    if(!lifecycle::find(local.active,0)&&!local.initial_complete&&lifecycle::desired(target,0)){
        uintptr_t software=0;std::array<int32_t,3> actual{};if(!version(world,software,actual,e))return false;
        const Cursor begin{"initial_tutorial",{},107,1,false,{},{}};bool set=false;
        const auto expected=target.version?target.version:target.terminal_version;
        if(!lifecycle::version_plan(actual,expected,&begin,lifecycle::desired(target,0),set,e))return false;
    }
    return true;
}
}
