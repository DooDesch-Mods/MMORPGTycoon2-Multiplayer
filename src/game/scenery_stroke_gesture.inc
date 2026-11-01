namespace scenery_stroke {
namespace {
namespace gesture=scenery_gesture;
struct GestureJob {
    std::string token,snapshot;
    action::Binding binding;std::optional<action::Context> context;
    host::WorldState world;
    uintptr_t cursor=0,receiver=0,default_receiver=0,region_owner=0;
    int32_t region_id=-1;
    Kind kind=Kind::Erase;
    uint64_t rejected_before=0,captured_before=0;
    gesture::Receipt receipt;
};
std::map<std::string,GestureJob> gesture_jobs;
json gesture_format(const GestureJob& job){const auto& v=job.receipt;return {
    {"token",job.token},{"snapshot_uuid",job.snapshot},{"attempted",v.attempted},{"returned",v.returned},
    {"captured",v.captured},{"stamps",v.stamps},{"action_uuid",v.action_uuid},{"cause",v.cause},
    {"host_ordered_action_confirmed",false}};}
bool gesture_uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;}
    return true;
}
bool gesture_current(Runtime& r,GestureJob& job,std::string& e){
    if(GetCurrentThreadId()!=r.thread||!Config::instance().local_test||!r.ready||!installed||!net::session_wire::connected())return q::fail(e,"Scenery gesture requires owned ready Mainthread session");
    action::Binding binding;host::WorldState current;uintptr_t cursor=0,receiver=0,default_receiver=0;
    if(net::current_snapshot_id()!=job.snapshot)return q::fail(e,"Scenery gesture shared Snapshot changed");
    if(!current_capture(job.context,job.snapshot,job.binding,e))return false;
    binding=job.binding;
    if(binding!=job.binding)return q::fail(e,"Scenery gesture World binding changed");
    if(!q::at(r.memory,r.image,0x13df9d0,cursor,e))return false;
    if(cursor!=job.cursor)return q::fail(e,"Scenery gesture Cursor singleton changed");
    if(!q::typed(r.memory,r.image,cursor,0x1786dd0,e)||!q::at(r.memory,cursor,0x190,receiver,e)||
       !q::at(r.memory,cursor,0x198,default_receiver,e))return false;
    if(receiver!=job.receiver)return q::fail(e,"Scenery gesture selected behaviour changed");
    if(default_receiver!=job.default_receiver)return q::fail(e,"Scenery gesture Default behaviour owner changed");
    // Cursor keeps its Default behaviour separately from the selected tool.
    if(!q::typed(r.memory,r.image,default_receiver,0x17391a0,e)||
       !q::typed(r.memory,r.image,receiver,job.kind==Kind::Paint?0x17396b0:0x173fc60,e)||
       !source_world(r,receiver,job.kind,current,e))return false;
    if(!host::same_world(current.origin,job.world.origin))return q::fail(e,"Scenery gesture native World origin changed");
    if(current.root!=job.world.root)return q::fail(e,"Scenery gesture native World root changed");
    if(current.selection!=job.world.selection)return q::fail(e,"Scenery gesture native selection owner changed");
    uintptr_t region_owner=0;
    if(!region(r,current,job.region_id,job.region_owner,region_owner,e))return false;
    native::SimulationState simulation;if(!native::read_simulation_state(simulation,e))return false;
    return simulation.clock.time_scale==0||q::fail(e,"Scenery gesture requires paused simulation");
}
uint32_t gesture_call(uintptr_t image,uintptr_t receiver,uintptr_t cursor,Kind kind,gesture::Step step,const gesture::Point& point)noexcept{
    const bool paint=kind==Kind::Paint;
    const uintptr_t rva=step==gesture::Step::Hover?(paint?0x5d5960:0x5d6460):
        step==gesture::Step::Press?(paint?0x5cf440:0x5d64e0):
        step==gesture::Step::Drag?(paint?0x5cead0:0x5d5e50):(paint?0x5d0600:0x5d6ca0);
    __try{
        if(step==gesture::Step::Drag)reinterpret_cast<void(*)(void*,void*,const void*,float)>(image+rva)(reinterpret_cast<void*>(receiver),reinterpret_cast<void*>(cursor),point.data(),0.0f);
        else reinterpret_cast<void(*)(void*,void*,const void*)>(image+rva)(reinterpret_cast<void*>(receiver),reinterpret_cast<void*>(cursor),point.data());
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t gesture_region(uintptr_t image,uintptr_t root,const gesture::Point& point,uintptr_t& region_owner)noexcept{
    __try{region_owner=reinterpret_cast<uintptr_t(*)(void*,const void*,void*)>(image+0x5089b0)(reinterpret_cast<void*>(root),point.data(),nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct GesturePort {
    Runtime& r;GestureJob& job;
    bool current(std::string& e){return gesture_current(r,job,e);}
    bool invoke(gesture::Step step,const gesture::Point& point,std::string& e){
        const auto exception=gesture_call(r.image,job.receiver,job.cursor,job.kind,step,point);
        return !exception||q::fail(e,"Scenery gesture callback SEH="+std::to_string(exception)+" step="+std::to_string(int(step))+"; no replay");
    }
    bool observed(gesture::Step step,std::string& e){
        if(rejections!=job.rejected_before)return q::fail(e,"Scenery gesture source rejected: "+cause);
        if(!current(e))return false;
        if(step==gesture::Step::Hover){Capture source;if(!read_source(r,job.receiver,job.kind,source,e))return false;
            if(source.intent.region!=job.region_id)return q::fail(e,"Scenery gesture hovered Region differs");}
        return true;
    }
    bool completed(std::string& id,size_t& stamps,std::string& e){
        if(capture||!queued||queued->binding!=job.binding||queued->snapshot!=job.snapshot||queued->intent.kind!=job.kind||
           queued->intent.region!=job.region_id||captures!=job.captured_before+1||queued->intent.stamps.empty())
            return q::fail(e,"Scenery gesture returned without one complete queued stroke; no replay");
        id=queued->id;stamps=queued->intent.stamps.size();return true;
    }
};
}
bool execute_native_gesture(const json& args,json& out,std::string& e){
    out=json::object();GestureJob* retained=nullptr;
    try{
        auto* r=published.load(std::memory_order_acquire);
        if(!r||!Config::instance().local_test||!r->ready||!installed||GetCurrentThreadId()!=r->thread||capture||queued)
            return q::fail(e,"Scenery gesture requires owned ready idle Mainthread capture");
        if(!input_admission(e))return false;
        const std::set<std::string> fields{"token","snapshot_uuid","expected_world_epoch","expected_generation","kind","region_id","points"};
        if(!args.is_object()||args.size()!=fields.size())return q::fail(e,"Scenery gesture requires exact token/Snapshot/World/kind/Region/points");
        for(const auto& field:fields)if(!args.contains(field))return q::fail(e,"Scenery gesture missing field="+field);
        if(!args.at("token").is_string()||!args.at("snapshot_uuid").is_string()||!args.at("kind").is_string()||
           !args.at("expected_world_epoch").is_number_unsigned()||!args.at("expected_generation").is_number_unsigned()||
           !args.at("region_id").is_number_integer()||!args.at("points").is_array())return q::fail(e,"Scenery gesture field types differ");
        GestureJob initial;initial.token=args.at("token").get<std::string>();initial.snapshot=args.at("snapshot_uuid").get<std::string>();
        const auto kind=args.at("kind").get<std::string>();const auto id=args.at("region_id").get<int64_t>();
        if(!gesture_uuid(initial.token)||!gesture_uuid(initial.snapshot)||gesture_jobs.contains(initial.token)||gesture_jobs.size()>=32||
           (kind!="paint"&&kind!="erase")||id<0||id>INT32_MAX||args.at("points").size()<2||args.at("points").size()>32)
            return q::fail(e,"Scenery gesture token/receipt capacity/kind/Region/point count differs");
        for(const auto& [token,job]:gesture_jobs){(void)token;if(job.receipt.attempted&&!job.receipt.captured)return q::fail(e,"Scenery prior gesture remains uncertain; no new input");}
        initial.kind=kind=="paint"?Kind::Paint:Kind::Erase;initial.region_id=int32_t(id);
        std::vector<gesture::Point> points;
        for(const auto& row:args.at("points")){if(!row.is_array()||row.size()!=3)return q::fail(e,"Scenery gesture point requires3 coordinates");gesture::Point point;
            for(size_t i=0;i<3;++i){if(!row[i].is_number())return q::fail(e,"Scenery gesture coordinate is not numeric");const auto value=row[i].get<double>();
                if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return q::fail(e,"Scenery gesture coordinate outside finite float32");point[i]=float(value);}points.push_back(point);}
        if(!verify_routes(*r,true,e))return false;
        for(const auto& guard:gesture::guards)if(!original_bytes(*r,guard,e))return false;
        if(initial.snapshot!=net::current_snapshot_id()||!capture_binding(initial.snapshot,initial.binding,initial.context,e)||
           initial.binding.native_epoch!=args.at("expected_world_epoch").get<uint64_t>()||initial.binding.native_generation!=args.at("expected_generation").get<uint64_t>()||
           !q::at(r->memory,r->image,0x13df9d0,initial.cursor,e)||!q::at(r->memory,initial.cursor,0x190,initial.receiver,e)||
           !q::at(r->memory,initial.cursor,0x198,initial.default_receiver,e)||
           !source_world(*r,initial.receiver,initial.kind,initial.world,e)||!region(*r,initial.world,initial.region_id,0,initial.region_owner,e)||!gesture_current(*r,initial,e))
            return q::fail(e,e.empty()?"Scenery gesture initial Snapshot/World/cursor differs":e);
        for(const auto& point:points){uintptr_t actual=0;const auto exception=gesture_region(r->image,initial.world.root,point,actual);
            if(exception||actual!=initial.region_owner)return q::fail(e,"Scenery gesture point outside exact owned Region SEH="+std::to_string(exception));}
        initial.rejected_before=rejections;initial.captured_before=captures;
        retained=&gesture_jobs.emplace(initial.token,std::move(initial)).first->second;
        GesturePort port{*r,*retained};const bool okay=gesture::execute(port,points,retained->receipt,e);out=gesture_format(*retained);
        if(!okay)LOG_ERROR("[SceneryGesture] token={} cause={}; no replay",retained->token,e);
        return okay;
    }catch(const std::exception& failure){e=std::string("Scenery gesture request failed: ")+failure.what();
        if(retained){retained->receipt.cause=e;out=gesture_format(*retained);}LOG_ERROR("[SceneryGesture] {}; no replay",e);return false;}
}
bool read_native_gesture(const json& args,json& out,std::string& e){
    out=json::object();if(!args.is_object()||args.size()!=1||!args.contains("token")||!args.at("token").is_string())return q::fail(e,"Scenery gesture read requires exact token");
    const auto found=gesture_jobs.find(args.at("token").get<std::string>());if(found==gesture_jobs.end())return q::fail(e,"Scenery gesture token is unknown");
    out=gesture_format(found->second);e.clear();return true;
}
}
