struct PartyToonBatch {
    Work world;
    bool held=false,used=false,failed=false,pending_visible=false;
    std::vector<PendingToon> pending;
};
namespace {
struct PartyToonRow {
    Work world;
    std::unique_ptr<Resources> resources=std::make_unique<Resources>();
    local::CallReceipt construct,load,next,resolve,login;
    local::LookupReceipt load_lookup,resolve_lookup;
    int32_t saved_party_id=0;
    bool party_clear_attempted=false,party_clear_returned=false;
    bool party_rearm_attempted=false,party_rearm_returned=false;
};
bool main_toon_party(const stage::Payload& payload,int32_t& out,std::string& e){
    out=0;size_t toon=SIZE_MAX;size_t toons=0,properties=0;
    for(size_t i=0;i<payload.records.size();++i){const auto& row=payload.records[i];
        if(row.label.text=="mmoToon"&&row.parent<payload.records.size()&&
           payload.records[row.parent].label.text=="main"&&payload.records[row.parent].parent==0){toon=i;++toons;}
    }
    if(toons!=1)return fail(e,"Party/Toon original main Toon Record is not unique");
    for(const auto& row:payload.records)if(row.parent==toon&&row.label.text=="party"){
        if(++properties!=1||row.tokens.size()!=1||row.tokens[0].type!=4)return fail(e,"Party/Toon saved party property is not one original integer");
        out=std::bit_cast<int32_t>(row.tokens[0].bits);
    }
    if(out<=0)return true;
    const auto found=std::find_if(payload.external.begin(),payload.external.end(),[&](const auto& ref){return ref.local_link_id==out;});
    return (found!=payload.external.end()&&found->key.kind==stage::ReferenceKind::SharedParty)||
        fail(e,"Party/Toon main saved Party link lacks its canonical external owner");
}
bool pending_array(PartyToonBatch& batch,std::vector<PendingToon>& out,std::string& e){
    if(!verify_party_toon_edits_held(batch,e))return false;
    const auto manager=batch.world.original.manager;
    uintptr_t data=0;int32_t count=-1,capacity=-1;
    if(!at(manager,0x40,data,e)||!at(manager,0x48,count,e)||!at(manager,0x4c,capacity,e))return false;
    const auto& before=batch.world.original.census.members;
    if(count<0||capacity<count||(count&&!data)||size_t(count)<before.size()||size_t(count)>(UINTPTR_MAX-data)/8)
        return fail(e,"Party/Toon pending Subscriber array extent differs");
    std::set<uintptr_t> pointers,toons;std::set<int32_t> ids;out.clear();
    for(const auto& member:before)if(member.toon)toons.insert(member.toon);
    for(int32_t i=0;i<count;++i){
        PendingToon row;uintptr_t ap=0;
        if(!at(data,size_t(i)*8,row.subscriber,e)||!at(row.subscriber,0,ap,e)||ap!=image_base+0x16e75f0||
           !at(row.subscriber,0x20,row.subscriber_id,e))return fail(e,e.empty()?"Party/Toon pending Subscriber class differs":e);
        if(row.subscriber_id<0||!pointers.insert(row.subscriber).second||!ids.insert(row.subscriber_id).second)
            return fail(e,"Party/Toon pending Subscriber identity duplicated");
        if(size_t(i)<before.size()){
            if(row.subscriber!=before[i].subscriber||row.subscriber_id!=before[i].id)
                return fail(e,"Party/Toon load changed existing Subscriber array order/identity");
            continue;
        }
        uintptr_t registered=0;
        if(!at(row.subscriber,0x80,row.toon,e)||!at(row.toon,0,ap,e)||ap!=image_base+0x1744e90||
           !at(row.toon,0x18,row.uid,e)||!row.uid||!stage::registry_lookup(image_base,row.uid,registered,e)||registered!=row.toon)
            return fail(e,e.empty()?"Party/Toon pending Toon actual class/full UID differs":e);
        if(!toons.insert(row.toon).second)return fail(e,"Party/Toon new constructor result reuses an existing Toon");
        out.push_back(row);
    }
    uintptr_t after_data=0;int32_t after_count=-1,after_capacity=-1;
    if(!at(manager,0x40,after_data,e)||!at(manager,0x48,after_count,e)||!at(manager,0x4c,after_capacity,e))return false;
    return (data==after_data&&count==after_count&&capacity==after_capacity)||fail(e,"Party/Toon pending array changed during readback");
}
struct DeferredLinks {
    PartyToonBatch& batch;
    Work& row;
    static bool fresh(void* user,const local::ExternalLink& link,std::string& e){
        auto& self=*static_cast<DeferredLinks*>(user);
        if(!verify_party_toon_edits_held(self.batch,e))return false;
        for(size_t i=0;i<self.row.links.size();++i)if(self.row.links[i].saved_link_id==link.saved_link_id){
            const auto& key=self.row.keys[i].first;
            if(key.kind==stage::ReferenceKind::SubscriberToon){
                const auto found=std::find_if(self.batch.pending.begin(),self.batch.pending.end(),[&](const auto& row){return row.subscriber_id==key.subscriber_id;});
                if(found!=self.batch.pending.end()){
                    PendingToon current;
                    return pending_toon(self.batch,key.subscriber_id,current,e)&&
                        (current.toon==link.object||fail(e,"Party/Toon pending external Toon lifetime changed"));
                }
            }
            return fresh_link(&self.row,link,e);
        }
        return fail(e,"Party/Toon undeclared deferred external link");
    }
};
bool deferred_links(PartyToonBatch& batch,Work& row,std::string& e){
    const stage::ReferenceWorld world{image_base,reinterpret_cast<uintptr_t>(row.held.origin.state),
        reinterpret_cast<uintptr_t>(row.held.origin.map),&row.building_pins,&row.npc_pins,row.parties};
    for(const auto& ref:row.payload.external){uintptr_t object=0;
        const auto pending=std::find_if(batch.pending.begin(),batch.pending.end(),[&](const auto& item){
            return ref.key.kind==stage::ReferenceKind::SubscriberToon&&item.subscriber_id==ref.key.subscriber_id;});
        if(pending!=batch.pending.end()){PendingToon current;if(!pending_toon(batch,pending->subscriber_id,current,e))return false;object=current.toon;}
        else if(!stage::resolve_reference(memory,world,ref.key,object,e))return false;
        // Saved IDs are unique within this original context and do not identify
        // another context's external keys, even when both saves start at ID 1.
        row.links.push_back({ref.local_link_id,"deferred:"+std::to_string(ref.local_link_id),object});
        row.keys.push_back({ref.key,object});
    }
    return true;
}
}
struct PartyToonInputs {
    FrameBinding frame;
    std::string origin;
    stage::ClientWorld original;
    int32_t next_id=-1;
    bool used=false;
    std::vector<std::unique_ptr<PartyToonRow>> rows;
};
bool prepare_party_toons(const std::vector<Json>& graphs,const std::string& origin,const FrameBinding& frame,
    std::shared_ptr<PartyToonInputs>& out,std::string& e){
    out.reset();e.clear();
    if(graphs.empty())return fail(e,"Party/Toon preparation has no new Subscribers");
    if(busy.test_and_set())return fail(e,"Party/Toon preparation shared native scope busy");
    struct Release {~Release(){busy.clear();}} release;
    try{
        if(!hook_ready||!image_base)return fail(e,"Party/Toon preparation original lookup hook is not ready");
        auto prepared=std::make_shared<PartyToonInputs>();prepared->frame=frame;prepared->origin=origin;
        Work admission;admission.origin=origin;admission.local_sha=frame.local_sha;admission.frame=&prepared->frame;
        if(!read_world(admission,false,prepared->original,e))return false;
        if(prepared->original.image!=image_base)return fail(e,"Party/Toon preparation native image differs");
        if(blocked_epoch==prepared->original.origin.world_epoch&&blocked_generation==prepared->original.origin.generation)
            return fail(e,"Party/Toon previous outcome unknown: "+blocked_cause);
        std::set<int32_t> ids;for(const auto& old:prepared->original.census.members)ids.insert(old.id);
        for(const auto& graph:graphs){auto row=std::make_unique<PartyToonRow>();auto& w=row->world;
            w.origin=origin;w.local_sha=frame.local_sha;w.frame=&prepared->frame;w.original=prepared->original;
            if(!stage::parse_payload(graph,w.payload,e)||!local::additions::frame_binding(graph,frame.context.session,frame.context.snapshot_uuid,frame.common_epoch,e)||
               !main_toon_party(w.payload,row->saved_party_id,e))return false;
            if(!w.payload.online||!ids.insert(w.payload.id).second||w.payload.next_id<prepared->original.census.next_id||
               (prepared->next_id>=0&&prepared->next_id!=w.payload.next_id))return fail(e,"Party/Toon new Subscriber ID/online/nextID differs");
            prepared->next_id=w.payload.next_id;
            if(std::any_of(w.payload.external.begin(),w.payload.external.end(),[](const auto& r){return r.key.kind==stage::ReferenceKind::SharedBuilding;})&&
               !w.building_pins.prepare(image_base,w.original.origin,e))return false;
            if(std::any_of(w.payload.external.begin(),w.payload.external.end(),[](const auto& r){return r.key.kind==stage::ReferenceKind::SharedNPC||r.key.kind==stage::ReferenceKind::SharedQuest;})&&
               !w.npc_pins.prepare(image_base,w.original.origin,e))return false;
            prepared->rows.push_back(std::move(row));
        }
        stage::ClientWorld current;
        if(!read_world(admission,false,current,e)||!stage::same_client_owner(prepared->original,current)||
           current.census.members!=prepared->original.census.members||current.census.next_id!=prepared->original.census.next_id)
            return fail(e,e.empty()?"Party/Toon preparation current cohort changed":e);
        out=std::move(prepared);return true;
    }catch(const std::exception& ex){return fail(e,std::string("Party/Toon preparation C++: ")+ex.what());}
    catch(...){return fail(e,"Party/Toon preparation unknown C++ exception");}
}
bool verify_party_toon_edits_held(PartyToonBatch& batch,std::string& e){
    if(!batch.held||batch.failed)return fail(e,"Party/Toon transaction is not a healthy held original SaveScope");
    stage::ClientWorld current;
    return read_world_roots(batch.world,true,current,e)&&
        (stage::same_client_owner(batch.world.original,current)||fail(e,"Party/Toon held Client owner changed"));
}
bool pending_toon(PartyToonBatch& batch,int32_t id,PendingToon& out,std::string& e){
    out={};
    if(!batch.pending_visible)return fail(e,"Party/Toon constructor results are outside their held completion phase");
    std::vector<PendingToon> current;
    if(!pending_array(batch,current,e)||current!=batch.pending)return fail(e,e.empty()?"Party/Toon constructor result array changed":e);
    for(const auto& row:current)if(row.subscriber_id==id){out=row;return true;}
    return fail(e,"Party/Toon pending Subscriber ID is absent");
}
bool with_party_toon_edits_held(const std::string& origin,const FrameBinding& frame,
    const std::function<bool(PartyToonBatch&,std::string&)>& body,
    const std::function<bool(std::string&)>& exit_guard,local::SaveScopeReceipt& receipt,std::string& e){
    receipt={};e.clear();
    if(!body||!exit_guard)return fail(e,"Party/Toon borrowed held callbacks are absent");
    if(busy.test_and_set())return fail(e,"Party/Toon shared Subscriber native scope busy");
    struct Release {~Release(){lookup_scope.clear();busy.clear();}} release;
    PartyToonBatch batch;batch.world.origin=origin;batch.world.local_sha=frame.local_sha;batch.world.frame=&frame;
    bool ok=false;
    try{
        if(!hook_ready||!image_base)return fail(e,"Party/Toon original lookup hook is not ready");
        if(!read_world(batch.world,true,batch.world.original,e))return false;
        if(batch.world.original.image!=image_base)return fail(e,"Party/Toon borrowed native image differs");
        if(blocked_epoch==batch.world.original.origin.world_epoch&&blocked_generation==batch.world.original.origin.generation)
            return fail(e,"Party/Toon previous outcome unknown: "+blocked_cause);
        batch.held=true;receipt.body_attempted=true;
        ok=body(batch,e)&&!batch.failed;
        receipt.body_cause=e;
    }catch(const std::exception& ex){e=std::string("Party/Toon borrowed body C++: ")+ex.what();}
    catch(...){e="Party/Toon borrowed body unknown C++ exception";}
    if(receipt.body_attempted){
        try{
            stage::ClientWorld current;
            receipt.exit_guard_confirmed=read_world_roots(batch.world,true,current,receipt.cleanup_cause)&&
                (stage::same_client_owner(batch.world.original,current)||fail(receipt.cleanup_cause,"Party/Toon borrowed exit owner changed"))&&exit_guard(receipt.cleanup_cause);
        }catch(const std::exception& ex){receipt.cleanup_cause=std::string("Party/Toon borrowed exit C++: ")+ex.what();}
        catch(...){receipt.cleanup_cause="Party/Toon borrowed exit unknown C++ exception";}
        if(!receipt.exit_guard_confirmed){ok=false;if(!receipt.cleanup_cause.empty())e+=(e.empty()?"":"; ")+receipt.cleanup_cause;}
        if(!ok){
            receipt.unknown=true;blocked_epoch=batch.world.original.origin.world_epoch;blocked_generation=batch.world.original.origin.generation;blocked_cause=e;
        }
    }
    batch.held=false;batch.pending_visible=false;return ok;
}
bool append_party_toons(PartyToonBatch& batch,const std::shared_ptr<PartyToonInputs>& prepared,stage::PartyReferences& parties,
    const std::function<bool(const std::vector<PendingToon>&,std::string&)>& finish_parties,
    const std::function<bool(const PendingToon&,int32_t,void*,std::string&)>& rearm_party,
    const std::function<bool(std::string&)>& finalize_parties,Json& result,std::string& e){
    result={{"state","rejected"},{"subscriber_creation_attempted",false},{"confirmed",false}};
    if(!verify_party_toon_edits_held(batch,e))return false;
    if(batch.used||!prepared||prepared->used||prepared->rows.empty()||!finish_parties||!rearm_party||!finalize_parties)
        return fail(e,"Party/Toon append batch reused or missing prepared input/callback");
    const auto input_owner=prepared;
    const auto& frame=*batch.world.frame;
    if(prepared->origin!=batch.world.origin||prepared->frame.context!=frame.context||prepared->frame.binding!=frame.binding||
       prepared->frame.common_epoch!=frame.common_epoch||prepared->frame.local_sha!=frame.local_sha||
       !stage::same_client_owner(prepared->original,batch.world.original)||prepared->original.census.members!=batch.world.original.census.members||
       prepared->original.census.next_id!=batch.world.original.census.next_id)
        return fail(e,"Party/Toon prepared owner/Frame/cohort changed before held append");
    batch.used=true;prepared->used=true;
    auto rows=std::move(prepared->rows);
    struct RetainOnUnwind {
        std::vector<std::unique_ptr<PartyToonRow>>& rows;
        PartyToonBatch& batch;
        bool released=false;
        ~RetainOnUnwind(){if(!released){
            batch.failed=true;
            for(auto& row:rows)if(row->resources){
                row->resources->retained_next=std::move(retained_resources);retained_resources=std::move(row->resources);
            }
        }}
    } retained_on_unwind{rows,batch};
    bool effect=false,okay=false,finalize_attempted=false,finalize_returned=false;
    stage::PartyToonOrderReceipt order;
    try{
        const int32_t next_id=prepared->next_id;
        native::SimulationState money_before;
        if(!native::read_simulation_state(money_before,e))throw std::runtime_error(e);
        for(auto& row:rows){auto& w=row->world;w.parties=&parties;
            if(!read_world_roots(w,true,w.held,e)||!stage::same_client_owner(w.original,w.held))throw std::runtime_error(e.empty()?"Party/Toon held row owner differs":e);
            const stage::ReferenceWorld world{image_base,reinterpret_cast<uintptr_t>(w.held.origin.state),reinterpret_cast<uintptr_t>(w.held.origin.map),&w.building_pins,&w.npc_pins,&parties};
            for(const auto& ref:w.payload.external)if(ref.key.kind==stage::ReferenceKind::RootRegion){uintptr_t region=0;
                if(!stage::resolve_reference(memory,world,ref.key,region,e))throw std::runtime_error(e);
                if(w.region&&w.region!=region)throw std::runtime_error("Party/Toon new Subscriber spans root Regions");w.region=region;}
            if(!w.region||!toon_array(w,w.old_toons,e))throw std::runtime_error(e.empty()?"Party/Toon new Subscriber root Region missing":e);
        }
        const auto load=[&](size_t index,std::string& cause){auto& row=*rows[index];auto& w=row.world;auto& resource=*row.resources;
            if(!verify_party_toon_edits_held(batch,cause)||
               !stage::decode_records(api.records,memory,w.payload.records,resource.record,resource.decoded,cause)||
               !stage::decode_integer_record(api.records,memory,"nextId",w.payload.next_id,resource.next,resource.next_decoded,cause))return false;
            if(!stage::context_call(api.context_construct,&resource.context,row.construct,cause))return false;
            // Original Load must defer external lookup until Resolve. Record an
            // unexpected eager lookup as a native failure, never a guessed link.
            if(!lookup_scope.arm(w.held.thread,&resource.context,w.payload.local_ids,{},fresh_link,&w,cause))return false;
            effect=true;
            bool ok=local::load_original(api.calls,reinterpret_cast<void*>(w.held.manager+0x38),reinterpret_cast<void*>(resource.record.root),&resource.context,row.load,cause);
            if(ok)ok=local::next_id_original(api.calls,reinterpret_cast<void*>(image_base+local::next_id_property_rva),reinterpret_cast<void*>(w.held.manager),reinterpret_cast<void*>(resource.next.root),&resource.context,row.next,cause);
            row.load_lookup=lookup_scope.receipt();lookup_scope.clear();
            if(!ok||row.load_lookup.failed)return fail(cause,cause.empty()?row.load_lookup.cause:cause);
            std::vector<PendingToon> actual;
            if(!pending_array(batch,actual,cause))return false;
            if(actual.size()!=index+1||!std::equal(batch.pending.begin(),batch.pending.end(),actual.begin())||actual.back().subscriber_id!=w.payload.id)
                return fail(cause,"Party/Toon original ArrayLoad constructor result differs");
            batch.pending=std::move(actual);return true;
        };
        const auto complete=[&](std::string& cause){
            batch.pending_visible=true;
            // Before Resolve this slot still contains a saved integer. Party
            // completion must not pass it to LeaveParty as a native pointer.
            for(size_t i=0;i<rows.size();++i)if(rows[i]->saved_party_id>0){
                auto& row=*rows[i];row.party_clear_attempted=true;
                if(!rearm_party(batch.pending[i],0,&row.resources->context,cause))return false;
                row.party_clear_returned=true;
            }
            if(!verify_party_toon_edits_held(batch,cause)||!finish_parties(batch.pending,cause))return false;
            std::vector<PendingToon> current;
            return pending_array(batch,current,cause)&&(current==batch.pending||fail(cause,"Party completion changed pending Toon lifetimes"));
        };
        const auto resolve=[&](size_t index,std::string& cause){auto& row=*rows[index];auto& w=row.world;auto& resource=*row.resources;
            if(!verify_party_toon_edits_held(batch,cause)||!deferred_links(batch,w,cause))return false;
            DeferredLinks borrowed{batch,w};
            if(!lookup_scope.arm(w.held.thread,&resource.context,w.payload.local_ids,w.links,DeferredLinks::fresh,&borrowed,cause))return false;
            // Party PostResolve needed the real reciprocal pointer. The original
            // Subscriber Resolve expects the saved ID in this same property slot.
            if(row.saved_party_id>0){
                row.party_rearm_attempted=true;
                if(!rearm_party(batch.pending[index],row.saved_party_id,&resource.context,cause)){lookup_scope.clear();return false;}
                row.party_rearm_returned=true;
            }
            const bool ok=stage::context_call(api.context_destroy,&resource.context,row.resolve,cause);
            row.resolve_lookup=lookup_scope.receipt();lookup_scope.clear();
            if(!ok||row.resolve_lookup.failed)return fail(cause,cause.empty()?row.resolve_lookup.cause:cause);
            resource.context=0;return w.npc_pins.verify_quest_references(cause);
        };
        okay=stage::party_toon_order(rows.size(),load,complete,resolve,order,e);
        if(okay){local::Census after;
            if(!stage::read_cohort(memory,batch.world.original,after,e,false)||after.members.size()!=batch.world.original.census.members.size()+rows.size()||after.next_id!=next_id)
                throw std::runtime_error(e.empty()?"Party/Toon resolved cohort count/nextID differs":e);
            for(size_t i=0;i<batch.world.original.census.members.size();++i)if(after.members[i]!=batch.world.original.census.members[i])throw std::runtime_error("Party/Toon append changed existing Subscriber graph");
            for(size_t i=0;i<rows.size();++i){const auto& actual=after.members[batch.world.original.census.members.size()+i];auto& row=*rows[i];
                if(actual.id!=batch.pending[i].subscriber_id||actual.subscriber!=batch.pending[i].subscriber||actual.toon!=batch.pending[i].toon||!actual.online||!actual.brain||
                   std::find(after.online.begin(),after.online.end(),actual.id)!=after.online.end()||std::find(after.offline.begin(),after.offline.end(),actual.id)!=after.offline.end())
                    throw std::runtime_error("Party/Toon resolved Subscriber/Toon/Brain/classification differs");
                if(!local::classify_original(api.calls,reinterpret_cast<void*>(batch.world.original.manager),reinterpret_cast<void*>(actual.subscriber),true,row.login,e))throw std::runtime_error(e);
            }
            const auto resolved_members=after.members;
            finalize_attempted=true;
            if(!finalize_parties(e))throw std::runtime_error(e.empty()?"Party/Toon final Party native phase rejected without cause":e);
            finalize_returned=true;
            std::vector<PendingToon> final_pending;
            if(!pending_array(batch,final_pending,e)||final_pending!=batch.pending)throw std::runtime_error(e.empty()?"Party/Toon final Party phase changed actual Toon lifetimes":e);
            if(!stage::read_cohort(memory,batch.world.original,after,e)||after.members!=resolved_members||after.next_id!=next_id)
                throw std::runtime_error(e.empty()?"Party/Toon final Party phase changed Subscriber cohort":e);
            for(size_t i=0;i<rows.size();++i){auto& w=rows[i]->world;const auto& born=batch.pending[i];
                std::vector<uintptr_t> toons,expected=w.old_toons;uintptr_t region=0,last_region=0,shard=0;
                for(size_t j=0;j<rows.size();++j)if(rows[j]->world.region==w.region)expected.push_back(batch.pending[j].toon);
                if(!toon_array(w,toons,e)||!at(born.toon,0x178,region,e)||!at(born.toon,0x9a8,last_region,e)||!at(born.toon,0x170,shard,e)||
                   toons!=expected||region!=w.region||last_region!=w.region||shard!=reinterpret_cast<uintptr_t>(w.held.origin.map)+0x1c0)
                    throw std::runtime_error(e.empty()?"Party/Toon original Region registration differs":e);
                native::SubscriberNeedsState needs;if(!native::read_subscriber_needs(born.subscriber_id,needs,e))throw std::runtime_error(e);
                for(size_t n=0;n<needs.needs.size();++n)if(std::bit_cast<uint32_t>(needs.needs[n])!=w.payload.needs_bits[n])throw std::runtime_error("Party/Toon original Need readback differs index="+std::to_string(n));
                if(!w.building_pins.verify_used(e)||!w.npc_pins.verify_used(e))throw std::runtime_error(e);
            }
            native::SimulationState money_after;
            if(!native::read_simulation_state(money_after,e)||!finances_equal(money_before,money_after))throw std::runtime_error(e.empty()?"Party/Toon append changed business finance":e);
        }
    }catch(const std::exception& ex){okay=false;e=std::string("Party/Toon append C++: ")+ex.what();}
    catch(...){okay=false;e="Party/Toon append unknown C++ exception";}
    lookup_scope.clear();batch.pending_visible=false;
    result["subscriber_creation_attempted"]=effect;
    result["order"]={{"loads_attempted",order.loads_attempted},{"loads_returned",order.loads_returned},{"parties_attempted",order.parties_attempted},
        {"parties_returned",order.parties_returned},{"resolves_attempted",order.resolves_attempted},{"resolves_returned",order.resolves_returned},
        {"finalize_parties_attempted",finalize_attempted},{"finalize_parties_returned",finalize_returned}};
    result["subscribers"]=Json::array();
    for(const auto& row:rows)result["subscribers"].push_back({{"id",row->world.payload.id},{"context_constructed",row->construct.returned},
        {"array_load_attempted",row->load.attempted},{"array_load_returned",row->load.returned},{"context_resolve_attempted",row->resolve.attempted},
        {"context_resolve_returned",row->resolve.returned},{"notify_login_returned",row->login.returned},
        {"party_saved_id_clear_attempted",row->party_clear_attempted},{"party_saved_id_clear_returned",row->party_clear_returned},
        {"party_saved_id_rearm_attempted",row->party_rearm_attempted},{"party_saved_id_rearm_returned",row->party_rearm_returned},
        {"load_lookup_cause",row->load_lookup.cause},{"resolve_lookup_cause",row->resolve_lookup.cause}});
    bool keep=!okay;
    const bool native_attempt=effect||std::any_of(rows.begin(),rows.end(),[](const auto& row){
        return row->construct.attempted||row->resources->decoded.unknown||row->resources->next_decoded.unknown;
    });
    for(auto& row:rows){auto& resource=*row->resources;
        if(!keep){std::string cleanup;
            if(!stage::destroy_records(api.records,resource.next,resource.next_decoded,cleanup)||!stage::destroy_records(api.records,resource.record,resource.decoded,cleanup)){
                e+=(e.empty()?"":"; ")+cleanup;keep=true;okay=false;}
        }
        if(keep){resource.retained_next=std::move(retained_resources);retained_resources=std::move(row->resources);}
    }
    if(!okay){
        batch.failed=true;result["state"]=native_attempt?"unknown":"rejected";result["replay_permitted"]=false;result["private_owners_retained"]=keep;result["cause"]=e;retained_on_unwind.released=true;return false;
    }
    result["state"]="held_readback";result["native_record_cleanup_confirmed"]=true;result["subscriber_count_added"]=rows.size();retained_on_unwind.released=true;return true;
}
