namespace {
bool queue_release(Runtime& r,std::string& error){const auto* lease=r.leases.find(r.context.local_peer);if(!lease)return fail(error,"Control local release lease absent");
    for(const auto& row:r.queue)if(row.intent.command==intervention::Command::ControlRelease)return true;
    a::InterventionIntent intent;intent.target=lease->actor;intent.command=intervention::Command::ControlRelease;intent.control=intervention::Control{};intent.control->lease=lease->id;
    return enqueue(r,std::move(intent),error);
}
void take_entry(void* player,void* actor){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<Pair>(r->routes[1].original);
    if(!active(*r)||r->presenting){original(player,actor);return;}std::string error;PresentationBinding local;
    if(!presentation(*r,uintptr_t(actor),local,error)||uintptr_t(player)!=local.player||!capture_acquire(uintptr_t(actor),error))rejected(*r,error.empty()?"Control TakeControl real Player differs":error);
}
void release_entry(void* player,void* actor){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<Pair>(r->routes[2].original);
    if(!active(*r)||r->presenting){original(player,actor);return;}std::string error;const auto* lease=r->leases.find(r->context.local_peer);
    if(!lease){original(player,actor);return;}PresentationBinding local;
    if(!presentation(*r,uintptr_t(actor),local,error)||local.player!=uintptr_t(player)||!queue_release(*r,error))rejected(*r,error.empty()?"Control release real Player differs":error);
}
void flag_entry(size_t route,void* actor,bool value,uintptr_t caller){auto* r=published.load(std::memory_order_acquire);
    if(r->presenting&&uintptr_t(actor)==r->presented_actor&&((value&&caller==r->image+0xd4d13)||(!value&&caller==r->image+0xd34d9)))return;
    reinterpret_cast<Flag>(r->routes[route].original)(actor,value);
}
void character_flag_entry(void* actor,bool value){flag_entry(4,actor,value,uintptr_t(_ReturnAddress()));}
void toon_flag_entry(void* actor,bool value){flag_entry(5,actor,value,uintptr_t(_ReturnAddress()));}
void time_entry(void* clock,float value){auto* r=published.load(std::memory_order_acquire);
    if(r->presenting&&uintptr_t(_ReturnAddress())==r->image+0xd4cfe)return;
    reinterpret_cast<Update>(r->routes[6].original)(clock,value);
}
void target_entry(void* actor,void* target){auto* r=published.load(std::memory_order_acquire);
    if(!active(*r)){reinterpret_cast<Pair>(r->routes[3].original)(actor,target);return;}
    uint8_t controlled{};std::string error;if(!at(*r,uintptr_t(actor),0x6c1,controlled,error)){lock_runtime(*r,error);return;}
    if(controlled){Target canonical;if(!owner(*r,uintptr_t(actor),canonical,error)){lock_runtime(*r,error);return;}
        for(const auto& lease:r->leases.values())if(lease&&lease->actor==canonical&&lease->peer!=r->context.local_peer){
            std::optional<Target> selected;if(target){Target value;if(!owner(*r,uintptr_t(target),value,error)){lock_runtime(*r,error);return;}selected=value;}
            NativePort native(r->context,r->binding,true);uint32_t exception{};
            if(!native.set_target(canonical,selected,exception,error))lock_runtime(*r,error);return;
        }
    }
    reinterpret_cast<Pair>(r->routes[3].original)(actor,target);
}
void item_entry(size_t route,void* item,uintptr_t caller){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<One>(r->routes[route].original);
    const bool button=route==7?caller==r->image+0x58095c:caller==r->image+0x580903;
    if(!active(*r)||r->replay||!button){game::item_activation_origin::forward(original,item,r->image,caller,route==7);return;}
    const auto* lease=r->leases.find(r->context.local_peer);if(!lease){game::item_activation_origin::forward(original,item,r->image,caller,route==7);return;}
    std::string error;uintptr_t character{},type{},data{};int32_t count{},capacity{};Target actor;
    if(!at(*r,uintptr_t(item),0x80,character,error)||!owner(*r,character,actor,error)||actor!=lease->actor){rejected(*r,error.empty()?"Control manual ability belongs to another actor":error);return;}
    intervention::Census census;if(!intervention::census(r->context,r->binding,census,error)||!at(*r,character,0x7b0,data,error)||!at(*r,character,0x7b8,count,error)||!at(*r,character,0x7bc,capacity,error)||count<0||count>4096||capacity<count||!at(*r,uintptr_t(item),0x30,type,error)){rejected(*r,error.empty()?"Control manual ability native member array differs":error);return;}
    const auto prototype=census.owners.find(type);if(prototype==census.owners.end()){rejected(*r,"Control manual ability canonical prototype absent");return;}
    int32_t ordinal=-1;for(int32_t i=0;i<count;++i){uintptr_t candidate{};if(!at(*r,data,size_t(i)*8,candidate,error)){rejected(*r,error);return;}if(candidate==uintptr_t(item)){if(ordinal>=0){rejected(*r,"Control manual ability duplicated in owner set");return;}ordinal=i;}}
    if(ordinal<0){rejected(*r,"Control manual ability absent from current actor set");return;}
    a::InterventionIntent intent;intent.target=actor;intent.command=route==7?intervention::Command::ControlActivate:intervention::Command::ControlDeactivate;intent.control=intervention::Control{};intent.control->lease=lease->id;intent.control->ability=ordinal;intent.control->prototype=prototype->second;
    if(r->context.role==a::Role::Client&&r->sample){
        if(r->queue.size()>2||r->local_sequence==UINT64_MAX){rejected(*r,"Control manual ability waits for input queue capacity");return;}
        a::InterventionIntent input;input.target=actor;input.command=intervention::Command::ControlInput;input.control=*r->sample;input.control->sequence=++r->local_sequence;
        input.control->jump=std::min(input.control->jump,lease->jump_sequence+(lease->jump_sequence!=UINT64_MAX));
        if(!enqueue(*r,std::move(input),error)){rejected(*r,error);return;}
    }
    if(!enqueue(*r,std::move(intent),error))rejected(*r,error);
}
void activate_entry(void* item){item_entry(7,item,uintptr_t(_ReturnAddress()));}
void deactivate_entry(void* item){item_entry(8,item,uintptr_t(_ReturnAddress()));}
void update_entry(void* player,float dt){auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<Update>(r->routes[0].original);
    if(!active(*r)){original(player,dt);return;}
    std::string error;const auto* local=r->leases.find(r->context.local_peer);
    if(r->context.role==a::Role::Client&&local){
        if(r->locked)return;NativePort native(r->context,r->binding,true);ActorState actor;PresentationBinding view;
        if(!current(*r,r->context,r->binding,error)||!native.read(local->actor,actor,error)||!presentation(*r,actor.pointer,view,error)||view.player!=uintptr_t(player)){lock_runtime(*r,error.empty()?"Control Client update actual Player differs":error);return;}
        if(actor.dead||!actor.on_map){if(!queue_release(*r,error))rejected(*r,error);return;}
        uint32_t exception=present_local_native(view);LocalInput input;if(!exception)exception=capture_local_native(view,input);
        if(exception){lock_runtime(*r,"Control Client local input/presentation failed SEH="+std::to_string(exception));return;}
        intervention::Control captured;captured.lease=local->id;captured.input=input.motion;captured.input.jump_pressed=false;
        if(input.motion.jump_pressed){if(r->local_jump==UINT64_MAX){lock_runtime(*r,"Control jump sequence exhausted");return;}++r->local_jump;}captured.jump=r->local_jump;
        if(input.selected){Target target;if(owner(*r,input.selected,target,error))captured.selected=target;else{
            // Native Player dynamic_cast returns null for non-Character selection.
            intervention::Census census;if(!intervention::census(r->context,r->binding,census,error)){lock_runtime(*r,error);return;}
            if(std::any_of(census.actors.begin(),census.actors.end(),[&](const auto& row){return row.object==input.selected;})){lock_runtime(*r,error);return;}
        }}
        r->sample=std::move(captured);
    }else original(player,dt);
    if(r->context.role!=a::Role::Host||r->locked)return;
    if(!current(*r,r->context,r->binding,error)){lock_runtime(*r,error);return;}
    if(r->frame==UINT64_MAX){lock_runtime(*r,"Control native frame sequence exhausted");return;}++r->frame;
    for(uint64_t peer=1;peer<=4;++peer){auto* lease=r->leases.find(peer);if(!lease||peer==r->context.local_peer)continue;
        NativePort port(r->context,r->binding);ActorState actor;
        if(!port.read(lease->actor,actor,error)){rejected(*r,error);continue;}
        if(actor.dead||!actor.on_map||!(r->context.peer_mask&(1u<<(peer-1)))){
            if(game::typed_action_runtime::controller_status().value("pending",true))continue;
            StepReceipt release;if(!port.controlled(lease->actor,false,release,error)||!r->leases.retire_disconnected(peer,release,error)){lock_runtime(*r,error);return;}continue;
        }
        StepReceipt step;if(!r->leases.step(port,peer,r->frame,GetTickCount64(),dt,step,error)){if(step.unknown){lock_runtime(*r,error);return;}rejected(*r,error);}
    }
}
}
bool enable(std::string& error){
    auto* r=published.load(std::memory_order_acquire);if(r&&r->ready)return true;if(r&&r->install_attempted)return fail(error,"Control earlier hook installation failed: "+r->cause);
    if(!r){r=new Runtime;published.store(r,std::memory_order_release);}r->install_attempted=true;uint32_t thread{};std::string sha;
    if(!game::native::read_resolver_receipt(r->image,thread,sha,error)||thread!=r->thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(error,"Control exact executable/Mainthread differs: "+error);
    r->routes={
#include "control_route_initializers.inc"
    };
    const void* entries[]={reinterpret_cast<void*>(&update_entry),reinterpret_cast<void*>(&take_entry),reinterpret_cast<void*>(&release_entry),reinterpret_cast<void*>(&target_entry),reinterpret_cast<void*>(&character_flag_entry),reinterpret_cast<void*>(&toon_flag_entry),reinterpret_cast<void*>(&time_entry),reinterpret_cast<void*>(&activate_entry),reinterpret_cast<void*>(&deactivate_entry)};
    for(size_t i=0;i<r->routes.size();++i){auto& route=r->routes[i];auto& owner=route.owner;owner.entry=r->image+route.rva;owner.detour=uintptr_t(entries[i]);owner.stolen=route.stolen;owner.pristine.resize(route.bytes);
        if(!r->memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),error)||mt2mp::sha256(owner.pristine)!=route.sha){r->cause=error.empty()?"Control original route fingerprint differs":error;return fail(error,r->cause);}
        const auto created=MH_CreateHook(reinterpret_cast<void*>(owner.entry),const_cast<void*>(entries[i]),&route.original);
        if(created!=MH_OK||!route.original){r->cause="Control named route prepare failed status="+std::to_string(created)+" original="+hooks::framework::process_backend().error();return fail(error,r->cause);}owner.trampoline=uintptr_t(route.original);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK){r->cause="Control named route enable failed status="+std::to_string(enabled)+" original="+hooks::framework::process_backend().error();return fail(error,r->cause);}
        MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"Control enabled route receipt missing");owner.generation=actual.generation;
    }
    if(!routes(*r,error))return false;r->ready=true;error.clear();return true;
}
bool bind(const a::Context& context,const a::Binding& binding,uint64_t epoch,std::string& error){auto* r=published.load();if(!r||!r->ready||r->locked||!epoch||GetCurrentThreadId()!=r->thread)return fail(error,"Control bind requires ready current Mainthread");
    if(r->bound&&(context!=r->context||binding!=r->binding||epoch!=r->epoch)){for(const auto& lease:r->leases.values())if(lease)return fail(error,"Control changed snapshot binding retains an unreleased native lease");r->receipts.clear();r->last_frame.clear();r->frame=0;}
    r->context=context;r->binding=binding;r->epoch=epoch;r->bound=true;r->suspended=false;error.clear();return true;
}
bool suspend(std::string& error){auto* r=published.load();if(!r)return true;if(r->locked)return fail(error,r->cause);
    for(uint64_t peer=1;peer<=4;++peer){const auto* lease=r->leases.find(peer);if(!lease)continue;const auto actor=lease->actor;NativePort port(r->context,r->binding,true);StepReceipt receipt;
        if(!port.controlled(actor,false,receipt,error)){lock_runtime(*r,error);return false;}if(peer==r->context.local_peer&&!present(*r,actor,false,error)){lock_runtime(*r,error);return false;}
        if(!r->leases.retire_disconnected(peer,receipt,error)){lock_runtime(*r,error);return false;}}
    r->queue.clear();r->after_release.reset();r->sample.reset();r->suspended=true;error.clear();return true;
}
bool retire_lobby(std::string& error){auto* r=published.load();if(!r)return true;for(const auto& lease:r->leases.values())if(lease)return fail(error,"Control lobby retirement retains native lease");r->bound=false;r->suspended=true;r->queue.clear();r->sample.reset();r->after_release.reset();r->receipts.clear();r->last_frame.clear();error.clear();return true;}
bool capture(const a::Context& context,const a::Binding& binding,ReplicaState& out,std::string& error){out.clear();auto* r=published.load();if(!r||!current(*r,context,binding,error)||context.role!=a::Role::Host)return false;
    for(const auto& lease:r->leases.values())if(lease){intervention::Proof actual;if(!proof(*r,lease->actor,actual,error))return false;out.push_back({lease->actor,*actual.control});}return valid(out,error);
}
bool apply(const a::Context& context,const a::Binding& binding,const ReplicaState& desired,bool& attempted,std::string& error){
    attempted=false;auto* r=published.load();if(!r||!current(*r,context,binding,error)||context.role!=a::Role::Client||!valid(desired,error))return false;NativePort port(context,binding,true);
    for(uint64_t peer=1;peer<=4;++peer){const auto* lease=r->leases.find(peer);const auto found=std::find_if(desired.begin(),desired.end(),[&](const auto& row){return row.state.peer==peer;});
        if(found==desired.end()){if(!lease)continue;const auto actor=lease->actor;StepReceipt released;
            if(!port.replica_flags(actor,false,false,released,error)){attempted|=released.attempted;return false;}attempted|=released.attempted;
            if(peer==context.local_peer&&!present(*r,actor,false,error))return false;
            if(!r->leases.retire_disconnected(peer,released,error))return false;continue;
        }
        if(!lease||lease->id!=found->state.lease||lease->actor!=found->actor||lease->input_sequence!=found->state.sequence||lease->jump_sequence!=found->state.jump)return fail(error,"Control Frame lease differs from admitted ordered Action138 state");
        StepReceipt flags;if(!port.replica_flags(found->actor,true,found->state.moving,flags,error)){attempted|=flags.attempted;return false;}attempted|=flags.attempted;
        uint32_t exception{};attempted=true;if(!port.set_target(found->actor,found->state.selected,exception,error))return false;
        ActorState actual;if(!port.read(found->actor,actual,error)||!actual.controlled||actual.moving!=found->state.moving||actual.target!=found->state.selected)return fail(error,error.empty()?"Control complete Frame native readback differs":error);
    }
    r->last_frame=desired;error.clear();return true;
}
nlohmann::json status(){const auto* r=published.load();if(!r)return {{"ready",false}};return {{"ready",r->ready},{"bound",r->bound},{"suspended",r->suspended},{"locked",r->locked},{"cause",r->cause},{"queued",r->queue.size()},{"frame",r->frame},{"replica",encode(r->last_frame)}};}
