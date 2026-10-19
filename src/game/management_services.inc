bool service_operation(const a::ManagementIntent& intent){return intent.op==a::ManagementOp::SetServicePrice||intent.op==a::ManagementOp::SetServiceSync;}
bool service_array(Runtime& r,uintptr_t owner,size_t offset,size_t maximum,std::vector<uintptr_t>& rows,std::string& e){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(r,owner,offset+8,data,e)||!at(r,owner,offset+0x10,count,e)||!at(r,owner,offset+0x14,capacity,e))return false;
    if(count<0||size_t(count)>maximum||capacity<count||capacity>1000000||(!data&&count))return fail(e,"Management service/config array bounds invalid");
    rows.resize(size_t(count));if(count&&!r.memory.copy(data,rows.data(),rows.size()*sizeof(uintptr_t),e))return false;
    const std::set<uintptr_t> unique(rows.begin(),rows.end());return unique.size()==rows.size()&&!unique.contains(0)||fail(e,"Management service/config native owner duplicate/null");
}
bool read_services(Runtime& r,const a::Context& context,const a::Binding& binding,const a::ManagementIntent& intent,Local& local,a::ManagementState& state,std::string& e){
    if(!service_operation(intent))return true;
    remove_diagnostics::LocalTarget target;
    std::vector<remove_diagnostics::LocalTarget> census;if(!remove_diagnostics::identity_targets(census,e))return false;
    const auto found=std::find_if(census.begin(),census.end(),[&](const auto& value){return value.record_key==intent.key;});
    if(found==census.end())return fail(e,"Management service canonical Building missing from complete current census");target=*found;
    if(!target.normal_building||
       target.world_epoch!=binding.native_epoch||target.world_generation!=binding.native_generation)return fail(e,e.empty()?"Management service requires exact current canonical Building/full UID":e);
    local.service_building=target.object;local.service_uid=target.uid;
    a::ManagementServices services;services.building_key=intent.key;
    if(!service_array(r,target.object,0x9d0,1024,local.service_rows,e)||size_t(intent.service_ordinal)>=local.service_rows.size())return fail(e,e.empty()?"Management requested service slot absent":e);
    local.service_object=local.service_rows[size_t(intent.service_ordinal)];
    uintptr_t manager=0;if(!at(r,r.image,0x13df840,manager,e))return false;services.config_present=manager!=0;
    std::vector<uintptr_t> configs;
    if(manager&&(!ap(r,manager,0x173fba0,e)||!service_array(r,manager,0x18,4096,configs,e)))return false;
    for(const auto config:configs){a::ManagementServiceConfig row;
        if(!ap(r,config,0x1736660,e)||!text(r,config+0x18,row.key,256,e)||!ap(r,config+0x38,0x174ab50,e)||!at(r,config,0x50,row.price,e))return false;
        services.configs.push_back(std::move(row));
    }
    for(const auto service:local.service_rows){a::ManagementService row;uintptr_t building=0,prototype=0,config=0;uint8_t sync=2,allow=1;
        if(!ap(r,service,0x1718480,e)||!at(r,service,0x148,building,e)||building!=target.object||!at(r,service,0x20,row.type,e)||
           !text(r,service+0x158,row.config_key,256,e)||!ap(r,service+0x28,0x174ab50,e)||!at(r,service,0x40,row.price,e)||
           !at(r,service,0x48,sync,e)||sync>1||!at(r,service,0x178,prototype,e)||!at(r,service,0x150,config,e))return fail(e,e.empty()?"Management service class/Building owner/Price/sync differs":e);
        if(prototype&&(!ap(r,prototype,0x173be20,e)||!at(r,prototype,0x41,allow,e)||allow>1))return fail(e,e.empty()?"Management actual service prototype sync policy invalid":e);
        if(config){auto found=std::find(configs.begin(),configs.end(),config);
            if(found==configs.end()||services.configs[size_t(found-configs.begin())].key!=row.config_key)return fail(e,"Management service config pointer/key not in actual current manager");}
        row.synced=sync!=0;row.allow_sync=allow!=0;services.rows.push_back(std::move(row));
    }
    const auto& selected=services.rows[size_t(intent.service_ordinal)];
    if(services.config_present&&(selected.synced||(intent.op==a::ManagementOp::SetServiceSync&&intent.value&&selected.allow_sync))&&selected.config_key.empty())return fail(e,"Management original config resolution requires nonempty native service key");
    state.services=std::move(services);return true;
}
void service_capture(Runtime& r,void* receiver,a::ManagementIntent intent){
    std::string e;uintptr_t building=0,selection=0;
    if(!ap(r,uintptr_t(receiver),0x1718480,e)||!at(r,uintptr_t(receiver),0x148,building,e)||!at(r,r.image,0x13df630,selection,e)){reject(r,e);return;}
    remove_diagnostics::LocalTarget target;if(!remove_diagnostics::resolve_local_target(selection,building,target,e)||!target.normal_building){reject(r,e.empty()?"Management service source Building identity unavailable":e);return;}
    std::vector<uintptr_t> rows;if(!service_array(r,building,0x9d0,1024,rows,e)){reject(r,e);return;}
    const auto found=std::find(rows.begin(),rows.end(),uintptr_t(receiver));if(found==rows.end()){reject(r,"Management native service receiver not in current Building array");return;}
    intent.key=target.record_key;intent.service_ordinal=int32_t(found-rows.begin());capture(r,receiver,intent,uintptr_t(receiver));
}
void service_price_entry(void* receiver,const void* price){auto* r=published.load(std::memory_order_acquire);
    if(r->replay||!net::session_wire::connected()||uintptr_t(_ReturnAddress())!=r->image+0x66dc1e){r->service_price(receiver,price);return;}
    std::string e;std::array<int32_t,2> value;
    if(!ap(*r,uintptr_t(price),0x174ab50,e)||!at(*r,uintptr_t(price),0x18,value,e)){reject(*r,e);return;}
    a::ManagementIntent intent;intent.op=a::ManagementOp::SetServicePrice;intent.currency=value[0];intent.value=value[1];service_capture(*r,receiver,intent);
}
void service_sync_entry(void* receiver,bool sync){auto* r=published.load(std::memory_order_acquire);
    if(r->replay||!net::session_wire::connected()||uintptr_t(_ReturnAddress())!=r->image+0x66dcbc){r->service_sync(receiver,sync);return;}
    a::ManagementIntent intent;intent.op=a::ManagementOp::SetServiceSync;intent.value=sync?1:0;service_capture(*r,receiver,intent);
}
