namespace scene_scenery_native_fixture {
inline constexpr uintptr_t scenery_object=0x20000,other_scenery=0x30000,gizmo_object=0x40000,other_gizmo=0x45000;
inline constexpr uintptr_t born_scenery=0x60000,born_gizmo=0x70000;
inline constexpr uint64_t scenery_uid=0x100000001ULL,other_scenery_uid=0x200000001ULL,gizmo_uid=0x300000001ULL,other_gizmo_uid=0x400000001ULL;
inline constexpr uint64_t born_scenery_uid=0x500000001ULL,born_gizmo_uid=0x600000001ULL;
inline constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
inline constexpr const char* source_id="33333333-3333-4333-8333-333333333333";
inline constexpr const char* snapshot_id="22222222-2222-4222-8222-222222222222";
using SceneryMember=mt2mp::build_actions::private_native::place::SceneryMember;
struct Case {
    Memory memory;Sample before;std::unique_ptr<a::Request> request=std::make_unique<a::Request>();
    std::string scenery_key,second_key,gizmo_key;
    explicit Case(c::Command command){
        boundary={};remove_diagnostics::scene_buildings::last_births.clear();remove_diagnostics::scene_buildings::last_retired.clear();
        dungeon_scene_identity_runtime::last_births.clear();dungeon_scene_identity_runtime::last_retired.clear();
        request->action_uuid=action_id;request->snapshot_uuid=snapshot_id;request->dungeon_input.scene=std::string("action:")+action_id+"/template";
        request->dungeon_input.controls.emplace();auto& intent=*request->dungeon_input.controls;intent.command=command;
        auto& census=before.source.scenes;census.world.uuid=snapshot_id;census.world.sha256=std::string(64,'a');census.world.world_epoch=1;census.world.generation=2;
        runtime_types::Pool pool;pool.scene.owner.key=request->dungeon_input.scene;pool.scene.owner.dungeon=0x90000;
        pool.scene.owner.shard=0x91000;pool.scene.owner.region=0x92000;pool.managers={0x93000,0x94000,0x95000,0x96000};
        pool.scenery.context={1,pool.scene.owner.region,pool.scene.owner.shard,pool.managers[0],true,true,true};
        SceneryMember first;first.object=scenery_object;first.uid=scenery_uid;first.category=2;first.bounds.valid=1;
        first.bounds.max={1,2,3};first.transform.rotation={0,0,0,1};first.transform.scale={1,1,1};
        auto second=first;second.object=other_scenery;second.uid=other_scenery_uid;second.category=3;
        pool.scenery.members={first,second};pool.scenery.counts[2]=pool.scenery.counts[3]=1;
        mt2mp::game::dungeon_gizmo_reader::Member gm;gm.object=gizmo_object;gm.uid=gizmo_uid;gm.definition=0xa0000;
        auto gm2=gm;gm2.object=other_gizmo;gm2.uid=other_gizmo_uid;
        pool.local.members={gm,gm2};pool.local.manager=pool.managers[3];pool.local.shard=pool.scene.owner.shard;pool.local.region=pool.scene.owner.region;
        pool.local.count=pool.local.capacity=2;pool.local.data=0x97000;pool.local.mutation_count=7;
        mt2mp::game::dungeon_gizmo_reader::Record record;record.name="Native lock";record.type="Lock";record.variant="Default";
        record.rotation={0,0,0,1};record.position={10,20,30};record.scale={1,1,1};pool.gizmos={record,record};
        gizmo_key=dungeon_gizmo_identity::birth(source_id,0);const auto second_gizmo_key=dungeon_gizmo_identity::birth(source_id,1);
        census.keys={gizmo_key,second_gizmo_key};census.pools.push_back(pool);
        scenery_key=mt2mp::game::scenery_scene_key::record(std::string(64,'a'),67,3,-1,0);
        second_key=mt2mp::game::scenery_scene_key::record(std::string(64,'a'),67,3,-1,1);
        native::BuildingState prop;prop.prop={scenery_uid,10,20,30};prop.type="Trees";prop.variant="Oak";prop.rotation={0,0,0,1};prop.scale={1,1,1};
        boundary.scenery[scenery_uid]=prop;prop.prop.uid=other_scenery_uid;prop.variant="Pine";boundary.scenery[other_scenery_uid]=prop;
        boundary.scenery_aliases={{scenery_object,scenery_uid,scenery_key},{other_scenery,other_scenery_uid,second_key}};
        boundary.gizmo_aliases={{gizmo_object,gizmo_uid,gizmo_key},{other_gizmo,other_gizmo_uid,second_gizmo_key}};
        memory.put(scenery_object,state.image+0x16c1390);memory.put(gizmo_object,state.image+0x1749ab0);
        for(const auto pointer:{scenery_object,other_scenery}){memory.put(pointer+0x270,std::bit_cast<uint32_t>(2.5f));memory.put(pointer+0x275,uint8_t(1));}
        boundary.current=census;
        c::Scenery selected;selected.type="Trees";selected.variant="Oak";selected.pose=c::pose(record);selected.height=std::bit_cast<uint32_t>(2.5f);selected.automatic=true;
        auto untouched=selected;untouched.variant="Pine";
        before.scenery.emplace(scenery_key,std::pair{request->dungeon_input.scene,selected});
        before.scenery.emplace(second_key,std::pair{request->dungeon_input.scene,untouched});
        c::Row sr;sr.key=scenery_key;sr.scenery=selected;sr.after=selected.pose;sr.after[4]=std::bit_cast<uint32_t>(14.0f);sr.cost=command==c::Command::Duplicate?35:0;
        c::Row gr;gr.key=gizmo_key;gr.before=record;gr.after=c::pose(record);gr.after[4]=std::bit_cast<uint32_t>(14.0f);gr.cost=sr.cost;
        intent.rows={sr,gr};
    }
    ControlPort port(){ControlPort value{memory};value.pool=&before.source.scenes.pools[0];value.sources={{scenery_object,scenery_uid},{gizmo_object,gizmo_uid}};
        if(request->dungeon_input.controls->command==c::Command::Duplicate){const auto& pool=*value.pool;
            value.created={{request->dungeon_input.scene,born_scenery,pool.managers[0],pool.scene.owner.shard,pool.scene.owner.region,born_scenery_uid},
                {request->dungeon_input.scene,born_gizmo,pool.managers[3],pool.scene.owner.shard,pool.scene.owner.region,born_gizmo_uid}};value.created_scenery={true,false};}
        return value;
    }
    void observed_effect(){auto& now=boundary.current.pools[0];const auto& rows=request->dungeon_input.controls->rows;
        const bool duplicate=request->dungeon_input.controls->command==c::Command::Duplicate;
        auto prop=boundary.scenery.at(scenery_uid);prop.prop.x=std::bit_cast<float>(rows[0].after[4]);
        const auto pointer=duplicate?born_scenery:scenery_object;const auto uid=duplicate?born_scenery_uid:scenery_uid;prop.prop.uid=uid;
        boundary.scenery[uid]=prop;memory.put(pointer+0x270,std::bit_cast<uint32_t>(4.25f));memory.put(pointer+0x275,uint8_t(duplicate?0:1));
        if(duplicate){auto member=now.scenery.members.front();member.object=pointer;member.uid=uid;member.transform.position.x=prop.prop.x;
            now.scenery.members.push_back(member);++now.scenery.counts[2];
            auto gm=now.local.members.front();gm.object=born_gizmo;gm.uid=born_gizmo_uid;now.local.members.push_back(gm);
            auto record=now.gizmos.front();c::pose(record,rows[1].after);record.parent.clear();now.gizmos.push_back(record);
            ++now.local.count;now.local.capacity=4;++now.local.mutation_count;
            boundary.current.keys.push_back(c::birth(rows[1],action_id,1));
        }else{now.scenery.members.front().transform.position.x=prop.prop.x;c::pose(now.gizmos.front(),rows[1].after);}
    }
};
template<class Check>void run(Check& check){std::string error;c::Row row;
    for(bool client:{false,true}){state.image=client?0x20000000:0x10000000;
        auto item=std::make_unique<Case>(c::Command::Move);
        row={};error.clear();
        check(control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid,false,row,error)&&row.key==item->scenery_key&&
            row.scenery==item->request->dungeon_input.controls->rows[0].scenery&&row.cost==0,"actual Scene Scenery Move reader retains full resource/pose/height/automatic and zero move cost");
        row={};check(control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid,true,row,error)&&row.cost==35,
            "actual Scene Scenery Duplicate reader captures original positive cost");
        row={};check(control_row(item->memory,item->before.source.scenes,gizmo_object,gizmo_uid,true,row,error)&&row.key==item->gizmo_key&&!row.scenery&&
            row.before==item->before.source.scenes.pools[0].gizmos[0]&&row.cost==35,"actual Gizmo Duplicate reader retains complete record and original cost");
        for(const auto command:{c::Command::Move,c::Command::Duplicate}){
            item=std::make_unique<Case>(command);item->observed_effect();auto port=item->port();error.clear();
            check(publish_controls(item->memory,{1,2},*item->request,item->before,port,error)&&boundary.scenery_publications==1&&boundary.gizmo_publications==1,
                "actual mixed Scene result validates complete owners/values before one alias publication per owner");
            const auto at=[&](const char* name){return std::find(boundary.trace.begin(),boundary.trace.end(),name)-boundary.trace.begin();};
            check(at("check-gizmos")<at("check-scenery")&&at("check-scenery")<at("publish-scenery")&&at("publish-scenery")<at("publish-gizmos"),
                "actual result preserves both original alias check fences before publication");
            if(command==c::Command::Duplicate){check(remove_diagnostics::scene_buildings::last_births.size()==1&&
                remove_diagnostics::scene_buildings::last_births[0].uid==born_scenery_uid&&
                remove_diagnostics::scene_buildings::last_births[0].key==c::birth(item->request->dungeon_input.controls->rows[0],action_id,0)&&
                dungeon_scene_identity_runtime::last_births.size()==1&&dungeon_scene_identity_runtime::last_births[0].second.uid==born_gizmo_uid,
                "actual mixed result separates native Scenery/Gizmo births with full UID and original row order");}
        }
    }
    {
        auto item=std::make_unique<Case>(c::Command::Move);boundary.failure="read-scenery";error.clear();
        check(!control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid,false,row,error)&&error=="controlled original read-scenery cause",
            "actual Scenery reader preserves upstream native read cause");
        boundary.failure.clear();boundary.scenery.at(scenery_uid).prop.uid=other_scenery_uid;error.clear();
        check(!control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid,false,row,error)&&error.find("full UID differs requested=")!=std::string::npos,
            "actual Scenery reader reports successful native read returning wrong full UID distinctly");
    }
    for(unsigned failure=0;failure<7;++failure){auto item=std::make_unique<Case>(c::Command::Move);error.clear();
        if(failure==0)item->memory.fields.erase(scenery_object+0x270);
        if(failure==1)item->memory.put(scenery_object+0x275,uint8_t(2));
        if(failure==2)item->memory.put(scenery_object+0x270,uint32_t(0x7fc00000));
        if(failure==3)boundary.scenery.at(scenery_uid).type.clear();
        if(failure==4)boundary.allowed=false;
        if(failure==5)boundary.price=-1;
        if(failure==6)boundary.capability_exception=0xc0000005;
        check(!control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid,false,row,error)&&!error.empty(),
            "actual Scenery current read rejects missing/native-invalid resource height automatic capability price and original exception");
    }
    {
        auto item=std::make_unique<Case>(c::Command::Move);error.clear();
        check(!control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid+(uint64_t(1)<<32),false,row,error),"actual Scenery resolver never substitutes matching low UID bits");
        item->before.source.scenes.world.generation+=1;error.clear();
        check(!control_row(item->memory,item->before.source.scenes,scenery_object,scenery_uid,false,row,error)&&error.find("Scene lifetime")!=std::string::npos,"actual Scenery current reader requires the same native Scene generation");
    }
    {
        auto item=std::make_unique<Case>(c::Command::Move);item->before.source.scenes.keys.pop_back();error.clear();
        check(!control_row(item->memory,item->before.source.scenes,gizmo_object,gizmo_uid,false,row,error)&&error.find("truncated")!=std::string::npos,"actual Gizmo reader rejects truncated complete canonical roster");
    }
    for(const auto command:{c::Command::Move,c::Command::Duplicate})for(unsigned failure=0;failure<11;++failure){
        auto item=std::make_unique<Case>(command);item->observed_effect();auto port=item->port();auto& now=boundary.current.pools[0];
        const auto uid=command==c::Command::Duplicate?born_scenery_uid:scenery_uid;
        const auto pointer=command==c::Command::Duplicate?born_scenery:scenery_object;
        if(failure==0)boundary.scenery.at(uid).variant="Foreign";
        if(failure==1)boundary.scenery.at(uid).prop.x+=1;
        if(failure==2)now.scenery.context.manager+=8;
        if(failure==3)now.scene.owner.region+=8;
        if(failure==4)now.scenery.members.back().uid+=uint64_t(1)<<32;
        if(failure==5)++now.scenery.counts[2];
        if(failure==6)boundary.scenery.at(other_scenery_uid).type="Changed unrelated";
        if(failure==7)item->memory.put(pointer+0x275,uint8_t(command==c::Command::Duplicate?1:0));
        if(failure==8){if(command==c::Command::Move)now.scenery.members[0].bounds.max.x+=1;else now.gizmos.back().variant="Foreign";}
        if(failure==9)boundary.failure="check-gizmos";
        if(failure==10){if(command==c::Command::Move)now.gizmos[0].position[0]+=1;else now.gizmos.back().parent="foreign";}
        error.clear();check(!publish_controls(item->memory,{1,2},*item->request,item->before,port,error)&&!error.empty()&&
            boundary.scenery_publications==0&&boundary.gizmo_publications==0,
            "actual mixed native result rejects resource pose Scene UID counts unrelated automatic geometry and failed-fence drift before publication");
    }
}
}
