namespace duplicate_edit {
namespace {
struct Port {
    Runtime& r;const action::Binding& binding;const action::Request& request;Sample& before;Sample after;
    duplicate_native::Receipt& receipt;bool source_peer=false;
    std::map<uint64_t,std::string> created;
    std::map<uint64_t,child_npc::Receipt> children;
    std::vector<uintptr_t> prior_selection,new_selection,cash_ops,destroy_ops;
    std::vector<uint64_t> prior_uids;
    std::map<std::string,int32_t> destination_regions;
    uintptr_t local_shard=0;
    bool fault(uint32_t code,const char* stage,std::string& e){return !code||q::fail(e,std::string("Duplicate original ")+stage+" failed SEH="+std::to_string(code));}
    bool ready(const action::DuplicateIntent& intent,const std::vector<Handle>& sources,int32_t total,std::string& e){
        if(!before.business||!before.undo.receiver||before.undo.active_graph||!duplicate_code::check(r.memory,r.image,e))return false;
        for(size_t i=0;i<sources.size();++i){uintptr_t found=0;bool allowed=false;int32_t cost=0;
            if(!group_edit::find(r,sources[i].uid,found,e)||found!=sources[i].object||!duplicate_code::target(r.memory,r.image,found,e)||
               !fault(duplicate_calls::cost(found,cost,allowed),"CanDuplicate/Cost",e))return false;
            if(!allowed||cost!=intent.rows[i].cost)return q::fail(e,"Duplicate original CanDuplicate/Cost differs index="+std::to_string(i));
            const auto& row=intent.rows[i];const std::array<uint32_t,3> point{row.after[4],row.after[5],row.after[6]};
            uintptr_t region=0,root=0;int32_t id=-1;
            if(!fault(duplicate_calls::region(r.image,before.world.root,row.family==Family::Scenery,point,region),"destination Region getter",e)||
               !region||std::find(before.census.regions.begin(),before.census.regions.end(),region)==before.census.regions.end()||
               !q::typed(r.memory,r.image,region,0x1787930,e)||!q::at(r.memory,region,0x70,root,e)||root!=before.world.root||
               !q::at(r.memory,region,0x834,id,e)||id<0)return q::fail(e,e.empty()?"Duplicate original destination Region is absent from current complete root":e);
            destination_regions.emplace(row.key,id);
        }
        bool affordable=false;if(!fault(duplicate_calls::afford(r.image,before.business,total,affordable),"CanAfford",e)||!affordable)return q::fail(e,e.empty()?"Duplicate original total cannot be afforded":e);
        uintptr_t manager=0;if(!q::at(r.memory,r.image,0x13df440,manager,e)||!q::at(r.memory,manager,0x60,local_shard,e))return false;
        std::vector<uintptr_t> filtered;if(!group_reader::selection(r.memory,r.image,before.world.selection,prior_selection,filtered,e))return false;
        for(const auto p:prior_selection){uint64_t uid=0;uintptr_t found=0;
            if(!q::at(r.memory,p,0x18,uid,e)||!group_edit::find(r,uid,found,e)||found!=p)return q::fail(e,e.empty()?"Duplicate local Selection full UID differs":e);prior_uids.push_back(uid);}
        return true;
    }
    bool charge(int32_t amount,std::string& e){bool paid=false;
        return fault(duplicate_calls::charge(r.image,before.business,amount,paid),"SubtractCash",e)&&(paid||q::fail(e,"Duplicate original SubtractCash returned false"));}
    bool begin_edit(bool& opened,std::string& e){uint32_t cleanup=0;const auto code=duplicate_calls::begin(r.image,opened,cleanup);
        if(!fault(code,"BeginEdit",e)){if(cleanup)e+="; string cleanup SEH="+std::to_string(cleanup);return false;}return fault(cleanup,"BeginEdit string cleanup",e);}
    bool end_edit(std::string& e){return fault(duplicate_calls::end(r.image),"EndEdit",e);}
    bool create_graph(uintptr_t& graph,std::string& e){return fault(duplicate_calls::graph(r.image,graph),"DoGraph constructor",e)&&(graph||q::fail(e,"Duplicate original graph is null"));}
    bool cash_inverse(uintptr_t graph,int32_t amount,const std::array<uint32_t,3>& position,std::string& e){uintptr_t op=0;
        if(!fault(duplicate_calls::cash(r.image,graph,amount,position,op),"DoCash/AddDo",e))return false;cash_ops.push_back(op);return true;}
    bool duplicate(const Handle& source,const action::DuplicateTarget& target,Handle& result,std::string& e){
        action::BuildingState pose;pose.alive=true;pose.type="duplicate";pose.variant="original";
        for(size_t i=0;i<4;++i)pose.rotation[i]=std::bit_cast<float>(target.after[i]);
        pose.x=std::bit_cast<float>(target.after[4]);pose.y=std::bit_cast<float>(target.after[5]);pose.z=std::bit_cast<float>(target.after[6]);
        for(size_t i=0;i<3;++i)pose.scale[i]=std::bit_cast<float>(target.after[i+7]);
        std::array<unsigned char,0x6c> transform;if(!move_kiss::transform(pose,transform,e))return false;
        if(!fault(duplicate_calls::duplicate(source.object,transform,result.object),"virtual Duplicate",e))return false;
        uintptr_t found=0;if(!result.object||!q::at(r.memory,result.object,0x18,result.uid,e)||!result.uid||
           !group_edit::find(r,result.uid,found,e)||found!=result.object)return q::fail(e,e.empty()?"Duplicate original result full UID is not registered":e);
        if(target.family==Family::Group){
            if(!q::typed(r.memory,r.image,result.object,0x1749de0,e))return false;q::Bounds members;
            if(!q::bounds(r.memory,result.object+0x210,100000,members,e)||size_t(members.count)!=target.created_keys.size())return q::fail(e,e.empty()?"Duplicate original Group member count differs":e);
            for(int32_t i=0;i<members.count;++i){uintptr_t member=0,p=0,parent=0;uint64_t uid=0;
                if(!q::at(r.memory,members.data,size_t(i)*8,member,e)||!q::typed(r.memory,r.image,member,0x16fbc40,e)||
                   !q::at(r.memory,member,0x18,p,e)||!q::typed(r.memory,r.image,p,0x16c1390,e)||!q::at(r.memory,p,0x18,uid,e)||
                   !q::at(r.memory,p,0x180,parent,e)||parent!=result.object||!uid||!created.emplace(uid,target.created_keys[size_t(i)]).second)
                    return q::fail(e,e.empty()?"Duplicate original Group child full UID/reciprocal owner differs":e);
            }
            if(!q::unchanged(r.memory,result.object+0x210,100000,members,e))return false;
        }else if(!created.emplace(result.uid,target.created_keys.front()).second)return q::fail(e,"Duplicate original full UID repeated");
        return true;
    }
    bool destroy_inverse(uintptr_t graph,const Handle& object,std::string& e){uintptr_t op=0;
        if(!fault(duplicate_calls::destroy_inverse(r.image,graph,object.object,op),"DoDestroy/AddDo",e))return false;destroy_ops.push_back(op);return true;}
    bool select_created(const std::vector<Handle>& objects,std::string& e){for(const auto& h:objects)new_selection.push_back(h.object);
        const duplicate_calls::Array array{r.image+0x1748c50,new_selection.data(),int32_t(new_selection.size()),int32_t(new_selection.size())};
        return fault(duplicate_calls::select(r.image,before.world.selection,array),"SetSelection",e);}
    bool release_points(const action::DuplicateIntent& intent,const std::vector<Handle>&,std::string& e){
        bool building=false,scenery=false;for(const auto& row:intent.rows){building|=row.family==Family::Building;scenery|=row.family==Family::Scenery;}
        if(scenery&&!fault(duplicate_calls::release(r.image,before.release+0x108,2),"Scenery AddEdit",e))return false;
        return !building||fault(duplicate_calls::release(r.image,before.release+0x108,1),"Building AddEdit",e);
    }
    bool commit_graph(uintptr_t graph,const std::vector<Handle>&,std::string& e){uint32_t cleanup=0;
        const duplicate_calls::Array array{r.image+0x1748c50,new_selection.data(),int32_t(new_selection.size()),int32_t(new_selection.size())};
        const auto code=duplicate_calls::commit(r.image,before.undo.receiver,graph,array,cleanup);
        if(!fault(code,"MakeUndoString/AddUndo",e)){if(cleanup)e+="; string cleanup SEH="+std::to_string(cleanup);return false;}return fault(cleanup,"Undo label cleanup",e);
    }
    bool inverse(std::string& e){
        witness::GraphWitness graph;if(!witness::read_graph(r.memory.process,r.image,receipt.graph,graph,e)||
           !witness::validate_undo_commit(before.undo,after.undo,graph,e))return false;
        q::Bounds ops;if(!q::bounds(r.memory,receipt.graph+0x18,100000,ops,e)||size_t(ops.count)!=cash_ops.size()+destroy_ops.size())return q::fail(e,e.empty()?"Duplicate original inverse count differs":e);
        for(int32_t i=0;i<ops.count;++i){uintptr_t op=0;if(!q::at(r.memory,ops.data,size_t(i)*8,op,e))return false;
            if(size_t(i)<cash_ops.size()){int32_t amount=0;std::array<uint32_t,3> position{};
                if(op!=cash_ops[size_t(i)]||!q::typed(r.memory,r.image,op,0x1786fb0,e)||!q::at(r.memory,op,0xb8,amount,e)||!q::at(r.memory,op,0xbc,position,e))return false;
                int64_t cost=0;std::array<uint32_t,3> expected;
                if(request.duplicate.rows.size()>9){for(const auto& row:request.duplicate.rows)cost+=row.cost;expected=request.duplicate.cash_position;}
                else{const auto& row=request.duplicate.rows[size_t(i)];cost=row.cost;expected={row.after[4],row.after[5],row.after[6]};}
                if(amount!=cost||position!=expected)return q::fail(e,"Duplicate original cash inverse amount/position differs");
            }else{const size_t n=size_t(i)-cash_ops.size();uint64_t uid=0;
                if(op!=destroy_ops[n]||!q::typed(r.memory,r.image,op,0x16de860,e)||!q::at(r.memory,op,0xb8,uid,e)||uid!=receipt.created[n].uid)
                    return q::fail(e,e.empty()?"Duplicate original destroy inverse full UID/order differs":e);}
        }
        return q::unchanged(r.memory,receipt.graph+0x18,100000,ops,e);
    }
    bool readback(const action::DuplicateIntent& intent,const std::vector<Handle>&,const duplicate_native::Receipt&,std::string& e){
        auto birth=[&](Sample& current,std::string& why){
            auto owner=[&](uintptr_t p,std::string& key,std::string& cause){return external_owner(r,current,p,key,cause);};
            for(const auto& [uid,key]:created){const auto row=current.objects.find(key);
                if(row==current.objects.end())return q::fail(why,"Duplicate newborn canonical prop is absent before Record capture");
                if(row->second.family!=Family::Building)continue;
                child_npc::Parent parent;child_npc::Receipt child;
                if(!child_parent(r,current,key,parent,why)||
                   !child_npc::capture_birth(child_lease(binding,request,request.action_uuid),parent,owner,child,why)||
                   !child_alias(current,child,why)||!children.emplace(uid,std::move(child)).second)
                    return q::fail(why,why.empty()?"Duplicate newborn Building child receipt repeats":why);
            }return true;
        };
        if(!sample_duplicate(r,binding,&intent,created,after,e,birth)||!host::same_world(before.world.origin,after.world.origin)||before.business!=after.business||
           before.release!=after.release||before.random!=after.random)return q::fail(e,e.empty()?"Duplicate original complete owner binding changed":e);
        if(after.census.objects.size()!=before.census.objects.size()+created.size())return q::fail(e,"Duplicate original complete native manager count differs");
        for(const auto& [p,row]:before.census.objects){const auto found=after.census.objects.find(p);if(found==after.census.objects.end()||found->second!=row)return q::fail(e,"Duplicate replaced or changed existing full native prop");}
        for(const auto& [key,handle]:before.handles){const auto found=after.handles.find(key);if(found==after.handles.end()||found->second!=handle)return q::fail(e,"Duplicate replaced existing Group/Prop full UID");}
        size_t groups=0;for(const auto& row:intent.rows)groups+=row.family==Family::Group;
        if(after.groups.state.groups.size()!=before.groups.state.groups.size()+groups)return q::fail(e,"Duplicate original complete Group census count differs");
        for(const auto& row:intent.rows){const auto key=row.family==Family::Group?"group:"+row.created_keys.front():row.created_keys.front();
            if(after.objects.at(key).region!=destination_regions.at(row.key))return q::fail(e,"Duplicate original created Region differs from preflight key="+key);}
        return duplicate_transition::effect(intent,before.state,after.state,e)&&inverse(e);
    }
    bool restore_selection(std::string& e){
        const auto& desired=source_peer&&receipt.graph_committed?new_selection:prior_selection;
        for(size_t i=0;i<prior_selection.size();++i){uintptr_t found=0;if(!group_edit::find(r,prior_uids[i],found,e)||found!=prior_selection[i])return q::fail(e,e.empty()?"Duplicate local prior Selection full UID changed":e);}
        const duplicate_calls::Array array{r.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
        host::WorldState boundary;bool admitted=false;
        auto guard=[&](std::string& why){
            if(GetCurrentThreadId()!=r.thread||!r.ready||request.snapshot_uuid!=net::current_snapshot_id())
                return q::fail(why,"Duplicate local Selection RNG current thread/session differs");
            host::NativeOrigin origin;host::LookupApi api;host::WorldState current;
            if(!host::resolve_lookup(r.memory.process,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",api,why))return false;
            host::Reader reader(r.memory.process,origin,r.image,r.thread,api);
            uintptr_t owner=0;
            if(!reader.current(current,false,why)||!q::at(r.memory,r.image,0x164b250,owner,why))return false;
            if(!host::same_world(before.world.origin,current.origin)||current.root!=before.world.root||
               current.selection!=before.world.selection||!owner||owner!=before.random)
                return q::fail(why,"Duplicate local Selection RNG current World/owner differs");
            if(admitted&&(current.saves!=boundary.saves||current.edits!=boundary.edits||current.active_graph!=boundary.active_graph))
                return q::fail(why,"Duplicate local Selection RNG save/edit/graph boundary changed");
            boundary=std::move(current);admitted=true;return true;
        };
        auto read=[&](std::array<uint64_t,4>& value,std::string& why){return q::at(r.memory,before.random,0,value,why);};
        auto invoke=[&](std::string& why){return fault(duplicate_calls::select(r.image,before.world.selection,array),"local Selection restore",why);};
        auto write=[&](const std::array<uint64_t,4>& value,std::string& why){return fault(duplicate_calls::restore_random(before.random,value),"local Selection RNG restore",why);};
        if(!duplicate_native::restore_selection_random(guard,read,invoke,write,e))return false;
        std::vector<uintptr_t> raw,filtered;uintptr_t manager=0,shard=0;
        if(!group_reader::selection(r.memory,r.image,before.world.selection,raw,filtered,e)||!q::at(r.memory,r.image,0x13df440,manager,e)||!q::at(r.memory,manager,0x60,shard,e))return false;
        return (raw==desired&&shard==local_shard)||q::fail(e,"Duplicate local Selection/current Shard restoration differs");
    }
};
}
}
