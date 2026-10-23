namespace ticket=gm_ticket;namespace gn=gm_ticket::native;
bool gm_helper_code(Runtime& r,std::string& error){
    struct Code{uintptr_t rva;size_t bytes;const char* sha;};const Code codes[]{
#include "gm_ticket_pins.inc"
    };
    for(const auto& code:codes){std::vector<unsigned char> body(code.bytes);
        if(!r.memory.copy(r.image+code.rva,body.data(),body.size(),error)||mt2mp::sha256(body)!=code.sha)return fail(error,error.empty()?"GM original helper body differs RVA="+std::to_string(code.rva):error);}
    return true;
}
bool gm_world(Runtime& r,const a::Context& context,const a::Binding& binding,building_mixed_identity::opened_file::CurrentWorld& world,std::string& error){
    a::Context current;if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(current,error)||current!=context||
       !native_open_runtime::read_current_world(world,error)||world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)return fail(error,error.empty()?"GM current saved Session/Mainthread/NativeOrigin differs":error);
    if(r.gm_failed)return fail(error,r.gm_cause);return true;
}
bool gm_census(Runtime& r,const a::Context& context,const a::Binding& binding,const Local& local,ticket::Census& out,building_mixed_identity::opened_file::CurrentWorld& world,std::string& error){
    if(!gm_world(r,context,binding,world,error))return false;
    live::apply_timing::PageReadScope census_pages(!live::apply_timing::PageReadScope::reader_active());
    uintptr_t queue=0;if(!at(r,r.image,0x13df360,queue,error))return false;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){live::apply_timing::PageReadScope original_call(live::apply_timing::PageBoundary::OriginalCall);const auto exception=uid_lookup(r.image,uid,object);return !exception||fail(cause,"GM current FindUID SEH="+std::to_string(exception));};
    return ticket::read(read,find,r.image,queue,uintptr_t(world.map)+0x1c0,local.gm_rows,out,error);
}
bool gm_key(Runtime& r,const a::Context& context,const a::Binding& binding,const Local& local,uintptr_t object,std::string& out,std::string& error){
    out.clear();building_mixed_identity::opened_file::CurrentWorld world;if(!gm_world(r,context,binding,world,error))return false;
    for(size_t i=0;i<local.gm_rows.size();++i)if(local.gm_rows[i]==object){uint64_t uid=0;int32_t slot=-1;uintptr_t registered=0;
        if(!ap(r,object,0x16e5070,error)||!at(r,object,0x18,uid,error)||uid!=local.gm_uids[i]||!at(r,object,0x4ac,slot,error)||slot!=int32_t(i))return fail(error,error.empty()?"GM current external fixed-slot/full UID changed":error);
        const auto exception=uid_lookup(r.image,uid,registered);if(exception||registered!=object)return fail(error,"GM external fixed slot FindUID differs SEH="+std::to_string(exception));out="gm:"+std::to_string(i);return true;}
    MarketingMemory memory(r);ticket::reference::native::ReferenceWorld references{r.image,uintptr_t(world.state),uintptr_t(world.map)};
    uintptr_t table=0;if(!at(r,object,0,table,error))return false;
    const auto kind=table==r.image+0x1744e90?ticket::reference::native::ReferenceKind::SubscriberToon:ticket::reference::native::ReferenceKind::RootRegion;
    return ticket::reference::identify(memory,references,object,kind,out,error);
}
bool gm_resolve(Runtime& r,const a::Context& context,const a::Binding& binding,const Local& local,const std::string& key,uintptr_t& out,std::string& error){
    out=0;building_mixed_identity::opened_file::CurrentWorld world;if(!gm_world(r,context,binding,world,error))return false;
    if(key.starts_with("gm:")){int32_t slot=-1;if(!ticket::reference::id(key,"gm:",slot,error))return false;
        if(size_t(slot)>=local.gm_rows.size())return fail(error,"GM external fixed-slot key exceeds the current pool");out=local.gm_rows[size_t(slot)];std::string checked;
        if(!gm_key(r,context,binding,local,out,checked,error)||checked!=key)return fail(error,error.empty()?"GM external fixed slot does not roundtrip":error);return true;}
    MarketingMemory memory(r);ticket::reference::native::ReferenceWorld references{r.image,uintptr_t(world.state),uintptr_t(world.map)};
    const auto kind=key.starts_with("toon:")?ticket::reference::native::ReferenceKind::SubscriberToon:ticket::reference::native::ReferenceKind::RootRegion;
    return ticket::reference::resolve(memory,references,key,kind,out,error);
}
bool gm_capture(Runtime& r,const a::Context& context,const a::Binding& binding,const Local& local,ticket::Publication& out,std::string& error,ticket::Census* retained=nullptr){
    out={};ticket::Census census;building_mixed_identity::opened_file::CurrentWorld world;if(!gm_census(r,context,binding,local,census,world,error))return false;
    auto toon=[&](uintptr_t object,std::string& key,std::string& cause){return gm_key(r,context,binding,local,object,key,cause);};
    auto region=toon;if(!ticket::portable(census,toon,region,out.state,error))return false;
    MarketingMemory memory(r);auto key=[&](uintptr_t object,std::string& value,std::string& cause){return gm_key(r,context,binding,local,object,value,cause);};gn::CaptureReceipt receipt;
    if(!gn::capture(r.image,memory,census.queue,key,out.queue,receipt,error))return false;
    ticket::Census after;building_mixed_identity::opened_file::CurrentWorld checked;
    if(!gm_census(r,context,binding,local,after,checked,error)||after!=census||checked.map!=world.map||checked.state!=world.state)return fail(error,error.empty()?"GM original complete Record capture changed current queue/master values":error);
    if(retained)*retained=std::move(census);return ticket::valid(out,error);
}
bool gm_adopt(Runtime& r,const a::Context& context,const a::Binding& binding,const Local& local,const ticket::Census& before,const ticket::State& desired,
    const std::vector<uintptr_t>& fresh,const std::vector<uintptr_t>& previous,live::ManagementReceipt& receipt,std::string& error){
    if(before.tickets.members!=previous||desired.tickets.size()!=fresh.size()||before.masters.size()!=desired.masters.size())return fail(error,"GM complete before/fresh Ticket or fixed master extent differs");
    const auto store=[&](uintptr_t address,unsigned bytes,uint64_t expected,uint64_t target){if(expected==target)return true;bool stored=false;receipt.attempted=true;++receipt.attempted_stores;
        const auto exception=gn::compare(address,bytes,expected,target,stored);if(exception||!stored)return fail(error,"GM native field CAS differs bytes="+std::to_string(bytes)+" SEH="+std::to_string(exception));++receipt.stores;return true;};
    const auto floats=[&](uintptr_t address,const auto& old,const auto& target){for(size_t i=0;i<old.size();++i)if(!store(address+i*4,4,std::bit_cast<uint32_t>(old[i]),std::bit_cast<uint32_t>(target[i])))return false;return true;};
    building_mixed_identity::opened_file::CurrentWorld world;if(!gm_world(r,context,binding,world,error)||!gm_route_code(r,error))return false;
    for(size_t i=0;i<before.masters.size();++i){const auto& old=before.masters[i];const auto& target=desired.masters[i];
        if(old.slot!=target.slot||old.object!=local.gm_rows[i]||old.uid!=local.gm_uids[i]||!gm_hook_owner(r,old.object,error))return fail(error,error.empty()?"GM adoption exact current fixed-slot/full UID differs":error);
        bool changed_root=false;if(!old.root&&(!target.region.empty()||target.on_map||target.state||target.ticket>=0)){receipt.attempted=true;const auto exception=gn::shard(r.image,old.object,uintptr_t(world.map)+0x1c0);if(exception)return fail(error,"GM original SetShard SEH="+std::to_string(exception));changed_root=true;}
        if(old.on_map&&!target.on_map){receipt.attempted=true;const auto exception=gn::on_map(r.image,old.object,false);if(exception)return fail(error,"GM original off-map transition SEH="+std::to_string(exception));}
        if(changed_root||old.position!=target.position||old.orientation!=target.orientation||old.scale!=target.scale){receipt.attempted=true;const auto exception=gn::transform(r.image,old.object,target);if(exception)return fail(error,"GM original transform SEH="+std::to_string(exception));
            const auto spatial=gn::spatial(r.image,old.object);if(spatial)return fail(error,"GM original spatial update SEH="+std::to_string(spatial));uint8_t dirty=0;if(!at(r,old.object,0x480,dirty,error))return false;
            if(dirty>1)return fail(error,"GM native spatial dirty flag is outside Boolean domain");if(dirty&&!store(old.object+0x480,1,dirty,0))return false;}
        const auto target_ticket=target.ticket<0?uintptr_t(0):fresh[size_t(target.ticket)];
        if(!store(old.object+0x4b8,8,old.ticket,target_ticket)||!store(old.object+0x4b0,4,uint32_t(old.state),uint32_t(target.state))||
           !store(old.object+0x4c0,4,std::bit_cast<uint32_t>(old.timers[0]),std::bit_cast<uint32_t>(target.timers[0]))||
           !store(old.object+0x4c8,4,std::bit_cast<uint32_t>(old.timers[1]),std::bit_cast<uint32_t>(target.timers[1]))||
           !store(old.object+0x4cc,4,std::bit_cast<uint32_t>(old.timers[2]),std::bit_cast<uint32_t>(target.timers[2]))||
           !store(old.object+0x4c4,1,old.employed,uint8_t(target.employed))||!store(old.object+0x4c5,1,old.active,uint8_t(target.active))||
           !store(old.object+0x4d0,1,old.evidence,uint8_t(target.evidence))||!floats(old.arrow+0x1e8,old.arrow_target,target.arrow_target)||
           !store(old.arrow+0x250,1,old.arrow_has_target,uint8_t(target.arrow_has_target)))return false;
        if(!old.on_map&&target.on_map){receipt.attempted=true;const auto exception=gn::on_map(r.image,old.object,true);if(exception)return fail(error,"GM original on-map transition SEH="+std::to_string(exception));}
    }return true;
}
bool gm_apply(Runtime& r,const a::Context& context,const a::Binding& binding,const Local& local,const ticket::Publication& desired,live::ManagementReceipt& receipt,std::string& error){
    if(!ticket::valid(desired,error)||!gm_route_code(r,error))return false;
    ticket::Publication before;ticket::Census census;if(!gm_capture(r,context,binding,local,before,error,&census))return false;
    if(before==desired)return true;
    if(before.queue==desired.queue){if(!gm_adopt(r,context,binding,local,census,desired.state,census.tickets.members,census.tickets.members,receipt,error))return false;}
    else {MarketingMemory memory(r);gn::ApplyReceipt applied;auto key=[&](uintptr_t object,std::string& value,std::string& cause){return gm_key(r,context,binding,local,object,value,cause);};
        auto resolve=[&](const std::string& value,uintptr_t& object,std::string& cause){return gm_resolve(r,context,binding,local,value,object,cause);};
        auto current=[&](std::string& cause){building_mixed_identity::opened_file::CurrentWorld world;return gm_world(r,context,binding,world,cause);};
        auto adopt=[&](const std::vector<uintptr_t>& fresh,const std::vector<uintptr_t>& old,std::string& cause){return gm_adopt(r,context,binding,local,census,desired.state,fresh,old,receipt,cause);};
        const bool done=gn::apply(r.image,memory,census.queue,desired.queue,key,resolve,current,adopt,applied,error);
        receipt.attempted|=applied.load_attempted||applied.storage_transfer.attempted;receipt.attempted_stores+=applied.storage_transfer.attempted;receipt.stores+=applied.storage_transfer.stored;
        if(!done)return false;
    }
    ticket::Publication actual;if(!gm_capture(r,context,binding,local,actual,error)||actual!=desired)return fail(error,error.empty()?"GM whole original queue/master readback differs from Host":error);return true;
}
