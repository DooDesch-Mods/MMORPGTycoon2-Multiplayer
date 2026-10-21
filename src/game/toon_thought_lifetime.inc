namespace thought_native {
namespace values=toon_thought;
Void original_destroy=nullptr;release_ui_owner::Owner destroy_owner;
bool enabled=false;std::string fault;uint64_t corrected=0,failed=0,stale_removed=0;
bool pending=false;std::string pending_cause;uint64_t deferred=0;
nlohmann::json last;
void retain_failure(const std::string& e){++failed;if(fault.empty()){fault=e.empty()?"Thought lifetime correction failed without a cause":e;LOG_ERROR("[Thought] coop ownership correction failed cause={}; original destructor is preserved",fault);}}
void defer(const std::string& e){++deferred;pending=true;const auto cause=e.empty()?"Thought current Zone owner is not available during original destruction":e;if(cause!=pending_cause){pending_cause=cause;LOG_WARN("[Thought] coop Zone cleanup waits for complete live ownership cause={}; original destructor is preserved",cause);}}
bool world(Runtime& r,const a::Context& c,const a::Binding& b,building_mixed_identity::opened_file::CurrentWorld& out,std::string& e){
    a::Context current;if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(current,e)||current!=c||!native_open_runtime::read_current_world(out,e)||out.world_epoch!=b.native_epoch||out.generation!=b.native_generation)return fail(e,e.empty()?"Thought current Session/Mainthread/NativeOpen differs":e);
    return true;
}
bool zone_current(Runtime& r,const a::Context& c,const a::Binding& b,const world_zone_runtime::Current& expected,std::string& e){
    building_mixed_identity::opened_file::CurrentWorld current;if(!world(r,c,b,current,e))return false;
    world_zone_runtime::Current actual;{live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!world_zone_runtime::resolve_current(b,c.snapshot_uuid,expected.object,expected.uid,actual,e))return false;}
    return actual==expected||fail(e,"Thought current full Zone/UID/Region/scene owner differs");
}
uint32_t remove_call(uintptr_t image,uintptr_t zone,uintptr_t thought)noexcept{__try{reinterpret_cast<void(*)(void*,void*)>(image+0x4543c0)(reinterpret_cast<void*>(zone),reinterpret_cast<void*>(thought));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool remove(Runtime& r,const a::Context& c,const a::Binding& b,const world_zone_runtime::Current& zone,uintptr_t thought,bool& attempted,std::string& e){
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    auto current=[&](std::string& cause){return zone_current(r,c,b,zone,cause);};
    auto original=[&](uintptr_t object,uintptr_t identity,std::string& cause){live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);const auto code=remove_call(r.image,object,identity);return !code||fail(cause,"Thought original ZoneNotifyDestroyed exception="+std::to_string(code)+"; no replay");};
    return values::remove_zone_link(read,current,original,r.image,zone.object,thought,attempted,e);
}
bool joined(Runtime& r,a::Context& c,a::Binding& b,building_mixed_identity::opened_file::CurrentWorld& out,std::string& e){
    if(!r.gm_context||!r.gm_binding||r.gm_suspended)return false;c=*r.gm_context;b=*r.gm_binding;return world(r,c,b,out,e);
}
void destroy(void* receiver){
    auto* r=published.load(std::memory_order_acquire);
    const auto object=reinterpret_cast<uintptr_t>(receiver);a::Context c;a::Binding b;
    std::optional<world_zone_runtime::Current> owned_zone;
    if(r&&enabled&&r->gm_context&&r->gm_binding&&!r->gm_suspended){try{
        building_mixed_identity::opened_file::CurrentWorld origin;std::string e;uintptr_t zone=0,proxy=0,current=0;
        if(!joined(*r,c,b,origin,e)||!ap(*r,object,0x16fdef0,e)||!at(*r,object,0x88,zone,e)||!at(*r,object,0x90,proxy,e)||(proxy&&!at(*r,proxy,0,current,e)))defer(e);
        else if(current&&zone){
            if(current!=zone)defer("Thought Zone weak target/proxy differs before original destruction");
            else{world_zone_runtime::Current zone_pin;uint64_t uid=0;bool read=false;
                if(at(*r,zone,0x18,uid,e)){live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);read=world_zone_runtime::resolve_current(b,c.snapshot_uuid,zone,uid,zone_pin,e);}
                if(read)owned_zone=zone_pin;else defer(e);
            }
        }
    }catch(const std::exception& error){defer(std::string("Thought destruction ownership guard: ")+error.what());}}
    // Let every original observer and bookkeeping call finish first. The saved
    // identity is used only for comparisons after D1 and is never dereferenced.
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);original_destroy(receiver);}
    bool correction_attempted=false;
    if(r&&owned_zone){try{
        std::string e;values::ZoneList list;auto at_=[&](uintptr_t owner,size_t offset,auto& value,std::string& cause){return at(*r,owner,offset,value,cause);};
        if(!zone_current(*r,c,b,*owned_zone,e)||!values::zone_list(at_,r->image,owned_zone->object,list,e))defer(e);
        else if(std::find(list.members.begin(),list.members.end(),object)!=list.members.end()){
            if(!remove(*r,c,b,*owned_zone,object,correction_attempted,e)){if(correction_attempted)retain_failure(e);else defer(e);}else ++corrected;
        }
    }catch(const std::exception& error){const auto cause=std::string("Thought post-D1 opaque Zone cleanup: ")+error.what();if(correction_attempted)retain_failure(cause);else defer(cause);}}
}
bool enable(Runtime& r,std::string& e){
    struct Body{uintptr_t rva;size_t bytes;const char* sha;};constexpr Body bodies[]={
        {0x3f0fc0,432,"b809f7293fa4d0c143cddd610b18480853bc841ab4ea3441fc6aacd40b0dc625"},
        {0x3f1170,32,"c7612d8fa43401163a68209387f44d700ad121da575cd770b3528704b0d61c5d"},
        {0x4543c0,352,"9efdc58b8c7c038faf16154d087acc6c8ccc1b04e1494a7953320bd99f24b0bb"}};
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.bytes);if(!r.memory.copy(r.image+body.rva,bytes.data(),bytes.size(),e)||mt2mp::sha256(bytes)!=body.sha)return fail(e,e.empty()?"Thought original lifetime/remover full body differs":e);if(body.rva==0x3f0fc0)destroy_owner={r.image+body.rva,reinterpret_cast<uintptr_t>(&destroy),0,1,5,std::move(bytes)};}
    const auto made=MH_CreateHook(reinterpret_cast<void*>(destroy_owner.entry),reinterpret_cast<void*>(&destroy),reinterpret_cast<void**>(&original_destroy));if(made!=MH_OK||!original_destroy)return fail(e,"Thought D1 hook create failed status="+std::to_string(made));destroy_owner.trampoline=reinterpret_cast<uintptr_t>(original_destroy);
    const auto result=MH_EnableHook(reinterpret_cast<void*>(destroy_owner.entry));if(result!=MH_OK)return fail(e,"Thought D1 hook enable failed status="+std::to_string(result));
    auto copy=[&](auto p,auto out,auto count,auto& cause){return r.memory.copy(p,out,count,cause);};auto hash=[](const auto& bytes){return mt2mp::sha256(bytes);};
    if(!release_ui_owner::verify(destroy_owner,copy,hash,bodies[0].sha,e))return false;enabled=true;e.clear();return true;
}
bool reconcile(Runtime& r,const a::Context& c,const a::Binding& b,uint64_t& removals,std::string& e){
    removals=0;if(!fault.empty())return fail(e,fault);live::NativePort port(c,b,nullptr);live::Pin pin;if(!port.read(pin,e)||pin.image!=r.image)return fail(e,e.empty()?"Thought cleanup current idle World admission differs":e);
    building_mixed_identity::opened_file::CurrentWorld current;if(!world(r,c,b,current,e))return false;
    uintptr_t manager=0,data=0;int32_t count=0,capacity=0;
    if(!at(r,r.image,0x13df690,manager,e)||!ap(r,manager,0x1725f40,e)||!at(r,manager,0x40,data,e)||!at(r,manager,0x48,count,e)||!at(r,manager,0x4c,capacity,e)||count<0||count>1000000||capacity<count||capacity>1000000||(!data&&capacity))return fail(e,e.empty()?"Thought stale cleanup current Subscriber manager extent differs":e);
    std::set<uintptr_t> live_objects,subscribers,toons;std::vector<std::pair<uintptr_t,values::Array>> owners;std::map<uintptr_t,std::vector<uintptr_t>> slots;std::vector<uintptr_t> subscriber_slots;
    for(int32_t i=0;i<count;++i){uintptr_t subscriber=0,toon=0,reverse=0;if(!at(r,data,size_t(i)*8,subscriber,e)||!ap(r,subscriber,0x16e75f0,e)||!subscribers.insert(subscriber).second||!at(r,subscriber,0x80,toon,e))return fail(e,e.empty()?"Thought cleanup complete Subscriber owner duplicated":e);
        subscriber_slots.push_back(subscriber);if(!toon)continue;if(!ap(r,toon,0x1744e90,e)||!at(r,toon,0x940,reverse,e)||reverse!=subscriber||!toons.insert(toon).second)return fail(e,e.empty()?"Thought cleanup Toon reciprocal current owner differs":e);
        values::Array array;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};if(!values::array(read,toon,0x8c8,array,e))return false;owners.emplace_back(toon,array);
        for(int32_t n=0;n<array.count;++n){uintptr_t object=0;if(!at(r,array.data,size_t(n)*8,object,e)||!object||!live_objects.insert(object).second)return fail(e,e.empty()?"Thought cleanup live ownership slots are null or duplicated":e);slots[toon].push_back(object);}
    }
    boss_current::State bosses;character_definition_identity::Census definitions;character_membership_reader::Users users;
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!boss_current::read_with_characters(c,b,bosses,definitions,users,e))return false;}
    for(const auto& user:users.rows)if(user.family==character_membership_reader::Family::Toon&&!toons.contains(user.object)){
        const auto toon=user.object;if(!ap(r,toon,0x1744e90,e))return false;values::Array array;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};if(!values::array(read,toon,0x8c8,array,e))return false;owners.emplace_back(toon,array);toons.insert(toon);
        for(int32_t n=0;n<array.count;++n){uintptr_t object=0;if(!at(r,array.data,size_t(n)*8,object,e)||!object||!live_objects.insert(object).second)return fail(e,e.empty()?"Thought cleanup complete Character user ownership is null or duplicated":e);slots[toon].push_back(object);}
    }
    std::vector<world_zone_runtime::Current> zones;{live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!world_zone_runtime::read_current(b,c.snapshot_uuid,zones,e))return false;}
    const auto complete=[&](){uintptr_t checked=0,checked_data=0;int32_t checked_count=0,checked_capacity=0;if(!world(r,c,b,current,e)||!port.current_owners(pin,e)||!at(r,r.image,0x13df690,checked,e)||checked!=manager||!at(r,manager,0x40,checked_data,e)||checked_data!=data||!at(r,manager,0x48,checked_count,e)||checked_count!=count||!at(r,manager,0x4c,checked_capacity,e)||checked_capacity!=capacity)return fail(e,e.empty()?"Thought cleanup current complete manager changed":e);
        for(size_t i=0;i<subscriber_slots.size();++i){uintptr_t member=0;if(!at(r,data,i*8,member,e)||member!=subscriber_slots[i])return fail(e,e.empty()?"Thought cleanup current Subscriber slot identity changed":e);}
        for(const auto& [toon,array]:owners){values::Array now;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};if(!values::array(read,toon,0x8c8,now,e)||now!=array)return fail(e,e.empty()?"Thought cleanup live Toon owning array changed":e);
            for(int32_t n=0;n<now.count;++n){uintptr_t thought=0;if(!at(r,now.data,size_t(n)*8,thought,e)||thought!=slots.at(toon)[size_t(n)])return fail(e,e.empty()?"Thought cleanup complete live owned slot identity changed":e);}}
        boss_current::State fresh_bosses;character_definition_identity::Census fresh_definitions;character_membership_reader::Users fresh_users;
        {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!boss_current::read_with_characters(c,b,fresh_bosses,fresh_definitions,fresh_users,e))return false;}
        if(fresh_users!=users)return fail(e,"Thought cleanup complete current Character users changed");
        return true;};
    for(const auto& zone:zones){values::ZoneList list;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};if(!values::zone_list(read,r.image,zone.object,list,e))return false;
        for(auto identity:list.members)if(!live_objects.contains(identity)){if(!complete())return false;bool attempted=false;if(!remove(r,c,b,zone,identity,attempted,e)){if(attempted)retain_failure(e);return false;}++removals;++stale_removed;}}
    if(!complete())return false;if(pending)LOG_INFO("[Thought] coop Zone cleanup recovered complete live ownership; removed={}",removals);pending=false;pending_cause.clear();last={{"stale_removed",removals},{"confirmed",true}};e.clear();return true;
}
}
