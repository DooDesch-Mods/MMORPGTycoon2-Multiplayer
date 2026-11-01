namespace world_path_runtime {
struct WallShardNative {
    region::Runtime& runtime;
    bool verify(std::string& error){
        constexpr wall_private_native::Code codes[]={
            {0x4cf6e0,16,"1352ba185c1aadfff33882b525618d08126efe44082443f61bec24ef1b8f35b1"},
            {0x4d0230,272,"b59d706d6dd8524b9bad98bcd2a7640a384f5364ca085a44fc81ac6bc0eec831"},
            {0x557cc0,480,"22c76f4cf31a025933fc70dc9d2b90d1bcd3271806b0de07bf9e0c6bdee80232"}};
        for(const auto& pin:codes){
            std::string bytes(pin.size,'\0');
            if(!runtime.memory.copy(runtime.image+pin.rva,bytes.data(),bytes.size(),error)||
               !hooks::framework::verify_original_hash(runtime.image+pin.rva,bytes.data(),bytes.size(),pin.sha256,error))
                return wall_history_owner::reject(error,"original body differs RVA="+std::to_string(pin.rva)+" original="+error);
        }
        return true;
    }
    bool read(uintptr_t path,uintptr_t& shard,std::string& error){
        uintptr_t ap=0,setter=0;
        if(!region::at(runtime,path,0,ap,error)||ap!=runtime.image+0x1743f10||
           !region::at(runtime,ap,0x190,setter,error)||setter!=runtime.image+0x4cf6e0)
            return wall_history_owner::reject(error,"Path address point or original SetShard slot differs: "+error);
        return region::at(runtime,path,0x170,shard,error);
    }
    static uint32_t call(uintptr_t image,uintptr_t path,uintptr_t shard){
        __try{reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x4cf6e0)(path,shard);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    }
    bool set(uintptr_t path,uintptr_t shard,std::string& error){
        return native_error(call(runtime.image,path,shard),"Wall Redo SetShard",error);
    }
};
bool source_wall_paths(const std::string& source,const mixed_path_native::Census& census,
                       std::map<std::string,uintptr_t>& out,std::string& error){
    for(const auto& path:census.paths){
        const auto found=owner.keys.keys().find(path.member.uid);
        if(found==owner.keys.keys().end())return wall_history_owner::reject(error,"native Path lacks restored canonical key");
        if(path.member.ref.type==5&&found->second.starts_with("action:"+source+":path:"))
            if(!out.emplace(found->second,path.member.object).second)return wall_history_owner::reject(error,"source Wall key repeated");
    }
    return true;
}
bool capture_wall_owners(region::Runtime& runtime,uintptr_t root,const std::string& source,const Sample& sample,
                          wall_history_owner::Receipt& receipt,std::string& error){
    std::map<std::string,uintptr_t> paths;WallShardNative native{runtime};
    return source_wall_paths(source,sample.mixed.native,paths,error)&&wall_history_owner::capture(native,root,paths,receipt,error);
}
bool restore_wall_owners(region::Runtime& runtime,uintptr_t root,const std::string& source,const Sample& expected,
                          const mixed_path_native::Census& census,const wall_history_owner::Receipt& receipt,std::string& error){
    mixed_path_graph::Snapshot graph;json actual,original;
    if(!mixed_path_graph::build(census,owner.keys.keys(),graph,error)||
       !mixed_path_graph::rebase(graph,expected.mixed.graph,actual,error)||
       !mixed_path_graph::rebase(expected.mixed.graph,expected.mixed.graph,original,error))return false;
    if(actual!=original)return wall_history_owner::reject(error,"complete restored topology differs before SetShard");
    std::map<std::string,uintptr_t> paths;WallShardNative native{runtime};
    return source_wall_paths(source,census,paths,error)&&wall_history_owner::restore(native,root,paths,receipt,error);
}
}
