namespace scenery_stroke {
namespace {
struct OrderedGesture {
    json arguments;
    action::Context context;
    action::Binding binding;
    bool attempted=false,returned=false;
    std::string cause;
    json native=json::object(),preparation=json::object();
};
std::map<std::string,OrderedGesture> ordered_gestures;
std::optional<std::string> ordered_gesture_pending;
json ordered_gesture_format(const std::string& token,const OrderedGesture& job){
    json out=job.native;
    out["token"]=token;out["snapshot_uuid"]=job.context.snapshot_uuid;
    out["queued"]=!job.attempted&&job.cause.empty();out["ordered"]=true;
    out["input_attempted"]=job.attempted;out["input_returned"]=job.returned;
    out["session"]=job.context.session;out["peer_mask"]=job.context.peer_mask;
    out["local_peer"]=job.context.local_peer;out["host_peer"]=job.context.host_peer;
    out["preparation"]=job.preparation;out["cause"]=job.cause;return out;
}
uint32_t gesture_select_call(uintptr_t image,unsigned step,uintptr_t object,int32_t index,uintptr_t& pane,const char* name)noexcept{
    struct String{const char* data;size_t size;char rest[16];};
    const String id{name,name?std::strlen(name):0,{}};
    __try{
        if(step==0)reinterpret_cast<void(*)(void*)>(image+0x580870)(reinterpret_cast<void*>(object));
        else if(step==1)reinterpret_cast<void(*)(void*,int32_t)>(image+0x7420f0)(reinterpret_cast<void*>(object),index);
        else if(step==2)pane=reinterpret_cast<uintptr_t(*)(void*,const String*)>(image+0x81d860)(reinterpret_cast<void*>(object),&id);
        else if(step==3)reinterpret_cast<void(*)(void*)>(image+0x73ffe0)(reinterpret_cast<void*>(object));
        else if(step==4)reinterpret_cast<void(*)(void*,float)>(image+0x5d4620)(reinterpret_cast<void*>(object),0.0f);
        else reinterpret_cast<void(*)(void*,float)>(image+0x5d7490)(reinterpret_cast<void*>(object),0.0f);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t gesture_erase_area(uintptr_t image,uintptr_t cross_section,float& value)noexcept{
    __try{value=reinterpret_cast<float(*)(const void*)>(image+0xdb2d0)(reinterpret_cast<const void*>(cross_section));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool gesture_prepare_eraser(Runtime& r,uintptr_t receiver,json& receipt,std::string& e){
    // SceneryEraser owns PaintLogic at +0x150. Its cross-section starts at
    // PaintLogic+0xc0; the original Update applies the native options to it.
    uint8_t active=0;uintptr_t prior_bounds=0;
    if(!q::at(r.memory,receiver,0x580,active,e)||!q::at(r.memory,receiver,0x228,prior_bounds,e))return false;
    if(active)return q::fail(e,"Scenery eraser preparation found an active original stroke");
    receipt={{"brush_bounds_before",prior_bounds!=0},{"update_attempted",false},{"update_returned",false}};
    constexpr unsigned char update[]{0x56,0x53,0x48,0x81,0xec,0xa8,0x00,0x00,0x00};
    constexpr unsigned char area[]{0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x51,0x18};
    if(!original_bytes(r,{0x5d7490,update,sizeof(update)},e)||!original_bytes(r,{0xdb2d0,area,sizeof(area)},e))return false;
    uintptr_t unused=0;receipt["update_attempted"]=true;
    if(!fault(gesture_select_call(r.image,5,receiver,0,unused,nullptr),"Scenery eraser Update",e))return false;
    receipt["update_returned"]=true;
    uintptr_t bounds=0;
    if(!q::at(r.memory,receiver,0x580,active,e)||!q::at(r.memory,receiver,0x228,bounds,e))return false;
    receipt["brush_bounds_after"]=bounds!=0;
    if(active||!bounds)return q::fail(e,"Scenery eraser original Update did not prepare an idle brush cross-section");
    float value=0;
    if(!fault(gesture_erase_area(r.image,receiver+0x210,value),"Scenery eraser brush Area",e))return false;
    receipt["brush_area"]=value;
    return (std::isfinite(value)&&value!=0)||q::fail(e,"Scenery eraser original Update produced an empty brush cross-section");
}

bool gesture_select(Runtime& r,OrderedGesture& job,std::string& e){
    namespace ui=placement_ui_probe;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,e)||world.uuid!=job.context.snapshot_uuid||
       world.world_epoch!=job.binding.native_epoch||world.generation!=job.binding.native_generation)
        return q::fail(e,e.empty()?"Scenery tool selection native world changed":e);
    const bool paint=job.arguments.at("kind")=="paint";
    ui::Context context{r.image,reinterpret_cast<uintptr_t>(world.state),world.world_epoch,world.generation,r.thread};
    ui::Request request;request.token=job.arguments.at("token").get<std::string>();
    request.type=job.arguments.at("selection").at("type").get<std::string>();
    request.variant=job.arguments.at("selection").at("variant").get<std::string>();
    request.family=paint?ui::Family::Scenery:ui::Family::SceneryErase;
    if(paint){bool scenery=false;
        if(!scenery_kiss::resolve_ui_scenery_catalog(r.image,r.thread,request.type,request.variant,scenery,request.category,request.variant_hash,e)||!scenery)
            return q::fail(e,e.empty()?"Scenery gesture selection is not an actual catalog entry":e);
    }
    ui::ProcessMemory memory;ui::Probe proof;ui::Binding binding;
    if(!proof.initialize(context,e)||!ui::read_existing_tool(memory,context,request,binding,e))return false;
    uintptr_t unused=0;
    if(!fault(gesture_select_call(r.image,0,binding.button,0,unused,nullptr),"Scenery ItemButton Activate",e))return false;
    uintptr_t active=0;
    if(!q::at(r.memory,binding.cursor,0x190,active,e)||active!=binding.behaviour)
        return q::fail(e,e.empty()?"Scenery original ItemButton did not select its owned behaviour":e);
    if(!paint)return gesture_prepare_eraser(r,binding.behaviour,job.preparation,e);
    auto mode=[&](const char* value,const char* id){
        uintptr_t window=0,pane=0;
        if(!ui::read_scenery_mode_window(memory,binding,window,e)||
           !fault(gesture_select_call(r.image,2,window,0,pane,id),"Scenery FindPane",e)||
           !ui::read_scenery_mode_button(memory,binding,window,pane,e,value)||
           !fault(gesture_select_call(r.image,3,pane,0,unused,nullptr),"Scenery ModeButton Activate",e)||
           !fault(gesture_select_call(r.image,4,binding.behaviour,0,unused,nullptr),"Scenery mode Update",e))return false;
        return true;
    };
    if(!mode("single","mode_single"))return false;
    int32_t index=-1;uintptr_t list=0;
    if(!ui::read_palette_index(memory,binding,request,index,list,e)||
       !fault(gesture_select_call(r.image,1,list,index,unused,nullptr),"Scenery ModelList SetSelectedIndex",e)||
       !mode("paint","mode_paint"))return false;
    return true;
}
bool queue_ordered_gesture(const json& args,json& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);bool inside=false;
    const std::set<std::string> fields{"token","snapshot_uuid","expected_world_epoch","expected_generation","kind","region_id","points","selection"};
    if(!r||!r->ready||!installed||!Config::instance().local_test||r->thread!=GetCurrentThreadId()||capture||queued||ordered_gesture_pending)
        return q::fail(e,"Ordered Scenery gesture requires an owned idle capture");
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||
       !typed_action_runtime::original_go_admission(inside,e))return q::fail(e,e.empty()?"Scenery diagnostic enqueue requires ready owned Controller":e);
    if(controller.value("pending",true)&&!inside)return q::fail(e,"Scenery diagnostic enqueue outside OriginalGo requires idle Controller");
    if(!args.is_object()||args.size()!=fields.size())return q::fail(e,"Ordered Scenery gesture requires exact gesture and selection fields");
    for(const auto& field:fields)if(!args.contains(field))return q::fail(e,"Ordered Scenery gesture missing field="+field);
    if(!args.at("token").is_string()||!args.at("snapshot_uuid").is_string()||!args.at("kind").is_string()||
       !args.at("expected_world_epoch").is_number_unsigned()||!args.at("expected_generation").is_number_unsigned()||
       !args.at("region_id").is_number_integer()||!args.at("points").is_array())return q::fail(e,"Ordered Scenery gesture scalar types differ");
    const auto token=args.at("token").get<std::string>(),snapshot=args.at("snapshot_uuid").get<std::string>(),kind=args.at("kind").get<std::string>();
    const auto region_id=args.at("region_id").get<int64_t>();const auto& selection=args.at("selection");
    if(!gesture_uuid(token)||!gesture_uuid(snapshot)||ordered_gestures.contains(token)||gesture_jobs.contains(token)||ordered_gestures.size()>=32||
       (kind!="paint"&&kind!="erase")||region_id<0||region_id>INT32_MAX||args.at("points").size()<2||args.at("points").size()>32||
       !selection.is_object()||selection.size()!=2||!selection.contains("type")||!selection.contains("variant")||
       !selection.at("type").is_string()||!selection.at("variant").is_string())return q::fail(e,"Ordered Scenery token/selection/points invalid");
    for(const char* key:{"type","variant"}){const auto text=selection.at(key).get<std::string>();
        if(text.size()>256||text.find('\0')!=std::string::npos||(kind=="paint"?text.empty():!text.empty()))return q::fail(e,"Ordered Scenery selection identity invalid");}
    for(const auto& point:args.at("points")){
        if(!point.is_array()||point.size()!=3)return q::fail(e,"Ordered Scenery point requires three coordinates");
        for(const auto& item:point)if(!item.is_number()||!std::isfinite(item.get<double>())||std::abs(item.get<double>())>std::numeric_limits<float>::max())return q::fail(e,"Ordered Scenery point is not finite float32");
    }
    for(const auto& [id,prior]:ordered_gestures){(void)id;if(prior.attempted&&!prior.returned)return q::fail(e,"Prior ordered Scenery input remains uncertain; no new gesture");}
    OrderedGesture job;job.arguments=args;
    if(!action::selected_session().current(job.context,e)||job.context.snapshot_uuid!=snapshot||
       !typed_action_runtime::read_binding(snapshot,job.binding,e)||job.binding.native_epoch!=args.at("expected_world_epoch").get<uint64_t>()||
       job.binding.native_generation!=args.at("expected_generation").get<uint64_t>())return q::fail(e,e.empty()?"Ordered Scenery enqueue Session/Binding differs":e);
    ordered_gestures.emplace(token,std::move(job));ordered_gesture_pending=token;
    out=ordered_gesture_format(token,ordered_gestures.at(token));return true;
}
void retire_ordered_gesture(){
    if(!ordered_gesture_pending)return;
    auto& job=ordered_gestures.at(*ordered_gesture_pending);
    job.cause="Ordered Scenery input cancelled after session retirement";
    ordered_gesture_pending.reset();reject(job.cause);
}
}
void tick_original_gesture(){
    if(!ordered_gesture_pending)return;
    auto* r=published.load(std::memory_order_acquire);std::string e;bool inside=false;
    if(!typed_action_runtime::original_go_admission(inside,e)||!inside){if(e.empty())return;}
    const auto token=*ordered_gesture_pending;auto& job=ordered_gestures.at(token);
    ordered_gesture_pending.reset();
    if(e.empty()&&(!r||!current_capture(job.context,job.context.snapshot_uuid,job.binding,e)))if(e.empty())e="Ordered Scenery queued owner retired";
    if(e.empty()){
        job.attempted=true;
        try{
            if(gesture_select(*r,job,e)){
                json arguments=job.arguments;arguments.erase("selection");
                job.returned=execute_native_gesture(arguments,job.native,e);
            }
        }catch(const std::exception& failure){e=std::string("Ordered Scenery input: ")+failure.what();}
    }
    job.cause=e;
    if(!job.returned){if(job.cause.empty())job.cause="Ordered Scenery input failed without native cause";reject(job.cause);}
}
bool native_gesture(const json& args,json& out,std::string& e){
    if(typed_action_runtime::controller_status().value("ordered",false))return queue_ordered_gesture(args,out,e);
    return execute_native_gesture(args,out,e);
}
bool read_gesture(const json& args,json& out,std::string& e){
    if(!args.is_object()||args.size()!=1||!args.contains("token")||!args.at("token").is_string())return q::fail(e,"Scenery gesture read requires exact token");
    const auto token=args.at("token").get<std::string>();const auto found=ordered_gestures.find(token);
    if(found==ordered_gestures.end())return read_native_gesture(args,out,e);
    out=ordered_gesture_format(token,found->second);e.clear();return true;
}
}
