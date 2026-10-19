namespace cycle=release_cycle;
namespace cn=release_cycle_native;
struct CyclePending {a::Context context;a::Binding binding;cycle::Intent intent;std::string id;};
struct CycleOperation {
    a::Context context;a::Binding binding;a::Request request;a::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false,allowed=true;
    uint32_t native_exception=0,boundaries=0;cn::Receipt script;
    release_graph_native::ApplyReceipt manager_apply,prepared_apply;std::string cause;
};
struct CycleRuntime {
    release_boundary::Owner boundary;
    std::optional<CyclePending> pending;std::map<std::string,CycleOperation> operations;
    cycle::Phase phase=cycle::Phase::Idle;std::string cycle,last_terminal,finalize_id;
    std::optional<a::Context> context;std::optional<a::Binding> binding;
    CycleOperation* original=nullptr;bool frame_original=false,local_finished=false;
    uint64_t captures=0,submitted=0,host_finishes=0,client_finishes=0,frame_applies=0;
};
CycleRuntime cycle_runtime;
bool cycle_roots(Runtime& r,const a::Context& c,const a::Binding& b,uintptr_t& manager,uintptr_t& release,uintptr_t& celebration,uintptr_t& mode,int32_t& native_phase,std::string& e){
    a::Context actual;building_mixed_identity::opened_file::CurrentWorld world;uintptr_t ap=0,current=0,singleton=0;
    if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=c||!native_open_runtime::read_current_world(world,e)||world.world_epoch!=b.native_epoch||world.generation!=b.native_generation||
       !at(r,r.image,0x13df4f0,manager,e)||!at(r,manager,0,ap,e)||ap!=r.image+0x1714ed0||!at(r,manager,0x38,release,e)||!at(r,release,0,ap,e)||ap!=r.image+0x16c12b0||
       !at(r,r.image,0x1a38700,current,e)||current!=release||!at(r,manager,0xc0,celebration,e)||!celebration||!at(r,r.image,0x13df6d0,singleton,e)||singleton!=celebration||!at(r,celebration,0,native_phase,e)||native_phase<0||native_phase>2)
        return fail(e,e.empty()?"Release cycle actual Session/NativeOrigin/Manager/current/Celebration owner differs":e);
    mode=uintptr_t(world.mode);if(!mode||!at(r,mode,0,ap,e)||ap!=r.image+0x16e6730)return fail(e,e.empty()?"Release cycle actual ModeInGame class differs":e);return true;
}
bool cycle_random(Runtime& r,const a::Context& c,const a::Binding& b,const std::array<uint64_t,4>& desired,bool& attempted,std::string& e){
    p::Local local;p::State before;a::Ledger money;if(!sample(r,c,b,local,before,money,e))return false;
    for(size_t i=0;i<4;++i){if(before.random[i]==desired[i])continue;bool stored=false;attempted=true;const auto exception=cn::random_cas(local.random+i*8,before.random[i],desired[i],stored);
        if(exception||!stored)return fail(e,"Release Host RNG CAS failed word="+std::to_string(i)+" SEH="+std::to_string(exception));}
    p::Local checked;p::State actual;a::Ledger after;
    if(!sample(r,c,b,checked,actual,after,e)||checked!=local||after!=money||actual.random!=desired)return fail(e,e.empty()?"Release Host RNG actual readback differs":e);return true;
}
bool cycle_pause_client(Runtime& r,const a::Context& c,const a::Binding& b,std::string& e){
    if(c.role!=a::Role::Client)return true;live::NativePort port(c,b,nullptr);live::Pin before;if(!port.read(before,e))return false;
    if(before.selected!=0){const auto exception=cn::clock_pause(r.image,before.clock);if(exception)return fail(e,"Release original Client presentation Clock pause SEH="+std::to_string(exception));}
    live::Pin after;if(!port.read(after,e)||after.selected!=0||after.clock!=before.clock)return fail(e,e.empty()?"Release Client original Clock0 actual readback differs":e);return true;
}
bool cycle_sample(Runtime& r,const a::Context& c,const a::Binding& b,cycle::Publication& value,cycle::Proof& proof,a::Ledger& money,std::string& e){
    p::Local local;p::State scalar;if(!sample(r,c,b,local,scalar,money,e))return false;
    value={};value.cycle=cycle_runtime.cycle;value.phase=cycle_runtime.phase;value.random=scalar.random;
    GraphMemory memory(r);auto key=[&](uintptr_t object,std::string& canonical,std::string& cause){return reference_key(r,c,b,object,canonical,cause);};
    release_graph_native::Receipt manager,prepared;
    if(!release_graph_native::capture(r.image,memory,local.manager,key,value.graph,manager,e)||!release_graph_native::capture(r.image,memory,local.manager,key,value.prepared,prepared,e,release_graph::Kind::PreparedDemands))return false;
    int32_t points=-1;if(!at(r,local.software,0x37c,points,e))return false;
    p::Local checked;p::State after;a::Ledger after_money;
    if(!sample(r,c,b,checked,after,after_money,e)||checked!=local||after!=scalar||after_money!=money)return fail(e,e.empty()?"Release complete original graph capture changed owner/scalars/RNG/Ledger":e);
    return cycle::proof(scalar,value,points,hash,proof,e);
}
bool cycle_queue_finalize(Runtime& r,const a::Context& c,const a::Binding& b,std::string& e){
    if(c.role!=a::Role::Host)return true;
    if(cycle_runtime.finalize_id.empty()){cycle_runtime.finalize_id=uuid(e);if(cycle_runtime.finalize_id.empty())return false;}
    cycle_runtime.context=c;cycle_runtime.binding=b;return true;
}
bool cycle_boundary(Runtime& r,const MT2FW_MT2_ReleaseCall& call,uint32_t& disposition,std::string& e){
    disposition=MT2FW_MT2_RELEASE_FORWARD;
    if(!typed_action_runtime::controller_status().value("started",false)){e.clear();return true;}
    a::Context c;a::Binding b;
    if(cycle_runtime.original){c=cycle_runtime.original->context;b=cycle_runtime.original->binding;}
    else if(cycle_runtime.context&&cycle_runtime.binding){c=*cycle_runtime.context;b=*cycle_runtime.binding;}
    else return fail(e,"Release boundary has no retained complete current Session/Binding");
    uintptr_t manager=0,release=0,celebration=0,mode=0;int32_t phase=0;
    if(call.native_return!=r.image+0x4faf6a||!cycle_roots(r,c,b,manager,release,celebration,mode,phase,e)||mode!=uintptr_t(call.receiver)||!code(r,e))return fail(e,e.empty()?"Release actual physical Mode boundary/current owner differs":e);
    std::string actual;if(!string(r,uintptr_t(call.native_command),actual,e)||actual!="LevelUp")return fail(e,e.empty()?"Release actual native GNU command changed":e);
    if(auto* original=cycle_runtime.original){
        const auto status=typed_action_runtime::controller_status();
        if(!status.value("pending",false)||status.value("action_uuid",std::string{})!=original->request.action_uuid||original->boundaries++)return fail(e,"Release private original boundary does not match one pending action; no replay");
        if(original->request.release_cycle.command==cycle::Command::Finalize){disposition=MT2FW_MT2_RELEASE_FORWARD;e.clear();return true;}
        if(original->request.release_cycle.command!=cycle::Command::Show&&original->request.release_cycle.command!=cycle::Command::Begin&&original->request.release_cycle.command!=cycle::Command::Skip)return fail(e,"Release original unexpected nested LevelUp command");
        cycle_runtime.phase=cycle::Phase::AwaitPublish;disposition=MT2FW_MT2_RELEASE_SUPPRESS;return cycle_queue_finalize(r,c,b,e);
    }
    std::vector<uintptr_t> frames;
    if(!release_boundary::current_native_frames(r.image,r.image_bytes,frames,e))return false;
    const auto origin=release_boundary::origin(call.native_return-r.image,frames);
    if(origin==release_boundary::Origin::Other){e.clear();return true;}
    if(cycle_runtime.frame_original){disposition=MT2FW_MT2_RELEASE_SUPPRESS;e.clear();return true;}
    if(origin!=release_boundary::Origin::Finish)return fail(e,"Release player Begin/Skip/immediate command escaped before-effect UI capture");
    if(cycle_runtime.cycle.empty()||cycle_runtime.phase==cycle::Phase::Idle||phase!=0)return fail(e,"Release original Finish lacks retained active celebration/current native mode0");
    disposition=MT2FW_MT2_RELEASE_SUPPRESS;
    if(c.role==a::Role::Client){++cycle_runtime.client_finishes;cycle_runtime.local_finished=true;e.clear();return true;}
    if(cycle_runtime.phase==cycle::Phase::AwaitPublish){e.clear();return true;}
    ++cycle_runtime.host_finishes;cycle_runtime.phase=cycle::Phase::AwaitPublish;return cycle_queue_finalize(r,c,b,e);
}
bool cycle_capture(Runtime& r,size_t index,void* receiver,void* record,const std::string& command,bool& handled,bool& accepted,std::string& e){
    handled=false;accepted=false;CyclePending pending;
    if(index==0&&command=="DoCelebration")pending.intent.command=cycle::Command::Show;
    else if(index==2&&command=="Abort")pending.intent.command=cycle::Command::Abort;
    else if(index==2&&command=="DoRelease")pending.intent.command=cycle::Command::Begin;
    else if(index==2&&command=="DoSkip")pending.intent.command=cycle::Command::Skip;else return true;
    handled=true;int32_t count=-1;const auto controller=typed_action_runtime::controller_status();
    if(r.unknown||r.pending||cycle_runtime.pending||!cycle_runtime.finalize_id.empty()||controller.value("pending",true)||controller.value("locked",true)||!window(r,uintptr_t(receiver),index,e)||!code(r,e)||!at(r,uintptr_t(record),0x40,count,e)||count||
       !a::selected_session().current(pending.context,e)||!typed_action_runtime::network_binding(pending.context.snapshot_uuid,pending.binding,e))return fail(e,e.empty()?"Release normal cycle capture needs idle exact owned UI/zero-token command":e);
    pending.id=uuid(e);if(pending.id.empty())return false;pending.intent.cycle=pending.intent.command==cycle::Command::Show?pending.id:cycle_runtime.cycle;
    auto value=std::make_unique<cycle::Publication>();cycle::Proof before;a::Ledger money;
    if(!cycle_sample(r,pending.context,pending.binding,*value,before,money,e)||!cycle::before(pending.intent,before,e))return false;
    cycle_runtime.pending=std::move(pending);++cycle_runtime.captures;accepted=true;return true;
}
bool cycle_adopt(Runtime& r,const a::Context& c,const a::Binding& b,const cycle::Publication& desired,bool presentation,bool& attempted,release_graph_native::ApplyReceipt& manager_receipt,release_graph_native::ApplyReceipt& prepared_receipt,std::string& e){
    if(!cycle::valid(desired,e)||c.role!=a::Role::Client)return fail(e,e.empty()?"Release whole Host value adoption requires Client":e);
    auto before=std::make_unique<cycle::Publication>();cycle::Proof proof;a::Ledger ledger;
    if(!cycle_sample(r,c,b,*before,proof,ledger,e))return false;
    p::Local local;p::State scalar;a::Ledger money;if(!sample(r,c,b,local,scalar,money,e))return false;
    GraphMemory memory(r);
    auto key=[&](uintptr_t object,std::string& canonical,std::string& cause){return reference_key(r,c,b,object,canonical,cause);};
    auto resolve=[&](const std::string& canonical,uintptr_t& object,std::string& cause){return reference_resolve(r,c,b,canonical,object,cause);};
    auto guard=[&](std::string& cause){live::Pin pin;return context(r,c,b,pin,cause)&&code(r,cause);};
    if(before->graph!=desired.graph){attempted=true;if(!release_graph_native::apply(r.image,memory,local.manager,desired.graph,key,resolve,guard,manager_receipt,e))return false;}
    if(before->prepared!=desired.prepared){attempted=true;if(!release_graph_native::apply(r.image,memory,local.manager,desired.prepared,key,resolve,guard,prepared_receipt,e))return false;}
    uintptr_t manager=0,release=0,celebration=0,mode=0;int32_t phase=0;
    if(!cycle_roots(r,c,b,manager,release,celebration,mode,phase,e))return false;
    const int32_t expected=desired.phase==cycle::Phase::Announcement?1:desired.phase==cycle::Phase::Playing?2:0;
    cycle_runtime.cycle=desired.cycle;cycle_runtime.phase=desired.phase;
    if(presentation&&phase!=expected){
        attempted=true;cycle_runtime.frame_original=true;uint32_t exception=0;
        exception=cn::phase(r.image,celebration,desired.phase==cycle::Phase::Announcement?cycle::Command::Show:desired.phase==cycle::Phase::Playing?cycle::Command::Begin:cycle::Command::Abort);
        cycle_runtime.frame_original=false;
        if(exception)return fail(e,"Release original Host phase presentation SEH="+std::to_string(exception));
    }
    if(!cycle_pause_client(r,c,b,e)||!cycle_random(r,c,b,desired.random,attempted,e))return false;
    auto actual=std::make_unique<cycle::Publication>();cycle::Proof final;a::Ledger after;
    if(!cycle_sample(r,c,b,*actual,final,after,e)||*actual!=desired)return fail(e,e.empty()?"Release whole native Manager/prepared graph/cycle/RNG readback differs from Host":e);
    if(!cycle_roots(r,c,b,manager,release,celebration,mode,phase,e)||phase!=expected)return fail(e,e.empty()?"Release actual original presentation phase differs from Host":e);
    cycle_runtime.local_finished=false;e.clear();return true;
}
a::Outcome cycle_perform(Runtime& r,const a::Binding& b,const a::Request& q,const a::Evidence* host,std::string& e){
    if(r.unknown||!cycle::valid(q.release_cycle)||cycle_runtime.operations.contains(q.action_uuid))return fail(e,"Release cycle unavailable or token already attempted; no replay"),a::Outcome::RejectedNoEffect;
    const auto controller=typed_action_runtime::controller_status();a::Context c;
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=q.action_uuid||!a::selected_session().current(c,e)||c.snapshot_uuid!=q.snapshot_uuid||(c.role==a::Role::Client)!=(host!=nullptr)||!code(r,e))return fail(e,e.empty()?"Release exact pending cycle Controller/context/role differs":e),a::Outcome::RejectedNoEffect;
    auto before_value=std::make_unique<cycle::Publication>();cycle::Proof before;a::Ledger money;
    if(!cycle_sample(r,c,b,*before_value,before,money,e)||!cycle::before(q.release_cycle,before,e))return a::Outcome::RejectedNoEffect;
    if(host&&(!host->release_cycle_before||!host->release_cycle_after||!host->release_cycle_value||*host->release_cycle_before!=before||host->before!=money))return fail(e,"Release Client exact complete before value/Ledger differs from Host"),a::Outcome::RejectedNoEffect;
    // Validate the complete ordered Host result before the first Client native effect.
    if(host&&(!cycle::effect(q.release_cycle,before,*host->release_cycle_after,e)||
       !cycle::publication_matches(*host->release_cycle_after,*host->release_cycle_value,hash,e)))return a::Outcome::RejectedNoEffect;
    if(cycle_runtime.operations.size()>=a::history_limit){auto oldest=cycle_runtime.operations.end();for(auto i=cycle_runtime.operations.begin();i!=cycle_runtime.operations.end();++i)if(i->second.retired&&(oldest==cycle_runtime.operations.end()||i->second.request.host_revision<oldest->second.request.host_revision))oldest=i;
        if(oldest==cycle_runtime.operations.end())return fail(e,"Release retained cycle receipts full"),a::Outcome::RejectedNoEffect;cycle_runtime.operations.erase(oldest);}
    auto& op=cycle_runtime.operations.try_emplace(q.action_uuid).first->second;op.context=c;op.binding=b;op.request=q;op.evidence.before=money;op.evidence.release_cycle_before=before;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"Release original cycle result Unknown":cause;r.unknown=true;rejected(r,e);return a::Outcome::Unknown;};
    uintptr_t manager=0,release=0,celebration=0,mode=0;int32_t native_phase=0;
    if(!cycle_roots(r,c,b,manager,release,celebration,mode,native_phase,e))return a::Outcome::RejectedNoEffect;
    const auto cmd=q.release_cycle.command;
    std::array<uint64_t,4> predicate_random=before.random;
    op.attempted=true;cycle_runtime.context=c;cycle_runtime.binding=b;
    struct OriginalScope {CycleOperation*& owner;OriginalScope(CycleOperation*& value,CycleOperation& op):owner(value){owner=&op;}~OriginalScope(){owner=nullptr;}} original(cycle_runtime.original,op);
    if(cmd==cycle::Command::Show){
        if(host){op.allowed=host->release_cycle_after->allowed;bool attempted=false;if(!cycle_random(r,c,b,host->release_cycle_after->predicate_random,attempted,e))return unknown(e);predicate_random=host->release_cycle_after->predicate_random;}
        else {op.native_exception=cn::predicate(r.image,release,op.allowed);if(op.native_exception)return unknown("Release original CanReleaseNow SEH="+std::to_string(op.native_exception));p::Local local;p::State scalar;a::Ledger after;if(!sample(r,c,b,local,scalar,after,e)||after!=money)return unknown(e.empty()?"Release native predicate changed full Ledger":e);predicate_random=scalar.random;}
        if(op.allowed){cycle_runtime.cycle=q.release_cycle.cycle;cycle_runtime.phase=cycle::Phase::Announcement;
            if(before.type==4){cycle_runtime.phase=cycle::Phase::AwaitPublish;op.native_exception=cn::script(r.image,op.script);}
            else op.native_exception=cn::show(r.image,manager);}
    }else if(cmd==cycle::Command::Finalize){op.native_exception=cn::script(r.image,op.script);if(!op.native_exception&&op.boundaries!=1)return unknown("Release original full Mode publish did not cross its one admitted physical boundary");cycle_runtime.last_terminal=cycle_runtime.cycle;cycle_runtime.cycle.clear();cycle_runtime.phase=cycle::Phase::Idle;cycle_runtime.finalize_id.clear();}
    else {cycle_runtime.phase=cmd==cycle::Command::Abort?cycle::Phase::Idle:cmd==cycle::Command::Skip?cycle::Phase::AwaitPublish:before.type==2||before.type==3?cycle::Phase::Playing:cycle::Phase::AwaitPublish;
        op.native_exception=cn::phase(r.image,celebration,cmd);if(cmd==cycle::Command::Abort){cycle_runtime.last_terminal=cycle_runtime.cycle;cycle_runtime.cycle.clear();}}
    if(op.native_exception||r.unknown)return unknown(op.native_exception?"Release original cycle call SEH="+std::to_string(op.native_exception):r.cause);
    if(!cycle_pause_client(r,c,b,e))return unknown(e);op.returned=true;
    if(cycle_runtime.phase==cycle::Phase::AwaitPublish&&!cycle_queue_finalize(r,c,b,e))return unknown(e);
    if(host){bool attempted=false;if(!cycle_adopt(r,c,b,*host->release_cycle_value,false,attempted,op.manager_apply,op.prepared_apply,e))return unknown(e);}
    auto after_value=std::make_unique<cycle::Publication>();cycle::Proof after;
    if(!cycle_sample(r,c,b,*after_value,after,op.evidence.after,e))return unknown(e);after.allowed=op.allowed;after.predicate_random=predicate_random;
    if(!cycle::effect(q.release_cycle,before,after,e)||(cmd!=cycle::Command::Finalize&&op.evidence.after!=money))return unknown(e.empty()?"Release non-publish original command changed full Ledger":e);
    if(host&&(after!=*host->release_cycle_after||op.evidence.after!=host->after))return unknown("Release Client full original cycle result differs from Host");
    op.evidence.release_cycle_after=std::move(after);op.evidence.release_cycle_value=std::move(*after_value);op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}
