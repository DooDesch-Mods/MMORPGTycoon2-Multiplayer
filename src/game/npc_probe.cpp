#include "npc_probe.h"
#include "npc_saved_identity_runtime.h"
#include "npc_target_key.h"
#include "npc_host_state_binding.h"
#include "session_provider.h"
#include "npc_census.h"
#include "npc_quest_records.h"
#include "npc_quest_retirement.h"
#include "npc_quest_visitor_reader.h"
#include "npc_quest_target_runtime.h"
#include "npc_remove_effect.h"
#include "move_kiss/move_kiss.h"
#include "ui_move_runtime.h"
#include "move_ui_probe/native_guards.h"
#include "typed_action_runtime.h"
#include "ui_remove_runtime.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include "ui_remove/diagnostic/native_contract_bytes.h"
#include "placement_ui_probe/ui_probe.h"
#include "region_saved_identity.h"
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/config.h"
#include <MinHook.h>
#include <SDL3/SDL_mouse.h>
#include <bcrypt.h>
#include <intrin.h>
#include <Windows.h>
#include <array>
#include <algorithm>
#include <map>
#include <optional>
#include <atomic>
#include <cstdio>
#include <cmath>
#include <set>

namespace mt2mp::move_kiss {
bool npc_transform(const State&,std::array<unsigned char,0x6c>&,std::string&);
}

