namespace dungeon_input {
namespace {
using ControlCommand=dungeon_gizmo_control::Command;
bool control_ui_active=false,control_ui_failed=false;
std::map<uintptr_t,uint64_t> control_ui_owners;
std::set<std::string> control_local_sources;
struct ControlPort {
    Memory& memory;const a::Binding& binding;const a::Request& request;const Sample& before;
    const dungeon_scene_identity_runtime::Pool* pool=nullptr;
    std::vector<duplicate_native::Handle> sources;
    std::vector<dungeon_gizmo_identity::Member> created;
    std::vector<bool> created_scenery;
    std::vector<uintptr_t> prior_selection,new_selection;
    std::vector<uint64_t> prior_uids;
    uintptr_t selection=0;bool edit_open=false,duplicate_committed=false;
    move_kiss::Receipt single_move;selection_move_native::Receipt multiple_move;
    const auto& intent()const{return *request.dungeon_input.controls;}
    const auto& native()const{return before.source.palette.dungeon.dungeon.dungeon;}
    bool fault(uint32_t value,const char* operation,std::string& error){return original_result(value,operation,error);}
    bool find(uint64_t uid,uintptr_t& object,std::string& error){host::LookupApi api;
        if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
        return fault(lookup(api.find,uid,object),"Gizmo control full UID lookup",error);}
    bool preflight(std::string& error){
        for(const auto& pin:dungeon_gizmo_live_codes::values){std::vector<unsigned char> bytes(pin.bytes);
            if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
               !hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
                return fail(error,"Gizmo control original helper differs RVA="+std::to_string(pin.rva)+" cause="+error);}
        pool=selected(before,request.dungeon_input);bool offline=false;int32_t instances=-1;
        if(dungeon_gizmo_control::mixed(intent())&&intent().command==ControlCommand::Duplicate&&
           (!before.value.scenery_release||before.value.scenery_release->at(2)==INT32_MAX))return fail(error,"Scene Scenery original Release edit counter exhausted before effect");
        if(!pool||pool->scene.owner.dungeon!=native().local.dungeon||
           !fault(dungeon_gizmo_control_calls::offline(state.image,native().local.dungeon,offline),"Dungeon IsOffline",error)||
           !field(memory,native().local.dungeon,0x4c8,instances,error))return false;
        if(!offline||instances!=0)return fail(error,"Gizmo editing requires the original offline Dungeon with zero Instances");
        if(native().undo.active_graph)return fail(error,"Gizmo control requires an idle original Undo graph");
        int32_t blocked=0;if(!field(memory,uintptr_t(native().world.origin.mode),0x30c,blocked,error))return false;
        if(blocked>0)return fail(error,"Gizmo control original in-game mutation gate is active");
        if(!sources.empty())return true;
        for(const auto& scene:before.source.scenes.pools)for(const auto& member:scene.local.members)control_ui_owners.emplace(member.object,member.uid);
        if(!field(memory,state.image,0x13df630,selection,error))return false;
        dungeon_cells::MonsterMemory adapter{memory};std::vector<uintptr_t> filtered;
        if(!group_reader::selection(adapter,state.image,selection,prior_selection,filtered,error))return false;
        for(const auto pointer:prior_selection){uint64_t uid=0;uintptr_t current=0;
            if(!field(memory,pointer,0x18,uid,error)||!find(uid,current,error)||current!=pointer)
                return fail(error,error.empty()?"Gizmo control prior local Selection full UID differs":error);
            prior_uids.push_back(uid);}
        for(const auto& row:intent().rows){dungeon_gizmo_identity::Member member;uintptr_t current=0;int32_t price=-1;bool allowed=false;
            if(row.scenery){
                bool found=false;
                for(const auto& actual:pool->scenery.members){std::string key;
                    if(!remove_diagnostics::scene_scenery::resolve(before.source.scenes.world,actual.object,actual.uid,key,error))return false;
                    if(key!=row.key)continue;
                    if(found)return fail(error,"Scene Scenery control key repeats");found=true;
                    dungeon_gizmo_control::SceneryRow record;if(!scenery_record(memory,before.source.scenes,actual.object,actual.uid,record,error)||record.value!=*row.scenery)
                        return fail(error,error.empty()?"Scene Scenery current source record differs":error);
                    member={request.dungeon_input.scene,actual.object,pool->managers[0],pool->scene.owner.shard,pool->scene.owner.region,actual.uid};
                }
                if(!found||!find(member.uid,current,error)||current!=member.object)return fail(error,error.empty()?"Scene Scenery selected full UID absent":error);
                dungeon_cells::MonsterMemory adapter{memory};
                if(!duplicate_code::check(adapter,state.image,error)||!duplicate_code::target(adapter,state.image,member.object,error))return false;
                uintptr_t method=0;
                if(!field(memory,state.image+0x16c1390,0xd0,method,error)||method!=state.image+0xcf3130)
                    return fail(error,error.empty()?"Scene Scenery original CanMove virtual differs":error);
                if(!fault(dungeon_gizmo_control_calls::capability(member.object,intent().command==ControlCommand::Duplicate,price,allowed),"Scene Scenery CanMove/CanDuplicate/Cost",error))return false;
                if((intent().command!=ControlCommand::Destroy&&!allowed)||price<0||(intent().command!=ControlCommand::Move&&price!=row.cost))return fail(error,"Scene Scenery original capability/price changed");
                std::array<uint32_t,3> point{row.after[4],row.after[5],row.after[6]};uintptr_t destination=0;
                if(!fault(duplicate_calls::region(state.image,pool->scene.owner.shard,true,point,destination),"Scene Scenery destination RegionAt",error)||destination!=pool->scene.owner.region)
                    return fail(error,error.empty()?"Scene Scenery destination left its current scene":error);
                sources.push_back({member.object,member.uid});control_ui_owners.emplace(member.object,member.uid);continue;
            }
            if(!dungeon_scene_identity_runtime::lookup(row.key,member,error)||member.scene!=request.dungeon_input.scene||
               !find(member.uid,current,error)||current!=member.object)return fail(error,error.empty()?"Gizmo control source full UID/scene differs":error);
            const auto found=std::find_if(pool->local.members.begin(),pool->local.members.end(),[&](const auto& value){return value.object==member.object&&value.uid==member.uid;});
            if(found==pool->local.members.end()||pool->gizmos[size_t(found-pool->local.members.begin())]!=row.before)
                return fail(error,"Gizmo control complete current source record differs");
            for(const auto& slot:std::array<std::pair<size_t,uintptr_t>,6>{{{0xd0,0xe470c0},{0xd8,0xe46ff0},{0xe8,0x144670},{0x160,0x146870},{0x278,0xe47040},{0x280,0xe3f3b0}}}){
                uintptr_t method=0;if(!field(memory,state.image+0x1749ab0,slot.first,method,error)||method!=state.image+slot.second)
                    return fail(error,error.empty()?"Gizmo control original virtual method differs":error);}
            if(!fault(dungeon_gizmo_control_calls::capability(member.object,intent().command==ControlCommand::Duplicate,price,allowed),"Gizmo CanMove/CanDuplicate/Cost",error))return false;
            if((intent().command!=ControlCommand::Destroy&&!allowed)||price<0||
               (intent().command!=ControlCommand::Move&&price!=row.cost))return fail(error,"Gizmo control original capability or price changed");
            std::array<uint32_t,3> point{row.after[4],row.after[5],row.after[6]};uintptr_t destination=0;
            if(!fault(duplicate_calls::region(state.image,pool->scene.owner.shard,false,point,destination),"Gizmo destination RegionAt",error)||destination!=pool->scene.owner.region)
                return fail(error,error.empty()?"Gizmo control destination left its original current scene Region":error);
            sources.push_back({member.object,member.uid});control_ui_owners.emplace(member.object,member.uid);
        }
        if(intent().command==ControlCommand::Duplicate){bool affordable=false;
            if(!fault(duplicate_calls::afford(state.image,native().business,dungeon_gizmo_control::total(intent()),affordable),"Gizmo duplicate CanAfford",error)||!affordable)
                return fail(error,error.empty()?"Gizmo duplicate original Business cannot afford complete collection":error);}
        return true;
    }
    bool ready(const a::DuplicateIntent&,const std::vector<duplicate_native::Handle>& handles,int32_t total,std::string& error){
        return handles==sources&&total==dungeon_gizmo_control::total(intent())||fail(error,"Gizmo duplicate native adapter source/cost differs");}
    bool charge(int32_t amount,std::string& error){bool paid=false;return fault(duplicate_calls::charge(state.image,native().business,amount,paid),"Gizmo duplicate SubtractCash",error)&&
        (paid||fail(error,"Gizmo duplicate original payment returned false"));}
    bool begin_edit(bool& opened,std::string& error){uint32_t cleanup=0;
        if(!fault(duplicate_calls::begin(state.image,opened,cleanup),"Gizmo duplicate BeginEdit",error))return false;
        edit_open=opened;return fault(cleanup,"Gizmo duplicate BeginEdit label cleanup",error);}
    bool end_edit(std::string& error){return fault(duplicate_calls::end(state.image),"Gizmo duplicate EndEdit",error);}
    bool create_graph(uintptr_t& graph,std::string& error){return fault(duplicate_calls::graph(state.image,graph),"Gizmo duplicate DoGraph",error)&&
        (graph||fail(error,"Gizmo duplicate original graph allocation returned null"));}
    bool cash_inverse(uintptr_t graph,int32_t amount,const std::array<uint32_t,3>& point,std::string& error){uintptr_t inverse=0;
        return fault(duplicate_calls::cash(state.image,graph,amount,point,inverse),"Gizmo duplicate Cash inverse",error)&&
            (inverse||fail(error,"Gizmo duplicate original Cash allocation returned null"));}
    bool duplicate(const duplicate_native::Handle& source,const a::DuplicateTarget& target,duplicate_native::Handle& out,std::string& error){
        a::DungeonGizmoRecord pose;dungeon_gizmo_control::pose(pose,target.after);input_calls::Transform transform;
        if(!fault(input_calls::transform(state.image,pose.rotation,pose.position,pose.scale,transform),"Gizmo duplicate Transform",error)||
           !fault(duplicate_calls::duplicate(source.object,transform.bytes,out.object),"Gizmo original Duplicate",error)||!out.object)return false;
        const auto position=std::find_if(sources.begin(),sources.end(),[&](const auto& handle){return handle==source;});
        if(position==sources.end())return fail(error,"Scene control Duplicate source is absent");
        const bool scenery=intent().rows[size_t(position-sources.begin())].scenery.has_value();
        uintptr_t current=0,ap=0,shard=0,region=0;
        if(!field(memory,out.object,0,ap,error)||ap!=state.image+(scenery?0x16c1390:0x1749ab0)||!field(memory,out.object,0x18,out.uid,error)||!out.uid||
           !field(memory,out.object,0x170,shard,error)||!field(memory,out.object,0x178,region,error)||
           shard!=pool->scene.owner.shard||region!=pool->scene.owner.region||!find(out.uid,current,error)||current!=out.object)
            return fail(error,error.empty()?"Gizmo duplicate returned a foreign full UID/class/reciprocal scene owner":error);
        created.push_back({request.dungeon_input.scene,out.object,pool->managers[scenery?0:3],shard,region,out.uid});created_scenery.push_back(scenery);
        control_ui_owners.emplace(out.object,out.uid);return true;
    }
    bool destroy_inverse(uintptr_t graph,const duplicate_native::Handle& target,std::string& error){uintptr_t inverse=0;
        return fault(duplicate_calls::destroy_inverse(state.image,graph,target.object,inverse),"Gizmo duplicate Destroy inverse",error)&&
            (inverse||fail(error,"Gizmo duplicate original Destroy allocation returned null"));}
    bool select_created(const std::vector<duplicate_native::Handle>& handles,std::string& error){
        for(const auto& handle:handles)new_selection.push_back(handle.object);
        const duplicate_calls::Array array{state.image+0x1748c50,new_selection.data(),int32_t(new_selection.size()),int32_t(new_selection.size())};
        return fault(duplicate_calls::select(state.image,selection,array),"Gizmo duplicate SetSelection",error);}
    bool release_points(const a::DuplicateIntent&,const std::vector<duplicate_native::Handle>&,std::string& error){
        if(!dungeon_gizmo_control::mixed(intent()))return true;
        uintptr_t manager=0,release=0;
        if(!field(memory,state.image,0x13df4f0,manager,error)||!field(memory,manager,0x38,release,error)||!release)return false;
        return fault(duplicate_calls::release(state.image,release+0x108,2),"Scene Scenery Duplicate AddEdit",error);
    }
    bool commit_graph(uintptr_t graph,const std::vector<duplicate_native::Handle>&,std::string& error){uint32_t cleanup=0;
        const duplicate_calls::Array array{state.image+0x1748c50,new_selection.data(),int32_t(new_selection.size()),int32_t(new_selection.size())};
        if(!fault(duplicate_calls::commit(state.image,native().undo.receiver,graph,array,cleanup),"Gizmo duplicate AddUndo",error))return false;
        if(!fault(cleanup,"Gizmo duplicate Undo label cleanup",error))return false;duplicate_committed=true;return true;}
    bool readback(const a::DuplicateIntent&,const std::vector<duplicate_native::Handle>&,const duplicate_native::Receipt& receipt,std::string& error){
        return receipt.created.size()==intent().rows.size()||fail(error,"Gizmo duplicate complete original return count differs");}
    bool restore_selection(std::string& error){
        std::vector<uintptr_t> desired;
        for(size_t i=0;i<prior_selection.size();++i){uintptr_t current=0;if(!find(prior_uids[i],current,error))return false;
            const bool retired=intent().command==ControlCommand::Destroy&&std::any_of(sources.begin(),sources.end(),[&](const auto& value){return value.uid==prior_uids[i];});
            if(retired){if(current)return fail(error,"Gizmo control removed Selection full UID remains live");continue;}
            if(current!=prior_selection[i])return fail(error,"Gizmo control prior Selection lifetime changed");desired.push_back(current);}
        if(duplicate_committed&&control_local_sources.contains(request.action_uuid))desired=new_selection;
        const duplicate_calls::Array array{state.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
        if(!fault(duplicate_calls::select(state.image,selection,array),"Gizmo control restore local Selection",error))return false;
        dungeon_cells::MonsterMemory adapter{memory};std::vector<uintptr_t> actual,filtered;
        return group_reader::selection(adapter,state.image,selection,actual,filtered,error)&&
            (actual==desired||fail(error,"Gizmo control local Selection readback differs"));
    }
    using SourceMemory=Memory;
    struct GizmoMoveMemory final:move_kiss::Memory{SourceMemory& memory;explicit GizmoMoveMemory(SourceMemory& value):memory(value){}
        bool read(uintptr_t p,void* out,size_t bytes,std::string& error)override{return memory.copy(p,out,bytes,error);}};
    bool move(uintptr_t& graph,std::string& error){GizmoMoveMemory adapter(memory);move_kiss::Calls calls;
        if(!move_kiss::resolve_calls(adapter,state.image,exe_sha,calls,error))return false;
        std::vector<selection_move_native::Target> targets;
        for(size_t i=0;i<sources.size();++i){a::DungeonGizmoRecord desired;dungeon_gizmo_control::pose(desired,intent().rows[i].after);
            input_calls::Transform transform;if(!fault(input_calls::transform(state.image,desired.rotation,desired.position,desired.scale,transform),"Gizmo Move Transform",error))return false;
            targets.push_back({sources[i].object,transform.bytes});}
        bool result=false;
        if(targets.size()==1){result=move_kiss::execute_once(calls,targets[0].object,targets[0].pose,single_move,error);graph=single_move.graph;}
        else{result=selection_move_native::execute(calls,targets,multiple_move,error);graph=multiple_move.scope.graph;}
        return result;
    }
    bool destroy(uintptr_t& graph,std::string& error){
        std::vector<uintptr_t> objects;for(const auto& handle:sources)objects.push_back(handle.object);
        duplicate_calls::Array targets{state.image+0x1748c50,objects.data(),int32_t(objects.size()),int32_t(objects.size())};
        if(!fault(dungeon_gizmo_control_calls::destroy(state.image,selection,targets),"Gizmo original DestroyProps",error))return false;
        undo_probe::UndoWitness after;if(!undo_probe::read_undo(memory.process,state.image,after,error))return false;
        if(after.undo.empty())return fail(error,"Gizmo Destroy original Undo stack did not retain a graph");
        graph=after.undo.back();return restore_selection(error);
    }
    bool complete(uintptr_t graph,std::string& error){
        if(control_ui_failed)return fail(error,"Gizmo control original UI preparation emitted a changed or foreign value");
        undo_probe::UndoWitness after;undo_probe::GraphWitness committed;
        return undo_probe::read_undo(memory.process,state.image,after,error)&&undo_probe::read_graph(memory.process,state.image,graph,committed,error)&&
            undo_probe::validate_undo_commit(native().undo,after,committed,error);
    }
};
}
bool preparing_gizmo_ui(){return control_ui_active;}
bool gizmo_ui_owner(uintptr_t object,uint64_t uid){const auto found=control_ui_owners.find(object);return found!=control_ui_owners.end()&&found->second==uid;}
void reject_gizmo_ui(){control_ui_failed=true;}
}
