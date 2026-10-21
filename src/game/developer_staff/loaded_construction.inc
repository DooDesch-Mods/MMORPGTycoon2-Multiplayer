bool capture_loaded(const std::string& snapshot,const action_session::Binding& binding,
                    std::vector<LocalJob>& out,std::string& e){
    out.clear();Capture before;job_census::Census queue;
    if(!read_process(before,e)||!read_jobs(before,queue,e))return false;
    if(snapshot.empty()||before.pin.origin.world_epoch!=binding.native_epoch||before.pin.origin.generation!=binding.native_generation)
        return fail(e,"Loaded construction snapshot/native Origin differs");
    // Snapshot refund reads do not install Free. Loaded target resolution
    // needs its actual receipt before the later UI capture installers run.
    auto& free=native_free_runtime::Runtime::instance();
    if(free.status().phase==native_free_runtime::Phase::Idle&&!free.prepare_and_enable(e))return false;
    native_free_runtime::Receipt heap;if(!free.read_actual_receipt(heap,e))return false;
    if(heap.image!=before.image||heap.main_thread!=before.thread)
        return fail(e,"Loaded construction Free receipt image/Mainthread differs");
    std::vector<LocalJob> admitted;std::vector<Target> targets;std::set<std::string> keys;
    for(const auto& job:queue.jobs){
        // Other families keep their own admission. A construction occurrence
        // must name one exact current saved Building before it can be retained.
        if(job.type!=1)continue;
        LocalJob link;link.job=job.pointer;link.building=job.building;link.building_proxy=job.building_proxy;link.region=job.region;
        if(!link.building||!link.building_proxy||!link.region||!at(link.building,0x18,link.building_uid,e)||!link.building_uid)
            return fail(e,e.empty()?"Loaded construction Building/full UID/weak proxy/Region absent":e);
        if(!remove_diagnostics::resolve_current_building_key(snapshot,link.building,link.building_uid,link.building_key,e))return false;
        if(!keys.insert(link.building_key).second)return fail(e,"Loaded construction has two Jobs for one canonical Building");
        Target target;if(!building(before,link,target,e))return false;
        if(target.binding.snapshot_id!=snapshot||!occurrence(link,job,target.binding.object,target.region,copy,e))
            return fail(e,e.empty()?"Loaded construction current snapshot/occurrence differs":e);
        admitted.push_back(std::move(link));targets.push_back(std::move(target));
    }
    Capture after;job_census::Census repeated;
    if(!read_process(after,e)||!owners(before,after,e)||!read_jobs(after,repeated,e))return false;
    if(before.state!=after.state||before.local!=after.local||queue!=repeated)
        return fail(e,"Loaded construction complete pool/Job graph changed during admission");
    for(size_t index=0;index<admitted.size();++index){
        const auto& link=admitted[index];const auto& first=targets[index];Target second;
        if(!building(after,link,second,e))return false;
        const auto job=std::find_if(repeated.jobs.begin(),repeated.jobs.end(),[&](const auto& value){return value.pointer==link.job;});
        if(job==repeated.jobs.end()||second.binding.snapshot_id!=snapshot||
           second.binding.type!=first.binding.type||second.binding.variant!=first.binding.variant||
           second.active!=first.active||second.state!=first.state||
           !occurrence(link,*job,second.binding.object,second.region,copy,e))
            return fail(e,e.empty()?"Loaded construction target changed during admission":e);
    }
    std::sort(admitted.begin(),admitted.end(),[](const auto& a,const auto& b){return a.building_key<b.building_key;});
    out=std::move(admitted);e.clear();return true;
}
