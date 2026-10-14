namespace pipe {
using Intent=action::NetworkPipeIntent;using Point=action::NetworkPoint;using Preview=network_pipe_native::Preview;
using Buy=bool(*)(void*,float,const void*);
struct Pending{Intent intent;action::Binding binding;std::string snapshot,token;};
struct Operation{action::Binding binding;action::Request request;action::Evidence evidence;uintptr_t tool=0,created=0;uint64_t uid=0,new_uid=0;Point settled_end{};int32_t inverse_cost=0;unsigned calls=0;bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;};
struct State{bool attempted=false,ready=false,capture_failed=false;Press press=nullptr;Buy purchase=nullptr;uintptr_t capture_receiver=0;std::optional<Pending> captured,pending;std::map<std::string,Operation> operations;std::map<std::string,json> jobs;uint64_t captures=0,submits=0,rejections=0;std::string token,cause,last_action;} state;
void rejected(const std::string& e){++state.rejections;if(state.cause!=e){state.cause=e;LOG_ERROR("[UINetworkPipe] Rejected cause={}",e);}}
void press_entry(void*,void*,const void*);bool purchase_entry(void*,float,const void*);
bool code(region::Runtime& r,bool hooked,std::string& e){
    for(size_t i=0;i<std::size(network_pipe_codes);++i){const auto& g=network_pipe_codes[i];std::vector<unsigned char> actual(g.size);
        if(!r.memory.copy(r.image+g.rva,actual.data(),actual.size(),e))return false;
        if(!hooked||i>=2){if(std::memcmp(actual.data(),g.bytes,g.size))return fail(e,"network Pipe original code differs RVA="+std::to_string(g.rva));continue;}
        if(actual[0]!=0xe9||std::memcmp(actual.data()+5,g.bytes+5,g.size-5))return fail(e,"network Pipe owned hook tail differs");
        const auto original=i?uintptr_t(state.purchase):uintptr_t(state.press);std::vector<unsigned char> prefix(g.span);
        if(!r.memory.copy(original,prefix.data(),prefix.size(),e)||std::memcmp(prefix.data(),g.bytes,g.span))return fail(e,"network Pipe original trampoline prefix differs");
        constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};unsigned char bytes[6]{};uintptr_t target=0;
        if(!r.memory.copy(original+g.span,bytes,6,e)||std::memcmp(bytes,jump,6)||!r.memory.copy(original+g.span+6,&target,8,e)||target!=r.image+g.rva+g.span)return fail(e,"network Pipe trampoline continuation differs");
        int32_t displacement=0;std::memcpy(&displacement,actual.data()+1,4);const auto relay=uintptr_t(int64_t(r.image+g.rva+5)+displacement);
        if(!r.memory.copy(relay,bytes,6,e)||std::memcmp(bytes,jump,6)||!r.memory.copy(relay+6,&target,8,e)||target!=(i?uintptr_t(&purchase_entry):uintptr_t(&press_entry)))return fail(e,"network Pipe hook relay owner differs");
    }return true;
}
uint32_t call(uintptr_t image,unsigned step,uintptr_t tool,uintptr_t cursor,const Point& point){
    __try{if(step==0)reinterpret_cast<void(*)(void*)>(image+0x580870)(reinterpret_cast<void*>(tool));
        else if(step==1)reinterpret_cast<Press>(image+0x5c7340)(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),point.data());
        else if(step==2)state.press(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),point.data());
        else if(step==3)reinterpret_cast<void(*)(void*,void*)>(image+0x5854e0)(reinterpret_cast<void*>(cursor),nullptr);
        else reinterpret_cast<Press>(image+0x5c7970)(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),point.data());return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t price(uintptr_t image,uintptr_t definition,float length,int32_t& value){
    __try{value=reinterpret_cast<int32_t(*)(void*,float)>(image+0x430250)(reinterpret_cast<void*>(definition),length);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t settled_height(uintptr_t image,uintptr_t map,const Point& point,float& height){
    __try{height=reinterpret_cast<float(*)(void*,const void*)>(image+0x32ab80)(reinterpret_cast<void*>(map),point.data());return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t inverse_price(uintptr_t image,uintptr_t object,int32_t& value){
    __try{value=reinterpret_cast<int32_t(*)(const void*)>(image+0x4332d0)(reinterpret_cast<const void*>(object));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t construct(uintptr_t image,uintptr_t& tool){
    __try{tool=uintptr_t(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x240));if(!tool)return ERROR_OUTOFMEMORY;
        reinterpret_cast<void(*)(void*)>(image+0x5c7ff0)(reinterpret_cast<void*>(tool));
        region::NativeString type{"Cable",5,{}},node{"Junction",8,{}};
        reinterpret_cast<void(*)(void*,const void*,const void*)>(image+0x5c6600)(reinterpret_cast<void*>(tool),&type,&node);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t destroy(uintptr_t image,uintptr_t tool){
    __try{reinterpret_cast<void(*)(void*)>(image+0x5c5a30)(reinterpret_cast<void*>(tool));
        reinterpret_cast<void(*)(void*)>(image+0x5c6080)(reinterpret_cast<void*>(tool));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool preview(region::Runtime& r,uintptr_t tool,const reader::Graph& before,const action::NetworkState& network,Preview& out,std::string& e){
    if(!network_pipe_native::preview(r.memory,r.image,tool,before,network.graph_sha256,out,e)||
       !native_error(price(r.image,out.definition,out.intent.quoted_length,out.intent.quoted_cost),"Pipe CostOf",e))return false;
    return action::valid_network_pipe(out.intent)||fail(e,"network original Pipe preview intent invalid");
}
bool purchase_entry(void* def,float length,const void* point){
    if(!state.capture_receiver)return state.purchase(def,length,point);
    std::string e;auto* r=runtime(e);if(!r){rejected(e);return false;}const auto caller=uintptr_t(_ReturnAddress());
    if(caller!=r->image+0x5c7b26&&caller!=r->image+0x5c7b8c){rejected("network Pipe Purchase caller differs from original Press");return false;}
    if(state.pending||state.captured){rejected("network Pipe intent already retained");return false;}
    uintptr_t region_object=0;int32_t id=-1;region::LocalView view;reader::Graph before;action::NetworkState network;Preview actual;Point endpoint{};
    if(!region::at(*r,state.capture_receiver,0x200,region_object,e)||!region::at(*r,region_object,0x834,id,e)||!region::identity(*r,id,region_object,view,e)||
       !graph(*r,view,before,network,e)||!preview(*r,state.capture_receiver,before,network,actual,e)||!r->memory.copy(uintptr_t(point),endpoint.data(),sizeof endpoint,e)||
       actual.definition!=uintptr_t(def)||actual.intent.quoted_length!=length||actual.intent.end!=endpoint){rejected(e.empty()?"network Pipe Purchase parameters differ from actual preview":e);return false;}
    state.captured=Pending{actual.intent,{view.world.origin.world_epoch,view.world.origin.generation},net::current_snapshot_id(),state.token};return false;
}
void press_entry(void* receiver,void* cursor,const void* point){
    if(!state.ready||!net::session_wire::connected()){state.press(receiver,cursor,point);return;}
    std::string e;auto* r=runtime(e);const auto controller=typed_action_runtime::status();
    if(!r||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||state.pending||state.captured||state.capture_receiver){rejected(e.empty()?"network Pipe UI requires idle Controller":e);return;}
    uintptr_t active=0;Point position{};action::Ledger money,after;undo::UndoWitness before_undo,after_undo;
    if(!region::at(*r,uintptr_t(cursor),0x190,active,e)||active!=uintptr_t(receiver)||!r->memory.copy(uintptr_t(point),position.data(),sizeof position,e)||
       !region::ledger(money,e)||!undo::read_undo(r->memory,r->image,before_undo,e)||before_undo.active_graph){rejected(e.empty()?"network Pipe actual UI owner or Undo differs":e);return;}
    state.capture_receiver=uintptr_t(receiver);const auto seh=call(r->image,2,uintptr_t(receiver),uintptr_t(cursor),position);state.capture_receiver=0;
    if(!native_error(seh,"Pipe UI Press",e)){rejected(e);return;}if(!state.captured)return;
    region::LocalView view;reader::Graph actual;action::NetworkState network;uintptr_t ghost=1;
    if(!native_error(call(r->image,3,0,uintptr_t(cursor),{}),"Pipe UI completion",e)||!region::at(*r,uintptr_t(receiver),0x1b0,ghost,e)||ghost||
       !region::ledger(after,e)||!undo::read_undo(r->memory,r->image,after_undo,e)||money!=after||before_undo!=after_undo||
       !region::identity(*r,state.captured->intent.region,0,view,e)||!graph(*r,view,actual,network,e)||network.graph_sha256!=state.captured->intent.graph_before_sha256){
        state.capture_failed=true;rejected(e.empty()?"network Pipe capture completion changed graph/cash/Undo or retained preview":e);return;}
    state.pending=std::move(state.captured);state.captured.reset();++state.captures;
}
bool enable(region::Runtime& r,std::string& e){
    if(state.attempted)return fail(e,"network Pipe hook installation already attempted");state.attempted=true;if(!code(r,false,e))return false;
    struct Hook{uintptr_t rva;void* entry;void** original;};const Hook hooks[]{
        {0x5c7970,reinterpret_cast<void*>(&press_entry),reinterpret_cast<void**>(&state.press)},
        {0x433e80,reinterpret_cast<void*>(&purchase_entry),reinterpret_cast<void**>(&state.purchase)}};
    for(const auto& h:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(r.image+h.rva),h.entry,h.original);if(result!=MH_OK||!*h.original)return fail(e,"network Pipe MH_CreateHook RVA="+std::to_string(h.rva)+" status="+std::to_string(result));}
    for(const auto& h:hooks){const auto result=MH_EnableHook(reinterpret_cast<void*>(r.image+h.rva));if(result!=MH_OK)return fail(e,"network Pipe MH_EnableHook RVA="+std::to_string(h.rva)+" status="+std::to_string(result));}
    if(!code(r,true,e))return false;state.ready=true;return true;
}
bool inverse(region::Runtime& r,const undo::UndoWitness& before,const undo::UndoWitness& after,Operation& op,std::string& e){
    if(after.undo.empty())return fail(e,"network Pipe original Undo absent");const auto address=after.undo.back();undo::GraphWitness witness;
    if(!undo::read_graph(r.memory,r.image,address,witness,e)||!undo::validate_undo_commit(before,after,witness,e))return false;
    const int32_t expected=op.new_uid?3:2;if(witness.operations!=expected)return fail(e,"network Pipe Undo count actual="+std::to_string(witness.operations)+" required="+std::to_string(expected));
    uintptr_t data=0;if(!region::at(r,address,0x20,data,e))return false;size_t index=0;
    auto removed=[&](uint64_t expected_uid){uintptr_t destroy_op=0;uint64_t uid=0;
        if(!region::at(r,data,8*index++,destroy_op,e)||!reader::read::ap(r.memory,destroy_op,r.image,0x16de860,e)||!region::at(r,destroy_op,0xb8,uid,e))return false;
        return uid==expected_uid||fail(e,"network Pipe Undo destroy UID differs from actual created object");};
    if(op.new_uid&&!removed(op.new_uid))return false;
    uintptr_t cash=0;Point actual{},position{};
    if(!region::at(r,data,8*index++,cash,e)||!reader::read::ap(r.memory,cash,r.image,0x1786fb0,e)||!region::at(r,cash,0xb8,op.inverse_cost,e)||
       !region::at(r,cash,0xbc,actual,e)||!region::at(r,op.created,0x110,position,e))return false;
    int32_t expected_cost=-1;if(!native_error(inverse_price(r.image,op.created,expected_cost),"Pipe inverse Cost",e))return false;
    if(actual!=position||op.inverse_cost<0||op.inverse_cost!=expected_cost)return fail(e,"network Pipe Undo native cash/position differs from original Cost");
    return removed(op.uid);
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=runtime(e);if(!r||!state.ready||!action::valid_network_pipe(request.network_pipe)){e="network Pipe runtime/intent unavailable: "+e;return action::Outcome::RejectedNoEffect;}
    if(state.operations.contains(request.action_uuid)){e="network Pipe action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::status();if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="network Pipe requires exact pending Controller action";return action::Outcome::RejectedNoEffect;}
    const auto& intent=request.network_pipe;region::LocalView view;reader::Graph before;action::NetworkState network;action::Ledger money;undo::UndoWitness before_undo;uintptr_t cursor=0;
    if(!code(*r,true,e)||!region::identity(*r,intent.region,0,view,e)||!region::bound(view,binding,request.snapshot_uuid,e)||!graph(*r,view,before,network,e)||
       !region::ledger(money,e)||!undo::read_undo(r->memory,r->image,before_undo,e)||!region::at(*r,r->image,0x13df9d0,cursor,e)||!reader::read::ap(r->memory,cursor,r->image,0x1786dd0,e))return action::Outcome::RejectedNoEffect;
    if(network.graph_sha256!=intent.graph_before_sha256||before_undo.active_graph||!money.record_present||money.cash<intent.quoted_cost){e="network Pipe before digest/Undo/cash admission differs";return action::Outcome::RejectedNoEffect;}
    if(authority&&(money!=authority->before||network!=authority->network_before)){e="network Pipe Client before differs from Host";return action::Outcome::RejectedNoEffect;}
    uintptr_t shards=0,active_shard=0;if(!region::at(*r,r->image,0x13df440,shards,e)||!reader::read::ap(r->memory,shards,r->image,0x1702b00,e)||!region::at(*r,shards,0x60,active_shard,e))return action::Outcome::RejectedNoEffect;
    if(active_shard!=view.world.root){e="network Pipe original Purchase requires actual root view shard";return action::Outcome::RejectedNoEffect;}
    auto settled=intent;
    // Original AddNode replaces input Y with the map point height before setting the endpoint.
    if(intent.end_node<0&&(!native_error(settled_height(r->image,view.world.root-0x1c0,intent.end,settled.end[1]),"Pipe endpoint GetHeightAt",e)||!std::isfinite(settled.end[1]))){if(e.empty())e="network original Junction height is not finite";return action::Outcome::RejectedNoEffect;}
    auto& op=state.operations[request.action_uuid];op.binding=binding;op.request=request;op.settled_end=settled.end;op.evidence.before=money;op.evidence.network_before=network;
    auto unknown=[&]{op.cause=e;return action::Outcome::Unknown;};
    // Construction and preview have native lifetime effects; failures remain unresolved until inspected.
    op.attempted=true;if(!native_error(construct(r->image,op.tool),"Pipe behaviour constructor/SetType",e))return unknown();
    auto invoke=[&](unsigned step,const Point& point){++op.calls;return native_error(call(r->image,step,op.tool,cursor,point),"Pipe original input",e);};
    if(!invoke(1,intent.start)||!invoke(2,intent.start)||!invoke(1,intent.end))return unknown();
    if(intent.end_node<0&&!invoke(2,intent.end))return unknown();
    Preview actual;if(!preview(*r,op.tool,before,network,actual,e)||actual.intent!=intent){if(e.empty())e="network Pipe reconstructed native preview differs from captured intent";return unknown();}
    op.created=actual.object;if(!invoke(2,intent.end))return unknown();op.returned=true;
    uintptr_t ghost=1;uint8_t active=1;reader::Graph after;undo::UndoWitness after_undo;
    if(!region::at(*r,op.tool,0x1b0,ghost,e)||!region::at(*r,op.tool,0x238,active,e)||ghost||active||
       !graph(*r,view,after,op.evidence.network_after,e)||!reader::pipe_delta(before,after,settled,op.created,op.uid,op.new_uid,e)||
       !region::ledger(op.evidence.after,e)||!undo::read_undo(r->memory,r->image,after_undo,e)||!inverse(*r,before_undo,after_undo,op,e)){if(e.empty())e="network Pipe original Finish did not release preview";return unknown();}
    op.evidence.undo_confirmed=true;
    if(!action::network_pipe_effect(request,op.evidence,e))return unknown();
    if(authority&&(op.evidence.after!=authority->after||op.evidence.network_after!=authority->network_after)){e="network Pipe actual Client result differs from Host";return unknown();}
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=runtime(e);if(!r)return false;const auto it=state.operations.find(request.action_uuid);
    if(it==state.operations.end()||it->second.binding!=binding||it->second.request!=request||!it->second.confirmed)return fail(e,"network Pipe observation lacks exact confirmed operation");
    const auto& op=it->second;region::LocalView view;reader::Graph actual;action::NetworkState network;action::Ledger money;
    if(!region::identity(*r,request.network_pipe.region,0,view,e)||!region::bound(view,binding,request.snapshot_uuid,e)||!graph(*r,view,actual,network,e)||!region::ledger(money,e))return false;
    const auto created=std::find_if(actual.pipe_values.begin(),actual.pipe_values.end(),[&](const auto& pipe){return pipe.object==op.created&&pipe.uid==op.uid;});
    if(network!=op.evidence.network_after||money!=op.evidence.after||created==actual.pipe_values.end())return fail(e,"network Pipe actual state changed after original return");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence evidence;if(!observe(binding,request,evidence,e))return false;auto& op=state.operations.at(request.action_uuid);
    if(op.retired)return fail(e,"network Pipe operation already retired");auto* r=runtime(e);if(!r||!native_error(destroy(r->image,op.tool),"Pipe behaviour retirement",e))return false;
    op.tool=0;op.retired=true;return true;
}
bool unresolved(){if(state.capture_failed||state.pending||state.captured)return true;for(const auto& [id,op]:state.operations){(void)id;if(!op.retired)return true;}for(const auto& [token,job]:state.jobs){(void)token;if(!job.value("terminal",false))return true;}return false;}
void tick(){
    const auto controller=typed_action_runtime::controller_status();for(auto& [token,job]:state.jobs){(void)token;const auto id=job.value("action_uuid",std::string{});
        if(id.empty()||job.value("terminal",false))continue;if(controller.value("action_uuid",std::string{})==id&&!controller.value("pending",true)){const auto outcome=controller.value("outcome",0);
            if(outcome==1||outcome==2){job["terminal"]=true;job["confirmed"]=outcome==1;job["outcome"]=outcome;job["cause"]=controller.value("cause",std::string{});}}}
    if(!state.pending)return;const auto pending=*state.pending;state.pending.reset();std::string e;auto* r=runtime(e);region::LocalView view;
    if(!r||!region::identity(*r,pending.intent.region,0,view,e)||!region::bound(view,pending.binding,pending.snapshot,e)){state.capture_failed=true;rejected(e);return;}
    const auto id=region::uuid(e);if(id.empty()||!typed_action_runtime::submit_network_pipe(id,pending.intent,e)){state.capture_failed=true;rejected(e.empty()?"network Pipe Controller submit failed":e);return;}
    ++state.submits;state.last_action=id;if(!pending.token.empty()){state.jobs[pending.token]["action_uuid"]=id;state.jobs[pending.token]["submitted"]=true;}
}
json status(){json operations=json::object();for(const auto& [id,op]:state.operations)operations[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"native_calls",op.calls},{"inverse_cost",op.inverse_cost},{"settled_end",op.settled_end},{"cause",op.cause},{"before",op.evidence.network_before.graph_sha256},{"after",op.evidence.network_after.graph_sha256}};
    return {{"enabled",state.ready},{"capture_count",state.captures},{"submitted",state.submits},{"rejected",state.rejections},{"pending",bool(state.pending)||bool(state.captured)},{"last_action",state.last_action},{"cause",state.cause},{"operations",operations},{"ui_jobs",state.jobs}};}
bool native_ui(const json& args,json& out,std::string& e){
    out=json::object();auto* r=runtime(e);if(!r)return false;const auto controller=typed_action_runtime::status();
    if(!state.ready||ui_network_runtime::unresolved()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!net::session_wire::connected())return fail(e,"network Pipe UI requires idle connected Controller");
    if(!args.is_object()||args.size()!=6||!args.contains("token")||!args.contains("id")||!args.contains("start_node")||!args.contains("end_node")||!args.contains("end")||!args.contains("graph_sha256")||
       !args.at("token").is_string()||!args.at("id").is_number_integer()||!args.at("start_node").is_number_integer()||!args.at("end_node").is_number_integer()||!args.at("graph_sha256").is_string())return fail(e,"network Pipe UI requires token,id,start_node,end_node,end,graph_sha256");
    const auto token=args.at("token").get<std::string>();const auto id=args.at("id").get<int64_t>(),start_index=args.at("start_node").get<int64_t>(),end_index=args.at("end_node").get<int64_t>();
    if(token.empty()||token.size()>128||token.find('\0')!=std::string::npos||state.jobs.contains(token)||id<0||id>INT32_MAX||start_index<0||start_index>INT32_MAX||end_index< -1||end_index>INT32_MAX||end_index==start_index)return fail(e,"network Pipe UI token or endpoint selector invalid/already attempted");
    Point end{};const auto& values=args.at("end");if(!values.is_array()||values.size()!=3)return fail(e,"network Pipe UI end requires three coordinates");
    for(size_t i=0;i<3;++i){if(!values[i].is_number())return fail(e,"network Pipe end coordinate is not numeric");const auto value=values[i].get<double>();if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return fail(e,"network Pipe end coordinate outside float32");end[i]=float(value);}
    region::LocalView view;reader::Graph actual;action::NetworkState network;
    if(!code(*r,true,e)||!region::identity(*r,int32_t(id),0,view,e)||!graph(*r,view,actual,network,e))return false;
    if(args.at("graph_sha256")!=network.graph_sha256)return fail(e,"network Pipe UI before graph differs");
    const auto start=reader::selected(actual,int32_t(id),int32_t(start_index));const auto target=reader::selected(actual,int32_t(id),int32_t(end_index));
    if(!start||(end_index>=0&&!target))return fail(e,"network Pipe UI node selector absent");
    if(target)end={target->transform[4],target->transform[5],target->transform[6]};const Point begin{start->transform[4],start->transform[5],start->transform[6]};
    placement_ui_probe::Context context{r->image,uintptr_t(view.world.origin.state),view.world.origin.world_epoch,view.world.origin.generation,r->thread};
    placement_ui_probe::Request request;request.token=token;request.family=placement_ui_probe::Family::NetworkPipe;request.type="Cable";request.variant="item_cable";
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding binding;if(!placement_ui_probe::read_existing_tool(memory,context,request,binding,e))return false;
    auto& job=state.jobs[token];job={{"token",token},{"attempted",0},{"returned",0},{"captured",false},{"submitted",false},{"confirmed",false},{"terminal",false},{"action_uuid",""},{"cause",""}};
    state.token=token;struct Reset{~Reset(){state.token.clear();}} reset;const auto captures=state.captures;
    auto invoke=[&](unsigned step,uintptr_t object,const Point& position){job["attempted"]=job.at("attempted").get<unsigned>()+1;
        if(!native_error(call(r->image,step,object,binding.cursor,position),"Pipe normal UI input",e))return false;job["returned"]=job.at("returned").get<unsigned>()+1;return true;};
    uintptr_t active=0;bool okay=invoke(0,binding.button,{})&&region::at(*r,binding.cursor,0x190,active,e)&&active==binding.behaviour&&
        invoke(1,binding.behaviour,begin)&&invoke(4,binding.behaviour,begin)&&invoke(1,binding.behaviour,end);
    Preview measured;if(okay)okay=preview(*r,binding.behaviour,actual,network,measured,e)&&measured.intent.start_node==start_index&&measured.intent.end_node==end_index;
    if(okay)okay=invoke(4,binding.behaviour,end);if(okay&&end_index<0)okay=invoke(4,binding.behaviour,end);
    if(okay)okay=state.captures==captures+1&&state.pending&&state.pending->token==token;
    if(!okay){if(e.empty())e="network Pipe UI did not capture exact intended endpoints: "+state.cause;job["cause"]=e;out=job;return false;}
    job["captured"]=true;job["start_node"]=start_index;job["end_node"]=end_index;job["start"]=measured.intent.start;job["end"]=measured.intent.end;job["cost"]=measured.intent.quoted_cost;job["length"]=measured.intent.quoted_length;out=job;return true;
}
}
