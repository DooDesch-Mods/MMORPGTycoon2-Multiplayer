namespace {
namespace native_lookup=build_actions::private_native::host;
struct DistrictLookup {
    native_lookup::LookupApi api;
    bool initialize(std::string& error){
        native_lookup::ProcessMemory process;
        return native_lookup::resolve_lookup(process,image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,error);
    }
    bool operator()(uint64_t uid,uintptr_t& object,std::string& error){
        if(!uid||!api.find)return q::fail(error,"District full UID/exact native resolver unavailable");
        const auto exception=find_call(api.find,uid,object);
        return !exception||q::fail(error,"District full UID lookup exception="+std::to_string(exception));
    }
};
bool raw_districts(DistrictLookup& lookup,opened::CurrentWorld& world,std::map<int32_t,q::Pool>& out,std::string& error){
    if(!current(world,error))return false;
    const auto map=reinterpret_cast<uintptr_t>(world.map),root=map+0x1c0;
    auto read=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return q::at(memory,p,offset,value,cause);};
    region_saved_identity::Index regions;
    if(!region_saved_identity::capture(image,map,root,regions,read,error))return false;
    const std::map<int32_t,uintptr_t> region_pointers(regions.saved.begin(),regions.saved.end());
    std::map<int32_t,q::Pool> next;std::set<uintptr_t> objects;std::set<uint64_t> uids;
    const auto no_link=[](uintptr_t,int32_t& link,std::string&){link=0;return true;};
    for(const auto& [id,known]:admitted){
        uintptr_t region=0,manager=0,owner=0;
        const auto selected=region_pointers.find(id);
        if(selected==region_pointers.end())return q::fail(error,"District current saved Region ID absent");
        region=selected->second;
        if(!q::at(memory,region,0x970,manager,error)||!q::at(memory,manager,0x60,owner,error))return false;
        if(manager!=known.manager||owner!=region)
            return q::fail(error,"District current reciprocal manager/Region differs");
        q::Pool pool;if(!q::observe(memory,image,manager,no_link,pool,error))return false;
        for(const auto& row:pool.records){uintptr_t actual_root=0,actual_region=0,found=0;
            if(!q::at(memory,row.pointer,0x170,actual_root,error)||!q::at(memory,row.pointer,0x178,actual_region,error)||
               !lookup(row.uid,found,error))return false;
            if(actual_root!=root||actual_region!=region||found!=row.pointer||
               !objects.insert(row.pointer).second||!uids.insert(row.uid).second)
                return q::fail(error,"District current member root/Region/full UID/unique ownership differs");
        }
        next.emplace(id,std::move(pool));
    }
    if(next.size()!=regions.saved.size())return q::fail(error,"District current Region roster differs from bound complete owners");
    opened::CurrentWorld after;
    if(!region_saved_identity::verify(image,map,root,regions,read,error)||!current(after,error)||!same_world(world,after))
        return q::fail(error,error.empty()?"District World/Region roster changed during complete read":error);
    out=std::move(next);return true;
}
bool admitted_rows(DistrictLookup& lookup,const std::map<int32_t,q::Pool>& raw,std::vector<Current>& out,std::string& error){
    std::vector<Current> next;std::set<std::string> keys;
    for(const auto& [id,pool]:raw){
        const auto known=admitted.find(id);std::vector<Current> rows;
        if(known==admitted.end())return q::fail(error,"District current Region has no confirmed identities");
        if(!district_lifecycle::current(known->second,pool,lookup,rows,error))return false;
        for(const auto& row:rows)if(!keys.insert(row.key).second)return q::fail(error,"District current canonical identity repeats across Regions");
        next.insert(next.end(),rows.begin(),rows.end());
    }
    out=std::move(next);return true;
}
bool edit_owner(const Edit& edit,std::string& error){
    opened::CurrentWorld world;
    if(!current(world,error))return false;
    if(!active_edit||*active_edit!=edit||edit.snapshot!=uuid||edit.before!=admitted)
        return q::fail(error,"District original edit receipt/current bound aliases differ");
    return true;
}
}
bool prepare_edit(const std::string& operation,Edit& out,std::string& error){
    live_host_state::apply_timing::PageReadScope pages;
    std::lock_guard lock(gate);out={};
    if(!district_lifecycle::uuid(operation)||active_edit||attempted_edits.contains(operation)||next_edit==UINT64_MAX)
        return q::fail(error,"District original edit UUID/busy/attempted/token guard differs");
    DistrictLookup lookup;if(!lookup.initialize(error))return false;
    opened::CurrentWorld world;std::map<int32_t,q::Pool> raw;std::vector<Current> checked;
    if(!raw_districts(lookup,world,raw,error)||!admitted_rows(lookup,raw,checked,error))return false;
    Edit value{++next_edit,operation,uuid,admitted};
    attempted_edits.insert(operation);active_edit=value;out=std::move(value);return true;
}
bool preview_edit(const Edit& edit,const std::map<int32_t,std::map<uint64_t,std::string>>& restored,Preview& out,std::string& error){
    live_host_state::apply_timing::PageReadScope pages;
    std::lock_guard lock(gate);out={};
    if(!edit_owner(edit,error))return false;
    if(active_preview)return q::fail(error,"District original edit already has an unpublished preview");
    DistrictLookup lookup;if(!lookup.initialize(error))return false;
    opened::CurrentWorld world;std::map<int32_t,q::Pool> raw;
    if(!raw_districts(lookup,world,raw,error))return false;
    for(const auto& [id,rows]:restored){(void)rows;if(!edit.before.contains(id))return q::fail(error,"District original restoration names a foreign Region");}
    Preview value;value.edit=edit;size_t next_ordinal=0;std::set<std::string> keys;
    const std::map<uint64_t,std::string> empty;
    for(const auto& [id,pool]:raw){
        district_lifecycle::Change change;const auto restore=restored.find(id);
        if(!district_lifecycle::reconcile(edit.before.at(id),pool,edit.operation,restore==restored.end()?empty:restore->second,
            lookup,change,error,&next_ordinal))return false;
        for(const auto& member:change.after.members)if(!keys.insert(member.key).second)
            return q::fail(error,"District original complete output repeats a canonical identity");
        value.changes.emplace(id,std::move(change));
    }
    active_preview=value;out=std::move(value);return true;
}
bool publish_edit(const Preview& value,std::string& error){
    live_host_state::apply_timing::PageReadScope pages;
    std::lock_guard lock(gate);
    if(!edit_owner(value.edit,error))return false;
    if(!active_preview||*active_preview!=value)return q::fail(error,"District original publication lacks its exact preview");
    DistrictLookup lookup;if(!lookup.initialize(error))return false;
    opened::CurrentWorld world;std::map<int32_t,q::Pool> raw;
    if(!raw_districts(lookup,world,raw,error))return false;
    std::map<int32_t,district_lifecycle::Roster> next;
    for(const auto& [id,pool]:raw){
        const auto found=value.changes.find(id);std::vector<Current> checked;
        if(found==value.changes.end()||!district_lifecycle::current(found->second.after,pool,lookup,checked,error))
            return q::fail(error,error.empty()?"District native membership changed after effect validation":error);
        if(pool.records.size()!=found->second.after.members.size())return q::fail(error,"District publication count changed after preview");
        for(size_t i=0;i<pool.records.size();++i)if(pool.records[i]!=found->second.after.members[i].native)
            return q::fail(error,"District original current order/full member changed after preview");
        next.emplace(id,found->second.after);
    }
    if(next.size()!=value.changes.size())return q::fail(error,"District publication complete Region count differs");
    admitted=std::move(next);active_preview.reset();active_edit.reset();return true;
}
bool cancel_edit(const Edit& edit,std::string& error){
    live_host_state::apply_timing::PageReadScope pages;
    std::lock_guard lock(gate);
    if(!edit_owner(edit,error))return false;
    DistrictLookup lookup;if(!lookup.initialize(error))return false;
    opened::CurrentWorld world;std::map<int32_t,q::Pool> raw;std::vector<Current> checked;
    if(!raw_districts(lookup,world,raw,error)||!admitted_rows(lookup,raw,checked,error))return false;
    active_preview.reset();active_edit.reset();return true;
}
