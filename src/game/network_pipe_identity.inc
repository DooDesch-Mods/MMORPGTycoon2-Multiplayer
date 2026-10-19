namespace objects=network_object_identity;
namespace aliases=network_snapshot_alias;
objects::Registry object_registry;
aliases::Manifest prepared_aliases,captured_aliases;
std::string prepared_alias_uuid,prepared_alias_sha,captured_alias_uuid,captured_alias_sha;

bool object_scope(const Roots& roots,objects::Scope& out,std::string& e){
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e))return false;
    if(world.map!=roots.map||reinterpret_cast<uintptr_t>(world.map)+0x1c0!=reinterpret_cast<uintptr_t>(roots.shard)){
        e="Network identity NativeOrigin/current root differs";return false;
    }
    out={world.uuid,world.world_epoch,world.generation,reinterpret_cast<uintptr_t>(roots.shard)};
    if(!objects::valid(out)){e="Network identity complete NativeOrigin missing";return false;}return true;
}
bool manager_pipes(const void* manager,std::vector<void*>& out,std::string& cause){
    out.clear();void** data=nullptr;int32_t count=-1,capacity=-1;
    if(!vtable(manager,manager_vtable)||!read_at(manager,0x50,data)||!read_at(manager,0x58,count)||!read_at(manager,0x5c,capacity)||
       count<0||count>int32_t(maximum_nodes)||capacity<count||(count&&!readable(data,size_t(count)*8))){cause="Network Pipe manager array bounds differ";return false;}
    std::set<void*> unique;for(int32_t n=0;n<count;++n){void* object=nullptr;if(!read_at(data,size_t(n)*8,object)||!object||!unique.insert(object).second){cause="Network Pipe manager array null/duplicate";return false;}out.push_back(object);}return true;
}
bool pipe_read(void* object,const Roots& roots,Binding& row,std::string& cause){
    row={};row.node=object;
    if(!vtable(object,0x16fca60)||!read_at(object,0x18,row.uid)||!row.uid||!read_at(object,0x178,row.region)||!read_at(object,0x170,row.shard)||
       row.shard!=roots.shard||std::find(roots.regions.begin(),roots.regions.end(),row.region)==roots.regions.end()||!vtable(row.region,region_vtable)||
       !read_at(row.region,0x834,row.region_id)||row.region_id<0||!read_at(row.region,0x978,row.manager)||!vtable(row.manager,manager_vtable)){
        cause="Network Pipe current class/full UID/Region/root differs";return false;
    }
    void* owner=nullptr;if(!read_at(row.manager,0x168,owner)||owner!=row.region){cause="Network Pipe manager owner differs";return false;}
    std::vector<void*> pipes;if(!manager_pipes(row.manager,pipes,cause))return false;
    if(std::count(pipes.begin(),pipes.end(),object)!=1){cause="Network Pipe current manager membership differs";return false;}return true;
}
bool pipe_census(const Roots& roots,std::map<uint64_t,Binding>& out,std::string& cause){
    out.clear();std::set<void*> pointers;for(auto* region:roots.regions){void* manager=nullptr;if(!read_at(region,0x978,manager)){cause="Network Pipe Region manager unreadable";return false;}if(!manager)continue;
        std::vector<void*> pipes;if(!manager_pipes(manager,pipes,cause))return false;
        for(auto* object:pipes){Binding row;if(!pipe_read(object,roots,row,cause)||row.region!=region)return false;
            if(!pointers.insert(object).second||!out.emplace(row.uid,row).second){cause="Network Pipe census duplicate full UID/pointer";return false;}}
    }return true;
}
bool capture_pipe_ids(Shared& s,Pending& pending,std::string& cause){
    std::vector<void*> pipes;if(!manager_pipes(pending.manager,pipes,cause))return false;
    for(void* object:pipes){Binding row;bool present=false,shard_present=false;
        if(!pipe_read(object,s.roots,row,cause)||row.manager!=pending.manager){if(cause.empty())cause="Network Pipe save property manager differs";return false;}
        const auto seh=contains_native(pending.context,object,row.id,present),shard_seh=contains_native(pending.context,row.shard,row.shard_id,shard_present);
        if(seh||shard_seh||!present||!shard_present||row.id<=0||row.shard_id<=0){cause="Network Pipe original SaveContext IDs missing SEH="+std::to_string(seh?seh:shard_seh);return false;}
        row.context=pending.context;pending.pipes.push_back(row);
    }return true;
}
bool finish_pipe_ids(Shared& s,const Pending& pending,std::string& cause){
    std::vector<void*> pipes;if(!manager_pipes(pending.manager,pipes,cause)||pipes.size()!=pending.pipes.size()){if(cause.empty())cause="Network Pipe count changed during original Node SaveStream";return false;}
    for(size_t n=0;n<pipes.size();++n){const auto& old=pending.pipes[n];Binding current;bool present=false;int32_t id=0;
        if(pipes[n]!=old.node||!pipe_read(pipes[n],s.roots,current,cause)||current.uid!=old.uid||current.region!=old.region||current.manager!=old.manager||current.shard!=old.shard){if(cause.empty())cause="Network Pipe current membership changed during original SaveStream";return false;}
        const auto seh=contains_native(pending.context,pipes[n],id,present);
        if(seh||!present||id!=old.id||!s.pipe_bindings.emplace(id,old).second){cause="Network Pipe original ID changed or duplicated SEH="+std::to_string(seh);return false;}
    }return true;
}
objects::Entry object_entry(objects::Kind kind,const Binding& row){return {kind,reinterpret_cast<uintptr_t>(row.node),reinterpret_cast<uintptr_t>(row.region),reinterpret_cast<uintptr_t>(row.manager),row.uid,row.region_id,false};}
bool publish_objects(Shared& s,const Roots& roots,std::vector<uint8_t>& digest,std::string& error){
    objects::Scope scope;if(!object_scope(roots,scope,error))return false;
    std::map<uint64_t,Binding> live;if(!pipe_census(roots,live,error))return false;
    if(live.size()!=s.pipe_bindings.size()){error="Network Pipe complete saved/current census count differs";return false;}
    for(auto& [id,row]:s.pipe_bindings){const auto found=live.find(row.uid);void* exact=nullptr;const auto seh=lookup_native(row.uid,exact);
        if(id<=0||found==live.end()||found->second.node!=row.node||seh||exact!=row.node){error="Network Pipe sealed ID/FindUID/membership differs SEH="+std::to_string(seh);return false;}
        if(s.role=="host"&&(row.manager!=found->second.manager||row.region!=found->second.region||row.shard!=found->second.shard)){error="Network Pipe saved owner changed";return false;}
        const auto context=row.context;const auto saved_shard=row.shard_id;row=found->second;row.id=id;row.context=context;row.shard_id=saved_shard;
        if(s.role=="client"){for(const auto& [key,object]:s.shards)if(key.first==context&&object==roots.shard){if(row.shard_id&&row.shard_id!=key.second){error="Network Pipe root shard link ambiguous";return false;}row.shard_id=key.second;}
            if(row.shard_id<=0){error="Network Pipe root shard link missing";return false;}}
    }
    std::vector<std::pair<std::string,objects::Entry>> previous;std::set<std::string> retired;std::string absent;
    const bool had_registry=object_registry.enumerate(scope,previous,retired,absent);
    const bool load=s.role=="client";
    if(load&&(prepared_alias_uuid!=s.uuid||prepared_alias_sha!=s.sha)){error="Network load lacks exact prepared companion alias decision";return false;}
    aliases::Manifest result;result.present=true;if(load)result.retired=prepared_aliases.retired;else if(had_registry)result.retired=retired;
    std::vector<std::pair<std::string,objects::Entry>> entries;size_t adopted=0;
    for(auto kind:{objects::Kind::Node,objects::Kind::Pipe}){
        auto& rows=kind==objects::Kind::Node?s.bindings:s.pipe_bindings;
        for(const auto& [id,row]:rows){const auto entry=object_entry(kind,row);std::string key;
            if(load&&prepared_aliases.present){for(const auto& alias:prepared_aliases.rows)if(alias.kind==kind&&alias.link==id){if(!key.empty()){error="Network loaded alias link ambiguous";return false;}key=alias.key;++adopted;}
                if(key.empty()){error="Network loaded object missing promised canonical alias";return false;}}
            else if(!load&&had_registry){for(const auto& [old,local]:previous)if(local.kind==kind&&local.pointer==entry.pointer&&local.uid==entry.uid){key=old;break;}}
            if(key.empty())key=objects::saved(s.sha,kind,id);
            result.rows.push_back({kind,id,key});entries.emplace_back(key,entry);
            const auto value=std::string(objects::name(kind))+":"+std::to_string(id)+":"+std::to_string(row.shard_id)+":"+std::to_string(row.region_id)+":"+key+"\n";digest.insert(digest.end(),value.begin(),value.end());
        }
    }
    if(load&&prepared_aliases.present&&adopted!=prepared_aliases.rows.size()){error="Network companion has extra unadopted aliases";return false;}
    if(!aliases::valid(result,error)||!object_registry.seal(scope,entries,result.retired,error))return false;
    captured_aliases=std::move(result);captured_alias_uuid=s.uuid;captured_alias_sha=s.sha;return true;
}
