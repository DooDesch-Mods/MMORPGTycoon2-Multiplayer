namespace {
namespace ml=live_host_state::monsters;
using LiveUpdate=void(*)(uintptr_t,float);
using LiveAdjust=float(*)(uintptr_t,int32_t,float,uintptr_t,bool);
struct LiveHook {uintptr_t rva=0;void* original=nullptr;std::array<unsigned char,32> entry{};std::array<unsigned char,64> trampoline{};};
LiveHook live_update_hook{0x3c0750},live_adjust_hook{0x39fb80};
bool live_hook_attempted=false,live_hook_ready=false;
std::string live_hook_failure;
void live_update_entry(uintptr_t,float);
float live_adjust_entry(uintptr_t,int32_t,float,uintptr_t,bool);
bool live_hook_current(Memory& m,const LiveHook& hook,std::string& e){
    std::array<unsigned char,32> entry{};std::array<unsigned char,64> trampoline{};
    if(!hook.original||!m.copy(state.image+hook.rva,entry.data(),entry.size(),e)||entry!=hook.entry||
       !m.copy(uintptr_t(hook.original),trampoline.data(),trampoline.size(),e)||trampoline!=hook.trampoline)
        return fail(e,e.empty()?"Monster live owned hook entry/trampoline changed":e);
    return true;
}
bool live_code(Memory& m,bool hooked,std::string& e){
    for(const auto& c:monster_live_codes::values){std::string bytes(c.size,'\0'),sha;
        bool observer=false;if(!interventions::actors::live::verify_observer(c.rva,c.sha,observer,e))return false;if(observer)continue;
        if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e))return false;
        if(c.tail&&hooked){const auto& hook=c.rva==live_update_hook.rva?live_update_hook:live_adjust_hook;
            if(!live_hook_current(m,hook,e)||!quest_bridge::sha256(bytes.substr(5),sha,e)||sha!=c.tail)
                return fail(e,e.empty()?"Monster live original tail differs RVA="+std::to_string(c.rva):e);
        }else if(!quest_bridge::sha256(bytes,sha,e)||sha!=c.sha)return fail(e,e.empty()?"Monster live body differs RVA="+std::to_string(c.rva):e);
    }return true;
}
bool live_hooks(std::string& e){
    if(live_hook_ready){Memory m;return live_code(m,true,e);}
    if(live_hook_attempted)return fail(e,"Monster live hook installation already attempted: "+live_hook_failure);
    live_hook_attempted=true;Memory m;
    const auto rejected=[&](const std::string& cause){live_hook_failure=cause;return fail(e,cause);};
    if(!live_code(m,false,e))return rejected(e);
    for(auto pair:{std::pair<LiveHook*,void*>{&live_update_hook,reinterpret_cast<void*>(&live_update_entry)},
                   {&live_adjust_hook,reinterpret_cast<void*>(&live_adjust_entry)}}){
        auto& hook=*pair.first;const auto target=reinterpret_cast<void*>(state.image+hook.rva);
        auto result=MH_CreateHook(target,pair.second,&hook.original);
        if(result!=MH_OK||!hook.original){std::string cause="Monster live hook prepare RVA="+std::to_string(hook.rva)+" status="+std::to_string(result);
#ifdef MT2MP_FRAMEWORK_PLUGIN
            cause+=" original="+hooks::framework::process_backend().error();
#endif
            return rejected(cause);}
        result=MH_EnableHook(target);
        if(result!=MH_OK){std::string cause="Monster live hook enable RVA="+std::to_string(hook.rva)+" status="+std::to_string(result);
#ifdef MT2MP_FRAMEWORK_PLUGIN
            cause+=" original="+hooks::framework::process_backend().error();
#endif
            return rejected(cause);}
        if(!m.copy(state.image+hook.rva,hook.entry.data(),hook.entry.size(),e)||
           !m.copy(uintptr_t(hook.original),hook.trampoline.data(),hook.trampoline.size(),e))return rejected(e);
    }
    if(!live_code(m,true,e))return rejected(e);live_hook_ready=true;return true;
}
uint32_t live_max(uintptr_t object,int32_t attribute,float& out)noexcept{
    __try{out=reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x39f6e0)(object,attribute);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_animation_read(uintptr_t character,uintptr_t actor,int32_t index,bool& playing,float& time,float& length,bool& paused,float& speed)noexcept{
    __try{playing=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(state.image+0x28760)(character,index);
        const auto local=*reinterpret_cast<const int32_t*>(character+8+size_t(index)*4);
        length=reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x23860)(actor,local);
        if(playing){time=reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x23890)(actor,local);
            paused=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(state.image+0x23840)(actor,local);
            speed=reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x238b0)(actor,local);}return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_transform(uintptr_t object,const subscriber_motion::Pose& pose)noexcept{
    alignas(4) std::array<unsigned char,0x6c> transform{};
    __try{reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(state.image+0x9ff370)(transform.data(),pose.orientation.data(),pose.position.data(),pose.scale.data());
        reinterpret_cast<void(*)(uintptr_t,const void*)>(state.image+0xc67c80)(object+0x30,transform.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_animation_set(uintptr_t character,uintptr_t actor,int32_t semantic,int32_t index,bool playing,uint32_t time,bool paused,uint32_t speed)noexcept{
    __try{if(playing){reinterpret_cast<void(*)(uintptr_t,int32_t)>(state.image+0x28730)(character,semantic);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(state.image+0x237a0)(actor,index,std::bit_cast<float>(speed));
            reinterpret_cast<void(*)(uintptr_t,int32_t,bool)>(state.image+0x23770)(actor,index,paused);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(state.image+0x237e0)(actor,index,std::bit_cast<float>(time));}
        else reinterpret_cast<void(*)(uintptr_t,int32_t)>(state.image+0x28750)(character,semantic);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_spatial(uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(state.image+0x3b1b50)(object);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_byte(uintptr_t address,uint8_t expected,uint8_t target,bool& exchanged)noexcept{
    __try{exchanged=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(target),char(expected)))==expected;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_present(uintptr_t object,uintptr_t character,uintptr_t actor,float dt)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(state.image+0x39d670)(object);
        const bool visible=*reinterpret_cast<const uint8_t*>(object+0x368)&&*reinterpret_cast<const uint8_t*>(object+0x808);
        reinterpret_cast<void(*)(uintptr_t,bool)>(state.image+0x287d0)(character,visible);
        const float elapsed=*reinterpret_cast<const uint8_t*>(object+0x26c)?0.0f:dt;
        reinterpret_cast<void(*)(uintptr_t,float)>(state.image+0x28970)(character,elapsed);
        reinterpret_cast<void(*)(uintptr_t)>(state.image+0x21df0)(actor);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_scene(uintptr_t screen,uintptr_t& scene)noexcept{
    __try{scene=reinterpret_cast<uintptr_t(*)(uintptr_t,int32_t)>(state.image+0x9ca3a0)(screen,0);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_membership_call(uintptr_t receiver,uintptr_t object,bool& present)noexcept{
    __try{present=reinterpret_cast<bool(*)(uintptr_t,uintptr_t)>(state.image+0x874df0)(receiver,object);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_lifecycle_call(uintptr_t rva,uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(state.image+rva)(object,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_expire(uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(state.image+0x39d560)(object);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct LiveWorld {host::WorldState world;host::LookupApi api;};
struct LiveLocal {identity::Entry entry;mr::Actor actor;std::array<int32_t,ml::animation_slots> animation_index{};std::array<float,ml::animation_slots> animation_length{};};
class MonsterLivePort final:public live_host_state::MonsterPort {
    a::Context context_;a::Binding binding_;uint64_t epoch_=0;
    bool bound_=false,suspended_=true,locked_=false;
    std::string cause_,prior_cause_;
    uint64_t revision_=0,updates_=0,damage_=0,failures_=0;json last_=nullptr,presentation_=nullptr,callback_scope_=nullptr;
    json lifecycle_transitions_=json::array();std::optional<ml::State> traced_;std::set<std::string> wounded_;uint64_t lifecycle_transition_dropped_=0;
    std::map<std::string,LiveLocal> locals_;
    std::atomic<bool> client_{false},wrong_thread_{false};
    bool reject(std::string& e,const std::string& cause,bool lock=false){if(lock){++failures_;const bool first=!locked_;locked_=true;if(cause_.empty())cause_=cause;if(first){live_host_state::economic_fence().journal_failed(cause_);LOG_ERROR("[MonsterLive] {}",cause_);}}return fail(e,cause);}
    bool world(Memory& m,LiveWorld& p,std::string& e,bool idle=true){
        if(!bound_||suspended_||locked_||GetCurrentThreadId()!=state.thread)return fail(e,cause_.empty()?"Monster live owner suspended, locked or outside Mainthread":cause_);
        if(wrong_thread_.load(std::memory_order_acquire))return reject(e,"Monster live owned callback entered outside Mainthread",true);
        a::Context current;if(!a::selected_session().current(current,e)||current!=context_)return fail(e,e.empty()?"Monster live current Session differs":e);
        if(!host::resolve_lookup(m.process,state.image,exe_sha,p.api,e))return false;
        host::NativeOrigin origin;host::Reader reader(m.process,origin,state.image,state.thread,p.api);
        if(!reader.current(p.world,idle,e))return fail(e,e+"; Monster live world require_idle="+std::to_string(idle)+
            " saves="+std::to_string(p.world.saves)+" edits="+std::to_string(p.world.edits)+" active_graph="+std::to_string(p.world.active_graph));
        if(!action_runtime::bound(p.world,binding_,context_.snapshot_uuid,e))return false;
        int32_t generating=-1;if(!field(m,uintptr_t(p.world.origin.map),0x3d4,generating,e)||generating!=7)return fail(e,e.empty()?"Monster live requires completed map generation state7":e);
        native::ClockState clock;if(!native::read_clock(clock,e))return false;
        if(context_.role==a::Role::Client&&clock.time_scale!=0)return fail(e,"Monster live Client actual Clock is not zero");return true;
    }
    bool full(Memory& m,const LiveWorld& p,uintptr_t object,identity::Entry& entry,std::string& birth,std::string& e){
        uintptr_t ap=0,root=0,region=0,manager=0,zone=0,proxy=0,owner=0,found=0;uint64_t uid=0;
        if(!field(m,object,0,ap,e)||ap!=state.image+0x16c0770||!field(m,object,0x18,uid,e)||!uid||
           !field(m,object,0x170,root,e)||root!=p.world.root||!field(m,object,0x178,region,e)||
           !field(m,region,0x988,manager,e)||!field(m,manager,0,ap,e)||ap!=state.image+0x1714100||
           !field(m,manager,0x80,owner,e)||owner!=region||!field(m,object,0x8d0,zone,e)||zone||!field(m,object,0x8d8,proxy,e)||proxy)
            return fail(e,e.empty()?"Monster live full class/owner/null-Zone binding differs":e);
        const auto seh=lookup(p.api.find,uid,found);if(seh||found!=object)return fail(e,"Monster live full UID lookup differs SEH="+std::to_string(seh));
        uint64_t after_uid=0;if(!field(m,object,0x18,after_uid,e)||after_uid!=uid)return fail(e,e.empty()?"Monster live full UID changed after lookup":e);
        const identity::Scope scope{p.world.origin.uuid,p.world.origin.world_epoch,p.world.origin.generation,p.world.root,region,manager};std::string key;
        if(!identity::registry.resolve(scope,object,uid,key,e)||!identity::registry.lookup(monster_target_key::argument(key),scope,entry,e))return false;
        q::Bounds members;d::ReadMemory read{m};if(!q::typed(read,state.image,manager+0x38,0x16e8b60,e)||!q::bounds(read,manager+0x38,4096,members,e))return false;
        std::vector<uintptr_t> rows(size_t(members.count));if(members.count&&!m.copy(members.data,rows.data(),rows.size()*8,e))return false;
        if(std::count(rows.begin(),rows.end(),object)!=1||!q::unchanged(read,manager+0x38,4096,members,e))return fail(e,e.empty()?"Monster live current owning membership differs":e);
        birth=monster_target_key::argument(key);return true;
    }
    bool actor(Memory& m,uintptr_t object,LiveLocal& out,std::string& e){
        if(!move_actor(m,object,out.actor,e))return false;uint8_t follows=1,ride=1,riding=1;uintptr_t puppet=1;
        if(!field(m,object,0x490,follows,e)||follows||!field(m,object,0x478,ride,e)||ride||!field(m,object,0x47d,riding,e)||riding||
           !field(m,object,0x628,puppet,e)||puppet)return fail(e,e.empty()?"Monster live fixed Region or plain actor attachment is unsupported":e);
        uintptr_t render=0,callback=0;int32_t count=-1;
        if(!field(m,object,0x30,render,e)||render!=state.image+0x16c0c58||!field(m,render,0x38,callback,e)||callback!=state.image+0x39e750||
           !field(m,out.actor.actor,0x20,count,e)||count<0||count>4096)return fail(e,e.empty()?"Monster live embedded transform or animation bounds differ":e);
        if(out.actor.character>UINTPTR_MAX-8-sizeof(out.animation_index)||!m.copy(out.actor.character+8,out.animation_index.data(),sizeof(out.animation_index),e))return false;
        for(const auto index:out.animation_index)if(index>=count)return fail(e,"Monster live semantic animation index outside local clip array");
        return true;
    }
    bool target(Memory& m,const LiveWorld& p,uintptr_t wrapper,ml::TargetObservation& out,std::string& e){
        uintptr_t object=0,proxy=0,live=0;if(!field(m,wrapper,8,object,e)||!field(m,wrapper,0x10,proxy,e))return false;
        if(!object&&!proxy){out={};return true;}if(!proxy){out={"expired",{}};return true;}
        if(!field(m,proxy,8,live,e))return false;if(!live||!object){out={"expired",{}};return true;}
        uintptr_t ap=0;if(!field(m,object,0,ap,e))return false;
        if(ap==state.image+0x16c0770){uintptr_t zone=0;if(!field(m,object,0x8d0,zone,e))return false;
            if(!zone){identity::Entry entry;std::string birth;if(!full(m,p,object,entry,birth,e))return false;out={"mapped",monster_target_key::canonical(birth)};return true;}}
        if(ap==state.image+0x1744e90){uintptr_t subscriber=0,back=0;int32_t id=-1;
            if(!field(m,object,0x940,subscriber,e)||!field(m,subscriber,0,ap,e)||ap!=state.image+0x16e75f0||
               !field(m,subscriber,0x80,back,e)||back!=object||!field(m,subscriber,0x20,id,e)||id<0)return fail(e,e.empty()?"Monster observed Toon target reciprocal Subscriber differs":e);
            out={"mapped","subscriber:"+std::to_string(id)};return true;}
        out={"unmapped",{}};return true;
    }
    struct Membership {uintptr_t spatial=0,quadtree=0,model=0;std::vector<uintptr_t> ready;bool registered=false,available=false;};
    bool membership(Memory& m,const LiveWorld& p,const LiveLocal& local,Membership& out,std::string& e){
        out={};const auto object=local.entry.pointer,manager=local.entry.scope.manager,region=local.entry.scope.region;
        uintptr_t data=0,owner=0,root=0,map=0;int32_t count=-1,capacity=-1,kind=-1;
        if(!field(m,manager,0x80,owner,e)||owner!=region||!field(m,region,0x70,root,e)||root!=p.world.root||
           !field(m,manager,0x70,data,e)||!field(m,manager,0x78,count,e)||!field(m,manager,0x7c,capacity,e)||
           count<0||capacity<count||capacity>65536||(capacity&&!data)||!field(m,manager,0x88,out.spatial,e)||!out.spatial)
            return fail(e,e.empty()?"Monster lifecycle ready owner/array differs":e);
        out.ready.resize(size_t(count));if(count&&!m.copy(data,out.ready.data(),out.ready.size()*8,e))return false;
        std::set<uintptr_t> unique(out.ready.begin(),out.ready.end());if(unique.size()!=out.ready.size()||unique.contains(0))return fail(e,"Monster lifecycle ready members not unique");
        bool spatial=false;const auto a=live_membership_call(region+0x7c8,object,out.registered),b=live_membership_call(out.spatial,object,spatial);
        if(a||b)return fail(e,"Monster lifecycle original membership lookup SEH="+std::to_string(a)+"/"+std::to_string(b));
        out.available=unique.contains(object);
        if(out.available!=spatial||out.registered!=local.actor.on_map)return fail(e,"Monster lifecycle ready/registry membership differs from native flags");
        uintptr_t after_data=0;int32_t after_count=-1,after_capacity=-1;
        if(!field(m,manager,0x70,after_data,e)||!field(m,manager,0x78,after_count,e)||!field(m,manager,0x7c,after_capacity,e)||
           after_data!=data||after_count!=count||after_capacity!=capacity)return fail(e,e.empty()?"Monster lifecycle ready array changed during read":e);
        // World-map Shard::GetQuadTree selects the current Map singleton +290.
        if(!field(m,p.world.root,0x48,kind,e)||kind!=0||!field(m,state.image,0x13df8c0,map,e)||map!=uintptr_t(p.world.origin.map)||
           !field(m,map,0x290,out.quadtree,e)||!out.quadtree||!field(m,object,0x488,out.model,e))
            return fail(e,e.empty()?"Monster lifecycle current world-map quadtree receiver differs":e);
        if(bool(out.model)!=local.actor.on_map)return fail(e,"Monster lifecycle quadtree model/OnMap differs");
        if(out.model){uintptr_t prop=0,transform=0;uint32_t type=0;
            if(!field(m,out.model,0x118,prop,e)||prop!=object||!field(m,out.model,8,transform,e)||transform!=object+0x30||
               !field(m,out.model,0,type,e)||type!=0x10)return fail(e,e.empty()?"Monster lifecycle quadtree model owner/type differs":e);}
        return true;
    }
    bool expire_proxy(Memory& m,uintptr_t object,bool& attempted,std::string& e){
        uintptr_t proxy=0,owner=0;int32_t references=-1;if(!field(m,object,0x600,proxy,e))return false;if(!proxy)return true;
        if(!field(m,proxy,8,owner,e)||owner!=object||!field(m,proxy,0x10,references,e)||references<0)
            return fail(e,e.empty()?"Monster death current live proxy owner/refcount differs":e);
        attempted=true;const auto seh=live_expire(object);if(seh)return fail(e,"Monster original live proxy expiry SEH="+std::to_string(seh));
        uintptr_t after=1;if(!field(m,object,0x600,after,e)||after)return fail(e,e.empty()?"Monster original proxy remained bound after death":e);
        if(references){int32_t remaining=-1;if(!field(m,proxy,8,owner,e)||owner||!field(m,proxy,0x10,remaining,e)||remaining!=references)
            return fail(e,e.empty()?"Monster expired proxy receiver/refcount readback differs":e);}
        return true;
    }
    bool lifecycle_scalars(Memory& m,uintptr_t object,const ml::Row& before,const ml::Row& target,bool& attempted,std::string& e){
        const auto store=[&](size_t offset,uint32_t old,uint32_t wanted){if(old==wanted)return true;attempted=true;
            return live_host_state::compare_fixed_i32(object+offset,std::bit_cast<int32_t>(old),std::bit_cast<int32_t>(wanted),e);};
        for(size_t i=0;i<3;++i){uintptr_t ap=0;if(!field(m,object,0x730+i*0x28,ap,e)||ap!=state.image+0x16dd550)return fail(e,e.empty()?"Monster embedded attribute class differs":e);}
        for(size_t i=0;i<3;++i)if(!store(0x750+i*0x28,before.host_combat.values[i],target.host_combat.values[i])||
            !store(0x754+i*0x28,before.host_combat.recovery[i],target.host_combat.recovery[i]))return false;
        if(!store(0x828,std::bit_cast<uint32_t>(before.host_combat.ai),std::bit_cast<uint32_t>(target.host_combat.ai))||
           !store(0x824,std::bit_cast<uint32_t>(before.host_combat.status),std::bit_cast<uint32_t>(target.host_combat.status))||
           !store(0x910,before.lifecycle.respawn_time_bits,target.lifecycle.respawn_time_bits)||
           !store(0x830,std::bit_cast<uint32_t>(before.spawn[1]),std::bit_cast<uint32_t>(target.spawn[1]))||
           !store(0x7f8,std::bit_cast<uint32_t>(before.lifecycle.action),std::bit_cast<uint32_t>(target.lifecycle.action)))return false;
        if(before.host_combat.dead!=target.host_combat.dead){bool exchanged=false;attempted=true;
            const auto code=live_byte(object+0x4ac,before.host_combat.dead,target.host_combat.dead,exchanged);
            if(code||!exchanged)return fail(e,"Monster dead byte CAS differs SEH="+std::to_string(code));}
        return true;
    }
    void trace(const ml::State& before,const ml::State& after,const ml::State& host,uint64_t revision){
        for(const auto& row:after.rows){const auto old=std::find_if(before.rows.begin(),before.rows.end(),[&](const auto& r){return r.birth==row.birth;});
            const auto wanted=std::find_if(host.rows.begin(),host.rows.end(),[&](const auto& r){return r.birth==row.birth;});
            if(old==before.rows.end()||wanted==host.rows.end()||!ml::same_applied(row,*wanted))continue;
            std::vector<std::string> phases;
            if(old->host_combat.dead&&!row.host_combat.dead){phases.push_back("respawn");wounded_.erase(row.birth);}
            const auto hp=std::bit_cast<float>(row.host_combat.values[0]),old_hp=std::bit_cast<float>(old->host_combat.values[0]);
            if(hp>0&&hp<old_hp&&wounded_.insert(row.birth).second)phases.push_back("wounded");
            if(!old->host_combat.dead&&row.host_combat.dead)phases.push_back("dead");
            if(old->on_map&&!row.on_map)phases.push_back("offmap");
            for(const auto& phase:phases){if(lifecycle_transitions_.size()>=32){++lifecycle_transition_dropped_;continue;}
                const auto& local=locals_.at(row.birth);
                lifecycle_transitions_.push_back({{"phase",phase},{"revision",revision},{"birth",row.birth},{"local_uid",local.entry.uid},
                    {"before",ml::encode({{*old},{}}).at("rows").at(0)},{"after",ml::encode({{row},{}}).at("rows").at(0)},
                    {"host_row",ml::encode({{*wanted},{}}).at("rows").at(0)}});}
        }
    }
    bool row(Memory& m,const LiveWorld& p,uintptr_t object,ml::Row& out,LiveLocal& local,std::string& e){
        if(!full(m,p,object,local.entry,out.birth,e)||!actor(m,object,local,e)||!npc_census::read_name(m,object,out.name,e))return false;
        if(!field(m,local.entry.scope.region,0x834,out.pose.region,e)||!field(m,object,0x820,out.tag,e)||!field(m,object,0x838,out.rank,e)||
           !field(m,object,0x4a8,out.slot,e)||!field(m,object,0x4a4,out.type,e)||!field(m,object,0x6f8,out.level,e)||
           !field(m,object,0x100,out.pose.orientation,e)||!field(m,object,0x110,out.pose.position,e)||!field(m,object,0x11c,out.pose.scale,e))return false;
        Membership members;if(!membership(m,p,local,members,e))return false;out.lifecycle.ready=members.available;
        if(!field(m,object,0x910,out.lifecycle.respawn_time_bits,e)||!field(m,object,0x7f8,out.lifecycle.action,e))return false;
        out.spawn=local.actor.spawn;out.on_map=local.actor.on_map;auto& combat=out.host_combat;uint8_t dead=2;
        if(!field(m,object,0x4ac,dead,e)||dead>1||!field(m,object,0x828,combat.ai,e)||!field(m,object,0x824,combat.status,e))return fail(e,e.empty()?"Monster live dead/status observation differs":e);combat.dead=dead!=0;
        for(size_t i=0;i<3;++i){uintptr_t ap=0;if(!field(m,object,0x730+i*0x28,ap,e)||ap!=state.image+0x16dd550)return fail(e,e.empty()?"Monster embedded attribute class differs":e);
            if(!field(m,object,0x750+i*0x28,combat.values[i],e)||!field(m,object,0x754+i*0x28,combat.recovery[i],e))return false;
            float maximum=0;const auto seh=live_max(object,int32_t(i),maximum);if(seh)return fail(e,"Monster Host maximum observation SEH="+std::to_string(seh));combat.maxima[i]=std::bit_cast<uint32_t>(maximum);}
        for(size_t i=0;i<ml::animation_slots;++i){
            if(local.animation_index[i]>=0){bool playing=false,paused=false;float time=0,length=0,speed=0;const auto code=live_animation_read(local.actor.character,local.actor.actor,int32_t(i),playing,time,length,paused,speed);
                const auto detail=[&]{return " semantic="+std::to_string(i)+" index="+std::to_string(local.animation_index[i])+" time="+std::to_string(time)+
                    " time_bits="+std::to_string(std::bit_cast<uint32_t>(time))+" length="+std::to_string(length)+" length_bits="+std::to_string(std::bit_cast<uint32_t>(length));};
                if(code)return fail(e,"Monster native animation observation SEH="+std::to_string(code)+detail());
                if(!std::isfinite(length)||length<=0)return fail(e,"Monster local animation asset length invalid"+detail());local.animation_length[i]=length;out.animation_length[i]=std::bit_cast<uint32_t>(length);
                // Native loop/ping-pong updates retain signed overshoot; SetTime and Apply use this raw time.
                if(playing){if(!std::isfinite(time))return fail(e,"Monster animation time is not finite"+detail());if(!std::isfinite(speed))return fail(e,"Monster animation speed is not finite"+detail());out.animation[i]=std::bit_cast<uint32_t>(time);out.animation_paused[i]=paused;out.animation_speed[i]=std::bit_cast<uint32_t>(speed);}}
        }
        return target(m,p,object+0x6d8,combat.target,e)&&target(m,p,object+0x8b0,combat.aggro,e);
    }
    bool read(ml::State& out,std::string& e){
        out={};locals_.clear();Memory m;LiveWorld p;if(!world(m,p,e))return false;
        std::vector<std::pair<std::string,identity::Entry>> entries;if(!identity::registry.enumerate(p.world.origin.uuid,p.world.origin.world_epoch,p.world.origin.generation,p.world.root,entries,e))return false;
        std::map<std::string,identity::Entry> expected;for(const auto& [birth,entry]:entries)if(entry.retired)out.retired.push_back(birth);else expected.emplace(birth,entry);
        d::ReadMemory memory{m};q::Bounds regions;if(!q::typed(memory,state.image,uintptr_t(p.world.origin.map)+0x230,0x16f04e0,e)||!q::bounds(memory,uintptr_t(p.world.origin.map)+0x230,4096,regions,e))return false;
        std::set<int32_t> ids;std::set<uintptr_t> region_pointers,objects;
        for(int32_t i=0;i<regions.count;++i){uintptr_t region=0,ap=0,root=0,manager=0,owner=0;int32_t id=-1;
            if(!field(m,regions.data,size_t(i)*8,region,e)||!field(m,region,0,ap,e)||ap!=state.image+0x1787930||!field(m,region,0x70,root,e)||root!=p.world.root||
               !field(m,region,0x834,id,e)||id<0||!ids.insert(id).second||!region_pointers.insert(region).second||!field(m,region,0x988,manager,e)||
               !field(m,manager,0,ap,e)||ap!=state.image+0x1714100||!field(m,manager,0x80,owner,e)||owner!=region)return fail(e,e.empty()?"Monster live complete Region/manager census differs":e);
            q::Bounds members;if(!q::typed(memory,state.image,manager+0x38,0x16e8b60,e)||!q::bounds(memory,manager+0x38,4096,members,e))return false;
            for(int32_t j=0;j<members.count;++j){uintptr_t object=0,zone=0,proxy=0;
                if(!field(m,members.data,size_t(j)*8,object,e)||!object||!objects.insert(object).second||!field(m,object,0,ap,e)||ap!=state.image+0x16c0770||
                   !field(m,object,0x8d0,zone,e)||!field(m,object,0x8d8,proxy,e))return fail(e,e.empty()?"Monster live owning row class/uniqueness differs":e);
                if(zone)continue;if(proxy)return fail(e,"Monster live null-Zone row retains proxy");ml::Row value;LiveLocal local;
                if(!row(m,p,object,value,local,e))return false;auto found=expected.find(value.birth);
                if(found==expected.end()||local.entry.scope.region!=region||local.entry.scope.manager!=manager||local.entry.pointer!=found->second.pointer||local.entry.uid!=found->second.uid)return fail(e,"Monster live unbound/mismatched birth in complete census");
                if(!locals_.emplace(value.birth,local).second)return fail(e,"Monster live duplicate current birth");out.rows.push_back(std::move(value));}
            if(!q::unchanged(memory,manager+0x38,4096,members,e))return false;
        }
        if(!q::unchanged(memory,uintptr_t(p.world.origin.map)+0x230,4096,regions,e)||out.rows.size()!=expected.size())return fail(e,e.empty()?"Monster live confirmed birth absent from complete current census":e);
        std::sort(out.rows.begin(),out.rows.end(),[](const auto& a,const auto& b){return a.birth<b.birth;});
        LiveWorld after;if(!world(m,after,e)||!same_world(p.world,after.world))return fail(e,e.empty()?"Monster live World changed during complete sample":e);
        return ml::valid(out,e);
    }
    bool set(const ml::Row& before,const ml::Row& target,bool& attempted,std::string& e){
        attempted=false;ml::State sampled;if(!read(sampled,e))return false;
        const auto found=std::find_if(sampled.rows.begin(),sampled.rows.end(),[&](const auto& value){return value.birth==before.birth;});
        if(found==sampled.rows.end()||*found!=before)return fail(e,"Monster live current row changed before original transform birth="+before.birth);
        const auto local=locals_.at(before.birth);Memory m;LiveWorld current;
        if(!world(m,current,e))return false;
        Pin p;d::Sample prior;if(!pin(m,binding_,context_.snapshot_uuid,before.pose.region,p,e)||!d::sample(m,p.region,prior,e))return false;
        if(prior.undo.active_graph)return fail(e,"Monster live transform requires no active Undo graph");
        const auto object=local.entry.pointer;Membership prior_members;
        if(!membership(m,current,local,prior_members,e))return false;
        if(!before.host_combat.dead&&target.host_combat.dead&&!expire_proxy(m,object,attempted,e))return false;
        if(before.lifecycle.ready&&!target.lifecycle.ready){attempted=true;const auto code=live_lifecycle_call(0x3bbc00,object,false);
            if(code)return fail(e,"Monster original combat membership removal SEH="+std::to_string(code));}
        if(before.on_map&&!target.on_map){attempted=true;const auto code=live_lifecycle_call(0x39a920,object,false);
            if(code)return fail(e,"Monster original off-map transition SEH="+std::to_string(code));}
        const bool current_on_map=before.on_map&&target.on_map;auto expected_spawn=before.spawn;
        const auto stable=[&](const mr::Actor& actual,bool on){return mr::same_actor(local.actor,actual)&&actual.spawn==expected_spawn&&actual.on_map==on;};
        if(before.pose!=target.pose){attempted=true;const auto seh=live_transform(object,target.pose);
            if(seh)return fail(e,"Monster live original transform SEH="+std::to_string(seh));
            identity::Entry entry;std::string birth;LiveLocal changed;
            if(!full(m,current,object,entry,birth,e)||entry!=local.entry||birth!=before.birth||!actor(m,object,changed,e)||!stable(changed.actor,current_on_map))
                return fail(e,e.empty()?"Monster live original transform changed current identity/actor/spawn":e);
            struct Spatial {
                Memory& memory;
                bool read_byte(uintptr_t address,uint8_t& value,std::string& error){return field(memory,address,0,value,error);}
                bool update_spatial(uintptr_t target,std::string& error){const auto code=live_spatial(target);return !code||fail(error,"Monster live original quadtree update SEH="+std::to_string(code));}
                bool compare_byte(uintptr_t address,uint8_t expected,uint8_t target,std::string& error){bool exchanged=false;const auto code=live_byte(address,expected,target,exchanged);
                    return (!code&&exchanged)||fail(error,"Monster live spatial byte CAS differs SEH="+std::to_string(code));}
            } spatial{m};
            if(!ml::clear_spatial_dirty(spatial,object,attempted,e))return false;
        }
        if(!before.on_map&&target.on_map){attempted=true;const auto code=live_lifecycle_call(0x39a920,object,true);
            if(code)return fail(e,"Monster original on-map transition SEH="+std::to_string(code));}
        if(!before.lifecycle.ready&&target.lifecycle.ready){attempted=true;const auto code=live_lifecycle_call(0x3bbc00,object,true);
            if(code)return fail(e,"Monster original combat membership addition SEH="+std::to_string(code));}
        identity::Entry scalar_entry;std::string scalar_birth;LiveLocal scalar_local;
        if(!full(m,current,object,scalar_entry,scalar_birth,e)||scalar_entry!=local.entry||!actor(m,object,scalar_local,e)||!stable(scalar_local.actor,target.on_map))
            return fail(e,e.empty()?"Monster lifecycle scalar current binding differs":e);
        if(!lifecycle_scalars(m,object,before,target,attempted,e))return false;expected_spawn=target.spawn;
        // Stop first, then play and set Host time. Native Play resets a stopped
        // clip to time0; setting its time first would lose the accepted phase.
        for(bool playing:{false,true})for(size_t i=0;i<ml::animation_slots;++i){if(bool(target.animation[i])!=playing||(before.animation[i]==target.animation[i]&&before.animation_paused[i]==target.animation_paused[i]&&before.animation_speed[i]==target.animation_speed[i]))continue;
            identity::Entry entry;std::string birth;LiveLocal fresh;
            if(!full(m,current,object,entry,birth,e)||entry!=local.entry||!actor(m,object,fresh,e)||!stable(fresh.actor,target.on_map)||
               fresh.animation_index!=local.animation_index)return fail(e,e.empty()?"Monster live animation point-of-use binding differs":e);
            if(fresh.animation_index[i]<0)return fail(e,"Monster live accepted semantic clip disappeared");
            attempted=true;const auto seh=live_animation_set(fresh.actor.character,fresh.actor.actor,int32_t(i),fresh.animation_index[i],playing,target.animation[i].value_or(0),target.animation_paused[i],target.animation_speed[i]);
            if(seh)return fail(e,"Monster live original sampled animation SEH="+std::to_string(seh));
        }
        LiveLocal rendered;identity::Entry final_entry;std::string final_birth;
        if(!full(m,current,object,final_entry,final_birth,e)||final_entry!=local.entry||!actor(m,object,rendered,e)||!stable(rendered.actor,target.on_map))
            return fail(e,e.empty()?"Monster live presentation point-of-use actor differs":e);
        attempted=true;if(!present(object,rendered,0.0f,e))return false;
        Pin after;d::Sample result;auto expected_roster=prior.state;
        for(auto& member:expected_roster.monsters)if(member.uid==local.entry.uid)member.status=target.host_combat.status;
        if(!pin(m,binding_,context_.snapshot_uuid,before.pose.region,after,e)||!same_world(p.world,after.world)||!d::sample(m,after.region,result,e)||
           !mr::unchanged_others(expected_roster,result.state,local.entry.uid,e)||prior.state.global_random!=result.state.global_random||
           prior.state.region_random!=result.state.region_random||prior.undo!=result.undo||prior.release_edits!=result.release_edits||ledger(prior.money)!=ledger(result.money))
            return fail(e,e.empty()?"Monster live transform changed current World, roster, counters, RNG, Undo, Release or ledger":e);
        LiveWorld final_world;ml::Row actual;LiveLocal actual_local;
        if(!world(m,final_world,e)||!same_world(current.world,final_world.world)||!row(m,final_world,object,actual,actual_local,e))return false;
        Membership final_members;if(!membership(m,final_world,actual_local,final_members,e))return false;
        auto old_ready=prior_members.ready,new_ready=final_members.ready;
        std::erase(old_ready,object);std::erase(new_ready,object);std::sort(old_ready.begin(),old_ready.end());std::sort(new_ready.begin(),new_ready.end());
        if(old_ready!=new_ready||prior_members.spatial!=final_members.spatial||prior_members.quadtree!=final_members.quadtree)
            return fail(e,"Monster lifecycle changed unrelated ready members or spatial owner");
        if(!ml::same_identity(actual,target)||!ml::same_applied(actual,target)||
           actual.host_combat.target!=before.host_combat.target||actual.host_combat.aggro!=before.host_combat.aggro)
            return fail(e,"Monster live native attributes/lifecycle/pose or untouched target readback differs birth="+before.birth);
        e.clear();return true;
    }
public:
    bool bind(const a::Context& c,const a::Binding& b,uint64_t epoch,std::string& e)override{
        if(!suspended_||!state.ready||GetCurrentThreadId()!=state.thread||!epoch)return fail(e,"Monster live binding requires suspended installed Mainthread owner");
        if(bound_&&locked_&&c==context_&&b==binding_&&epoch==epoch_)return fail(e,"Monster live Unknown requires a fresh World/Session; no same-binding replay");
        context_=c;binding_=b;epoch_=epoch;bound_=true;suspended_=false;prior_cause_=cause_;cause_.clear();locked_=false;wrong_thread_=false;
        ml::State sample;if(!live_hooks(e)||!read(sample,e)){suspended_=true;return false;}
        revision_=updates_=damage_=0;lifecycle_transitions_=json::array();lifecycle_transition_dropped_=0;traced_.reset();wounded_.clear();last_=presentation_=callback_scope_=nullptr;client_.store(c.role==a::Role::Client,std::memory_order_release);return true;
    }
    bool suspend(std::string& e)override{client_.store(false,std::memory_order_release);suspended_=true;locals_.clear();e.clear();return true;}
    bool read_current(const a::Context& c,const a::Binding& b,uint64_t epoch,ml::State& out,std::string& e)override{
        out={};if(!bound_||suspended_||locked_||c!=context_||b!=binding_||epoch!=epoch_||GetCurrentThreadId()!=state.thread)
            return fail(e,"Monster current-read requires its actual active Session/Binding/epoch/Mainthread");
        Memory memory;if(!live_code(memory,true,e))return false;return read(out,e);
    }
    bool capture(const a::Context& c,const a::Binding& b,const live_host_state::Frame& f,ml::State& out,std::string& e)override{
        if(c!=context_||b!=binding_||c.role!=a::Role::Host||f.epoch!=epoch_)return fail(e,"Monster live capture bound Host context differs");Memory m;
        if(!live_code(m,true,e)||!read(out,e))return false;if(traced_)trace(*traced_,out,out,f.revision);traced_=out;last_={{"frame_revision",f.revision},{"host_state",ml::encode(out)},{"combat_observations_applied",false}};return true;
    }
    bool preflight(const a::Context& c,const a::Binding& b,const live_host_state::Frame& f,std::string& e)override{
        if(c!=context_||b!=binding_||c.role!=a::Role::Client||f.epoch!=epoch_||!f.monsters)return fail(e,"Monster live preflight exact Client component/binding differs");Memory m;ml::State before;
        if(!live_code(m,true,e)||!read(before,e)||!ml::preflight(before,*f.monsters,e))return false;
        for(const auto& row:f.monsters->rows){const auto& local=locals_.at(row.birth);for(size_t i=0;i<ml::animation_slots;++i){if(row.animation[i]&&local.animation_index[i]<0)return fail(e,"Monster Host animation has no matching local clip/length birth="+row.birth);
            for(size_t j=0;j<i;++j)if(local.animation_index[i]>=0&&local.animation_index[i]==local.animation_index[j]&&
                (row.animation[i]!=row.animation[j]||row.animation_paused[i]!=row.animation_paused[j]||row.animation_speed[i]!=row.animation_speed[j]))
                return fail(e,"Monster Host semantic aliases disagree on one local clip birth="+row.birth);}}
        return true;
    }
    bool apply(const a::Context& c,const a::Binding& b,const live_host_state::Frame& f,ml::Receipt& out,std::string& e)override{
        out={};if(!preflight(c,b,f,e)){out.cause=e;return false;}
        struct Calls {
            MonsterLivePort& owner;
            ml::State observed_before,observed_after;bool witnessed=false;
            bool read(ml::State& value,std::string& error){return owner.read(value,error);}
            bool set(const ml::Row& before,const ml::Row& target,bool& attempted,std::string& error){return owner.set(before,target,attempted,error);}
            void observed(const ml::State& first,const ml::State& final){observed_before=first;observed_after=final;witnessed=true;}
        } calls{*this};
        out=ml::apply_motion(calls,*f.monsters);
        last_={{"frame_revision",f.revision},{"attempted",out.attempted},{"confirmed",out.confirmed},{"unknown",out.unknown},
            {"attempted_rows",out.attempted_rows},{"completed_rows",out.completed_rows},{"cause",out.cause},
            {"native_readback_present",calls.witnessed},{"before",calls.witnessed?ml::encode(calls.observed_before):json(nullptr)},
            {"after",calls.witnessed?ml::encode(calls.observed_after):json(nullptr)},
            {"host_state",ml::encode(*f.monsters)},{"attributes_lifecycle_applied",out.confirmed},{"combat_observations_applied",false}};
        if(!out.confirmed){e=out.cause;if(out.unknown)reject(e,out.cause,true);return false;}
        trace(calls.observed_before,calls.observed_after,*f.monsters,f.revision);revision_=f.revision;e.clear();return true;
    }
    bool after_action(const a::Context& c,const a::Binding& b,std::string& e)override{if(c!=context_||b!=binding_)return fail(e,"Monster live after-action binding differs");ml::State sample;return read(sample,e);}
    json status()const override{return {{"bound",bound_},{"suspended",suspended_},{"locked",locked_},{"cause",cause_},{"prior_cause",prior_cause_},{"frame_revision",revision_},{"failures",failures_},{"suppressed_updates",updates_},{"suppressed_adjustments",damage_},{"last",last_},{"presentation",presentation_},{"last_scoped_callback",callback_scope_},{"attributes_lifecycle_applied",last_.is_object()&&last_.value("attributes_lifecycle_applied",false)},{"lifecycle_transitions",lifecycle_transitions_},{"lifecycle_transition_dropped",lifecycle_transition_dropped_},{"combat_observations_applied",false}};}
    bool active()const{return client_.load(std::memory_order_acquire);}
    bool classify(uintptr_t object,bool& owned,LiveLocal& local,std::string& e){
        owned=false;if(!active())return true;Memory m;uintptr_t ap=0,zone=0;
        if(!field(m,object,0,ap,e))return false;if(ap!=state.image+0x16c0770)return true;
        if(!field(m,object,0x8d0,zone,e))return false;if(zone)return true;owned=true;
        if(GetCurrentThreadId()!=state.thread){wrong_thread_.store(true,std::memory_order_release);return fail(e,"Monster live owned callback outside Mainthread");}
        // Original manager callbacks can run inside Save/Edit scopes. This
        // read-only classification retains full ownership; census and writes require idle.
        LiveWorld p;std::string birth;if(!world(m,p,e,false)||!full(m,p,object,local.entry,birth,e)||!actor(m,object,local,e))return false;
        if(p.world.saves||p.world.edits||p.world.active_graph)callback_scope_={{"saves",p.world.saves},{"edits",p.world.edits},{"active_graph",p.world.active_graph},{"local_pointer",object},{"local_uid",local.entry.uid}};
        return true;
    }
    void failed(const std::string& e){if(GetCurrentThreadId()!=state.thread){wrong_thread_.store(true,std::memory_order_release);return;}std::string out;reject(out,e.empty()?"Monster live owned callback failed without cause":e,true);}
    bool present(uintptr_t object,LiveLocal& local,float dt,std::string& e){
        if(!std::isfinite(dt)||dt!=0)return fail(e,"Monster live Client original clock interval must be finite zero");Memory m;uintptr_t screen_slot=0,screen=0,scenes=0,expected_scene=0,scene=0,camera=0;int32_t scene_count=0;
        // The image refptr addresses the singleton slot, not the Screen instance.
        if(!field(m,state.image,0x164b420,screen_slot,e)||!screen_slot)return fail(e,e.empty()?"Monster live current Screen singleton slot absent":e);
        if(!field(m,screen_slot,0,screen,e)||!screen)return fail(e,e.empty()?"Monster live current Screen absent":e);
        if(!field(m,screen,0x30,scene_count,e))return false;
        if(scene_count<=0||scene_count>1048576)return fail(e,"Monster live current Screen scene count outside bounds: "+std::to_string(scene_count));
        if(!field(m,screen,0x10,scenes,e)||!scenes)return fail(e,e.empty()?"Monster live current Screen scene array absent":e);
        if(!field(m,scenes,0,expected_scene,e)||!expected_scene)return fail(e,e.empty()?"Monster live current Scene zero absent":e);
        const auto scene_code=live_scene(screen,scene);if(scene_code)return fail(e,"Monster live Scene lookup SEH="+std::to_string(scene_code));
        if(scene!=expected_scene)return fail(e,"Monster live original Scene lookup differs from current Screen scene zero");
        if(!field(m,scene,0x40,camera,e)||!camera)return fail(e,e.empty()?"Monster live current Scene camera absent":e);
        const auto code=live_present(object,local.actor.character,local.actor.actor,dt);if(code)return fail(e,"Monster live presentation SEH="+std::to_string(code));
        mr::Actor actual;if(!move_actor(m,object,actual,e)||!mr::same_registered_actor(local.actor,actual))return fail(e,e.empty()?"Monster live presentation changed current actor/spawn/OnMap":e);
        presentation_={{"local_pointer",object},{"local_uid",local.entry.uid},{"character_handle",actual.character},{"actor_pointer",actual.actor},
            {"skeleton_pointer",actual.skeleton},{"on_map",actual.on_map},{"in_view",actual.in_view},{"distance_visible",actual.distance_visible},
            {"skeleton_visible",actual.visible},{"native_interval",dt},{"applied_revision",revision_}};
        if(actual.visible!=(actual.on_map&&actual.distance_visible))return fail(e,"Monster live local presentation visibility readback differs");++updates_;return true;
    }
    void adjustment(){++damage_;}
};
MonsterLivePort monster_live_port;
void live_update_entry(uintptr_t object,float dt){
    bool natural=false;std::string natural_error;if(!interventions::actors::live::monster_update(object,dt,natural,natural_error))return;if(natural)return;
    if(!monster_live_port.active()||management_runtime::intervention_replaying()){reinterpret_cast<LiveUpdate>(live_update_hook.original)(object,dt);return;}
    bool owned=false;LiveLocal local;std::string error;
    try{const auto caller=uintptr_t(_ReturnAddress());Memory m;
        if(!monster_live_port.classify(object,owned,local,error)){monster_live_port.failed(error);return;}
        if(!owned){reinterpret_cast<LiveUpdate>(live_update_hook.original)(object,dt);return;}
        if((caller!=state.image+0x29457d&&caller!=state.image+0x2945ba&&caller!=state.image+0x2946b8)||!live_hook_current(m,live_update_hook,error)){
            monster_live_port.failed(error.empty()?"Monster live owned EntityUpdate physical caller differs":error);return;}
        if(!monster_live_port.present(object,local,dt,error))monster_live_port.failed(error);
    }catch(const std::exception& x){monster_live_port.failed(std::string("Monster live update exception: ")+x.what());}
     catch(...){monster_live_port.failed("Monster live update nonstandard exception");}
}
float live_adjust_entry(uintptr_t object,int32_t attribute,float delta,uintptr_t source,bool fifth){
    bool shared_actor=false;std::string actor_error;if(!interventions::actors::live::adjustment(object,shared_actor,actor_error))return 0.0f;if(shared_actor)return 0.0f;
    if(!monster_live_port.active()||management_runtime::intervention_replaying())return reinterpret_cast<LiveAdjust>(live_adjust_hook.original)(object,attribute,delta,source,fifth);
    bool owned=false;LiveLocal local;std::string error;
    try{Memory m;if(!monster_live_port.classify(object,owned,local,error)){monster_live_port.failed(error);return 0.0f;}
        if(!owned)return reinterpret_cast<LiveAdjust>(live_adjust_hook.original)(object,attribute,delta,source,fifth);
        if(!live_hook_current(m,live_adjust_hook,error)){monster_live_port.failed(error);return 0.0f;}
        monster_live_port.adjustment();return 0.0f;
    }catch(const std::exception& x){monster_live_port.failed(std::string("Monster live incoming attribute exception: ")+x.what());return 0.0f;}
     catch(...){monster_live_port.failed("Monster live incoming attribute nonstandard exception");return 0.0f;}
}
}
live_host_state::MonsterPort& live_port(){return monster_live_port;}
