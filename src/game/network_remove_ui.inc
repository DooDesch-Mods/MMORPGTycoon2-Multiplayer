#include "ui_remove/diagnostic/ui_diagnostic.h"
namespace mt2mp::game::network_remove_runtime {
namespace {
namespace diagnostic=a::ui_diagnostic;
struct ProbeTarget {a::Binding binding;a::Context context;network_identity::ObjectEntry target;};
std::map<std::string,ProbeTarget> probe_targets;
struct ProbeMemory final:ui::Memory {
    region::Runtime& owner;
    explicit ProbeMemory(region::Runtime& value):owner(value){}
    bool copy(uintptr_t address,void* out,size_t size,std::string& e)override{return owner.memory.copy(address,out,size,e);}
    bool region(uintptr_t,ui::Region&,std::string& e)override{return fail(e,"Network UI diagnostic does not use executable-page inspection");}
};
bool probe_current(const ProbeTarget& target,const diagnostic::Request& request,std::string& e){
    a::Context context;a::Binding binding;network_identity::ObjectEntry object;
    if(!a::selected_session().current(context,e)||!typed_action_runtime::read_binding(request.snapshot_uuid,binding,e)||
       !network_identity::object_target(request.canonical_record_key,object,e))return false;
    return (context==target.context&&binding==target.binding&&object==target.target&&context.snapshot_uuid==request.snapshot_uuid&&
        net::current_snapshot_id()==request.snapshot_uuid&&Config::instance().is_host==(context.role==a::Role::Host))||fail(e,"Network Remove queued exact Context/Binding/target changed");
}
class ProbeServices final:public diagnostic::RootServices {
    region::Runtime& r;const ProbeTarget& owner;diagnostic::Request request;
    a::NetworkRemoval before;a::Ledger money;undo::UndoWitness history;
    std::map<std::string,network_identity::ObjectEntry> entries;
    uint64_t captures=0,shared_captures=0,attempts=0;
public:
    std::string action;
    ProbeServices(region::Runtime& value,const ProbeTarget& target):r(value),owner(target){}
    bool unchanged(std::string& e){
        region::LocalView view;a::NetworkRemoval actual;a::Ledger after;undo::UndoWitness undo_after;
        std::map<std::string,network_identity::ObjectEntry> objects;
        if(!probe_current(owner,request,e)||!current(r,owner.binding,request.snapshot_uuid,owner.target.region_id,view,e)||
           !census(r,view,actual,objects,e)||!region::ledger(after,e)||!undo::read_undo(r.memory,r.image,undo_after,e))return false;
        return (actual==before&&objects==entries&&after==money&&undo_after==history)||fail(e,"Network Remove UI changed full graph/identity/Ledger/Undo before consumed execution");
    }
    bool preflight(const diagnostic::Request& value,diagnostic::Context& out,std::string& e)override{
        request=value;
        if(!Config::instance().local_test||!ui_remove_runtime::enabled()||!nr::input_admission(e)||!probe_current(owner,request,e))return fail(e,e.empty()?"Network Remove requires owned actual OriginalGo capture":e);
        const auto shared=ui_remove_runtime::status();const auto controller=typed_action_runtime::status();
        if(shared.value("unknown",true)||shared.value("replay_active",true)||pending||capture_fault)return fail(e,"Network Remove UI capture is unresolved or Unknown");
        region::LocalView view;
        if(!current(r,owner.binding,request.snapshot_uuid,owner.target.region_id,view,e)||!census(r,view,before,entries,e)||
           !region::ledger(money,e)||!undo::read_undo(r.memory,r.image,history,e))return false;
        if(history.active_graph)return fail(e,"Network Remove UI requires idle original Undo graph");
        uintptr_t cursor=0,ap=0,active=0,desired=0;uint8_t permits=0;
        if(!region::at(r,r.image,0x13df9d0,cursor,e)||!region::at(r,cursor,0,ap,e)||ap!=r.image+0x1786dd0||
           !region::at(r,cursor,0x190,active,e)||!region::at(r,cursor,0x198,desired,e)||!desired||active!=desired||
           !region::at(r,desired,0,ap,e)||ap!=r.image+0x17391a0||!region::at(r,desired,0x120,permits,e)||permits!=1)
            return fail(e,e.empty()?"Network Remove selection requires original Default cursor":e);
        uint32_t thread=0;std::string hash;uintptr_t image=0;
        if(!native::read_resolver_receipt(image,thread,hash,e)||image!=r.image||thread!=r.thread)return fail(e,e.empty()?"Network Remove native resolver identity differs":e);
        captures=captured;shared_captures=shared.at("captured").get<uint64_t>();attempts=controller.at("attempted").get<uint64_t>();
        out={r.image,view.world.selection,owner.target.pointer,owner.target.uid,owner.binding.native_epoch,owner.binding.native_generation,hash,
            owner.target.kind==network_object_identity::Kind::Node?diagnostic::Family::NetworkNode:diagnostic::Family::NetworkPipe};
        return true;
    }
    bool preparation_unchanged(const diagnostic::Context& context,const diagnostic::Window& window,std::string& e)override{
        return (context.target==owner.target.pointer&&window.target==context.target&&captured==captures&&!pending&&unchanged(e))||
            fail(e,e.empty()?"Network Remove UI preparation captured an unexpected action":e);
    }
    bool observed_intent(const diagnostic::Request& value,const diagnostic::Context&,std::string& e)override{
        const auto shared=ui_remove_runtime::status();const auto controller=typed_action_runtime::status();
        if(captured!=captures+1||!pending||pending->target!=owner.target||pending->binding!=owner.binding||pending->context!=std::optional<a::Context>(owner.context)||
           pending->intent.snapshot_uuid!=value.snapshot_uuid||pending->intent.canonical_record_key!=value.canonical_record_key||pending->intent.action_uuid.empty()||
           shared.at("captured").get<uint64_t>()!=shared_captures+1||shared.value("unknown",true)||shared.value("replay_active",true)||
           shared.at("last_origin").get<uint8_t>()!=uint8_t(ui::Origin::NetworkWindow)||
           shared.at("last_disposition").get<uint8_t>()!=uint8_t(ui::Disposition::IntentCaptured)||
           shared["last_intent"]["action_uuid"]!=pending->intent.action_uuid||controller.at("attempted").get<uint64_t>()!=attempts)
            return fail(e,"Network original window did not capture one exact pending Remove intent");
        if(!unchanged(e))return false;action=pending->intent.action_uuid;return true;
    }
    void log(const char* stage,const std::string& e)noexcept override{LOG_ERROR("[NetworkRemoveUI] stage={} cause={}",stage,e);}
};
json probe_receipt(const diagnostic::Receipt& r){return {{"selection_attempted",r.selection_attempted},{"selection_returned",r.selection_returned},{"window_confirmed",r.window_confirmed},
    {"record_constructed",r.record_constructed},{"record_label_confirmed",r.record_label_confirmed},{"command_attempted",r.ui_command_attempted},
    {"command_returned",r.ui_command_returned},{"command_accepted",r.ui_command_accepted},{"cleanup_attempted",r.record_cleanup_attempted},
    {"cleanup_confirmed",r.record_cleanup_confirmed},{"intent_observed",r.intent_observed},{"native_exception",r.native_exception},{"cleanup_exception",r.cleanup_exception},
    {"failed_stage",r.failed_stage},{"cause",r.cause}};}
}
bool probe(const json& args,json& out,std::string& e){
    if(!args.is_object()||args.size()!=3||!args.contains("diagnostic_uuid")||!args.contains("snapshot_uuid")||!args.contains("record_key")||
       !args["diagnostic_uuid"].is_string()||!args["snapshot_uuid"].is_string()||!args["record_key"].is_string())return fail(e,"Network Remove UI requires exact scalar diagnostic UUID, snapshot UUID and canonical key");
    const diagnostic::Request request{args["diagnostic_uuid"],args["snapshot_uuid"],args["record_key"]};
    if(!network_object_identity::uuid(request.diagnostic_uuid)||!network_object_identity::uuid(request.snapshot_uuid)||!network_object_identity::key(request.canonical_record_key)||
       probe_targets.contains(request.diagnostic_uuid))return fail(e,"Network Remove UI key/token invalid or already attempted - no replay");
    ProbeTarget target;
    if(!a::selected_session().current(target.context,e)||!typed_action_runtime::read_binding(request.snapshot_uuid,target.binding,e)||
       !network_identity::object_target(request.canonical_record_key,target.target,e)||!probe_current(target,request,e))return false;
    if(!nr::ordered_ui::enqueue(args,false,out,e,true))return false;
    probe_targets.emplace(request.diagnostic_uuid,std::move(target));return true;
}
bool original_ui(const json& args,json& out,std::string& e){
    const std::string token=args.at("diagnostic_uuid");const auto found=probe_targets.find(token);
    if(found==probe_targets.end())return fail(e,"Network Remove original input lacks its admitted target");
    auto* r=nr::runtime(e);if(!r)return false;
    const diagnostic::Request request{token,args.at("snapshot_uuid"),args.at("record_key")};
    ProbeMemory memory(*r);ProbeServices services(*r,found->second);diagnostic::Receipt receipt;
    bool accepted=false;
    try{accepted=diagnostic::invoke(request,services,memory,diagnostic::process_calls(),receipt,e);}
    catch(const std::exception& problem){e=problem.what();receipt.cause=e;receipt.failed_stage="root_exception";services.log("root_exception",e);}
    catch(...){e="Network Remove UI nonstandard exception";receipt.cause=e;receipt.failed_stage="root_exception";services.log("root_exception",e);}
    out={{"diagnostic_uuid",token},{"snapshot_uuid",request.snapshot_uuid},{"record_key",request.canonical_record_key},{"confirmed",accepted},
        {"action_uuid",services.action},{"receipt",probe_receipt(receipt)},{"local_state_unchanged",accepted},{"captured_once",accepted},{"cause",e}};
    return accepted;
}
}
