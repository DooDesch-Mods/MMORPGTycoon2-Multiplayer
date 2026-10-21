namespace dungeon_gizmo {
namespace {
namespace gl=dungeon_gizmo_live;
uint32_t live_clip(uintptr_t actor,int32_t index,gl::Clip& out)noexcept{
    __try{out={};if(index<0)return 0;
        out.length=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x23860)(actor,index));
        if(reinterpret_cast<bool(*)(uintptr_t,int32_t)>(state.image+0x23820)(actor,index)){
            out.time=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x23890)(actor,index));
            out.paused=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(state.image+0x23840)(actor,index);
            out.speed=std::bit_cast<uint32_t>(reinterpret_cast<float(*)(uintptr_t,int32_t)>(state.image+0x238b0)(actor,index));}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_clip(uintptr_t actor,int32_t index,const gl::Clip& value)noexcept{
    __try{if(index<0)return 0xe0010003;
        if(value.time){reinterpret_cast<void(*)(uintptr_t,int32_t)>(state.image+0x235f0)(actor,index);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(state.image+0x237a0)(actor,index,std::bit_cast<float>(value.speed));
            reinterpret_cast<void(*)(uintptr_t,int32_t,bool)>(state.image+0x23770)(actor,index,value.paused);
            reinterpret_cast<void(*)(uintptr_t,int32_t,float)>(state.image+0x237e0)(actor,index,std::bit_cast<float>(*value.time));
        }else reinterpret_cast<void(*)(uintptr_t,int32_t)>(state.image+0x23a30)(actor,index);
        reinterpret_cast<void(*)(uintptr_t)>(state.image+0x21df0)(actor);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_cas(uintptr_t address,unsigned size,uint32_t before,uint32_t after,bool& exchanged)noexcept{
    exchanged=false;__try{
        if(size==1)exchanged=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(after),char(before)))==uint8_t(before);
        else if(size==4&&!(address&3))exchanged=uint32_t(_InterlockedCompareExchange(reinterpret_cast<volatile long*>(address),long(after),long(before)))==before;
        else return 0xe0010003;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t live_on_map(uintptr_t object,bool value)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(state.image+0x147f90)(object,value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct LiveSample {dungeon_scene_identity_runtime::Census census;gl::State value;std::map<std::string,gl::Local> locals;};
// Shared Context identity and local NativeOpen file identity have separate owners.
template<class Left,class Right>bool live_world_same(const Left& x,const Right& y){
    return x.game==y.game&&x.mode==y.mode&&x.state==y.state&&x.world==y.world&&x.map==y.map&&
        x.world_epoch==y.world_epoch&&x.generation==y.generation&&x.uuid==y.uuid&&x.sha256==y.sha256;
}
bool live_scope(const a::Context& context,const a::Binding& binding,std::string& error){
    a::Context actual;a::Binding current;
    return GetCurrentThreadId()==state.thread&&a::selected_session().current(actual,error)&&actual==context&&
        typed_action_runtime::read_binding(context.snapshot_uuid,current,error)&&current==binding||
        fail(error,error.empty()?"Gizmo live saved Context/Binding/Mainthread differs":error);
}
bool live_once(Memory& memory,const a::Context& context,const a::Binding& binding,LiveSample& out,std::string& error){
    out={};building_mixed_identity::opened_file::CurrentWorld origin;
    if(!live_scope(context,binding,error)||!native_open_runtime::read_current_world(origin,error)||
       !dungeon_scene_identity_runtime::read_current(out.census,error))return false;
    if(origin.world_epoch!=binding.native_epoch||origin.generation!=binding.native_generation||!live_world_same(origin,out.census.world))
        return fail(error,"Gizmo live exact current snapshot World lifetime differs");
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& why){return field(memory,object,offset,value,why);};
    auto clip=[&](uintptr_t actor,int32_t index,gl::Clip& value,std::string& why){live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);
        const auto seh=live_clip(actor,index,value);return !seh||fail(why,"Gizmo live native clip read SEH="+std::to_string(seh));};
    size_t slot=0;
    for(const auto& pool:out.census.pools){gl::Scene scene;scene.key=pool.scene.owner.key;
        if(!at(pool.scene.owner.shard,0x44,scene.unlocked,error))return false;
        for(const auto& member:pool.local.members){
            if(slot>=out.census.keys.size())return fail(error,"Gizmo live complete canonical roster is truncated");
            dungeon_gizmo_identity::Member owner{scene.key,member.object,pool.managers[3],pool.scene.owner.shard,pool.scene.owner.region,member.uid};
            gl::Row row;gl::Local local;const auto& key=out.census.keys[slot++];
            if(!gl::read_row(at,clip,state.image,owner,key,row,local,error)||!out.locals.emplace(key,local).second)return fail(error,error.empty()?"Gizmo live canonical member repeated":error);
            scene.rows.push_back(std::move(row));}
        out.value.push_back(std::move(scene));}
    if(slot!=out.census.keys.size())return fail(error,"Gizmo live complete canonical roster has extra keys");
    building_mixed_identity::opened_file::CurrentWorld after;
    if(!native_open_runtime::read_current_world(after,error))return false;
    if(!live_world_same(origin,after))return fail(error,"Gizmo live physical World changed during read");
    return gl::valid(out.value,error)&&live_scope(context,binding,error);
}
bool live_sample(Memory& memory,const a::Context& context,const a::Binding& binding,LiveSample& out,std::string& error){
    LiveSample first;if(!code(memory,error)||!live_once(memory,context,binding,first,error)||!live_once(memory,context,binding,out,error))return false;
    if(!live_world_same(first.census.world,out.census.world)||first.census.keys!=out.census.keys||first.locals!=out.locals||first.value!=out.value)return fail(error,"Gizmo live complete native values or owners changed during read");return true;
}
}
bool capture_live(const a::Context& context,const a::Binding& binding,dungeon_gizmo_live::State& out,std::string& error){
    out.clear();Memory memory;LiveSample value;if(!live_sample(memory,context,binding,value,error))return false;out=std::move(value.value);return true;
}
bool apply_live(const a::Context& context,const a::Binding& binding,const dungeon_gizmo_live::State& target,dungeon_gizmo_live::Receipt& receipt,std::string& error){
    receipt={};Memory memory;LiveSample before;
    if(context.role!=a::Role::Client||!gl::valid(target,error)||!live_sample(memory,context,binding,before,error)||!gl::same_members(before.value,target,error))return false;
    auto expected=before.value;
    const auto failed=[&]{receipt.cause=error.empty()?"Gizmo live adoption failed without cause":error;error=receipt.cause;receipt.unknown=receipt.attempted;return false;};
    const auto current=[&](LiveSample& now){return live_sample(memory,context,binding,now,error)&&now.locals==before.locals&&now.value==expected||fail(error,error.empty()?"Gizmo live current complete owner/value differs before write":error);};
    const auto store=[&](uintptr_t address,unsigned bytes,uint32_t old,uint32_t wanted){if(old==wanted)return true;LiveSample now;if(!current(now))return false;
        bool exchanged=false;receipt.attempted=true;++receipt.attempted_stores;const auto seh=live_cas(address,bytes,old,wanted,exchanged);
        if(seh||!exchanged)return fail(error,"Gizmo live exact scalar CAS failed SEH="+std::to_string(seh)+"; no replay");++receipt.stores;return true;};
    for(size_t s=0;s<target.size();++s){
        const auto& scene=before.census.pools[s].scene.owner;
        if(!store(scene.shard+0x44,4,expected[s].unlocked,target[s].unlocked))return failed();expected[s].unlocked=target[s].unlocked;
        for(size_t n=0;n<target[s].rows.size();++n){const auto& wanted=target[s].rows[n];auto& row=expected[s].rows[n];const auto& local=before.locals.at(row.key);const auto object=local.member.object;
            if(row.on_map!=wanted.on_map){LiveSample now;if(!current(now))return failed();receipt.attempted=true;++receipt.attempted_stores;
                const auto seh=live_on_map(object,wanted.on_map);if(seh){fail(error,"Gizmo live original SetOnMap SEH="+std::to_string(seh)+"; no replay");return failed();}++receipt.stores;row.on_map=wanted.on_map;
                LiveSample adopted;if(!live_sample(memory,context,binding,adopted,error))return failed();
                before.locals.at(row.key).model=adopted.locals.at(row.key).model;
                if(adopted.locals!=before.locals||adopted.value!=expected){fail(error,"Gizmo live original SetOnMap changed another current owner/value");return failed();}}
            const auto byte=[&](size_t offset,bool& value,bool desired){if(!store(object+offset,1,uint32_t(value),uint32_t(desired)))return false;value=desired;return true;};
            const auto word=[&](size_t offset,int32_t& value,int32_t desired){if(!store(object+offset,4,std::bit_cast<uint32_t>(value),std::bit_cast<uint32_t>(desired)))return false;value=desired;return true;};
            if(!byte(0x250,row.triggered,wanted.triggered)||!word(0x254,row.state,wanted.state)||!word(0x328,row.immediate,wanted.immediate))return failed();
            if((local.features&1)&&local.trigger_actor&&!byte(0x2f8,row.interaction,wanted.interaction))return failed();
            if((!(local.features&1)||!local.trigger_actor)&&wanted.interaction){fail(error,"Gizmo live container interaction lacks its initialized native render owner");return failed();}
            if(wanted.transition){
                if(!(local.features&2)||(wanted.animation>=0&&local.indexes[size_t(wanted.animation)]<0)){fail(error,"Gizmo live target transition has no current local Door asset");return failed();}
                // Cached transition values have native ownership only while the
                // flag is active. Read the real dormant bytes as CAS operands.
                int32_t index=-1,from=0,to=0;
                if(!field(memory,object,0x2c4,index,error)||!field(memory,object,0x2c8,from,error)||!field(memory,object,0x2cc,to,error))return failed();
                const int32_t desired_index=wanted.animation<0?-1:local.indexes[size_t(wanted.animation)];
                if(!store(object+0x2c4,4,std::bit_cast<uint32_t>(index),std::bit_cast<uint32_t>(desired_index)))return failed();
                if(row.transition)row.animation=wanted.animation;
                if(!store(object+0x2c8,4,std::bit_cast<uint32_t>(from),std::bit_cast<uint32_t>(wanted.from)))return failed();if(row.transition)row.from=wanted.from;
                if(!store(object+0x2cc,4,std::bit_cast<uint32_t>(to),std::bit_cast<uint32_t>(wanted.to)))return failed();if(row.transition)row.to=wanted.to;
            }
            if(!byte(0x2d0,row.transition,wanted.transition))return failed();row.animation=wanted.animation;row.from=wanted.from;row.to=wanted.to;
            for(size_t c=0;c<4;++c){if(row.clips[c]==wanted.clips[c])continue;LiveSample now;if(!current(now))return failed();
                const auto actor=c<3?local.door_actor:local.trigger_actor;const auto index=local.indexes[c];
                if(!actor||index<0){fail(error,"Gizmo live changed clip lacks current native Actor asset");return failed();}
                for(size_t other=0;other<4;++other)if((other<3?local.door_actor:local.trigger_actor)==actor&&local.indexes[other]==index&&wanted.clips[other]!=wanted.clips[c]){fail(error,"Gizmo live semantic aliases disagree on one native clip");return failed();}
                receipt.attempted=true;++receipt.attempted_stores;const auto seh=live_clip(actor,index,wanted.clips[c]);
                if(seh){fail(error,"Gizmo live original clip adoption SEH="+std::to_string(seh)+"; no replay");return failed();}++receipt.stores;
                for(size_t other=0;other<4;++other)if((other<3?local.door_actor:local.trigger_actor)==actor&&local.indexes[other]==index)row.clips[other]=wanted.clips[c];
            }
        }
    }
    LiveSample after;if(!current(after)||after.value!=target)return failed();receipt.confirmed=true;return true;
}
}
