namespace scenery_stroke {
namespace {
struct Capture {
    action::Binding binding;
    scenery_stroke_capture::Local local;
    action::SceneryStrokeIntent intent;
    std::string snapshot,cause;
};
struct Pending {action::Binding binding;action::SceneryStrokeIntent intent;std::string snapshot,id;};
struct Port;
struct Operation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    scenery_stroke_native::Receipt native;std::shared_ptr<Port> port;
    bool confirmed=false,retired=false;std::string cause;
};
bool installed=false,installation_attempted=false;
std::array<uintptr_t,4> originals{};
std::optional<Capture> capture;
std::optional<Pending> queued;
std::map<std::string,Operation> operations,quarantined;
uint64_t captures=0,submissions=0,rejections=0;std::string cause;
bool verify_routes(Runtime&,bool,std::string&);
void retire_histories();
json history_status();
void reject(const std::string& error){++rejections;if(cause!=error){cause=error;LOG_ERROR("[SceneryStroke] {}",error);}}
bool fault(uint32_t code,const char* operation,std::string& e){
    return !code||q::fail(e,std::string("Scenery stroke original ")+operation+" SEH="+std::to_string(code));
}
bool source_world(Runtime& r,uintptr_t receiver,Kind kind,host::WorldState& out,std::string& e){
    host::NativeOrigin origin;host::LookupApi lookup;
    if(!host::resolve_lookup(r.memory.process,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",lookup,e))return false;
    host::Reader reader(r.memory.process,origin,r.image,r.thread,lookup);
    if(!reader.current(out,false,e))return false;uint8_t own_edit=0;
    if(kind==Kind::Paint){uintptr_t scope=0;if(!q::at(r.memory,receiver,0x360,scope,e)||(scope&&!q::at(r.memory,scope,0,own_edit,e)))return false;
        if(own_edit>1)return q::fail(e,"Scenery source EditScope flag differs");}
    return (!out.saves&&!out.active_graph&&out.edits==own_edit)||q::fail(e,"Scenery source has a foreign Save/Edit/Undo scope");
}
struct Generator {
    Runtime& r;calls::Array result;
    bool generate(uintptr_t pattern,uintptr_t shape,uintptr_t& out,bool& made,std::string& e){
        const auto code=calls::generate(r.image,pattern,reinterpret_cast<const void*>(shape),result,made);out=uintptr_t(&result);return fault(code,"GetDotsInside",e);
    }
    bool read(uintptr_t array,size_t budget,std::vector<action::SceneryStrokePose>& out,std::string& e){return scenery_stroke_capture::dots(r.memory,r.image,array,budget,out,e);}
    bool destroy(uintptr_t array,std::string& e){
        if(array!=uintptr_t(&result))return q::fail(e,"Scenery generated array owner differs");return fault(calls::destroy_generated(r.image,result),"generated array destructor",e);
    }
};
bool read_source(Runtime& r,uintptr_t receiver,Kind kind,Capture& out,std::string& e){
    host::WorldState current;if(!source_world(r,receiver,kind,current,e))return false;
    auto identify=[&](int32_t category,int32_t variant,std::string& type,std::string& file,std::string& why){return catalog(r,category,variant,type,file,why);};
    auto resolve=[&](int32_t id,uintptr_t receiver_region,uintptr_t root,std::string& why){uintptr_t actual=0;
        return root==current.root&&region(r,current,id,receiver_region,actual,why);};
    return scenery_stroke_capture::read(r.memory,r.image,receiver,kind,identify,resolve,out.local,out.intent,e);
}
bool capture_paint(Runtime& r,uintptr_t receiver,uintptr_t shape,Kind kind,std::string& e){
    const auto controller=typed_action_runtime::controller_status();
    if(!installed||!r.ready||queued||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return q::fail(e,"Scenery stroke capture requires an idle ready Controller");
    if(capture&&capture->local.receiver!=receiver)return q::fail(e,"Scenery stroke already belongs to another native cursor");
    if(capture&&!capture->cause.empty())return q::fail(e,capture->cause);
    if(!capture&&!verify_routes(r,true,e))return false;
    Capture current;if(!read_source(r,receiver,kind,current,e))return false;
    if(!capture){current.snapshot=net::current_snapshot_id();
        if(!typed_action_runtime::network_binding(current.snapshot,current.binding,e))return false;capture=std::move(current);++captures;
    }else if(capture->local!=current.local||capture->intent.kind!=current.intent.kind||capture->intent.region!=current.intent.region||
             capture->intent.mode!=current.intent.mode||capture->intent.type!=current.intent.type||capture->intent.variant!=current.intent.variant||
             capture->intent.random_variant!=current.intent.random_variant||capture->intent.replace!=current.intent.replace)
        return q::fail(e,"Scenery source mode/catalog/Region/pattern changed inside one stroke");
    action::SceneryStrokeStamp stamp;
    if(!poly::read(r.memory.process,shape,stamp.shape,e))return false;
    size_t used=0;for(const auto& value:capture->intent.stamps){
        if(used>action::max_payload||value.dots.size()>action::max_payload-used)return q::fail(e,"Scenery source transform resource budget exceeded");used+=value.dots.size();}
    Generator generator{r};
    if(!scenery_stroke_capture::generated(generator,capture->local,shape,action::max_payload-used,stamp.dots,e))return false;
    capture->intent.stamps.push_back(std::move(stamp));
    // Bounds are supplied by the original PaintLogic only at Finish. Finite
    // zero placeholders allow the same complete shape/pose budget check now.
    return action::valid_scenery_stroke(capture->intent)||q::fail(e,"Scenery source complete stroke shape/pose budget is invalid");
}
void paint_entry(void* receiver,const void* shape,Kind kind){
    auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<calls::Paint>(originals[kind==Kind::Paint?0:2]);
    if(!r||!original){reject("Scenery AddPaint hook has no published typed original");return;}
    if(r->thread!=GetCurrentThreadId()||!net::session_wire::connected()||!typed_action_runtime::status().value("started",false)){original(receiver,shape);return;}
    try{bool own=false;std::string e;
        if(!scenery_kiss::live_cursor(r->memory.process,r->image,uintptr_t(receiver),own,e)){reject(e);return;}
        if(!own){original(receiver,shape);return;}
        if(!capture_paint(*r,uintptr_t(receiver),uintptr_t(shape),kind,e)){
            if(capture&&capture->cause.empty())capture->cause=e;reject(e);}
    }catch(const std::exception& ex){const auto e=std::string("Scenery source AddPaint: ")+ex.what();if(capture&&capture->cause.empty())capture->cause=e;reject(e);}
}
void brush_paint_entry(void* receiver,const void* shape){paint_entry(receiver,shape,Kind::Paint);}
void erase_paint_entry(void* receiver,const void* shape){paint_entry(receiver,shape,Kind::Erase);}
bool finish_capture(Runtime& r,uintptr_t receiver,Kind kind,std::string& e){
    if(!capture||capture->local.receiver!=receiver)return q::fail(e,"Scenery source Finish has no matching captured cursor");
    auto value=std::move(*capture);capture.reset();e=value.cause;
    std::string input_error;uintptr_t graph=0;
    if(!q::at(r.memory,receiver,kind==Kind::Paint?0xc50:0x5c0,graph,input_error)||graph){
        const auto failed=input_error.empty()?"Scenery source unexpectedly owns a native mutation graph":input_error;
        if(e.empty())e=failed;else e+="; source graph: "+failed;return false;}
    if(!scenery_stroke_capture::finish(r.memory,value.local,value.intent,input_error)&&e.empty())e=input_error;
    uintptr_t cursor=0;
    if((!q::at(r.memory,r.image,0x13df9d0,cursor,input_error)||!q::typed(r.memory,r.image,cursor,0x1786dd0,input_error)||
        !q::at(r.memory,cursor,0x1e4,value.intent.finish_position,input_error))&&e.empty())e=input_error;
    bool entered=false,returned=false;const auto code=calls::finish(reinterpret_cast<calls::Finish>(originals[kind==Kind::Paint?1:3]),receiver,entered,returned);
    if(code){const auto cleanup="Scenery original empty source Finish SEH="+std::to_string(code);if(e.empty())e=cleanup;else e+="; source cleanup: "+cleanup;return false;}
    if(!entered||!returned)return q::fail(e,e.empty()?"Scenery original empty source Finish did not return":e);
    host::WorldState current;uintptr_t remaining_scope=0;
    if(!source_world(r,receiver,kind,current,input_error)||current.edits||!q::at(r.memory,receiver,kind==Kind::Paint?0xc50:0x5c0,graph,input_error)||graph||
       (kind==Kind::Paint&&(!q::at(r.memory,receiver,0x360,remaining_scope,input_error)||remaining_scope))){
        if(e.empty())e=input_error.empty()?"Scenery source original graph/EditScope cleanup differs":input_error;return false;}
    if(!e.empty())return false;
    if(value.binding.native_epoch!=current.origin.world_epoch||value.binding.native_generation!=current.origin.generation||value.snapshot!=net::current_snapshot_id()||queued)
        return q::fail(e,"Scenery source snapshot/World binding changed before submission");
    if(!action::valid_scenery_stroke(value.intent))return q::fail(e,"Scenery finished source intent is invalid");
    const auto id=uuid(e);if(id.empty())return false;
    action::Message proposal;proposal.kind=action::Kind::Request;proposal.session=1;proposal.request.action_uuid=id;proposal.request.snapshot_uuid=value.snapshot;
    proposal.request.peer=1;proposal.request.sequence=1;proposal.request.action=kind==Kind::Paint?action::Action::SceneryBrushStroke:action::Action::SceneryEraseStroke;
    proposal.request.canonical_record_key="scenery-stroke:"+id;proposal.request.scenery_stroke=value.intent;
    std::vector<uint8_t> bytes;if(!action::encode(proposal,bytes,e))return false;
    queued=Pending{value.binding,std::move(value.intent),std::move(value.snapshot),id};return true;
}
void finish_entry(void* receiver,Kind kind){
    auto* r=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<calls::Finish>(originals[kind==Kind::Paint?1:3]);
    if(!r||!original){reject("Scenery Finish hook has no published typed original");return;}
    if(r->thread!=GetCurrentThreadId()||!capture||capture->local.receiver!=uintptr_t(receiver)){original(receiver);return;}
    try{std::string e;if(!finish_capture(*r,uintptr_t(receiver),kind,e))reject(e);}
    catch(const std::exception& ex){reject(std::string("Scenery source Finish: ")+ex.what());}
}
void brush_finish_entry(void* receiver){finish_entry(receiver,Kind::Paint);}
void erase_finish_entry(void* receiver){finish_entry(receiver,Kind::Erase);}
}
}
