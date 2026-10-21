namespace modular {
namespace {
namespace q=npc_quest_targets;namespace calls=modular_calls;namespace value=modular_transition;
using Sample=scenery_stroke::Sample;using Family=action::DuplicateFamily;
using Building=duplicate_building_history::Saved;
using Scenery=scenery_make_history::Saved;
bool installed=false,installation_attempted=false;std::array<uintptr_t,2> originals{};
std::array<release_ui_owner::Owner,2> route_owners;
uint64_t captures=0,submitted=0,rejected=0;std::string cause;
void reject(const std::string& e){++rejected;if(cause!=e){cause=e;LOG_ERROR("[Modular] {}",e);}}
bool code(Runtime& r,std::string& e){for(const auto& guard:modular_code::guards){std::vector<unsigned char> bytes(guard.bytes);if(!r.memory.read(r.image+guard.rva,bytes.data(),bytes.size(),e))return false;
    const auto owner=std::find_if(route_owners.begin(),route_owners.end(),[&](const auto& row){return row.entry==r.image+guard.rva&&row.trampoline;});if(owner!=route_owners.end()){auto copy=[&](uintptr_t p,void* out,size_t count,std::string& why){return r.memory.read(p,out,count,why);};auto hash=[](const auto& value){return mt2mp::sha256(value);};if(!release_ui_owner::verify(*owner,copy,hash,guard.sha,e))return false;
#ifdef MT2MP_FRAMEWORK_PLUGIN
        MT2FW_MT2_RouteReceipt receipt{};auto& backend=hooks::framework::process_backend();if(!backend.read_route(reinterpret_cast<void*>(owner->entry),receipt)||receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.entry!=owner->entry||receipt.detour!=owner->detour||receipt.trampoline!=owner->trampoline||receipt.generation!=owner->generation)return q::fail(e,"Modular current named route owner/generation differs: "+backend.error());
#endif
        continue;}
    if(!std::memcmp(bytes.data(),guard.original,bytes.size()))continue;
#ifdef MT2MP_FRAMEWORK_PLUGIN
    if(hooks::framework::verify_code(r.image+guard.rva,guard.original,guard.bytes,e))continue;
#endif
    return q::fail(e,e.empty()?"Modular original complete native body differs RVA="+std::to_string(guard.rva):e);
    }return true;}
bool call(Runtime& r,calls::Work& work,std::string& e){work.image=r.image;uint32_t exception=0;{live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);exception=calls::invoke(work);}if(exception||work.cleanup||!work.returned)return q::fail(e,"Modular original operation="+std::to_string(int(work.op))+" SEH="+std::to_string(exception)+" cleanup="+std::to_string(work.cleanup)+"; no replay");return true;}
bool owned_record(Runtime& r,uintptr_t object,bool plan,json& records,std::string& digest,std::string& e){
    duplicate_building_values::Methods methods;if(!duplicate_building_values::methods(plan?duplicate_building_values::OwnedKind::Plan:duplicate_building_values::OwnedKind::Layout,methods,e)||!q::typed(r.memory,r.image,object,methods.addresspoint,e))return false;
    duplicate_edit::GraphMemory memory(r.memory);duplicate_edit::RecordCapture captured{memory};subscriber_lifecycle::CaptureReceipt receipt;
    const subscriber_lifecycle::CaptureApi api{reinterpret_cast<subscriber_lifecycle::Construct>(r.image+0x9667c0),reinterpret_cast<subscriber_lifecycle::Construct>(r.image+0x9676a0),reinterpret_cast<subscriber_lifecycle::Construct>(r.image+0x89e580),reinterpret_cast<subscriber_lifecycle::Construct>(r.image+0x89e5f0),reinterpret_cast<subscriber_lifecycle::PrepareSave>(r.image+methods.prepare),reinterpret_cast<subscriber_lifecycle::SaveRecord>(r.image+methods.save)};
    {live_host_state::apply_timing::PageReadScope boundary(live_host_state::apply_timing::PageBoundary::OriginalCall);if(!subscriber_lifecycle::capture_original(api,reinterpret_cast<void*>(object),duplicate_edit::inspect_record,&captured,receipt,e))return false;}
    if(!duplicate_owned_history::captured(captured.graph,digest,e))return false;records=value::records_json(captured.graph.records);std::vector<subscriber_lifecycle::Record> rows;return value::records(records,plan,rows,e);
}
bool module_sample(Runtime& r,const action::Binding& binding,int32_t region,const std::map<uint64_t,std::string>& aliases,Sample& out,std::string& e){return scenery_stroke::sample(r,binding,region,aliases,out,e);}
bool module_graph(Runtime& r,uintptr_t graph,uint64_t uid,const Building& expected,std::string& e,bool sorted=true){auto record=[&](uintptr_t object,bool plan,std::string& sha,std::string& why){json rows;return owned_record(r,object,plan,rows,sha,why);};return modular_layout_graph::read(r.memory,r.image,graph,uid,expected,record,e,sorted);}
struct Pending {action::Binding binding;action::ModularIntent intent;std::string snapshot,id;};
std::optional<Pending> queued;
struct Port;
struct Operation {
    action::Binding binding;action::Request request;action::Evidence evidence;modular_transaction::Receipt receipt;
    std::shared_ptr<Port> port;uintptr_t graph=0;bool confirmed=false,retired=false;std::string cause;
};
std::map<std::string,Operation> operations,quarantined;
}
}
