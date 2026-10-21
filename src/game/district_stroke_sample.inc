namespace district_stroke {
namespace {
namespace q=district_saved_identity;
namespace identity=district_saved_identity_runtime;
namespace state=district_stroke_state;
namespace proof=district_stroke_proof;
namespace inverse=district_stroke_graph;
namespace calls=district_stroke_calls;
namespace poly=ui_terrain_runtime::poly;
using Kind=action::DistrictStrokeKind;
struct Snapshot {
    host::WorldState world;
    state::Values values;
    std::map<std::string,identity::Current> rows;
    std::map<uint64_t,std::string> keys;
    std::map<uintptr_t,int32_t> regions;
    action::DistrictStrokeState wire;
    action::Ledger money;
    witness::UndoWitness undo;
};
bool same_row(const identity::Current& first,const identity::Current& second){
    return first.key==second.key&&first.saved_region==second.saved_region&&first.row==second.row;
}
bool fault(uint32_t code,const char* operation,std::string& error){
    return !code||q::fail(error,std::string("District stroke original ")+operation+" SEH="+std::to_string(code));
}
bool digest(const json& value,std::string& out,std::string& error){
    const auto text=value.dump();out=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));
    return state::sha(out)||q::fail(error,"District complete canonical SHA256 failed");
}
bool bound_world(Runtime& runtime,const action::Binding& binding,host::WorldState& out,uint8_t own_edit,std::string& error){
    host::NativeOrigin origin;host::LookupApi lookup;
    if(!host::resolve_lookup(runtime.memory.process,runtime.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",lookup,error))return false;
    host::Reader reader(runtime.memory.process,origin,runtime.image,runtime.thread,lookup);
    if(!reader.current(out,false,error))return false;
    return (out.origin.world_epoch==binding.native_epoch&&out.origin.generation==binding.native_generation&&
        !out.saves&&!out.active_graph&&out.edits==own_edit)||q::fail(error,"District complete read World/Save/Edit/Undo scope differs");
}
bool snapshot(Runtime& runtime,const action::Binding& binding,Snapshot& out,std::string& error,
              const identity::Preview* preview=nullptr,uint8_t own_edit=0){
    Snapshot value;if(!bound_world(runtime,binding,value.world,own_edit,error))return false;
    std::vector<identity::Current> rows;
    if(preview){
        if(preview->edit.snapshot!=net::current_snapshot_id())return q::fail(error,"District preview belongs to another snapshot");
        for(const auto& [region,change]:preview->changes)for(const auto& member:change.after.members)
            rows.push_back({member.key,region,member.native});
    }else if(!identity::read_current(rows,error))return false;
    auto contains=[&](uintptr_t shape,uintptr_t point,bool& inside,std::string& cause){
        return fault(district_text::contains_call(runtime.image,shape,point,inside),"title containment",cause);};
    for(const auto& row:rows){
        action::DistrictState first,second;uintptr_t region=0,actual=0;
        if(!group_edit::find(runtime,row.row.uid,actual,error)||actual!=row.row.pointer||
           !q::at(runtime.memory,row.row.pointer,0x178,region,error)||
           !district_reader::read(runtime.memory,runtime.image,uintptr_t(value.world.origin.map),row,contains,first,error)||
           !district_reader::read(runtime.memory,runtime.image,uintptr_t(value.world.origin.map),row,contains,second,error))return false;
        if(first!=second||!value.rows.emplace(row.key,row).second||!value.keys.emplace(row.row.uid,row.key).second)
            return q::fail(error,"District complete values changed or full canonical owner repeats");
        const auto inserted=value.regions.emplace(region,row.saved_region);
        if(!inserted.second&&inserted.first->second!=row.saved_region)return q::fail(error,"District current Region pointer has conflicting canonical IDs");
        value.values.emplace(row.key,std::move(second));
    }
    if(!preview){std::vector<identity::Current> after;if(!identity::read_current(after,error)||rows.size()!=after.size()||
        !std::equal(rows.begin(),rows.end(),after.begin(),same_row))
        return q::fail(error,error.empty()?"District canonical roster changed during full read":error);}
    host::WorldState after;
    if(!bound_world(runtime,binding,after,own_edit,error)||!host::same_world(value.world.origin,after.origin)||
       value.world.root!=after.root||value.world.selection!=after.selection||!ledger(value.money,error)||
       !witness::read_undo(runtime.memory.process,runtime.image,value.undo,error)||value.undo.active_graph)
        return q::fail(error,error.empty()?"District complete World/Undo changed during read":error);
    value.wire.count=uint32_t(value.values.size());if(!digest(proof::values(value.values),value.wire.values_sha,error))return false;
    out=std::move(value);return true;
}
bool same(const Snapshot& first,const Snapshot& second){
    return host::same_world(first.world.origin,second.world.origin)&&first.world.root==second.world.root&&first.world.selection==second.world.selection&&
        first.values==second.values&&first.rows.size()==second.rows.size()&&
        std::equal(first.rows.begin(),first.rows.end(),second.rows.begin(),[](const auto& a,const auto& b){return a.first==b.first&&same_row(a.second,b.second);})&&
        first.money==second.money&&first.undo==second.undo;
}
bool resolve_region(Runtime& runtime,const Snapshot& sample,int32_t id,uintptr_t& out,std::string& error){
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return q::at(runtime.memory,object,offset,value,cause);};
    return region_saved_identity::resolve(runtime.image,uintptr_t(sample.world.origin.map),sample.world.root,id,0,out,at,error);
}
struct LocalSelection {
    struct Member{uintptr_t object=0;uint64_t uid=0;};
    std::vector<Member> members;std::array<uint8_t,9> filters{};
    bool capture(Runtime& runtime,uintptr_t manager,std::string& error){
        std::vector<uintptr_t> raw,filtered;
        if(!group_reader::selection(runtime.memory,runtime.image,manager,raw,filtered,error)||!q::at(runtime.memory,manager,0xc8,filters,error))return false;
        for(auto value:filters)if(value>1)return q::fail(error,"District local selection filter is not Boolean");
        for(auto object:raw){uint64_t uid=0;uintptr_t current=0;
            if(!q::at(runtime.memory,object,0x18,uid,error)||!group_edit::find(runtime,uid,current,error)||current!=object)return q::fail(error,error.empty()?"District local selection full UID differs":error);
            members.push_back({object,uid});}
        return true;
    }
    bool restore(Runtime& runtime,uintptr_t manager,const std::map<uint64_t,std::string>& retired,std::string& error){
        std::vector<uintptr_t> desired;
        for(const auto& row:members){uintptr_t current=0;if(!group_edit::find(runtime,row.uid,current,error))return false;
            if(current==row.object)desired.push_back(current);
            else if(current||!retired.contains(row.uid))return q::fail(error,"District original lost an unrelated local selection full UID");}
        std::vector<uintptr_t> raw,filtered;
        if(!group_reader::selection(runtime.memory,runtime.image,manager,raw,filtered,error))return false;
        if(raw!=desired){duplicate_calls::Array values{runtime.image+0x1748c50,desired.data(),int32_t(desired.size()),int32_t(desired.size())};
            if(!fault(duplicate_calls::select(runtime.image,manager,values),"local selection restore",error)||
               !group_reader::selection(runtime.memory,runtime.image,manager,raw,filtered,error))return false;}
        std::array<uint8_t,9> current{};if(!q::at(runtime.memory,manager,0xc8,current,error))return false;
        return (raw==desired&&current==filters)||q::fail(error,"District original local selection/filter readback differs");
    }
};
}
}
