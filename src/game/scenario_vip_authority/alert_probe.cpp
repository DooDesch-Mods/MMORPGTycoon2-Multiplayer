#include "alert_probe.h"
#include "alert_probe_reader.h"
#include "../native_open_runtime.h"
#include "../typed_action_runtime.h"
#include "../typed_action/action_session.h"
#include "../session_provider.h"
#include "../live_host_state/memory_query_timing.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>

namespace mt2mp::game::scenario_vip_authority {
namespace {
using World=building_mixed_identity::opened_file::CurrentWorld;
bool same_world(const World& a,const World& b){
    return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&
        a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes;
}
}
bool read_alert_owners(nlohmann::json& out,std::string& error){
    out={{"read_only",true},{"attempted",false},{"observed",false},{"status","error"}};
    const auto rejected=[&](const std::string& cause){error=cause;out["cause"]=cause;return false;};
    try{
        uintptr_t image=0;uint32_t thread=0;std::string sha;
        if(!native::read_resolver_receipt(image,thread,sha,error))return rejected(error);
        if(thread!=GetCurrentThreadId()||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
            return rejected("VIP alert probe requires pinned executable and bound Mainthread");
        World before,after;
        if(!native_open_runtime::read_current_world(before,error)){
            const auto status=building_mixed_identity::opened_file::status();
            if(status.state!="failed"&&(error=="current_world_read_origin_unavailable"||error=="current_world_read_generation_mismatch")){
                out["status"]="absent";out["wait"]="completed owned World unavailable (Title, loading or retired origin)";
            }
            return rejected(error);
        }
        action_session::Context context,context_after;action_session::Binding binding,binding_after;
        if(!action_session::selected_session().current(context,error)||
           !typed_action_runtime::network_binding(context.snapshot_uuid,binding,error))return rejected(error);
        if(binding.native_epoch!=before.world_epoch||binding.native_generation!=before.generation)
            return rejected("VIP alert probe Controller and completed World differ");
        placement_ui_probe::ProcessMemory memory;
        alert_probe::Graph graph;
        {
        live_host_state::apply_timing::PageReadScope pages;
        int32_t pe_offset=0;uint32_t image_size=0;
        if(!image||image>UINTPTR_MAX-0x40||!memory.copy(image+0x3c,&pe_offset,sizeof pe_offset,error))return rejected(error);
        if(pe_offset<0||pe_offset>4096||image>UINTPTR_MAX-size_t(pe_offset)-80)
            return rejected("VIP alert probe PE header extent differs");
        if(!memory.copy(image+size_t(pe_offset)+24+56,&image_size,sizeof image_size,error))return rejected(error);
        alert_probe::Reader reader(memory,image,image_size);
        if(!reader.read(uintptr_t(before.mode),uintptr_t(before.state),graph,error))return rejected(error);
        }
        if(!native_open_runtime::read_current_world(after,error)||
           !action_session::selected_session().current(context_after,error)||
           !typed_action_runtime::network_binding(context_after.snapshot_uuid,binding_after,error))return rejected(error);
        if(!same_world(before,after)||context!=context_after||binding!=binding_after)
            return rejected("VIP alert probe full Origin/Controller/Session changed during read");
        out["session"]=context.session;out["snapshot_uuid"]=context.snapshot_uuid;
        out["role"]=context.role==action_session::Role::Host?"Host":"Client";
        out["native_epoch"]=before.world_epoch;out["native_generation"]=before.generation;
        out["origin_uuid"]=before.uuid;out["origin_sha256"]=before.sha256;
        if(!graph.present){out["status"]="absent";return rejected(graph.absent);}
        out["window_ap_rva"]=graph.window_ap-image;out["window_name"]=graph.window_name;
        out["window_class_assumed"]=false;out["ancestry"]=nlohmann::json::array();
        for(const auto& pane:graph.ancestry)out["ancestry"].push_back({{"ap_rva",pane.ap-image},
            {"parent_children",pane.children.count},{"parent_panes",pane.panes.count},{"reciprocal_membership",true}});
        out["selector_count"]=graph.selectors.count;out["pane_count"]=graph.panes.count;
        out["manager_active_count"]=graph.active.count;out["manager_owned_count"]=graph.owned.count;
        out["scenario_count"]=graph.scenarios.count;out["vip_arrivals"]=nlohmann::json::array();
        for(const auto& alert:graph.alerts)out["vip_arrivals"].push_back({{"type",alert.type},{"extra",alert.extra},
            {"serialized_id",alert.subscriber_id},{"name",alert.name},{"shown",true},{"scenario_owned",true},
            {"manager_owned",false},{"selector_membership",true}});
        out["arrival_status"]=graph.alerts.empty()?"absent":"observed";
        out["status"]="observed";out["observed"]=true;error.clear();return true;
    }catch(const std::exception& exception){return rejected(std::string("VIP alert read-only probe exception: ")+exception.what());}
    catch(...){return rejected("VIP alert read-only probe nonstandard exception");}
}
}
