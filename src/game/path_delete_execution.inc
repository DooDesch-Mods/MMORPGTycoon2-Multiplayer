namespace {
uint32_t delete_token(uintptr_t image,void* record,uintptr_t& text){
    __try{text=uintptr_t(reinterpret_cast<const void*(*)(void*)>(image+0x96d880)(record));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t delete_quote(uintptr_t image,uintptr_t road,int32_t& price,action::PathPoint& position){
    __try{price=reinterpret_cast<int32_t(*)(void*)>(image+0x43c680)(reinterpret_cast<void*>(road));
        reinterpret_cast<void*(*)(void*,void*)>(image+0x43c470)(position.data(),reinterpret_cast<void*>(road));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t delete_original(uintptr_t image,uintptr_t road,bool& returned){
    const region::NativeString command{"Destroy",7,{}},edit{"Destroy path",12,{}};
    __try{
        reinterpret_cast<void(*)(const void*)>(image+0x265300)(&edit);
        returned=reinterpret_cast<bool(*)(void*,const void*)>(image+0x43a2e0)(reinterpret_cast<void*>(road),&command);
        reinterpret_cast<void(*)()>(image+0x265650)();return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool delete_binding(const Graph& g,const action::PathDeleteIntent& p,std::string& e){
    return action::valid_path_delete(p)&&g.state.graph_sha256==p.graph_before_sha256&&size_t(p.path_index)<g.paths.size()||fail(e,"path delete target/index/graph changed");
}
bool delete_capture(region::Runtime& r,uintptr_t window,std::string& e){
    bool mixed=false;
    if(!world_path_runtime::capture_remove(r,window,mixed,e))return false;
    if(mixed)return true;
    const auto controller=typed_action_runtime::status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||path.pending||path.captured||path.delete_pending||path.ui_receiver)return fail(e,"path delete UI requires idle bound Controller");
    uintptr_t road=0,proxy=0,live=0,owner=0;int32_t id=-1;
    if(!region::at(r,window,0x4f0,road,e)||!region::at(r,window,0x4f8,proxy,e)||!region::at(r,proxy,0,live,e)||!road||live!=road+0x20||!region::at(r,road,0x178,owner,e)||!region::at(r,owner,0x834,id,e))return fail(e,"path delete selected native road unavailable: "+e);
    region::LocalView view;Graph actual;undo::UndoWitness history;
    if(!region::identity(r,id,owner,view,e)||!graph(r,view,actual,e)||!undo::read_undo(r.memory,r.image,history,e))return false;
    if(history.active_graph)return fail(e,"path delete UI requires idle Undo");
    const auto found=std::find(actual.paths.begin(),actual.paths.end(),road);
    if(found==actual.paths.end())return fail(e,"path delete selected object is outside native Road graph");
    path.delete_pending=PendingDelete{{id,int32_t(found-actual.paths.begin()),actual.state.graph_sha256},{view.world.origin.world_epoch,view.world.origin.generation},net::current_snapshot_id(),path.token};
    ++path.capture_count;return true;
}
bool remove_ui_entry(void* window,void* record){
    if(!path.ready||!net::session_wire::connected()||path.replay)return path.remove_ui(window,record);
    std::string e;auto* r=runtime(e);if(!r){rejected(e);return true;}
    uintptr_t text=0;if(!native_error(delete_token(r->image,record,text),"delete UI token",e)){rejected(e);return true;}
    std::string command;if(!region::text(*r,text,command,e)){rejected(e);return true;}
    if(command!="Destroy")return path.remove_ui(window,record);
    uintptr_t road=0;if(!region::at(*r,uintptr_t(window),0x4f0,road,e)){rejected(e);return true;}
    if(!road)return path.remove_ui(window,record);
    try{if(!delete_capture(*r,uintptr_t(window),e))rejected(e);}
    catch(const std::exception& x){rejected(std::string("path delete UI capture exception: ")+x.what());}
    catch(...){rejected("path delete UI capture nonstandard exception");}
    return true;
}
bool delete_delta(const Graph& before,const Graph& after,const action::PathDeleteIntent& intent,std::vector<uint64_t>& removed,std::string& e){
    if(!delete_binding(before,intent,e))return false;
    const size_t target=size_t(intent.path_index);const auto& road=before.rows.at("paths")[target];
    std::set<uintptr_t> removed_nodes;
    for(const auto& end:{road[0].get<size_t>(),road[1].get<size_t>()})if(before.rows.at("nodes")[end][1].size()==1)removed_nodes.insert(before.nodes[end]);
    if(after.paths.size()+1!=before.paths.size()||after.nodes.size()+removed_nodes.size()!=before.nodes.size())return fail(e,"path delete removed road/orphan node count differs");
    removed={before.path_uids[target]};
    std::map<uintptr_t,size_t> paths,nodes;
    for(size_t i=0;i<after.paths.size();++i)paths.emplace(after.paths[i],i);
    for(size_t i=0;i<after.nodes.size();++i)nodes.emplace(after.nodes[i],i);
    if(paths.contains(before.paths[target]))return fail(e,"path delete target remains in graph");
    for(size_t i=0;i<before.nodes.size();++i){
        if(removed_nodes.contains(before.nodes[i])){if(nodes.contains(before.nodes[i]))return fail(e,"path delete orphan node remains");removed.push_back(before.node_uids[i]);continue;}
        if(!nodes.contains(before.nodes[i]))return fail(e,"path delete removed a surviving node");
        const auto current=nodes.at(before.nodes[i]);
        if(after.node_uids[current]!=before.node_uids[i]||before.rows.at("nodes")[i][0]!=after.rows.at("nodes")[current][0])return fail(e,"path delete surviving node UID/position changed");
        std::multiset<size_t> expected,actual;
        for(const auto& edge:before.rows.at("nodes")[i][1]){const auto index=edge.get<size_t>();if(index!=target){if(!paths.contains(before.paths[index]))return fail(e,"path delete lost node adjacency survivor");expected.insert(paths.at(before.paths[index]));}}
        for(const auto& edge:after.rows.at("nodes")[current][1])actual.insert(edge.get<size_t>());
        if(expected!=actual)return fail(e,"path delete surviving node adjacency differs");
    }
    for(size_t i=0;i<before.paths.size();++i){if(i==target)continue;
        if(!paths.contains(before.paths[i]))return fail(e,"path delete removed another road");
        const auto current=paths.at(before.paths[i]);auto expected=before.rows.at("paths")[i];
        const auto a=before.nodes[expected[0].get<size_t>()],b=before.nodes[expected[1].get<size_t>()];
        if(!nodes.contains(a)||!nodes.contains(b))return fail(e,"path delete lost surviving road endpoint");
        expected[0]=nodes.at(a);expected[1]=nodes.at(b);
        if(after.path_uids[current]!=before.path_uids[i]||after.rows.at("paths")[current]!=expected)return fail(e,"path delete surviving road UID/content changed");
    }return true;
}
bool delete_inverse(region::Runtime& r,const undo::UndoWitness& before,const undo::UndoWitness& after,const Operation& op,const action::PathPoint& position,std::string& e){
    if(after.undo.empty())return fail(e,"path delete original Undo history is empty");
    undo::GraphWitness witness;const auto address=after.undo.back();
    if(!undo::read_graph(r.memory,r.image,address,witness,e)||!undo::validate_undo_commit(before,after,witness,e))return false;
    const size_t expected=op.uids.size()+size_t(op.evidence.path_after.cost!=0);
    if(witness.operations!=expected)return fail(e,"path delete inverse operation count actual="+std::to_string(witness.operations)+" required="+std::to_string(expected));
    uintptr_t data=0;if(!region::at(r,address,0x20,data,e))return false;
    std::set<uint64_t> made;unsigned cash=0;
    for(size_t i=0;i<expected;++i){uintptr_t p=0,ap=0;if(!region::at(r,data,i*8,p,e)||!region::at(r,p,0,ap,e))return false;
        if(ap==r.image+0x16e49c0||ap==r.image+0x172bf40){uint64_t uid=0;if(!region::at(r,p,0xb8,uid,e)||!made.insert(uid).second)return fail(e,"path delete inverse duplicate UID: "+e);if((uid==op.uids.front())!=(ap==r.image+0x16e49c0))return fail(e,"path delete inverse road/node type differs");}
        else if(ap==r.image+0x1786fb0){int32_t amount=0;action::PathPoint point{};if(!region::at(r,p,0xb8,amount,e)||!region::at(r,p,0xbc,point,e))return false;
            if(int64_t(amount)!=-int64_t(op.evidence.path_after.cost)||point!=position)return fail(e,"path delete inverse cash amount/position differs");++cash;}
        else return fail(e,"path delete inverse contains unexpected operation AP="+std::to_string(ap));
    }
    return cash==unsigned(op.evidence.path_after.cost!=0)&&made==std::set<uint64_t>(op.uids.begin(),op.uids.end())||fail(e,"path delete inverse UID set differs");
}
uint32_t delete_select(uintptr_t image,uintptr_t manager,uintptr_t road){
    __try{
        reinterpret_cast<void(*)(void*,void*)>(image+0x2d5c60)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(road));
        reinterpret_cast<void(*)(void*)>(image+0x2d28b0)(reinterpret_cast<void*>(manager));return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct DeleteUIReceipt {bool constructed=false,attempted=false,returned=false,accepted=false,cleaned=false;uint32_t fault=0,cleanup_fault=0;};
uint32_t delete_ui_command(uintptr_t image,uintptr_t window,DeleteUIReceipt& out){
    alignas(8) unsigned char record[0x78]{};
    __try{
        reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,"Destroy");out.constructed=true;
        out.attempted=true;out.accepted=reinterpret_cast<RemoveUI>(image+0x7b04b0)(reinterpret_cast<void*>(window),record);out.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){out.fault=GetExceptionCode();}
    if(out.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);out.cleaned=true;}__except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_fault=GetExceptionCode();}}
    return out.fault?out.fault:out.cleanup_fault;
}
}
bool native_delete(const json& args,json& out,std::string& e){
    if(!args.is_object()||args.size()!=3||!args.contains("token")||!args.contains("id")||!args.contains("path_index")||!args.at("token").is_string()||!args.at("id").is_number_integer()||!args.at("path_index").is_number_integer())return fail(e,"path delete UI requires exact token/id/path_index");
    const auto token=args.at("token").get<std::string>();const auto id=args.at("id").get<int64_t>(),index=args.at("path_index").get<int64_t>();
    if(token.empty()||token.size()>64||token.find('\0')!=std::string::npos||id<0||id>INT32_MAX||index<0||index>=1000000||path.jobs.contains(token))return fail(e,"path delete UI token/id/index invalid or already attempted");
    auto* r=runtime(e);if(!r||!path.ready||unresolved()||!code(*r,true,e))return fail(e,"path delete UI runtime unavailable: "+e);
    const auto controller=typed_action_runtime::status();if(!controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true))return fail(e,"path delete UI requires idle bound Controller");
    region::LocalView view;Graph before;action::Ledger money_before,money_after;undo::UndoWitness undo_before,undo_after;uintptr_t manager=0,handler=0,window=0,target=0,proxy=0,live=0;
    if(!region::read(*r,int32_t(id),0,view,e)||!graph(*r,view,before,e)||size_t(index)>=before.paths.size()||!region::ledger(money_before,e)||!undo::read_undo(r->memory,r->image,undo_before,e)||!region::at(*r,r->image,0x13df630,manager,e)||!native::ap(r->memory,manager,r->image,0x17205d0,e))return fail(e,"path delete UI target/before read failed: "+e);
    auto& job=path.jobs[token];job={{"token",token},{"id",id},{"path_index",index},{"attempted",true},{"submitted",false},{"confirmed",false}};out=job;
    auto failed=[&]{job["cause"]=e;out=job;return false;};
    const auto road=before.paths[size_t(index)];
    if(!native_error(delete_select(r->image,manager,road),"delete SelectProp/SetInfoWindow",e))return failed();
    if(!region::at(*r,manager,0xb8,handler,e)||!region::at(*r,handler,0x10,window,e)||!native::ap(r->memory,window,r->image,0x1714810,e)||!region::at(*r,window,0x4f0,target,e)||!region::at(*r,window,0x4f8,proxy,e)||!region::at(*r,proxy,0,live,e)||target!=road||live!=road+0x20){if(e.empty())e="path delete native InfoWindow target differs";return failed();}
    path.token=token;struct ResetToken{~ResetToken(){path.token.clear();}} reset_token;
    DeleteUIReceipt receipt;const auto seh=delete_ui_command(r->image,window,receipt);
    job["ui_command_attempted"]=receipt.attempted;job["ui_command_returned"]=receipt.returned;job["ui_command_accepted"]=receipt.accepted;job["record_cleanup_confirmed"]=receipt.cleaned;job["exception"]=receipt.fault;job["cleanup_exception"]=receipt.cleanup_fault;
    if(!native_error(seh,"delete native UICommand/record cleanup",e)||!receipt.returned||!receipt.accepted||!receipt.cleaned){if(e.empty())e="path delete native UICommand receipt incomplete";return failed();}
    Graph after;if(!graph(*r,view,after,e)||!region::ledger(money_after,e)||!undo::read_undo(r->memory,r->image,undo_after,e))return failed();
    if(before.state!=after.state||money_before!=money_after||undo_before!=undo_after||!path.delete_pending||path.delete_pending->intent!=action::PathDeleteIntent{int32_t(id),int32_t(index),before.state.graph_sha256}){e="path delete UI capture changed native world or differs from selected target";return failed();}
    job["captured"]=true;out=job;return true;
}
action::Outcome perform_remove(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=runtime(e);if(!r||!path.ready||!action::valid_path_delete(request.path_delete)){e="path delete runtime/intent unavailable: "+e;return action::Outcome::RejectedNoEffect;}
    if(path.operations.contains(request.action_uuid)){e="path delete original action already attempted; no replay";return action::Outcome::Unknown;}
    if(!code(*r,true,e))return action::Outcome::RejectedNoEffect;
    const auto controller=typed_action_runtime::status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="path delete lacks exact Controller request";return action::Outcome::RejectedNoEffect;}
    region::LocalView before;Graph old;action::TerrainState state;action::Ledger money;undo::UndoWitness undo_before;uintptr_t global=0;
    if(!region::read(*r,request.path_delete.region,0,before,e)||!region::bound(before,binding,request.snapshot_uuid,e)||!region::ledger(money,e)||!graph(*r,before,old,e)||!delete_binding(old,request.path_delete,e)||!terrain::terrain_state(*r,before,state,global,e)||!undo::read_undo(r->memory,r->image,undo_before,e))return action::Outcome::RejectedNoEffect;
    if(undo_before.active_graph){e="path delete requires idle Undo";return action::Outcome::RejectedNoEffect;}
    if(!money.record_present||!action::path_receipt_fits(request,money,before.state,e)){if(e.empty())e="path delete requires native cash record";return action::Outcome::RejectedNoEffect;}
    if(authority&&(money!=authority->before||old.state!=authority->path_before||!action::same_region_content(before.state,authority->region_before)||!action::same_terrain_content(state,authority->terrain_before))){e="path delete client before content differs from Host";return action::Outcome::RejectedNoEffect;}
    const auto road=old.paths[size_t(request.path_delete.path_index)];int32_t price=-1;action::PathPoint position{};
    if(!native_error(delete_quote(r->image,road,price,position),"delete native Cost/CostPosition",e)||price<0)return action::Outcome::RejectedNoEffect;
    auto& op=path.operations[request.action_uuid];op.binding=binding;op.request=request;op.evidence.before=money;op.evidence.region_before=authority?authority->region_before:before.state;op.evidence.terrain_before=authority?authority->terrain_before:state;op.evidence.path_before=old.state;
    op.attempted=true;auto unknown=[&]{op.cause=e;return action::Outcome::Unknown;};
    if(authority&&!native_error(terrain::seed(before.random,global,*authority),"delete authority RNG seed",e))return unknown();
    path.replay=&op;struct Reset{~Reset(){path.replay=nullptr;}} reset;bool returned=false;
    if(!native_error(delete_original(r->image,road,returned),"delete original Command",e)||!returned){if(e.empty())e="path delete original Command returned false";return unknown();}
    region::LocalView after;Graph current;undo::UndoWitness undo_after;uintptr_t after_global=0,selection=0,focused=0;
    if(!region::read(*r,request.path_delete.region,before.region,after,e)||!region::bound(after,binding,request.snapshot_uuid,e)||!graph(*r,after,current,e)||!delete_delta(old,current,request.path_delete,op.uids,e)||!region::ledger(op.evidence.after,e)||!terrain::terrain_state(*r,after,op.evidence.terrain_after,after_global,e)||!undo::read_undo(r->memory,r->image,undo_after,e)||!region::at(*r,r->image,0x13df630,selection,e)||!native::ap(r->memory,selection,r->image,0x17205d0,e)||!region::at(*r,selection,0x80,focused,e))return unknown();
    if(focused==road){e="path delete target remains focused";return unknown();}
    for(size_t off:{size_t(0x8),size_t(0x20),size_t(0x38),size_t(0x50)}){std::vector<uintptr_t> list;
        if(!native::array(r->memory,selection,off+8,off+16,off+20,1000000,list,e))return unknown();
        for(auto p:list)if(p==road){e="path delete target remains selected";return unknown();}}
    op.evidence.region_after=after.state;op.evidence.path_after=current.state;op.evidence.path_after.cost=price;op.evidence.target_retired=true;op.evidence.selection_confirmed=true;
    if(!delete_inverse(*r,undo_before,undo_after,op,position,e))return unknown();op.evidence.undo_confirmed=true;
    if(!action::path_delete_effect(request,op.evidence,e))return unknown();
    if(authority&&(op.evidence.after!=authority->after||op.evidence.region_after!=authority->region_after||op.evidence.terrain_after!=authority->terrain_after||op.evidence.path_after!=authority->path_after)){e="path delete original result differs from Host";return unknown();}
    op.history_before=path_history_graph::Snapshot{old.path_uids,old.node_uids,old.rows};
    op.history_after=path_history_graph::Snapshot{current.path_uids,current.node_uids,current.rows};op.history_graph=undo_after.undo.back();
    op.confirmed=true;return action::Outcome::Confirmed;
}
