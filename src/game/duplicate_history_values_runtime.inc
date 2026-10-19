struct LocalizedPort {
    Runtime& runtime;alignas(8) std::array<unsigned char,0x38> value{};
    bool get(uintptr_t building,uintptr_t& pointer,std::string& e){
        const auto code=duplicate_history_value_calls::get_fluff(runtime.image,building,value.data());
        if(code)return q::fail(e,"Duplicate original GetFluff SEH="+std::to_string(code));
        pointer=reinterpret_cast<uintptr_t>(value.data());return true;
    }
    bool read(uintptr_t pointer,duplicate_localized_history::Value& out,std::string& e){
        auto text=[&](uintptr_t p,std::string& s,std::string& why){return prop_text::text(runtime,p,s,why);};
        return duplicate_localized_history::read(runtime.memory,pointer,{100000,100000,32*1024*1024},text,out,e);
    }
    bool destroy(uintptr_t pointer,bool arguments,std::string& e){
        const auto code=duplicate_history_value_calls::destroy_fluff(runtime.image,reinterpret_cast<void*>(pointer),arguments);
        return !code||q::fail(e,std::string("Duplicate original localized ")+(arguments?"arguments":"text")+" cleanup SEH="+std::to_string(code));
    }
};
bool history_localized(Runtime& r,uintptr_t building,std::string& sha,std::string& e){
    if(!duplicate_history_value_calls::code(r.memory,r.image,e))return false;
    LocalizedPort port{r};duplicate_building_values::LocalizedReceipt receipt;
    return duplicate_building_values::localized(port,building,sha,receipt,e);
}
bool history_stored_localized(Runtime& r,uintptr_t pointer,std::string& sha,std::string& e){
    duplicate_localized_history::Value value;
    auto text=[&](uintptr_t p,std::string& s,std::string& why){return prop_text::text(r,p,s,why);};
    return duplicate_localized_history::read(r.memory,pointer,{100000,100000,32*1024*1024},text,value,e)&&
        duplicate_localized_history::summarize(value,sha,e);
}
bool history_owned_record(Runtime& r,uintptr_t pointer,duplicate_building_values::OwnedKind kind,bool stored,std::string& sha,std::string& e){
    if(!duplicate_history_value_calls::code(r.memory,r.image,e))return false;
    GraphMemory memory(r.memory);
    if(stored)return duplicate_building_values::stored(memory,pointer,{100000,1000000,32*1024*1024,100000},sha,e);
    auto capture=[&](uintptr_t object,uintptr_t prepare,uintptr_t save,subscriber_lifecycle::Graph& graph,std::string& why){
        namespace native_record=subscriber_lifecycle;RecordCapture result{memory};native_record::CaptureReceipt receipt;
        const native_record::CaptureApi api{reinterpret_cast<native_record::Construct>(r.image+0x9667c0),
            reinterpret_cast<native_record::Construct>(r.image+0x9676a0),reinterpret_cast<native_record::Construct>(r.image+0x89e580),
            reinterpret_cast<native_record::Construct>(r.image+0x89e5f0),reinterpret_cast<native_record::PrepareSave>(r.image+prepare),
            reinterpret_cast<native_record::SaveRecord>(r.image+save)};
        if(!native_record::capture_original(api,reinterpret_cast<const void*>(object),inspect_record,&result,receipt,why))return false;
        graph=std::move(result.graph);return true;
    };
    return duplicate_building_values::live(r.memory,r.image,pointer,kind,capture,sha,e);
}
bool history_building_value(Runtime& r,uintptr_t building,duplicate_building_history::Saved& out,std::string& e){
    if(!duplicate_history_value_calls::code(r.memory,r.image,e))return false;
    auto text=[&](uintptr_t p,std::string& value,std::string& why){return prop_text::text(r,p,value,why);};
    auto fluff=[&](uintptr_t p,std::string& sha,std::string& why){return history_localized(r,p,sha,why);};
    auto record=[&](uintptr_t p,duplicate_building_values::OwnedKind kind,std::string& sha,std::string& why){return history_owned_record(r,p,kind,false,sha,why);};
    auto theme=[&](uintptr_t p,std::string& name,std::string& why){uintptr_t value=0;
        const auto code=duplicate_history_value_calls::theme(r.image,p,value);
        if(code||!value)return q::fail(why,"Duplicate original Building Theme absent/SEH="+std::to_string(code));
        return prop_text::text(r,value+0x20,name,why);};
    auto model=[&](uintptr_t p,uintptr_t& value,std::string& why){const auto code=duplicate_history_value_calls::model(r.image,p,value);
        return !code||q::fail(why,"Duplicate original Art Building model SEH="+std::to_string(code));};
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& why){return group_edit::find(r,uid,value,why);};
    return duplicate_building_history::live(r.memory,r.image,building,text,fluff,record,theme,model,find,out,e);
}
bool history_building_inverse(Runtime& r,uintptr_t node,const duplicate_building_history::Saved& expected,std::string& e){
    auto text=[&](uintptr_t p,std::string& value,std::string& why){return prop_text::text(r,p,value,why);};
    auto localized=[&](uintptr_t p,std::string& sha,std::string& why){return history_stored_localized(r,p,sha,why);};
    auto record=[&](uintptr_t p,bool stored,std::string& sha,std::string& why){
        using Kind=duplicate_building_values::OwnedKind;Kind kind=Kind::Bugs;
        if(!stored){uintptr_t ap=0;if(!q::at(r.memory,p,0,ap,why))return false;
            if(ap==r.image+0x1712a60)kind=Kind::Layout;
            else if(ap==r.image+0x16fea40)kind=Kind::Plan;
            else return q::fail(why,"Duplicate Make Building owned value is not its original Layout/Plan");}
        return history_owned_record(r,p,kind,stored,sha,why);
    };
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& why){return group_edit::find(r,uid,value,why);};
    return duplicate_building_history::inverse(r.memory,r.image,node,expected,text,localized,record,find,e);
}
struct HistoryServices {
    duplicate_service_history::World saved;
    std::map<uint64_t,action::ServiceMembershipState> shared;
    std::map<uint64_t,service_membership_reader::Local> local;
};
bool history_service_world(Runtime& r,const Sample& current,HistoryServices& out,std::string& e){
    out={};std::set<uint64_t> services;uintptr_t character_owner=0;
    if(!q::at(r.memory,uintptr_t(current.world.origin.state),0x168,character_owner,e))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& why){return q::at(r.memory,base,offset,value,why);};
    auto text=[&](uintptr_t p,std::string& value,std::string& why){return prop_text::text(r,p,value,why);};
    auto find=[&](uint64_t uid,uintptr_t& value,std::string& why){return group_edit::find(r,uid,value,why);};
    auto destination=[&](uintptr_t pointer,std::string& key,std::string& why){
        const auto local=current.census.objects.find(pointer);
        if(local==current.census.objects.end()||local->second.family!=Family::Building)
            return q::fail(why,"Duplicate history Service destination is outside the complete current Building census");
        for(const auto& [candidate,handle]:current.handles)if(handle==local->second.handle){key=candidate;return true;}
        return q::fail(why,"Duplicate history Service destination lacks a current canonical full UID");
    };
    for(const auto& [pointer,row]:current.census.objects){if(row.family!=Family::Building)continue;
        const auto uid=row.handle.uid;auto& state=out.shared[uid];auto& local=out.local[uid];
        if(!service_membership_reader::sample(at,text,destination,r.image,pointer,state,local,e))return false;
        if(local.manager!=character_owner)return q::fail(e,"Duplicate history Service Character manager differs from current native World");
        auto& saved=out.saved[uid];
        for(size_t i=0;i<local.rows.size();++i){std::array<int32_t,2> price{};duplicate_service_history::Saved value;
            if(!duplicate_service_history::effective_price(state,local,i,price,e)||
               !duplicate_service_history::capture(r.memory,r.image,local.rows[i].object,pointer,price,find,value,e))return false;
            if(value.uid!=local.rows[i].uid||!services.insert(value.uid).second)
                return q::fail(e,"Duplicate history complete Service full UID is repeated or changed");
            saved.push_back(std::move(value));
        }
    }
    host::WorldState after;
    if(!world(r,after,e)||!host::same_world(current.world.origin,after.origin))
        return q::fail(e,e.empty()?"Duplicate history World changed during complete Service census":e);
    return true;
}
bool history_effect_witness(const Sample& current,const HistoryServices& services,const monster_quest_retirement::Witness& quests,
                            const std::set<std::string>& related,const std::vector<duplicate_history_effects::OwnedChild>& children,
                            action::DuplicateState& state,std::string& e){
    std::map<uint64_t,std::string> keys;
    for(const auto& [key,row]:current.objects)if(row.family==Family::Building){const auto handle=current.handles.find(key);
        if(handle==current.handles.end()||!keys.emplace(handle->second.uid,key).second)
            return q::fail(e,"Duplicate complete Service witness current Building full UID/key repeats");}
    action::DuplicateHistoryEffects effects;effects.related_keys.assign(related.begin(),related.end());std::string props_sha;
    if(!duplicate_history_effects::services(services.saved,services.shared,keys,effects.services_sha,e)||
       !duplicate_history_effects::quests(quests,effects.quests_sha,e)||
       !duplicate_history_effects::prop_records(current.objects,state,related,props_sha,state.untouched_sha,e)||
       !duplicate_history_effects::owned_children(props_sha,children,effects.related_sha,e))return false;
    state.history=std::move(effects);return true;
}
struct HistoryBuildingPlan {
    std::map<uint64_t,duplicate_building_history::Saved> values;
    HistoryServices services;monster_quest_retirement::Witness quests;
    duplicate_service_history::Removal service_removal;
    std::vector<action::DuplicateCash> undo_cash;
    std::vector<uint64_t> ordered_buildings;std::vector<uintptr_t> destinations,quest_owners;
    std::set<std::string> related_keys;
    std::map<uint64_t,child_npc::Receipt> children;
};
std::vector<duplicate_history_effects::OwnedChild> history_children(const std::map<uint64_t,child_npc::Receipt>& children,bool retired){
    std::vector<duplicate_history_effects::OwnedChild> out;
    for(const auto& [uid,receipt]:children){(void)uid;duplicate_history_effects::OwnedChild row;
        row.parent_key=receipt.parent.key;row.present=receipt.present&&!retired;
        if(row.present){row.child_key=receipt.child.key;row.record_sha=receipt.original_record_sha;}out.push_back(std::move(row));}
    return out;
}
bool history_child_owners(const std::map<uint64_t,child_npc::Receipt>& children,monster_quest_retirement::Witness& seed,std::string& e){
    for(const auto& [uid,receipt]:children){(void)uid;if(!receipt.present)continue;
        const auto pointer=receipt.child.row.local_pointer;monster_quest_retirement::Owner owner;
        owner.pointer=pointer;owner.uid=receipt.child.row.local_uid;owner.key=receipt.child.key;
        owner.state=receipt.original_quests;owner.destination_uids=receipt.original_destination_uids;
        const auto found=seed.owners.find(pointer);
        if(found!=seed.owners.end()&&(found->second.uid!=owner.uid||found->second.key!=owner.key))
            return q::fail(e,"Duplicate child Quest previous owner aliases another full UID/key");
        seed.owners[pointer]=std::move(owner);
    }return true;
}
bool history_building_plan(Runtime& r,const action::Binding& binding,const Sample& current,uintptr_t graph,
                           const std::map<uint64_t,duplicate_history::Known>& known,const action::Request& request,
                           HistoryBuildingPlan& out,std::string& e){
    out={};duplicate_history::Graph original;std::vector<uintptr_t> ordered;
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& why){return group_edit::find(r,uid,pointer,why);};
    auto text=[&](uintptr_t p,std::string& value,std::string& why){return prop_text::text(r,p,value,why);};
    if(!duplicate_history::read(r.memory,r.image,graph,true,known,find,text,original,e)||
       !duplicate_history::execution_order(r.memory,r.image,graph,original,ordered,e))return false;
    out.undo_cash=original.cash;
    for(auto pointer:ordered){uintptr_t ap=0;uint64_t uid=0;
        if(!q::at(r.memory,pointer,0,ap,e))return false;if(ap!=r.image+0x16de860)continue;
        if(!q::at(r.memory,pointer,0xb8,uid,e))return false;
        const auto target=known.find(uid);if(target==known.end())return q::fail(e,"Duplicate Building history sorted Destroy target absent");
        if(target->second.row.family!=Family::Building)continue;
        duplicate_building_history::Saved value;
        if(!history_building_value(r,target->second.object,value,e)||value.uid!=uid||!out.values.emplace(uid,std::move(value)).second)
            return q::fail(e,e.empty()?"Duplicate Building history complete live target repeats or changed UID":e);
        out.ordered_buildings.push_back(uid);out.destinations.push_back(target->second.object);
        child_npc::Parent parent;child_npc::Receipt child;
        auto owner=[&](uintptr_t p,std::string& key,std::string& why){return external_owner(r,current,p,key,why);};
        if(!child_parent(r,current,target->second.row.key,parent,e)||
           !child_npc::capture_before(child_lease(binding,request,request.target_action_uuid),parent,owner,child,e))return false;
        if(child.present)out.destinations.push_back(child.child.row.local_pointer);
        out.children.emplace(uid,std::move(child));
    }
    if(out.values.empty())return q::fail(e,"Duplicate Building history plan has no original Building target");
    monster_quest_retirement::Witness seed;
    if(!history_child_owners(out.children,seed,e)||!history_service_world(r,current,out.services,e)||
       !duplicate_service_history::removal(out.services.saved,out.ordered_buildings,out.service_removal,e)||
       !npc_probe::world_quest_history::read(binding,net::current_snapshot_id(),out.quests,e,&seed))return false;
    out.quest_owners=duplicate_quest_history::affected(out.quests,out.destinations);
    for(const auto& [uid,child]:out.children){(void)uid;if(child.present&&
        std::find(out.quest_owners.begin(),out.quest_owners.end(),child.child.row.local_pointer)==out.quest_owners.end())
            out.quest_owners.push_back(child.child.row.local_pointer);}
    for(auto uid:out.service_removal.related_owners){if(out.values.contains(uid))continue;
        bool found=false;for(const auto& [key,handle]:current.handles)if(handle.uid==uid){
            const auto object=current.objects.find(key);
            if(object==current.objects.end()||object->second.family!=Family::Building)
                return q::fail(e,"Duplicate related reciprocal owner is not a current Building");
            out.related_keys.insert(key);found=true;break;}
        if(!found)return q::fail(e,"Duplicate related reciprocal owner has no complete canonical full UID binding");
    }
    return true;
}
struct HistoryQuestMemory final:npc_census::Memory {
    Runtime& runtime;explicit HistoryQuestMemory(Runtime& value):runtime(value){}
    bool copy(uintptr_t p,void* value,size_t size,std::string& e)override{return runtime.memory.read(p,value,size,e);}
};
bool history_building_graph(Runtime& r,uintptr_t graph,const std::map<uint64_t,duplicate_history::Known>& known,
                            const child_npc::Lease& lease,HistoryBuildingPlan& expected,std::vector<uintptr_t>& quest_order,std::string& e){
    quest_order.clear();if(!duplicate_history_value_calls::code(r.memory,r.image,e))return false;
    std::set<uint64_t> services,quests,npcs;std::map<uint64_t,const monster_quest_retirement::Owner*> owners;
    std::map<uint64_t,child_npc::Receipt*> children;
    for(auto& [uid,child]:expected.children){(void)uid;if(child.present&&!children.emplace(child.child.row.local_uid,&child).second)
        return q::fail(e,"Duplicate Building child inverse full UID repeats");}
    for(auto pointer:expected.quest_owners){const auto found=expected.quests.owners.find(pointer);
        if(found==expected.quests.owners.end()||!owners.emplace(found->second.uid,&found->second).second)
            return q::fail(e,"Duplicate Building history related Quest owner full UID repeats or is absent");}
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& why){return group_edit::find(r,uid,pointer,why);};
    auto text=[&](uintptr_t p,std::string& value,std::string& why){return prop_text::text(r,p,value,why);};
    auto building=[&](uintptr_t pointer,const duplicate_history::Known& target,std::string& why){const auto value=expected.values.find(target.uid);
        return value!=expected.values.end()?history_building_inverse(r,pointer,value->second,why):
            q::fail(why,"Duplicate Make Building inverse has no complete captured original value");};
    HistoryQuestMemory memory(r);
    auto related=[&](uintptr_t pointer,uintptr_t ap,std::string& why){uint64_t uid=0;
        if(!q::at(r.memory,pointer,0xb8,uid,why))return false;
        if(ap==r.image+0x1737830){const auto value=expected.service_removal.saved.find(uid);
            if(value==expected.service_removal.saved.end()||!services.insert(uid).second)
                return q::fail(why,"Duplicate MakeService inverse full UID is foreign or repeated");
            return duplicate_service_history::inverse(r.memory,r.image,pointer,value->second,find,why);
        }
        if(ap==r.image+0x16de8a0){const auto child=children.find(uid);child_npc::MakeNPCInverse inverse;
            if(child==children.end()||!npcs.insert(uid).second)
                return q::fail(why,"Duplicate MakeNPC inverse full UID is foreign or repeated");
            return child_npc::validate_make_npc(lease,pointer,*child->second,inverse,why);
        }
        uintptr_t method=0;const auto owner=owners.find(uid);
        if(ap!=r.image+0x1713330||owner==owners.end()||!quests.insert(uid).second)
            return q::fail(why,"Duplicate SetNPCQuests inverse full UID/class is foreign or repeated");
        if(!q::at(r.memory,ap,0x10,method,why))return false;
        if(method!=r.image+0x563f50)return q::fail(why,"Duplicate SetNPCQuests inverse original Do method differs");
        return monster_quest_retirement::inverse(memory,r.image,pointer,*owner->second,why);
    };
    duplicate_history::Graph nodes;std::vector<uintptr_t> ordered;
    if(!duplicate_history::read(r.memory,r.image,graph,false,known,find,text,building,related,nodes,e))return false;
    auto cash=expected.undo_cash;for(auto& row:cash){if(row.amount<0)return q::fail(e,"Duplicate original Undo cash amount is negative");row.amount=-row.amount;}
    if(!duplicate_history::cash_equal(nodes.cash,cash))return q::fail(e,"Duplicate Building inverse complete signed cash nodes differ");
    if(services.size()!=expected.service_removal.saved.size()||quests.size()!=owners.size()||npcs.size()!=children.size())
        return q::fail(e,"Duplicate Building inverse count differs services="+std::to_string(services.size())+" required="+
            std::to_string(expected.service_removal.saved.size())+" quests="+std::to_string(quests.size())+" required="+std::to_string(owners.size())+
            " npcs="+std::to_string(npcs.size())+" required="+std::to_string(children.size()));
    if(!duplicate_history::execution_order(r.memory,r.image,graph,nodes,ordered,e))return false;
    for(auto pointer:ordered){uintptr_t ap=0;uint64_t uid=0;
        if(!q::at(r.memory,pointer,0,ap,e))return false;if(ap!=r.image+0x1713330)continue;
        if(!q::at(r.memory,pointer,0xb8,uid,e))return false;
        const auto owner=owners.find(uid);if(owner==owners.end())return q::fail(e,"Duplicate sorted Quest inverse owner is foreign");
        quest_order.push_back(owner->second->pointer);
    }
    return true;
}
