namespace sample_fixture::clearance_cases {
constexpr uintptr_t graph_owner=0x190000,make_node=0x1a0000,primary_node=0x1b0000;
const std::string action_id="44444444-4444-4444-8444-444444444444",undo_id="55555555-5555-4555-8555-555555555555";
void dependency(Memory& memory,uintptr_t owner,uint64_t uid){
    const auto table=owner*4096+0x10000000,values=table+0x1000,link=table+0x2000;
    memory.put(owner,table);memory.put(owner+8,int32_t(32));memory.put(owner+12,int32_t(27));
    memory.put(owner+0x18,values);memory.put(owner+0x20,int32_t(uid?1:0));memory.put(owner+0x24,int32_t(4));
    for(size_t bucket=0;bucket<32;++bucket)memory.put(table+bucket*24+0x10,uintptr_t(0));
    if(uid){memory.put(values,uid);memory.put(link,uid);memory.put(link+8,uint32_t(uid));memory.put(link+0x10,uintptr_t(0));
        memory.put(table+size_t((uint32_t(uid)*0x9e3779ffu)>>27)*24+0x10,link);}
}
void inverse(Runtime& runtime,const scenery_make_history::Saved& saved,bool make){
    auto& memory=runtime.memory;
    memory.put(primary_node,f::image+(make?0x16de860:0x17065f0));memory.put(primary_node+0xb8,fixture_building_uid);
    memory.put(make_node,f::image+(make?0x1706590:0x16de860));memory.put(make_node+0xb8,saved.uid);
    memory.put(f::image+0x1706590+0x10,f::image+0x55a3c0);memory.put(f::image+0x16de860+0x10,f::image+0x53e1c0);
    if(make){memory.put(make_node+0xc0,saved.type);memory.put(make_node+0xc8,saved.root);memory.put(make_node+0xd0,saved.transform);
        memory.put(make_node+0x13c,saved.height);memory.put(make_node+0x140,saved.automatic);}
    dependency(memory,make_node+8,make?saved.uid:0);dependency(memory,make_node+0x30,0);dependency(memory,make_node+0x58,make?0:saved.uid);
    f::array(memory,graph_owner,0x16e32d0,{});
    f::array(memory,graph_owner+0x18,0x16e2dc0,{primary_node,make_node});f::array(memory,graph_owner+0x30,0x1748aa0,{});
    clearance_graph=graph_owner;
}
Runtime begin(){
    auto runtime=setup();clearance_events.clear();clearance_failure.clear();remove_diagnostics::saved_extra.clear();
    remove_diagnostics::rows.push_back(binding(f::member_uid,scenery_key));
    remove_diagnostics::rows.push_back(binding(f::free_uid,"action:33333333-3333-4333-8333-333333333333"));
    for(auto pointer:{f::scenery,f::free_prop}){
        std::array<uint8_t,0x6c> transform{};const auto pose=f::pose();std::memcpy(transform.data(),pose.data(),sizeof pose);
        runtime.memory.put(pointer+0x100,transform);runtime.memory.put(pointer+0x268,std::array<int32_t,2>{3,1234});
        runtime.memory.put(pointer+0x270,uint32_t(0));runtime.memory.put(pointer+0x275,uint8_t(1));
    }
    return runtime;
}
void set_alive(Runtime& runtime,bool alive){
    if(alive)uid_objects[f::member_uid]=f::scenery;else uid_objects.erase(f::member_uid);
    f::array(runtime.memory,scenery_manager+0x18+3*0x30,0x172d6a0,alive?std::vector<uintptr_t>{f::scenery,f::free_prop}:std::vector<uintptr_t>{f::free_prop});
}
action::Request request(){action::Request value;value.action=action::Action::PlaceBuilding;value.action_uuid=action_id;
    value.snapshot_uuid=net::current_snapshot_id();value.canonical_record_key="action:"+action_id;value.building.type="SmallInn";return value;}
void run(){
    namespace bc=building_clearance;const action::Binding admitted{3,4};std::string error;
    auto runtime=begin();published=&runtime;auto original=request();bc::Snapshot before,after;
    f::need(bc::capture(admitted,original,before,error,nullptr),error);
    f::need(remove_diagnostics::fixture_identity_reads==1&&remove_diagnostics::fixture_refund_reads==0,
        "Building clearance captured unused refund getters");
    std::vector<world_state::LocalBinding> current_rows;
    const world_state::Origin origin{original.snapshot_uuid,std::string(64,'a'),3,4};
    f::need(bc::current_bindings(before,origin,current_rows,error)&&current_rows.size()==2&&
        current_rows[0].state.alive&&current_rows[0].state.type=="cactus"&&current_rows[0].local_uid,
        "Actual before projection lost full live Scenery state: "+error);
    auto foreign=origin;foreign.generation++;
    f::need(!bc::current_bindings(before,foreign,current_rows,error)&&current_rows.empty(),"Foreign World projection was admitted");
    const auto saved=before.local->live.at(f::member_uid);
    set_alive(runtime,false);inverse(runtime,saved,true);
    f::need(bc::settle(admitted,original,before,nullptr,fixture_building_uid,graph_owner,after,error)&&
        clearance_events==std::vector<std::string>{"retire"},"Exact graph clearance aliases did not settle before Record read: "+error);
    f::need(bc::finish(admitted,original,before,after,nullptr,error)&&after.local->effect.removed.size()==1&&
        after.local->effect.removed[0].key==scenery_key&&after.local->effect.created.empty(),error);
    action::SceneryStrokeEffect observed;
    f::need(bc::observe(admitted,original,after,observed,error)&&observed==after.local->effect,error);
    f::need(remove_diagnostics::fixture_identity_reads==3&&remove_diagnostics::fixture_refund_reads==0,
        "Building clearance finish/observe bypassed current identities or hydrated unused refunds");
    auto undo=original;undo.action=action::Action::UndoBuilding;undo.action_uuid=undo_id;undo.target_action_uuid=original.action_uuid;
    bc::Snapshot history_before,history_after;
    f::need(bc::prepare_history(admitted,undo,after,history_before,error,nullptr),error);
    set_alive(runtime,true);inverse(runtime,saved,false);
    f::need(bc::settle(admitted,undo,history_before,&after,fixture_building_uid,graph_owner,history_after,error),error);
    f::need(bc::finish(admitted,undo,history_before,history_after,nullptr,error)&&history_after.local->effect.created==after.local->effect.removed&&
        clearance_events==std::vector<std::string>{"retire","restore"},"Undo did not restore the exact original complete Record: "+error);
    auto redo=undo;redo.action=action::Action::RedoBuilding;redo.action_uuid="66666666-6666-4666-8666-666666666666";
    bc::Snapshot redo_before,redo_after;
    f::need(bc::prepare_history(admitted,redo,after,redo_before,error,nullptr),error);
    set_alive(runtime,false);inverse(runtime,saved,true);
    f::need(bc::settle(admitted,redo,redo_before,&after,fixture_building_uid,graph_owner,redo_after,error)&&
        bc::finish(admitted,redo,redo_before,redo_after,nullptr,error)&&redo_after.local->effect.removed==after.local->effect.removed,error);
    f::need(clearance_events==std::vector<std::string>{"retire","restore","retire"},"Original history clearance alias sequence differs");
    f::need(remove_diagnostics::fixture_identity_reads==7&&remove_diagnostics::fixture_refund_reads==0,
        "Building Undo/Redo lost fresh canonical samples or invoked unused refund getters");
    for(const std::string failure:{"graph","retire"}){
        runtime=begin();published=&runtime;before={};after={};f::need(bc::capture(admitted,original,before,error,nullptr),error);
        const auto expected=before.local->live.at(f::member_uid);set_alive(runtime,false);inverse(runtime,expected,true);clearance_failure=failure;
        f::need(!bc::settle(admitted,original,before,nullptr,fixture_building_uid,graph_owner,after,error)&&!after.local&&
            error==(failure=="graph"?"original graph read failed":"original absence proof failed"),"Original settlement failure was hidden: "+error);
    }
    runtime=begin();published=&runtime;before={};f::need(bc::capture(admitted,original,before,error,nullptr),error);
    set_alive(runtime,false);inverse(runtime,before.local->live.at(f::member_uid),true);runtime.memory.put(make_node+0xb8,uint64_t(999));
    f::need(!bc::settle(admitted,original,before,nullptr,fixture_building_uid,graph_owner,after,error)&&clearance_events.empty(),"Foreign graph retired an alias");
    runtime=begin();published=&runtime;before={};f::need(bc::capture(admitted,original,before,error,nullptr),error);
    action::Evidence authority;authority.building_clearance=observed;authority.building_clearance->removed[0].record_sha=std::string(64,'d');
    bc::Snapshot client;
    f::need(!bc::capture(admitted,original,client,error,&authority)&&error.find("Client original Record/pose differs")!=std::string::npos&&clearance_events.empty(),"Client mismatched original Record was admitted");
    runtime=begin();published=&runtime;before={};clearance_failure="metadata";
    f::need(!bc::capture(admitted,original,before,error,nullptr)&&error=="original saved Root metadata proof failed","Saved Root failure was hidden");
    runtime=begin();published=&runtime;auto scenery=original;scenery.building.type="cactus";
    f::need(bc::capture(admitted,scenery,before,error,nullptr)&&!before.local,"Ordinary Scenery placement was captured as Building clearance");
    published=nullptr;
}
}
