#include "mt2mp/core/config.h"
namespace mt2mp::game::network_remove_runtime {
namespace {
namespace a=action_session;namespace ui=a::ui_capture;namespace nr=ui_network_runtime;
namespace region=ui_region_runtime;namespace graph_reader=connected_network_native;namespace undo=build_actions::private_native;
using json=nlohmann::json;
struct Operation {a::Binding binding;a::Request request;a::Evidence evidence;ui::LocalBinding target;
    std::map<std::string,network_identity::ObjectEntry> before;std::vector<std::string> removed;
    bool attempted=false,returned=false,confirmed=false,retired=false;uint64_t original_calls=0;std::string cause;};
std::map<std::string,Operation> operations;
const ui::Replay* active_replay=nullptr;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool current(region::Runtime& r,const a::Binding& binding,const std::string& snapshot,int32_t id,region::LocalView& view,std::string& e){
    return GetCurrentThreadId()==r.thread&&region::identity(r,id,0,view,e)&&region::bound(view,binding,snapshot,e);
}
bool census(region::Runtime& r,const region::LocalView& view,a::NetworkRemoval& out,std::map<std::string,network_identity::ObjectEntry>& objects,std::string& e){
    out={};objects.clear();graph_reader::Graph graph;
    if(!nr::graph(r,view,graph,out.graph,e))return false;
    auto add=[&](uintptr_t pointer,uint64_t uid){std::string key;network_identity::ObjectEntry entry;
        if(!network_identity::object_key(pointer,uid,key,entry,e))return false;
        if(!objects.emplace(key,entry).second)return fail(e,"Network Remove complete census aliases a canonical key");return true;};
    for(const auto& node:graph.nodes)if(!add(node.object,node.uid))return false;
    for(const auto& pipe:graph.pipe_values)if(!add(pipe.object,pipe.uid))return false;
    auto keys=json::array();for(const auto& [key,row]:objects){(void)row;keys.push_back(key);}
    if(!region::sha256(keys.dump(),out.identities_sha256,e))return false;
    return a::valid_network_removal(out)||fail(e,"Network Remove full canonical graph census invalid");
}
uint32_t invoke_cost(uintptr_t entry,uintptr_t object,int32_t& value){
    __try{value=reinterpret_cast<int32_t(*)(const void*)>(entry)(reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool cost(region::Runtime& r,const network_identity::ObjectEntry& target,int32_t& out,std::string& e){
    const uintptr_t ap=r.image+(target.kind==network_object_identity::Kind::Node?0x16fc700:0x16fca60);
    uintptr_t getter=0;const uintptr_t expected=target.kind==network_object_identity::Kind::Node?0xd97cd0:0x4332d0;
    if(!region::at(r,ap,0x278,getter,e)||getter!=r.image+expected)return fail(e,e.empty()?"Network original Cost virtual target differs":e);
    struct Code{uintptr_t rva;size_t size;const char* sha;};
    const Code code[]{
        {0xd97cd0,16,"6689dfaa5771dc1b559da15586956e6ea8cacd2689eeeb998c85ef48fdfeac86"},
        {0x4332d0,112,"b53443e787b22d2d610189d2ffcb95fcee3705a49fb33c2f595c619ff78688cf"},
        {0x433130,416,"2041f7cf6735d722438d02ad95c83a56a379cbbe75ae537b9c988059b95140b9"}};
    for(const auto& pin:code){if((target.kind==network_object_identity::Kind::Node)!=(pin.rva==0xd97cd0))continue;
        std::string bytes(pin.size,'\0'),hash;if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),e)||!region::sha256(bytes,hash,e))return false;
        if(hash!=pin.sha)return fail(e,"Network original Cost body differs RVA="+std::to_string(pin.rva));}
    const auto seh=invoke_cost(getter,target.pointer,out);
    return (!seh&&out>=0)||fail(e,"Network original Cost returned invalid refund="+std::to_string(out)+" SEH="+std::to_string(seh));
}
bool bound_target(const a::Binding& binding,const std::string& snapshot,uintptr_t manager,const std::string& key,ui::LocalBinding& out,std::string& e){
    out={};auto* r=nr::runtime(e);if(!r)return false;network_identity::ObjectEntry entry;
    if(!network_identity::object_target(key,entry,e))return false;
    region::LocalView view;if(!current(*r,binding,snapshot,entry.region_id,view,e))return false;
    if(view.world.selection!=manager||view.region!=entry.region)return fail(e,"Network Remove SelectionManager/current Region differs");
    out={manager,entry.pointer,entry.uid,binding.native_epoch,binding.native_generation,snapshot,key,false,false,false,false,true};return true;
}
uint32_t destroy(uintptr_t image,uintptr_t manager,uintptr_t object){
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0x2d5520)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool absent(region::Runtime& r,uintptr_t manager,uintptr_t pointer,uint64_t uid,std::string& e){
    uintptr_t found=0;const auto seh=region::find(r.api.find,uid,found);
    if(seh||found)return fail(e,"Network Remove UID remains registered SEH="+std::to_string(seh));
    uintptr_t focused=0;if(!region::at(r,manager,0x80,focused,e))return false;
    if(focused==pointer)return fail(e,"Network Remove target remains focused");
    for(const auto offset:{0x20u,0x38u}){std::vector<uintptr_t> selected;
        if(!graph_reader::read::array(r.memory,manager,offset+8,offset+16,offset+20,100000,selected,e))return false;
        if(std::find(selected.begin(),selected.end(),pointer)!=selected.end())return fail(e,"Network removed object remains in SelectionManager");}
    return true;
}
bool actual_after(region::Runtime& r,Operation& op,bool retired,std::string& e){
    region::LocalView view;a::NetworkRemoval actual;std::map<std::string,network_identity::ObjectEntry> entries;a::Ledger money;
    if(!current(r,op.binding,op.request.snapshot_uuid,op.request.network_remove.region,view,e)||!census(r,view,actual,entries,e)||!region::ledger(money,e))return false;
    if(!op.evidence.network_removal_after||actual!=*op.evidence.network_removal_after||money!=op.evidence.after)return fail(e,"Network Remove complete current graph/canonical set/full Ledger differs");
    for(const auto& key:op.removed){const auto& old=op.before.at(key);if(!absent(r,op.target.manager,old.pointer,old.uid,e))return false;}
    for(const auto& [key,row]:entries){const auto old=op.before.find(key);if(old==op.before.end()||old->second!=row)return fail(e,"Network Remove surviving local full UID/owner changed");}
    op.evidence.retirement_confirmed=retired;return true;
}
}
bool local_binding(uintptr_t manager,uintptr_t object,ui::LocalBinding& out,std::string& e){
    out={};if(active_replay){if(active_replay->binding.manager!=manager||active_replay->binding.object!=object)return fail(e,"Network Remove private replay current target differs");
        if(!authorize(*active_replay,e))return false;out=active_replay->binding;return true;}
    auto* r=nr::runtime(e);if(!r)return false;uint64_t uid=0;std::string key;network_identity::ObjectEntry entry;
    if(!region::at(*r,object,0x18,uid,e)||!network_identity::object_key(object,uid,key,entry,e))return false;
    const auto snapshot=net::current_snapshot_id();a::Binding binding;if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    return bound_target(binding,snapshot,manager,key,out,e);
}
bool submit(const ui::Intent& intent,std::string& e){
    auto* r=nr::runtime(e);if(!r)return false;a::Binding binding;network_identity::ObjectEntry target;
    if(!typed_action_runtime::network_binding(intent.snapshot_uuid,binding,e)||!network_identity::object_target(intent.canonical_record_key,target,e))return false;
    region::LocalView view;a::NetworkRemoval before;std::map<std::string,network_identity::ObjectEntry> entries;int32_t refund=0;
    if(!current(*r,binding,intent.snapshot_uuid,target.region_id,view,e)||!census(*r,view,before,entries,e)||!cost(*r,target,refund,e))return false;
    return typed_action_runtime::submit_network_remove(intent.action_uuid,intent.canonical_record_key,{target.region_id,before.graph.graph_sha256,refund},e);
}
bool authorize(const ui::Replay& replay,std::string& e){
    const auto it=operations.find(replay.intent.action_uuid);const auto controller=typed_action_runtime::controller_status();
    if(!active_replay||it==operations.end()||active_replay->binding!=replay.binding||active_replay->intent.action_uuid!=replay.intent.action_uuid||
       active_replay->purpose!=replay.purpose||!replay.binding.supported_network||!it->second.attempted||it->second.returned||
       !controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=replay.intent.action_uuid||
       (replay.purpose==ui::ReplayPurpose::HostExecution)!=Config::instance().is_host)return fail(e,"Network Remove private original authority differs");
    ui::LocalBinding current;
    return bound_target(it->second.binding,it->second.request.snapshot_uuid,replay.binding.manager,it->second.request.canonical_record_key,current,e)&&current==replay.binding;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& e){
    auto* r=nr::runtime(e);if(!r)return a::Outcome::RejectedNoEffect;
    if(request.action!=a::Action::RemoveNetworkObject||operations.contains(request.action_uuid)||active_replay||!ui_remove_runtime::enabled()){
        e="Network Remove requires fresh action and enabled original DestroyProps route";return a::Outcome::RejectedNoEffect;}
    region::LocalView view;a::NetworkRemoval before;std::map<std::string,network_identity::ObjectEntry> entries;a::Ledger money;undo::UndoWitness history;
    network_identity::ObjectEntry target;
    if(!network_identity::object_target(request.canonical_record_key,target,e)||target.region_id!=request.network_remove.region||
       !current(*r,binding,request.snapshot_uuid,target.region_id,view,e)||!census(*r,view,before,entries,e)||!region::ledger(money,e)||!undo::read_undo(r->memory,r->image,history,e)||history.active_graph)return a::Outcome::RejectedNoEffect;
    int32_t refund=0;if(!cost(*r,target,refund,e)||refund!=request.network_remove.quoted_refund||before.graph.graph_sha256!=request.network_remove.graph_before_sha256){if(e.empty())e="Network Remove captured graph/refund differs";return a::Outcome::RejectedNoEffect;}
    if(host&&(!host->network_removal_before||*host->network_removal_before!=before||host->before!=money)){e="Network Remove Client complete before graph/canonical set/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    ui::LocalBinding local;if(!bound_target(binding,request.snapshot_uuid,view.world.selection,request.canonical_record_key,local,e))return a::Outcome::RejectedNoEffect;
    connected_network_native::Graph original_graph;a::NetworkState original_state;network_history_runtime::Snapshot history_before;
    if(!nr::graph(*r,view,original_graph,original_state,e)||!network_history_runtime::capture(original_graph,{},history_before,e))return a::Outcome::RejectedNoEffect;
    auto& op=operations[request.action_uuid];op.binding=binding;op.request=request;op.before=entries;op.target=local;op.evidence.before=money;op.evidence.network_removal_before=before;
    auto unknown=[&](){if(e.empty())e="Network Remove native result is Unknown";op.cause=e;LOG_ERROR("[NetworkRemove] action={} cause={}",request.action_uuid,e);return a::Outcome::Unknown;};
    ui::Replay replay{host?ui::ReplayPurpose::ConfirmedHostApplication:ui::ReplayPurpose::HostExecution,{request.action_uuid,request.snapshot_uuid,request.canonical_record_key},local};
    const auto calls=ui_remove_runtime::status().value("replay_originals",uint64_t{});op.attempted=true;bool ok=false;
    {active_replay=&replay;struct Reset{~Reset(){active_replay=nullptr;}} reset;
        ok=ui_remove_runtime::with_replay(replay,[&](std::string& cause){const auto seh=destroy(r->image,local.manager,local.object);return !seh||fail(cause,"Network original DestroyProp SEH="+std::to_string(seh));},e);}
    const auto after_calls=ui_remove_runtime::status().value("replay_originals",uint64_t{});op.original_calls=after_calls>=calls?after_calls-calls:0;
    if(!ok||op.original_calls!=1)return unknown();op.returned=true;
    region::LocalView after_view;a::NetworkRemoval after;std::map<std::string,network_identity::ObjectEntry> survivors;undo::UndoWitness after_history;
    if(!current(*r,binding,request.snapshot_uuid,target.region_id,after_view,e)||!census(*r,after_view,after,survivors,e)||!region::ledger(op.evidence.after,e)||!undo::read_undo(r->memory,r->image,after_history,e))return unknown();
    if(!a::network_remove_transition(request.canonical_record_key,request.network_remove,before,after)||survivors.contains(request.canonical_record_key)){e="Network Remove original canonical topology transition differs";return unknown();}
    if(after_history.undo.empty()){e="Network Remove original inverse graph absent";return unknown();}undo::GraphWitness inverse;
    if(!undo::read_graph(r->memory,r->image,after_history.undo.back(),inverse,e)||!undo::validate_undo_commit(history,after_history,inverse,e)||inverse.pending_delete||inverse.nodes||!inverse.operations)return unknown();
    for(const auto& [key,row]:entries)if(!survivors.contains(key)){if(!absent(*r,local.manager,row.pointer,row.uid,e))return unknown();op.removed.push_back(key);}
    op.evidence.network_removal_after=after;op.evidence.target_retired=true;op.evidence.undo_confirmed=true;op.evidence.selection_confirmed=true;
    if(host&&(host->network_removal_after!=op.evidence.network_removal_after||host->after!=op.evidence.after||!host->undo_confirmed||!host->target_retired||!host->selection_confirmed)){e="Network Remove original Client result differs from Host";return unknown();}
    if(!actual_after(*r,op,false,e))return unknown();
    connected_network_native::Graph final_graph;a::NetworkState final_state;network_history_runtime::Snapshot history_after;
    if(!nr::graph(*r,after_view,final_graph,final_state,e)||!network_history_runtime::capture(final_graph,{},history_after,e)||!network_history_runtime::record(binding,request,history_before,history_after,after_history.undo.back(),request.network_remove.quoted_refund,e))return unknown();op.confirmed=true;return a::Outcome::Confirmed;
}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto it=operations.find(request.action_uuid);if(it==operations.end()||it->second.binding!=binding||it->second.request!=request||!it->second.confirmed)return fail(e,"Network Remove observation lacks confirmed exact request");
    auto* r=nr::runtime(e);if(!r||!actual_after(*r,it->second,it->second.retired,e))return false;out=it->second.evidence;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){
    const auto it=operations.find(request.action_uuid);if(it==operations.end()||it->second.binding!=binding||it->second.request!=request||!it->second.confirmed||it->second.retired)return fail(e,"Network Remove retirement lacks fresh confirmed exact request");
    auto* r=nr::runtime(e);if(!r||!actual_after(*r,it->second,false,e))return false;
    for(const auto& key:it->second.removed){const auto& old=it->second.before.at(key);if(!network_identity::retire_object(key,old.pointer,old.uid,e))return false;}
    if(!network_history_runtime::retire_source(binding,request,e))return false;
    it->second.retired=true;it->second.evidence.retirement_confirmed=true;return true;
}
bool unresolved(){for(const auto& [key,op]:operations){(void)key;if(!op.retired)return true;}return false;}
json status(){json out=json::object();for(const auto& [id,op]:operations)out[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"original_calls",op.original_calls},{"local_uid",op.target.uid},{"removed",op.removed},{"cause",op.cause},
    {"before",op.evidence.network_removal_before?json{{"graph_sha256",op.evidence.network_removal_before->graph.graph_sha256},{"identities_sha256",op.evidence.network_removal_before->identities_sha256}}:json(nullptr)},
    {"after",op.evidence.network_removal_after?json{{"graph_sha256",op.evidence.network_removal_after->graph.graph_sha256},{"identities_sha256",op.evidence.network_removal_after->identities_sha256}}:json(nullptr)}};return out;}
}
