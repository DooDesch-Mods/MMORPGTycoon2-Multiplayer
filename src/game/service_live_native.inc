namespace {
bool service_uid(Runtime& r,uintptr_t object,uint64_t uid,uintptr_t expected,std::string& e){
    uint64_t actual=0;uintptr_t found=0;
    if(!ap(r,object,expected,e)||!at(r,object,0x18,actual,e)||actual!=uid)return fail(e,e.empty()?"Service Frame full UID/class changed":e);
    const auto seh=uid_lookup(r.image,uid,found);return (!seh&&found==object)||fail(e,"Service Frame FindUID/current pointer differs SEH="+std::to_string(seh));
}
bool service_sample(Runtime& r,const std::map<uintptr_t,std::string>& destinations,ServiceOwner& owner,service_live::Building& out,std::string& e){
    auto read=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return at(r,p,offset,value,cause);};
    auto string=[&](uintptr_t p,std::string& value,std::string& cause){return text(r,p,value,4096,cause);};
    auto destination=[&](uintptr_t p,std::string& key,std::string& cause){const auto found=destinations.find(p);if(found==destinations.end())return fail(cause,"Service Frame destination lacks complete canonical Building identity");key=found->second;return true;};
    a::ServiceMembershipState state;
    if(!service_uid(r,owner.object,owner.uid,0x16d93c0,e)||!service_membership_reader::sample(read,string,destination,r.image,owner.object,state,owner.local,e))return false;
    for(const auto& row:owner.local.rows)if(!service_uid(r,row.object,row.uid,0x1718480,e))return false;
    for(size_t i=0;i<owner.local.characters.size();++i)if(!service_uid(r,owner.local.characters[i],owner.local.character_uids[i],0x1705990,e))return false;
    owner.sampled=state;out=service_live::project(owner.key,state,owner.local,[](const auto& value){return mt2mp::sha256(std::vector<unsigned char>(value.begin(),value.end()));});return true;
}
bool services_read(Runtime& r,const a::Context& c,const a::Binding& b,Local& local,service_live::State& out,std::string& e){
    std::vector<remove_diagnostics::LocalTarget> census;if(!remove_diagnostics::identity_targets(census,e))return false;
    local.services.clear();local.service_destinations.clear();out.clear();
    for(const auto& target:census){
        if(target.world_epoch!=b.native_epoch||target.world_generation!=b.native_generation||!local.service_destinations.emplace(target.object,target.record_key).second)return fail(e,"Service Frame duplicate or changed canonical Building owner");
        if(target.normal_building)local.services.push_back({target.record_key,target.object,target.uid,{},{}});
    }
    std::sort(local.services.begin(),local.services.end(),[](const auto& x,const auto& y){return x.key<y.key;});
    for(auto& owner:local.services){service_live::Building value;if(!service_sample(r,local.service_destinations,owner,value,e))return false;out.push_back(std::move(value));}
    return service_live::valid(out,e);
}
nlohmann::json services_difference(const Local& local,const service_live::State& before,const service_live::State& target){
    auto difference=service_live::first_difference(before,target);
    if(!difference.is_null()&&difference.contains("ordinal")){
        const auto index=difference.at("ordinal").get<size_t>();
        if(index<local.services.size()&&index<before.size()&&local.services[index].key==before[index].key)
            difference["actual_sample"]=service_live::sample_diagnostic(local.services[index].sampled,[](const auto& value){return mt2mp::sha256(std::vector<unsigned char>(value.begin(),value.end()));});
    }return difference;
}
uint32_t service_reserve_once(uintptr_t image,uintptr_t array,int32_t capacity)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0xe8080)(array,capacity);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t service_pause_once(uintptr_t image,uintptr_t clock,float value)noexcept{
    __try{reinterpret_cast<void(*)(void*,float)>(image+0x259b70)(reinterpret_cast<void*>(clock),value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct ServiceWork {std::function<bool(std::string&)> body,guard;};
bool service_work_body(void* p,std::string& e){return static_cast<ServiceWork*>(p)->body(e);}
bool service_work_guard(void* p,std::string& e){return static_cast<ServiceWork*>(p)->guard(e);}
bool services_apply(Runtime& r,const a::Context& c,const a::Binding& b,Local& local,const service_live::State& before,const service_live::State& target,live::ManagementReceipt& receipt,std::string& e){
    if(!service_live::compatible(before,target,e))return false;if(before==target)return true;
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;
    if(world.world_epoch!=b.native_epoch||world.generation!=b.native_generation||!world.mode)return fail(e,"Service Frame Apply actual NativeOrigin differs");
    float old_pause=0;if(!at(r,local.pin.clock,0x5c,old_pause,e)||(old_pause!=0.0f&&old_pause!=1.0f))return fail(e,e.empty()?"Service Frame actual pause invalid":e);
    bool entered=false;
    const auto guard=[&](std::string& cause){
        a::Context actual;building_mixed_identity::opened_file::CurrentWorld current;native::WorldStatus status;float selected=-1;uintptr_t graph=1;
        if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,cause)||actual!=c||!native_open_runtime::read_current_world(current,cause)||
           current.world!=world.world||current.mode!=world.mode||current.world_epoch!=b.native_epoch||current.generation!=b.native_generation||!native::read_world_status(status,cause)||
           status.saves_in_progress!=(entered?1:0)||status.edit_operations||!at(r,r.image,0x1a3aa80,graph,cause)||graph||!at(r,local.pin.clock,0x58,selected,cause)||selected!=0.0f)return fail(cause,cause.empty()?"Service Frame saved Context/Binding/Clock/SaveScope changed":cause);
        for(const auto& owner:local.services){uintptr_t data=0;int32_t count=0,capacity=0;
            if(!service_uid(r,owner.object,owner.uid,0x16d93c0,cause)||!at(r,owner.object,0x9d8,data,cause)||!at(r,owner.object,0x9e0,count,cause)||!at(r,owner.object,0x9e4,capacity,cause)||
               data!=owner.local.data||count!=int32_t(owner.local.rows.size())||capacity!=owner.local.capacity)return fail(cause,cause.empty()?"Service Frame current owning Building membership changed":cause);
            for(size_t i=0;i<owner.local.rows.size();++i){const auto& row=owner.local.rows[i];uintptr_t current=0,building=0;
                if(!at(r,data,i*sizeof(uintptr_t),current,cause)||current!=row.object||!service_uid(r,row.object,row.uid,0x1718480,cause)||!at(r,row.object,0x148,building,cause)||building!=owner.object)return fail(cause,cause.empty()?"Service Frame exact current Service owner/full UID changed":cause);
            }
        }return true;
    };
    auto compare=[&](uintptr_t address,const void* expected,const void* desired,size_t size,std::string& cause){
        if(!guard(cause)||!size||address>UINTPTR_MAX-size||(size!=1&&size%4))return fail(cause,cause.empty()?"Service Frame CAS span invalid":cause);
        for(size_t offset=0;offset<size;offset+=size==1?1:4){int32_t x=0,y=0;std::memcpy(&x,static_cast<const unsigned char*>(expected)+offset,size==1?1:4);std::memcpy(&y,static_cast<const unsigned char*>(desired)+offset,size==1?1:4);bool stored=false;
            receipt.attempted=true;++receipt.attempted_stores;const auto seh=scalar_cas(address+offset,size==1,x,y,stored);if(seh||!stored)return fail(cause,"Service Frame CAS concurrent value differs SEH="+std::to_string(seh)+"; no replay");++receipt.stores;}return true;
    };
    const auto reserve=[&](uintptr_t array,int32_t capacity,std::string& cause){
        constexpr std::array<unsigned char,16> expected{0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0xb8,0xfe,0xff,0xff,0xff,0xff,0xff,0x1f};std::array<unsigned char,16> code{};
        if(!guard(cause)||!r.memory.copy(r.image+0xe8080,code.data(),code.size(),cause)||code!=expected)return fail(cause,cause.empty()?"Service Frame original Reserve body differs":cause);
        receipt.attempted=true;++receipt.attempted_stores;const auto seh=service_reserve_once(r.image,array,capacity);return !seh||fail(cause,"Service Frame original Reserve exception="+std::to_string(seh)+"; no replay");
    };
    const auto body=[&](std::string& cause){
        entered=true;if(!guard(cause))return false;
        for(size_t i=0;i<before.size();++i){ServiceOwner sampled=local.services[i];service_live::Building actual;
            if(!service_sample(r,local.service_destinations,sampled,actual,cause)||actual!=before[i]||sampled.local!=local.services[i].local)return fail(cause,cause.empty()?"Service Frame post-SaveScope entry before/owner differs":cause);
            for(size_t j=0;j<before[i].rows.size();++j)for(size_t k=0;k<2;++k){service_live::Receipt count;
                if(!service_live::apply_count(r.memory,r.image,local.services[i].local.rows[j].object,0x50+k*0x70,before[i].rows[j].counts[k],target[i].rows[j].counts[k],compare,reserve,guard,count,cause))return false;
            }
            if(!service_sample(r,local.service_destinations,sampled,actual,cause)||actual!=target[i])return fail(cause,cause.empty()?"Service Frame complete owned after differs":cause);
        }return true;
    };
    if(!guard(e))return false;
    ServiceWork work{body,guard};subscriber_lifecycle::SaveScopeReceipt scope;
    const subscriber_lifecycle::SaveScopeApi api{reinterpret_cast<subscriber_lifecycle::SaveScopeCall>(r.image+0x4a03a0),reinterpret_cast<subscriber_lifecycle::SaveScopeCall>(r.image+0x49f6f0)};
    receipt.attempted=true;++receipt.attempted_stores;
    if(!subscriber_lifecycle::with_save_scope(api,world.mode,service_work_body,service_work_guard,&work,scope,e))return false;
    entered=false;if(!guard(e))return false;
    float pause=0;if(!at(r,local.pin.clock,0x5c,pause,e))return false;
    if(pause!=old_pause){constexpr std::array<unsigned char,10> expected{0x56,0x53,0x48,0x83,0xec,0x48,0x0f,0x2f,0x49,0x5c};std::array<unsigned char,10> code{};
        if(pause!=0.0f||old_pause!=1.0f||!r.memory.copy(r.image+0x259b70,code.data(),code.size(),e)||code!=expected)return fail(e,e.empty()?"Service Frame pause restore original/transition differs":e);
        ++receipt.attempted_stores;const auto seh=service_pause_once(r.image,local.pin.clock,old_pause);if(seh)return fail(e,"Service Frame original pause restore exception="+std::to_string(seh)+"; no replay");
        if(!at(r,local.pin.clock,0x5c,pause,e)||pause!=old_pause)return fail(e,e.empty()?"Service Frame original pause restore readback differs":e);
    }return true;
}
}
