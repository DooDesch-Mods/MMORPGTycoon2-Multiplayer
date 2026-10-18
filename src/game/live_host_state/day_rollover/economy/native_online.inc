namespace {
struct OnlineCalls {
    void (*logout)(uintptr_t)=nullptr;
    void (*notify_logout)(uintptr_t,uintptr_t)=nullptr;
    void (*queues)(uintptr_t)=nullptr;
    void (*login)(uintptr_t,uintptr_t)=nullptr;
    int32_t (*capacity)(uintptr_t)=nullptr;
    bool (*login_empty)(uintptr_t)=nullptr;
};
OnlineCalls online_calls(uintptr_t image){
    return {reinterpret_cast<void(*)(uintptr_t)>(image+0x3e9920),
        reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x2f0860),
        reinterpret_cast<void(*)(uintptr_t)>(image+0x2ff930),
        reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x2ff3b0),
        reinterpret_cast<int32_t(*)(uintptr_t)>(image+0x34f860),
        reinterpret_cast<bool(*)(uintptr_t)>(image+0x28b860)};
}
struct OnlineOwner {
    MotionOwner motion;
    uint64_t uid=0;
    bool initialize_last_region=false;
    std::vector<uintptr_t> registered,on,off;
};
bool online_array(uintptr_t owner,size_t base,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t data=0;int32_t count=-1,capacity=-1;
    if(!at(owner,base+8,data,e)||!at(owner,base+0x10,count,e)||!at(owner,base+0x14,capacity,e))return false;
    if(count<0||count>1000000||capacity<count||(count&&!data))return fail(e,"Subscriber lifecycle array bounds differ");
    out.clear();std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!at(data,size_t(i)*8,value,e))return false;
        if(!value||!unique.insert(value).second)return fail(e,"Subscriber lifecycle null/duplicate array member");out.push_back(value);}
    return true;
}
bool online_queues(uintptr_t manager,const std::vector<uintptr_t>& logout,const OnlineCalls& calls,std::string& e){
    std::vector<uintptr_t> actual;
    if(!online_array(manager,0x38,actual,e))return false;
    if(actual!=logout)return fail(e,"Subscriber lifecycle logout queue differs actual_count="+std::to_string(actual.size())+" required_count="+std::to_string(logout.size()));
    for(size_t base:{size_t(0x50),size_t(0x68)})if(!online_array(manager,base,actual,e)||!actual.empty())return fail(e,e.empty()?"Subscriber lifecycle foreign transfer queue present":e);
    if(!calls.login_empty(manager+0x80))return fail(e,"Subscriber lifecycle foreign login queue present");
    return true;
}
bool online_partition(const Owners& before,OnlineOwner& owner,std::string& e){
    if(!online_array(before.subscriber_manager,0x80,owner.on,e)||!online_array(before.subscriber_manager,0x98,owner.off,e))return false;
    std::set<uintptr_t> all(owner.on.begin(),owner.on.end());
    for(auto value:owner.off)if(!all.insert(value).second)return fail(e,"Subscriber lifecycle online/offline partitions overlap");
    if(all!=std::set<uintptr_t>(before.subscribers.begin(),before.subscribers.end()))return fail(e,"Subscriber lifecycle partitions differ from actual cohort");
    for(auto sub:before.subscribers){uintptr_t toon=0,reverse=0;uint8_t logged=0;
        if(!at(sub,0x80,toon,e))return false;
        if(toon&&(!ap(toon,before.history.pin.image,0x1744e90,e)||!at(toon,0x940,reverse,e)||reverse!=sub||!at(toon,0x825,logged,e)||logged>1))return fail(e,e.empty()?"Subscriber lifecycle reciprocal Toon differs":e);
        const auto& rows=logged?owner.on:owner.off;
        if(std::find(rows.begin(),rows.end(),sub)==rows.end())return fail(e,"Subscriber lifecycle Toon flag/manager partition differs");
    }
    return true;
}
bool online_prepare(const Owners& before,size_t index,const std::optional<Motion>& target,const OnlineCalls& calls,OnlineOwner& out,std::string& e){
    if(index>=before.subscribers.size()||index>=before.state.subscribers.size())return fail(e,"Subscriber lifecycle saved-ID slot invalid");
    const auto& p=before.history.pin;const auto subscriber=before.subscribers[index];
    const auto& old=before.state.subscribers[index].motion;
    if(old.has_value()==target.has_value())return fail(e,"Subscriber lifecycle requires exactly one online transition");
    out={};auto& owner=out.motion;uintptr_t reverse=0,shard=0,last=0,actor=0,render=0,callback=0;
    uint8_t generated=0,logged=0,logging_out=0,queued=0;int32_t region_id=-1,id=-1;
    if(!at(subscriber,0x20,id,e)||id!=before.state.subscribers[index].serialized_id||!at(subscriber,0x80,owner.toon,e)||
       !ap(owner.toon,p.image,0x1744e90,e)||!at(owner.toon,0x18,out.uid,e)||!out.uid||
       !at(owner.toon,0x940,reverse,e)||reverse!=subscriber||!at(owner.toon,0x824,generated,e)||
       !at(owner.toon,0x825,logged,e)||!at(owner.toon,0x9ed,logging_out,e)||!at(owner.toon,0x9ec,queued,e)||
       !at(owner.toon,0x6f0,actor,e))return fail(e,e.empty()?"Subscriber lifecycle native identity unreadable":e);
    if(generated!=1||logged!=uint8_t(old.has_value())||logging_out||queued||bool(actor)!=bool(old))return fail(e,"Subscriber lifecycle prior state differs id="+std::to_string(id)+" generated="+std::to_string(generated)+" logged="+std::to_string(logged)+" required_logged="+std::to_string(old.has_value())+" actor_present="+std::to_string(bool(actor))+" logging_out="+std::to_string(logging_out)+" queued="+std::to_string(queued));
    if(!motion_world(p,owner,e)||!at(owner.toon,0x170,shard,e)||!at(owner.toon,0x178,owner.region,e)||
       !at(owner.toon,0x9a8,last,e)||!at(owner.region,0x834,region_id,e))return fail(e,e.empty()?"Subscriber lifecycle root/registered Region differs":e);
    out.initialize_last_region=target.has_value()&&!old&&last==0;
    if(shard!=owner.root||(!out.initialize_last_region&&last!=owner.region))return fail(e,"Subscriber lifecycle root/registered Region differs");
    if(out.initialize_last_region){
        uint8_t on_map=1;
        if(!at(owner.toon,0x368,on_map,e)||on_map)return fail(e,e.empty()?"Subscriber loaded offline Toon is already on map":e);
    }
    uintptr_t resolved=0;
    if(!motion_region(p,owner,region_id,owner.region,resolved,e)||!motion_manager(p,owner.region,owner.manager,out.registered,e)||
       !at(owner.manager,0x18,resolved,e)||resolved!=owner.region||!at(owner.toon,0x30,render,e)||!at(render,0x38,callback,e)||callback!=p.image+0x3e82a0)return fail(e,e.empty()?"Subscriber lifecycle native Region/manager/callback differs":e);
    if(std::count(out.registered.begin(),out.registered.end(),owner.toon)!=(old?1:0))return fail(e,"Subscriber lifecycle native Region membership differs");
    if(!online_queues(owner.manager,{},calls,e)||!online_partition(before,out,e))return false;
    if(target){
        if(!motion_target(p,owner,*target,resolved,e))return false;
        const auto capacity=calls.capacity(owner.region);
        if(capacity<=0||out.registered.size()>=size_t(capacity))return fail(e,"Subscriber lifecycle original login has no immediate native capacity count="+std::to_string(out.registered.size())+" capacity="+std::to_string(capacity));
    }else {
        std::optional<Motion> actual;
        if(!motion_read(p,subscriber,actual,e)||actual!=old)return fail(e,e.empty()?"Subscriber lifecycle prior native pose differs":e);
    }
    return true;
}
template<class Move>bool online_apply(const Owners& before,size_t index,const std::optional<Motion>& old,const std::optional<Motion>& target,const OnlineCalls& calls,Move&& move,std::string& e){
    if(index>=before.subscribers.size()||index>=before.state.subscribers.size()||old!=before.state.subscribers[index].motion)return fail(e,"Subscriber lifecycle prior slot/pose differs");
    const auto& p=before.history.pin;const auto subscriber=before.subscribers[index];
    OnlineOwner held;
    if(!online_prepare(before,index,target,calls,held,e))return false;
    auto& owner=held.motion;
    if(target){
        if(held.initialize_last_region){
            OnlineOwner fresh;
            if(!online_prepare(before,index,target,calls,fresh,e))return false;
            const auto& actual=fresh.motion;
            if(!fresh.initialize_last_region||fresh.uid!=held.uid||actual.map!=owner.map||actual.root!=owner.root||
               actual.toon!=owner.toon||actual.region!=owner.region||actual.manager!=owner.manager||
               fresh.registered!=held.registered||fresh.on!=held.on||fresh.off!=held.off)
                return fail(e,"Subscriber loaded login owner changed before last Region initialization");
            // DoLogIn sets this field before Manager::LogIn; offline PostResolve leaves it null.
            if(!compare_owned_pointer(owner.toon+0x9a8,0,owner.region,e))return false;
            uintptr_t last=0;
            if(!at(owner.toon,0x9a8,last,e)||last!=owner.region)
                return fail(e,e.empty()?"Subscriber loaded login last Region initialization readback differs; no replay":e);
        }
        // Native login owns actor creation, OnMap, Region registration and NotifyLogin.
        calls.login(owner.manager,owner.toon);
    }else {
        // Reproduce the native Toon lifecycle without Subscriber AI/RNG/unsubscribe decisions.
        calls.logout(owner.toon);
        if(!online_queues(owner.manager,{owner.toon},calls,e))return false;
        calls.notify_logout(before.subscriber_manager,subscriber);
        calls.queues(owner.manager);
    }
    uint64_t uid=0;uintptr_t toon=0,reverse=0,actor=0;uint8_t logged=0,logging_out=1,queued=1;
    if(!at(subscriber,0x80,toon,e)||toon!=owner.toon||!at(toon,0x18,uid,e)||uid!=held.uid||
       !at(toon,0x940,reverse,e)||reverse!=subscriber||!at(toon,0x825,logged,e)||logged!=uint8_t(target.has_value())||
       !at(toon,0x9ed,logging_out,e)||logging_out||!at(toon,0x9ec,queued,e)||queued||!at(toon,0x6f0,actor,e)||bool(actor)!=bool(target))return fail(e,e.empty()?"Subscriber original lifecycle Toon/UID/actor readback differs; no replay":e);
    if(target){
        uintptr_t last=0,region=0,shard=0;
        if(!at(owner.toon,0x9a8,last,e)||!at(owner.toon,0x178,region,e)||!at(owner.toon,0x170,shard,e)||
           last!=owner.region||region!=owner.region||shard!=owner.root)
            return fail(e,e.empty()?"Subscriber original login registered Region readback differs; no replay":e);
    }
    if(!online_queues(owner.manager,{},calls,e))return false;
    OnlineOwner after;if(!online_partition(before,after,e))return false;
    auto members=[](const std::vector<uintptr_t>& rows){return std::set<uintptr_t>(rows.begin(),rows.end());};
    auto on=members(held.on),off=members(held.off),registered=members(held.registered);
    if(target){on.insert(subscriber);off.erase(subscriber);registered.insert(toon);}
    else {on.erase(subscriber);off.insert(subscriber);registered.erase(toon);}
    std::vector<uintptr_t> actual_registered;uintptr_t manager=0;
    if(members(after.on)!=on||members(after.off)!=off||!motion_manager(p,owner.region,manager,actual_registered,e)||manager!=owner.manager||members(actual_registered)!=registered)return fail(e,e.empty()?"Subscriber original lifecycle changed foreign membership; no replay":e);
    if(target){
        std::optional<Motion> actual;MotionOwner actual_owner;
        if(!motion_read(p,subscriber,actual,e,&actual_owner)||!actual||actual_owner.toon!=toon)return fail(e,e.empty()?"Subscriber original login pose readback differs; no replay":e);
        if(*actual!=*target&&!move(*actual,*target,e))return false;
    }
    std::optional<Motion> final;
    if(!motion_read(p,subscriber,final,e)||final!=target)return fail(e,e.empty()?"Subscriber original lifecycle full pose readback differs; no replay":e);
    e.clear();return true;
}
}
bool NativePort::preflight_lifecycle(const Owners& before,const State& target,std::string& e){
    if(!admit_existing(before,target,e)||!current(before,0,true,e))return false;
    return preflight_lifecycle_body(before,target,e);
}
bool NativePort::preflight_lifecycle_body(const Owners& before,const State& target,std::string& e){
    const auto calls=online_calls(before.history.pin.image);
    for(size_t i=0;i<target.subscribers.size();++i)if(before.state.subscribers[i].motion.has_value()!=target.subscribers[i].motion.has_value()){
        OnlineOwner owner;if(!online_prepare(before,i,target.subscribers[i].motion,calls,owner,e)){e="Subscriber lifecycle preflight id="+std::to_string(target.subscribers[i].serialized_id)+" original="+e;return false;}
    }
    e.clear();return true;
}
bool NativePort::store_lifecycle(const Owners& before,size_t index,const std::optional<Motion>& old,const std::optional<Motion>& target,std::string& e){
    if(index>=before.subscribers.size())return fail(e,"Subscriber lifecycle saved-ID slot invalid");
    if(!current_subscriber(before,index,e))return false;
    const auto good=online_apply(before,index,old,target,online_calls(before.history.pin.image),
        [&](const Motion& from,const Motion& to,std::string& error){return store_motion(before,index,from,to,error);},e);
    if(!good)e="Subscriber original lifecycle id="+std::to_string(before.state.subscribers[index].serialized_id)+" target_online="+std::to_string(target.has_value())+" original="+e;
    return good;
}
