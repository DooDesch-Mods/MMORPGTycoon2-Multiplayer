#include "construction_native.h"
#include "construction_calls.h"
#include "construction_links.h"
#include "construction_plan.h"
#include "construction_occurrence.h"
#include "scalar.h"
#include "region_saved_identity.h"
#include "remove_diagnostics.h"
#include "session_provider.h"
#include "live_host_state/scalar_store.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace mt2mp::game::developer_staff::construction {
namespace {
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& e){
    if(!p||p>UINTPTR_MAX-offset||p+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Construction replica field address overflow");
    return memory.copy(p+offset,&value,sizeof(T),e);
}
const job_census::Read copy=[](auto p,auto out,auto n,auto& e){return memory.copy(p,out,n,e);};
bool owners(const Capture& a,const Capture& b,std::string& e){
    return (same_origin(a.pin.origin,b.pin.origin)&&a.image==b.image&&a.thread==b.thread&&
        a.pin.ledger==b.pin.ledger&&a.pin.time_scale==b.pin.time_scale&&a.pin.pause_scale==b.pin.pause_scale&&
        a.local.lounge==b.local.lounge&&a.local.business==b.local.business&&a.local.data==b.local.data&&
        a.local.capacity==b.local.capacity&&a.local.indoor==b.local.indoor&&a.local.outdoor==b.local.outdoor&&
        a.local.developers==b.local.developers)||fail(e,"Construction replica actual Origin/ledger/Clock/catalogue owners changed");
}
bool client(const action_session::Context& context,const action_session::Binding& binding,const Capture& c,std::string& e){
    action_session::Context current;
    if(!action_session::selected_session().current(current,e))return false;
    return (current==context&&context.role==action_session::Role::Client&&!Config::instance().is_host&&
        c.image==uintptr_t(GetModuleHandleW(nullptr))&&c.thread==GetCurrentThreadId()&&c.pin.time_scale==0&&
        c.pin.origin.world_epoch==binding.native_epoch&&c.pin.origin.generation==binding.native_generation)||
        fail(e,"Construction replica requires actual Client/Mainthread/Clock0/common and native binding");
}
struct Actor {
    ActorPose pose;uintptr_t shard=0,region=0,quadtree=0,current=0,destination=0,task=0,road=0,node=0;
    uint8_t moving=0,path=0,pending=0;int32_t steps=0,index=0;
    bool operator==(const Actor&)const=default;
};
bool actor(const Capture& c,size_t slot,Actor& out,std::string& e){
    if(c.state.rows.size()!=50||c.local.developers.size()!=50||slot>=50)return fail(e,"Construction actor fixed50 absent");
    const auto p=c.local.developers[slot];uintptr_t ap=0,render=0,callback=0;
    if(!at(p,0,ap,e)||!at(p,0x30,render,e)||!at(render,0x38,callback,e))return false;
    if(ap!=c.image+0x17422e0||render!=c.image+0x17426e0||callback!=c.image+0x3b13a0)return fail(e,"Construction actor class/transform callback differs");
    out={};out.pose.id=c.state.rows[slot].id;
    if(!at(p,0x100,out.pose.orientation,e)||!at(p,0x11c,out.pose.scale,e)||!at(p,0x170,out.shard,e)||
       !at(p,0x178,out.region,e)||!at(p,0x488,out.quadtree,e)||!at(p,0x338,out.current,e)||!at(p,0x348,out.destination,e)||
       !at(p,0x308,out.task,e)||!at(p,0x2f8,out.road,e)||!at(p,0x2e8,out.node,e)||!at(p,0x269,out.moving,e)||
       !at(p,0x26b,out.path,e)||!at(p,0x310,out.pending,e)||!at(p,0x328,out.steps,e)||!at(p,0x330,out.index,e))return false;
    const auto map=reinterpret_cast<uintptr_t>(c.pin.origin.map);const auto root=map+0x1c0;
    if(out.shard&&out.shard!=root)return fail(e,"Construction actor is not in the current root shard");
    if(out.region){uintptr_t selected=0;
        if(!at(out.region,0x834,out.pose.region_id,e)||!region_saved_identity::resolve(c.image,map,root,out.pose.region_id,out.region,selected,
            [](uintptr_t p,size_t o,auto& v,std::string& cause){return at(p,o,v,cause);},e))return false;
    }
    for(const auto value:out.pose.orientation)if(!std::isfinite(value))return fail(e,"Construction actor orientation is not finite");
    float norm=0;for(const auto value:out.pose.orientation)norm+=value*value;
    if(norm<0.99f||norm>1.01f)return fail(e,"Construction actor orientation is not normalized");
    for(const auto value:out.pose.scale)if(!std::isfinite(value)||value<=0)return fail(e,"Construction actor scale is not finite positive");
    return true;
}
bool quiet(const Actor& a,std::string& e){
    return (!a.current&&!a.destination&&!a.task&&!a.road&&!a.node&&!a.moving&&!a.path&&!a.pending&&!a.steps)||
        fail(e,"Construction Client private movement is not quiescent; no worker AI or path replay permitted");
}
struct Target {remove_diagnostics::LocalTarget binding;uintptr_t region=0,shard=0;uint8_t active=0;int32_t state=0;};
bool building(const Capture& c,const LocalJob& link,Target& out,std::string& e){
    uintptr_t selection=0,ap=0;
    if(!at(c.image,0x13df630,selection,e)||!at(selection,0,ap,e))return false;
    if(ap!=c.image+0x17205d0)return fail(e,"Construction SelectionManager class differs");
    // Resolve by key first, then resolve that actual local receiver again.
    remove_diagnostics::LocalTarget by_key;
    action_session::Context context;if(!action_session::selected_session().current(context,e))return false;
    if(!remove_diagnostics::resolve_record_target(context.snapshot_uuid,link.building_key,by_key,e)||
       !remove_diagnostics::resolve_local_target(selection,by_key.object,out.binding,e))return false;
    const auto& b=out.binding;
    if(!b.normal_building||b.type!="SmallInn"||b.manager!=selection||b.object!=by_key.object||b.uid!=link.building_uid||
       b.record_key!=link.building_key||b.snapshot_id!=context.snapshot_uuid||b.world_epoch!=c.pin.origin.world_epoch||
       b.world_generation!=c.pin.origin.generation)return fail(e,"Construction existing SmallInn binding/UID/native Origin differs");
    if(!at(b.object,0x178,out.region,e)||!at(b.object,0x170,out.shard,e)||!at(b.object,0x7e1,out.active,e)||!at(b.object,0x7a8,out.state,e))return false;
    if(out.shard!=reinterpret_cast<uintptr_t>(c.pin.origin.map)+0x1c0||out.state!=2||out.active>1)
        return fail(e,"Construction target requires actual online root-shard Building and Boolean active field");
    return true;
}
bool graph(const Capture& c,const std::vector<LocalJob>& links,State& out,job_census::Census& jobs,std::vector<Actor>& actors,std::string& e){
    out={};actors.clear();if(!job_census::read(copy,c,jobs,e))return false;
    for(size_t slot=0;slot<50;++slot){Actor a;if(!actor(c,slot,a,e))return false;out.actors.push_back(a.pose);actors.push_back(a);}
    std::set<std::string> keys;std::set<uintptr_t> bound;
    for(const auto& link:links){
        if(!link.job||!link.building_uid||link.building_key.empty()||!keys.insert(link.building_key).second)
            return fail(e,"Construction original occurrence binding is empty or duplicated");
        Target target;
        if(link.target_alive&&!building(c,link,target,e))return false;
        // Absence observed before the next original placement closes an old
        // allocation before its address can be reused by that placement.
        const auto found=std::find_if(jobs.jobs.begin(),jobs.jobs.end(),[&](const auto& j){return !link.retired&&j.pointer==link.job;});
        JobState state{link.building_key,found!=jobs.jobs.end(),target.active!=0,-1};
        if(found!=jobs.jobs.end()){
            const auto& j=*found;
            if(!bound.insert(j.pointer).second)return fail(e,"Construction active original occurrence receiver is duplicated");
            if(!occurrence(link,j,target.binding.object,target.region,copy,e))return false;
            if(j.developer)state.developer_id=j.developer_id;
        }else if(link.target_alive&&!link.target_restored&&!state.active)return fail(e,"Construction original Job disappeared without authoritative Building activation");
        out.jobs.push_back(std::move(state));
    }
    for(const auto& j:jobs.jobs)if(!bound.contains(j.pointer))return fail(e,"Construction queue contains a Job outside retained original placement occurrences");
    return true;
}
bool code(const Capture& c,uintptr_t& dp,uintptr_t& jp,std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    static constexpr Body bodies[]={
        {0x967370,416,"2a6e4111f91e2c3cecb2aa35b99ed8d71f22a75f8300694f7d9ba6b086c01daa"},
        {0x9676a0,160,"33000088d5ae9e353738b283b76f13a641243aa996c7952714c57d6a07cc170f"},
        {0x1113d80,272,"dee54a7232d3aef27b73436213be00e802ba3cb997d4fb09a5f41f38468fafcc"},
        {0x967740,432,"205a65bdce865e61ecf78e04774d5c8d8a88ffa0d46454369f01410d6834579f"},
        {0x96d2c0,64,"f2d31e127b8ea951dc2775ada32c5fb90af88ab0455384aa4f18cfb4eabae95a"},
        {0x8a1c70,128,"f803a22293cf408d76ed09a49c80764c6872b2ff069af07ea247a4c66365ce75"},
        {0xd258b0,32,"92284a2bddcc708ff1be3237cb4371f99dcd33fb3697874d819255b81af22ab0"},
        {0xd0fd10,672,"04893cac0714f4ce3a4f95f882fd1851e214348a695754ed67021050809cacfc"},
        {0xd20ff0,32,"80e8d807ddd43641e558be3496b984967a19793b2e0af40ee184ef55a787941d"},
        {0xe031d0,64,"fe055bdaa930a65e92bd59bd371d9de2b1c1ca1f145c88fc06ebfa53dc7660ae"},
        {0xe03390,64,"de6c1492286a284636bc131c31fc626238709cb0aaf3fd1306ce9bf886b8c357"},
        {0xe02b50,64,"a4a23a998cb00f1fa7784beca0190b7debb0d467ca9444cba7e3347d4e0279bf"},
        {0xe02d10,64,"4214acb6b5c9101141f73f3e8ac4cb35cc42098f27b6ca584f595b20f69718e8"},
        {0x3b1c00,176,"a607be569b8737a55d1f690fa9fc42a5af43475557f68da5f40dfb96870b3885"},
        {0x3b1b50,176,"70d822dcde278c15da85a3e0b1ca44c0ab0aa2f94325434abfc8db767d4e3c09"},
        {0x3b0cf0,240,"e96246ce8a52b623b720f8dc29f858689e14de3706462ee8edd10038b3f2ca80"},
        {0x3b13a0,240,"2d0918f618bcf6cd3ecf6d39b821e0a695f463c6921c09392210ab6b736c00f2"},
        {0x9ff370,64,"177129c348811abd9b0d6e9cef580ef6c4aa044ce3afc4fafa5bc1935213387f"},
        {0xc67c80,240,"69bb662a4baf97906c2c5669ed569370a9b16b3c62c060664eee79407c5c325c"},
        {0x25eb30,48,"d1d12e1ba77adf6276e1aab10e5492acad6449affe6c7c47a59164becffcbe65"},
        {0x25eb00,48,"82126880703d8c8a8c813c8d72adc79985100c8482bfea94cb48c6aba370e0e8"},
        {0x261080,336,"201c5d03a4c2036b3085a4e991a071859ff99f6285583ab42cb759716a91c35a"},
        {0x261320,224,"6f94557821f9b76ba3aa6cc6c0403ae56f95f10ac1ea1109f5b543bf47e09b62"},
        {0x40ab90,1056,"76fea89d8d3e254fbf9259679efc57db74c9242c89117dec1d93601959e218bb"},
        {0x40a920,256,"90d38b047a2f449da7387a461942c6869c13c9dda9673c6a5d9d8ea511e3a341"},
        {0xa91670,8,"a2cd879c472cf43a9bb3204f150fa4150c59a714d744434fb224bfb9ececb038"},
        {0xc2e810,176,"9aa893e96fc125d33f182e51dd1ff11ec5164ed30cbd1cd62c2765da01cb20e2"},
        {0x89e940,224,"c3dc4f40d9604953e71e4faf6d70671f6813bc45ba74ec7d8996b357712dc6f0"},
        {0x89e480,256,"d95b690b624d4f2e5b1050ebe0fd0ddc7ea6f8a52f31ac6f60d00372a162cae5"}};
    for(const auto& b:bodies){std::vector<unsigned char> bytes(b.size);
        if(!memory.copy(c.image+b.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=b.sha)return fail(e,"Construction native full body differs RVA="+std::to_string(b.rva));}
    if(!at(c.image,0x1a32148,dp,e)||!at(c.image,0x1a30a20,jp,e))return false;
    struct Property{uintptr_t object,ap,offset;};
    const Property properties[]={{dp,0x1729040,0x4c8},{jp,0x1728f60,0xb8},
        {c.image+0x13c8f40,0x16d2410,0x4d9},{c.image+0x13c8fc0,0x16c9a40,0x4c4},{c.image+0x13cbf80,0x16cf9a0,0x7e1}};
    for(const auto& p:properties){uintptr_t ap=0,offset=0;
        if(!at(p.object,0,ap,e)||!at(p.object,0x38,offset,e))return false;
        if(ap!=c.image+p.ap||offset!=p.offset)return fail(e,"Construction original property class/offset differs");}
    return true;
}
struct Work {
    const action_session::Context& context;const action_session::Binding& binding;const Capture& before;
    const PoolState& target;const std::vector<LocalJob>& links;
    uintptr_t dev_property=0,job_property=0;bool attempted=false;
    bool fresh(std::string& e){Capture now;return read_process(now,e)&&client(context,binding,now,e)&&owners(before,now,e);}
};
bool fresh_pair(void* user,std::string& e){return static_cast<Work*>(user)->fresh(e);}
bool call(Work& w,const Call& r,const char* name,std::string& e){
    w.attempted=w.attempted||r.attempted;
    return (!r.exception&&!r.cleanup_exception&&r.returned)||fail(e,std::string("Construction original ")+name+" unconfirmed SEH="+std::to_string(r.exception)+" cleanup="+std::to_string(r.cleanup_exception));
}
bool pair(Work& w,uintptr_t dev,const LocalJob& job,bool attach,std::string& e){
    subscriber_lifecycle_append::DeveloperPairReceipt receipt;
    const subscriber_lifecycle_append::DeveloperPairRequest request{w.before.image,dev,job.job,w.dev_property,w.job_property,
        w.before.thread,attach,job.building_key,fresh_pair,&w};
    const bool ok=subscriber_lifecycle_append::bind_developer_pair(request,receipt,e);
    w.attempted=w.attempted||receipt.attempted||receipt.unknown;
    return ok;
}
bool field(Work& w,size_t slot,scalar::Field kind,int32_t value,std::string& e){
    scalar::Receipt receipt;receipt.exception=scalar::invoke(w.before.image,w.before.local.developers[slot],kind,value,&receipt);
    w.attempted=w.attempted||receipt.attempted;
    return (!receipt.exception&&!receipt.cleanup_exception&&receipt.record_confirmed&&receipt.returned&&receipt.accepted&&receipt.cleanup_confirmed)||
        fail(e,"Construction original Developer scalar Load/record cleanup unconfirmed");
}
std::vector<int32_t> member_ids(const Capture& c,const std::vector<uintptr_t>& list){
    std::vector<int32_t> result;
    for(const auto p:list){const auto found=std::find(c.local.developers.begin(),c.local.developers.end(),p);
        if(found==c.local.developers.end())throw std::runtime_error("Construction manager receiver absent from fixed50");
        result.push_back(c.state.rows[size_t(found-c.local.developers.begin())].id);}
    return result;
}
size_t actor_slot(const Capture& c,int32_t id){
    const auto found=std::find_if(c.state.rows.begin(),c.state.rows.end(),[&](const auto& r){return r.id==id;});
    if(found==c.state.rows.end())throw std::runtime_error("Construction actor ID absent from fixed50");
    return size_t(found-c.state.rows.begin());
}
bool retire(Work& w,const LocalJob& link,std::string& e){
    Capture current;job_census::Census queue;
    if(!read_process(current,e)||!owners(w.before,current,e)||!job_census::read(copy,current,queue,e))return false;
    const auto selected=std::find_if(queue.jobs.begin(),queue.jobs.end(),[&](const auto& j){return j.pointer==link.job;});
    if(selected==queue.jobs.end())return fail(e,"Construction selected Job disappeared before original retirement");
    Target target;
    if(link.target_alive&&!building(current,link,target,e))return false;
    if(!occurrence(link,*selected,target.binding.object,target.region,copy,e))return false;
    if((link.target_alive&&!link.target_restored&&!target.active)||selected->developer)
        return fail(e,"Construction selected retirement target/active/no-backlink shape differs");
    for(const auto& dev:queue.developers)if(dev.job==link.job)return fail(e,"Construction selected Job still has a Developer backlink");
    const size_t index=size_t(selected-queue.jobs.begin());
    std::array<unsigned char,0xc8> original{},check{};
    if(!memory.copy(link.job,original.data(),original.size(),e)||!w.fresh(e))return false;
    auto expected=queue.jobs;expected.erase(expected.begin()+static_cast<ptrdiff_t>(index));
    auto expected_developers=queue.developers;
    for(auto& developer:expected_developers){developer.job_index=-1;
        for(size_t n=0;n<expected.size();++n)if(expected[n].pointer==developer.job)developer.job_index=int32_t(n);}
    if(index+1<queue.jobs.size()){
        Call r;shift(current.image,queue.data+index*8,queue.data+(index+1)*8,(queue.jobs.size()-index-1)*8,&r);
        if(!call(w,r,"selected queue memmove",e))return false;
    }
    w.attempted=true;
    if(!live_host_state::compare_fixed_i32(queue.queue+0x28,int32_t(queue.jobs.size()),int32_t(expected.size()),e))return false;
    job_census::Census detached;
    if(!read_process(current,e)||!owners(w.before,current,e)||!job_census::read(copy,current,detached,e))return false;
    if(detached.queue!=queue.queue||detached.data!=queue.data||detached.capacity!=queue.capacity||detached.jobs!=expected||detached.developers!=expected_developers)
        return fail(e,"Construction selected queue erase changed another receiver or owner");
    if(!memory.copy(link.job,check.data(),check.size(),e))return false;
    if(check!=original)return fail(e,"Construction selected detached Job changed before original destructor");
    Call r;unary(current.image,Unary::DestroyJob,link.job,&r);
    if(!call(w,r,"selected Job destructor",e))return false;
    // The deleted receiver is never read again. Only surviving queue entries
    // and Developer links can confirm the original selected-object retirement.
    job_census::Census final;
    if(!read_process(current,e)||!owners(w.before,current,e)||!job_census::read(copy,current,final,e))return false;
    return final==detached||fail(e,"Construction original Job destructor changed surviving queue graph");
}
bool target_plan(const Capture& c,const PoolState& wire,const std::vector<LocalJob>& links,PoolState& target,State& actual,
                 job_census::Census& jobs,std::vector<Actor>& actors,std::string& e){
    if(!wire.construction||!valid(wire,e))return fail(e,e.empty()?"Construction frame state absent":e);
    if(!graph(c,links,actual,jobs,actors,e))return false;
    target=wire;
    if(!expand_pending(actual.jobs,wire.construction->jobs,links,target.construction->jobs,e))return false;
    const auto& wanted=*target.construction;
    if(wanted.jobs.size()!=actual.jobs.size()||wanted.actors.size()!=50||target.count!=c.state.business_count||c.state.lounge_count!=target.count)
        return fail(e,"Construction frame original occurrence/count/fixed50 shape differs");
    for(size_t i=0;i<50;++i){const auto& row=c.state.rows[i];const auto& t=target.rows[i];
        if(row.id!=t.id||row.employed!=t.employed||wanted.actors[i].id!=t.id||!quiet(actors[i],e))return fail(e,e.empty()?"Construction frame actor ID/employment differs":e);
        if((t.on_map||t.position!=row.position||wanted.actors[i]!=actual.actors[i])&&
           (!actors[i].shard||!actors[i].region||wanted.actors[i].region_id<0))return fail(e,"Construction moving actor has no existing root-shard/Region owner");
        if(t.on_map&&t.state!=0&&t.state!=1&&t.state!=2&&t.state!=4)return fail(e,"Construction frame Developer state requires another Job family");
        if(t.on_map&&!t.active)return fail(e,"Construction frame inactive Developer remains on map");
        if(!t.on_map&&t.job_link)return fail(e,"Construction frame Job-linked actor is off map");
        if(wanted.actors[i].region_id!=actual.actors[i].region_id)return fail(e,"Construction frame Developer Region owner differs before pose adoption");
        uintptr_t carried=0;if(!at(c.local.developers[i],0x4d0,carried,e))return false;
        if(carried)return fail(e,"Construction frame actor carries a Monster from another Job family");
        if(wanted.actors[i].region_id>=0){uintptr_t selected=0;const auto map=reinterpret_cast<uintptr_t>(c.pin.origin.map);
            if(!region_saved_identity::resolve(c.image,map,map+0x1c0,wanted.actors[i].region_id,0,selected,
                [](uintptr_t p,size_t o,auto& v,std::string& cause){return at(p,o,v,cause);},e))return false;}
    }
    for(size_t i=0;i<wanted.jobs.size();++i){const auto& a=actual.jobs[i];const auto& t=wanted.jobs[i];
        if(t.building_key!=a.building_key||(a.active&&!t.active)||(!a.present&&t.present))return fail(e,"Construction frame changes original occurrence identity or reverses completion");
        const bool inactive_restored=links[i].target_alive&&links[i].target_restored&&!t.active;
        if((!links[i].target_alive&&t.active)||(!t.present&&((t.active!=links[i].target_alive&&!inactive_restored)||t.developer_id!=-1)))
            return fail(e,"Construction completed target activation/tombstone or Developer backlink differs");
        if(t.developer_id>=0){const auto found=std::find_if(target.rows.begin(),target.rows.end(),[&](const auto& r){return r.id==t.developer_id;});
            if(found==target.rows.end()||!found->job_link||!found->on_map||(found->state!=1&&found->state!=2))return fail(e,"Construction target Job/Developer reciprocity differs");}
    }
    for(const auto& r:target.rows)if(r.job_link&&std::count_if(wanted.jobs.begin(),wanted.jobs.end(),[&](const auto& j){return j.present&&j.developer_id==r.id;})!=1)
        return fail(e,"Construction target Developer has no unique original Job occurrence");
    std::vector<Membership> operations;
    return membership_plan(member_ids(c,c.local.inside),member_ids(c,c.local.outside),target.indoor,target.outdoor,operations,e);
}
}
bool capture_placement(PlacementBefore& out,std::string& e){
    out={};if(!read_process(out.capture,e)||!job_census::read(copy,out.capture,out.queue,e))return false;
    Capture after;if(!read_process(after,e)||!owners(out.capture,after,e))return false;
    return (out.capture.state==after.state&&out.capture.local==after.local)||fail(e,"Construction placement before graph changed");
}
bool finish_placement(const PlacementBefore& before,const std::string& snapshot,const std::string& key,uint64_t uid,LocalJob& out,std::string& e){
    out={};Capture after;job_census::Census queue;
    if(!read_process(after,e)||!job_census::read(copy,after,queue,e))return false;
    if(!same_origin(before.capture.pin.origin,after.pin.origin)||before.capture.state!=after.state||before.capture.local!=after.local||
       before.capture.pin.time_scale!=after.pin.time_scale||before.queue.queue!=queue.queue)
        return fail(e,"Construction original placement changed Developer/world/queue owner");
    // Placement is expected to spend cash. Its existing Controller checks the
    // full ledger; this guard preserves the pre-existing Job and actor graph.
    if(queue.jobs.size()<before.queue.jobs.size()||queue.jobs.size()>before.queue.jobs.size()+1||queue.developers!=before.queue.developers)
        return fail(e,"Construction original placement Job delta is not zero or one");
    for(size_t i=0;i<before.queue.jobs.size();++i)if(before.queue.jobs[i]!=queue.jobs[i])return fail(e,"Construction original placement changed a previous Job");
    if(queue.jobs.size()==before.queue.jobs.size()){e.clear();return true;}
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_record_target(snapshot,key,target,e))return false;
    const auto& job=queue.jobs.back();uintptr_t region=0;
    if(!at(target.object,0x178,region,e))return false;
    if(!target.normal_building||target.uid!=uid||target.record_key!=key||target.snapshot_id!=snapshot||
       target.world_epoch!=after.pin.origin.world_epoch||target.world_generation!=after.pin.origin.generation||
       job.type!=1||!job.valid||job.building!=target.object||!job.building_proxy||job.region!=region||job.developer||
       job.bug.pointer||job.monster||job.monster_proxy)return fail(e,"Construction original new Job does not bind exact created Building");
    out={key,job.pointer,uid,true,false,job.building,job.building_proxy,job.region};
    if(!occurrence(out,job,target.object,region,copy,e))return false;
    return true;
}
bool capture(const Capture& c,const std::vector<LocalJob>& links,State& out,std::string& e){
    job_census::Census jobs;std::vector<Actor> actors;
    if(!graph(c,links,out,jobs,actors,e))return false;
    Capture after;if(!read_process(after,e)||!owners(c,after,e))return false;
    if(c.state!=after.state||c.local!=after.local)return fail(e,"Construction frame capture changed actual pool");
    job_census::Census check;if(!job_census::read(copy,after,check,e))return false;
    if(check!=jobs)return fail(e,"Construction frame capture changed original Job graph");
    for(size_t slot=0;slot<50;++slot){Actor second;if(!actor(after,slot,second,e))return false;
        if(second!=actors[slot])return fail(e,"Construction frame capture changed actor movement/Region");}
    std::vector<JobState> pending;
    for(size_t i=0;i<out.jobs.size();++i)if(out.jobs[i].present||
        (links[i].target_alive&&links[i].target_restored&&!out.jobs[i].active))pending.push_back(std::move(out.jobs[i]));
    out.jobs=std::move(pending);
    return true;
}
bool preflight(const action_session::Context& context,const action_session::Binding& binding,const Capture& c,
               const PoolState& target,const std::vector<LocalJob>& links,std::string& e){
    State actual;PoolState expanded;job_census::Census jobs;std::vector<Actor> actors;uintptr_t dp=0,jp=0;
    return client(context,binding,c,e)&&target_plan(c,target,links,expanded,actual,jobs,actors,e)&&code(c,dp,jp,e);
}
bool apply(const action_session::Context& context,const action_session::Binding& binding,const Capture& before,
           const PoolState& target,const std::vector<LocalJob>& links,bool& attempted,bool& unknown,std::string& e){
    attempted=false;unknown=false;
    Work w{context,binding,before,target,links};
    const auto body=[&](){
        Capture current;if(!read_process(current,e)||!client(context,binding,current,e)||!owners(before,current,e))return false;
        if(current.state!=before.state||current.local!=before.local)return fail(e,"Construction actual pool changed after frame preflight");
        State actual;PoolState expanded;job_census::Census jobs;std::vector<Actor> actors;
        if(!target_plan(current,target,links,expanded,actual,jobs,actors,e)||!code(current,w.dev_property,w.job_property,e))return false;
        const auto& wanted=*expanded.construction;
        std::vector<Membership> operations;
        if(!membership_plan(member_ids(current,current.local.inside),member_ids(current,current.local.outside),target.indoor,target.outdoor,operations,e))return false;
        // Detach only a selected original occurrence whose authoritative owner
        // changed. Each borrowed property Resolve must confirm both links.
        for(size_t j=0;j<links.size();++j)if(actual.jobs[j].developer_id>=0&&
            (actual.jobs[j].developer_id!=wanted.jobs[j].developer_id||!wanted.jobs[j].present)){
            if(!w.fresh(e)||!pair(w,before.local.developers[actor_slot(before,actual.jobs[j].developer_id)],links[j],false,e))return false;
        }
        for(size_t j=0;j<links.size();++j)if(!actual.jobs[j].active&&wanted.jobs[j].active){
            Target t;if(!w.fresh(e)||!building(current,links[j],t,e))return false;
            Call r;active_property(before.image,t.binding.object,&r);
            if(!call(w,r,"Building active Load",e))return false;
            if(!r.record_confirmed||!r.accepted||!r.cleanup_confirmed)return fail(e,"Construction Building active record/acceptance/cleanup unconfirmed");
            uint8_t active=0;if(!at(t.binding.object,0x7e1,active,e))return false;
            if(active!=1)return fail(e,"Construction original Building active property readback differs");
            Call colors;unary(before.image,Unary::UpdateBuildingColors,t.binding.object,&colors);
            if(!call(w,colors,"Building color refresh",e)||!w.fresh(e))return false;
        }
        for(size_t i=0;i<50;++i){const auto& old=before.state.rows[i];const auto& next=target.rows[i];
            if(old.state!=next.state){if(!w.fresh(e)||!field(w,i,scalar::Field::State,next.state,e))return false;}
            if(old.position!=next.position||actual.actors[i]!=wanted.actors[i]){
                if(!w.fresh(e))return false;
                Call r;pose(before.image,before.local.developers[i],&wanted.actors[i],&next.position,&r);
                if(!call(w,r,"Developer pose",e))return false;
                if(old.on_map){Call quad;unary(before.image,Unary::UpdateQuadtree,before.local.developers[i],&quad);
                    if(!call(w,quad,"Developer quadtree refresh",e))return false;}
            }
        }
        for(const auto& operation:operations){
            if(!w.fresh(e))return false;
            const size_t slot=actor_slot(before,operation.id);const auto developer=before.local.developers[slot];
            if(operation.reorder){Call r;reorder(before.image,operation.outside?before.local.outdoor:before.local.indoor,developer,&r);
                if(!call(w,r,"Developer manager order",e))return false;}
            else{
                Call map;unary(before.image,operation.outside?Unary::OnMap:Unary::OffMap,developer,&map);
                if(!call(w,map,"Developer map membership",e))return false;
                Call lounge;membership(before.image,before.local.lounge,developer,operation.outside,&lounge);
                if(!call(w,lounge,"Developer Lounge membership",e))return false;
            }
        }
        for(size_t j=0;j<links.size();++j)if(wanted.jobs[j].present&&wanted.jobs[j].developer_id>=0&&
            wanted.jobs[j].developer_id!=actual.jobs[j].developer_id){
            if(!w.fresh(e)||!pair(w,before.local.developers[actor_slot(before,wanted.jobs[j].developer_id)],links[j],true,e))return false;
        }
        for(size_t j=0;j<links.size();++j)if(actual.jobs[j].present&&!wanted.jobs[j].present){
            if(!w.fresh(e)||!retire(w,links[j],e))return false;
        }
        for(size_t i=0;i<50;++i)if(before.state.rows[i].active!=target.rows[i].active){
            if(!w.fresh(e)||!field(w,i,scalar::Field::Active,target.rows[i].active?1:0,e))return false;}
        Capture final;if(!read_process(final,e)||!client(context,binding,final,e)||!owners(before,final,e))return false;
        State projected;job_census::Census final_jobs;std::vector<Actor> final_actors;
        if(!graph(final,links,projected,final_jobs,final_actors,e))return false;
        for(size_t i=0;i<50;++i){const auto& r=final.state.rows[i];const auto& t=target.rows[i];
            if(r.id!=t.id||r.state!=t.state||r.employed!=t.employed||r.active!=t.active||r.on_map!=t.on_map||r.job_link!=t.job_link||r.position!=t.position)
                return fail(e,"Construction final full50 scalar/map/pose/Job readback differs id="+std::to_string(r.id));
            if(!quiet(final_actors[i],e))return false;
            if(r.on_map!=bool(final_actors[i].quadtree))return fail(e,"Construction final Developer quadtree/map presence differs");
        }
        if(projected!=wanted||member_ids(final,final.local.inside)!=target.indoor||member_ids(final,final.local.outside)!=target.outdoor)
            return fail(e,"Construction final actor Region/pose/Job/Building/ordered manager frame differs");
        return w.fresh(e);
    };
    bool ok=false;
    try{ok=body();}
    catch(const std::exception& cause){e+=(e.empty()?"":"; ")+std::string("Construction C++ exception: ")+cause.what();}
    catch(...){e+=(e.empty()?"":"; ")+std::string("Construction unknown C++ exception");}
    attempted=w.attempted;unknown=!ok&&attempted;
    if(!ok&&e.empty())e="Construction native adoption unconfirmed";
    return ok;
}
}
