bool read_metadata_prop(host::ProcessMemory& memory,host::Reader& reader,uintptr_t image,
    const host::WorldState& world,void* object,uint64_t uid,prop_lease::Prop& out,std::string& error){
    uintptr_t addresspoint=0;if(!memory.copy(uintptr_t(object),&addresspoint,sizeof addresspoint,error))return false;
    if(addresspoint!=image+0x16c1390)return reader.prop(object,uid,out,error);
    uintptr_t shard=0,region=0;
    if(!memory.copy(uintptr_t(object)+0x170,&shard,sizeof shard,error)||!memory.copy(uintptr_t(object)+0x178,&region,sizeof region,error))return false;
    if(shard!=world.root)return reader.prop(object,uid,out,error);
    int32_t region_id=-1;if(!memory.copy(region+0x834,&region_id,sizeof region_id,error))return false;
    return reader.saved_root_scenery(object,uid,region_id,region,out,error);
}
