namespace lifecycle_native {
bool host(World& w,action_session::Context& c,action_session::Binding& b,std::string& e){
    if(load.armed||!action_session::selected_session().current(c,e)||c.role!=action_session::Role::Host)return false;
    if(!native_open_runtime::read_current_world(w,e))return false;
    uintptr_t base=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(base,thread,sha,e)||base!=image||thread!=GetCurrentThreadId())return fail(e,e.empty()?"tutorial lifecycle Host receipt requires bound Mainthread/image":e);
    if(!mainthread)mainthread=thread;
    b={w.world_epoch,w.generation};return origin(c,b,w,e)&&bind_cache(w,c,e);
}
void remember_terminal(uintptr_t sequence){
    try{
        uint8_t complete=0;std::string e;
        if(!at(sequence,0x54,complete,e)){block(e);return;}if(complete!=1)return;
        World w;action_session::Context c;action_session::Binding b;if(!host(w,c,b,e)){
            if(!e.empty())LOG_WARN("[Tutorial] Host terminal receipt unavailable cause={}",e);return;
        }
        for(size_t index=0;index<2;++index){Owners owner;NativeCursor n;bool found=false;
            if(!owners(w,lifecycle::name(index),owner,e)||!owned_cursor(owner,lifecycle::name(index),n,found,e)){block(e);return;}
            if(!found||n.sequence!=sequence)continue;
            if(lifecycle::find(cache->proof.terminal,index))return;
            State observed;std::vector<NativeCursor> cursors;if(!native_state(c,b,observed,cursors,e)){block("tutorial terminal capture: "+e);return;}
            const auto* terminal=lifecycle::find(observed.active,index);
            if(!terminal||!terminal->complete||terminal->next!=terminal->count||terminal->message_progress||cache->serial>=4){block("tutorial terminal capture cursor/serial differs");return;}
            cache->proof.terminal.push_back(*terminal);cache->proof.lifecycle_order[index][1]=++cache->serial;
            if(index==0){cache->proof.terminal_startingpoint=observed.startingpoint;cache->proof.terminal_version=observed.version;}
            return;
        }
    }catch(const std::exception& x){block(std::string("tutorial terminal receipt: ")+x.what());}
}
void advance(void* value){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!creation){
        const auto sequence=reinterpret_cast<uintptr_t>(value);
        if(load.armed&&load.sequences.contains(sequence)&&(GetCurrentThreadId()!=mainthread||scoped_sequence!=sequence)){
            block("Client tutorial Advance has no bound restoration/Host-frame step scope");return;
        }
        original_advance(value);return;
    }
    try{
        auto& scope=*creation;std::string e;World current;const auto sequence=reinterpret_cast<uintptr_t>(value);
        if(caller!=image+0x4f61fe||scope.entered||!origin(scope.context,scope.binding,current,e)||!same_world(scope.world,current)){
            block(e.empty()?"tutorial constructor Advance caller/scope/origin differs":e);return;}
        if(!sequence||load.sequences.contains(sequence)||load.quarantined.contains(sequence)){block("tutorial constructor Sequence address is null or already fenced");return;}
        load.quarantined.insert(sequence);
        if(!type(sequence,0x16dbd80,e)){block(e);return;}
        // The original constructor supplied this typed Sequence under the exact
        // owned Start call. Retain its fence even if later asset checks fail.
        scope.sequence=sequence;load.sequences.insert(sequence);
        uintptr_t steps=0,step=0,current_step=0;int32_t count=0,next=0;uint8_t complete=0;std::string command;
        if(!array(sequence,0x20,2048,steps,count,e)||!at(sequence,0x50,next,e)||!at(sequence,0x54,complete,e)||!at(sequence,0x48,current_step,e)||
           !at(steps,0,step,e)||!type(step,0x16fd930,e)||!str(step,0x40,command,e)){block(e);return;}
        const bool initial=scope.id=="initial_tutorial";
        if(count!=(initial?107:17)||next||complete||current_step||command!=(initial?"MessageTo SoftwareStatus LockLevelUp":"MessageTo Hud Tutorial 1")){
            block("tutorial constructor first Command/count/index/fresh sequence differs");return;}
        scope.entered=true;
        Scope effects(sequence,false);original_advance(value);
    }catch(const std::exception& x){block(std::string("tutorial constructor Advance guard: ")+x.what());}
}
void scenario_start(void* log,void* scenario,void* parameters){
    if(load.armed){
        try{
            std::string e,id;if(!str(reinterpret_cast<uintptr_t>(scenario),0x18,id,e)){block(e);return;}
            if(!supported(id)){original_scenario_start(log,scenario,parameters);return;}
            World w;if(GetCurrentThreadId()!=mainthread||!native_open_runtime::read_current_world(w,e)||w.uuid!=load.uuid||w.generation!=load.generation){block(e.empty()?"Client scenario creation bound thread/origin differs":e);return;}
            Owners owner;if(!owners(w,id,owner,e)||owner.log!=reinterpret_cast<uintptr_t>(log)||owner.asset!=reinterpret_cast<uintptr_t>(scenario)){block(e.empty()?"Client scenario creation Mode/library/log owner differs":e);return;}
            if(!creation){LOG_INFO("[Tutorial] Client native scenario start suppressed until Host frame scenario={}",id);return;}
            if(parameters||creation->log!=owner.log||creation->asset!=owner.asset||creation->id!=id||!same_world(creation->world,w)||creation->entered){block("tutorial requested creation does not match exact scoped native StartScenario");return;}
            original_scenario_start(log,scenario,parameters);return;
        }catch(const std::exception& x){block(std::string("Client scenario creation: ")+x.what());return;}
    }
    World w;action_session::Context c;action_session::Binding b;std::string e,id;bool observe=false;int32_t checkpoint=-1;
    try{
        if(str(reinterpret_cast<uintptr_t>(scenario),0x18,id,e)&&supported(id)&&host(w,c,b,e)){
            Owners owner;NativeCursor existing;bool present=false;
            if(owners(w,id,owner,e)&&owner.log==reinterpret_cast<uintptr_t>(log)&&owner.asset==reinterpret_cast<uintptr_t>(scenario)&&
               !parameters&&!owner.completed&&owned_cursor(owner,id,existing,present,e)&&!present){
                if(id=="leveltwo_tutorial"){
                    Owners initial;NativeCursor cursor;bool active=false;
                    if(!owners(w,"initial_tutorial",initial,e)||!owned_cursor(initial,"initial_tutorial",cursor,active,e))throw std::runtime_error(e);
                    if(active)checkpoint=cursor.value.next;
                }observe=true;
            }
        }
    }catch(const std::exception& x){e=std::string("tutorial pre-StartScenario receipt: ")+x.what();}
    original_scenario_start(log,scenario,parameters);
    if(!observe){if(!e.empty()&&supported(id))LOG_WARN("[Tutorial] Host scenario birth receipt unavailable cause={}",e);return;}
    try{
        World after;Owners owner;NativeCursor cursor;bool present=false;
        if(!origin(c,b,after,e)||!same_world(w,after)||!owners(after,id,owner,e)||!owned_cursor(owner,id,cursor,present,e)||!present||
           cursor.value.next!=1||cursor.value.complete||owner.completed||cache->serial>=4||cache->proof.lifecycle_order[lifecycle::slot(id)][0]){
            block(e.empty()?"tutorial Host StartScenario first cursor/order readback differs":e);return;}
        cache->proof.lifecycle_order[lifecycle::slot(id)][0]=++cache->serial;
        if(id=="leveltwo_tutorial")cache->proof.leveltwo_initial_next=checkpoint;
    }catch(const std::exception& x){block(std::string("tutorial post-StartScenario receipt: ")+x.what());}
}
}
