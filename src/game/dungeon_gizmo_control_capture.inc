namespace dungeon_input {
namespace {
dungeon_cell_graph::Policy control_policy(const dungeon_gizmo_control::Intent& intent){
    const bool scenery=dungeon_gizmo_control::mixed(intent);
    return {intent.command==ControlCommand::Move?(scenery?dungeon_cell_graph::Shape::SceneMove:dungeon_cell_graph::Shape::GizmoMove):
        (scenery?dungeon_cell_graph::Shape::SceneControls:dungeon_cell_graph::Shape::Gizmos),dungeon_gizmo_control::cash_count(intent),uint32_t(intent.rows.size())};
}
bool control_graph(Memory& memory,const a::Request& request,const dungeon_cells::BoundGraph& graph,std::string& error){
    const auto& controls=*request.dungeon_input.controls;std::set<std::string> targets;size_t cash=0;
    for(const auto& node:graph.value.nodes){
        if(node.kind==dungeon_cell_graph::Kind::Cash){
            if(cash>=dungeon_gizmo_control::cash_count(controls))return fail(error,"Gizmo control original graph has an extra Cash node");
            int32_t amount=0;std::array<uint32_t,3> position{};
            if(!field(memory,node.pointer,0xb8,amount,error)||!field(memory,node.pointer,0xbc,position,error))return false;
            const auto price=controls.rows.size()>9?dungeon_gizmo_control::total(controls):controls.rows[cash].cost;
            std::array<uint32_t,3> expected=controls.cash_position;
            if(controls.rows.size()<=9){const auto pose=controls.command==ControlCommand::Duplicate?controls.rows[cash].after:dungeon_gizmo_control::before_pose(controls.rows[cash]);
                expected={pose[4],pose[5],pose[6]};}
            if(int64_t(amount)!=int64_t(price)*(controls.command==ControlCommand::Destroy?-1:1)||position!=expected)
                return fail(error,"Gizmo control complete original Cash inverse amount/position differs");++cash;continue;
        }
        const auto actor=graph.actors.find(node.uid);
        if(actor==graph.actors.end()||(actor->second.kind!=dungeon_cell_graph::Kind::Gizmo&&actor->second.kind!=dungeon_cell_graph::Kind::Scenery)||actor->second.scene!=request.dungeon_input.scene||
           !targets.insert(actor->second.key).second)return fail(error,"Gizmo control graph target lacks its unique complete canonical owner");
        const auto found=std::find_if(controls.rows.begin(),controls.rows.end(),[&](const auto& row){const auto index=uint32_t(&row-controls.rows.data());
            return actor->second.key==(controls.command==ControlCommand::Duplicate?dungeon_gizmo_control::birth(row,request.action_uuid,index):row.key);});
        const bool kind=controls.command==ControlCommand::Move?node.kind==dungeon_cell_graph::Kind::Move:
            controls.command==ControlCommand::Duplicate?node.kind==dungeon_cell_graph::Kind::Destroy:
            node.kind==dungeon_cell_graph::Kind::Gizmo||node.kind==dungeon_cell_graph::Kind::Door||node.kind==dungeon_cell_graph::Kind::Scenery;
        if(found==controls.rows.end()||!kind||bool(found->scenery)!=(actor->second.kind==dungeon_cell_graph::Kind::Scenery)||
           (node.kind==dungeon_cell_graph::Kind::Scenery&&!found->scenery))return fail(error,"Gizmo control graph has a foreign canonical target or inverse kind");
    }
    return targets.size()==controls.rows.size()&&cash==dungeon_gizmo_control::cash_count(controls)||
        fail(error,"Gizmo control original complete target/Cash census differs");
}
bool publish_controls(Memory&,const a::Binding&,const a::Request& request,const Sample& before,ControlPort& port,std::string& error){
    const auto& controls=*request.dungeon_input.controls;
    std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>> births,retired;
    std::vector<remove_diagnostics::scene_buildings::Row> scenery_births,scenery_retired;
    const auto scenery_member=[](const std::string& key,const dungeon_gizmo_identity::Member& member){return remove_diagnostics::scene_buildings::Row{
        key,member.scene,member.object,member.shard,member.region,member.manager,member.uid,true};};
    if(controls.command==ControlCommand::Duplicate){
        if(port.created.size()!=controls.rows.size())return fail(error,"Gizmo duplicate complete native return count differs");
        if(port.created_scenery.size()!=controls.rows.size())return fail(error,"Scene control complete native returned family count differs");
        for(size_t i=0;i<port.created.size();++i){const auto key=dungeon_gizmo_control::birth(controls.rows[i],request.action_uuid,uint32_t(i));
            if(port.created_scenery[i])scenery_births.push_back(scenery_member(key,port.created[i]));else births.emplace_back(key,port.created[i]);}
    }else if(controls.command==ControlCommand::Destroy){
        for(size_t i=0;i<controls.rows.size();++i){dungeon_gizmo_identity::Member member;
            if(controls.rows[i].scenery){const auto& pool=*port.pool;scenery_retired.push_back({controls.rows[i].key,request.dungeon_input.scene,
                port.sources[i].object,pool.scene.owner.shard,pool.scene.owner.region,pool.managers[0],port.sources[i].uid,true});continue;}
            if(!dungeon_scene_identity_runtime::lookup(controls.rows[i].key,member,error)||member.object!=port.sources[i].object||member.uid!=port.sources[i].uid)
                return fail(error,error.empty()?"Gizmo Destroy retained canonical source changed":error);retired.emplace_back(controls.rows[i].key,member);}
    }
    if(controls.command==ControlCommand::Move)for(size_t i=0;i<controls.rows.size();++i)if(controls.rows[i].scenery&&
       !remove_diagnostics::refresh_moved_building(request.snapshot_uuid,controls.rows[i].key,port.sources[i].uid,error))return false;
    remove_diagnostics::scene_buildings::Preview scenery_preview;
    if(!remove_diagnostics::scene_buildings::preview(before.source.scenes.world,scenery_births,scenery_retired,{},scenery_preview,error))return false;
    dungeon_scene_identity_runtime::ClonePreview preview;dungeon_scene_identity_runtime::Census actual;
    if(!dungeon_scene_identity_runtime::preview_clones(before.source.scenes.world,births,retired,preview,actual,error))return false;
    if(actual.pools.size()!=before.source.scenes.pools.size())return fail(error,"Gizmo control changed the complete scene roster");
    for(size_t scene=0;scene<actual.pools.size();++scene){const auto& old=before.source.scenes.pools[scene];const auto& now=actual.pools[scene];
        if(old.scene.owner.key!=now.scene.owner.key||old.scene.owner.shard!=now.scene.owner.shard||old.scene.owner.region!=now.scene.owner.region||
           old.scene.owner.dungeon!=now.scene.owner.dungeon||old.managers!=now.managers||
           old.zones.manager!=now.zones.manager||old.zones.shard!=now.zones.shard||old.zones.region!=now.zones.region||old.zones.members!=now.zones.members)
            return fail(error,"Gizmo control changed a protected scene/Scenery/Zone owner");
        for(size_t type=0;type<4;++type){const auto& previous=old.zones.arrays[type];const auto& current=now.zones.arrays[type];
            if(previous.data!=current.data||previous.count!=current.count||previous.capacity!=current.capacity||previous.members!=current.members)
                return fail(error,"Gizmo control changed a protected complete all-type Zone array");}
        if(old.scene.owner.key!=request.dungeon_input.scene){if(old.scenery!=now.scenery)return fail(error,"Scene control changed unrelated complete Scenery owner/values");}
        else {
            if(old.scenery.context!=now.scenery.context)return fail(error,"Scene control changed selected Scenery context");
            std::set<uint64_t> affected,born;std::array<int32_t,47> counts=old.scenery.counts;
            for(size_t i=0;i<controls.rows.size();++i)if(controls.rows[i].scenery){const auto& row=controls.rows[i];
                const auto prior=std::find_if(old.scenery.members.begin(),old.scenery.members.end(),[&](const auto& member){return member.object==port.sources[i].object&&member.uid==port.sources[i].uid;});
                if(prior==old.scenery.members.end())return fail(error,"Scene Scenery selected source absent from before census");
                affected.insert(prior->uid);
                if(controls.command==ControlCommand::Destroy){--counts[size_t(prior->category)];continue;}
                const auto object=controls.command==ControlCommand::Duplicate?port.created[i].object:prior->object;
                const auto uid=controls.command==ControlCommand::Duplicate?port.created[i].uid:prior->uid;
                const auto current=std::find_if(now.scenery.members.begin(),now.scenery.members.end(),[&](const auto& member){return member.object==object&&member.uid==uid;});
                if(current==now.scenery.members.end()||current->category!=prior->category)return fail(error,"Scene Scenery original result owner/category differs");
                dungeon_gizmo_control::Scenery value;if(!scenery_observed(port.memory,object,uid,value,error))return false;
                auto wanted=*row.scenery;wanted.pose=row.after;wanted.height=value.height;
                if(controls.command==ControlCommand::Duplicate)wanted.automatic=false;
                if(value!=wanted)return fail(error,"Scene Scenery original result resource/pose differs");
                if(controls.command==ControlCommand::Move&&current->bounds!=prior->bounds)return fail(error,"Scene Scenery Move changed original local geometry");
                if(controls.command==ControlCommand::Duplicate){++counts[size_t(prior->category)];born.insert(uid);}
            }
            if(counts!=now.scenery.counts)return fail(error,"Scene control complete Scenery category counts differ");
            size_t survivors=0;
            for(const auto& member:old.scenery.members){if(controls.command!=ControlCommand::Duplicate&&affected.contains(member.uid))continue;
                const auto current=std::find(now.scenery.members.begin(),now.scenery.members.end(),member);
                if(current==now.scenery.members.end())return fail(error,"Scene control changed an unrelated full Scenery member/value");++survivors;}
            const size_t changed=controls.command==ControlCommand::Move?affected.size():controls.command==ControlCommand::Duplicate?born.size():0;
            if(now.scenery.members.size()!=survivors+changed)return fail(error,"Scene control complete Scenery membership has an extra/missing result");
        }
        if(dungeon_gizmo_control::mixed(controls))for(const auto& member:old.scenery.members){
            const bool changed=old.scene.owner.key==request.dungeon_input.scene&&controls.command!=ControlCommand::Duplicate&&
                std::any_of(controls.rows.begin(),controls.rows.end(),[&](const auto& row){
                    const auto index=size_t(&row-controls.rows.data());return row.scenery&&port.sources[index].uid==member.uid;});
            if(changed)continue;
            std::string key;dungeon_gizmo_control::Scenery value;
            if(!remove_diagnostics::scene_scenery::resolve(before.source.scenes.world,member.object,member.uid,key,error)||
               !scenery_observed(port.memory,member.object,member.uid,value,error))return false;
            const auto previous=before.scenery.find(key);
            if(previous==before.scenery.end()||previous->second.first!=old.scene.owner.key||previous->second.second!=value)
                return fail(error,"Scene control changed an unrelated complete Scenery resource/height/automatic value");
        }
        auto members=old.local.members;auto records=old.gizmos;
        if(old.scene.owner.key==request.dungeon_input.scene){
            for(size_t i=0;i<controls.rows.size();++i){const auto& row=controls.rows[i];if(row.scenery)continue;
                const auto found=std::find_if(members.begin(),members.end(),[&](const auto& value){return value.object==port.sources[i].object&&value.uid==port.sources[i].uid;});
                if(found==members.end())return fail(error,"Gizmo control complete source disappeared from its native owner");
                const auto index=size_t(found-members.begin());
                if(controls.command==ControlCommand::Move)dungeon_gizmo_control::pose(records[index],row.after);
                else if(controls.command==ControlCommand::Destroy){members.erase(found);records.erase(records.begin()+index);}
            }
            if(controls.command==ControlCommand::Duplicate){
                const size_t added=size_t(std::count(port.created_scenery.begin(),port.created_scenery.end(),false));
                if(now.local.members.size()!=members.size()+added||now.gizmos.size()!=records.size()+added)
                    return fail(error,"Gizmo Duplicate native complete append count differs");
                size_t append=0;for(size_t i=0;i<port.created.size();++i){if(port.created_scenery[i])continue;
                    const auto& born=now.local.members[members.size()+append];const auto& record=now.gizmos[records.size()+append];const auto& expected=controls.rows[i];++append;
                    if(born.object!=port.created[i].object||born.uid!=port.created[i].uid||record.type!=expected.before.type||record.variant!=expected.before.variant||
                       !record.parent.empty()||dungeon_gizmo_control::pose(record)!=expected.after)return fail(error,"Gizmo Duplicate original append/resource/pose differs");}
                if(!std::equal(members.begin(),members.end(),now.local.members.begin())||!std::equal(records.begin(),records.end(),now.gizmos.begin()))
                    return fail(error,"Gizmo Duplicate changed existing complete native members or values");
                continue;
            }
        }
        if(members!=now.local.members||records!=now.gizmos)return fail(error,"Gizmo control changed an unrelated member/value or native order");
    }
    if(!dungeon_scene_identity_runtime::check_clones(preview,error)||!remove_diagnostics::scene_buildings::check(scenery_preview,error))return false;
    return remove_diagnostics::scene_buildings::publish(scenery_preview,error)&&dungeon_scene_identity_runtime::publish_clones(preview,error);
}
bool control_row(Memory& memory,const dungeon_scene_identity_runtime::Census& census,uintptr_t object,uint64_t uid,
                 bool duplicate,dungeon_gizmo_control::Row& out,std::string& error,bool destroying=false){
    uintptr_t ap=0;if(!field(memory,object,0,ap,error))return false;
    if(ap==state.image+0x16c1390){dungeon_gizmo_control::SceneryRow row;
        if(!scenery_record(memory,census,object,uid,row,error))return false;out.key=row.key;out.scenery=row.value;
        bool allowed=false;int32_t cost=0;
        if(!original_result(dungeon_gizmo_control_calls::capability(object,duplicate,cost,allowed),"Scene Scenery capability/price",error)||(!destroying&&!allowed)||cost<0)
            return fail(error,error.empty()?"Scene Scenery original capability/price unavailable":error);
        out.cost=duplicate?cost:0;return true;
    }
    if(!dungeon_scene_identity_runtime::resolve(object,uid,out.key,error))return false;
    size_t key=0;bool found=false;
    for(const auto& pool:census.pools)for(size_t n=0;n<pool.local.members.size();++n,++key){const auto& member=pool.local.members[n];
        if(key>=census.keys.size())return fail(error,"Gizmo input canonical roster is truncated");
        if(member.object==object&&member.uid==uid){if(found||out.key!=census.keys[key])return fail(error,"Gizmo input canonical member repeats or differs");found=true;out.before=pool.gizmos[n];}}
    if(!found||key!=census.keys.size())return fail(error,"Gizmo input lacks complete current scene member");
    bool allowed=false;int32_t cost=0;
    if(!original_result(dungeon_gizmo_control_calls::capability(object,duplicate,cost,allowed),"Gizmo input capability/price",error)||(!destroying&&!allowed)||cost<0)
        return fail(error,error.empty()?"Gizmo input original capability or price unavailable":error);
    out.cost=duplicate?cost:0;return true;
}
bool capture_control(Memory& memory,Pending& value,std::string& error){
    if(!finish_capture(memory,value,error))return false;
    Sample before;if(!sample(memory,value.binding,value.snapshot,value.intent,before,error))return false;
    a::Request request;request.action=a::Action::DungeonInput;request.snapshot_uuid=value.snapshot;request.dungeon_input=value.intent;
    ControlPort port{memory,value.binding,request,before};control_ui_owners.clear();
    const bool ready=port.preflight(error);control_ui_owners.clear();return ready;
}
}
bool capture_gizmo_move(uintptr_t behaviour,uintptr_t caller,bool& handled,std::string& error)try{
    handled=false;if(!ready||replaying||!net::session_wire::connected()||caller!=state.image+0x5c2a20)return true;
    Memory memory;dungeon_cells::MonsterMemory adapter{memory};npc_quest_targets::Bounds records;
    if(!npc_quest_targets::bounds(adapter,behaviour+0x458,100000,records,error))return false;
    bool gizmo=false,scenery=false,other=false;
    for(int32_t i=0;i<records.count;++i){uintptr_t object=0,ap=0;
        if(!field(memory,records.data,size_t(i)*24+16,object,error)||!field(memory,object,0,ap,error))return false;
        if(ap==state.image+0x1749ab0)gizmo=true;
        else if(ap==state.image+0x16c1390){uintptr_t shard=0;int32_t type=-1;
            if(!field(memory,object,0x170,shard,error)||!field(memory,shard,0x48,type,error))return false;
            if(type>=1&&type<=3)scenery=true;else other=true;
        }else other=true;}
    if(!gizmo&&!scenery)return true;handled=true;
    if(other)return reject("Scene control selection contains a target outside the current Gizmo/Scene Scenery owners");
    Pending value;value.snapshot=net::current_snapshot_id();value.intent.tool=a::DungeonInputTool::Gizmos;
    if(!idle(value.binding,value.snapshot,error)||!code(memory,error)||!capture_scene(value.intent,error))return reject(error);
    dungeon_scene_identity_runtime::Census census;if(!dungeon_scene_identity_runtime::read_current(census,error))return reject(error);
    uint8_t duplicate=2;if(!field(memory,behaviour,0x499,duplicate,error)||duplicate>1)return reject(error.empty()?"Gizmo Move duplicate flag differs":error);
    auto owner=[&](uintptr_t object,uint64_t uid,dungeon_gizmo_control::Row& row,std::string& cause){return control_row(memory,census,object,uid,duplicate!=0,row,cause);};
    auto point=[&](std::array<uint32_t,3>& out,std::string& cause){
        uintptr_t cursor=0,current=0,axis=0;std::array<float,3> position{},up{};float scale=0;
        if(!field(memory,state.image,0x13df9d0,cursor,cause)||!field(memory,cursor,0x190,current,cause)||current!=behaviour||
           !field(memory,cursor,0x1e4,position,cause)||!field(memory,state.image,0x164b0d0,axis,cause)||axis!=state.image+0x1c4e8f0||
           !field(memory,axis,0,up,cause)||!field(memory,state.image,0x14ee978,scale,cause)||scale!=20.0f)return fail(cause,cause.empty()?"Gizmo Duplicate original Cursor/YAxis/offset differs":cause);
        for(size_t i=0;i<3;++i)out[i]=std::bit_cast<uint32_t>(position[i]+up[i]*scale);return true;};
    dungeon_gizmo_move_read::Capture first,second;
    if(!dungeon_gizmo_move_read::read(adapter,state.image,behaviour,owner,point,first,error)||
       !dungeon_gizmo_move_read::read(adapter,state.image,behaviour,owner,point,second,error)||first!=second)
        return reject(error.empty()?"Gizmo Move complete original input changed during capture":error);
    value.intent.controls=first.intent;value.control_cursor=std::move(first);
    if(!capture_control(memory,value,error))return reject(error);pending=std::move(value);++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Gizmo Move capture: ")+exception.what());}
catch(...){fault=true;return reject("Gizmo Move capture non-standard boundary exception");}
bool capture_scene_remove(uintptr_t manager,uintptr_t array,bool& handled,std::string& error)try{
    handled=false;if(!ready||replaying||!net::session_wire::connected())return true;
    Memory memory;dungeon_cells::MonsterMemory adapter{memory};npc_quest_targets::Bounds header;
    if(!npc_quest_targets::typed(adapter,state.image,array,0x1748c50,error)||!npc_quest_targets::bounds(adapter,array,100000,header,error))return false;
    if(!header.count)return true;
    std::vector<std::pair<uintptr_t,uint64_t>> objects;bool scene=false,gizmo=false,other=false;
    for(int32_t i=0;i<header.count;++i){uintptr_t object=0,ap=0;uint64_t uid=0;
        if(!field(memory,header.data,size_t(i)*8,object,error)||!field(memory,object,0,ap,error)||!field(memory,object,0x18,uid,error))return false;
        objects.emplace_back(object,uid);
        if(ap==state.image+0x1749ab0)gizmo=true;
        else if(ap==state.image+0x16c1390){uintptr_t shard=0;int32_t type=-1;
            if(!field(memory,object,0x170,shard,error)||!field(memory,shard,0x48,type,error))return false;
            if(type>=1&&type<=3)scene=true;else other=true;
        }else other=true;
    }
    // Single Scenery keeps the original existing RemoveSnapshotBuilding owner.
    if((!gizmo&&!scene)||(!gizmo&&header.count==1))return true;handled=true;
    if(other)return fail(error,"Scene Destroy collection contains another native owner family");
    Pending value;value.snapshot=net::current_snapshot_id();value.intent.tool=a::DungeonInputTool::Gizmos;
    uintptr_t selected=0;if(!idle(value.binding,value.snapshot,error)||!code(memory,error)||
       !field(memory,state.image,0x13df630,selected,error)||selected!=manager)return fail(error,error.empty()?"Scene Destroy Selection manager differs":error);
    dungeon_scene_identity_runtime::Census census;if(!dungeon_scene_identity_runtime::read_current(census,error))return false;
    dungeon_gizmo_control::Intent controls;controls.command=ControlCommand::Destroy;std::array<float,3> sum{};
    std::set<uintptr_t> unique;std::set<uint64_t> uids;
    for(const auto& [object,uid]:objects){dungeon_gizmo_control::Row row;
        if(!uid||!unique.insert(object).second||!uids.insert(uid).second||!control_row(memory,census,object,uid,false,row,error,true))return fail(error,error.empty()?"Scene Destroy full source lifetime repeats":error);
        std::string owner;
        if(row.scenery){for(const auto& pool:census.pools)for(const auto& member:pool.scenery.members)if(member.object==object&&member.uid==uid)owner=pool.scene.owner.key;}
        else{dungeon_gizmo_identity::Member member;if(!dungeon_scene_identity_runtime::lookup(row.key,member,error))return false;owner=member.scene;}
        if(owner.empty()||(!value.intent.scene.empty()&&value.intent.scene!=owner))return fail(error,"Scene Destroy selected native owners differ");value.intent.scene=owner;
        bool unused=false;std::array<uint32_t,3> point{};
        if(!original_result(dungeon_gizmo_control_calls::capability(object,false,row.cost,unused),"Scene Destroy actual Cost",error)||
           !original_result(dungeon_gizmo_control_calls::cost_position(object,point),"Scene Destroy actual CostPosition",error))return false;
        row.after=dungeon_gizmo_control::before_pose(row);
        for(size_t axis=0;axis<3;++axis){const auto coordinate=std::bit_cast<float>(point[axis]);if(!std::isfinite(coordinate))return fail(error,"Scene Destroy CostPosition is nonfinite");sum[axis]+=coordinate;}
        controls.rows.push_back(std::move(row));
    }
    const float factor=1.0f/float(objects.size());for(size_t axis=0;axis<3;++axis)controls.cash_position[axis]=std::bit_cast<uint32_t>(sum[axis]*factor);
    const auto end=value.intent.scene.ends_with("/template")?value.intent.scene.size()-9:value.intent.scene.rfind("/instance/");
    if(end==std::string::npos)return fail(error,"Scene Destroy collection lacks its canonical Dungeon owner");value.intent.key=value.intent.scene.substr(0,end);
    if(!npc_quest_targets::unchanged(adapter,array,100000,header,error))return false;
    for(size_t i=0;i<objects.size();++i){uintptr_t object=0;uint64_t uid=0;
        if(!field(memory,header.data,i*8,object,error)||!field(memory,object,0x18,uid,error)||objects[i]!=std::pair{object,uid})return fail(error,error.empty()?"Scene Destroy collection changed during complete capture":error);}
    value.intent.controls=std::move(controls);if(!capture_control(memory,value,error))return false;
    pending=std::move(value);++captures;return true;
}catch(const std::exception& exception){fault=true;return fail(error,std::string("Scene Destroy capture: ")+exception.what());}
catch(...){fault=true;return fail(error,"Scene Destroy capture non-standard boundary exception");}
bool capture_gizmo_destroy(uintptr_t object,uint64_t uid,std::string& error)try{
    Memory memory;Pending value;value.snapshot=net::current_snapshot_id();value.intent.tool=a::DungeonInputTool::Gizmos;
    if(!idle(value.binding,value.snapshot,error)||!code(memory,error))return reject(error);
    dungeon_scene_identity_runtime::Census census;dungeon_gizmo_control::Row row;dungeon_gizmo_identity::Member member;
    if(!dungeon_scene_identity_runtime::read_current(census,error)||!dungeon_scene_identity_runtime::resolve(object,uid,row.key,error)||
       !dungeon_scene_identity_runtime::lookup(row.key,member,error))return reject(error);
    value.intent.scene=member.scene;const auto end=member.scene.ends_with("/template")?member.scene.size()-9:member.scene.rfind("/instance/");
    if(end==std::string::npos)return reject("Gizmo Destroy target lacks its canonical Dungeon owner");value.intent.key=member.scene.substr(0,end);
    bool found=false;
    for(const auto& pool:census.pools)for(size_t n=0;n<pool.local.members.size();++n)if(pool.local.members[n].object==object&&pool.local.members[n].uid==uid){
        if(found)return reject("Gizmo Destroy canonical source repeats");found=true;row.before=pool.gizmos[n];}
    if(!found)return reject("Gizmo Destroy current full UID absent from complete native scene census");
    bool unused=false;if(!original_result(dungeon_gizmo_control_calls::capability(object,false,row.cost,unused),"Gizmo Destroy actual Cost",error))return reject(error);
    row.after=dungeon_gizmo_control::pose(row.before);dungeon_gizmo_control::Intent controls;controls.command=ControlCommand::Destroy;controls.rows.push_back(row);
    if(!original_result(dungeon_gizmo_control_calls::cost_position(object,controls.cash_position),"Gizmo Destroy actual CostPosition",error))return reject(error);
    value.intent.controls=std::move(controls);if(!capture_control(memory,value,error))return reject(error);
    pending=std::move(value);++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Gizmo Destroy capture: ")+exception.what());}
catch(...){fault=true;return reject("Gizmo Destroy capture non-standard boundary exception");}
}
