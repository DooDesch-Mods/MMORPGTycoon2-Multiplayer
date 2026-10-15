// Fixed owned-world original UI probe. Controller adoption is intentionally not
// inferred from this local receipt; the caller must never distribute this world.
namespace zone_probe_detail {
namespace z=monster_zone_read;
namespace q=npc_quest_targets;
struct ReadMemory {
    Memory& memory;size_t remaining=8*1024*1024;
    bool read(uintptr_t p,void* out,size_t size,std::string& e){return copy(p,out,size,e);}
    bool copy(uintptr_t p,void* out,size_t size,std::string& e){if(size>remaining)return fail(e,"zone probe read budget exceeded");remaining-=size;return memory.copy(p,out,size,e);}
};
struct Sample {
    z::State state;native::SimulationState money;undo_probe::UndoWitness undo;
    std::array<int32_t,8> release_edits{};
};
json state_json(const z::State& s){
    json zones=json::array(),monsters=json::array();
    for(const auto& row:s.zones){
        json shape=json::array();for(const auto& path:row.shape){json points=json::array();for(const auto& p:path)points.push_back({p.x,p.z});shape.push_back(std::move(points));}
        const auto& c=row.value.content;
        zones.push_back({{"local_uid",row.value.uid},{"type",c.type},{"density",c.density},{"level",c.level},
            {"monster_type",c.monster_type},{"constructed_day",c.constructed_day},{"sections",c.sections},
            {"shape",std::move(shape)},{"monster_local_uids",row.monsters}});
    }
    for(const auto& row:s.monsters){uint64_t zone=0;for(const auto& v:s.zones)if(v.value.pointer==row.zone)zone=v.value.uid;
        monsters.push_back({{"local_uid",row.uid},{"local_tag",row.tag},{"zone_local_uid",zone},{"status",row.status},
            {"rank",row.rank},{"character_id",{row.slot,row.type}},{"level",row.level},
            {"position",row.position},{"rotation",row.rotation},{"scale",row.scale}});
    }
    return {{"zones",zones},{"monsters",monsters},{"next_monster_tag",s.next_monster_tag},
        {"created_monsters",s.created_monsters},{"global_random",s.global_random},{"region_random",s.region_random},{"local_identity_scope","diagnostic_only"}};
}
json sample_json(const Sample& s){return {{"state",state_json(s.state)},{"ledger",ledger_json(s.money)},
    {"undo",undo_json(s.undo)},{"release_edits",s.release_edits}};}
bool sample(Memory& memory,const q::World& w,Sample& out,std::string& e){
    ReadMemory m{memory};uintptr_t manager=0,release=0;
    return z::read(m,w,out.state,e)&&native::read_simulation_state(out.money,e)&&
        undo_probe::read_undo(memory.process,w.image,out.undo,e)&&
        q::at(m,w.image,0x13df4f0,manager,e)&&q::typed(m,w.image,manager,0x1714ed0,e)&&
        q::at(m,manager,0x38,release,e)&&q::typed(m,w.image,release,0x16c12b0,e)&&
        q::at(m,release,0x120,out.release_edits,e);
}
bool tool(Memory& m,uintptr_t image,uintptr_t& brush,uintptr_t& cursor,std::string& e){
    brush=0;uintptr_t player=0,inventory=0,ap=0;
    npc_census::InventoryHeader header,after;
    if(!field(m,image,0x13df9f0,player,e)||!field(m,player,0,ap,e)||ap!=image+0x17873b0||
       !field(m,player,0x1d0,inventory,e)||!field(m,inventory,0,ap,e)||ap!=image+0x16df0f0||
       !field(m,image,0x13df9d0,cursor,e)||!field(m,cursor,0,ap,e)||ap!=image+0x1786dd0||
       !npc_census::read_inventory_header(m,inventory,header,e))return fail(e,e.empty()?"zone probe Player/inventory/cursor differs":e);
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<header.count;++i){
        uintptr_t item=0,type=0,weak=0,target=0,owned=0,value=0;
        if(!field(m,header.items,size_t(i)*8,item,e))return false;if(!item)continue;
        if(!unique.insert(item).second||!field(m,item,0,ap,e)||ap!=image+0x17437f0||
           !field(m,item,0x68,type,e)||!field(m,item,0x70,weak,e)||!field(m,weak,0,target,e)||
           !field(m,type,0x40,owned,e)||target!=type+0x38||owned!=weak||!field(m,type,0,ap,e))
            return fail(e,e.empty()?"zone probe inventory weak owner differs":e);
        if(ap!=image+0x173b7b0)continue;
        if(!field(m,type,0x160,value,e)||!field(m,value,0,ap,e))return false;
        if(ap!=image+0x172fa80)continue;
        int32_t kind=-1;if(!field(m,value,0x178,kind,e))return false;if(kind!=1)continue;
        if(brush&&brush!=value)return fail(e,"zone probe MonsterZone tool ambiguous");brush=value;
    }
    return npc_census::read_inventory_header(m,inventory,after,e)&&
        ((brush&&after==header)||fail(e,"zone probe MonsterZone tool absent or inventory changed"));
}
bool options(Memory& m,uintptr_t image,uintptr_t brush,uintptr_t& window,std::vector<Option>& available,json& selected,std::string& e){
    available.clear();selected=nullptr;uintptr_t ap=0,list=0,data=0,current=0;int32_t count=0;
    if(!field(m,brush,0x5d8,window,e)||!field(m,window,0,ap,e)||ap!=image+0x17321a0||
       !field(m,window,0x4e8,list,e)||!field(m,list,0,ap,e)||ap!=image+0x172b1d0||
       !field(m,list,0x348,data,e)||!field(m,list,0x350,count,e))return fail(e,e.empty()?"zone probe ThemeWindow/List differs":e);
    if(count<0||count>256||(count&&!data))return fail(e,"zone probe ThemeList count differs");
    std::set<std::array<int32_t,2>> ids;
    for(int32_t i=0;i<count;++i){uintptr_t entry=0;Option o;
        if(!field(m,data,size_t(i)*8,entry,e)||!field(m,entry,0,ap,e)||ap!=image+0x1738b10||
           !field(m,entry,0x338,o.character,e))return fail(e,e.empty()?"zone probe ThemeSelector differs":e);
        if(!o.character)continue;
        if(!field(m,o.character,0,ap,e)||ap!=image+0x1705990||!field(m,o.character,0x2c,o.id,e))return fail(e,e.empty()?"zone probe CharacterType differs":e);
        if(o.id[0]!=1||o.id[1]<0||!ids.insert(o.id).second)return fail(e,"zone probe Monster catalogue ID invalid/duplicate");
        available.push_back(o);
    }
    const auto code=original_get_selection(image,window,current);
    if(code)return fail(e,"zone probe original GetSelection exception="+std::to_string(code));
    if(current){const auto i=std::find_if(available.begin(),available.end(),[&](const Option& o){return o.character==current;});
        if(i==available.end())return fail(e,"zone probe selected CharacterType not catalogue-owned");selected=i->id;}
    return true;
}
uint32_t input(uintptr_t image,unsigned step,uintptr_t brush,uintptr_t cursor,const std::array<float,3>& point)noexcept{
    __try{
        if(step==2)reinterpret_cast<void(*)(void*,void*,const float*,float)>(image+0x5ef800)(reinterpret_cast<void*>(brush),reinterpret_cast<void*>(cursor),point.data(),0.f);
        else reinterpret_cast<void(*)(void*,void*,const float*)>(image+(step==0?0x5f00b0:step==1?0x5ef6a0:0x5eea20))(reinterpret_cast<void*>(brush),reinterpret_cast<void*>(cursor),point.data());
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct TabAttempt {host::opened::CurrentWorld origin;uintptr_t root=0;monster_zone_tab::Pin pin;};
std::optional<TabAttempt> tab_attempt;
uint32_t press_tab(uintptr_t image,const monster_zone_tab::Pin& p)noexcept{
    const float position[2]{};
    __try{reinterpret_cast<void(*)(void*,void*,const void*)>(image+0x57c950)(reinterpret_cast<void*>(p.tab),reinterpret_cast<void*>(p.cursor),position);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::map<std::string,json> attempts;
std::string fault;
}
bool monster_zone_probe_impl(const json& args,json& result,std::string& error){
    namespace d=zone_probe_detail;result={{"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false}};
    const std::set<std::string> keys{"token","snapshot_uuid","expected_world_epoch","expected_generation","region_id","phase","character_type","points"};
    if(!args.is_object()||args.size()!=keys.size())return fail(error,"zone probe requires exact owned request fields");
    for(const auto& key:keys)if(!args.contains(key))return fail(error,"zone probe request field missing: "+key);
    if(!args.at("token").is_string()||!args.at("snapshot_uuid").is_string()||!args.at("phase").is_string()||
       !args.at("expected_world_epoch").is_number_unsigned()||!args.at("expected_generation").is_number_unsigned()||
       !args.at("region_id").is_number_integer()||!args.at("character_type").is_number_integer()||!args.at("points").is_array())return fail(error,"zone probe request scalar types differ");
    const auto token=args.at("token").get<std::string>(),phase=args.at("phase").get<std::string>();
    const auto region_id=args.at("region_id").get<int64_t>(),type=args.at("character_type").get<int64_t>();
    if(!token_valid(token)||!token_valid(args.at("snapshot_uuid").get<std::string>())||region_id<0||region_id>INT32_MAX||type< -1||type>INT32_MAX||
       (phase!="open"&&phase!="select"&&phase!="state"&&phase!="stroke"&&phase!="prepare_tab")||args.at("points").size()>8)return fail(error,"zone probe request values differ");
    std::vector<std::array<float,3>> points;
    for(const auto& p:args.at("points")){if(!p.is_array()||p.size()!=3)return fail(error,"zone probe point requires XYZ");std::array<float,3> value{};
        for(size_t n=0;n<3;++n){if(!p[n].is_number())return fail(error,"zone probe point is not numeric");const auto v=p[n].get<double>();if(!std::isfinite(v)||std::abs(v)>(std::numeric_limits<float>::max)())return fail(error,"zone probe point is not finite float32");value[n]=float(v);}points.push_back(value);}
    if((phase=="stroke"?(points.size()<2||type<0):!points.empty())||((phase=="open"||phase=="state"||phase=="prepare_tab")&&type!=-1)||(phase=="select"&&type<0))return fail(error,"zone probe phase payload differs");
    if(d::attempts.contains(token)){result=d::attempts.at(token);return fail(error,"zone probe token already attempted - no replay");}
    if(phase!="state"&&!d::fault.empty())return fail(error,"zone probe previous Unknown blocks new input: "+d::fault);
    if(d::attempts.size()>=64)return fail(error,"zone probe token limit reached");
    const auto& config=Config::instance();const auto controller=typed_action_runtime::status();
    if(!config.local_test||!config.is_host||!controller.contains("initialized")||controller.at("initialized")!=false||net::session_wire::peers()>1)
        return fail(error,"zone probe requires isolated local-test Host with uninitialized Controller");
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error)||sha!=exe_sha||!thread||thread!=GetCurrentThreadId())return fail(error,"zone probe exact EXE/Mainthread differs: "+error);
    Memory memory;host::LookupApi api;if(!host::resolve_lookup(memory.process,image,sha,api,error)||!test_world(memory,image,error)||!monster_zone_guards::verify(memory,image,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,image,thread,api);host::WorldState before_world,after_world;
    if(!reader.current(before_world,true,error))return false;
    if(before_world.origin.uuid!=args.at("snapshot_uuid").get<std::string>()||before_world.origin.world_epoch!=args.at("expected_world_epoch").get<uint64_t>()||before_world.origin.generation!=args.at("expected_generation").get<uint64_t>())return fail(error,"zone probe full local Origin differs");
    uintptr_t region=0,brush=0,cursor=0,window=0,chosen=0;
    const auto field_read=[&](uintptr_t p,size_t off,auto& v,std::string& e){return field(memory,p,off,v,e);};
    if(!region_saved_identity::resolve(image,uintptr_t(before_world.origin.map),before_world.root,int32_t(region_id),0,region,field_read,error)||!d::tool(memory,image,brush,cursor,error))return false;
    uint8_t owned=0;if(!field(memory,region,0x850,owned,error)||owned!=1)return fail(error,error.empty()?"zone probe region is not owned":error);
    d::q::World world;world.image=image;world.map=uintptr_t(before_world.origin.map);world.root=before_world.root;world.region=region;world.saved_region=int32_t(region_id);
    d::Sample before,after;if(!d::sample(memory,world,before,error))return false;
    if(before.undo.active_graph||(!before.money.clock.paused&&before.money.clock.time_scale!=0))return fail(error,"zone probe requires idle Undo and paused actual Clock");
    result["token"]=token;result["operation"]=phase;result["before"]=d::sample_json(before);result["calls"]=json::array();
    const auto observe_tab=[&](monster_zone_tab::Pin& pin,monster_zone_tab::Status& status,std::string& e){
        d::ReadMemory m{memory};
        if(!monster_zone_tab::inspect(m,image,uintptr_t(before_world.origin.world),pin,e)||
           !monster_zone_tab::status(m,pin,status,e))return false;
        if(d::tab_attempt&&(!host::same_world(d::tab_attempt->origin,before_world.origin)||
           d::tab_attempt->root!=before_world.root||!monster_zone_tab::same_owner(d::tab_attempt->pin,pin)))return fail(e,"zone NPCs tab retained owner changed");
        return true;
    };
    const auto publish_tab=[&](const monster_zone_tab::Pin& pin,const monster_zone_tab::Status& status){
        result["tab_requested"]=d::tab_attempt.has_value();result["tab_ready"]=status.ready;
        result["tab_pending"]=!status.ready;result["tab_cause"]=status.cause;
        result["tab_active"]=pin.active==1;result["view_mode"]=pin.view;result["required_view_mode"]=pin.edit;
    };
    if(phase=="prepare_tab"){
        if(d::tab_attempt)return fail(error,"zone NPCs tab already attempted - read state; no replay");
        d::ReadMemory m{memory};if(!monster_zone_tab::verify(m,image,error))return false;
        int32_t painting=0;uintptr_t graph=0;
        if(!field(memory,brush,0x6fc,painting,error)||painting||!field(memory,brush,0x740,graph,error)||graph)return fail(error,error.empty()?"zone NPCs tab requires idle brush":error);
        struct Calls {
            Memory& memory;host::Reader& reader;const host::WorldState& before_world;
            uintptr_t image,brush,cursor;const d::q::World& world;const d::Sample& before;
            const std::string& token;json& result;
            bool pin(monster_zone_tab::Pin& p,std::string& e){
                host::WorldState current;uintptr_t actual_brush=0,actual_cursor=0;d::ReadMemory m{memory};
                if(!reader.current(current,true,e)||!host::same_world(current.origin,before_world.origin)||current.root!=before_world.root||
                   !d::tool(memory,image,actual_brush,actual_cursor,e)||actual_brush!=brush||actual_cursor!=cursor)return fail(e,e.empty()?"zone NPCs tab full Origin/tool changed":e);
                return monster_zone_tab::inspect(m,image,uintptr_t(current.origin.world),p,e);
            }
            bool reserve(const monster_zone_tab::Pin& p,std::string&){
                d::tab_attempt=d::TabAttempt{before_world.origin,before_world.root,p};
                result["attempted"]=true;d::attempts.emplace(token,result);return true;
            }
            bool press(const monster_zone_tab::Pin& p,std::string& e){
                result["calls"].push_back({{"name","ActionBarTab::Press"},{"attempted",true},{"returned",false},{"exception",0}});
                d::attempts.at(token)=result;const auto code=d::press_tab(image,p);
                result["calls"].back()["exception"]=code;result["calls"].back()["returned"]=code==0;d::attempts.at(token)=result;
                return !code||fail(e,"zone NPCs original Press exception="+std::to_string(code));
            }
            bool status(const monster_zone_tab::Pin& p,monster_zone_tab::Status& out,std::string& e){d::ReadMemory m{memory};return monster_zone_tab::status(m,p,out,e);}
            bool unchanged(std::string& e){d::Sample after;if(!d::sample(memory,world,after,e))return false;
                result["after"]=d::sample_json(after);return d::sample_json(before)==d::sample_json(after)||fail(e,"zone NPCs tab changed world/ledger/RNG/Undo/Release");}
        } calls{memory,reader,before_world,image,brush,cursor,world,before,token,result};
        monster_zone_tab::Receipt receipt;const bool ok=monster_zone_tab::prepare(calls,receipt,error);
        result["attempted"]=receipt.attempted;result["returned"]=receipt.returned;result["confirmed"]=receipt.confirmed;result["unknown"]=receipt.unknown;
        result["tab_requested"]=receipt.returned;result["tab_ready"]=receipt.state.ready;result["tab_pending"]=receipt.confirmed&&!receipt.state.ready;result["tab_cause"]=receipt.state.cause;
        result["host_ordered_action"]=false;result["cause"]=error;
        if(receipt.unknown&&d::fault.empty())d::fault=error;
        if(receipt.attempted)d::attempts.at(token)=result;
        return ok;
    }
    placement_ui_probe::Binding binding;std::vector<Option> available;json selected;
    if(phase=="open"){
        placement_ui_probe::Request request;request.family=placement_ui_probe::Family::MonsterZone;request.type="MonsterZone";
        placement_ui_probe::Context context{image,uintptr_t(before_world.origin.state),before_world.origin.world_epoch,before_world.origin.generation,thread};placement_ui_probe::ProcessMemory ui_memory;
        if(!placement_ui_probe::read_existing_tool(ui_memory,context,request,binding,error)||binding.behaviour!=brush)return fail(error,error.empty()?"zone probe original ItemButton binding differs":error);
    }else{
        uintptr_t active=0;if(!field(memory,cursor,0x190,active,error))return false;
        result["tool_active"]=active==brush;
        if(!monster_zone_tab::read_active_options(phase=="state",active,brush,[&](std::string& e){return d::options(memory,image,brush,window,available,selected,e);},error))return false;
        if(type>=0){const auto it=std::find_if(available.begin(),available.end(),[&](const Option& o){return o.id==std::array<int32_t,2>{1,int32_t(type)};});if(it==available.end())return fail(error,"zone probe requested Monster absent from catalogue");chosen=it->character;}
    }
    if(phase=="state"){
        result["read_only"]=true;result["selected"]=selected;result["options"]=json::array();for(const auto& o:available)result["options"].push_back(o.id);
        uintptr_t actual_region=0;int32_t painting=0;if(!field(memory,brush,0x170,actual_region,error)||!field(memory,brush,0x6fc,painting,error))return false;
        result["brush_region_matches"]=actual_region==region;result["painting_mode"]=painting;
        monster_zone_tab::Pin tab;monster_zone_tab::Status tab_status;
        if(!observe_tab(tab,tab_status,error))return false;publish_tab(tab,tab_status);
        if(!d::sample(memory,world,after,error)||!reader.current(after_world,true,error)||
           !host::same_world(before_world.origin,after_world.origin)||before_world.root!=after_world.root||
           d::sample_json(before)!=d::sample_json(after))return fail(error,error.empty()?"zone probe read-only state changed during readback":error);
        result["confirmed"]=true;return true;
    }
    if(phase=="stroke"){
        uintptr_t actual_region=0,graph=0;int32_t painting=0;uint8_t visible=0;
        if(selected!=json(std::array<int32_t,2>{1,int32_t(type)})||!field(memory,brush,0x170,actual_region,error)||actual_region!=region||
           !field(memory,brush,0x6fc,painting,error)||painting||!field(memory,brush,0x740,graph,error)||graph||
           !field(memory,window,0x45f,visible,error)||visible!=1)return fail(error,error.empty()?"zone probe brush region/selection/idle/visible differs":error);
        uintptr_t collision=0;int32_t collision_kind=0;
        if(!field(memory,cursor,0x230,collision,error)||!field(memory,collision,0x1ec,collision_kind,error))return false;
        result["collision_is_zone"]=collision_kind==0x40000;
        if(collision_kind==0x40000)return fail(error,"zone probe create stroke requires no existing Zone collision");
        for(const auto& p:points){uintptr_t actual=0;const auto seh=shard_region(image,world.root,p,actual);if(seh||actual!=region)return fail(error,"zone probe point outside current region exception="+std::to_string(seh));}
    }
    d::attempts.emplace(token,result);auto unknown=[&](const std::string& cause){error=cause;if(d::fault.empty())d::fault=cause;result["unknown"]=true;result["cause"]=cause;d::attempts.at(token)=result;return false;};
    result["attempted"]=true;d::attempts.at(token)=result;
    auto call=[&](const char* name,auto callback){
        uintptr_t current_brush=0,current_cursor=0;
        if(!reader.current(after_world,false,error)||!host::same_world(before_world.origin,after_world.origin)||
           before_world.root!=after_world.root||!d::tool(memory,image,current_brush,current_cursor,error)||
           current_brush!=brush||current_cursor!=cursor)return unknown(error.empty()?"zone probe owner changed before next original callback":error);
        json entry{{"name",name},{"attempted",true},{"returned",false},{"exception",0}};result["calls"].push_back(entry);d::attempts.at(token)=result;const auto code=callback();result["calls"].back()["exception"]=code;result["calls"].back()["returned"]=code==0;d::attempts.at(token)=result;if(code)return unknown(std::string("zone probe original ")+name+" exception="+std::to_string(code));return true;};
    if(phase=="open"){if(!call("ItemButton::Press",[&]{return original_press(image,binding.button);}))return false;}
    else if(phase=="select"){if(!call("ThemeWindow::SetSelection",[&]{return original_select(image,window,chosen);}))return false;}
    else{
        const auto input=[&](unsigned step,const std::array<float,3>& point,std::string&){
            constexpr const char* names[]{"MouseOver","Press_Left","Drag_Left","Release_Left"};
            return call(names[step],[&]{return d::input(image,step,brush,cursor,point);});
        };
        const auto observe=[&](std::string& cause){d::ReadMemory m{memory};d::z::State stage;
            if(!d::z::read(m,world,stage,cause))return unknown(cause);
            result["after_press"]=d::state_json(stage);return true;};
        if(!monster_zone_stroke::run(points,input,observe,error))return false;
    }
    result["returned"]=true;
    if(!d::sample(memory,world,after,error)||!reader.current(after_world,true,error)||!host::same_world(before_world.origin,after_world.origin)||before_world.root!=after_world.root)return unknown(error.empty()?"zone probe final native World differs":error);
    result["after"]=d::sample_json(after);
    if(!d::options(memory,image,brush,window,available,selected,error))return unknown(error);
    result["selected"]=selected;result["options"]=json::array();for(const auto& o:available)result["options"].push_back(o.id);
    if(phase!="stroke"&&d::sample_json(before)!=d::sample_json(after))return unknown("zone probe UI selection changed world/ledger/RNG/Undo/Release");
    if(phase=="select"&&selected!=json(std::array<int32_t,2>{1,int32_t(type)}))return unknown("zone probe original selection readback differs");
    if(phase=="stroke"){
        if(after.state.zones.size()!=before.state.zones.size()+1||after.undo.active_graph||after.undo.undo.size()!=before.undo.undo.size()+1)return unknown("zone probe stroke did not create one Zone and one closed original Undo graph");
        for(size_t i=0;i<before.state.zones.size();++i)if(before.state.zones[i]!=after.state.zones[i])return unknown("zone probe stroke changed an existing Zone");
        const auto& made=after.state.zones.back();if(made.value.content.monster_type!=type)return unknown("zone probe created Monster type differs");
        result["created_zone_local_uid"]=made.value.uid;
    }
    result["confirmed"]=true;result["host_ordered_action"]=false;d::attempts.at(token)=result;error.clear();return true;
}

bool monster_zone_probe(const json& args,json& result,std::string& error){
    try{return monster_zone_probe_impl(args,result,error);}
    catch(const std::exception& exception){error=std::string("zone probe C++ boundary exception: ")+exception.what();}
    catch(...){error="zone probe non-standard boundary exception";}
    const bool attempted=result.is_object()&&result.value("attempted",false);
    if(attempted){if(zone_probe_detail::fault.empty())zone_probe_detail::fault=error;result["unknown"]=true;result["confirmed"]=false;result["cause"]=error;
        if(args.is_object()&&args.contains("token")&&args.at("token").is_string()){
            const auto token=args.at("token").get<std::string>();
            if(zone_probe_detail::attempts.contains(token))zone_probe_detail::attempts.at(token)=result;
        }
    }
    LOG_ERROR("[MonsterZoneProbe] cause={} attempted={}; no replay",error,attempted);return false;
}
