#include "building_status_work.h"
#include "bug_authority/runtime_internal.h"
#include "bug_authority/work_runtime.h"
#include "bug_authority/codec.h"
#include "developer_staff/action_native.h"
#include "developer_staff/construction_native.h"
#include "typed_action_runtime.h"
#include "mt2mp/core/hash.h"
namespace mt2mp::game::building_status_work {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool digest(const nlohmann::json& value,std::string& out,std::string& error){const auto text=value.dump();const std::vector<unsigned char> bytes(text.begin(),text.end());out=mt2mp::sha256(bytes);if(out.empty())return fail(error,"Building status native work SHA256 failed");return true;}
}
bool read(const action_session::Context& context,const action_session::Binding& binding,uintptr_t image,uint32_t thread,
          uintptr_t world,uintptr_t map,uintptr_t building,uintptr_t collection,const std::string& key,int32_t target,Readback& out,std::string& error){
    out={};
    developer_staff::Capture staff;bug_authority::State bugs;developer_staff::PoolState pool;
    if(!bug_authority::action_state(context,binding,bugs,staff,error)||!developer_staff::action_runtime::capture(context,binding,pool,error))return false;
    if(staff.image!=image||staff.thread!=thread||uintptr_t(staff.pin.origin.world)!=world||uintptr_t(staff.pin.origin.map)!=map)return fail(error,"Building status Bug/Developer capture world differs");
    if(!pool.construction){std::vector<developer_staff::construction::LocalJob> jobs;if(!typed_action_runtime::construction_jobs(context,binding,jobs,error))return false;developer_staff::construction::State extra;if(!developer_staff::construction::capture(staff,jobs,extra,error))return false;pool.construction=std::move(extra);}
    developer_staff::job_census::Census queue;bug_authority::WorkState work;std::vector<uintptr_t> bound;
    auto read=[&](auto object,auto bytes,auto size,auto& why){return bug_authority::runtime().memory.copy(object,bytes,size,why);};
    if(!developer_staff::job_census::read(read,staff,queue,error)||!bug_authority::capture_work(staff,queue,work,bound,error))return false;pool.construction->work=std::move(work);
    if(!developer_staff::valid(pool,error))return false;out.jobs=uint32_t(queue.jobs.size());out.developers=uint32_t(pool.rows.size());out.bugs=uint32_t(bugs.rows.size());
    auto expected=bugs;auto& tracker=bug_authority::runtime();std::lock_guard lock(tracker.gate);std::set<uintptr_t> children,linked;
    for(auto& row:expected.rows)if(row.parent_kind==bug_authority::ParentKind::Building&&row.parent_key==key){
        const auto* local=tracker.lifetimes.identity(row);uintptr_t job=0;
        if(!local||local->parent!=building||local->collection!=collection||!children.insert(local->bug).second||!bug_authority::field(local->bug,0x48,job,error))return fail(error,error.empty()?"Building status selected Bug original lifetime/collection differs":error);
        out.children.push_back({local->bug,local->parent,local->collection});
        if(job){if(!linked.insert(job).second)return fail(error,"Building status multiple Bugs share one native Job");const auto found=std::find_if(queue.jobs.begin(),queue.jobs.end(),[&](const auto& value){return value.pointer==job;});if(found==queue.jobs.end()||found->bug.pointer!=local->bug)return fail(error,"Building status linked Bug Job lacks exact current queue witness");++out.linked;}
        if(target==2){if(job)row.value.manual=true;}else if(!job)row.value.manual=false;
    }
    std::vector<uintptr_t> actual_children;if(collection&&!bug_authority::local_members(collection,actual_children,error))return false;
    if(actual_children.size()!=children.size()||std::any_of(actual_children.begin(),actual_children.end(),[&](auto child){return !children.contains(child);}))return fail(error,"Building status native Bug collection and witnessed parent census differ");
    out.bugs_record=bug_authority::encode(bugs);out.pool_record=developer_staff::encode(pool);
    return digest(out.bugs_record,out.bugs_sha256,error)&&digest(bug_authority::encode(expected),out.expected_bugs_sha256,error)&&digest(out.pool_record,out.pool_sha256,error);
}
}
