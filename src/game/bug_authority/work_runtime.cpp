#include "work_runtime.h"
#include "runtime_internal.h"
#include "job_native.h"
#include "parent_visual.h"
#include "developer_staff/construction_links.h"
#include "region_saved_identity.h"
#include "npc_quest_daily_apply.h"
#include "mt2mp/core/hash.h"
#include "typed_action_runtime.h"
#include "live_host_state/scalar_store.h"
#include <Windows.h>
#include <algorithm>
#include <set>
#include <tuple>
namespace mt2mp::game::bug_authority {
namespace {
using developer_staff::Capture;
namespace census=developer_staff::job_census;
JobRead reader(){return [](uintptr_t p,void* out,size_t n,std::string& e){return runtime().memory.copy(p,out,n,e);};}
bool fresh(const Capture& before,std::string& e){Capture now;auto& r=runtime();
    if(!work_pin(r.context,r.binding,now,e))return false;
    return (developer_staff::same_origin(before.pin.origin,now.pin.origin)&&before.pin.ledger==now.pin.ledger&&
        before.pin.time_scale==now.pin.time_scale&&before.pin.pause_scale==now.pin.pause_scale&&before.image==now.image&&before.thread==now.thread&&
        before.local.lounge==now.local.lounge&&before.local.developers==now.local.developers)||fail(e,"Bug-work original Origin/ledger/Clock/catalogue changed");
}
bool snapshot_jobs(const census::Census& queue,std::string& e){
    auto& r=runtime();if(!work_bindings(e))return false;
    for(const auto& saved:r.saved_jobs){if(!saved.job||r.jobs.pointer(saved.job))continue;
        const auto found=std::find_if(queue.jobs.begin(),queue.jobs.end(),[&](const auto& job){return job.pointer==saved.job;});
        if(found==queue.jobs.end())return fail(e,"Serialized Job disappeared without original destructor witness");
        if(found->type==0&&!found->valid&&found->developer){
            JobRow row;row.type=0;row.valid=false;row.developer_id=found->developer_id;
            row.birth={BirthKind::Snapshot,r.snapshot_sha,0,uint64_t(saved.link)};
            if(!r.jobs.born({row,saved.job,0,0,false},e))return false;continue;
        }
        if(found->type!=7)continue;
        const auto* bug=r.lifetimes.pointer(found->bug.pointer);
        if(!bug||found->bug.job!=saved.job||found->bug.anchor!=bug->parent)return fail(e,"Serialized type7 Job lacks exact original Bug child/link resolution");
        JobRow row;row.bug=bug_identity(bug->row);row.birth={BirthKind::Snapshot,r.snapshot_sha,0,uint64_t(saved.link)};
        if(!r.jobs.born({row,saved.job,bug->bug,bug->parent,false},e))return false;
    }
    r.saved_jobs.clear();return true;
}
bool region_id(const Capture& capture,uintptr_t region,int32_t& id,std::string& e){
    id=-1;if(!region)return true;uintptr_t selected=0;const auto map=reinterpret_cast<uintptr_t>(capture.pin.origin.map);
    if(!field(region,0x834,id,e))return false;
    return region_saved_identity::resolve(capture.image,map,map+0x1c0,id,region,selected,
        [](uintptr_t p,size_t o,auto& value,std::string& cause){return field(p,o,value,cause);},e);
}
bool read_counter(const Capture& capture,uintptr_t queue,size_t offset,JobCounter& out,std::string& e){
    npc_quest_daily_apply::NativeState state;
    if(!npc_quest_daily_apply::read_native(runtime().memory,capture.image,queue+offset,state,e))return false;
    out={state.value,state.words[6],state.words[4]};return true;
}
bool parent_owner(const Capture& capture,const ParentVisual& target,ParentOwner& out,std::string& e){
    Row row;row.parent_kind=target.kind;row.parent_key=target.key;uintptr_t collection=0,parent=0;
    if(!work_parent(row,parent,collection,e))return false;
    out={capture.image,parent,reinterpret_cast<uintptr_t>(capture.pin.origin.map)+0x1c0,target.kind,target.key};return true;
}
bool returns(const JobCall& call,bool& attempted,const char* name,std::string& e){
    attempted|=call.attempted;
    return (call.returned&&!call.exception&&!call.cleanup_exception)||fail(e,std::string("Bug-work original ")+name+" unconfirmed SEH="+std::to_string(call.exception));
}
bool properties(const Capture& capture,std::string& e){
    struct Body{uintptr_t rva;size_t size;const char* sha;};
    static constexpr Body bodies[]={
        {0xd00b60,304,"a637aae8dd61640db66a77efb5465bf8f4e9cb7af0aa8db6ad652748d23cc995"},
        {0xd10a60,304,"4cd2a69f35951e37fc58ef452e60efe447745033393411c626bec980d6747193"},
        {0xd20ff0,32,"80e8d807ddd43641e558be3496b984967a19793b2e0af40ee184ef55a787941d"},
        {0x426830,16,"84038befb3ab6f3e3abbfb09b01942bcdbf1492076e814220f033ee87a159db1"},
        {0x39d020,96,"ae19e77552b1e5f24a021305a0c49e42f4318459b6e3fa96cf7ca87f1b3c5f91"},
        {0x40ab90,1056,"76fea89d8d3e254fbf9259679efc57db74c9242c89117dec1d93601959e218bb"},
        {0x2817a0,96,"29b374120b45c5491394a14655dc51f4a3fcfee060ea3b9f7c7781581a61c0d6"},
        {0xe8080,288,"6d7c8326e01c886f5fd3115cb51b04fe002baed6c687c3fc27ee356b126fa9f9"}};
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);if(!runtime().memory.copy(capture.image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(e,"Bug-work original full body differs RVA="+std::to_string(body.rva));}
    struct Property{uintptr_t rva,ap,offset;};
    for(const auto& p:{Property{0x13cc040,0x16c8070,0x7a8},Property{0x13ca640,0x16c9b20,0x838},Property{0x13cb940,0x16cf9a0,0xbd0}}){
        uintptr_t ap=0,offset=0;if(!field(capture.image+p.rva,0,ap,e)||!field(capture.image+p.rva,0x38,offset,e))return false;
        if(ap!=capture.image+p.ap||offset!=p.offset)return fail(e,"Bug-work parent original property class/offset differs");}
    return native_code(e);
}
class NativeParent final:public ParentServices {
    const Capture& capture_;
public:
    explicit NativeParent(const Capture& capture):capture_(capture){}
    bool authorize(const ParentOwner& owner,std::string& e)override{ParentOwner current;ParentVisual row;row.kind=owner.kind;row.key=owner.key;
        return fresh(capture_,e)&&parent_owner(capture_,row,current,e)&&
            ((current.object==owner.object&&current.shard==owner.shard)||fail(e,"Bug-work parent native lifetime changed"));}
    bool observe(const ParentOwner& owner,ParentVisual& out,std::string& e)override{return read_parent_visual(reader(),owner,out,e);}
    bool state(const ParentOwner& owner,int32_t value,JobCall& call,std::string& e)override{
        const char* building[]={"state offline","state maintenance","state online"};
        const char* npc[]={"onlineState Offline","onlineState Maintenance","onlineState Online"};
        const bool b=owner.kind==ParentKind::Building;
        work_property(owner.image,owner.image+(b?0x13cc040:0x13ca640),b?0xd00b60:0xd10a60,owner.object,b?building[value]:npc[value],&call);
        bool ignored=false;return returns(call,ignored,"parent state Load",e);
    }
    bool detail(const ParentOwner& owner,const ParentVisual& before,const ParentVisual& target,bool& attempted,std::string& e)override{
        attempted=false;if(!authorize(owner,e))return false;
        if(owner.kind==ParentKind::Building){
            if(before.bandwidth!=target.bandwidth){JobCall call;work_bandwidth(owner.image,owner.object,target.bandwidth,&call);if(!returns(call,attempted,"SetBandwidthCost",e))return false;}
            if(before.user_set!=target.user_set){JobCall call;work_property(owner.image,owner.image+0x13cb940,0xd20ff0,owner.object,target.user_set?"userSetState true":"userSetState false",&call);
                if(!returns(call,attempted,"userSetState Load",e)||!call.accepted||!call.record_confirmed||!call.cleanup_confirmed)return fail(e,e.empty()?"Bug-work userSetState property not accepted":e);}
        }else{
            uint32_t exception=0;const uint8_t old=before.tint_override?1:0,next=target.tint_override?1:0;
            if(before.tint!=target.tint){attempted=true;if(!work_store(owner.object+0x6b0,before.tint.data(),target.tint.data(),16,&exception))return fail(e,"Bug-work NPC tint compare/store failed SEH="+std::to_string(exception));}
            if(old!=next){attempted=true;if(!work_store(owner.object+0x6c0,&old,&next,1,&exception))return fail(e,"Bug-work NPC tint flag compare/store failed SEH="+std::to_string(exception));}
        }return true;
    }
    bool refresh(const ParentOwner& owner,JobCall& call,std::string& e)override{work_unary(owner.image+(owner.kind==ParentKind::Building?0x40ab90:0x39d020),owner.object,&call);bool ignored=false;return returns(call,ignored,"parent visual refresh",e);}
};
}
bool capture_work(const Capture& capture,const census::Census& queue,WorkState& out,std::vector<uintptr_t>& bound,std::string& e){
    auto& r=runtime();std::lock_guard lock(r.gate);out={};bound.clear();
    Context context;Binding binding;uint64_t publication=0;bool active=false;Capture pinned;
    if(!active_world(context,binding,publication,active,e)||!active||!work_pin(context,binding,pinned,e))return fail(e,e.empty()?"Bug-work capture requires current active authority":e);
    if(!snapshot_jobs(queue,e))return false;
    std::map<std::pair<ParentKind,std::string>,ParentVisual> parents;
    for(const auto& local:r.jobs.rows()){
        const auto selected=std::find_if(queue.jobs.begin(),queue.jobs.end(),[&](const auto& j){return !local.retired&&j.pointer==local.job;});
        if(!local.retired&&selected==queue.jobs.end())return fail(e,"Witnessed Bug Job disappeared without original destructor");
        if(selected!=queue.jobs.end()){
            const auto& j=*selected;JobRow row=local.row;
            if(j.building||j.building_proxy||j.monster||j.monster_proxy)return fail(e,"Bug Job has foreign target fields");
            if(j.destination!=std::array<float,3>{})return fail(e,"Bug Job has a destination from another native Job family");
            if(j.developer&&(j.developer_id<0||!j.reciprocal))return fail(e,"Bug Job Developer receiver is outside exact reciprocal fixed50");
            if(!j.developer&&std::any_of(queue.developers.begin(),queue.developers.end(),[&](const auto& dev){return dev.job==j.pointer;}))
                return fail(e,"Bug Job has an unmatched incoming Developer reference");
            if(j.type==7){if(j.valid!=1||j.bug.pointer!=local.bug||j.bug.job!=local.job||j.bug.anchor!=local.parent)return fail(e,"Bug Job exact reciprocal Bug/parent lifetime differs");}
            else if(j.type!=0||j.valid||j.bug.pointer||!j.developer)return fail(e,"Bug Job abort is not an assigned invalid original occurrence");
            row.type=j.type;row.valid=j.valid!=0;row.developer_id=j.developer_id;row.position=j.position;
            if(!region_id(capture,j.region,row.region_id,e))return false;
            out.jobs.push_back(std::move(row));bound.push_back(j.pointer);
        }
        ParentVisual visual;visual.kind=local.row.bug.parent_kind;visual.key=local.row.bug.parent_key;
        if(!visual.key.empty()&&!parents.contains({visual.kind,visual.key})){
            if(visual.kind==ParentKind::Building){bool known=false,alive=false;
                if(!typed_action_runtime::bug_parent_alive(r.context,r.binding,visual.key,known,alive,e))return false;
                if(known&&!alive)continue;}
            ParentOwner owner;if(!parent_owner(capture,visual,owner,e)||!read_parent_visual(reader(),owner,visual,e))return false;
            parents.emplace(std::make_pair(visual.kind,visual.key),visual);
        }
    }
    for(const auto& [key,visual]:parents){(void)key;out.parents.push_back(visual);}
    return read_counter(capture,queue.queue,0x48,out.created,e)&&read_counter(capture,queue.queue,0xb8,out.fixed,e)&&fresh(capture,e);
}
bool has_work(){auto& r=runtime();std::lock_guard lock(r.gate);return !r.jobs.rows().empty()||r.frame_work.has_value();}
bool preflight_work(const Capture& capture,const WorkState& target,std::string& e){
    auto& r=runtime();std::lock_guard lock(r.gate);
    if(!r.frame_work||*r.frame_work!=target||!r.frame_publication||r.context.role!=action_session::Role::Client||capture.pin.time_scale!=0)
        return fail(e,"Bug-work adoption lacks exact prepared Host publication/Client Clock0");
    if(!properties(capture,e)||!verify_job_calls(reader(),capture.image,e)||!fresh(capture,e))return false;
    for(const auto& row:target.jobs){
        if(row.birth.kind==BirthKind::Snapshot&&(row.birth.source!=r.snapshot_sha||!row.birth.occurrence||row.birth.occurrence>INT32_MAX))return fail(e,"Bug Job snapshot witness digest/linkId differs");
        if(row.birth.kind==BirthKind::Action&&!r.confirmed_actions.contains(row.birth.source)&&
           !(r.ordered_job_birth&&*r.ordered_job_birth==row.birth&&r.action==row.birth.source))
            return fail(e,"Bug Job Action witness is not locally confirmed or the exact admitted ordered birth");
        if(row.birth.kind==BirthKind::Publication&&(row.birth.source!=r.context.snapshot_uuid||row.birth.sequence>r.frame_publication))return fail(e,"Bug Job publication witness is outside current frame");
        if(!row.bug.parent_key.empty()){
            if(row.bug.birth.kind==BirthKind::Snapshot&&row.bug.birth.source!=r.snapshot_sha)return fail(e,"Bug Job retained Bug snapshot provenance differs");
            if(row.bug.birth.kind==BirthKind::Action&&!r.confirmed_actions.contains(row.bug.birth.source))return fail(e,"Bug Job retained Bug Action provenance is not locally confirmed");
            if(row.bug.birth.kind==BirthKind::Publication&&(row.bug.birth.source!=r.context.snapshot_uuid||row.bug.birth.sequence>r.frame_publication))return fail(e,"Bug Job retained Bug publication provenance differs");
        }
        const auto* local=r.jobs.identity(row.birth);
        if(!local&&r.jobs.consumed(row.birth))return fail(e,"Bug Job retired occurrence cannot replay");
        if(!local&&row.birth.kind==BirthKind::Snapshot)return fail(e,"Bug Job original shared snapshot occurrence missing");
        if(local&&local->row.bug!=row.bug)return fail(e,"Bug Job occurrence changed original Bug provenance");
        if(!local&&row.type==7&&std::none_of(r.frame_bugs.rows.begin(),r.frame_bugs.rows.end(),[&](const auto& bug){return same_identity(bug,bug_row(row.bug));}))
            return fail(e,"Bug Job birth has no authoritative live Bug occurrence");
    }
    for(const auto& parent:target.parents){ParentOwner owner;if(!parent_owner(capture,parent,owner,e))return false;}
    return true;
}
bool verify_job_destructor(std::string& e){return native_code(e);}
namespace {
bool capture_current(Capture& out,std::string& e){auto& r=runtime();return work_pin(r.context,r.binding,out,e);}
JobLease lease(const Capture& c,const LocalWork& local,uintptr_t queue,std::string& e){
    uintptr_t region=0;if(local.parent&&!field(local.parent,0x178,region,e))return {};
    auto& r=runtime();return {c.image,queue,local.bug,local.parent,region,c.thread,r.context.snapshot_uuid,job_key(local.row.birth),r.binding.native_epoch,r.binding.native_generation};
}
bool authorize_job(const JobLease& owner,std::string& e){
    Capture c;if(!capture_current(c,e))return false;auto& r=runtime();
    const auto* bug=r.lifetimes.pointer(owner.bug);uintptr_t parent=0,collection=0;
    if(!bug||!work_parent(bug->row,parent,collection,e))return fail(e,e.empty()?"Bug Job selected lease has no live witnessed Bug":e);
    uintptr_t region=0,queue=0,singleton=0;if(!field(parent,0x178,region,e)||
        !field(reinterpret_cast<uintptr_t>(c.pin.origin.state),0x1a8,queue,e)||!field(c.image,0x13df230,singleton,e))return false;
    return (owner.image==c.image&&owner.thread==c.thread&&owner.snapshot==r.context.snapshot_uuid&&owner.epoch==r.binding.native_epoch&&owner.generation==r.binding.native_generation&&
        owner.parent==parent&&owner.region==region&&owner.queue==queue&&queue==singleton)||fail(e,"Bug Job selected lease changed");
}
bool set_job_pose(const Capture& capture,uintptr_t job,const JobRow& target,bool& attempted,std::string& e){
    uintptr_t region=0;const auto map=reinterpret_cast<uintptr_t>(capture.pin.origin.map);
    if(target.region_id>=0&&!region_saved_identity::resolve(capture.image,map,map+0x1c0,target.region_id,0,region,
        [](uintptr_t p,size_t o,auto& value,std::string& error){return field(p,o,value,error);},e))return false;
    std::array<float,3> before{};uintptr_t old_region=0;
    if(!field(job,0x1c,before,e)||!field(job,0xb0,old_region,e)||!fresh(capture,e))return false;
    uint32_t exception=0;
    if(before!=target.position){attempted=true;if(!work_store(job+0x1c,before.data(),target.position.data(),12,&exception))return fail(e,"Bug Job position compare/store failed SEH="+std::to_string(exception));}
    if(old_region!=region){attempted=true;if(!work_store(job+0xb0,&old_region,&region,8,&exception))return fail(e,"Bug Job Region compare/store failed SEH="+std::to_string(exception));}
    return true;
}
bool erase_aborted(const Capture& capture,uintptr_t pointer,bool& attempted,std::string& e){
    census::Census before;if(!census::read(reader(),capture,before,e))return false;
    const auto found=std::find_if(before.jobs.begin(),before.jobs.end(),[&](const auto& j){return j.pointer==pointer;});
    if(found==before.jobs.end()||found->type||found->valid||found->developer||found->bug.pointer||found->building||found->building_proxy||found->monster||found->monster_proxy)
        return fail(e,"Bug Job selected invalid retirement still has target/Developer references");
    for(const auto& dev:before.developers)if(dev.job==pointer)return fail(e,"Bug Job selected invalid retirement has incoming Developer reference");
    const auto index=size_t(found-before.jobs.begin());auto expected=before;expected.jobs.erase(expected.jobs.begin()+static_cast<ptrdiff_t>(index));
    for(auto& dev:expected.developers){dev.job_index=-1;for(size_t i=0;i<expected.jobs.size();++i)if(expected.jobs[i].pointer==dev.job)dev.job_index=int32_t(i);}
    std::array<unsigned char,0xc8> body{},check{};if(!runtime().memory.copy(pointer,body.data(),body.size(),e)||!fresh(capture,e))return false;
    if(index+1<before.jobs.size()){JobCall call;developer_staff::construction::shift(capture.image,before.data+index*8,before.data+(index+1)*8,(before.jobs.size()-index-1)*8,&call);
        if(!returns(call,attempted,"selected invalid queue shift",e))return false;}
    attempted=true;if(!live_host_state::compare_fixed_i32(before.queue+0x28,int32_t(before.jobs.size()),int32_t(expected.jobs.size()),e))return false;
    Capture now;census::Census after;if(!capture_current(now,e)||!census::read(reader(),now,after,e)||after!=expected||!runtime().memory.copy(pointer,check.data(),check.size(),e)||check!=body)
        return fail(e,e.empty()?"Bug Job selected invalid erase changed another owner":e);
    JobCall call;work_unary(capture.image+0xc2e810,pointer,&call);if(!returns(call,attempted,"selected invalid destructor",e))return false;
    return capture_current(now,e)&&census::read(reader(),now,after,e)&&(after==expected||fail(e,"Bug Job original invalid destructor changed surviving graph"));
}
bool apply_counter(const Capture& capture,uintptr_t queue,size_t offset,const JobCounter& target,bool& attempted,std::string& e){
    JobCounter before;if(!read_counter(capture,queue,offset,before,e))return false;
    npc_quest_daily_apply::Receipt receipt;
    auto store=[&](uintptr_t p,const void* a,const void* b,size_t n,std::string& cause){uint32_t exception=0;return work_store(p,a,b,n,&exception)||fail(cause,"Bug Job DailyCount compare/store failed SEH="+std::to_string(exception));};
    auto reserve=[&](uintptr_t p,int32_t capacity,std::string& cause){JobCall call;work_reserve(capture.image,p,capacity,&call);return returns(call,attempted,"DailyCount Reserve",cause);};
    const bool ok=npc_quest_daily_apply::apply_daily(runtime().memory,capture.image,queue,offset,before.value,target.value,target.highest,target.day,store,reserve,
        [&](std::string& cause){return fresh(capture,cause);},receipt,e);
    attempted|=receipt.mutation_attempted;return ok;
}
}
bool begin_work(const Capture& capture,const WorkState& target,bool& attempted,std::string& e){
    auto& r=runtime();std::lock_guard lock(r.gate);if(!preflight_work(capture,target,e))return false;
    census::Census queue;if(!census::read(reader(),capture,queue,e))return false;
    NativeJobBirth services(reader(),authorize_job,capture_current);
    for(const auto& row:target.jobs){auto* local=r.jobs.identity(row.birth);
        if(!local){
            if(row.type==7){const auto* bug=r.lifetimes.identity(bug_row(row.bug));if(!bug)return fail(e,"Bug Job authoritative birth still lacks prepared Bug");
                LocalWork selected{row,0,bug->bug,bug->parent,false};auto owner=lease(capture,selected,queue.queue,e);if(!e.empty())return false;
                r.job_birth_calls.emplace_back();auto& receipt=r.job_birth_calls.back();const bool ok=create_job(services,owner,receipt);attempted|=receipt.attempted;
                if(!ok)return fail(e,receipt.cause);selected.job=receipt.calls.job;
                if(!r.jobs.born(std::move(selected),e))return false;
            }else{
                // A frame can first observe a Job after Host abort, while its
                // Developer still owns the occurrence. No Bug is recreated.
                r.job_birth_calls.emplace_back();auto& retained=r.job_birth_calls.back();retained.admitted=true;
                retained.lease={capture.image,queue.queue,0,0,0,capture.thread,r.context.snapshot_uuid,job_key(row.birth),r.binding.native_epoch,r.binding.native_generation};
                auto& birth=retained.calls;construct_job(capture.image,&birth);retained.attempted=birth.allocate.attempted||birth.construct.attempted;
                if(!returns(birth.allocate,attempted,"aborted Job allocation",e)||!returns(birth.construct,attempted,"aborted Job constructor",e)||!birth.job)return false;
                auto& abort=birth.configure;work_unary(capture.image+0x2817a0,birth.job,&abort);if(!returns(abort,attempted,"Job Abort",e))return false;
                if(!set_job_pose(capture,birth.job,row,attempted,e))return false;
                auto& insert=birth.insert;insert_job(capture.image,queue.queue,birth.job,&insert);if(!returns(insert,attempted,"aborted Job insertion",e))return false;
                if(!r.jobs.born({row,birth.job,0,0,false},e))return false;
                retained.confirmed=true;
            }
            local=r.jobs.identity(row.birth);
        }
        if(!local)return fail(e,"Bug Job selected birth lost its occurrence");
        int32_t type=0;if(!field(local->job,0x18,type,e))return false;
        if(type==7&&row.type==0){auto owner=lease(capture,*local,queue.queue,e);if(!e.empty())return false;
            subscriber_lifecycle_append::BugPairReceipt pair;const bool ok=services.detach(owner,local->job,pair,e);attempted|=pair.attempted;if(!ok)return false;
            JobCall abort;work_unary(capture.image+0x2817a0,local->job,&abort);if(!returns(abort,attempted,"selected Job Abort",e))return false;
        }else if(type!=row.type)return fail(e,"Bug Job target reverses an original abort");
        if(!set_job_pose(capture,local->job,row,attempted,e))return false;
    }
    return fresh(capture,e);
}
bool work_actor_links(const Capture& capture,const WorkState& target,uintptr_t dp,uintptr_t jp,bool attach,bool& attempted,std::string& e){
    auto& r=runtime();std::lock_guard lock(r.gate);Capture now;census::Census queue;if(!capture_current(now,e)||!census::read(reader(),now,queue,e))return false;
    for(const auto& local:r.jobs.rows())if(!local.retired){
        const auto current=std::find_if(queue.jobs.begin(),queue.jobs.end(),[&](const auto& j){return j.pointer==local.job;});
        if(current==queue.jobs.end())return fail(e,"Bug Job actor pairing lacks current queue occurrence");
        const auto next=std::find_if(target.jobs.begin(),target.jobs.end(),[&](const auto& row){return row.birth==local.row.birth;});
        const int32_t desired=next==target.jobs.end()?-1:next->developer_id;
        const int32_t selected=attach?desired:current->developer_id;
        if(selected<0||desired==current->developer_id)continue;
        const auto actor=std::find_if(capture.state.rows.begin(),capture.state.rows.end(),[&](const auto& row){return row.id==selected;});
        if(actor==capture.state.rows.end())return fail(e,"Bug Job assigned Developer absent from fixed50");
        struct Owner{const Capture* capture;};Owner owner{&capture};
        auto guard=[](void* user,std::string& cause){return fresh(*static_cast<Owner*>(user)->capture,cause);};
        subscriber_lifecycle_append::DeveloperPairReceipt receipt;
        const subscriber_lifecycle_append::DeveloperPairRequest request{capture.image,capture.local.developers[size_t(actor-capture.state.rows.begin())],local.job,dp,jp,capture.thread,attach,job_key(local.row.birth),guard,&owner};
        const bool ok=subscriber_lifecycle_append::bind_developer_pair(request,receipt,e);attempted|=receipt.attempted;if(!ok)return false;
    }
    return true;
}
bool finish_work(const Capture& capture,const WorkState& target,bool& attempted,std::string& e){
    auto& r=runtime();std::lock_guard lock(r.gate);NativeJobBirth services(reader(),authorize_job,capture_current);
    std::vector<LocalWork> removed;for(const auto& local:r.jobs.rows())if(!local.retired&&std::none_of(target.jobs.begin(),target.jobs.end(),[&](const auto& row){return row.birth==local.row.birth;}))removed.push_back(local);
    for(const auto& local:removed){Capture now;census::Census queue;if(!capture_current(now,e)||!census::read(reader(),now,queue,e))return false;
        const auto found=std::find_if(queue.jobs.begin(),queue.jobs.end(),[&](const auto& j){return j.pointer==local.job;});
        if(found==queue.jobs.end())return fail(e,"Bug Job selected retirement disappeared before call");
        if(found->type==0){if(!erase_aborted(capture,local.job,attempted,e))return false;}
        else{auto owner=lease(capture,local,queue.queue,e);if(!e.empty())return false;r.job_retire_calls.emplace_back();auto& receipt=r.job_retire_calls.back();
            const bool ok=retire_job(services,owner,local.job,receipt);attempted|=receipt.attempted;if(!ok)return fail(e,receipt.cause);}
    }
    NativeParent parents(capture);
    for(const auto& target_parent:target.parents){ParentOwner owner;if(!parent_owner(capture,target_parent,owner,e))return false;
        ParentReceipt receipt;const bool ok=apply_parent_visual(parents,owner,target_parent,receipt);attempted|=receipt.attempted;if(!ok)return fail(e,receipt.cause);}
    Capture now;census::Census queue;if(!capture_current(now,e)||!census::read(reader(),now,queue,e))return false;
    return apply_counter(capture,queue.queue,0x48,target.created,attempted,e)&&apply_counter(capture,queue.queue,0xb8,target.fixed,attempted,e);
}
}
