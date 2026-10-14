#include "object_identity/remove_runtime/copy/undo_witness.h"
#include <algorithm>

namespace {
namespace undo=build_actions::private_native;

struct Option {
    std::array<int32_t,2> id{};
    uintptr_t character=0;
};

bool token_valid(const std::string& token) {
    if(token.size()!=36)return false;
    for(size_t i=0;i<token.size();++i) {
        if(i==8||i==13||i==18||i==23) {if(token[i]!='-')return false;}
        else if(!((token[i]>='0'&&token[i]<='9')||(token[i]>='a'&&token[i]<='f')))return false;
    }
    return true;
}

uint32_t original_press(uintptr_t image,uintptr_t button) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*)>(image+0x580870)(reinterpret_cast<void*>(button));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t original_select(uintptr_t image,uintptr_t window,uintptr_t character) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x67fb60)(reinterpret_cast<void*>(window),reinterpret_cast<void*>(character));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t original_get_selection(uintptr_t image,uintptr_t window,uintptr_t& selected) noexcept {
    __try {selected=uintptr_t(reinterpret_cast<void*(__fastcall*)(void*)>(image+0x67fb40)(reinterpret_cast<void*>(window)));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

bool options(Memory& memory,uintptr_t image,uintptr_t behaviour,
             uintptr_t& window,std::vector<Option>& available,json& selected,
             std::string& error) {
    window=0;available.clear();selected=nullptr;
    uintptr_t ap=0,list=0,data=0,current=0;
    int32_t count=0;
    if(!field(memory,behaviour,0,ap,error)||ap!=image+0x172b450||
       !field(memory,behaviour,0x168,window,error)||!window||
       !field(memory,window,0,ap,error)||ap!=image+0x17321a0||
       !field(memory,window,0x4e8,list,error)||!list||
       !field(memory,list,0,ap,error)||ap!=image+0x172b1d0||
       !field(memory,list,0x348,data,error)||
       !field(memory,list,0x350,count,error))
        return fail(error,error.empty()?"NPC original ThemeWindow/List class or fields differ":error);
    if(count<0||count>256||(count&&!data)||data>UINTPTR_MAX-size_t(count)*8)
        return fail(error,"NPC original ThemeList option span differs");
    std::set<std::array<int32_t,2>> ids;
    for(int32_t i=0;i<count;++i) {
        uintptr_t entry=0,character=0;
        Option option;
        if(!field(memory,data,size_t(i)*8,entry,error)||!entry||
           !field(memory,entry,0,ap,error)||ap!=image+0x1738b10||
           !field(memory,entry,0x338,character,error))
            return fail(error,error.empty()?"NPC original ThemeSelector class differs":error);
        if(!character)continue;
        if(!field(memory,character,0,ap,error)||ap!=image+0x1705990||
           !field(memory,character,0x2c,option.id,error))
            return fail(error,error.empty()?"NPC original ThemeSelector or CharacterType class differs":error);
        if(option.id[0]<0||option.id[1]<0||!ids.insert(option.id).second)
            return fail(error,"NPC ThemeList CharacterTypeID is invalid or duplicated");
        option.character=character;
        available.push_back(option);
    }
    const auto seh=original_get_selection(image,window,current);
    if(seh)return fail(error,"NPC original GetSelection SEH="+std::to_string(seh));
    if(current) {
        const auto found=std::find_if(available.begin(),available.end(),
            [current](const Option& item){return item.character==current;});
        if(found==available.end())return fail(error,"NPC selected CharacterType absent from original ThemeList");
        selected=found->id;
    }
    return true;
}

bool same_world_effect(const npc_census::State& a,const npc_census::State& b,
                       const native::SimulationState& money_a,const native::SimulationState& money_b,
                       const undo::UndoWitness& undo_a,const undo::UndoWitness& undo_b) {
    return a.manager==b.manager&&a.next_local_tag==b.next_local_tag&&
        a.global_random==b.global_random&&a.rows==b.rows&&
        money_a.cash==money_b.cash&&money_a.record_present==money_b.record_present&&
        money_a.record_day==money_b.record_day&&money_a.record_cash==money_b.record_cash&&
        money_a.record_income==money_b.record_income&&money_a.record_expense==money_b.record_expense&&
        money_a.logged_in_subscribers==money_b.logged_in_subscribers&&
        money_a.line_counts==money_b.line_counts&&money_a.line_income==money_b.line_income&&
        undo_a==undo_b;
}
std::map<std::string,json> ui_attempts;
}

bool tool_ui(const std::string& token,int32_t region_id,const std::string& tool,
             const std::string& phase,int32_t character_slot,int32_t character_type,
             json& result,std::string& error) {
    result={{"token",token},{"operation",phase},{"attempted",false},{"returned",false},
            {"confirmed",false},{"exception",0}};
    if(!token_valid(token)||region_id<0||quest_bridge::tool_type(tool)<0||
       (phase!="open"&&phase!="select")||
       (phase=="open"&&(character_slot!=-1||character_type!=-1))||
       (phase=="select"&&(character_slot<0||character_type<0)))
        return fail(error,"NPC UI requires canonical token, region, tool and open/select fields");
    if(ui_attempts.contains(token)){result=ui_attempts.at(token);return fail(error,"NPC UI token already attempted - no replay");}
    if(ui_attempts.size()>=64)return fail(error,"NPC UI retained token limit reached");
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error)||sha!=exe_sha||
       !thread||thread!=GetCurrentThreadId())
        return fail(error,"NPC UI pinned EXE or Mainthread differs: "+error);
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,image,sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,image,thread,api);
    host::WorldState before_world,after_world;
    if(!reader.current(before_world,true,error))return false;
    uintptr_t region=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e) {
        return field(memory,base,offset,value,e);
    };
    if(!region_saved_identity::resolve(image,uintptr_t(before_world.origin.map),
        before_world.root,region_id,0,region,read_field,error))return false;
    npc_census::State before,after;
    native::SimulationState money_before,money_after;
    undo::UndoWitness undo_before,undo_after;
    std::map<std::string,Tool> inventory;
    if(!npc_census::read(memory,image,region,before,error)||
       !native::read_simulation_state(money_before,error)||
       !undo::read_undo(memory.process,image,undo_before,error)||
       !tools(memory,image,inventory,error))return false;
    const auto& item=inventory.at(tool);
    if(item.count!=1||item.native_type!=quest_bridge::tool_type(tool)||!item.behaviour)
        return fail(error,"NPC original tool is absent or ambiguous in actual inventory");
    uintptr_t cursor=0,active=0,window=0,chosen=0;
    std::vector<Option> available;
    json selected_before=nullptr,selected_after=nullptr;
    placement_ui_probe::Binding binding;
    if(phase=="open") {
        placement_ui_probe::Request request;request.family=placement_ui_probe::Family::NPC;
        request.type=tool;request.variant="npc";
        const placement_ui_probe::Context context{image,uintptr_t(before_world.origin.state),
            before_world.origin.world_epoch,before_world.origin.generation,thread};
        placement_ui_probe::ProcessMemory ui_memory;
        if(!placement_ui_probe::read_existing_tool(ui_memory,context,request,binding,error)||
           binding.behaviour!=item.behaviour)
            return fail(error,error.empty()?"NPC original actionbar button differs":error);
        cursor=binding.cursor;
    }else {
        if(!field(memory,image,0x13df9d0,cursor,error)||!cursor||
           !field(memory,cursor,0,active,error)||active!=image+0x1786dd0||
           !field(memory,cursor,0x190,active,error)||active!=item.behaviour||
           !options(memory,image,item.behaviour,window,available,selected_before,error))
            return fail(error,error.empty()?"NPC selected tool is not current original cursor":error);
        const std::array<int32_t,2> requested{character_slot,character_type};
        const auto found=std::find_if(available.begin(),available.end(),
            [&requested](const Option& value){return value.id==requested;});
        if(found==available.end())return fail(error,"NPC requested CharacterTypeID absent from original ThemeList");
        chosen=found->character;
    }
    result["selected_before"]=selected_before;
    ui_attempts.emplace(token,result);
    result["attempted"]=true;
    ui_attempts.at(token)=result;
    const auto seh=phase=="open"?original_press(image,binding.button):original_select(image,window,chosen);
    result["exception"]=seh;
    if(!seh)result["returned"]=true;
    if(seh) {
        result["phase"]="unknown";ui_attempts.at(token)=result;
        return fail(error,"NPC original "+phase+" SEH="+std::to_string(seh)+"; no replay");
    }
    if(!field(memory,cursor,0x190,active,error)||active!=item.behaviour||
       !options(memory,image,item.behaviour,window,available,selected_after,error)||
       !npc_census::read(memory,image,region,after,error)||
       !native::read_simulation_state(money_after,error)||
       !undo::read_undo(memory.process,image,undo_after,error)||
       !reader.current(after_world,true,error)||
       !host::same_world(before_world.origin,after_world.origin)||
       before_world.root!=after_world.root||
       !same_world_effect(before,after,money_before,money_after,undo_before,undo_after)||
       (phase=="select"&&selected_after!=json(std::array<int32_t,2>{character_slot,character_type}))) {
        if(error.empty())error="NPC original UI readback, world or selected CharacterTypeID differs";
        result["phase"]="unknown";result["selected_after"]=selected_after;
        ui_attempts.at(token)=result;
        return false;
    }
    json entries=json::array();
    for(const auto& value:available)entries.push_back(value.id);
    result["options"]=std::move(entries);
    result["selected_after"]=selected_after;
    result["world_unchanged"]=true;
    result["phase"]="confirmed";
    result["confirmed"]=true;
    ui_attempts.at(token)=result;
    error.clear();return true;
}
