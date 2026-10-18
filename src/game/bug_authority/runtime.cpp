#include "live_host_state/memory_query_timing.h"
#include "runtime_internal.h"
#include "codec.h"
#include "work_runtime.h"
#include "job_codec.h"
#include "typed_action_runtime.h"
#include "remove_diagnostics.h"
#include "npc_probe.h"
#include "npc_target_key.h"
#include "developer_staff/job_census.h"
#include "mt2mp/core/logger.h"
#include <Windows.h>
#include <algorithm>
#include <tuple>
namespace mt2mp::game::bug_authority {
Runtime& runtime(){static Runtime value;return value;}
bool reject(const std::string& cause,std::string& error){
    auto& r=runtime();if(!r.locked){r.locked=true;r.cause=cause;LOG_ERROR("[BugAuthority] {}",cause);}
    error=r.cause;return false;
}
bool local_members(uintptr_t collection,std::vector<uintptr_t>& out,std::string& error){
    out.clear();if(!collection)return true;
    uintptr_t ap=0,array=0,data=0;int32_t count=0,capacity=0;
    if(!field(collection,0,ap,error)||!field(collection,0x18,array,error)||!field(collection,0x20,data,error)||
       !field(collection,0x28,count,error)||!field(collection,0x2c,capacity,error))return false;
    if(ap!=runtime().image+0x1705640||array!=runtime().image+0x16ef1c0||count<0||capacity<count||capacity>1024*1024||(count&&!data))
        return fail(error,"Bug collection exact class/count/capacity differs");
    out.resize(static_cast<size_t>(count));if(count&&!runtime().memory.copy(data,out.data(),out.size()*sizeof(uintptr_t),error))return false;
    std::set<uintptr_t> seen;for(const auto value:out)if(!value||!seen.insert(value).second)return fail(error,"Bug collection has null/duplicate member");
    return true;
}
bool active_world(Context& context,Binding& binding,uint64_t& publication,bool& active,std::string& error){
    auto& r=runtime();
    if(!typed_action_runtime::bug_context(context,binding,publication,active,error))return false;
    if(!active)return true;
    if(!r.ready||GetCurrentThreadId()!=r.thread)return fail(error,"Bug authority native owner/thread unavailable");
    if(r.locked)return fail(error,r.cause);
    if(!r.snapshot_sealed||r.snapshot_uuid!=context.snapshot_uuid)return fail(error,"Bug authority lacks current common snapshot serialization witness");
    return true;
}
namespace {
creation::Read reader(){return [](uintptr_t p,void* out,size_t n,std::string& error){return runtime().memory.copy(p,out,n,error);};}
Value value(const creation::Value& v){return {v.type,v.created,v.discovered,v.toons,v.variant,v.reproducibility,v.known,v.manual,v.debugged};}
creation::Value value(const Value& v){return {v.type,v.created,v.discovered,v.toons,v.variant,v.reproducibility,v.known,v.manual,v.debugged};}
bool parent_from_local(uintptr_t object,ParentKind& kind,std::string& key,std::string& error){
    uintptr_t ap=0;uint64_t uid=0;if(!field(object,0,ap,error)||!field(object,0x18,uid,error))return false;
    auto& r=runtime();
    if(ap==r.image+0x1742980){kind=ParentKind::Npc;std::string target;
        if(!npc_probe::action_runtime::local_key(object,uid,target,error))return false;
        if(!npc_target_key::valid(target))return fail(error,"Bug NPC resolver returned a noncanonical parent key");key=target;return true;}
    uintptr_t selection=0;if(!field(r.image,0x13df630,selection,error))return false;
    remove_diagnostics::LocalTarget target;
    if(!remove_diagnostics::resolve_local_target(selection,object,target,error))return false;
    if(!target.normal_building||target.object!=object||target.uid!=uid||target.snapshot_id!=r.context.snapshot_uuid||
       target.world_epoch!=r.binding.native_epoch||target.world_generation!=r.binding.native_generation)
        return fail(error,"Bug Building parent common/native binding differs");
    kind=ParentKind::Building;key=target.record_key;return true;
}
bool resolve_parent(const Row& row,uintptr_t& parent,uintptr_t& collection,std::string& error){
    auto& r=runtime();parent=collection=0;
    if(row.parent_kind==ParentKind::Npc){if(!npc_probe::action_runtime::bug_parent(r.context,r.binding,row.parent_key,parent,error))return false;}
    else{remove_diagnostics::LocalTarget target;if(!remove_diagnostics::resolve_record_target(r.context.snapshot_uuid,row.parent_key,target,error))return false;
        if(!target.normal_building||target.snapshot_id!=r.context.snapshot_uuid||target.world_epoch!=r.binding.native_epoch||target.world_generation!=r.binding.native_generation)
            return fail(error,"Bug resolved Building parent native/common binding differs");parent=target.object;}
    return field(parent,row.parent_kind==ParentKind::Building?0xbd8:0x848,collection,error);
}
creation::Lease lease(const Row& row,uintptr_t parent,uintptr_t collection){
    auto& r=runtime();return {r.image,parent,collection,r.tracker,r.lounge,row.parent_kind==ParentKind::Building?creation::Parent::Building:creation::Parent::Npc,
        r.context.snapshot_uuid,row.parent_key,row.birth.source+":"+std::to_string(row.birth.sequence)+":"+std::to_string(row.birth.occurrence),r.binding.native_epoch,r.binding.native_generation};
}
bool tracker_members(std::vector<uintptr_t>& out,std::string& error){
    live_host_state::apply_timing::PageReadScope pages;
    auto& r=runtime();uintptr_t data=0;int32_t count=0,capacity=0;
    if(!field(r.tracker,0x18,data,error)||!field(r.tracker,0x20,count,error)||!field(r.tracker,0x24,capacity,error))return false;
    if(count<0||capacity<count||capacity>1024*1024||(count&&!data))return fail(error,"Bug Tracker count/capacity invalid");
    out.resize(static_cast<size_t>(count));if(count&&!r.memory.copy(data,out.data(),out.size()*8,error))return false;
    std::set<uintptr_t> unique;for(auto ptr:out)if(!ptr||!unique.insert(ptr).second)return fail(error,"Bug Tracker live member duplicate/null");return true;
}
bool pin(const Context& expected,const Binding& binding,developer_staff::Capture& capture,std::string& error){
    auto& r=runtime();Context actual;Binding current;uint64_t revision=0;bool active=false;
    if(!active_world(actual,current,revision,active,error))return false;
    if(!active||actual!=expected||current!=binding)return fail(error,"Bug frame current Controller context/binding differs");
    if(!developer_staff::read_process(capture,error))return false;
    if(capture.image!=r.image||capture.thread!=r.thread||capture.pin.origin.world_epoch!=binding.native_epoch||capture.pin.origin.generation!=binding.native_generation||
       (actual.role==action_session::Role::Client&&capture.pin.time_scale!=0))return fail(error,"Bug frame native Origin/thread/Client Clock0 differs");
    r.context=actual;r.binding=binding;r.lounge=capture.local.lounge;r.tracker=r.lounge+0x100;
    uintptr_t singleton=0;if(!field(r.image,0x13df2e0,singleton,error))return false;
    if(singleton!=r.tracker)return fail(error,"Bug Tracker singleton differs from current Lounge");
    return native_code(error);
}
bool complete_bindings(std::string& error){
    auto& r=runtime();std::vector<uintptr_t> all;if(!tracker_members(all,error))return false;
    // Snapshot ordinals come only from the completed original serde scope.
    for(const auto& saved:r.saved){
        for(size_t index=0;index<saved.children.size();++index){const auto ptr=saved.children[index];
            if(!ptr||std::find(all.begin(),all.end(),ptr)==all.end())continue;
            if(r.lifetimes.pointer(ptr))continue;
            uintptr_t parent=0;if(!field(ptr,0x50,parent,error))return false;
            ParentKind kind;std::string key;if(!parent_from_local(parent,kind,key,error))return false;
            Row row{kind,key,{BirthKind::Snapshot,r.snapshot_sha,0,index},{}};
            if(!r.lifetimes.born({row,ptr,parent,saved.collection,false},error))return false;
        }
    }
    r.saved.clear();
    for(const auto ptr:all){auto* local=r.lifetimes.pointer(ptr);if(!local)return fail(error,"Live Bug lacks original snapshot/action/publication birth witness");
        uintptr_t parent=0;if(!field(ptr,0x50,parent,error))return false;
        ParentKind kind;std::string key;if(!parent_from_local(parent,kind,key,error))return false;
        if(!r.lifetimes.bind_parent(ptr,kind,key,parent,error))return false;
        uintptr_t resolved=0,collection=0;if(!resolve_parent(local->row,resolved,collection,error))return false;
        if(resolved!=parent||collection!=local->collection)return fail(error,"Bug witnessed parent collection lifetime differs");
    }
    for(const auto& local:r.lifetimes.rows())if(!local.retired&&std::find(all.begin(),all.end(),local.bug)==all.end())
        return fail(error,"Bug left Tracker without observed original removal");
    return true;
}
bool observe(State& out,std::string& error){
    auto& r=runtime();out={};if(!complete_bindings(error))return false;
    for(const auto& local:r.lifetimes.rows())if(!local.retired){
        creation::Snapshot snapshot;if(!creation::observe(reader(),lease(local.row,local.parent,local.collection),snapshot,error))return false;
        const auto found=std::find_if(snapshot.bugs.begin(),snapshot.bugs.end(),[&](const auto& b){return b.pointer==local.bug;});
        if(found==snapshot.bugs.end())return fail(error,"Bug witnessed receiver absent during full Tracker readback");
        Row row=local.row;row.value=value(found->value);out.rows.push_back(std::move(row));
    }
    std::sort(out.rows.begin(),out.rows.end(),[](const Row& a,const Row& b){return std::tie(a.parent_kind,a.parent_key,a.birth.kind,a.birth.source,a.birth.sequence,a.birth.occurrence)<
        std::tie(b.parent_kind,b.parent_key,b.birth.kind,b.birth.source,b.birth.sequence,b.birth.occurrence);});
    return true;
}
bool untouched(const developer_staff::Capture& before,std::string& error){
    developer_staff::Capture after;if(!developer_staff::read_process(after,error))return false;
    if(!developer_staff::same_origin(before.pin.origin,after.pin.origin)||before.pin.ledger!=after.pin.ledger||before.pin.time_scale!=after.pin.time_scale||
       before.pin.pause_scale!=after.pin.pause_scale||before.local!=after.local||before.state!=after.state)return fail(error,"Bug adoption changed Origin/full ledger/Clock/Developer pool");return true;
}
bool prepare(const Context& context,const Binding& binding,uint64_t publication,const State& target,developer_staff::Capture& before,State& current,std::string& error){
    if(context.role!=action_session::Role::Client||!publication)return fail(error,"Bug adoption requires Client publication");
    if(!valid(target,error))return false;
    if(!std::is_sorted(target.rows.begin(),target.rows.end(),[](const Row& a,const Row& b){return std::tie(a.parent_kind,a.parent_key,a.birth.kind,a.birth.source,a.birth.sequence,a.birth.occurrence)<std::tie(b.parent_kind,b.parent_key,b.birth.kind,b.birth.source,b.birth.sequence,b.birth.occurrence);}))
        return fail(error,"Bug authority rows are not in stable identity order");
    if(!pin(context,binding,before,error)||!observe(current,error))return false;
    developer_staff::job_census::Census jobs;if(!([&]{live_host_state::apply_timing::PageReadScope pages;return developer_staff::job_census::read(reader(),before,jobs,error);}()))return false;
    auto& r=runtime();
    for(const auto& row:target.rows){
        if(row.birth.kind==BirthKind::Snapshot&&row.birth.source!=r.snapshot_sha)return fail(error,"Bug snapshot birth digest differs from original shared capture");
        if(row.birth.kind==BirthKind::Action&&!r.confirmed_actions.contains(row.birth.source))return fail(error,"Bug Action birth lacks locally confirmed ordered action");
        if(row.birth.kind==BirthKind::Publication&&(row.birth.source!=context.snapshot_uuid||row.birth.sequence>publication))return fail(error,"Bug publication birth is outside current frame");
        uintptr_t parent=0,collection=0;if(!resolve_parent(row,parent,collection,error))return false;
        auto* local=r.lifetimes.identity(row);
        if(!local&&r.lifetimes.consumed(row))return fail(error,"Bug retired birth identity cannot replay");
        if(local&&(local->parent!=parent||local->collection!=collection))return fail(error,"Bug authority identity resolves to a replacement parent");
        if(!local&&row.birth.kind==BirthKind::Snapshot)return fail(error,"Bug original shared snapshot child is missing locally");
        if(local){uintptr_t job=0;if(!field(local->bug,0x48,job,error))return false;
            if(!r.frame_work&&(job||std::any_of(jobs.jobs.begin(),jobs.jobs.end(),[&](const auto& j){return j.bug.pointer==local->bug;})))return fail(error,"Bug Client occurrence has an unreplicated native Job link");
            const auto old=std::find_if(current.rows.begin(),current.rows.end(),[&](const Row& x){return same_identity(x,row);});
            if(old==current.rows.end())return fail(error,"Bug witnessed occurrence is absent from current state");
        }
        if(!r.frame_work&&(row.value.manual||row.value.debugged))return fail(error,"Bug manual/debug activity lacks authoritative Work state");
    }
    for(const auto& local:r.lifetimes.rows())if(!local.retired&&std::none_of(target.rows.begin(),target.rows.end(),[&](const Row& x){return same_identity(x,local.row);})){
        uintptr_t job=0;if(!field(local.bug,0x48,job,error))return false;
        if(!r.frame_work&&(job||std::any_of(jobs.jobs.begin(),jobs.jobs.end(),[&](const auto& j){return j.bug.pointer==local.bug;})))return fail(error,"Bug retirement still has native Job references");
    }
    return untouched(before,error);
}
}
bool work_parent(const Row& row,uintptr_t& parent,uintptr_t& collection,std::string& error){return resolve_parent(row,parent,collection,error);}
bool work_pin(const Context& context,const Binding& binding,developer_staff::Capture& capture,std::string& error){return pin(context,binding,capture,error);}
bool work_bindings(std::string& error){return complete_bindings(error);}
bool action_state(const Context& context,const Binding& binding,State& out,developer_staff::Capture& before,std::string& error){
    auto& r=runtime();std::lock_guard lock(r.gate);
    return pin(context,binding,before,error)&&observe(out,error)&&untouched(before,error);
}
bool witness_birth(uintptr_t collection,uintptr_t bug,unsigned lane,std::string& error){
    if(!bug)return true;auto& r=runtime();Context context;Binding binding;uint64_t publication=0;bool active=false;
    if(!active_world(context,binding,publication,active,error))return false;if(!active)return true;
    if(r.context!=context||r.binding!=binding)return fail(error,"Bug birth does not belong to activated snapshot/Controller");
    if(r.publication!=publication&&r.action.empty()){r.publication=publication;r.publication_random=r.publication_specific=r.publication_record=0;}
    auto& counter=r.action.empty()?(lane==1?r.publication_random:(lane==2?r.publication_specific:r.publication_record)):
        (lane==1?r.random_occurrence:(lane==2?r.specific_occurrence:r.record_occurrence));
    if(counter>=((uint64_t(1)<<56)-1))return fail(error,"Bug creation occurrence overflow");
    Birth birth{r.action.empty()?BirthKind::Publication:BirthKind::Action,r.action.empty()?context.snapshot_uuid:r.action,r.action.empty()?publication:0,(uint64_t(lane)<<56)|++counter};
    Row row;row.birth=std::move(birth);return r.lifetimes.born({row,bug,0,collection,false},error);
}
bool witness_removal(uintptr_t tracker,uintptr_t bug,std::string& error){
    auto& r=runtime();
    for(auto& saved:r.saved)for(auto& child:saved.children)if(child==bug)child=0;
    if(tracker!=r.tracker)return true;
    return r.lifetimes.remove(bug,error);
}
bool capture(const Context& context,const Binding& binding,uint64_t publication,State& out,std::string& error){
    auto& r=runtime();std::lock_guard lock(r.gate);developer_staff::Capture before;
    if(context.role!=action_session::Role::Host||!publication)return fail(error,"Bug capture requires Host publication");
    return pin(context,binding,before,error)&&observe(out,error)&&untouched(before,error);
}
bool preflight_frame(const Context& context,const Binding& binding,uint64_t publication,const State& target,std::string& error,const WorkState* work){
    auto& r=runtime();std::lock_guard lock(r.gate);developer_staff::Capture before;State current;
    r.frame_work=work?std::optional<WorkState>(*work):std::nullopt;r.frame_bugs=target;r.frame_publication=publication;
    if(work&&!work_codec::valid(*work,error))return false;
    return prepare(context,binding,publication,target,before,current,error);
}
bool adopt_frame(const Context& context,const Binding& binding,uint64_t publication,const State& target,bool& attempted,bool& unknown,std::string& error){
    attempted=unknown=false;auto& r=runtime();std::lock_guard lock(r.gate);developer_staff::Capture before;State current;
    if(!prepare(context,binding,publication,target,before,current,error))return false;
    try{
        auto authorize=[&](const creation::Lease& expected,std::string& cause){
            developer_staff::Capture fresh;if(!pin(context,binding,fresh,cause))return false;
            Row row;row.parent_kind=expected.kind==creation::Parent::Building?ParentKind::Building:ParentKind::Npc;row.parent_key=expected.parent_key;
            uintptr_t parent=0,collection=0;if(!resolve_parent(row,parent,collection,cause))return false;
            return (parent==expected.parent&&collection==expected.collection&&expected.snapshot==context.snapshot_uuid&&expected.epoch==binding.native_epoch&&expected.generation==binding.native_generation)||fail(cause,"Bug original call lease changed");};
        creation::NativeServices services(reader(),authorize,anchor_original());
        std::vector<Local> removed;for(const auto& local:r.lifetimes.rows())if(!local.retired&&std::none_of(target.rows.begin(),target.rows.end(),[&](const Row& x){return same_identity(x,local.row);}))removed.push_back(local);
        if(!r.frame_work)for(const auto& local:removed){bool call_attempted=false;const bool ok=native_remove(local.collection,local.bug,call_attempted,error);attempted=attempted||call_attempted;if(!ok)throw std::runtime_error(error);}
        for(const auto& row:target.rows){
            auto* local=r.lifetimes.identity(row);uintptr_t parent=0,collection=0;if(!resolve_parent(row,parent,collection,error))throw std::runtime_error(error);
            auto owner=lease(row,parent,collection);
            if(!local){creation::CollectionReceipt attached;if(!services.ensure_collection(owner,attached)){attempted=attempted||attached.attempted;throw std::runtime_error(attached.cause);}attempted=attempted||attached.attempted;
                owner.collection=attached.collection;creation::Receipt born;
                auto initial=value(row.value);initial.manual=false;initial.debugged=false;
                const bool ok=create_replica(services,owner,initial,born);attempted=attempted||born.attempted;
                if(!ok)throw std::runtime_error(born.cause);
                if(!r.lifetimes.born({row,born.bug,parent,owner.collection,false},error))throw std::runtime_error(error);
                if(row.value.manual||row.value.debugged){attempted=true;
                    if(!services.scalars(owner,born.bug,value(row.value),error)||!services.anchor(owner,born.bug,error))throw std::runtime_error(error);}
            }else{
                const auto old=std::find_if(current.rows.begin(),current.rows.end(),[&](const Row& x){return same_identity(x,row);});
                if(old->value==row.value)continue;
                attempted=true;if(!services.scalars(owner,local->bug,value(row.value),error)||!services.anchor(owner,local->bug,error))throw std::runtime_error(error);
            }
        }
        State actual;if(!observe(actual,error))throw std::runtime_error(error);
        if(r.frame_work){for(const auto& row:target.rows){const auto found=std::find_if(actual.rows.begin(),actual.rows.end(),[&](const auto& x){return same_identity(x,row);});
            if(found==actual.rows.end()||*found!=row)throw std::runtime_error("Bug prepared scalar/birth readback differs");}}
        if((!r.frame_work&&actual!=target)||!untouched(before,error)){
            if(error.empty())error="Bug full authoritative state readback differs";throw std::runtime_error(error);}
        return true;
    }catch(const std::exception& ex){if(error.empty())error=ex.what();}
    catch(...){if(error.empty())error="Bug authority native apply nonstandard exception";}
    unknown=attempted;if(unknown)return reject(error,error);return false;
}
bool finish_frame(const Context& context,const Binding& binding,uint64_t publication,const State& target,bool& attempted,bool& unknown,std::string& error){
    attempted=unknown=false;auto& r=runtime();std::lock_guard lock(r.gate);
    if(!r.frame_work)return true;
    developer_staff::Capture before;if(r.frame_bugs!=target||r.frame_publication!=publication||!pin(context,binding,before,error))return fail(error,error.empty()?"Bug finish lost its prepared publication":error);
    try{
        std::vector<Local> removed;for(const auto& local:r.lifetimes.rows())if(!local.retired&&std::none_of(target.rows.begin(),target.rows.end(),[&](const auto& row){return same_identity(row,local.row);}))removed.push_back(local);
        developer_staff::job_census::Census jobs;if(!([&]{live_host_state::apply_timing::PageReadScope pages;return developer_staff::job_census::read(reader(),before,jobs,error);}()))throw std::runtime_error(error);
        for(const auto& local:removed){uintptr_t job=0;if(!field(local.bug,0x48,job,error))throw std::runtime_error(error);
            if(job||std::any_of(jobs.jobs.begin(),jobs.jobs.end(),[&](const auto& x){return x.bug.pointer==local.bug;}))throw std::runtime_error("Bug finish still has a native Job reference");
            bool called=false;const bool ok=native_remove(local.collection,local.bug,called,error);attempted|=called;if(!ok)throw std::runtime_error(error);
        }
        State actual;if(!observe(actual,error)||actual!=target||!untouched(before,error))throw std::runtime_error(error.empty()?"Bug final authoritative state differs":error);
        return true;
    }catch(const std::exception& cause){if(error.empty())error=cause.what();}
    catch(...){if(error.empty())error="Bug finish nonstandard exception";}
    unknown=attempted;if(unknown)return reject(error,error);return false;
}
bool begin_action(const Context& context,const Binding& binding,const std::string& action,std::string& error){
    auto& r=runtime();std::lock_guard lock(r.gate);developer_staff::Capture before;
    if(!r.action.empty()||action.empty())return fail(error,"Bug action scope nested or UUID absent");
    if(!pin(context,binding,before,error)||!complete_bindings(error))return false;
    r.job_action_occurrence=0;r.action=action;r.random_occurrence=r.specific_occurrence=r.record_occurrence=0;return true;
}
bool end_action(bool confirmed,std::string& error){
    auto& r=runtime();std::lock_guard lock(r.gate);if(r.action.empty())return fail(error,"Bug action scope absent at return");
    if(confirmed)r.confirmed_actions.insert(r.action);
    r.action.clear();
    if(!confirmed&&(r.random_occurrence||r.specific_occurrence||r.record_occurrence))return reject("Bug ordered action did not confirm; retained births cannot replay",error);
    return !r.locked||fail(error,r.cause);
}
bool arm_snapshot(const std::string& uuid,const std::string& sha,bool host,std::string& error){
    auto& r=runtime();std::lock_guard lock(r.gate);
    if(!r.ready||uuid.empty()||(!host&&sha.size()!=64))return fail(error,"Bug snapshot arm lacks native hooks/UUID/digest");
    if(r.snapshot_armed)return fail(error,"Bug snapshot serde already armed");
    r.thread=GetCurrentThreadId();r.snapshot_armed=true;r.snapshot_sealed=false;r.snapshot_host=host;r.snapshot_uuid=uuid;r.snapshot_sha=sha;r.saved.clear();r.lifetimes.clear();r.saved_jobs.clear();r.jobs.clear();r.job_occurrence=r.job_publication=r.job_action_occurrence=0;r.action.clear();r.confirmed_actions.clear();r.ordered_job_birth.reset();
    r.frame_work.reset();r.frame_bugs={};r.frame_publication=0;r.locked=false;r.cause.clear();r.tracker=r.lounge=0;r.publication=0;return true;
}
bool seal_snapshot(const std::string& uuid,const std::string& sha,std::string& error){
    auto& r=runtime();std::lock_guard lock(r.gate);
    if(r.locked)return fail(error,r.cause);
    if(!r.snapshot_armed||uuid!=r.snapshot_uuid||sha.size()!=64||(!r.snapshot_host&&sha!=r.snapshot_sha))return fail(error,"Bug snapshot seal UUID/digest/lifetime differs");
    r.snapshot_sha=sha;r.snapshot_armed=false;r.snapshot_sealed=true;return true;
}
void cancel_snapshot(const std::string& cause){auto& r=runtime();std::lock_guard lock(r.gate);r.snapshot_armed=r.snapshot_sealed=false;r.saved.clear();r.lifetimes.clear();r.saved_jobs.clear();r.jobs.clear();r.job_occurrence=r.job_publication=r.job_action_occurrence=0;r.cause=cause;}
bool initialize(std::string& error){auto& r=runtime();if(r.ready)return native_code(error);r.image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));r.thread=GetCurrentThreadId();return install_native(error);}
}
