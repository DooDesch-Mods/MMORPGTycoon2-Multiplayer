namespace zone_edit_runtime {
namespace {
struct LocalSelection {
    std::vector<std::pair<uintptr_t,uint64_t>> members;
    std::array<uint8_t,9> filters{};
    bool capture(Memory& memory,uintptr_t manager,std::string& error){
        std::vector<uintptr_t> raw,filtered;
        if(!group_reader::selection(memory,za::state.image,manager,raw,filtered,error)||!field(memory,manager,0xc8,filters,error))return false;
        for(auto value:filters)if(value>1)return fail(error,"Zone local selection filter is not Boolean");
        for(auto object:raw){uint64_t uid=0;uintptr_t actual=0;
            if(!field(memory,object,0x18,uid,error)||!find_uid(memory,uid,actual,error)||actual!=object)
                return fail(error,error.empty()?"Zone local selection full UID differs":error);
            members.emplace_back(object,uid);
        }
        return true;
    }
    bool restore(Memory& memory,uintptr_t manager,const std::set<uint64_t>& retired,std::string& error){
        std::vector<uintptr_t> desired,raw,filtered;
        for(const auto& [object,uid]:members){uintptr_t actual=0;if(!find_uid(memory,uid,actual,error))return false;
            if(actual==object)desired.push_back(object);
            else if(actual||!retired.contains(uid))return fail(error,"Zone original lost an unrelated local selection owner");
        }
        if(!group_reader::selection(memory,za::state.image,manager,raw,filtered,error))return false;
        if(raw!=desired){
            const duplicate_calls::Array array{za::state.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
            if(!za::native_ok(duplicate_calls::select(za::state.image,manager,array),"local selection restore",error)||
               !group_reader::selection(memory,za::state.image,manager,raw,filtered,error))return false;
        }
        std::array<uint8_t,9> current{};
        return field(memory,manager,0xc8,current,error)&&((raw==desired&&current==filters)||fail(error,"Zone local selection/filter readback differs"));
    }
};
struct Operation {
    a::Binding binding;a::Request request;
    Sample before,after;identity::Preview preview;
    a::Evidence evidence;
    Graph graph;std::map<uintptr_t,Graph> inverses;
    std::map<uint64_t,identity::Current> known_zones;
    std::unique_ptr<zone_edit_calls::Brush> brush;
    LocalSelection selection;
    zone_edit_calls::ExistingReceipt existing;
    uintptr_t born=0;uint64_t born_uid=0;
    int32_t region=-1;
    bool attempted=false,returned=false,confirmed=false,retired=false,remove_entered=false,remove_returned=false;
    bool rng_attempted=false,rng_returned=false;
    std::string cause;
};
std::map<std::string,Operation> operations,history_operations;
Operation* remove_replay=nullptr;
bool invoke_remove(Memory&,Operation&,std::string&);
const zone_edit_read::Row* selected(const Sample& sample,const std::string& key){
    const auto found=std::find_if(sample.rows.begin(),sample.rows.end(),[&](const auto& value){return value.owner.key==key;});
    return found==sample.rows.end()?nullptr:&*found;
}
bool validate_input(Memory& memory,const a::ZoneEditIntent& intent,const Sample& before,uintptr_t& window,
                    monster_zone_selection::Catalogue& catalogue,std::string& error){
    if(!a::valid_zone_edit_intent(intent))return fail(error,"Zone requested input is outside its native domain");
    if(intent.kind==Kind::Stroke){uint8_t owned=0;
        if(!field(memory,before.edit.world.region,0x850,owned,error)||owned!=1)
            return fail(error,error.empty()?"Zone stroke requires the original owned Region":error);}
    const auto* target=intent.selected.empty()?nullptr:selected(before,intent.selected);
    if(!intent.selected.empty()&&!target)return fail(error,"Zone requested canonical target is absent from this Region");
    if(intent.kind==Kind::Stroke&&target&&target->owner.type!=intent.zone_type)
        return fail(error,"Zone stroke selected target differs from the original tool type");
    if(intent.kind==Kind::Apply&&target->owner.type!=1)return fail(error,"Zone Apply requires original Monster Zone controls");
    if(intent.kind==Kind::Stroke||intent.kind==Kind::Apply){
        int32_t minimum=0,maximum=0;
        if(!field(memory,before.edit.world.region,0x838,minimum,error)||!field(memory,before.edit.world.region,0x83c,maximum,error)||
           minimum<1||maximum<minimum)return fail(error,error.empty()?"Zone current native Region level range differs":error);
        if(intent.level<minimum||intent.level>maximum)return fail(error,"Zone requested level is outside current native Region bounds");
        d::ReadMemory bounded{memory};
        if(!monster_zone_selection::read(bounded,za::state.image,intent.character_type,catalogue,error))return false;
        if(intent.kind==Kind::Stroke&&!za::theme_owner(memory,window,error))return false;
    }
    if(intent.kind==Kind::Stroke)for(const auto& stamp:intent.stamps){
        uintptr_t region=0;const auto exception=shard_region(za::state.image,before.world.root,stamp.position,region);
        if(!za::native_ok(exception,"stamp Region",error)||region!=before.edit.world.region)
            return fail(error,error.empty()?"Zone stamp belongs to another current Region":error);
    }
    return true;
}
bool run_stroke(Memory& memory,Operation& operation,uintptr_t window,const monster_zone_selection::Catalogue& catalogue,std::string& error){
    const auto& intent=operation.request.zone_edit;
    const auto* target=intent.selected.empty()?nullptr:selected(operation.before,intent.selected);
    operation.brush=std::make_unique<zone_edit_calls::Brush>();auto& brush=*operation.brush;
    if(!za::native_ok(zone_edit_calls::construct(za::state.image,brush,operation.before.edit.world.region,window,
        target?target->owner.object:0,intent.zone_type,intent.level,intent.add,intent.press.data()),"private edit brush construction",error))return false;
    for(const auto& stamp:intent.stamps){
        poly::BorrowedPaths paths(stamp.shape);poly::Storage shape;bool made=false;
        bool okay=za::native_ok(za::shape_make(shape,paths.view,made),"edit shape construction",error);
        a::TerrainShape actual;
        if(okay)okay=poly::read(memory,uintptr_t(&shape),actual,error)&&actual==stamp.shape;
        if(okay){
            monster_zone_selection::Binding bound{uintptr_t(brush.bytes.data()),window,catalogue.character,za::state.image,za::state.thread};
            monster_zone_selection::Scope scope(za::state.selection,bound);
            monster_zone_selection::PaintScope paint(za::state.private_brush,za::state.inside_paint,bound.brush);
            operation.attempted=true;
            okay=za::native_ok(zone_edit_calls::paint(brush,za::state.paint,stamp.position.data(),&shape),"edit AddPaint",error);
        }
        const auto destroyed=made?za::unary(reinterpret_cast<za::Unary>(za::state.image+0xf15a0),&shape):0;
        if(destroyed){error+="; Zone shape cleanup exception="+std::to_string(destroyed);return false;}
        if(!okay)return fail(error,error.empty()?"Zone private original shape differs":error);
    }
    uintptr_t object=0;if(!field(memory,uintptr_t(brush.bytes.data()),0x710,object,error))return false;
    if(object&&!operation.before.objects.contains(object)){
        operation.born=object;
        if(!field(memory,object,0x18,operation.born_uid,error)||!operation.born_uid)return fail(error,error.empty()?"Zone original birth full UID absent":error);
    }
    if(!za::native_ok(zone_edit_calls::finish(brush,za::state.finished),"edit PaintingFinished",error))return false;
    operation.returned=true;
    uintptr_t retained=0;
    if(!field(memory,uintptr_t(brush.bytes.data()),0x740,retained,error)||retained)return fail(error,error.empty()?"Zone original Finish retained its graph":error);
    return za::native_ok(zone_edit_calls::destroy(za::state.image,brush),"edit private brush destructor",error);
}
bool remember_zones(Operation& operation,std::string& error){
    for(const auto* value:{&operation.before,&operation.after})for(const auto& row:value->rows){
        const auto found=operation.known_zones.find(row.owner.uid);
        if(found!=operation.known_zones.end()&&(found->second.key!=row.owner.key||found->second.type!=row.owner.type))
            return fail(error,"Zone history full UID reused another canonical owner");
        operation.known_zones[row.owner.uid]=row.owner;
    }
    return true;
}
bool quest_transition(Memory& memory,const Operation& operation,const Graph* consumed,std::string& error){
    if(operation.request.action==a::Action::EditZone&&operation.request.zone_edit.kind==Kind::Respawn)
        return qr::unchanged(operation.before.quests,operation.after.quests,error)&&
            ((operation.before.references==operation.after.references)||fail(error,"Zone Respawn changed unrelated Quest references"));
    if(consumed){
        std::vector<zone_quest_history::Step> execution;
        for(auto pointer:consumed->native.execution){
            const auto found=std::find_if(consumed->native.nodes.begin(),consumed->native.nodes.end(),
                [&](const auto& node){return node.pointer==pointer;});
            if(found==consumed->native.nodes.end())return fail(error,"Zone history Quest execution node is absent from its retained graph");
            const auto& node=*found;
            if(node.kind==cg::Kind::Cash)continue;
            const auto key=consumed->keys.find(node.uid);
            if(key==consumed->keys.end())return fail(error,"Zone history Quest execution full UID lacks its canonical owner");
            zone_quest_history::Step step;step.key=key->second;step.uid=node.uid;
            switch(node.kind){
            case cg::Kind::Zone:case cg::Kind::Monster:step.kind=zone_quest_history::Kind::Make;break;
            case cg::Kind::EditZone:step.kind=zone_quest_history::Kind::EditZone;break;
            case cg::Kind::Destroy:
                step.kind=zone_quest_history::Kind::Destroy;
                for(const auto& row:operation.before.rows)if(row.owner.uid==node.uid)step.receiver=row.owner.object;
                for(const auto& actor:operation.before.native.state.monsters)if(actor.uid==node.uid){
                    if(step.receiver&&step.receiver!=actor.pointer)return fail(error,"Zone history Destroy full UID has conflicting current receivers");
                    step.receiver=actor.pointer;
                }
                break;
            case cg::Kind::Quests:
                step.kind=zone_quest_history::Kind::Quests;
                for(const auto& [receiver,owner]:operation.before.quests.owners)if(owner.uid==node.uid&&owner.key==step.key){
                    if(step.receiver)return fail(error,"Zone history SetNPCQuests full UID has duplicate current owners");
                    step.receiver=receiver;
                }
                if(!step.receiver)return fail(error,"Zone history SetNPCQuests lacks its seeded current owner");
                break;
            default:return fail(error,"Zone history Quest execution contains a foreign node family");
            }
            execution.push_back(std::move(step));
        }
        qr::Witness retired;
        return zone_quest_history::verify(consumed->quests,operation.before.quests,operation.after.quests,execution,
            operation.before.references,operation.after.references,retired,error)&&qr::absent(memory,za::state.image,retired,error);
    }
    std::set<std::string> changed;
    for(const auto& node:operation.graph.native.nodes)if(node.kind==cg::Kind::Quests)changed.insert(operation.graph.keys.at(node.uid));
    if(!duplicate_quest_history::unchanged_owners(operation.before.quests,operation.after.quests,changed,{}, {},error))return false;
    if(!consumed){
        // Make nodes are appended by the original destruction path. Their
        // insertion order records the corresponding OnDestroy callback order.
        std::vector<uintptr_t> removed;
        for(const auto& node:operation.graph.native.nodes)if(node.kind==cg::Kind::Zone||node.kind==cg::Kind::Monster){
            uintptr_t actual=0;if(!find_uid(memory,node.uid,actual,error))return false;
            if(actual)continue;
            if(node.kind==cg::Kind::Zone){const auto found=operation.graph.zones.find(node.uid);
                if(found==operation.graph.zones.end())return fail(error,"Zone retired graph target lacks prior owner");removed.push_back(found->second.object);}
            else{const auto found=operation.graph.monsters.find(node.uid);
                if(found==operation.graph.monsters.end())return fail(error,"Zone retired Monster graph target lacks prior owner");removed.push_back(found->second.pointer);}
        }
        qr::Witness expected;
        if(!duplicate_quest_history::removal_order(operation.before.quests,removed,expected,error)||
           !qr::survivors(expected,operation.after.quests,error)||!qr::absent(memory,za::state.image,expected,error)||
           !quest_cascade_read::references(expected,operation.before.references,operation.after.references,error))return false;
    }
    return true;
}
bool finish_result(Memory& memory,Operation& operation,const a::Evidence* authority,uintptr_t inverse,uint32_t cash,
                   const std::map<uint64_t,identity::Current>& restored,std::string& error,const Graph* consumed=nullptr){
    if(!identity::preview_edit(operation.before.edit,operation.born,operation.born_uid,restored,operation.preview,error)||
       !sample(memory,operation.binding,operation.request.snapshot_uuid,operation.region,operation.request.action_uuid,operation.after,error,
               &operation.before.edit,&operation.preview,&operation.before.quests))return false;
    if(!action_runtime::same_world(operation.before.world,operation.after.world))
        return fail(error,"Zone original changed current complete World ownership");
    if(operation.before.money!=operation.after.money)return fail(error,"Zone original zero-cost operation changed the complete Ledger");
    a::ZoneEditEffect effect{operation.before.value,operation.after.value,{},0};
    if(inverse){
        if(!graph_capture(memory,operation.before,operation.after,inverse,cash,operation.graph,error))return false;
        for(const auto& node:operation.graph.native.nodes)if(node.kind==cg::Kind::Cash&&
            (node.values.empty()||node.values.front()!="0"))return fail(error,"Zone original inverse Cash differs from its zero-cost tool");
        effect.inverse_sha=operation.graph.sha;effect.inverse_operations=uint32_t(operation.graph.native.nodes.size());
    }else if(operation.before.rows!=operation.after.rows||operation.before.monster_keys!=operation.after.monster_keys||
             operation.before.money!=operation.after.money||operation.before.native.undo!=operation.after.native.undo)
        return fail(error,"Zone Respawn changed authored ownership/Ledger/history");
    if(!quest_transition(memory,operation,consumed,error)||!remember_zones(operation,error))return false;
    operation.evidence.before=operation.before.money;operation.evidence.after=operation.after.money;
    operation.evidence.zone_edit=effect;operation.evidence.undo_confirmed=inverse!=0;
    if(!a::valid_zone_edit_evidence(operation.request,operation.evidence)||
       (authority&&(authority->zone_edit!=operation.evidence.zone_edit||authority->after!=operation.evidence.after)))
        return fail(error,"Zone original complete effect/inverse/Ledger differs from Host");
    std::set<uint64_t> retired;
    for(const auto& [uid,key]:operation.before.keys){(void)key;if(!operation.after.keys.contains(uid))retired.insert(uid);}
    for(const auto& [uid,key]:operation.before.monster_keys){(void)key;if(!operation.after.monster_keys.contains(uid)){
        uintptr_t actual=0;if(!find_uid(memory,uid,actual,error)||actual)return fail(error,error.empty()?"Zone retired Monster full UID remains registered":error);
        retired.insert(uid);
    }}
    if(!operation.selection.restore(memory,operation.before.world.selection,retired,error)||
       !identity::publish_edit(operation.before.edit,operation.preview,error))return false;
    Sample confirmed;
    if(!sample(memory,operation.binding,operation.request.snapshot_uuid,operation.region,operation.request.action_uuid,confirmed,error,
        nullptr,nullptr,&operation.after.quests)||!same(operation.after,confirmed))
        return fail(error,error.empty()?"Zone complete effect changed at canonical publication":error);
    if(inverse)operation.inverses.emplace(inverse,operation.graph);
    operation.confirmed=true;return true;
}
bool observed(Memory& memory,const Operation& operation,a::Evidence& out,std::string& error){
    Sample current;
    if(!sample(memory,operation.binding,operation.request.snapshot_uuid,operation.region,operation.request.action_uuid,current,error,
        nullptr,nullptr,&operation.after.quests)||!same(operation.after,current)||
       (operation.graph.native.pointer&&!graph_unchanged(memory,operation.graph,error)))
        return fail(error,error.empty()?"Zone complete confirmed state/inverse changed before ACK":error);
    out=operation.evidence;out.retirement_confirmed=operation.retired;return true;
}
void clear_operations(){operations.clear();history_operations.clear();remove_replay=nullptr;}
json operation_status(){
    json result=json::object();
    const auto append=[&](const auto& source){for(const auto& [id,operation]:source)result[id]={
        {"attempted",operation.attempted},{"returned",operation.returned},{"confirmed",operation.confirmed},
        {"retired",operation.retired},{"rng_attempted",operation.rng_attempted},{"rng_returned",operation.rng_returned},{"cause",operation.cause}};};
    append(operations);append(history_operations);return result;
}
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    uint64_t revision=0;
    if(!ready||capture_fault||za::state.thread!=GetCurrentThreadId()||request.action!=a::Action::EditZone||
       request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       !ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="Zone edit requires exact current pending Mainthread/role";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Zone edit original already entered; replay forbidden";return a::Outcome::Unknown;}
    Memory memory;Sample before;uintptr_t window=0;monster_zone_selection::Catalogue catalogue;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,request.zone_edit.region,request.action_uuid,before,error)||
       !validate_input(memory,request.zone_edit,before,window,catalogue,error))return a::Outcome::RejectedNoEffect;
    if(authority){
        auto comparison=before.value;comparison.global_random=authority->zone_edit?authority->zone_edit->before.global_random:std::array<uint64_t,4>{};
        if(!a::valid_zone_edit_evidence(request,*authority)||comparison!=authority->zone_edit->before||before.money!=authority->before){
            error="Zone Client complete before state/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    }
    if(!a::zone_edit_wire_fits(request,before.money,before.value,error))return a::Outcome::RejectedNoEffect;
    auto& operation=operations.try_emplace(request.action_uuid).first->second;operation.binding=binding;operation.request=request;operation.region=request.zone_edit.region;
    const auto unknown=[&](){replaying=false;operation.cause=error.empty()?"Zone original failed without cause":error;error=operation.cause;return a::Outcome::Unknown;};
    if(authority){
        const auto region_random=before.value.region_random;
        operation.rng_attempted=true;
        if(!za::native_ok(action_runtime::seed(za::state.image,authority->zone_edit->before.global_random),"edit Host RNG adoption",error))return unknown();
        operation.rng_returned=true;
        if(!sample(memory,binding,request.snapshot_uuid,operation.region,request.action_uuid,before,error)||
           before.value.region_random!=region_random||before.value!=authority->zone_edit->before)
            return unknown();
    }
    operation.before=std::move(before);
    if(!operation.selection.capture(memory,operation.before.world.selection,error)){operation.cause=error;return a::Outcome::RejectedNoEffect;}
    replaying=true;bool success=false;
    if(request.zone_edit.kind==Kind::Stroke)success=run_stroke(memory,operation,window,catalogue,error);
    else if(request.zone_edit.kind==Kind::Delete)success=invoke_remove(memory,operation,error);
    else{
        const auto* target=selected(operation.before,request.zone_edit.selected);
        if(!target){error="Zone original target disappeared before invocation";return unknown();}
        operation.attempted=true;
        if(request.zone_edit.kind==Kind::Respawn)success=za::native_ok(zone_edit_calls::respawn(za::state.image,target->owner.object,operation.attempted,operation.returned),"Respawn",error);
        else{
            success=za::native_ok(zone_edit_calls::existing(za::state.image,target->owner.object,request.zone_edit.kind==Kind::Apply,
                request.zone_edit.character_type,request.zone_edit.density,request.zone_edit.level,operation.existing),"Apply/Reset",error);
            operation.returned=operation.existing.close_returned;
        }
    }
    replaying=false;if(!success||!operation.returned)return unknown();
    uintptr_t inverse=0;uint32_t cash=0;
    if(request.zone_edit.kind!=Kind::Respawn){
        undo_probe::UndoWitness undo;undo_probe::GraphWitness graph;
        if(!undo_probe::read_undo(memory.process,za::state.image,undo,error))return unknown();
        if(undo==operation.before.native.undo){
            if(authority||!identity::preview_edit(operation.before.edit,operation.born,operation.born_uid,{},operation.preview,error)||
               !sample(memory,binding,request.snapshot_uuid,operation.region,request.action_uuid,operation.after,error,
                    &operation.before.edit,&operation.preview,&operation.before.quests)||!same(operation.before,operation.after)){
                if(error.empty())error="Zone original omitted history but changed state or disagrees with confirmed Host";return unknown();}
            error=operation.cause="Zone original produced no authored or history effect";return a::Outcome::RejectedNoEffect;
        }
        if(undo.undo.empty()||!undo_probe::read_graph(memory.process,za::state.image,undo.undo.back(),graph,error)||
           !undo_probe::validate_undo_commit(operation.before.native.undo,undo,graph,error))return unknown();
        inverse=undo.undo.back();cash=request.zone_edit.kind==Kind::Stroke?1u:0u;
    }
    if(!finish_result(memory,operation,authority,inverse,cash,{},error))return unknown();
    return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;error=std::string("Zone original execution boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){replaying=false;error="Zone original execution non-standard boundary exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=za::state.thread||found==operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed)return fail(error,"Zone observation lacks exact confirmed original receipt");
    Memory memory;return observed(memory,found->second,out,error);
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){
    auto out=std::make_unique<a::Evidence>();if(!observe(binding,request,*out,error))return false;auto& operation=operations.at(request.action_uuid);
    if(operation.retired)return fail(error,"Zone original receipt already retired");
    if(operation.graph.native.pointer&&!building_history_slice::remember_zone_edit(request.action_uuid,operation.graph.native.pointer,error))return false;
    operation.retired=true;return true;
}
}