bool cycle_observe(Runtime& r,const a::Binding& b,const a::Request& q,a::Evidence& out,std::string& e){
    const auto found=cycle_runtime.operations.find(q.action_uuid);if(found==cycle_runtime.operations.end()||found->second.binding!=b||found->second.request!=q||!found->second.confirmed)return fail(e,"Release confirmed original cycle receipt absent");const auto& op=found->second;
    auto value=std::make_unique<cycle::Publication>();cycle::Proof proof;a::Ledger money;
    if(!cycle_sample(r,op.context,b,*value,proof,money,e)||money!=op.evidence.after)return fail(e,e.empty()?"Release current exact full Ledger differs before retirement":e);
    // Local celebration timers may advance; authored values and the retained
    // original operation stay exact until the Controller retires this action.
    if(value->graph!=op.evidence.release_cycle_value->graph||value->prepared!=op.evidence.release_cycle_value->prepared)return fail(e,"Release current whole authored graph changed before Controller retirement");
    out=op.evidence;out.retirement_confirmed=op.retired;e.clear();return true;
}
void cycle_tick(Runtime& r){
    if(r.unknown)return;const auto controller=typed_action_runtime::controller_status();if(controller.value("pending",true)||controller.value("locked",true))return;
    if(!cycle_runtime.pending&&!cycle_runtime.finalize_id.empty()){
        if(!cycle_runtime.context||!cycle_runtime.binding){r.unknown=true;rejected(r,"Release retained natural Finish has no saved Context/Binding");return;}
        cycle_runtime.pending=CyclePending{*cycle_runtime.context,*cycle_runtime.binding,{cycle::Command::Finalize,cycle_runtime.cycle},cycle_runtime.finalize_id};
    }
    if(!cycle_runtime.pending)return;auto pending=std::move(*cycle_runtime.pending);cycle_runtime.pending.reset();a::Context current;a::Binding binding;std::string e;
    if(!a::selected_session().current(current,e)||current!=pending.context||!typed_action_runtime::network_binding(current.snapshot_uuid,binding,e)||binding!=pending.binding||!typed_action_runtime::submit_release_cycle(pending.id,pending.intent,e)){
        r.unknown=true;rejected(r,e.empty()?"Release retained original cycle intent could not order; no replay":e);return;}++cycle_runtime.submitted;
}
nlohmann::json cycle_status(){auto ops=nlohmann::json::object();for(const auto& [id,op]:cycle_runtime.operations)ops[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"boundaries",op.boundaries},{"native_exception",op.native_exception},{"cause",op.cause},{"before",op.evidence.release_cycle_before?cycle::encode(*op.evidence.release_cycle_before):nlohmann::json(nullptr)},{"after",op.evidence.release_cycle_after?cycle::encode(*op.evidence.release_cycle_after):nlohmann::json(nullptr)}};
    return {{"cycle",cycle_runtime.cycle},{"phase",uint8_t(cycle_runtime.phase)},{"finalize_action",cycle_runtime.finalize_id},{"boundary_registration",cycle_runtime.boundary.registration()},{"boundary_failed",cycle_runtime.boundary.failed()},{"captures",cycle_runtime.captures},{"submitted",cycle_runtime.submitted},{"host_finishes",cycle_runtime.host_finishes},{"client_finishes",cycle_runtime.client_finishes},{"frame_applies",cycle_runtime.frame_applies},{"operations",std::move(ops)}};
}
