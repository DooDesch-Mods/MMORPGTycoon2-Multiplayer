namespace zone_ordered_ui {
namespace a=action_session;
struct Job {
    json arguments,native=json::object();a::Context context;a::Binding binding;
    bool attempted=false,returned=false;std::string cause;
};
std::map<std::string,Job> jobs;
std::optional<std::string> pending;
bool current(const Job& job,std::string& e){
    a::Context context;a::Binding binding;
    if(!a::selected_session().current(context,e)||!typed_action_runtime::read_binding(job.context.snapshot_uuid,binding,e))return false;
    return (context==job.context&&binding==job.binding&&net::current_snapshot_id()==job.context.snapshot_uuid&&
        Config::instance().is_host==(context.role==a::Role::Host))||fail(e,"Ordered Zone input Session/Binding/role changed");
}
json job_json(const std::string& token,const Job& job){
    return {{"token",token},{"operation",job.arguments.at("phase")},{"ordered",true},
        {"queued",!job.attempted&&job.cause.empty()},{"input_attempted",job.attempted},{"input_returned",job.returned},
        {"session",job.context.session},{"snapshot_uuid",job.context.snapshot_uuid},{"peer_mask",job.context.peer_mask},
        {"local_peer",job.context.local_peer},{"host_peer",job.context.host_peer},
        {"native",job.native},{"capture",zone_action_runtime::ui_receipt(token)},{"cause",job.cause}};
}
bool request(const json& args,json& out,std::string& e){
    const std::set<std::string> keys{"token","snapshot_uuid","expected_world_epoch","expected_generation","region_id","phase","character_type","points"};
    if(!args.is_object()||args.size()!=keys.size())return fail(e,"Ordered Zone input requires exact fields");
    for(const auto& key:keys)if(!args.contains(key))return fail(e,"Ordered Zone input missing field="+key);
    if(!args.at("token").is_string()||!args.at("snapshot_uuid").is_string()||!args.at("phase").is_string()||
       !args.at("expected_world_epoch").is_number_unsigned()||!args.at("expected_generation").is_number_unsigned()||
       !args.at("region_id").is_number_integer()||!args.at("character_type").is_number_integer()||!args.at("points").is_array())
        return fail(e,"Ordered Zone input scalar types differ");
    const auto token=args.at("token").get<std::string>(),snapshot=args.at("snapshot_uuid").get<std::string>(),phase=args.at("phase").get<std::string>();
    const auto region=args.at("region_id").get<int64_t>(),type=args.at("character_type").get<int64_t>();
    const auto& points=args.at("points");
    if(!token_valid(token)||!token_valid(snapshot)||region<0||region>INT32_MAX||type< -1||type>INT32_MAX||
       !args.at("expected_world_epoch").get<uint64_t>()||!args.at("expected_generation").get<uint64_t>()||
       (phase!="prepare_tab"&&phase!="open"&&phase!="select"&&phase!="stroke"&&phase!="hover"&&phase!="receipt")||points.size()>8||
       (phase=="stroke"?(type<0||type>3||points.size()<2):phase=="hover"?(points.size()!=1||type!=-1):!points.empty())||
       (phase=="select"?(type<0||type>3):(phase!="stroke"&&type!=-1)))return fail(e,"Ordered Zone input phase/catalogue/points differ");
    for(const auto& point:points){
        if(!point.is_array()||point.size()!=3)return fail(e,"Ordered Zone input point requires XYZ");
        for(const auto& value:point)if(!value.is_number()||!std::isfinite(value.get<double>())||
            std::abs(value.get<double>())>(std::numeric_limits<float>::max)())return fail(e,"Ordered Zone input point is not finite float32");
    }
    const auto found=jobs.find(token);
    if(phase=="receipt"){
        if(found==jobs.end())return fail(e,"Ordered Zone input token absent");
        const auto& job=found->second;
        if(snapshot!=job.context.snapshot_uuid||args.at("expected_world_epoch")!=job.arguments.at("expected_world_epoch")||
           args.at("expected_generation")!=job.arguments.at("expected_generation")||args.at("region_id")!=job.arguments.at("region_id"))
            return fail(e,"Ordered Zone receipt requested owner differs");
        out=job_json(token,job);e.clear();return true;
    }
    if(found!=jobs.end()){out=job_json(token,found->second);return fail(e,"Ordered Zone input token already reserved - no replay");}
    if(!zone_probe_detail::fault.empty())return fail(e,"Ordered Zone previous Unknown blocks input: "+zone_probe_detail::fault);
    const auto controller=typed_action_runtime::controller_status();bool inside=false;
    if(!Config::instance().local_test||!controller.value("ordered",false)||!controller.value("started",false)||
       controller.value("locked",true)||pending||zone_action_runtime::pending_ui()||
       zone_probe_detail::attempts.contains(token)||jobs.size()>=64||
       !typed_action_runtime::original_go_admission(inside,e))
        return fail(e,e.empty()?"Ordered Zone enqueue requires ready owned Controller":e);
    if(controller.value("pending",true)&&!inside)return fail(e,"Ordered Zone enqueue outside OriginalGo requires idle Controller");
    for(const auto& [id,prior]:jobs){(void)id;if(prior.attempted&&!prior.returned)return fail(e,"Prior ordered Zone input uncertain - no new input");}
    Job job;job.arguments=args;
    if(!a::selected_session().current(job.context,e)||job.context.snapshot_uuid!=snapshot||
       !typed_action_runtime::read_binding(snapshot,job.binding,e)||
       job.binding.native_epoch!=args.at("expected_world_epoch").get<uint64_t>()||
       job.binding.native_generation!=args.at("expected_generation").get<uint64_t>()||!current(job,e))
        return fail(e,e.empty()?"Ordered Zone enqueue Session/Binding differs":e);
    jobs.emplace(token,std::move(job));pending=token;out=job_json(token,jobs.at(token));return true;
}
}
void tick_original_monster_zone_ui(){
    namespace u=zone_ordered_ui;if(!u::pending)return;
    bool inside=false;std::string e;
    if(!typed_action_runtime::original_go_admission(inside,e)||!inside){if(e.empty())return;}
    const auto token=*u::pending;auto& job=u::jobs.at(token);u::pending.reset();
    if(e.empty()&&!u::current(job,e)&&e.empty())e="Ordered Zone queued owner retired";
    if(e.empty()){
        job.attempted=true;
        job.returned=execute_monster_zone_ui(job.arguments,job.native,e);
    }
    job.cause=e;
    if(!job.returned){
        if(job.cause.empty())job.cause="Ordered Zone input failed without native cause";
        LOG_ERROR("[MonsterZoneUI] token={} cause={}; no replay",token,job.cause);
    }
}
bool monster_zone_ui(const json& args,json& result,std::string& error){
    try{
        if(args.is_object()&&args.value("phase",std::string{})=="receipt")return zone_ordered_ui::request(args,result,error);
        if(typed_action_runtime::controller_status().value("ordered",false)&&
           (!args.is_object()||args.value("phase",std::string{})!="state"))return zone_ordered_ui::request(args,result,error);
        return execute_monster_zone_ui(args,result,error);
    }catch(const std::exception& x){error=std::string("Ordered Zone UI request boundary: ")+x.what();}
    catch(...){error="Ordered Zone UI request non-standard boundary exception";}
    LOG_ERROR("[MonsterZoneUI] {}",error);return false;
}
