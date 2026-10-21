namespace dungeon_input {
namespace {
namespace input_calls=dungeon_input_calls;
bool original_result(uint32_t exception,const char* operation,std::string& error){
    return !exception||fail(error,std::string("Dungeon input original ")+operation+" SEH="+std::to_string(exception));
}
struct Port {
    Memory& memory;const a::Binding& binding;const a::Request& request;const Sample& before;
    const dungeon_scene_identity_runtime::Pool* pool=nullptr;
    input_calls::Transform transform;input_calls::Paint painted;dungeon_fill::Owned fill;
    dungeon_input_action_calls::Commit committed;
    dungeon_input_action_calls::MonsterTail monster_tail;
    uintptr_t object=0,inverse=0,canvas=0,sound=0;uint64_t uid=0;
    bool level_stored=false,resources_cleaned=false;
    std::string cleanup_cause;
    const a::DungeonInputIntent& intent()const{return request.dungeon_input;}
    uintptr_t dungeon()const{return before.source.palette.dungeon.dungeon.dungeon.local.dungeon;}
    const auto& native()const{return before.source.palette.dungeon.dungeon.dungeon;}
    int32_t cost()const{return intent().cost;}
    bool preflight(dungeon_input_native::Tool tool,std::string& error){
        if(uint8_t(tool)!=uint8_t(intent().tool)||!a::valid_dungeon_input(intent()))return fail(error,"Dungeon input native tool/intent differs");
        pool=selected(before,intent());if(!pool||pool->scene.owner.dungeon!=dungeon())return fail(error,"Dungeon input current scene does not belong to captured Dungeon");
        dungeon_cells::MonsterMemory code_memory{memory};
        if(!dungeon_input_codes::verify(state.image,code_memory,error)||!dungeon_cells::code(memory,error))return false;
        if(before.source.palette.dungeon.dungeon.dungeon.undo.active_graph)return fail(error,"Dungeon input requires idle original Undo graph");
        if(intent().tool==a::DungeonInputTool::Door||intent().tool==a::DungeonInputTool::Key||intent().tool==a::DungeonInputTool::Treasure){
            int32_t actual=-1;bool allowed=false;const input_calls::Text type{intent().type.data(),uint64_t(intent().type.size())};
            if(!original_result(input_calls::cost(state.image,type,actual),"GetTypeCost",error)||actual!=intent().cost)
                return fail(error,error.empty()?"Dungeon input actual original resource price changed":error);
            if(!original_result(duplicate_calls::afford(state.image,native().business,actual,allowed),"CanAfford",error))return false;
            if(!allowed)return fail(error,"Dungeon input original Business cannot afford selected resource");
        }
        if(intent().tool==a::DungeonInputTool::Paint){
            uintptr_t ap=0;canvas=dungeon()+0x500;
            if(!field(memory,canvas,0,ap,error))return false;
            if(ap!=state.image+0x1786c50)return fail(error,"Dungeon input embedded original Canvas class differs");
            return true;
        }
        if(intent().tool==a::DungeonInputTool::Monster){
            if(!field(memory,state.image,0x13df450,sound,error))return false;
            if(!sound)return fail(error,"Dungeon input original SoundManager singleton is unavailable before Monster entry");
            auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(memory,base,offset,value,cause);};
            const auto game_state=uintptr_t(native().world.origin.state);uintptr_t manager=0;character_definition_identity::Census catalogue;
            if(!field(memory,game_state,0x168,manager,error)||!character_definition_identity::read(at,state.image,game_state,manager,catalogue,error))return false;
            if(std::count_if(catalogue.definitions.begin(),catalogue.definitions.end(),[&](const auto& row){return row.kind==1&&row.id==intent().character_type&&!row.deleted;})!=1)
                return fail(error,"Dungeon input selected Monster CharacterTypeID is absent, duplicated or deleted in current canonical catalogue");
        }
        return original_result(input_calls::transform(state.image,intent().rotation,intent().position,intent().scale,transform),"Transform construction",error);
    }
    bool purchase(std::string& error){
        bool paid=false;const input_calls::Text type{intent().type.data(),uint64_t(intent().type.size())};
        if(!original_result(input_calls::purchase(state.image,type,intent().position,paid),"Gizmo Purchase",error))return false;
        if(!paid)return fail(error,"Dungeon input original Purchase returned false after ordered entry");
        return intent().tool!=a::DungeonInputTool::Treasure||original_result(dungeon_input_action_calls::treasure_sound(state.image),"Treasure placement sound",error);
    }
    bool begin(bool& opened,std::string& error){
        opened=false;if(intent().tool!=a::DungeonInputTool::Door)return true;
        uint32_t cleanup=0;const auto exception=dungeon_input_action_calls::begin(state.image,opened,cleanup);
        if(exception||cleanup)return fail(error,"Dungeon input original BeginEdit/label failed SEH="+std::to_string(exception)+" cleanup="+std::to_string(cleanup));
        return true;
    }
    bool create(dungeon_input_native::Tool tool,uintptr_t& out,std::string& error){
        if(!pool)return fail(error,"Dungeon input original creation lost selected current scene");
        if(tool==dungeon_input_native::Tool::Monster){
            if(!original_result(input_calls::monster(state.image,pool->managers[2],transform,intent().rank,intent().character_type,object),"SpawnNewMonsterHere",error))return false;
        }else{
            const input_calls::Text type{intent().type.data(),uint64_t(intent().type.size())},variant{intent().variant.data(),uint64_t(intent().variant.size())};
            if(!original_result(input_calls::gizmo(state.image,pool->managers[3],type,variant,transform,object),"AddGizmo",error))return false;
        }
        out=object;if(!object)return fail(error,"Dungeon input original creation returned null");
        uintptr_t ap=0,shard=0,region=0,registered=0;host::LookupApi api;
        if(!field(memory,object,0,ap,error)||!field(memory,object,0x18,uid,error)||!field(memory,object,0x170,shard,error)||
           !field(memory,object,0x178,region,error)||!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
        if(!uid||shard!=pool->scene.owner.shard||region!=pool->scene.owner.region||
           ap!=state.image+(tool==dungeon_input_native::Tool::Monster?0x16c0770:0x1749ab0))
            return fail(error,"Dungeon input original return class/full UID/reciprocal scene owner differs");
        const auto found=lookup(api.find,uid,registered);
        if(found||registered!=object)return fail(error,"Dungeon input original return not registered by exact full UID SEH="+std::to_string(found));
        return true;
    }
    bool edge(uintptr_t actor,std::string& error){
        input_calls::Edge edge;
        return original_result(input_calls::nearest_edge(state.image,dungeon(),intent().position,edge),"FindCorridorCellEdgeNear",error)&&
            original_result(input_calls::edge(state.image,actor,edge),"Door SetEdge",error);
    }
    bool graph(uintptr_t& out,std::string& error){
        if(!original_result(duplicate_calls::graph(state.image,out),"DoGraph construction",error))return false;
        return out!=0||fail(error,"Dungeon input original DoGraph allocation returned null");
    }
    bool cash(uintptr_t graph,std::string& error){
        std::array<uint32_t,3> bits{};for(size_t i=0;i<3;++i)bits[i]=std::bit_cast<uint32_t>(intent().position[i]);
        return original_result(duplicate_calls::cash(state.image,graph,intent().cost,bits,inverse),"DoCash construction/AddDo",error)&&
            (inverse!=0||fail(error,"Dungeon input original Cash inverse allocation returned null"));
    }
    bool destroy_inverse(uintptr_t graph,uintptr_t actor,std::string& error){
        return original_result(duplicate_calls::destroy_inverse(state.image,graph,actor,inverse),"DoDestroy construction/AddDo",error)&&
            (inverse!=0||fail(error,"Dungeon input original Destroy inverse allocation returned null"));
    }
    bool snapshot_paint(uintptr_t graph,std::string& error){
        return original_result(input_calls::paint_inverse(state.image,dungeon(),graph,inverse),"DoDungeonPaint construction/AddDo",error)&&
            (inverse!=0||fail(error,"Dungeon input original Paint inverse allocation returned null"));
    }
    bool paint(std::string& error){
        if(!canvas)return fail(error,"Dungeon input original Paint lost its embedded Canvas owner");
        if(!intent().erase){
            std::string desired;if(!quest_bridge::sha256(intent().fill.dump(),desired,error))return false;
            dungeon_fill::Intent selection{desired,intent().fill};dungeon_cells::ValueMemory view(memory);
            if(!dungeon_fill::create(state.image,view,selection,fill,error))return false;
        }
        if(!original_result(input_calls::paint_begin(state.image,painted),"private accumulated PolyShape construction",error))return false;
        for(const auto& stamp:intent().shapes){
            ui_terrain_runtime::poly::BorrowedPaths borrowed(stamp);ui_terrain_runtime::poly::Storage shape;bool constructed=false;
            const auto exception=input_calls::shape(state.image,shape,borrowed.view,constructed);
            bool applied=original_result(exception,"clipped PolyShape construction",error);
            if(applied)applied=original_result(input_calls::paint_stamp(state.image,canvas,fill.pointer,intent().erase,shape,painted),"Canvas clipped stamp",error);
            if(constructed){const auto cleanup=input_calls::shape_destroy(state.image,shape);
                if(cleanup){const auto cause="Dungeon input original clipped PolyShape destruction SEH="+std::to_string(cleanup);
                    if(!error.empty())error+="; cleanup: "+cause;else error=cause;return false;}}
            if(!applied)return false;
        }
        return true;
    }
    bool finish_paint(std::string& error){
        if(!original_result(input_calls::paint_finish(state.image,canvas,painted),"PaintingFinished stroke cleanup",error))return false;
        return painted.finished&&std::isfinite(painted.painted)&&std::isfinite(painted.erased)&&
            (painted.painted>0||painted.erased>0)||fail(error,"Dungeon input original Canvas stroke produced no finite changed area");
    }
    bool cleanup(std::string& error){
        if(resources_cleaned)return cleanup_cause.empty()||fail(error,cleanup_cause);resources_cleaned=true;
        const auto failure=[&](const std::string& cause){if(!cleanup_cause.empty())cleanup_cause+="; ";cleanup_cause+=cause;};
        if(painted.constructed){const auto exception=input_calls::shape_destroy(state.image,painted.accumulated);
            if(exception)failure("Dungeon input original accumulated PolyShape destruction SEH="+std::to_string(exception));else painted.constructed=false;}
        if(painted.pending_stroke)failure("Dungeon input partial original Stroke ownership is unresolved; no destructor replay");
        if(fill.pointer){std::string cause;if(!dungeon_fill::discard(state.image,fill,cause))failure(cause.empty()?"Dungeon input original Fill destruction failed without cause":cause);}
        else if(fill.unknown)failure("Dungeon input original Fill construction remains Unknown; no destructor replay");
        return cleanup_cause.empty()||fail(error,cleanup_cause);
    }
    bool commit(uintptr_t graph,std::string& error){
        const auto exception=dungeon_input_action_calls::commit(state.image,native().undo.receiver,graph,object,committed);
        if(exception||committed.arguments_cleanup||committed.label_cleanup||committed.text_cleanup||!committed.returned)
            return fail(error,"Dungeon input original Undo AddDo/label lifetime failed SEH="+std::to_string(exception)+" arguments_cleanup="+
                std::to_string(committed.arguments_cleanup)+" label_cleanup="+std::to_string(committed.label_cleanup)+" text_cleanup="+std::to_string(committed.text_cleanup));
        if(intent().tool==a::DungeonInputTool::Monster){
            uintptr_t current_sound=0;if(!field(memory,state.image,0x13df450,current_sound,error))return false;
            if(current_sound!=sound)return fail(error,"Dungeon input original SoundManager changed before Monster release tail");
            if(!original_result(dungeon_input_action_calls::monster_tail(state.image,object,intent().rank,sound,pool->scene.owner.shard,
                intent().position,monster_tail),"Monster achievement/sound tail",error))return false;
            int32_t level=-1;if(!field(memory,object,0x6f8,level,error))return false;
            if(!original_result(dungeon_input_action_calls::level(state.image,object,level,intent().level,level_stored),"selected level/LevelChanged",error))return false;
        }
        return true;
    }
    bool end(std::string& error){return original_result(duplicate_calls::end(state.image),"EndEdit",error);}
    bool readback(const dungeon_input_native::Receipt& receipt,std::string& error){
        if(!cleanup(error))return false;
        return receipt.committed&&(!receipt.edit_opened||receipt.edit_closed)||fail(error,"Dungeon input original graph/edit lifetime not settled");
    }
};
}
}
