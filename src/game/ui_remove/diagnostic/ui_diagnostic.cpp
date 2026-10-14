#include "../../snapshot_building_key.h"
#include "ui_diagnostic.h"
#include "native_contract_bytes.h"
#include "scenery_ui_contract_bytes.h"
#include <cstring>
#include <regex>
#include <vector>

namespace mt2mp::action_session::ui_diagnostic {
namespace {
template<class T>bool field(ui::Memory& m,uintptr_t base,size_t offset,T& out,std::string& cause) {
    if(!base||base>UINTPTR_MAX-offset){cause="native UI object field address overflows";return false;}
    if(!m.copy(base+offset,&out,sizeof out,cause)){if(cause.empty())cause="native UI object field read failed without cause";return false;}return true;
}
bool selection(ui::Memory& m,uintptr_t image,uintptr_t manager,size_t offset,uintptr_t target,std::string& cause) {
    uintptr_t ap=0,data=0,selected=0;int32_t count=0,capacity=0;
    if(!field(m,manager,offset,ap,cause)||!field(m,manager,offset+8,data,cause)||!field(m,manager,offset+16,count,cause)||!field(m,manager,offset+20,capacity,cause))return false;
    if(ap!=image+0x1748c50||count!=1||capacity<1||!field(m,data,0,selected,cause)||selected!=target){if(cause.empty())cause="native UI single selection differs from snapshot target";return false;}return true;
}
bool input(const Request& r) {
    if(r.diagnostic_uuid.size()!=36||r.snapshot_uuid.size()!=36||r.canonical_record_key.size()>160)return false;
    static const std::regex uuid("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    static const std::regex key("^action:[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return std::regex_match(r.diagnostic_uuid,uuid)&&std::regex_match(r.snapshot_uuid,uuid)&&(std::regex_match(r.canonical_record_key,key)||game::snapshot_building_key::valid(r.canonical_record_key));
}
}
bool resolve_api(ui::Memory& memory,uintptr_t image,std::string_view sha,Api& out,std::string& cause,Family family) {
    out={};cause.clear();if(!image||image>UINTPTR_MAX-0x1748c50||sha!=contract::exe_sha256){cause="native UI diagnostic exact module identity differs";return false;}
    for(const auto& guard:contract::guards) {
        std::vector<uint8_t> actual(guard.size);
        if(!memory.copy(image+guard.rva,actual.data(),actual.size(),cause))return false;
        if(std::memcmp(actual.data(),guard.bytes,guard.size)){cause="native UI diagnostic full code guard differs rva="+std::to_string(guard.rva);return false;}
    }
    if(family!=Family::Building&&family!=Family::Scenery){cause="native UI diagnostic family invalid";return false;}
    const bool scene=family==Family::Scenery;
    if(scene)for(const auto& guard:scene_contract::guards){std::vector<uint8_t> actual(guard.size);if(!memory.copy(image+guard.rva,actual.data(),actual.size(),cause))return false;
        if(std::memcmp(actual.data(),guard.bytes,guard.size)){cause="native Scenery UI diagnostic full code guard differs rva="+std::to_string(guard.rva);return false;}}
    uintptr_t command=0,handler_open=0;
    if(!field(memory,image+(scene?0x1725bb0:0x172af10),contract::window_ui_command_slot,command,cause)||command!=image+(scene?0x7b3cf0:contract::ui_command_rva)||
       !field(memory,image+(scene?0x17316a0:0x1731720),0x20,handler_open,cause)||handler_open!=image+(scene?0xc13640:contract::building_handler_open_rva)){if(cause.empty())cause="native UI diagnostic vtable slots differ";return false;}
    out={reinterpret_cast<SelectProp>(image+contract::select_prop_rva),reinterpret_cast<SetInfoWindow>(image+contract::set_info_window_rva),
        reinterpret_cast<RecordCtor>(image+contract::record_cstr_ctor_rva),reinterpret_cast<RecordDtor>(image+contract::record_dtor_rva),
        reinterpret_cast<RecordEquals>(image+contract::record_label_equals_rva),reinterpret_cast<UiCommand>(image+(scene?0x7b3cf0:contract::ui_command_rva)),scene?"Destroy":"DestroyBuilding"};
    cause.clear();return true;
}
bool read_live_window(ui::Memory& memory,const Context& context,Window& out,std::string& cause) {
    out={};cause.clear();uintptr_t manager=0,ap=0,focused=0,proxy_target=0,command=0;uint8_t progress=0,queue=0,visible=0;
    if(context.family!=Family::Building&&context.family!=Family::Scenery){cause="native UI live window family invalid";return false;}
    const bool scene=context.family==Family::Scenery;
    if(!context.image||context.image>UINTPTR_MAX-0x1748c50||!context.manager||!context.target||context.target>UINTPTR_MAX-0x20){cause="native UI diagnostic context addresses invalid";return false;}
    if(!field(memory,context.image,0x13df630,manager,cause)||manager!=context.manager||!field(memory,manager,0,ap,cause)||ap!=context.image+0x17205d0||
       !field(memory,manager,0xd1,progress,cause)||!field(memory,manager,0xd3,queue,cause)||progress||queue||
       !field(memory,manager,0x80,focused,cause)||focused!=context.target||
       !selection(memory,context.image,manager,0x20,context.target,cause)||!selection(memory,context.image,manager,0x38,context.target,cause)) {
        if(cause.empty())cause="native UI diagnostic manager/current single selection differs";return false;
    }
    if(!field(memory,manager,0xb8,out.handler,cause)||!field(memory,out.handler,0,ap,cause)||ap!=context.image+(scene?0x17316a0:0x1731720)||
       !field(memory,out.handler,0x10,out.window,cause)||!field(memory,out.window,0,ap,cause)||ap!=context.image+(scene?0x1725bb0:0x172af10)||
       !field(memory,ap,contract::window_ui_command_slot,command,cause)||command!=context.image+(scene?0x7b3cf0:contract::ui_command_rva)||
       !field(memory,out.window,0x45f,visible,cause)||visible!=1||!field(memory,out.window,scene?0x4f0:0x4f8,out.target,cause)||out.target!=context.target||
       !field(memory,out.window,scene?0x4f8:0x500,out.proxy,cause)||!field(memory,out.proxy,0,proxy_target,cause)||proxy_target!=context.target+0x20) {
        if(cause.empty())cause="native UI diagnostic live typed InfoWindow/weak target differs";return false;
    }
    cause.clear();return true;
}
bool invoke(const Request& request,RootServices& root,ui::Memory& memory,Calls& calls,Receipt& receipt,std::string& cause) {
    receipt={};cause.clear();auto failed=[&](const char* stage) {
        receipt.failed_stage=stage;if(cause.empty())cause=std::string("native UI diagnostic failed without cause stage=")+stage;
        receipt.cause=cause;root.log(stage,cause);return false;
    };
    if(!input(request)){cause="native UI diagnostic requires scalar UUID, snapshot UUID and canonical record key";return failed("schema");}
    Context context;if(!root.preflight(request,context,cause)||!context.uid||!context.epoch||!context.generation)return failed("actual_root_preflight");
    Api api;if(!resolve_api(memory,context.image,context.exe_sha256,api,cause,context.family))return failed("exact_native_api");
    if(!calls.select(api,context,receipt,cause))return failed("native_select_show");
    if(!receipt.selection_attempted||!receipt.selection_returned||receipt.native_exception){cause="native UI selection receipt incomplete";return failed("native_select_receipt");}
    Window window;if(!read_live_window(memory,context,window,cause))return failed("actual_live_window");
    receipt.window_confirmed=true;
    if(!root.preparation_unchanged(context,window,cause))return failed("world_unchanged_before_ui_command");
    if(!calls.command(api,window,receipt,cause))return failed("native_destroy_prop_ui_command");
    if(!receipt.record_constructed||!receipt.record_label_confirmed||!receipt.ui_command_attempted||!receipt.ui_command_returned||!receipt.ui_command_accepted||
       !receipt.record_cleanup_attempted||!receipt.record_cleanup_confirmed||receipt.native_exception||receipt.cleanup_exception){cause="native UI command/record cleanup receipt incomplete";return failed("native_ui_command_receipt");}
    if(!root.observed_intent(request,context,cause))return failed("actual_ui_intent_ack");
    receipt.intent_observed=true;cause.clear();return true;
}
}
