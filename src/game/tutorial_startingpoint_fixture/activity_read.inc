bool activity_loc(uintptr_t value,json& out,std::string& e,unsigned depth=0){
    if(depth>2)return fail(e,"Staff native LocString nesting exceeds2");
    std::string key;uintptr_t first=0,last=0,end=0;
    if(!text(value,0,key,e)||!at(value,0x20,first,e)||!at(value,0x28,last,e)||!at(value,0x30,end,e))return false;
    if((!first&&(last||end))||last<first||end<last||(last-first)%0x68||(end-first)%0x68||
       (last-first)/0x68>4||(end-first)/0x68>8)return fail(e,"Staff native LocString argument bounds invalid");
    out={{"key",key},{"args",json::array()}};std::set<std::string> names;
    for(uintptr_t p=first;p<last;p+=0x68){std::string name;int32_t kind=0;
        if(!text(p,0,name,e)||!at(p,0x60,kind,e))return false;
        if(name.empty()||!names.insert(name).second)return fail(e,"Staff native LocString argument name absent/duplicated");
        json item={{"name",name},{"type",kind}};
        if(kind==0){json nested;if(!activity_loc(p+0x20,nested,e,depth+1))return false;item["value"]=nested;}
        else if(kind==1){int32_t number=0;if(!at(p,0x58,number,e))return false;item["value"]=number;}
        else if(kind==2){float number=0;int32_t places=0;if(!at(p,0x5c,number,e)||!at(p,0x58,places,e))return false;
            if(!std::isfinite(number))return fail(e,"Staff native LocString float is not finite");item["value"]=number;item["places"]=places;}
        else return fail(e,"Staff native LocString argument type outside0..2");
        out["args"].push_back(std::move(item));
    }return true;
}
struct ActivityDeveloper {uintptr_t address=0;int32_t id=0,job=0;uint8_t employed=0,on_map=0;bool operator==(const ActivityDeveloper&)const=default;};
bool activity_developers(const tutorial_development::StartingpointSnapshot& s,std::vector<ActivityDeveloper>& out,std::string& e){
    if(!s.creation||s.creation->developers.empty()||s.creation->developers.size()>50)return fail(e,"Staff activity requires bounded owned Developer census1..50");
    std::set<int32_t> ids;
    for(const auto& dev:s.creation->developers){ActivityDeveloper d;d.address=dev.address;d.job=dev.job;
        if(!type(d.address,s.image+0x17422e0,e)||!at(d.address,0x4c0,d.id,e)||!at(d.address,0x4d9,d.employed,e)||!at(d.address,0x368,d.on_map,e))return false;
        if(d.id<0||d.id>=50||!ids.insert(d.id).second||d.employed>1||d.on_map>1)return fail(e,"Staff activity Developer identity/flags invalid");
        if(d.employed&&(d.job!=0&&d.job!=5&&d.job!=6&&d.job!=7))return fail(e,"Staff activity probe supports only actual idle/returning/developing/debugging Developer");
        out.push_back(d);
    }return true;
}
bool activity_one(uintptr_t image,uintptr_t dev,json& value,std::string& e){
    NativeLocStorage result;
    if(!activity_error(activity_get(image,result.bytes,dev),"GetActivity",e))return false;
    bool read=false;
    try{read=activity_loc(reinterpret_cast<uintptr_t>(result.bytes),value,e);}
    catch(const std::exception& x){e=std::string("Staff activity read exception: ")+x.what();}
    std::string disposal;if(!activity_release(image,result,disposal)){e+=(e.empty()?"":"; ")+disposal;return false;}
    return read;
}
struct ActivityManagers {std::array<uintptr_t,2> owner{},data{};std::array<int32_t,2> count{},capacity{};int32_t active_tech=0,active_devs=0;bool operator==(const ActivityManagers&)const=default;};
bool activity_managers(const tutorial_development::StartingpointSnapshot& s,const std::vector<ActivityDeveloper>& developers,ActivityManagers& out,std::string& e){
    int32_t expected_tech=0;
    for(size_t side=0;side<2;++side){
        if(!at(s.lounge,0x98+side*8,out.owner[side],e)||!type(out.owner[side],s.image+0x16e47f0,e)||
           !at(out.owner[side],0x38,out.data[side],e)||!at(out.owner[side],0x40,out.count[side],e)||!at(out.owner[side],0x44,out.capacity[side],e))return false;
        if(out.count[side]<0||out.count[side]>50||out.capacity[side]<out.count[side]||out.capacity[side]>64||!out.data[side])
            return fail(e,"Staff native manager bounds invalid");
        std::set<uintptr_t> expected,actual;
        for(const auto& d:developers)if(d.on_map==side){expected.insert(d.address);if(side==0&&d.employed&&d.job>=5&&d.job<=7)++expected_tech;}
        for(int32_t i=0;i<out.count[side];++i){uintptr_t item=0;if(!at(out.data[side],size_t(i)*8,item,e))return false;
            if(!item||!actual.insert(item).second)return fail(e,"Staff native manager contains null/duplicate Developer");}
        if(actual!=expected)return fail(e,"Staff native manager membership differs from owned Developer census");
    }
    if(out.owner[0]==out.owner[1])return fail(e,"Staff native managers alias");
    if(!activity_error(activity_count(s.image,0x25ed30,s.lounge,out.active_tech),"GetActiveTechWorkerCount",e)||
       !activity_error(activity_count(s.image,0x25ead0,s.lounge,out.active_devs),"GetActiveDevCount",e))return false;
    return (out.active_tech==expected_tech&&out.active_devs==expected_tech+out.count[1])||fail(e,"Staff original active counts differ from owned manager census");
}
struct ActivityFormatCalls {
    uintptr_t image=0,project=0;NativeLocStorage name_value,format_value;NativeArgStorage arg_value;
    bool name(std::string& e){return activity_error(activity_name(image,name_value.bytes,project),"GetDisplayName",e);}
    bool read_name(json& out,std::string& e){return activity_loc(reinterpret_cast<uintptr_t>(name_value.bytes),out,e);}
    bool arg(std::string& e){const BorrowedString key{"techname",8,{}};
        return activity_error(activity_arg(image,arg_value.bytes,&key,name_value.bytes),"LocArg construction",e);}
    bool format(const char* key,std::string& e){return activity_error(activity_format(image,format_value.bytes,key,arg_value.bytes),"LocFormat",e);}
    bool read_format(json& out,std::string& e){return activity_loc(reinterpret_cast<uintptr_t>(format_value.bytes),out,e);}
    bool release_format(std::string& e){return activity_release(image,format_value,e);}
    bool release_arg(std::string& e){return activity_error(activity_destroy(image,0xc6c9b0,arg_value.bytes),"LocArg destruction",e);}
    bool release_name(std::string& e){return activity_release(image,name_value,e);}
};
bool activity_sample(const Request& request,json& result,std::string& e){
    const bool client=!Config::instance().is_host;
    Pin p;tutorial_development::StartingpointSnapshot snapshot;json diagnostic;
    if(!pin(request,p,e,client)||!begin_snapshot(p,snapshot,diagnostic,e))return false;
    if(!snapshot.project||!snapshot.row||!snapshot.row->project_present||!snapshot.row->purchased||snapshot.row->deployed||
       p.development!=request.expected_development||diagnostic!=p.development)return fail(e,"Staff activity requires exact purchased current Startingpoint project");
    const auto status=typed_action_runtime::status();if(!status.value("initialized",false))return fail(e,"Staff activity requires initialized authoritative Controller");
    action_session::Context context,current;action_session::Binding binding;
    if(!action_session::selected_session().current(context,e)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e)||!activity_code(p.image,e))return false;
    live_host_state::NativePort economy(context,binding,nullptr);live_host_state::Pin ledger_before,ledger_after;
    if(!economy.read(ledger_before,e))return false;
    std::vector<ActivityDeveloper> developers;if(!activity_developers(snapshot,developers,e))return false;
    ActivityManagers managers;if(!activity_managers(snapshot,developers,managers,e))return false;
    json rows=json::array(),formatted;
    // These calls construct private native display values only. No UI setter,
    // worker tick, simulation update or network mutation is invoked.
    for(const auto& d:developers)if(d.employed){json activity;if(!activity_one(p.image,d.address,activity,e))return false;
        rows.push_back({{"id",d.id},{"job",d.job},{"on_map",d.on_map!=0},{"activity",activity}});}
    ActivityFormatCalls calls{p.image,p.project};if(!activity_probe::format(calls,formatted,e))return false;
    Pin after;tutorial_development::StartingpointSnapshot after_snapshot;json after_diagnostic;std::vector<ActivityDeveloper> after_developers;
    if(!pin(request,after,e,client)||!begin_snapshot(after,after_snapshot,after_diagnostic,e)||!activity_developers(after_snapshot,after_developers,e)||!economy.read(ledger_after,e))return false;
    ActivityManagers after_managers;if(!activity_managers(after_snapshot,after_developers,after_managers,e))return false;
    if(!same_research_native(p,after,snapshot,after_snapshot)||developers!=after_developers||managers!=after_managers||ledger_before!=ledger_after)
        return fail(e,"Staff activity native owners/project/workers/full ledger changed during read");
    if(!action_session::selected_session().current(current,e)||current!=context)return fail(e,e.empty()?"Staff activity common Context changed":e);
    result={{"operation",request.operation},{"token",request.token},{"attempted",false},{"returned",true},{"confirmed",true},{"unknown",false},
        {"read_only",true},{"formatter_confirmed",true},{"native_project_unchanged",true},{"ledger_unchanged",true},
        {"active_tech_workers",managers.active_tech},{"active_developers",managers.active_devs},
        {"developers",rows},{"formatted",formatted},{"development",after.development}};
    return true;
}

bool staff_activity_state(const Request& request,json& result,std::string& e){
    result={{"operation",request.operation},{"token",request.token},{"read_only",true},
        {"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false}};
    if(activity_faulted){result["unknown"]=true;result["retry_blocked"]=true;
        return fail(e,activity_fault_cause+"; Staff activity probe requires process restart after native temporary-object failure");}
    activity_calls={};bool ok=false;
    try{ok=activity_sample(request,result,e);}
    catch(const std::exception& x){e=std::string("Staff activity probe exception: ")+x.what();}
    json native_calls=json::object();unsigned index=0;
    for(const char* name:{"getter","formatter","destructor"}){
        native_calls[name]={{"attempted",activity_calls.attempted[index]},{"returned",activity_calls.returned[index]},
            {"confirmed",ok&&activity_calls.attempted[index]==activity_calls.returned[index]}};++index;}
    result["native_calls"]=native_calls;result["native_exception"]=activity_calls.exception;
    result["returned"]=ok;result["confirmed"]=ok;result["unknown"]=activity_calls.unknown;
    if(!ok)result["cause"]=e;
    if(activity_calls.unknown){activity_faulted=true;activity_fault_cause=e;result["retry_blocked"]=true;}
    return ok;
}
