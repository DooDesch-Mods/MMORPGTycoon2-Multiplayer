namespace {
using Motion=subscriber_motion::Pose;
struct MotionOwner {uintptr_t map=0,root=0,toon=0,region=0,manager=0;};
struct MotionManagerHeader {
    uintptr_t manager=0,data=0;int32_t count=0,capacity=0;
    bool operator==(const MotionManagerHeader&)const=default;
};
struct MotionSamplePass {
    struct Manager {MotionManagerHeader header;std::vector<uintptr_t> members;std::map<uintptr_t,size_t> slots;};
    bool bound=false;
    building_mixed_identity::opened_file::CurrentWorld world;
    SampleRegionIndex regions;
    std::map<uintptr_t,Manager> managers;
};
bool sample_motion_world(const Pin& p,MotionSamplePass& pass,MotionOwner& out,std::string& e){
    if(!pass.bound){
        building_mixed_identity::opened_file::CurrentWorld w;
        if(!native_open_runtime::read_current_world(w,e))return false;
        if(reinterpret_cast<uintptr_t>(w.world)!=p.world||w.world_epoch!=p.binding.native_epoch||w.generation!=p.binding.native_generation)return fail(e,"Subscriber motion actual world binding differs");
        pass.world=std::move(w);pass.bound=true;
    }
    out.map=reinterpret_cast<uintptr_t>(pass.world.map);out.root=out.map+0x1c0;return true;
}
bool motion_world(const Pin& p,MotionOwner& out,std::string& e){
    building_mixed_identity::opened_file::CurrentWorld w;
    if(!native_open_runtime::read_current_world(w,e))return false;
    if(reinterpret_cast<uintptr_t>(w.world)!=p.world||w.world_epoch!=p.binding.native_epoch||w.generation!=p.binding.native_generation)return fail(e,"Subscriber motion actual world binding differs");
    out.map=reinterpret_cast<uintptr_t>(w.map);out.root=out.map+0x1c0;return true;
}
bool motion_region(const Pin& p,const MotionOwner& owner,int32_t id,uintptr_t expected,uintptr_t& region,std::string& e,SampleRegionIndex* index=nullptr){
    apply_timing::PageReadScope pages(apply_timing::page_read_scope==nullptr);
    const auto read=[](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);};
    if(index){if(!index->resolve(p.image,owner.map,owner.root,id,expected,region,read,e))return false;}
    else if(!region_saved_identity::resolve(p.image,owner.map,owner.root,id,expected,region,[](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},e))return false;
    uint8_t owned=0;if(!at(region,0x850,owned,e))return false;
    return owned==1||fail(e,"Subscriber motion target Region is not natively owned");
}
bool motion_manager(const Pin& p,uintptr_t region,uintptr_t& manager,std::vector<uintptr_t>& toons,std::string& e,MotionManagerHeader* header=nullptr){
    apply_timing::PageReadScope pages(apply_timing::page_read_scope==nullptr);
    uintptr_t data=0;int32_t count=-1,capacity=-1;uint8_t updating=1;
    if(!at(region,0x9c0,manager,e)||!ap(manager,p.image,0x16fde30,e)||!at(manager,0xc8,updating,e)||!at(manager,0x28,data,e)||!at(manager,0x30,count,e)||!at(manager,0x34,capacity,e))return false;
    if(updating||count<0||capacity<count||count>1000000||(count&&(!data||data>UINTPTR_MAX-size_t(count)*sizeof(uintptr_t))))return fail(e,"Subscriber motion ToonManager is updating or array bounds differ");
    toons.clear();toons.resize(size_t(count));
    if(count&&!read_native_block(memory,data,toons.data(),toons.size()*sizeof(uintptr_t),e)){toons.clear();return false;}
    std::set<uintptr_t> unique;
    for(auto toon:toons)if(!toon||!unique.insert(toon).second)return fail(e,"Subscriber motion ToonManager null/duplicate receiver");
    if(header)*header={manager,data,count,capacity};
    return true;
}
bool sample_motion_member(const Pin& p,MotionSamplePass& pass,MotionOwner& owner,std::string& e){
    auto found=pass.managers.find(owner.region);
    if(found==pass.managers.end()){
        MotionSamplePass::Manager entry;
        uintptr_t manager=0;
        if(!motion_manager(p,owner.region,manager,entry.members,e,&entry.header))return false;
        for(size_t i=0;i<entry.members.size();++i)entry.slots.emplace(entry.members[i],i);
        found=pass.managers.emplace(owner.region,std::move(entry)).first;
    }
    const auto& entry=found->second;const auto slot=entry.slots.find(owner.toon);
    if(slot==entry.slots.end())return fail(e,"Subscriber motion Toon absent from registered Region manager");
    MotionManagerHeader actual;uint8_t updating=1;uintptr_t receiver=0;
    if(!at(owner.region,0x9c0,actual.manager,e)||!ap(actual.manager,p.image,0x16fde30,e)||!at(actual.manager,0xc8,updating,e)||
       !at(actual.manager,0x28,actual.data,e)||!at(actual.manager,0x30,actual.count,e)||!at(actual.manager,0x34,actual.capacity,e))return false;
    if(updating||actual!=entry.header)return fail(e,"Subscriber motion sample manager/header changed during pass");
    if(!at(actual.data,slot->second*sizeof(uintptr_t),receiver,e))return false;
    if(receiver!=owner.toon)return fail(e,"Subscriber motion sample actual Toon membership slot changed");
    owner.manager=actual.manager;return true;
}
bool motion_read(const Pin& p,uintptr_t subscriber,std::optional<Motion>& pose,std::string& e,MotionOwner* selected=nullptr,SampleRegionIndex* index=nullptr,MotionSamplePass* pass=nullptr){
    apply_timing::PageReadScope pages(pass==nullptr);
    pose.reset();uintptr_t toon=0;if(!at(subscriber,0x80,toon,e))return false;
    if(!toon)return true;
    uintptr_t backlink=0;uint8_t logged=0;if(!ap(toon,p.image,0x1744e90,e)||!at(toon,0x940,backlink,e)||backlink!=subscriber||!at(toon,0x825,logged,e)||logged>1)return fail(e,e.empty()?"Subscriber motion reciprocal native Toon/login differs":e);
    if(!logged)return true;
    Motion value;MotionOwner owner;owner.toon=toon;uintptr_t shard=0,last=0,render=0,callback=0;
    if(!(pass?sample_motion_world(p,*pass,owner,e):motion_world(p,owner,e))||!at(toon,0x170,shard,e)||!at(toon,0x178,owner.region,e)||!at(toon,0x9a8,last,e)||!at(owner.region,0x834,value.region,e))return false;
    if(shard!=owner.root||last!=owner.region)return fail(e,"Subscriber motion requires reciprocal registered root-shard Toon");
    uintptr_t region=0;if(!motion_region(p,owner,value.region,owner.region,region,e,pass?&pass->regions:index)||!at(toon,0x30,render,e)||!at(render,0x38,callback,e))return false;
    if(callback!=p.image+0x3e82a0)return fail(e,"Subscriber motion native Toon transform callback differs");
    if(!memory.copy(toon+0x100,value.orientation.data(),sizeof value.orientation,e)||!memory.copy(toon+0x110,value.position.data(),sizeof value.position,e)||!memory.copy(toon+0x11c,value.scale.data(),sizeof value.scale,e))return false;
    if(!subscriber_motion::valid(value,e)){
        const auto original=e;int32_t subscriber_id=-1;uint64_t toon_uid=0;std::string id_error,uid_error;
        const bool id_read=at(subscriber,0x20,subscriber_id,id_error);const bool uid_read=at(toon,0x18,toon_uid,uid_error);
        double norm_squared=0;std::array<uint32_t,4> bits{};for(size_t i=0;i<value.orientation.size();++i){const auto component=value.orientation[i];norm_squared+=double(component)*component;bits[i]=std::bit_cast<uint32_t>(component);}
        nlohmann::json detail={{"subscriber_id_read",id_read},{"toon_uid_read",uid_read},{"orientation",value.orientation},{"orientation_bits",bits},{"norm_squared",norm_squared},{"region",value.region}};
        if(id_read)detail["subscriber_id"]=subscriber_id;else detail["subscriber_id_read_error"]=id_error;
        if(uid_read)detail["toon_uid"]=toon_uid;else detail["toon_uid_read_error"]=uid_error;
        return fail(e,original+" native_motion_sample="+detail.dump());
    }
    if(pass){if(!sample_motion_member(p,*pass,owner,e))return false;}
    else {std::vector<uintptr_t> registered;if(!motion_manager(p,owner.region,owner.manager,registered,e)||std::count(registered.begin(),registered.end(),toon)!=1)return fail(e,e.empty()?"Subscriber motion Toon absent from registered Region manager":e);}
    pose=value;if(selected)*selected=owner;return true;
}
bool finish_motion_index(const Pin& p,const SampleRegionIndex& index,std::string& e){
    if(!index.active())return true;
    MotionOwner owner;if(!motion_world(p,owner,e))return false;
    return index.finish(p.image,owner.map,owner.root,[](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},e);
}
bool finish_motion_pass(const Pin& p,const MotionSamplePass& pass,std::string& e){
    if(!pass.bound)return true;
    building_mixed_identity::opened_file::CurrentWorld actual;
    if(!native_open_runtime::read_current_world(actual,e))return false;
    const auto& old=pass.world;
    if(actual.game!=old.game||actual.mode!=old.mode||actual.state!=old.state||actual.world!=old.world||actual.map!=old.map||
       actual.generation!=old.generation||actual.world_epoch!=old.world_epoch||actual.bytes!=old.bytes||actual.uuid!=old.uuid||actual.sha256!=old.sha256||
       reinterpret_cast<uintptr_t>(actual.world)!=p.world||actual.world_epoch!=p.binding.native_epoch||actual.generation!=p.binding.native_generation)return fail(e,"Subscriber motion complete World receipt changed during pass");
    if(!pass.regions.finish(p.image,reinterpret_cast<uintptr_t>(actual.map),reinterpret_cast<uintptr_t>(actual.map)+0x1c0,[](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},e))return false;
    for(const auto& [region,entry]:pass.managers){
        MotionManagerHeader header;uintptr_t manager=0;std::vector<uintptr_t> members;
        if(!motion_manager(p,region,manager,members,e,&header))return false;
        if(header!=entry.header||members!=entry.members)return fail(e,"Subscriber motion complete manager/membership changed during pass");
    }
    return true;
}
bool motion_target(const Pin& p,const MotionOwner& owner,const Motion& target,uintptr_t& region,std::string& e){
    if(!subscriber_motion::valid(target,e)||!motion_region(p,owner,target.region,0,region,e))return false;
    const auto region_at=reinterpret_cast<uintptr_t(*)(uintptr_t,const float*,uintptr_t)>(p.image+0x5089b0);
    if(region_at(owner.root,target.position.data(),owner.region)!=region)return fail(e,"Subscriber motion target pose resolves to another native Region");
    uint8_t follows=0;if(!at(owner.toon,0x490,follows,e))return false;
    if(region!=owner.region&&follows!=1)return fail(e,"Subscriber motion native Toon does not follow Region at target position");
    uintptr_t manager=0;std::vector<uintptr_t> toons;
    if(!motion_manager(p,region,manager,toons,e))return false;
    if(region!=owner.region&&std::find(toons.begin(),toons.end(),owner.toon)!=toons.end())return fail(e,"Subscriber motion Toon already registered in target Region");
    return true;
}
}
#include "native_online.inc"
bool NativePort::preflight_motion(const Owners& before,const State& target,std::string& e){
    if(!admit_existing(before,target,e)||!current(before,0,true,e))return false;
    return preflight_motion_body(before,target,e);
}
bool NativePort::preflight_motion_body(const Owners& before,const State& target,std::string& e){
    const auto& p=before.history.pin;
    for(size_t i=0;i<target.subscribers.size();++i){
        if(before.state.subscribers[i].motion==target.subscribers[i].motion)continue;
        if(before.state.subscribers[i].motion.has_value()!=target.subscribers[i].motion.has_value())continue;
        std::optional<Motion> actual;MotionOwner owner;uintptr_t region=0;
        if(!motion_read(p,before.subscribers[i],actual,e,&owner)||actual!=before.state.subscribers[i].motion||!actual||!target.subscribers[i].motion)return fail(e,e.empty()?"Subscriber motion prior native pose differs":e);
        if(!motion_target(p,owner,*target.subscribers[i].motion,region,e))return false;
    }
    e.clear();return true;
}
bool NativePort::store_motion(const Owners& before,size_t index,const Motion& old,const Motion& target,std::string& e){
    if(index>=before.subscribers.size())return fail(e,"Subscriber motion saved-ID slot invalid");
    const auto subscriber=before.subscribers[index];if(!current_subscriber(before,index,e))return false;
    const auto& p=before.history.pin;std::optional<Motion> actual;MotionOwner owner;uintptr_t region=0;
    if(!motion_read(p,subscriber,actual,e,&owner)||actual!=old)return fail(e,e.empty()?"Subscriber motion original setter prior pose differs":e);
    if(!motion_target(p,owner,target,region,e))return false;
    uintptr_t old_manager=0,new_manager=0;std::vector<uintptr_t> old_toons,new_toons;
    if(!motion_manager(p,owner.region,old_manager,old_toons,e)||!motion_manager(p,region,new_manager,new_toons,e))return false;
    // The native constructor supplies dirty state and matrix cache. The native
    // setter invokes actor/spatial updates and original Region-manager transfer.
    alignas(4) std::array<std::byte,0x6c> transform{};
    const auto construct=reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(p.image+0x9ff370);
    const auto set=reinterpret_cast<void(*)(uintptr_t,const void*)>(p.image+0xc67c80);
    {apply_timing::Scope measured(apply_timing::Part::MotionOriginal);
    construct(transform.data(),target.orientation.data(),target.position.data(),target.scale.data());
    set(owner.toon+0x30,transform.data());}
    MotionOwner after;if(!motion_read(p,subscriber,actual,e,&after)||actual!=target||after.toon!=owner.toon||after.region!=region)return fail(e,e.empty()?"Subscriber motion native setter pose/registered Region readback differs; no replay":e);
    std::vector<uintptr_t> old_after,new_after;uintptr_t manager=0;
    if(!motion_manager(p,owner.region,manager,old_after,e)||manager!=old_manager||!motion_manager(p,region,manager,new_after,e)||manager!=new_manager)return fail(e,e.empty()?"Subscriber motion Region-manager owner changed; no replay":e);
    auto members=[](const std::vector<uintptr_t>& rows){return std::set<uintptr_t>(rows.begin(),rows.end());};
    auto wanted_old=members(old_toons),wanted_new=members(new_toons);
    if(owner.region!=region){wanted_old.erase(owner.toon);wanted_new.insert(owner.toon);}
    if(members(old_after)!=wanted_old||members(new_after)!=wanted_new)return fail(e,"Subscriber motion native Region transfer changed foreign Toon membership; no replay");
    e.clear();return true;
}
