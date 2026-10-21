bool finish_duplicate(const PlacementBefore& before,const std::string& snapshot,
                      const std::map<uint64_t,std::string>& created,std::vector<LocalJob>& out,std::string& e){
    out.clear();Capture after;job_census::Census queue;
    if(!read_process(after,e)||!read_jobs(after,queue,e))return false;
    if(!same_origin(before.capture.pin.origin,after.pin.origin)||before.capture.image!=after.image||before.capture.thread!=after.thread||
       before.capture.state!=after.state||before.capture.local!=after.local||before.capture.pin.time_scale!=after.pin.time_scale||
       before.capture.pin.pause_scale!=after.pin.pause_scale||before.queue.queue!=queue.queue)
        return fail(e,"Construction original Duplicate changed Developer/world/queue owner");
    if(queue.jobs.size()<before.queue.jobs.size()||queue.jobs.size()-before.queue.jobs.size()>created.size()||queue.developers!=before.queue.developers)
        return fail(e,"Construction original Duplicate Job delta exceeds exact created Buildings");
    for(size_t i=0;i<before.queue.jobs.size();++i)if(before.queue.jobs[i]!=queue.jobs[i])
        return fail(e,"Construction original Duplicate changed a previous Job");
    struct Created {remove_diagnostics::LocalTarget target;uintptr_t region=0;};
    std::map<uintptr_t,Created> created_owners;std::set<std::string> keys;
    for(const auto& [uid,key]:created){Created value;
        if(!remove_diagnostics::resolve_record_target(snapshot,key,value.target,e)||!at(value.target.object,0x178,value.region,e))return false;
        const auto& target=value.target;
        if(!uid||!target.normal_building||target.normal_scenery||target.uid!=uid||target.record_key!=key||target.snapshot_id!=snapshot||
           target.world_epoch!=after.pin.origin.world_epoch||target.world_generation!=after.pin.origin.generation||
           !keys.insert(key).second||!created_owners.emplace(target.object,value).second)
            return fail(e,"Construction original Duplicate created Building binding/full UID/owner differs");
    }
    std::vector<LocalJob> admitted;std::set<uintptr_t> jobs,targets;
    for(const auto& job:before.queue.jobs)jobs.insert(job.pointer);
    for(size_t i=before.queue.jobs.size();i<queue.jobs.size();++i){const auto& job=queue.jobs[i];
        const auto found=created_owners.find(job.building);
        if(found==created_owners.end()||!jobs.insert(job.pointer).second||!targets.insert(job.building).second)
            return fail(e,"Construction original Duplicate new Job is foreign or repeated");
        const auto& owner=found->second;const auto& target=owner.target;
        if(job.type!=1||!job.valid||!job.building_proxy||job.region!=owner.region||job.developer||job.bug.pointer||job.monster||job.monster_proxy)
            return fail(e,"Construction original Duplicate new Job does not bind exact created Building");
        LocalJob link{target.record_key,job.pointer,target.uid,true,false,job.building,job.building_proxy,job.region};
        if(!occurrence(link,job,target.object,owner.region,copy,e))return false;
        admitted.push_back(std::move(link));
    }
    Capture final;job_census::Census repeated;
    if(!read_process(final,e)||!owners(after,final,e)||!read_jobs(final,repeated,e))return false;
    if(after.state!=final.state||after.local!=final.local||queue!=repeated)
        return fail(e,"Construction original Duplicate complete pool/Job graph changed during admission");
    std::sort(admitted.begin(),admitted.end(),[](const auto& a,const auto& b){return a.building_key<b.building_key;});
    out=std::move(admitted);e.clear();return true;
}
