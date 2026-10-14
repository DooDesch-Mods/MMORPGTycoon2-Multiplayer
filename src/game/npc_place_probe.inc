#include <algorithm>
#include <cmath>
#include <limits>

namespace {
namespace undo_probe=build_actions::private_native;

json ledger_json(const native::SimulationState& value) {
    return {{"cash",value.cash},{"record_present",value.record_present},
        {"record_day",value.record_day},{"record_cash",value.record_cash},
        {"record_income",value.record_income},{"record_expense",value.record_expense},
        {"logged_in_subscribers",value.logged_in_subscribers},
        {"line_counts",value.line_counts},{"line_income",value.line_income},
        {"clock",{{"flat_day",value.clock.flat_day},{"seconds",value.clock.seconds},
                  {"time_scale",value.clock.time_scale},{"pause_scale",value.clock.pause_scale},
                  {"paused",value.clock.paused}}}};
}

json census_json(const npc_census::State& value) {
    json rows=json::array();
    for(const auto& row:value.rows)
        rows.push_back({{"local_uid",row.local_uid},{"type",row.type},
            {"local_tag",row.local_tag},{"character_id",{row.character_slot,row.character_type}},
            {"name",row.name},{"level",row.level},{"quest_count",row.quest_count},
            {"position",row.position},{"rotation",row.rotation},{"scale",row.scale}});
    return {{"count",value.rows.size()},{"next_local_tag",value.next_local_tag},
        {"global_random",value.global_random},{"rows",std::move(rows)},
        {"local_uid_scope","diagnostic_only"}};
}

json undo_json(const undo_probe::UndoWitness& value) {
    return {{"undo_count",value.undo.size()},{"redo_count",value.redo.size()},
        {"active_graph",value.active_graph!=0}};
}

bool same_ledger(const native::SimulationState& a,const native::SimulationState& b) {
    return ledger_json(a)==ledger_json(b);
}

bool test_world(Memory& memory,uintptr_t image,std::string& error) {
    uintptr_t game=0,data=0;size_t length=0;
    if(!field(memory,image,0x13df8f0,game,error)||
       !field(memory,game,0x470,data,error)||
       !field(memory,game,0x478,length,error))return false;
    if(!data||length<6||length>64||data>UINTPTR_MAX-length)
        return fail(error,"NPC original placement probe requires actual MPTest world folder");
    std::string folder(length,'\0');
    if(!memory.copy(data,folder.data(),length,error))return false;
    if(!folder.starts_with("MPTest")||
       !std::all_of(folder.begin(),folder.end(),[](unsigned char c){
           return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                  (c>='0'&&c<='9')||c=='_'||c=='-';
       }))return fail(error,"NPC original placement probe requires actual MPTest world folder");
    return true;
}

uint32_t map_region(uintptr_t image,uintptr_t map,const std::array<float,3>& point,
                    uintptr_t& result) noexcept {
    __try {result=uintptr_t(reinterpret_cast<void*(__fastcall*)(void*,const float*,void*)>(
        image+0x32a990)(reinterpret_cast<void*>(map),point.data(),nullptr));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t shard_region(uintptr_t image,uintptr_t shard,const std::array<float,3>& point,
                      uintptr_t& result) noexcept {
    __try {result=uintptr_t(reinterpret_cast<void*(__fastcall*)(void*,const float*,void*)>(
        image+0x5089b0)(reinterpret_cast<void*>(shard),point.data(),nullptr));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t npc_input(uintptr_t image,uintptr_t behaviour,uintptr_t cursor,
                   const std::array<float,3>& point,bool release) noexcept {
    __try {
        reinterpret_cast<void(__fastcall*)(void*,void*,const float*)>(
            image+(release?0x5c3ef0:0x5c4210))(
                reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(cursor),point.data());
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

bool point_and_cursor(Memory& memory,uintptr_t image,uintptr_t map,uintptr_t shard,
                      uintptr_t region,uintptr_t cursor,uintptr_t behaviour,
                      const std::array<float,3>& point,std::string& error) {
    uintptr_t current=0,ap=0,from_map=0,from_shard=0;
    const auto map_seh=map_region(image,map,point,from_map);
    const auto shard_seh=shard_region(image,shard,point,from_shard);
    if(map_seh||shard_seh||from_map!=region||from_shard!=region)
        return fail(error,"NPC original point region differs map_seh="+
            std::to_string(map_seh)+" shard_seh="+std::to_string(shard_seh));
    if(!field(memory,cursor,0,ap,error)||ap!=image+0x1786dd0||
       !field(memory,cursor,0x190,current,error)||current!=behaviour||
       !field(memory,behaviour,0,ap,error)||ap!=image+0x172b450)
        return fail(error,error.empty()?"NPC original cursor/behavior owner differs":error);
    return true;
}
}

bool place_probe(const json& args,json& result,std::string& error) {
    result=json::object();
    if(!args.is_object()||args.size()!=6||!args.contains("token")||
       !args.contains("region_id")||!args.contains("tool")||
       !args.contains("character_slot")||!args.contains("character_type")||
       !args.contains("position")||!args.at("token").is_string()||
       !args.at("region_id").is_number_integer()||!args.at("tool").is_string()||
       !args.at("character_slot").is_number_integer()||
       !args.at("character_type").is_number_integer()||
       !args.at("position").is_array()||args.at("position").size()!=3)
        return fail(error,"NPC original probe requires exact token, region, tool, CharacterTypeID and position");
    const auto token=args.at("token").get<std::string>();
    const auto id=args.at("region_id").get<int64_t>();
    const auto slot=args.at("character_slot").get<int64_t>();
    const auto type=args.at("character_type").get<int64_t>();
    const auto tool=args.at("tool").get<std::string>();
    if(!token_valid(token)||ui_attempts.contains(token)||ui_attempts.size()>=64||
       id<0||id>INT32_MAX||slot<0||slot>INT32_MAX||type<0||type>INT32_MAX||
       quest_bridge::tool_type(tool)<0)
        return fail(error,"NPC original probe token, region, tool or CharacterTypeID invalid");
    std::array<float,3> point{};
    for(size_t i=0;i<point.size();++i){
        if(!args.at("position")[i].is_number())return fail(error,"NPC probe position requires numbers");
        const auto value=args.at("position")[i].get<double>();
        if(!std::isfinite(value)||std::abs(value)>(std::numeric_limits<float>::max)())
            return fail(error,"NPC probe position outside finite float32");
        point[i]=float(value);
    }
    result={{"token",token},{"attempted_mask",0},{"returned_mask",0},
        {"exception",0},{"phase","preflight"},{"confirmed",false}};
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error)||sha!=exe_sha||
       !thread||thread!=GetCurrentThreadId())
        return fail(error,"NPC probe pinned EXE or Mainthread differs: "+error);
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,image,sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,image,thread,api);
    host::WorldState world_before,world_after;
    if(!reader.current(world_before,true,error)||!test_world(memory,image,error))return false;
    uintptr_t region=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e) {
        return field(memory,base,offset,value,e);
    };
    if(!region_saved_identity::resolve(image,uintptr_t(world_before.origin.map),
        world_before.root,int32_t(id),0,region,read_field,error))return false;
    std::map<std::string,Tool> inventory;
    if(!tools(memory,image,inventory,error))return false;
    const auto& item=inventory.at(tool);
    if(item.count!=1||item.native_type!=quest_bridge::tool_type(tool)||!item.behaviour)
        return fail(error,"NPC original probe tool absent or ambiguous");
    uintptr_t cursor=0,window=0,chosen=0,preview=0,selected_ptr=0;
    uint8_t alternate=1;
    if(!field(memory,image,0x13df9d0,cursor,error)||!cursor||
       !field(memory,item.behaviour,0x3e1,alternate,error)||alternate!=0||
       !point_and_cursor(memory,image,uintptr_t(world_before.origin.map),
           world_before.root,region,cursor,item.behaviour,point,error))
        return fail(error,error.empty()?"NPC original probe cursor or alternate release mode differs":error);
    std::vector<Option> available;json selected=nullptr;
    if(!options(memory,image,item.behaviour,window,available,selected,error)||
       selected!=json(std::array<int32_t,2>{int32_t(slot),int32_t(type)}))
        return fail(error,error.empty()?"NPC original probe selected CharacterTypeID differs":error);
    const auto choice=std::find_if(available.begin(),available.end(),[&](const Option& value){
        return value.id==std::array<int32_t,2>{int32_t(slot),int32_t(type)};});
    if(choice==available.end())return fail(error,"NPC original probe CharacterType absent from ThemeList");
    chosen=choice->character;
    npc_census::State before,hover,after;
    native::SimulationState money_before,money_hover,money_after;
    undo_probe::UndoWitness undo_before,undo_hover,undo_after;
    if(!npc_census::read(memory,image,region,before,error)||
       !native::read_simulation_state(money_before,error)||
       !undo_probe::read_undo(memory.process,image,undo_before,error)||
       undo_before.active_graph)return fail(error,error.empty()?"NPC original probe requires idle Undo":error);
    if(before.rows.size()>256)return fail(error,"NPC original probe small-world row bound exceeded");
    result["before"]={{"npc",census_json(before)},{"ledger",ledger_json(money_before)},
        {"undo",undo_json(undo_before)},{"world_edits",world_before.edits},
        {"world_saves",world_before.saves}};
    ui_attempts.emplace(token,result);
    auto& receipt=ui_attempts.at(token);
    auto unknown=[&](const std::string& cause){
        error=cause;result["phase"]="unknown";result["cause"]=cause;
        receipt=result;return false;
    };
    result["attempted_mask"]=1;receipt=result;
    auto seh=npc_input(image,item.behaviour,cursor,point,false);
    result["exception"]=seh;
    if(seh)return unknown("NPC original MouseOver SEH="+std::to_string(seh));
    result["returned_mask"]=1;receipt=result;
    std::array<float,3> native_point{};
    if(!field(memory,item.behaviour,0x358,preview,error)||!preview||
       !field(memory,item.behaviour,0x360,selected_ptr,error)||selected_ptr!=chosen||
       !field(memory,item.behaviour,0x378,native_point,error)||native_point!=point||
       !npc_census::read(memory,image,region,hover,error)||
       !native::read_simulation_state(money_hover,error)||
       !undo_probe::read_undo(memory.process,image,undo_hover,error)||
       !reader.current(world_after,true,error))
        return unknown(error.empty()?"NPC original MouseOver preview/readback differs":error);
    result["after_mouse_over"]={{"preview_present",true},{"npc",census_json(hover)},
        {"ledger",ledger_json(money_hover)},{"undo",undo_json(undo_hover)}};
    if(hover.manager!=before.manager||hover.next_local_tag!=before.next_local_tag||
       hover.rows!=before.rows||!same_ledger(money_before,money_hover)||
       undo_before!=undo_hover||!host::same_world(world_before.origin,world_after.origin)||
       world_before.root!=world_after.root||world_before.saves!=world_after.saves||
       world_before.edits!=world_after.edits)
        return unknown("NPC original MouseOver changed world, NPC, ledger or Undo before Release");
    if(!point_and_cursor(memory,image,uintptr_t(world_before.origin.map),
        world_before.root,region,cursor,item.behaviour,point,error)||
       !field(memory,item.behaviour,0x3e1,alternate,error)||alternate)
        return unknown(error.empty()?"NPC original alternate Release mode became active":error);
    result["attempted_mask"]=3;receipt=result;
    seh=npc_input(image,item.behaviour,cursor,point,true);
    result["exception"]=seh;
    if(seh)return unknown("NPC original Release SEH="+std::to_string(seh));
    result["returned_mask"]=3;receipt=result;
    if(!npc_census::read(memory,image,region,after,error)||
       !native::read_simulation_state(money_after,error)||
       !undo_probe::read_undo(memory.process,image,undo_after,error)||
       !reader.current(world_after,false,error))return unknown(error);
    result["after"]={{"npc",census_json(after)},{"ledger",ledger_json(money_after)},
        {"undo",undo_json(undo_after)},{"world_edits",world_after.edits},
        {"world_saves",world_after.saves},{"world_active_graph",world_after.active_graph!=0}};
    npc_census::Row created;
    if(!npc_census::added_one(before,after,created,error))return unknown(error);
    npc_quest_records::State created_quests;
    if(!quest_bridge::records(memory,image,int32_t(id),created,created_quests,error))return unknown(error);
    if(created.type!=quest_bridge::tool_type(tool))
        return unknown("NPC original Release created native NPC type differs");
    if(created.character_slot!=slot||created.character_type!=type)
        return unknown("NPC original Release CharacterTypeID differs expected=["+
            std::to_string(slot)+","+std::to_string(type)+"] actual=["+
            std::to_string(created.character_slot)+","+
            std::to_string(created.character_type)+"]");
    if(!host::same_world(world_before.origin,world_after.origin)||
        world_before.root!=world_after.root)
        return unknown("NPC original Release world origin changed");
    if(world_after.saves||world_after.edits||world_after.active_graph)
        return unknown("NPC original Release left saves, edits or active Graph");
    if(undo_after.undo.empty())
        return unknown("NPC original Release committed Undo is absent");
    uintptr_t registered=0;
    const auto lookup_seh=lookup(api.find,created.local_uid,registered);
    if(lookup_seh||registered!=created.local_pointer)
        return unknown("NPC original created UID lookup differs SEH="+std::to_string(lookup_seh));
    undo_probe::GraphWitness graph;
    if(!undo_probe::read_graph(memory.process,image,undo_after.undo.back(),graph,error)||
       !undo_probe::validate_undo_commit(undo_before,undo_after,graph,error))return unknown(error);
    result["created"]={{"local_uid",created.local_uid},{"name",created.name},
        {"character_id",{created.character_slot,created.character_type}},
        {"level",created.level},{"quest_count",created.quest_count},
        {"position",created.position},{"rotation",created.rotation},{"scale",created.scale},
        {"quests",quest_bridge::records_json(created_quests.records)},
        {"quest_tasks",quest_bridge::tasks_json(created_quests)}};
    result["cash_delta"]=int64_t(money_after.cash)-money_before.cash;
    result["undo_graph"]={{"nodes",graph.nodes},{"operations",graph.operations},
        {"locked",graph.locked},{"pending_delete",graph.pending_delete}};
    result["phase"]="confirmed";result["confirmed"]=true;receipt=result;
    error.clear();return true;
}
