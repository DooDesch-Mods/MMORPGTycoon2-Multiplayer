namespace building_clearance {
namespace q=npc_quest_targets;
using Family=action::DuplicateFamily;
using Saved=scenery_make_history::Saved;
struct Local {
    action::Binding binding;action::Request request;
    duplicate_edit::Sample sample;
    std::map<uint64_t,Saved> live,removed;
    std::map<uint64_t,std::string> aliases;
    std::map<std::string,action::DuplicateObject> original;
    scenery_stroke_state::Objects objects;
    action::SceneryStrokeEffect effect;
    scenery_alias_order::Receipt settlement;
    uintptr_t graph=0;bool restoring=false,initial=false,finished=false;
};
namespace {
bool runtime(Runtime*& out,const action::Binding& binding,const action::Request& request,std::string& error){
    out=published.load(std::memory_order_acquire);
    if(!out||!out->ready||out->thread!=GetCurrentThreadId()||request.snapshot_uuid!=net::current_snapshot_id()||
       !binding.native_epoch||!binding.native_generation)
        return q::fail(error,"Building clearance lacks its current Mainthread/snapshot binding");
    return true;
}
bool read(Runtime& r,const action::Binding& binding,Local& out,std::string& error){
    out.live.clear();out.objects.clear();
    if(!duplicate_edit::sample(r,binding,nullptr,{},out.sample,error))return false;
    // Saved Root Scenery also exists outside the current selectable Region
    // array. Its existing metadata census proves those exact saved owners.
    std::vector<remove_diagnostics::LocalTarget> complete;
    if(!remove_diagnostics::identity_targets(complete,error))return false;
    std::vector<std::string> extra;
    for(const auto& target:complete){
        if(!target.normal_scenery||out.sample.handles.contains(target.record_key))continue;
        uintptr_t root=0,region=0,parent=0;int32_t id=-1;
        if(!q::at(r.memory,target.object,0x170,root,error)||!q::at(r.memory,target.object,0x178,region,error)||
           !q::at(r.memory,target.object,0x180,parent,error)||!q::at(r.memory,region,0x834,id,error))return false;
        if(root!=out.sample.world.root||id<0||parent)
            return q::fail(error,"Building clearance saved Root Scenery owner or unbound Group differs");
        action::DuplicateObject row;row.key=target.record_key;row.family=Family::Scenery;row.region=id;
        if(!group_reader::pose(r.memory,target.object,0x100,row.pose,error)||
           !out.sample.handles.emplace(row.key,duplicate_native::Handle{target.object,target.uid}).second||
           !out.sample.objects.emplace(row.key,row).second||
           !out.sample.census.objects.emplace(target.object,duplicate_reader::Object{{target.object,target.uid},region,0,id,Family::Scenery,row.pose}).second)
            return q::fail(error,error.empty()?"Building clearance complete saved Root key/full UID repeats":error);
        if(std::find(out.sample.census.regions.begin(),out.sample.census.regions.end(),region)==out.sample.census.regions.end())
            out.sample.census.regions.push_back(region);
        native::BuildingState actual;
        if(!scenery_kiss::read_action_prop(target.uid,actual,error))return false;
        out.sample.props.emplace(row.key,world_state::building_state(actual));
        extra.push_back(row.key);
    }
    for(const auto& key:extra)if(!duplicate_edit::record(r,out.sample,key,out.sample.objects.at(key),error))return false;
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& why){return group_edit::find(r,uid,pointer,why);};
    for(const auto& [key,row]:out.sample.objects){
        if(row.family==Family::Group){out.objects.emplace(key,row);continue;}
        if(row.family!=Family::Scenery)continue;
        const auto handle=out.sample.handles.at(key);const auto current=out.sample.census.objects.find(handle.object);
        Saved value;
        if(current==out.sample.census.objects.end()||
           !scenery_make_history::live(r.memory,r.image,handle.object,handle.uid,out.sample.world.root,current->second.region,find,value,error)||
           !out.live.emplace(handle.uid,value).second||!out.objects.emplace(key,row).second)
            return q::fail(error,error.empty()?"Building clearance current Scenery owner/full UID repeats":error);
    }
    host::WorldState final_world;
    return world(r,final_world,error)&&
        (host::same_world(out.sample.world.origin,final_world.origin)||q::fail(error,"Building clearance complete saved Root read changed its World"));
}
bool closed_graph(Runtime& r,uintptr_t graph,std::string& error){
    witness::GraphWitness value;
    return witness::read_graph(r.memory.process,r.image,graph,value,error)&&
        ((value.locked&&!value.pending_delete)||q::fail(error,"Building clearance original graph is not locked and drained"));
}
bool graph(Runtime& r,uintptr_t owner,const std::map<uint64_t,Saved>& saved,bool makes,
           const std::map<uint64_t,Saved>& known,bool initial,std::string& error){
    std::set<uint64_t> all,destroy;
    for(const auto& [uid,value]:known){(void)value;all.insert(uid);}
    for(const auto& [uid,value]:saved){(void)value;all.insert(uid);if(!makes)destroy.insert(uid);}
    return closed_graph(r,owner,error)&&building_clearance_graph::read(r.memory,r.image,owner,
        makes?saved:std::map<uint64_t,Saved>{},destroy,all,initial,error);
}
bool authority_before(const Local& before,const action::Evidence* authority,std::string& error){
    if(!authority)return true;
    if(!authority->building_clearance)return q::fail(error,"Building clearance Client lacks the Host complete effect");
    const auto& effect=*authority->building_clearance;
    for(const auto& row:effect.removed){const auto found=before.objects.find(row.key);
        if(found==before.objects.end()||found->second!=row)return q::fail(error,"Building clearance Client original Record/pose differs key="+row.key);}
    for(const auto& row:effect.created)if(before.objects.contains(row.key))
        return q::fail(error,"Building clearance Host restored alias is already live key="+row.key);
    for(const auto& change:effect.changed){const auto found=before.objects.find(change.before.key);
        if(found==before.objects.end()||found->second!=change.before)return q::fail(error,"Building clearance related Group before differs");}
    return true;
}
}
bool capture(const action::Binding& binding,const action::Request& request,Snapshot& out,std::string& error,
             const action::Evidence* authority){
    out={};Runtime* r=nullptr;if(!runtime(r,binding,request,error))return false;
    if(request.action==action::Action::PlaceBuilding){
        bool scenery=false;scenery_kiss::NativeCatalog catalog(r->memory.process,r->image,r->thread);
        if(!catalog.classify_directory(request.building.type,scenery,error))return false;
        if(scenery){
            if(authority&&authority->building_clearance)return q::fail(error,"Scenery placement carries a Building clearance proof");
            return true;
        }
    }
    auto next=std::make_shared<Local>();next->binding=binding;next->request=request;
    if(!read(*r,binding,*next,error)||!authority_before(*next,authority,error))return false;
    out.local=std::move(next);return true;
}
bool prepare_history(const action::Binding& binding,const action::Request& request,const Snapshot& source,
                     Snapshot& before,std::string& error,const action::Evidence* authority){
    Runtime* r=nullptr;
    if(!runtime(r,binding,request,error)||!source.local||!source.local->finished||source.local->binding!=binding||
       source.local->request.action_uuid!=request.target_action_uuid||source.local->request.snapshot_uuid!=request.snapshot_uuid||
       source.local->request.canonical_record_key!=request.canonical_record_key)
        return q::fail(error,error.empty()?"Building clearance history lacks its confirmed source":error);
    if(!capture(binding,request,before,error,authority))return false;
    const bool restoring=request.action==action::Action::UndoBuilding;
    for(const auto& [uid,saved]:source.local->removed){
        const auto found=before.local->live.find(uid);
        if(restoring?found!=before.local->live.end():found==before.local->live.end())
            return q::fail(error,"Building clearance history live/retired full UID differs uid="+std::to_string(uid));
        if(!restoring&&(!scenery_stroke_history::same_inputs(found->second,saved)||
           before.local->objects.at(source.local->aliases.at(uid))!=source.local->original.at(source.local->aliases.at(uid))))
            return q::fail(error,"Building clearance history restored original Scenery differs");
    }
    uintptr_t consumed=0;
    return building_history_slice::graph_for(request.action,request.target_action_uuid,consumed,error)&&
        graph(*r,consumed,source.local->removed,restoring,before.local->live,false,error);
}
bool settle(const action::Binding& binding,const action::Request& request,const Snapshot& before,
            const Snapshot* source,uint64_t building_uid,uintptr_t inverse_graph,Snapshot& out,std::string& error){
    out={};Runtime* r=nullptr;
    if(!runtime(r,binding,request,error)||!before.local||before.local->binding!=binding||before.local->request!=request||!building_uid)
        return q::fail(error,error.empty()?"Building clearance settlement before receipt differs":error);
    auto next=std::make_shared<Local>();next->binding=binding;next->request=request;next->graph=inverse_graph;
    next->initial=source==nullptr;next->restoring=source&&request.action==action::Action::UndoBuilding;
    const auto& prior=*before.local;host::WorldState current;duplicate_reader::Census census;
    auto find=[&](uint64_t uid,uintptr_t& pointer,std::string& why){return group_edit::find(*r,uid,pointer,why);};
    if(!world(*r,current,error))return false;
    if(!host::same_world(prior.sample.world.origin,current.origin))return q::fail(error,"Building clearance World changed after original execution");
    if(!duplicate_reader::census(r->memory,r->image,uintptr_t(current.origin.map),current.root,find,census,error))return false;
    std::set<uint64_t> alive;std::map<uint64_t,std::string> previous;
    for(const auto& [pointer,row]:census.objects){(void)pointer;if(row.family==Family::Scenery)alive.insert(row.handle.uid);}
    for(const auto& [uid,saved]:prior.live){
        uintptr_t current_object=0;
        if(!find(uid,current_object,error))return false;
        if(current_object){
            Saved actual;
            if(!scenery_make_history::live(r->memory,r->image,current_object,uid,saved.root,saved.region,find,actual,error)||
               actual.object!=saved.object||!scenery_stroke_history::same_inputs(actual,saved))
                return q::fail(error,error.empty()?"Building clearance survivor full UID/owner/payload differs":error);
            alive.insert(uid);
        }else if(alive.contains(uid))return q::fail(error,"Building clearance census contains an unresolved full UID");
        const auto found=std::find_if(prior.sample.handles.begin(),prior.sample.handles.end(),[&](const auto& row){return row.second.uid==uid&&row.second.object==saved.object;});
        if(found==prior.sample.handles.end())return q::fail(error,"Building clearance before UID lacks exact canonical handle");
        previous.emplace(uid,found->first);
        if(!alive.contains(uid))next->removed.emplace(uid,saved);
    }
    if(source){
        if(!source->local||!source->local->finished)return q::fail(error,"Building clearance history source is incomplete");
        const auto& expected=source->local->removed;
        if(next->restoring)for(const auto& [uid,saved]:expected){
            uintptr_t current_object=0;
            if(!find(uid,current_object,error))return false;
            if(current_object){Saved actual;
                if(!scenery_make_history::live(r->memory,r->image,current_object,uid,saved.root,saved.region,find,actual,error)||
                   !scenery_stroke_history::same_inputs(actual,saved))return q::fail(error,error.empty()?"Building clearance restored saved owner/payload differs":error);
                alive.insert(uid);}
        }
        if(next->restoring){if(!next->removed.empty())return q::fail(error,"Building clearance Undo removed unrelated Scenery");}
        else if(next->removed.size()!=expected.size())return q::fail(error,"Building clearance Redo removed a different Scenery set");
        for(const auto& [uid,saved]:expected){
            if(next->restoring?!alive.contains(uid):!next->removed.contains(uid))
                return q::fail(error,"Building clearance history complete native UID set differs uid="+std::to_string(uid));
            if(!next->restoring&&!scenery_stroke_history::same_inputs(next->removed.at(uid),saved))
                return q::fail(error,"Building clearance Redo source native stored inputs differ");
        }
        next->removed=expected;next->aliases=source->local->aliases;next->original=source->local->original;
    }else for(const auto& [uid,saved]:next->removed){(void)saved;
        next->aliases.emplace(uid,previous.at(uid));next->original.emplace(previous.at(uid),prior.objects.at(previous.at(uid)));}
    for(auto uid:alive)if(!prior.live.contains(uid)&&(!next->restoring||!next->removed.contains(uid)))
        return q::fail(error,"Building clearance original operation created an unmatched Scenery UID="+std::to_string(uid));
    if(!graph(*r,inverse_graph,next->removed,!next->restoring,prior.live,next->initial,error))return false;
    auto retire=[&](const auto& ids,auto& keys,std::string& why){return remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,ids,keys,why);};
    auto restore=[&](uint64_t uid,const std::string& key,std::string& why){
        const auto expected=next->removed.find(uid);uintptr_t pointer=0;Saved actual;
        if(expected==next->removed.end()||!find(uid,pointer,why)||
           !scenery_make_history::live(r->memory,r->image,pointer,uid,expected->second.root,expected->second.region,find,actual,why)||
           !scenery_stroke_history::same_inputs(actual,expected->second))
            return q::fail(why,why.empty()?"Building clearance restored native full UID/owner/payload differs":why);
        return remove_diagnostics::restore_redone_building(request.snapshot_uuid,key,uid,why);
    };
    // The primary Building alias is admitted by the caller next. Its full
    // Record read is deferred until both sides of the native graph are bound.
    auto defer_records=[](std::string&){return true;};
    if(!scenery_alias_order::capture(previous,alive,next->restoring?next->aliases:std::map<uint64_t,std::string>{},
        retire,restore,defer_records,next->settlement,error))return false;
    out.local=std::move(next);return true;
}
bool finish(const action::Binding& binding,const action::Request& request,const Snapshot& before,
            Snapshot& after,const action::Evidence* authority,std::string& error){
    Runtime* r=nullptr;
    if(!runtime(r,binding,request,error)||!before.local||!after.local||after.local->request!=request||after.local->finished)
        return q::fail(error,error.empty()?"Building clearance full Record settlement receipt differs":error);
    auto& next=*after.local;
    if(!read(*r,binding,next,error))return false;
    if(!duplicate_edit::same_binding(before.local->sample,next.sample))return q::fail(error,"Building clearance Record settlement changed its complete World owners");
    std::vector<std::string> restored=next.settlement.restored;std::sort(restored.begin(),restored.end());
    if(!scenery_stroke_state::difference(before.local->objects,next.objects,restored,scenery_stroke::digest,next.effect,error,true))return false;
    const auto& affected=next.restoring?next.effect.created:next.effect.removed;
    if(affected.size()!=next.original.size()||!(next.restoring?next.effect.removed:next.effect.created).empty())
        return q::fail(error,"Building clearance canonical Record effect differs from original graph set");
    for(const auto& row:affected){const auto found=next.original.find(row.key);
        if(found==next.original.end()||found->second!=row)return q::fail(error,"Building clearance original/restored complete Record differs key="+row.key);}
    if(authority&&(!authority->building_clearance||*authority->building_clearance!=next.effect))
        return q::fail(error,"Building clearance Client complete effect differs from Host");
    next.finished=true;return true;
}
bool observe(const action::Binding& binding,const action::Request& request,const Snapshot& expected,
             action::SceneryStrokeEffect& out,std::string& error){
    Runtime* r=nullptr;
    if(!runtime(r,binding,request,error)||!expected.local||!expected.local->finished||expected.local->binding!=binding||expected.local->request!=request)
        return q::fail(error,error.empty()?"Building clearance observation lacks its exact confirmed receipt":error);
    auto current=std::make_unique<Local>();
    if(!read(*r,binding,*current,error)||!duplicate_edit::same_binding(expected.local->sample,current->sample)||
       current->objects!=expected.local->objects||current->live!=expected.local->live||current->sample.undo!=expected.local->sample.undo||
       !graph(*r,expected.local->graph,expected.local->removed,!expected.local->restoring,current->live,expected.local->initial,error))
        return q::fail(error,error.empty()?"Building clearance current full Records/UIDs/graph changed before ACK":error);
    out=expected.local->effect;return true;
}
bool publication(const Snapshot& value,const world_state::Origin& origin,std::vector<std::string>& retired,
                 std::vector<world_state::LocalBinding>& restored,std::string& error){
    retired.clear();restored.clear();
    if(!value.local||!value.local->finished)return q::fail(error,"Building clearance publication lacks complete original Record proof");
    retired=value.local->settlement.retired;
    for(const auto& key:value.local->settlement.restored){
        const auto handle=value.local->sample.handles.find(key);remove_diagnostics::LocalTarget target;native::BuildingState actual;world_state::LocalBinding row;
        if(handle==value.local->sample.handles.end()||
           !remove_diagnostics::resolve_identity_destination(origin.snapshot_uuid,key,target,error)||target.uid!=handle->second.uid||!target.normal_scenery||
           !scenery_kiss::read_action_prop(target.uid,actual,error)||
           !world_state::bind_root_readback(origin,key,target,actual,row,error))return false;
        restored.push_back(std::move(row));
    }
    return true;
}
bool current_bindings(const Snapshot& value,const world_state::Origin& origin,std::vector<world_state::LocalBinding>& out,std::string& error){
    out.clear();
    if(!value.local||value.local->request.snapshot_uuid!=origin.snapshot_uuid||
       value.local->binding!=action::Binding{origin.world_epoch,origin.generation})
        return q::fail(error,"Building clearance current projection has another snapshot/native binding");
    for(const auto& [key,row]:value.local->objects)if(row.family==Family::Scenery){
        const auto handle=value.local->sample.handles.find(key);const auto state=value.local->sample.props.find(key);
        if(handle==value.local->sample.handles.end()||state==value.local->sample.props.end()||!state->second.alive)
            return q::fail(error,"Building clearance current full Scenery state is absent key="+key);
        out.push_back({origin,key,handle->second.uid,state->second});
    }
    return true;
}
}
