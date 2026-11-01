namespace world_path_runtime {
json sample_json(const Sample& value,std::string& error){
    json canonical;if(!mixed::canonical(value.mixed,value.mixed,canonical,error))return nullptr;
    return {{"complete_sha256",value.wire.complete_sha256},{"world_effect_sha256",value.wire.world_effect_sha256},
        {"paths",value.wire.paths},{"nodes",value.wire.nodes},{"canonical",std::move(canonical)},{"effects",value.effects}};
}
json status(){
    json out={{"enabled",capture.ready},{"captured",capture.accepted},{"rejected",capture.rejected_count},
        {"pending",pending()},{"last_action_uuid",capture.last_action},{"cause",capture.cause},{"last_operation",nullptr}};
    const Operation* latest=nullptr;
    for(const auto& [id,value]:operations){(void)id;if(!latest||value.request.host_revision>latest->request.host_revision)latest=&value;}
    if(!latest)return out;const auto& o=*latest;
    out["last_terrain_settlement"]=o.terrain_settlement;
    auto ledger=[](const action::Ledger& l){return json{{"cash",l.cash},{"flat_day",l.flat_day},{"record_present",l.record_present},
        {"record_day",l.record_day},{"record_cash",l.record_cash},{"record_income",l.record_income},{"record_expense",l.record_expense},
        {"line_counts",l.line_counts},{"line_income",l.line_income}};};
    std::string error;json before=sample_json(o.before,error),after;
    if(error.empty()&&o.confirmed)after=sample_json(o.after,error);
    out["last_operation"]={{"action_uuid",o.request.action_uuid},{"action",int(o.request.action)},{"snapshot_uuid",o.request.snapshot_uuid},
        {"origin_peer",o.request.peer},{"request_sequence",o.request.sequence},{"observed_revision",o.request.host_revision},
        {"native_epoch",o.binding.native_epoch},{"native_generation",o.binding.native_generation},
        {"attempted",o.attempted},{"confirmed",o.confirmed},{"retired",o.retired},{"ordered",o.ordered},
        {"undo_confirmed",o.evidence.undo_confirmed},{"purchases",o.purchases},{"successful_purchases",o.successful_purchases},
        {"ledger_before",ledger(o.evidence.before)},{"ledger_after",ledger(o.evidence.after)},
        {"before",std::move(before)},{"after",std::move(after)},{"cause",o.cause},{"readback_cause",error}};
    return out;
}
}
namespace {
bool world_path_ui_code(region::Runtime& r,std::string& error){
    struct Pin{uintptr_t rva;size_t size;const char* sha;};
    static constexpr Pin pins[]={
        {0x5c8cc0,1472,"2038e45da0a0db9615c2304d3b2702ee819a44824a8346c11ac27241059cbee2"},
        {0x5eb8c0,1120,"a5e388de2e4176f42afc11929c9221ca180213d53f1d13235ae8fabecd0e64d2"},
        {0xf7890,208,"0705dc08bf04997423d260ef643a0e93185f60da469babda526f84202cd92d71"},
        {0x34f650,16,"1c3e117b185d4cca2b7db5b6ff243df12ad2e86074138664182a59b84b515b73"},
        {0x34f660,32,"588f141024558eedbd712410f251a49c7e9aff772f17988fc378293154aa6d7b"},
        {0x4393b0,864,"5742e4c396c3ff7de0084a74cce2801dbbc8dec79d0fd60353b0dfcdca26850e"},
        {0x7420f0,144,"bca8f855f6cb36226f359db76b15b370f05b5f9da4e674c626da32f2d68323a5"},
    };
    for(const auto& p:pins){std::string bytes(p.size,'\0'),hash;
        if(!r.memory.copy(r.image+p.rva,bytes.data(),bytes.size(),error)||!region::sha256(bytes,hash,error))return false;
        if(hash!=p.sha)return fail(error,"World Path original diagnostic body differs RVA="+std::to_string(p.rva));
    }
    return code(r,true,error);
}
uint32_t world_path_ui_call(uintptr_t image,bool wall,unsigned step,const placement_ui_probe::Binding& b,const action::PathPoint& point,uintptr_t list=0){
    __try{
        if(step==0)reinterpret_cast<void(*)(void*)>(image+0x580870)(reinterpret_cast<void*>(b.button));
        else if(step==1)reinterpret_cast<Press>(image+(wall?0x5eb8c0:0x5c8cc0))(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),point.data());
        else if(step==2)reinterpret_cast<Press>(image+(wall?0x5ec320:0x5c97e0))(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),point.data());
        else if(step==3)reinterpret_cast<void(*)(void*,int32_t)>(image+0x7420f0)(reinterpret_cast<void*>(list),0);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t world_path_wall_type(uintptr_t image,int32_t& value){
    const region::NativeString name{"Wall",4,{}};
    __try{value=reinterpret_cast<int32_t(*)(const void*)>(image+0x4393b0)(&name);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t world_path_edge(uintptr_t image,uintptr_t region,int32_t index,int32_t& count,uintptr_t& neighbor,action::PathPoint& point){
    __try{
        count=reinterpret_cast<int32_t(*)(void*)>(image+0x34f650)(reinterpret_cast<void*>(region));
        if(index<0)return 0;
        if(index>=count)return ERROR_INVALID_PARAMETER;
        neighbor=uintptr_t(reinterpret_cast<void*(*)(void*,int32_t)>(image+0x34f660)(reinterpret_cast<void*>(region),index));
        auto shape=*reinterpret_cast<void**>(region+0x28);
        reinterpret_cast<void*(*)(void*,void*,int32_t,float)>(image+0xf7890)(point.data(),shape,index,.5f);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool world_path_query(const json& args,json& out,std::string& error){
    out=json::object();auto* r=runtime(error);
    if(!r||!path.ready||!args.is_object()||args.size()!=1||!args.contains("id")||!args.at("id").is_number_integer())return fail(error,"World Path query requires native runtime and Region id");
    const auto id=args.at("id").get<int64_t>();if(id<0||id>INT32_MAX)return fail(error,"World Path Region id invalid");
    region::LocalView view;world_path_runtime::Sample sample;
    if(!region::identity(*r,int32_t(id),0,view,error)||!world_path_ui_code(*r,error)||
       !world_path_runtime::world_path_sample(*r,net::current_snapshot_id(),int32_t(id),int32_t(id),sample,error))return false;
    out=world_path_runtime::sample_json(sample,error);if(!error.empty())return false;
    out["pass_borders"]=json::array();int32_t count=0;uintptr_t neighbor=0;action::PathPoint point{};
    auto seh=world_path_edge(r->image,view.region,-1,count,neighbor,point);
    if(seh||count<0||count>4096)return fail(error,"Pass original Region edge count failed SEH="+std::to_string(seh));
    uint8_t owned=0;if(!region::at(*r,view.region,0x850,owned,error))return false;
    for(int32_t index=0;index<count;++index){
        seh=world_path_edge(r->image,view.region,index,count,neighbor,point);
        if(seh)return fail(error,"Pass original border read SEH="+std::to_string(seh));
        if(!neighbor)continue;int32_t other=-1,type=-1;uint8_t other_owned=0;
        if(!region::at(*r,neighbor,0x834,other,error)||!region::at(*r,neighbor,0x830,type,error)||!region::at(*r,neighbor,0x850,other_owned,error))return false;
        region::LocalView exact;if(!region::identity(*r,other,neighbor,exact,error)||!mixed_path_native::finite(point))return fail(error,"Pass original border Region or finite point differs");
        if(type!=0)out["pass_borders"].push_back({{"edge",index},{"region_id",int32_t(id)},{"neighbor_id",other},{"owned",owned==1},{"neighbor_owned",other_owned==1},{"position",point}});
    }
    if(out.dump().size()>200000){out=json::object();return fail(error,"World Path complete query exceeds 200000 bytes");}return true;
}
bool native_world_path(const json& args,json& out,std::string& error){
    out=json::object();auto* r=runtime(error);
    if(!r||!path.ready||!world_path_runtime::capture.ready||unresolved()||placement_ui_diagnostics::unresolved()||
       !world_path_runtime::original_input_admission(*r,error))return fail(error,error.empty()?"World Path UI requires exact idle OriginalGo":error);
    if(!args.is_object()||args.size()!=5||!args.contains("token")||!args.at("token").is_string()||!args.contains("kind")||!args.at("kind").is_string()||
       !args.contains("id")||!args.at("id").is_number_integer()||!args.contains("start")||!args.contains("end"))return fail(error,"World Path UI requires token, kind, id, start and end");
    const auto token=args.at("token").get<std::string>(),kind=args.at("kind").get<std::string>();const auto id=args.at("id").get<int64_t>();
    if(token.empty()||token.size()>128||token.find('\0')!=std::string::npos||path.jobs.contains(token)||id<0||id>INT32_MAX||
       (kind!="wall"&&kind!="pass"))return fail(error,"World Path UI token/kind/Region invalid or already attempted");
    action::PathPoint start{},end{};
    for(auto pair:{std::pair{"start",&start},std::pair{"end",&end}}){const auto& v=args.at(pair.first);
        if(!v.is_array()||v.size()!=3)return fail(error,"World Path endpoint requires three coordinates");
        for(size_t i=0;i<3;++i){if(!v[i].is_number())return fail(error,"World Path coordinate not numeric");const auto x=v[i].get<double>();
            if(!std::isfinite(x)||std::abs(x)>std::numeric_limits<float>::max())return fail(error,"World Path coordinate not finite float32");(*pair.second)[i]=float(x);}}
    const bool wall=kind=="wall";if((wall&&start==end)||(!wall&&start!=end))return fail(error,"Wall requires distinct endpoints; Pass uses one native border point");
    region::LocalView view;if(!region::identity(*r,int32_t(id),0,view,error)||!world_path_ui_code(*r,error))return false;
    if(wall)for(const auto& point:{start,end}){uintptr_t actual=0;const auto seh=terrain::point_region(r->image,view.world.root,point,actual);
        if(seh||actual!=view.region)return fail(error,"Wall endpoint original Region lookup differs SEH="+std::to_string(seh));}
    placement_ui_probe::Context context{r->image,uintptr_t(view.world.origin.state),view.world.origin.world_epoch,view.world.origin.generation,r->thread};
    placement_ui_probe::Request request;request.token=token;request.family=wall?placement_ui_probe::Family::Wall:placement_ui_probe::Family::Pass;request.type=wall?"Wall":"Pass";request.variant.clear();
    if(wall){const auto seh=world_path_wall_type(r->image,request.category);if(seh||request.category<0||request.category>=11)return fail(error,"Wall original type lookup failed SEH="+std::to_string(seh));}
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding binding;
    if(!placement_ui_probe::read_existing_tool(memory,context,request,binding,error))return false;
    const auto action_uuid=region::uuid(error);if(action_uuid.empty())return false;
    auto& job=path.jobs[token];job={{"token",token},{"kind",kind},{"action_uuid",action_uuid},{"attempted",0},{"returned",0},{"captured",false},{"submitted",false},{"confirmed",false},{"cause",""}};
    const auto accepted=world_path_runtime::capture.accepted,rejected=world_path_runtime::capture.rejected_count;
    auto call=[&](unsigned step,const action::PathPoint& point,uintptr_t list=0){job["attempted"]=job.at("attempted").get<unsigned>()+1;
        const auto seh=world_path_ui_call(r->image,wall,step,binding,point,list);if(seh){error="World Path original input SEH="+std::to_string(seh)+" step="+std::to_string(step);return false;}
        job["returned"]=job.at("returned").get<unsigned>()+1;return true;};
    bool ok=call(0,start);uintptr_t active=0;if(ok)ok=region::at(*r,binding.cursor,0x190,active,error)&&active==binding.behaviour;
    if(ok&&wall){uintptr_t window=0,list=0;int32_t models=0;
        ok=region::at(*r,binding.behaviour,0x180,window,error)&&window&&region::at(*r,window,0x4e8,list,error)&&
            native::ap(r->memory,list,r->image,0x16dfc30,error)&&region::at(*r,list,0x350,models,error)&&models>0&&models<=4096;
        if(ok)ok=call(3,start,list);
        if(ok)ok=call(1,start)&&call(2,start)&&call(1,end)&&call(2,end);
        // An unsnapped endpoint needs the original final click at the same point.
        if(ok&&world_path_runtime::capture.accepted==accepted&&world_path_runtime::capture.rejected_count==rejected)ok=call(2,end);
    }else if(ok)ok=call(1,start)&&call(2,start);
    auto& capture=world_path_runtime::capture;
    if(ok)ok=capture.accepted==accepted+1&&capture.pending&&capture.pending->context.has_value();
    if(!ok){if(error.empty())error="World Path original input did not capture once: "+capture.cause;job["cause"]=error;out=job;return false;}
    capture.pending_action=action_uuid;job["captured"]=true;
    job["start_region"]=capture.pending->intent.start_region;job["end_region"]=capture.pending->intent.end_region;
    job["graph_before_sha256"]=capture.pending->intent.graph_before_sha256;job["native_name"]=capture.pending->intent.name;
    out=job;return true;
}
