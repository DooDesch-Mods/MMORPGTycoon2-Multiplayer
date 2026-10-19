namespace scenery_stroke {
namespace {
struct Shape {
    poly::Storage value;calls::Transfer transfer;
    bool constructed=false,destruction_entered=false,destroyed=false;
};
struct Port {
    Runtime& r;action::Binding binding;action::Request request;Sample before,after;
    scenery_stroke_native::Receipt& receipt;calls::Cursor cursor;calls::Context context;
    std::vector<std::unique_ptr<Shape>> shapes;
    std::map<uint64_t,std::string> aliases;
    std::vector<std::string> birth_order;
    std::map<uint64_t,Saved> births,removed;
    std::vector<action::DuplicateCash> cash;
    std::vector<duplicate_native::Handle> selection;
    std::array<uint8_t,9> filter{};
    action::Evidence evidence;
    scenery_alias_order::Receipt settled_aliases;
    int32_t category=-1,variant=-1,seen_births=0;
    bool no_effect=false,rng_attempted=false,history_started=false;
    std::optional<action::SceneryStrokeState> authority_before;
    bool ready(const action::SceneryStrokeIntent& intent,std::string& e){
        if(!installed||!r.ready||r.thread!=GetCurrentThreadId()||!action::valid_scenery_stroke(intent)||!verify_routes(r,true,e)||
           !resolve_catalog(r,intent,category,variant,e))return q::fail(e,e.empty()?"Scenery original replay input/routes/catalog are unavailable":e);
        std::vector<uintptr_t> raw,filtered;
        if(!group_reader::selection(r.memory,r.image,before.props.world.selection,raw,filtered,e)||
           !q::at(r.memory,before.props.world.selection,0xc8,filter,e)||std::any_of(filter.begin(),filter.end(),[](auto value){return value>1;}))return false;
        for(auto object:raw){uint64_t uid=0;uintptr_t actual=0;
            if(!q::at(r.memory,object,0x18,uid,e)||!group_edit::find(r,uid,actual,e)||actual!=object)return q::fail(e,e.empty()?"Scenery local selected full UID differs":e);
            selection.push_back({object,uid});
        }
        size_t maximum=0;for(const auto& stamp:intent.stamps){
            if(stamp.dots.size()>action::max_payload-maximum)return q::fail(e,"Scenery complete birth budget exceeds payload domain");maximum+=stamp.dots.size();}
        for(size_t index=0;index<maximum;++index){std::string key;if(!birth_key(request.action_uuid,index,key,e))return false;
            if(before.props.objects.contains(key))return q::fail(e,"Scenery new canonical birth key already exists");}
        return true;
    }
    bool prepare(const action::SceneryStrokeIntent& intent,std::string& e){
        if(authority_before){
            rng_attempted=true;
            if(!fault(calls::seed(before.props.random,before.region_random,*authority_before),"authority RNG seed",e))return false;
            std::array<uint64_t,4> global{},regional{};
            if(!q::at(r.memory,before.props.random,0,global,e)||!q::at(r.memory,before.region_random,0,regional,e)||
               global!=authority_before->random||regional!=authority_before->region_random)
                return q::fail(e,e.empty()?"Scenery authority RNG seed readback differs":e);
            before.state.random=global;before.state.region_random=regional;before.props.state.random=global;
        }
        cursor.image=r.image;
        if(!fault(calls::construct(cursor,intent,before.region,category,variant),"cursor/options construction",e)||!cursor.constructed)return false;
        for(const auto& stamp:intent.stamps){auto shape=std::make_unique<Shape>();shape->transfer.image=r.image;shapes.push_back(std::move(shape));auto& owned=*shapes.back();
            poly::BorrowedPaths paths(stamp.shape);
            if(!fault(calls::shape_make(r.image,owned.value,paths.view,owned.constructed),"PolyShape construction",e))return false;
            action::TerrainShape actual;if(!poly::read(r.memory.process,uintptr_t(&owned.value),actual,e)||actual!=stamp.shape)
                return q::fail(e,e.empty()?"Scenery original PolyShape reconstruction differs":e);
            if(intent.kind==Kind::Paint&&intent.mode==2&&!fault(calls::prepare(owned.transfer,stamp.dots),"generated transform preparation",e))return false;
        }
        if(!q::at(r.memory,r.image,0x13df440,context.manager,e)||!context.manager||!q::at(r.memory,r.image,0x13df9d0,context.cursor,e)||
           !q::typed(r.memory,r.image,context.cursor,0x1786dd0,e))return q::fail(e,e.empty()?"Scenery replay native ShardManager/Cursor is absent":e);
        return true;
    }
    bool enter_context(bool& entered,std::string& e){
        const auto code=calls::enter(context,before.props.world.root,request.scenery_stroke.finish_position);entered=context.installed;
        return fault(code,"Shard/Cursor context installation",e);
    }
    bool begin_edit(bool& opened,std::string& e){
        uint32_t cleanup=0;const auto code=calls::begin_edit(cursor,opened,cleanup);
        const bool okay=fault(code,"BeginEdit",e);std::string cause;
        if(!fault(cleanup,"BeginEdit label cleanup",cause)){if(e.empty())e=cause;else e+="; cleanup: "+cause;return false;}return okay;
    }
    bool before_stamp(size_t index,std::string& e){
        uintptr_t current_graph=0,active=0,current_root=0;
        if(index>=shapes.size()||!q::at(r.memory,r.image,0x1a3aa80,active,e)||active||!q::at(r.memory,context.manager,0x60,current_root,e)||
           current_root!=before.props.world.root||!q::at(r.memory,cursor.object,cursor.brush?0xc50:0x5c0,current_graph,e)||current_graph!=receipt.graph)
            return q::fail(e,e.empty()?"Scenery original graph/root changed before stamp index="+std::to_string(index):e);
        return true;
    }
    bool add_paint(size_t index,uintptr_t& graph,std::string& e){
        auto& shape=*shapes.at(index);bool entered=false,restored=false;
        const auto code=calls::paint(reinterpret_cast<calls::Paint>(originals[cursor.brush?0:2]),reinterpret_cast<void*>(cursor.object),&shape.value,
            shape.transfer.prepared?&shape.transfer:nullptr,entered,restored);
        if(!fault(code,"AddPaint",e)||!entered||!restored)return q::fail(e,e.empty()?"Scenery original AddPaint did not restore its pattern":e);
        if(shape.transfer.prepared&&!shape.transfer.transferred)return q::fail(e,"Scenery original AddPaint did not consume its generated transforms");
        uintptr_t actual=0;if(!q::at(r.memory,cursor.object,cursor.brush?0xc50:0x5c0,actual,e))return false;
        if(graph&&actual!=graph)return q::fail(e,"Scenery original AddPaint replaced its whole stroke graph");graph=actual;return true;
    }
    bool after_stamp(size_t,uintptr_t,std::string& e){
        if(!cursor.brush)return true;q::Bounds rows;
        if(!q::typed(r.memory,r.image,cursor.object+0xb70,0x1748c50,e)||!q::bounds(r.memory,cursor.object+0xb70,1000000,rows,e))return false;
        if(rows.count<seen_births)return q::fail(e,"Scenery original birth list lost an earlier entry");
        auto find=[&](uint64_t uid,uintptr_t& object,std::string& cause){return group_edit::find(r,uid,object,cause);};
        for(int32_t index=seen_births;index<rows.count;++index){uintptr_t object=0;uint64_t uid=0;Saved saved;std::string key;
            if(!q::at(r.memory,rows.data,size_t(index)*8,object,e)||!q::at(r.memory,object,0x18,uid,e)||
               before.scenery.contains(uid)||births.contains(uid)||!birth_key(request.action_uuid,size_t(index),key,e)||
               !scenery_make_history::live(r.memory,r.image,object,uid,before.props.world.root,before.region,find,saved,e))
                return q::fail(e,e.empty()?"Scenery original ordered birth is stale, repeated or foreign":e);
            if(!births.emplace(uid,saved).second||!aliases.emplace(uid,key).second)return q::fail(e,"Scenery original full birth UID repeats");birth_order.push_back(std::move(key));
        }
        seen_births=rows.count;return q::unchanged(r.memory,cursor.object+0xb70,1000000,rows,e);
    }
    bool read_cash(std::string& e){
        cash.clear();int32_t paid=0,refund=0;std::array<uint32_t,3> refunded_at{};
        if(cursor.brush){if(!q::at(r.memory,cursor.object,0xb8c,paid,e)||!q::at(r.memory,cursor.object,0xb90,refund,e)||!q::at(r.memory,cursor.object,0xb94,refunded_at,e))return false;}
        else if(!q::at(r.memory,cursor.object,0x59c,refund,e)||!q::at(r.memory,cursor.object,0x5a0,refunded_at,e))return false;
        if(paid<0||refund<0)return q::fail(e,"Scenery original stroke cost/refund counter overflowed");
        if(paid)cash.push_back({paid,request.scenery_stroke.finish_position});
        if(refund){for(auto bits:refunded_at)if(!std::isfinite(std::bit_cast<float>(bits)))return q::fail(e,"Scenery original refund position is nonfinite");cash.push_back({-refund,refunded_at});}
        return true;
    }
    bool finish(uintptr_t&,bool& entered,bool& returned,std::string& e){
        if(!read_cash(e))return false;return fault(calls::finish(reinterpret_cast<calls::Finish>(originals[cursor.brush?1:3]),cursor.object,entered,returned),"Finish",e);
    }
    bool readback(uintptr_t graph,std::string& e){
        uintptr_t retained=0,edit=0;
        if(!q::at(r.memory,cursor.object,cursor.brush?0xc50:0x5c0,retained,e)||retained||
           (cursor.brush&&(!q::at(r.memory,cursor.object,0x360,edit,e)||edit)))
            return q::fail(e,e.empty()?"Scenery original Finish retained graph/EditScope":e);
        host::WorldState current;duplicate_reader::Census census;
        auto find=[&](uint64_t uid,uintptr_t& object,std::string& why){return group_edit::find(r,uid,object,why);};
        if(!world(r,current,e)||!host::same_world(before.props.world.origin,current.origin)||
           !duplicate_reader::census(r.memory,r.image,uintptr_t(current.origin.map),current.root,find,census,e))return false;
        std::set<uint64_t> alive;
        for(const auto& [object,row]:census.objects){(void)object;if(row.family==Family::Scenery)alive.insert(row.handle.uid);}
        removed.clear();for(const auto& [uid,saved]:before.scenery)if(!alive.contains(uid))removed.emplace(uid,saved);
        for(const auto& [uid,saved]:births)if(!alive.contains(uid))removed.emplace(uid,saved);
        std::set<uint64_t> born;for(const auto& [uid,saved]:births){(void)saved;born.insert(uid);}
        witness::UndoWitness undo;witness::GraphWitness checked;scenery_stroke_graph::Graph original;
        if(!witness::read_undo(r.memory.process,r.image,undo,e))return false;
        if(graph){if(!witness::read_graph(r.memory.process,r.image,graph,checked,e)||!witness::validate_undo_commit(before.props.undo,undo,checked,e)||
            !scenery_stroke_graph::read(r.memory,r.image,graph,removed,born,cash,original,e))return false;}
        else if(!removed.empty()||!births.empty()||undo!=before.props.undo)return q::fail(e,"Scenery original changed objects/Undo without its complete graph");
        std::map<uint64_t,std::string> prior;
        for(const auto& [uid,saved]:before.scenery){
            const auto found=std::find_if(before.props.handles.begin(),before.props.handles.end(),[&](const auto& row){return row.second.uid==uid&&row.second.object==saved.object;});
            if(found==before.props.handles.end()||!prior.emplace(uid,found->first).second)return q::fail(e,"Scenery before native full UID lacks its canonical alias");
            if(removed.contains(uid))aliases.emplace(uid,found->first);
        }
        auto retire=[&](const auto& ids,auto& keys,std::string& why){return remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,ids,keys,why);};
        auto restore=[](uint64_t,const std::string&,std::string& why){return q::fail(why,"Scenery forward stroke cannot restore a prior native alias");};
        auto records=[&](std::string& why){return sample(r,binding,request.scenery_stroke.region,aliases,after,why)&&
            (same_binding(before,after)||q::fail(why,"Scenery complete Record capture changed its World binding"));};
        if(!scenery_alias_order::capture(prior,alive,{},retire,restore,records,settled_aliases,e))return false;
        no_effect=before.state==after.state&&before.props.money==after.props.money&&before.props.undo==after.props.undo;
        if(no_effect)return q::fail(e,"Scenery stroke changed no objects");
        evidence.before=before.props.money;evidence.after=after.props.money;evidence.scenery_stroke_before=before.state;evidence.scenery_stroke_after=after.state;
        action::SceneryStrokeEffect effect;
        if(!scenery_stroke_state::difference(before.props.objects,after.props.objects,birth_order,digest,effect,e))return false;
        evidence.scenery_stroke_result=std::move(effect);evidence.undo_confirmed=true;
        return true;
    }
    bool restore_context(std::string& e){return fault(calls::restore(context),"Shard/Cursor context restoration",e);}
    bool finish_empty(std::string& e){bool entered=false,returned=false;
        return fault(calls::finish(reinterpret_cast<calls::Finish>(originals[cursor.brush?1:3]),cursor.object,entered,returned),"empty replay Finish",e)&&entered&&returned;
    }
    bool restore_selection(std::string& e){
        std::vector<uintptr_t> desired;
        for(const auto& prior:selection){uintptr_t current=0;if(!group_edit::find(r,prior.uid,current,e))return false;
            if(current==prior.object){desired.push_back(current);continue;}
            if(current||!removed.contains(prior.uid)||removed.at(prior.uid).object!=prior.object)return q::fail(e,"Scenery replay lost an unrelated local selected full UID");
        }
        std::vector<uintptr_t> raw,filtered;std::array<uint8_t,9> current_filter{};
        if(!group_reader::selection(r.memory,r.image,before.props.world.selection,raw,filtered,e))return false;
        if(raw!=desired){const duplicate_calls::Array values{r.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
            if(!fault(duplicate_calls::select(r.image,before.props.world.selection,values),"local Selection restore",e)||
               !group_reader::selection(r.memory,r.image,before.props.world.selection,raw,filtered,e))return false;}
        if(!q::at(r.memory,before.props.world.selection,0xc8,current_filter,e))return false;
        return (raw==desired&&current_filter==filter)||q::fail(e,"Scenery replay changed local Selection/filter");
    }
    bool destroy_cursor(std::string& e){
        if(!cursor.constructed)return true;uintptr_t graph=0,edit=0;
        if(!q::at(r.memory,cursor.object,cursor.brush?0xc50:0x5c0,graph,e)||graph||
           (cursor.brush&&(!q::at(r.memory,cursor.object,0x360,edit,e)||edit)))return q::fail(e,e.empty()?"Scenery uncertain cursor still owns a graph/EditScope; retained":e);
        if(!fault(calls::destroy(cursor),"cursor destructor",e))return false;
        return !cursor.options_constructed||fault(calls::destroy_options(cursor),"options destructor",e);
    }
    bool release_resources(std::string& e){
        bool okay=true;auto retain=[&](const std::string& failed){okay=false;if(e.empty())e=failed;else e+="; "+failed;};
        for(size_t index=0;index<shapes.size();++index){auto& shape=*shapes[index];std::string error;
            if(!fault(calls::release_untransferred(shape.transfer),"untransferred transform release",error))retain("stamp="+std::to_string(index)+" "+error);
            if(shape.constructed&&!shape.destruction_entered){shape.destruction_entered=true;
                if(!fault(calls::shape_destroy(r.image,shape.value),"PolyShape destructor",error))retain("stamp="+std::to_string(index)+" "+error);else shape.destroyed=true;}
        }
        return okay;
    }
    void trim(){
        trim_sample(before);trim_sample(after);shapes.clear();
    }
};
}
}
