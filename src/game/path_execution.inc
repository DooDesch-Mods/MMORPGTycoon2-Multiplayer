namespace {
uint32_t cost(uintptr_t image,float length,int32_t& out){__try{out=reinterpret_cast<int32_t(*)(int32_t,float)>(image+0x439720)(path.road,length);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t release_units(uintptr_t image,uintptr_t road,float& out){__try{out=reinterpret_cast<float(*)(void*)>(image+0x43c4b0)(reinterpret_cast<void*>(road))*(*reinterpret_cast<const float*>(image+0x14f0f90));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t surface_point(uintptr_t image,uintptr_t shard,action::PathPoint& point,bool& in_bounds){__try{in_bounds=reinterpret_cast<bool(*)(void*,const void*)>(image+0x508540)(reinterpret_cast<void*>(shard),point.data());if(in_bounds)point[1]=reinterpret_cast<float(*)(void*,const void*)>(image+0x508390)(reinterpret_cast<void*>(shard),point.data());return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool inverse(region::Runtime& r,const undo::UndoWitness& before,const undo::UndoWitness& after,const Operation& op,std::string& e) {
    if(after.undo.empty())return fail(e,"path original Undo history is empty");
    undo::GraphWitness witness;const auto address=after.undo.back();
    if(!undo::read_graph(r.memory,r.image,address,witness,e)||!undo::validate_undo_commit(before,after,witness,e))return false;
    const size_t expected=2+size_t(action::path_new_nodes(op.request.path));
    if(witness.operations!=expected)return fail(e,"path Undo operation count actual="+std::to_string(witness.operations)+" required="+std::to_string(expected));
    uintptr_t data=0;if(!region::at(r,address,0x20,data,e))return false;
    std::set<uint64_t> destroyed;unsigned cash=0;
    for(size_t i=0;i<expected;++i){uintptr_t p=0,ap=0;if(!region::at(r,data,i*8,p,e)||!region::at(r,p,0,ap,e))return false;
        if(ap==r.image+0x16de860){uint64_t uid=0;if(!region::at(r,p,0xb8,uid,e)||!destroyed.insert(uid).second)return fail(e,"path Undo duplicate destroy UID: "+e);}
        else if(ap==r.image+0x1786fb0){int32_t amount=0;action::PathPoint position{};if(!region::at(r,p,0xb8,amount,e)||!region::at(r,p,0xbc,position,e))return false;
            if(amount!=op.evidence.path_after.cost||position!=op.request.path.end)return fail(e,"path Undo cash amount/endpoint differs");++cash;}
        else return fail(e,"path Undo contains unexpected original operation AP="+std::to_string(ap));
    }
    return cash==1&&destroyed==std::set<uint64_t>(op.uids.begin(),op.uids.end())||fail(e,"path Undo destroy set differs from actual new road/node UIDs");
}
bool graph_delta(const Graph& before,const Graph& after,const action::PathIntent& intent,std::string& e) {
    if(!endpoint_binding(before,intent,e))return false;
    const size_t added=size_t(action::path_new_nodes(intent));
    if(after.paths.size()!=before.paths.size()+1||after.nodes.size()!=before.nodes.size()+added)return fail(e,"path commit road/node count differs from requested endpoints");
    if(!std::equal(before.paths.begin(),before.paths.end(),after.paths.begin())||!std::equal(before.nodes.begin(),before.nodes.end(),after.nodes.begin()))return fail(e,"path commit changed existing local topology identities");
    for(size_t i=0;i<before.paths.size();++i)if(before.rows.at("paths")[i]!=after.rows.at("paths")[i])return fail(e,"path commit changed existing road contents");
    const auto p=before.paths.size();size_t next=before.nodes.size();
    const size_t a=intent.start_node<0?next++:size_t(intent.start_node),b=intent.end_node<0?next++:size_t(intent.end_node);
    const auto& row=after.rows.at("paths").back();if(row[0]!=a||row[1]!=b)return fail(e,"path committed endpoint indices differ");
    for(size_t i=0;i<before.nodes.size();++i){auto expected=before.rows.at("nodes")[i];if(i==a||i==b)expected[1].push_back(p);
        if(after.rows.at("nodes")[i]!=expected)return fail(e,"path existing node position/adjacency differs index="+std::to_string(i));}
    if(intent.start_node<0&&after.rows.at("nodes")[a]!=json::array({intent.start,json::array({p})}))return fail(e,"path new start position/adjacency differs");
    if(intent.end_node<0&&after.rows.at("nodes")[b]!=json::array({intent.end,json::array({p})}))return fail(e,"path new end position/adjacency differs");
    return true;
}
}
bool enable(std::string& e) {
    auto* r=runtime(e);if(!r)return false;
    if(path.attempted)return fail(e,"path hook installation already attempted");path.attempted=true;
    if(!native_error(enums(r->image,path.road,path.node),"canonical Road enum lookup",e)||path.road<0||path.road>=11||path.road==2||path.node<0||path.node>=6)return fail(e,"path canonical Road enum outside first-segment family: "+e);
    if(!code(*r,false,e))return false;
    struct Hook{uintptr_t rva;void* entry;void** original;};
    const Hook hooks[]{{0x5cc2b0,reinterpret_cast<void*>(&press_entry),reinterpret_cast<void**>(&path.press)},{0x43a1b0,reinterpret_cast<void*>(&purchase_entry),reinterpret_cast<void**>(&path.purchase)},{0x7b04b0,reinterpret_cast<void*>(&remove_ui_entry),reinterpret_cast<void**>(&path.remove_ui)}};
    for(const auto& h:hooks){auto s=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.entry,h.original);if(s!=MH_OK||!*h.original)return fail(e,"path MH_CreateHook RVA="+std::to_string(h.rva)+" status="+std::to_string(s));}
    for(const auto& h:hooks){auto s=MH_EnableHook(reinterpret_cast<void*>(r->image+h.rva));if(s!=MH_OK)return fail(e,"path MH_EnableHook RVA="+std::to_string(h.rva)+" status="+std::to_string(s));}
    if(!code(*r,true,e))return false;path.ready=true;return true;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e) {
    if(request.action==action::Action::RemovePath)return perform_remove(binding,request,authority,e);
    auto* r=runtime(e);if(!r||!path.ready||request.action!=action::Action::PlacePath||!action::valid_path(request.path)){e="path runtime/intent unavailable: "+e;return action::Outcome::RejectedNoEffect;}
    if(path.operations.contains(request.action_uuid)){e="path original action already attempted; no replay";return action::Outcome::Unknown;}
    if(!code(*r,true,e))return action::Outcome::RejectedNoEffect;
    const auto controller=typed_action_runtime::status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="path lacks exact Controller request";return action::Outcome::RejectedNoEffect;}
    region::LocalView before;Graph graph_before;action::TerrainState state;action::Ledger money;undo::UndoWitness undo_before;uintptr_t global=0;
    if(!region::read(*r,request.path.region,0,before,e)||!region::bound(before,binding,request.snapshot_uuid,e)||!region::ledger(money,e)||!graph(*r,before,graph_before,e)||
       !terrain::terrain_state(*r,before,state,global,e)||!undo::read_undo(r->memory,r->image,undo_before,e))return action::Outcome::RejectedNoEffect;
    if(!endpoint_binding(graph_before,request.path,e))return action::Outcome::RejectedNoEffect;
    if(undo_before.active_graph){e="path requires idle Undo";return action::Outcome::RejectedNoEffect;}
    uint8_t owned=0;if(!region::at(*r,before.region,0x850,owned,e)||owned!=1){e="path requires owned native region: "+e;return action::Outcome::RejectedNoEffect;}
    for(const auto& point:{request.path.start,request.path.end}){uintptr_t actual=0;const auto seh=terrain::point_region(r->image,before.world.root,point,actual);if(seh||actual!=before.region){e="path endpoint native region differs before effect, SEH="+std::to_string(seh);return action::Outcome::RejectedNoEffect;}}
    if(!money.record_present||!action::path_receipt_fits(request,money,before.state,e)){if(e.empty())e="path requires original current cash record";return action::Outcome::RejectedNoEffect;}
    if(authority&&(money!=authority->before||graph_before.state!=authority->path_before||!action::same_region_content(before.state,authority->region_before)||!action::same_terrain_content(state,authority->terrain_before))){e="path client before content differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=path.operations[request.action_uuid];op.binding=binding;op.request=request;op.evidence.before=money;
    op.evidence.region_before=authority?authority->region_before:before.state;op.evidence.terrain_before=authority?authority->terrain_before:state;op.evidence.path_before=graph_before.state;
    op.attempted=true;auto unknown=[&]{op.cause=e;return action::Outcome::Unknown;};
    if(authority&&!native_error(terrain::seed(before.random,global,*authority),"authority RNG seed",e))return unknown();
    if(!native_error(construct(r->image,op),"private cursor constructor",e))return unknown();
    path.replay=&op;struct Reset{~Reset(){path.replay=nullptr;}} reset;
    uintptr_t cursor=0;if(!region::at(*r,r->image,0x13df9d0,cursor,e)||!cursor)return unknown();
    auto call=[&](unsigned step,const action::PathPoint& point){return native_error(native_call(r->image,step,op.receiver,cursor,point),"private cursor input",e);};
    const uintptr_t expected_start=request.path.start_node<0?0:graph_before.nodes[size_t(request.path.start_node)];
    const uintptr_t expected_end=request.path.end_node<0?0:graph_before.nodes[size_t(request.path.end_node)];
    if(!call(1,request.path.start)||!native::snap_target(r->memory,op.receiver,expected_start,e)||!call(2,request.path.start)||
       !call(1,request.path.end)||!native::snap_target(r->memory,op.receiver,expected_end,e))return unknown();
    // A snapped final node commits on the next Press; a free final point must first be pinned.
    if(request.path.end_node<0&&!call(2,request.path.end))return unknown();
    native::Preview preview;if(!native::preview(r->memory,r->image,op.receiver,path.road,path.node,preview,e)||!bind_preview(graph_before,preview,e))return unknown();
    if(preview.intent!=request.path||op.purchases){e="path private original preview differs from captured UI before Purchase";return unknown();}
    int32_t price=-1;if(!native_error(cost(r->image,preview.intent.quoted_length,price),"CostOf",e)||price<0)return unknown();
    if(!call(2,request.path.end))return unknown();
    uintptr_t ghost=1;uint8_t active=1;undo::UndoWitness undo_after;Graph graph_after;region::LocalView after;uintptr_t after_global=0;
    if(!region::at(*r,op.receiver,0x1a8,ghost,e)||!region::at(*r,op.receiver,0x230,active,e)||!region::read(*r,request.path.region,before.region,after,e)||
       !region::bound(after,binding,request.snapshot_uuid,e)||!graph(*r,after,graph_after,e)||!graph_delta(graph_before,graph_after,request.path,e)||!region::ledger(op.evidence.after,e)||
       !terrain::terrain_state(*r,after,op.evidence.terrain_after,after_global,e)||!undo::read_undo(r->memory,r->image,undo_after,e))return unknown();
    if(ghost||active||op.purchases!=1){e="path original completion retained preview or purchase count differs";return unknown();}
    op.evidence.region_after=after.state;op.evidence.path_after=graph_after.state;op.evidence.path_after.cost=price;
    op.uids={graph_after.path_uids.back()};
    for(size_t i=graph_before.nodes.size();i<graph_after.node_uids.size();++i)op.uids.push_back(graph_after.node_uids[i]);
    float units=0;if(!native_error(release_units(r->image,graph_after.paths.back(),units),"committed Length release readback",e))return unknown();
    if(!std::isfinite(units)||units<0||double(units)>=INT32_MAX||int64_t(op.evidence.terrain_after.release_edits[3])-state.release_edits[3]!=int64_t(std::floor(units))+1){e="path measured Release edit3 differs from original committed Length";return unknown();}
    if(!inverse(*r,undo_before,undo_after,op,e))return unknown();op.evidence.undo_confirmed=true;
    if(!action::path_effect(request,op.evidence,e))return unknown();
    if(authority&&(op.evidence.after!=authority->after||op.evidence.region_after!=authority->region_after||op.evidence.terrain_after!=authority->terrain_after||op.evidence.path_after!=authority->path_after)){e="path original commit readback differs from Host";return unknown();}
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e) {
    auto* r=runtime(e);if(!r)return false;const auto found=path.operations.find(request.action_uuid);
    if(found==path.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(e,"path observation lacks exact confirmed operation");
    const auto& op=found->second;region::LocalView view;Graph actual;action::TerrainState state;action::Ledger money;uintptr_t global=0;
    if(!region::read(*r,(request.action==action::Action::RemovePath?request.path_delete.region:request.path.region),0,view,e)||!region::bound(view,binding,request.snapshot_uuid,e)||!graph(*r,view,actual,e)||!terrain::terrain_state(*r,view,state,global,e)||!region::ledger(money,e))return false;
    actual.state.cost=op.evidence.path_after.cost;
    if(view.state!=op.evidence.region_after||state!=op.evidence.terrain_after||actual.state!=op.evidence.path_after||money!=op.evidence.after)return fail(e,"path observation changed after original return");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e) {
    action::Evidence evidence;if(!observe(binding,request,evidence,e))return false;auto& op=path.operations.at(request.action_uuid);auto* r=runtime(e);if(!r)return false;
    if(op.retired)return fail(e,"path receipt already retired");
    if(op.constructed&&!native_error(native_call(r->image,4,op.receiver,0,{}),"private cursor destructor",e))return false;
    op.constructed=false;op.receiver=0;op.retired=true;return true;
}
bool unresolved(){if(path.captured||path.pending||path.delete_pending)return true;for(const auto& [token,job]:path.jobs){(void)token;if(!job.value("confirmed",false))return true;}return false;}
void tick() {
    if(!path.ready)return;const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:path.jobs){(void)token;const auto id=job.value("action_uuid",std::string{});if(id.empty()||job.value("confirmed",false)||id!=controller.value("action_uuid",std::string{}))continue;
        if(controller.value("locked",false)){job["cause"]=controller.value("cause",std::string{"path Controller locked"});continue;}
        const auto op=path.operations.find(id);if(!controller.value("pending",true)&&controller.value("outcome",0)==1&&op!=path.operations.end()&&op->second.confirmed&&op->second.retired)job["confirmed"]=true;}
    if(path.delete_pending){auto held=std::move(*path.delete_pending);path.delete_pending.reset();std::string e;auto* r=runtime(e);region::LocalView view;
        if(!r||!region::identity(*r,held.intent.region,0,view,e)||!region::bound(view,held.binding,held.snapshot,e)){rejected(e);return;}
        const auto id=region::uuid(e);if(id.empty()){rejected(e);return;}path.last_action=id;
        if(!held.token.empty())path.jobs.at(held.token)["action_uuid"]=id;
        if(!typed_action_runtime::submit_path_delete(id,held.intent,e)){if(!held.token.empty())path.jobs.at(held.token)["cause"]=e;rejected(e);return;}++path.submitted;if(!held.token.empty())path.jobs.at(held.token)["submitted"]=true;return;}
    if(!path.pending)return;auto held=std::move(*path.pending);path.pending.reset();std::string e;auto* r=runtime(e);region::LocalView view;
    if(!r||!region::identity(*r,held.preview.intent.region,0,view,e)||!region::bound(view,held.binding,held.snapshot,e)){rejected(e);return;}
    const auto id=region::uuid(e);if(id.empty()){rejected(e);return;}path.last_action=id;
    if(!held.token.empty())path.jobs.at(held.token)["action_uuid"]=id;
    if(!typed_action_runtime::submit_path(id,held.preview.intent,e)){if(!held.token.empty())path.jobs.at(held.token)["cause"]=e;rejected(e);return;}
    ++path.submitted;if(!held.token.empty())path.jobs.at(held.token)["submitted"]=true;
}
json status(){json operations=json::array();for(const auto& [id,o]:path.operations)operations.push_back({{"action",int(o.request.action)},{"action_uuid",id},{"canonical_record_key",o.request.canonical_record_key},{"attempted",o.attempted},{"confirmed",o.confirmed},{"retired",o.retired},{"purchases",o.purchases},{"cost",o.evidence.path_after.cost},{"local_uids",o.uids},{"undo_confirmed",o.evidence.undo_confirmed},{"graph_before",o.evidence.path_before.graph_sha256},{"graph_after",o.evidence.path_after.graph_sha256},
        {"cash_before",o.evidence.before.cash},{"cash_after",o.evidence.after.cash},{"global_random_before",o.evidence.terrain_before.global_random},{"global_random_after",o.evidence.terrain_after.global_random},{"region_random_before",o.evidence.region_before.random},{"region_random_after",o.evidence.region_after.random},
        {"release_before",o.evidence.terrain_before.release_edits},{"release_after",o.evidence.terrain_after.release_edits},{"bugs_before",o.evidence.terrain_before.bugs_sha256},{"bugs_after",o.evidence.terrain_after.bugs_sha256},{"bug_count_before",o.evidence.terrain_before.bug_count},{"bug_count_after",o.evidence.terrain_after.bug_count},{"paths_before",o.evidence.path_before.paths},{"paths_after",o.evidence.path_after.paths},{"nodes_before",o.evidence.path_before.nodes},{"nodes_after",o.evidence.path_after.nodes},{"cause",o.cause}});
    return {{"enabled",path.ready},{"captured",path.capture_count},{"pending",bool(path.pending)},{"submitted",path.submitted},{"rejected",path.rejected},{"cause",path.cause},{"last_action_uuid",path.last_action},{"operations",operations},{"ui_jobs",path.jobs}};}
bool region_members(int32_t id,std::vector<uint64_t>& roads,std::vector<uint64_t>& nodes,std::string& e){
    roads.clear();nodes.clear();auto* r=runtime(e);
    if(!r)return fail(e,"path Region census requires enabled Region readback: "+e);
    region::LocalView view;Graph actual;
    if(!region::identity(*r,id,0,view,e)||!graph(*r,view,actual,e))return false;
    roads=std::move(actual.path_uids);nodes=std::move(actual.node_uids);
    std::sort(roads.begin(),roads.end());std::sort(nodes.begin(),nodes.end());return true;
}
bool query(int32_t id,json& out,std::string& e){auto* r=runtime(e);if(!r||!path.ready)return fail(e,"path query requires enabled runtime: "+e);region::LocalView view;Graph actual;action::TerrainState state;uintptr_t global=0;json bugs;
    if(!region::read(*r,id,0,view,e)||!graph(*r,view,actual,e)||!terrain::terrain_state(*r,view,state,global,e,&bugs))return false;
    std::vector<native::TargetPosition> targets;if(!native::target_positions(r->memory,r->image,view.region,targets,e))return false;
    out=region::state_json(view.state);out["paths"]=actual.state.paths;out["nodes"]=actual.state.nodes;out["graph_sha256"]=actual.state.graph_sha256;out["graph"]=actual.rows;out["local_path_uids"]=actual.path_uids;out["local_node_uids"]=actual.node_uids;out["release_edits"]=state.release_edits;out["global_random"]=state.global_random;out["bugs_sha256"]=state.bugs_sha256;out["bug_count"]=state.bug_count;out["bug_rows"]=std::move(bugs);
    out["bug_target_candidates"]=json::array();for(const auto& t:targets)out["bug_target_candidates"].push_back({{"family",t.family==0?"building":"npc"},{"bucket",t.bucket},{"index",t.index},{"local_uid",t.uid},{"position",t.position}});
    uintptr_t scenery_manager=0;if(!region::at(*r,view.region,0x960,scenery_manager,e))return false;
    region::SceneryPort port(*r,view,scenery_manager);region::place::SceneryWitness scenery;if(!region::place::read_scenery(r->memory,port,r->image,scenery,e))return false;
    out["surface_anchors"]=json::array();
    for(const auto& member:scenery.members){const auto& p=member.transform.position;action::PathPoint point{p.x,p.y,p.z};bool in_bounds=false;
        if(!native_error(surface_point(r->image,view.world.root,point,in_bounds),"readonly GetHeightAt",e))return false;
        if(!in_bounds||!std::isfinite(point[1]))return fail(e,"path actual scenery anchor outside native surface");
        uintptr_t actual_region=0;if(!native_error(terrain::point_region(r->image,view.world.root,point,actual_region),"readonly anchor region",e))return false;
        if(actual_region!=view.region)return fail(e,"path actual scenery anchor belongs to another region");
        out["surface_anchors"].push_back({{"local_uid",member.uid},{"position",std::array<float,3>{p.x,p.y,p.z}},{"surface_position",point}});
    }
    const auto size=out.dump().size();if(size>200000){out=json::object();return fail(e,"path readonly query exceeds mailbox bound actual="+std::to_string(size)+" required<=200000");}return true;}
bool native_segment(const json& args,json& out,std::string& e) {
    out=json::object();auto* r=runtime(e);if(!r)return false;
    const auto controller=typed_action_runtime::status();
    if(!path.ready||unresolved()||placement_ui_diagnostics::unresolved()||!net::session_wire::connected()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"path UI requires ready idle connected Controller");
    if(!args.is_object()||args.size()!=4||!args.contains("token")||!args.at("token").is_string()||!args.contains("id")||!args.at("id").is_number_integer()||!args.contains("start")||!args.contains("end"))return fail(e,"path native UI requires token, id, start, end");
    const auto token=args.at("token").get<std::string>();const auto id=args.at("id").get<int64_t>();
    if(token.empty()||token.size()>128||token.find('\0')!=std::string::npos||path.jobs.contains(token)||id<0||id>INT32_MAX)return fail(e,"path native UI token/id invalid or already attempted");
    action::PathPoint start{},end{};for(auto pair:{std::pair{"start",&start},std::pair{"end",&end}}){const auto& a=args.at(pair.first);if(!a.is_array()||a.size()!=3)return fail(e,"path UI requires three coordinates per endpoint");for(size_t i=0;i<3;++i){if(!a[i].is_number())return fail(e,"path UI coordinate not numeric");const auto v=a[i].get<double>();if(!std::isfinite(v)||std::abs(v)>std::numeric_limits<float>::max())return fail(e,"path UI coordinate outside finite float32");(*pair.second)[i]=float(v);}}
    if(start==end)return fail(e,"path UI endpoints coincide");if(!code(*r,true,e))return false;
    region::LocalView view;if(!region::identity(*r,int32_t(id),0,view,e))return false;
    for(const auto& point:{start,end}){uintptr_t actual=0;const auto seh=terrain::point_region(r->image,view.world.root,point,actual);if(seh||actual!=view.region)return fail(e,"path endpoint original region lookup differs, SEH="+std::to_string(seh));}
    placement_ui_probe::Context context{r->image,uintptr_t(view.world.origin.state),view.world.origin.world_epoch,view.world.origin.generation,r->thread};
    placement_ui_probe::Request request;request.token=token;request.family=placement_ui_probe::Family::Path;request.type="Road";request.variant="RoadNode";
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding binding;if(!placement_ui_probe::read_existing_tool(memory,context,request,binding,e))return false;
    auto& job=path.jobs[token];job={{"token",token},{"attempted",0},{"returned",0},{"captured",false},{"submitted",false},{"confirmed",false},{"action_uuid",""},{"cause",""}};
    path.token=token;struct Reset{~Reset(){path.token.clear();}} reset;const auto captures=path.capture_count;
    auto call=[&](unsigned step,const action::PathPoint& point){job["attempted"]=job.at("attempted").get<unsigned>()+1;const auto seh=native_call(r->image,step,step==0?binding.button:binding.behaviour,binding.cursor,point);if(seh){e="path original UI callback SEH="+std::to_string(seh)+" step="+std::to_string(step);return false;}job["returned"]=job.at("returned").get<unsigned>()+1;return true;};
    bool ok=call(0,start);uintptr_t active=0;if(ok)ok=region::at(*r,binding.cursor,0x190,active,e)&&active==binding.behaviour;
    if(ok)ok=call(1,start)&&call(5,start)&&call(1,end);
    uintptr_t snapped_end=0;if(ok)ok=region::at(*r,binding.behaviour,0x1e8,snapped_end,e);
    if(ok)ok=call(5,end);if(ok&&!snapped_end)ok=call(5,end);
    if(ok)ok=path.capture_count==captures+1&&path.pending&&path.pending->token==token;
    if(!ok){if(e.empty())e="path original UI returned without one captured commit: "+path.cause;job["cause"]=e;out=job;return false;}
    job["captured"]=true;job["start"]=path.pending->preview.intent.start;job["end"]=path.pending->preview.intent.end;job["quoted_length"]=path.pending->preview.intent.quoted_length;job["start_node"]=path.pending->preview.intent.start_node;job["end_node"]=path.pending->preview.intent.end_node;job["graph_before_sha256"]=path.pending->preview.intent.graph_before_sha256;out=job;return true;
}