namespace mt2mp::game::npc_probe {
namespace {
namespace host=build_actions::private_native::host;
using json=nlohmann::json;
constexpr const char* exe_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";

bool fail(std::string& error,const std::string& reason){error=reason;return false;}
class Memory final:public npc_census::Memory,public action_session::ui_capture::Memory {
public:
    host::ProcessMemory process;
    bool copy(uintptr_t address,void* output,size_t size,std::string& error) override {
        return process.copy(address,output,size,error);
    }
    bool region(uintptr_t address,action_session::ui_capture::Region& out,std::string& error)override {
        out={};MEMORY_BASIC_INFORMATION info{};
        if(VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof info)!=sizeof info){
            error="NPC UI VirtualQuery failed Win32="+std::to_string(GetLastError());return false;}
        const auto protection=info.Protect&0xff;
        out={uintptr_t(info.AllocationBase),info.State==MEM_COMMIT,
            !(info.Protect&PAGE_GUARD)&&(protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||
                protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY),
            info.Type==MEM_IMAGE,
            protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||
                protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY};
        return true;
    }
};
template<class T>bool field(Memory& memory,uintptr_t base,size_t offset,T& value,std::string& error) {
    if(!base||base>UINTPTR_MAX-offset)return fail(error,"NPC query native address invalid");
    return memory.copy(base+offset,&value,sizeof value,error);
}
bool name(Memory& memory,uintptr_t object,std::string& output,std::string& error) {
    uintptr_t data=0;size_t length=0;
    if(!field(memory,object,0,data,error)||!field(memory,object,8,length,error))return false;
    if(!data||length<1||length>32||data>UINTPTR_MAX-length)
        return fail(error,"NPC tool native typeName span invalid");
    output.resize(length);
    if(!memory.copy(data,output.data(),length,error))return false;
    if(output.find('\0')!=std::string::npos||!npc_census::utf8(output))
        return fail(error,"NPC tool native typeName invalid UTF-8");
    return true;
}
struct Tool {
    int32_t count=0;
    int32_t native_type=-1;
    bool selected=false;
    std::array<int32_t,2> selected_character_id{};
    uintptr_t behaviour=0;
};
bool tools(Memory& memory,uintptr_t image,std::map<std::string,Tool>& result,std::string& error) {
    result={{"QuestGiver",{}},{"Guard",{}},{"ClassTrainer",{}}};
    uintptr_t player=0,inventory=0,cursor=0,ap=0;
    npc_census::InventoryHeader header;
    if(!field(memory,image,0x13df9f0,player,error)||
       !field(memory,player,0,ap,error)||ap!=image+0x17873b0||
       !field(memory,player,0x1d0,inventory,error)||
       !field(memory,inventory,0,ap,error)||ap!=image+0x16df0f0||
       !field(memory,image,0x13df9d0,cursor,error)||
       !field(memory,cursor,0,ap,error)||ap!=image+0x1786dd0||
       !npc_census::read_inventory_header(memory,inventory,header,error))
        return fail(error,error.empty()?"NPC native player, inventory or cursor class differs":error);
    std::set<uintptr_t> seen_items;
    for(int32_t i=0;i<header.count;++i) {
        uintptr_t item=0,type=0,weak=0,target=0,owned=0,behaviour=0;
        if(!field(memory,header.items,size_t(i)*8,item,error))return false;
        if(!item)continue;
        if(!seen_items.insert(item).second||
           !field(memory,item,0,ap,error)||ap!=image+0x17437f0||
           !field(memory,item,0x68,type,error)||!type||
           !field(memory,item,0x70,weak,error)||!weak||
           !field(memory,weak,0,target,error)||
           !field(memory,type,0x40,owned,error)||target!=type+0x38||owned!=weak||
           !field(memory,type,0,ap,error))
            return fail(error,error.empty()?"NPC native inventory item or weak owner differs":error);
        if(ap!=image+0x173b7b0)continue;
        if(!field(memory,type,0x160,behaviour,error)||!behaviour||
           !field(memory,behaviour,0,ap,error))return false;
        if(ap!=image+0x172b450)continue;
        std::string type_name;
        int32_t npc_type=-1;
        if(!name(memory,behaviour+0x148,type_name,error)||
           !field(memory,behaviour,0x170,npc_type,error))return false;
        const auto found=result.find(type_name);
        if(found==result.end()||npc_type<0||npc_type>3)
            return fail(error,"NPC native inventory typeName or type enum differs");
        if(found->second.count&&found->second.native_type!=npc_type)
            return fail(error,"NPC duplicate tool type enums differ");
        found->second.native_type=npc_type;
        ++found->second.count;
        found->second.behaviour=behaviour;
        uintptr_t selected=0;
        if(!field(memory,behaviour,0x360,selected,error))return false;
        if(selected) {
            if(!field(memory,selected,0,ap,error)||ap!=image+0x1705990||
               !field(memory,selected,0x2c,found->second.selected_character_id,error))
                return fail(error,error.empty()?"NPC selected CharacterType class or ID unreadable":error);
            found->second.selected=true;
        }
    }
    npc_census::InventoryHeader after;
    uintptr_t after_player=0;
    if(!npc_census::read_inventory_header(memory,inventory,after,error)||
       !field(memory,image,0x13df9f0,after_player,error))return false;
    if(after!=header||after_player!=player)
        return fail(error,"NPC native inventory changed during readback");
    return true;
}
uint32_t lookup(host::FindUid find,uint64_t uid,uintptr_t& pointer) noexcept {
    __try{pointer=uintptr_t(find(uid));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}

#include "npc_quest_bridge.inc"
#include "npc_quest_visitors.inc"

bool query(int32_t region_id,json& result,std::string& error) {
    result=json::object();
    if(region_id<0)return fail(error,"NPC query region ID requires nonnegative int32");
    uintptr_t image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(image,thread,sha,error)||sha!=exe_sha||
       !thread||thread!=GetCurrentThreadId())
        return fail(error,"NPC query pinned EXE or Mainthread differs: "+error);
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,image,sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,image,thread,api);
    host::WorldState before,after;
    if(!reader.current(before,true,error))return false;
    const uintptr_t map=uintptr_t(before.origin.map);
    uintptr_t region=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e) {
        return field(memory,base,offset,value,e);
    };
    if(!region_saved_identity::resolve(image,map,before.root,region_id,0,region,read_field,error))return false;
    npc_census::State census;
    if(!npc_census::read(memory,image,region,census,error))return false;
    for(const auto& row:census.rows) {
        uintptr_t found=0;const auto seh=lookup(api.find,row.local_uid,found);
        if(seh||found!=row.local_pointer)
            return fail(error,"NPC native UID table differs uid="+std::to_string(row.local_uid)+
                              " SEH="+std::to_string(seh));
    }
    std::map<std::string,Tool> inventory;
    if(!tools(memory,image,inventory,error)||!reader.current(after,true,error))return false;
    if(!host::same_world(before.origin,after.origin)||before.root!=after.root||
       before.saves!=after.saves||before.edits!=after.edits||before.active_graph!=after.active_graph)
        return fail(error,"NPC native world changed during query");
    npc_quest_target_runtime::CurrentTargets current_targets;std::string targets_sha256;
    if(!npc_quest_target_runtime::read_current(region_id,current_targets,error)||
       !quest_bridge::pool(region_id,current_targets,targets_sha256,error))return false;
    json counts={{"quest_giver",0},{"guard",0},{"trainer",0}};
    json samples=json::array(),visual_samples=json::array();
    std::set<std::array<int32_t,2>> character_ids;
    for(const auto& row:census.rows) {
        npc_quest_records::State quests;
        if(!quest_bridge::records(memory,image,region_id,row,quests,error))return false;
        const char* field_name=row.type==0?"quest_giver":row.type==1?"guard":"trainer";
        counts[field_name]=counts.at(field_name).get<int32_t>()+1;
        character_ids.insert({row.character_slot,row.character_type});
        if(samples.size()<32){
            std::string key,binding_error;
            const bool bound=action_runtime::local_key(row.local_pointer,row.local_uid,key,binding_error);
            samples.push_back({{"local_uid",row.local_uid},{"type",row.type},
                {"canonical_record_key",bound?key:std::string{}},{"binding_cause",binding_error},
                {"name",row.name},{"character_id",{row.character_slot,row.character_type}},
                {"level",row.level},{"quest_count",row.quest_count},{"position",row.position},
                {"rotation",row.rotation},{"scale",row.scale},
                {"quests",quest_bridge::records_json(quests.records)},
                {"quest_tasks",quest_bridge::tasks_json(quests)}});
            json visual={{"local_uid",row.local_uid}};
            npc_move_visibility::Snapshot snapshot;std::string visual_error;
            if(!npc_move_visibility::read(image,row.local_pointer,read_field,snapshot,visual_error)){
                visual["read"]=false;visual["cause"]=visual_error;
            }else{
                visual["read"]=true;visual["on_map"]=snapshot.on_map;
                visual["actor_visible"]=snapshot.actor_visible;
                uintptr_t definition=0;std::array<float,16> matrix{};
                float extra_scale=0,actual_scale=0;
                if(!field(memory,snapshot.skeleton,0x20,matrix,visual_error)||
                   !field(memory,snapshot.skeleton,0xb4,extra_scale,visual_error)||
                   !field(memory,snapshot.skeleton,0x10,definition,visual_error)||
                   !definition||!field(memory,definition,0x34,actual_scale,visual_error)){
                    visual["scale_read"]=false;
                    visual["scale_cause"]=visual_error.empty()?"NPC visual definition absent":visual_error;
                }else{
                    bool finite=std::isfinite(extra_scale)&&std::isfinite(actual_scale)&&
                        std::isfinite(extra_scale*actual_scale);
                    for(const float value:matrix)finite=finite&&std::isfinite(value);
                    if(!finite){visual["scale_read"]=false;
                        visual["scale_cause"]="NPC visual matrix or scales nonfinite";}
                    else{visual["scale_read"]=true;visual["skeleton_matrix"]=matrix;
                        visual["extra_scale"]=extra_scale;
                        visual["actual_scale"]=actual_scale;
                        visual["visual_scale"]=extra_scale*actual_scale;}
                }
            }
            visual_samples.push_back(std::move(visual));
        }
    }
    json visitors;
    if(!quest_visitors::read(memory,image,region,visitors,error))return false;
    if(!reader.current(after,true,error))return false;
    if(!host::same_world(before.origin,after.origin)||before.root!=after.root||
       before.saves!=after.saves||before.edits!=after.edits||before.active_graph!=after.active_graph)
        return fail(error,"NPC native world changed during visitor Quest query");
    json characters=json::array();for(const auto& id:character_ids)characters.push_back(id);
    npc_quest_target_runtime::CurrentTargets targets_after;
    if(!npc_quest_target_runtime::read_current(region_id,targets_after,error))return false;
    if(targets_after.snapshot_uuid!=current_targets.snapshot_uuid||
       targets_after.saved_region!=current_targets.saved_region||
       targets_after.building_keys!=current_targets.building_keys||
       targets_after.monster_zone_keys!=current_targets.monster_zone_keys)
        return fail(error,"NPC Quest target pools changed during query");
    json tool_data=json::object();for(const auto& [name,item]:inventory)
        tool_data[name]={{"inventory_count",item.count},{"native_type",item.native_type},
            {"type_resolved",item.native_type==(name=="QuestGiver"?0:name=="Guard"?1:2)},
            {"selected_character_id",
            item.selected?json(item.selected_character_id):json(nullptr)}};
    std::string snapshot_sha;
    if(!npc_saved_identity_runtime::current_snapshot_sha(snapshot_sha,error))return false;
    result={{"region_id",region_id},{"snapshot_uuid",net::current_snapshot_id()},
        {"snapshot_sha256",snapshot_sha},
        {"local_world_epoch",before.origin.world_epoch},{"local_generation",before.origin.generation},
        {"counts",counts},{"total",census.rows.size()},{"character_ids",characters},
        {"samples",samples},{"samples_truncated",census.rows.size()>samples.size()},
        {"visual_samples",visual_samples},{"quest_visitors",std::move(visitors)},
        {"quest_targets_sha256",targets_sha256},
        {"quest_targets",{{"building_keys",current_targets.building_keys},
                           {"monster_zone_keys",current_targets.monster_zone_keys}}},
        {"tool_inventory",tool_data},{"local_uid_scope","diagnostic_only"},
        {"global_random",census.global_random}};
    error.clear();return true;
}
#include "npc_ui_selection.inc"
#include "npc_place_probe.inc"
#include "npc_action_runtime.inc"
}
