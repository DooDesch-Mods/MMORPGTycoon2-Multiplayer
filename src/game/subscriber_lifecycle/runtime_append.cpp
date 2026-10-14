#include "runtime_append.h"
#include "building_reference_pins.h"
#include "append_calls.h"
#include "client_world.h"
#include "payload.h"
#include "save_scope.h"
#include "additions.h"
#include "live_host_state/scalar_store.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/core/hash.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <Windows.h>
namespace mt2mp::game::subscriber_lifecycle_append {
namespace {
namespace local=mt2mp::subscriber_lifecycle;
namespace stage=local::first_append;
using Json=nlohmann::json;
std::atomic_flag busy=ATOMIC_FLAG_INIT;
std::atomic<bool> hook_ready=false;
uintptr_t image_base=0;stage::AppendApi api;
uint64_t blocked_epoch=0,blocked_generation=0;std::string blocked_cause;
local::LookupScope lookup_scope;
class ProcessMemory final:public local::Memory{public:bool read(uintptr_t p,void* v,size_t n,std::string& e)override{
    if(!p||p>UINTPTR_MAX-n){e="Subscriber append memory extent invalid";return false;}SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),v,n,&copied)||copied!=n){e="Subscriber append ReadProcessMemory error="+std::to_string(GetLastError());return false;}return true;
}} memory;
bool fail(std::string& e,const std::string& s){e=s;return false;}
std::string hash(const std::string& s){return mt2mp::sha256(std::vector<unsigned char>(s.begin(),s.end()));}
template<class T>bool at(uintptr_t p,size_t o,T& v,std::string& e){if(!p||p>UINTPTR_MAX-o)return fail(e,"Subscriber append field extent invalid");return memory.read(p+o,&v,sizeof v,e);}
bool filename(const std::string& s){if(s.size()<6||s.size()>160||s.substr(s.size()-5)!=".json")return false;return std::all_of(s.begin(),s.end()-5,[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-';});}
bool finances_equal(const native::SimulationState& a,const native::SimulationState& b){return a.cash==b.cash&&a.record_present==b.record_present&&a.record_day==b.record_day&&a.record_cash==b.record_cash&&a.record_income==b.record_income&&a.record_expense==b.record_expense&&a.line_counts==b.line_counts&&a.line_income==b.line_income;}
struct Work{
    stage::ClientWorld original,held;
    stage::Payload payload;
    subscriber_building_references::Pins building_pins;
    std::string origin,local_sha;
    const FrameBinding* frame=nullptr;
    Json* receipt=nullptr;
    std::vector<local::ExternalLink> links;
    std::vector<std::pair<stage::ReferenceKey,uintptr_t>> keys;
    bool effect=false,unknown=false;
    uintptr_t region=0,toon_manager=0;
    std::vector<uintptr_t> old_toons;
};
bool current_frame(const Work& w,std::string& e){
    if(!w.frame)return true;
    action_session::Context current;
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!action_session::selected_session().current(current,e)||!native_open_runtime::read_current_world(world,e))return false;
    const auto& f=*w.frame;
    if(current!=f.context||current.role!=action_session::Role::Client||net::current_world_epoch()!=f.common_epoch||world.world_epoch!=f.binding.native_epoch||world.generation!=f.binding.native_generation||world.uuid!=w.origin||world.sha256!=w.local_sha)return fail(e,"Subscriber automatic current shared/local origin binding differs");
    return true;
}
bool read_world(Work& w,bool held,stage::ClientWorld& out,std::string& e){
    return current_frame(w,e)&&stage::read_client_world(memory,w.local_sha,w.origin,held,out,e);
}
bool fresh_link(void* user,const local::ExternalLink& link,std::string& e){
    auto& w=*static_cast<Work*>(user);
    if(!current_frame(w,e))return false;
    const stage::ReferenceWorld world{w.held.image,reinterpret_cast<uintptr_t>(w.held.origin.state),reinterpret_cast<uintptr_t>(w.held.origin.map),&w.building_pins};
    for(size_t i=0;i<w.links.size();++i)if(w.links[i].saved_link_id==link.saved_link_id){uintptr_t p=0;return stage::resolve_reference(memory,world,w.keys[i].first,p,e)&&p==link.object||fail(e,e.empty()?"Subscriber external owner changed during native Resolve":e);}
    return fail(e,"Subscriber external link not declared");
}
bool toon_array(Work& w,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t manager=0,vt=0,data=0;int32_t count=-1,capacity=-1;uint8_t updating=1;
    if(!at(w.region,0x9c0,manager,e)||!at(manager,0,vt,e)||vt!=w.held.image+0x16fde30||!at(manager,0xc8,updating,e))return fail(e,e.empty()?"Subscriber Region ToonManager owner/addresspoint differs":e);
    if(updating)return fail(e,"Subscriber Region ToonManager is updating; append not admitted");
    if(w.toon_manager&&w.toon_manager!=manager)return fail(e,"Subscriber Region ToonManager receiver changed");w.toon_manager=manager;
    if(!at(manager,0x28,data,e)||!at(manager,0x30,count,e)||!at(manager,0x34,capacity,e))return false;
    if(count<0||capacity<count||(count&&!data)||size_t(count)>(UINTPTR_MAX-data)/8)return fail(e,"Subscriber ToonManager array invalid");
    out.clear();std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t p=0;if(!at(data,size_t(i)*8,p,e))return false;if(!p||!unique.insert(p).second)return fail(e,"Subscriber ToonManager contains null/duplicate");out.push_back(p);}return true;
}
bool exit_guard(void* user,std::string& e){
    auto& w=*static_cast<Work*>(user);stage::ClientWorld current;
    if(!read_world(w,true,current,e))return false;
    return (stage::same_client_owner(w.original,current)||fail(e,"Subscriber append owner changed before original SaveScope leave"))&&w.building_pins.verify_used(e);
}
struct Resources {
    stage::NativeTree record,next;
    stage::DecodeReceipt decoded,next_decoded;
    uintptr_t context=0;
    std::unique_ptr<Resources> retained_next;
};
std::unique_ptr<Resources> retained_resources;
bool body_inner(Work& w,Resources& owners,std::string& e){
    auto& result=*w.receipt;
    auto& record=owners.record;auto& next=owners.next;
    auto& decoded=owners.decoded;auto& next_decoded=owners.next_decoded;
    auto* context=&owners.context;
    if(!read_world(w,true,w.held,e)||!stage::same_client_owner(w.original,w.held))return fail(e,e.empty()?"Subscriber append owner changed entering SaveScope":e);
    const auto before=w.held.census;
    if(w.payload.next_id<before.next_id)return fail(e,"Subscriber Host nextID moved backwards");
    for(const auto& member:before.members)if(member.id==w.payload.id)return fail(e,"Subscriber already present; no C1 replay");
    const stage::ReferenceWorld world{w.held.image,reinterpret_cast<uintptr_t>(w.held.origin.state),reinterpret_cast<uintptr_t>(w.held.origin.map),&w.building_pins};
    for(const auto& ref:w.payload.external){uintptr_t object=0;
        if(!stage::resolve_reference(memory,world,ref.key,object,e))return false;
        const auto name=std::to_string(int(ref.key.kind))+":"+std::to_string(ref.key.region_id)+":"+std::to_string(ref.key.character_kind)+":"+std::to_string(ref.key.character_id)+":"+std::to_string(ref.key.ability_index)+":"+ref.key.building_key+(ref.key.kind==stage::ReferenceKind::SubscriberToon?":"+std::to_string(ref.key.subscriber_id):"");
        w.links.push_back({ref.local_link_id,name,object});w.keys.push_back({ref.key,object});
        if(ref.key.kind==stage::ReferenceKind::RootRegion){if(w.region&&w.region!=object)return fail(e,"single Subscriber probe spans multiple root Regions");w.region=object;}
    }
    if(!w.payload.online||!w.region)return fail(e,"first native append probe requires captured online Toon and root Region");
    if(!toon_array(w,w.old_toons,e))return false;
    native::SimulationState money_before;if(!native::read_simulation_state(money_before,e))return false;
    if(!stage::decode_records(api.records,memory,w.payload.records,record,decoded,e)){
        w.unknown=decoded.unknown;return false;}
    if(!stage::decode_integer_record(api.records,memory,"nextId",w.payload.next_id,next,next_decoded,e)){w.unknown=next_decoded.unknown;return false;}
    local::CallReceipt ctor,load,next_call,dtor,login;
    if(!stage::context_call(api.context_construct,context,ctor,e)){w.unknown=true;return false;}
    if(!lookup_scope.arm(w.held.thread,context,w.payload.local_ids,w.links,fresh_link,&w,e)){
        local::CallReceipt abort,cleanup;std::string cause;
        if(!stage::context_call(api.context_abort,context,abort,cause)||!stage::context_call(api.context_destroy,context,cleanup,cause)){w.unknown=true;e+="; "+cause;}return false;
    }
    w.effect=true;
    bool ok=local::load_original(api.calls,reinterpret_cast<void*>(w.held.manager+0x38),reinterpret_cast<void*>(record.root),context,load,e);
    if(ok)ok=local::next_id_original(api.calls,reinterpret_cast<void*>(w.held.image+local::next_id_property_rva),reinterpret_cast<void*>(w.held.manager),reinterpret_cast<void*>(next.root),context,next_call,e);
    if(ok)ok=stage::context_call(api.context_destroy,context,dtor,e);
    const auto lookup_result=lookup_scope.receipt();lookup_scope.clear();
    result["native_calls"]={{"context_constructed",ctor.returned},{"array_load_attempted",load.attempted},{"array_load_returned",load.returned},{"next_id_returned",next_call.returned},{"context_resolve_destroy_attempted",dtor.attempted},{"context_resolve_destroy_returned",dtor.returned},{"lookup_original_calls",lookup_result.original_calls},{"external_lookup_hits",lookup_result.external_hits},{"lookup_failed",lookup_result.failed},{"lookup_cause",lookup_result.cause}};
    if(!ok||lookup_result.failed){w.unknown=true;if(lookup_result.failed)e+=(e.empty()?"":"; ")+lookup_result.cause;return false;}
    local::Census after;
    if(!stage::read_cohort(memory,w.held,after,e,false)){w.unknown=true;return false;}
    if(after.members.size()!=before.members.size()+1||after.next_id!=w.payload.next_id){w.unknown=true;return fail(e,"native Subscriber append count/nextID differs");}
    for(size_t i=0;i<before.members.size();++i)if(before.members[i]!=after.members[i]){w.unknown=true;return fail(e,"native append changed existing Subscriber graph");}
    const auto added=after.members.back();
    if(added.id!=w.payload.id||!added.online||!added.toon||!added.brain){w.unknown=true;return fail(e,"new native Subscriber/Toon/Brain readback differs");}
    uint64_t local_uid=0;uintptr_t registry_object=0;
    if(!at(added.toon,0x18,local_uid,e)||!local_uid||!stage::registry_lookup(w.held.image,local_uid,registry_object,e)||registry_object!=added.toon){w.unknown=true;return fail(e,e.empty()?"new Toon local UID registry identity differs":e);}
    if(std::find(after.online.begin(),after.online.end(),added.id)!=after.online.end()||std::find(after.offline.begin(),after.offline.end(),added.id)!=after.offline.end()){
        w.unknown=true;return fail(e,"new native Subscriber unexpectedly already classified; original login not replayed");}
    if(!local::classify_original(api.calls,reinterpret_cast<void*>(w.held.manager),reinterpret_cast<void*>(added.subscriber),true,login,e)){w.unknown=true;return false;}
    result["native_calls"]["notify_login_returned"]=login.returned;
    std::vector<uintptr_t> toons;uintptr_t region=0,last_region=0,shard=0;
    if(!toon_array(w,toons,e)||!at(added.toon,0x178,region,e)||!at(added.toon,0x9a8,last_region,e)||!at(added.toon,0x170,shard,e)){w.unknown=true;return false;}
    auto expected=w.old_toons;expected.push_back(added.toon);
    if(toons!=expected||region!=w.region||last_region!=w.region||shard!=world.map+0x1c0){w.unknown=true;return fail(e,"native Toon Region registration differs");}
    if(!stage::read_cohort(memory,w.held,after,e)){w.unknown=true;return false;}
    native::SimulationState money_after;native::SubscriberNeedsState needs;
    if(!native::read_simulation_state(money_after,e)||!native::read_subscriber_needs(added.id,needs,e)||!finances_equal(money_before,money_after)){w.unknown=true;return fail(e,e.empty()?"native append changed business finance":e);}
    for(size_t i=0;i<needs.needs.size();++i)if(std::bit_cast<uint32_t>(needs.needs[i])!=w.payload.needs_bits[i]){
        w.unknown=true;return fail(e,"native Subscriber Need readback differs at index="+std::to_string(i));}
    result["toon_local_uid_registry_confirmed"]=true;
    result["source_needs_bitwise_confirmed"]=true;
    result.update({{"subscriber_count_before",before.members.size()},{"subscriber_count_after",after.members.size()},{"next_subscriber_id",after.next_id},{"online",needs.logged_in},{"needs",needs.needs},{"original_cohort_unchanged",true},{"toon_region_registered",true},{"business_unchanged",true},{"source_record_sha256",w.payload.record_sha}});
    return true;
}
bool body(void* user,std::string& e){
    auto& w=*static_cast<Work*>(user);
    auto owners=std::make_unique<Resources>();
    bool ok=false;
    try{ok=body_inner(w,*owners,e);}
    catch(const std::exception& ex){e=std::string("Subscriber append body C++: ")+ex.what();w.unknown=true;}
    catch(...){e="Subscriber append body unknown C++ exception";w.unknown=true;}
    const bool keep=w.unknown||(!ok&&w.effect)||owners->decoded.unknown||owners->next_decoded.unknown;
    if(!keep){
        std::string cleanup;
        if(!stage::destroy_records(api.records,owners->next,owners->next_decoded,cleanup)){
            e+=(e.empty()?"":"; ")+cleanup;w.unknown=true;ok=false;
        }
        if(!w.unknown&&!stage::destroy_records(api.records,owners->record,owners->decoded,cleanup)){
            e+=(e.empty()?"":"; ")+cleanup;w.unknown=true;ok=false;
        }
    }else w.unknown=true;
    (*w.receipt)["native_record_cleanup_confirmed"]=owners->decoded.cleaned&&owners->next_decoded.cleaned;
    (*w.receipt)["private_owners_retained"]=w.unknown;
    (*w.receipt)["private_record_root_owners_retained"]=w.unknown?owners->record.detached.size()+owners->next.detached.size():0;
    if(w.unknown){owners->retained_next=std::move(retained_resources);retained_resources=std::move(owners);}
    return ok&&!w.unknown;
}
}
bool initialize(uintptr_t image,std::string& e){if(image_base)return image_base==image||fail(e,"Subscriber append image changed");if(!stage::resolve_append_api(memory,image,api,e))return false;image_base=image;return true;}
void* lookup_entrypoint(){return image_base?reinterpret_cast<void*>(image_base+local::lookup_rva):nullptr;}
void set_lookup_hook_ready(bool ready){hook_ready=ready;}
void* lookup(void* context,int32_t id,local::Lookup original){return lookup_scope.lookup(GetCurrentThreadId(),context,id,original);}
// QUEST_LINK_BODY_BEGIN
namespace {
struct QuestWeak {
    uintptr_t target=0,proxy=0,proxy_target=0;
    int32_t id=0;
    bool operator==(const QuestWeak&) const=default;
};
struct QuestLinkWork {
    Work world;
    QuestLinkRequest request;
    QuestLinkReceipt* receipt=nullptr;
    uintptr_t context=0;
    bool armed=false,unknown=false;
    std::unique_ptr<QuestLinkWork> retained_next;
};
std::unique_ptr<QuestLinkWork> retained_quest_links;
bool quest_weak(uintptr_t instance,QuestWeak& out,std::string& e){
    uintptr_t object_ap=0,link_ap=0,weak_ap=0;
    if(!instance||instance>UINTPTR_MAX-0x58||
       !at(instance,0,object_ap,e)||!at(instance,0x18,link_ap,e)||!at(instance,0x38,weak_ap,e))
        return fail(e,e.empty()?"Quest link Instance extent/read failed":e);
    if(object_ap!=image_base+0x17087e0||link_ap!=image_base+0x1711c30||weak_ap!=image_base+0x16f9f20)
        return fail(e,"Quest link Instance/Weak addresspoints differ");
    if(!at(instance,0x30,out.id,e)||!at(instance,0x40,out.target,e)||!at(instance,0x48,out.proxy,e))return false;
    if(!out.proxy)return !out.target||fail(e,"Quest link target without Proxy");
    int32_t references=0;
    if(!at(out.proxy,0,out.proxy_target,e)||!at(out.proxy,8,references,e))return false;
    if(references<=0)return fail(e,"Quest link Proxy refcount not positive");
    // An expired Proxy may still leave a stale target value. Never dereference it.
    if(!out.proxy_target)return true;
    uintptr_t ap=0,proxy=0;
    if(!out.target||out.target>UINTPTR_MAX-0x28||out.proxy_target!=out.target+0x18)
        return fail(e,"Quest link live Proxy target differs");
    if(!at(out.target,0,ap,e)||!at(out.target,0x20,proxy,e))return false;
    return (ap==image_base+0x174ac10&&proxy==out.proxy)||fail(e,"Quest link live Quest/Proxy binding differs");
}
bool quest_fresh(void* user,const local::ExternalLink& link,std::string& e){
    auto& w=*static_cast<QuestLinkWork*>(user);
    stage::ClientWorld current;
    if(!read_world(w.world,true,current,e))return false;
    if(!stage::same_client_owner(w.world.original,current))return fail(e,"Quest link current Client owner changed");
    if(link.saved_link_id!=(w.request.quest?1:0)||link.object!=w.request.quest||link.portable_key!=w.request.portable_key)
        return fail(e,"Quest link borrowed request differs");
    if(!w.request.fresh(w.request.user,link,e))return fail(e,e.empty()?"Quest link fresh owner rejected without cause":e);
    return true;
}
bool quest_close(QuestLinkWork& w,bool abort,std::string& e){
    auto& r=*w.receipt;
    std::string cleanup;
    if(abort&&!stage::context_call(api.context_abort,&w.context,r.abort,cleanup)){
        w.unknown=true;e+=(e.empty()?"":"; ")+cleanup;return false;
    }
    if(!stage::context_call(api.context_destroy,&w.context,r.destroy,cleanup)){
        w.unknown=true;e+=(e.empty()?"":"; ")+cleanup;return false;
    }
    // D1 frees its private data but leaves the C++ slot dangling.
    w.context=0;r.context_closed=true;return true;
}
bool quest_link_body(void* user,std::string& e){
    auto& w=*static_cast<QuestLinkWork*>(user);auto& r=*w.receipt;
    const int32_t local_id=w.request.quest?1:0;
    const local::ExternalLink borrowed{local_id,w.request.portable_key,w.request.quest};
    if(!quest_fresh(&w,borrowed,e))return false;
    QuestWeak before;
    if(!quest_weak(w.request.instance,before,e))return false;
    r.local_id_before=before.id;r.before_confirmed=true;
    if(w.request.quest?(before.target==w.request.quest&&before.proxy_target==w.request.quest+0x18):
                       (!before.target&&!before.proxy)){
        r.local_id_after=before.id;r.id_restored=true;r.after_confirmed=true;return true;
    }
    if(!stage::context_call(api.context_construct,&w.context,r.construct,e)){w.unknown=true;return false;}
    if(!w.context){w.unknown=true;return fail(e,"Quest link original Context Ctor returned null data");}
    // A fresh empty Context must make original GetObjectId(0) return null.
    // Its linked lookup path is selected when +18 is zero, with head at +10.
    if(!w.request.quest){
        uintptr_t first=0,hashed=0;
        if(!at(w.context,0x10,first,e)||!at(w.context,0x18,hashed,e)||first||hashed){
            if(e.empty())e="Quest link empty Context lookup storage differs";
            quest_close(w,true,e);return false;
        }
    }
    std::vector<local::ExternalLink> links;
    if(w.request.quest)links.push_back(borrowed);
    if(!lookup_scope.arm(w.world.original.thread,&w.context,{},std::move(links),quest_fresh,&w,e)){
        quest_close(w,true,e);return false;
    }
    w.armed=true;
    QuestWeak current;
    if(!quest_fresh(&w,borrowed,e)||!quest_weak(w.request.instance,current,e)||current!=before){
        if(e.empty())e="Quest link exact before changed before ID store";
        quest_close(w,true,e);return false;
    }
    if(before.id!=local_id){
        r.id_store_attempted=true;
        if(!live_host_state::compare_fixed_i32(w.request.instance+0x30,before.id,local_id,e)){w.unknown=true;return false;}
    }
    if(!stage::quest_link_call(api.quest_resolve,reinterpret_cast<void*>(w.request.instance+0x18),&w.context,r.resolve,e)){
        w.unknown=true;return false;
    }
    r.lookup=lookup_scope.receipt();
    const uint64_t expected_hits=w.request.quest?1u:0u;
    if(r.lookup.failed||r.lookup.external_hits!=expected_hits||r.lookup.original_calls!=1){
        w.unknown=true;return fail(e,r.lookup.failed?r.lookup.cause:"Quest link original/borrowed lookup counts differ");
    }
    QuestWeak resolved;
    if(!quest_fresh(&w,borrowed,e)||!quest_weak(w.request.instance,resolved,e)) {w.unknown=true;return false;}
    if(resolved.id!=local_id||resolved.target!=w.request.quest||
       (w.request.quest?resolved.proxy_target!=w.request.quest+0x18:
                        (resolved.proxy!=0||resolved.proxy_target!=0))){
        w.unknown=true;return fail(e,"Quest link original Resolve readback differs");
    }
    if(before.id!=local_id&&!live_host_state::compare_fixed_i32(w.request.instance+0x30,local_id,before.id,e)){
        w.unknown=true;return false;
    }
    r.id_restored=true;
    // No AddObject call registered this link or the borrowed Quest. D1's Resolve
    // has no objects to revisit; BEE180 ran exactly once above.
    if(!quest_close(w,false,e))return false;
    r.lookup=lookup_scope.receipt();
    if(r.lookup.failed||r.lookup.external_hits!=expected_hits||r.lookup.original_calls!=1){
        w.unknown=true;return fail(e,r.lookup.failed?r.lookup.cause:"Quest link Context close performed another lookup");
    }
    QuestWeak after;
    if(!quest_fresh(&w,borrowed,e)||!quest_weak(w.request.instance,after,e)){w.unknown=true;return false;}
    r.local_id_after=after.id;
    if(after.id!=before.id||after.target!=resolved.target||after.proxy!=resolved.proxy||after.proxy_target!=resolved.proxy_target){
        w.unknown=true;return fail(e,"Quest link final Weak/ID readback differs");
    }
    r.after_confirmed=true;return true;
}
bool quest_link_exit(void* user,std::string& e){
    auto& w=*static_cast<QuestLinkWork*>(user);
    const local::ExternalLink borrowed{w.request.quest?1:0,w.request.portable_key,w.request.quest};
    return quest_fresh(&w,borrowed,e);
}
}
bool bind_quest_instance(const QuestLinkRequest& request,const std::string& origin,
                         const FrameBinding& frame,QuestLinkReceipt& result,std::string& e){
    result={};e.clear();
    if(busy.test_and_set())return fail(e,"Quest link shared Subscriber append scope busy");
    struct Busy{~Busy(){busy.clear();}} release;
    auto owner=std::make_unique<QuestLinkWork>();auto& w=*owner;
    w.request=request;w.receipt=&result;w.world.origin=origin;w.world.local_sha=frame.local_sha;w.world.frame=&frame;
    local::SaveScopeReceipt scope;bool ok=false;
    try{
        if(!hook_ready||!image_base||!api.quest_resolve)return fail(e,"Quest link original shared lookup hook/API not ready");
        if(!request.instance||request.quest>UINTPTR_MAX-0x28||request.portable_key.empty()||!request.fresh)
            return fail(e,"Quest link local request missing");
        if(!read_world(w.world,false,w.world.original,e))return false;
        if(w.world.original.image!=image_base)return fail(e,"Quest link current image differs");
        if(blocked_epoch==w.world.original.origin.world_epoch&&blocked_generation==w.world.original.origin.generation)
            return fail(e,"Quest link prior outcome unknown: "+blocked_cause);
        const local::SaveScopeApi save{reinterpret_cast<local::SaveScopeCall>(image_base+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(image_base+0x49f6f0)};
        ok=local::with_save_scope(save,w.world.original.origin.mode,quest_link_body,quest_link_exit,&w,scope,e);
        if(scope.unknown)w.unknown=true;
        if(!ok&&(result.id_store_attempted||result.resolve.attempted))w.unknown=true;
        if(ok){stage::ClientWorld closed;
            if(!read_world(w.world,false,closed,e)||!stage::same_client_owner(w.world.original,closed)){
                if(e.empty())e="Quest link current owner changed after SaveScope";
                w.unknown=true;ok=false;
            }
        }
    }catch(const std::exception& ex){e+=(e.empty()?"":"; ")+std::string("Quest link C++: ")+ex.what();w.unknown=true;}
    catch(...){e+=(e.empty()?"":"; ")+std::string("Quest link unknown C++ exception");w.unknown=true;}
    if(w.armed){result.lookup=lookup_scope.receipt();lookup_scope.clear();w.armed=false;}
    if(w.unknown){
        blocked_epoch=w.world.original.origin.world_epoch;blocked_generation=w.world.original.origin.generation;blocked_cause=e;
        result.unknown=true;result.private_context_retained=true;
        // Retain the C++ slot even when Ctor or Dtor ownership is indeterminate.
        w.receipt=nullptr;w.world.frame=nullptr;w.request.fresh=nullptr;w.request.user=nullptr;
        w.retained_next=std::move(retained_quest_links);retained_quest_links=std::move(owner);
        return false;
    }
    result.confirmed=ok&&result.after_confirmed;return result.confirmed;
}
// QUEST_LINK_BODY_END

// QUEST_PROGRESS_BODY_BEGIN
namespace {
struct ProgressArray {
    uintptr_t data=0;int32_t count=0,capacity=0;
    std::vector<uintptr_t> members;
    bool operator==(const ProgressArray&) const=default;
};
struct ProgressRow {
    uintptr_t npc=0,proxy=0,proxy_target=0;
    QuestWeak quest;
    int32_t completed=0,monsters=0;
    uint8_t visited=0,action=0,complete=0;
    bool operator==(const ProgressRow&) const=default;
};
struct ProgressWork {
    Work world;ProgressRequest request;ProgressReceipt* receipt=nullptr;
    uintptr_t selected=0;bool retire=false,unknown=false;
    std::unique_ptr<ProgressWork> retained_next;
};
std::unique_ptr<ProgressWork> retained_progress;
bool progress_array(uintptr_t toon,ProgressArray& out,std::string& e){
    uintptr_t ap=0,array_ap=0;
    if(!toon||toon>UINTPTR_MAX-0xb08)return fail(e,"Progress Toon extent invalid");
    if(!at(toon,0,ap,e)||!at(toon,0xaf0,array_ap,e)||!at(toon,0xaf8,out.data,e)||
       !at(toon,0xb00,out.count,e)||!at(toon,0xb04,out.capacity,e))return false;
    if(ap!=image_base+0x1744e90||array_ap!=image_base+0x16ee0e0||out.count<0||
       out.capacity<out.count||out.capacity>100000||(out.count&&!out.data)||
       size_t(out.count)>(UINTPTR_MAX-out.data)/sizeof(uintptr_t))return fail(e,"Progress native array header differs");
    std::set<uintptr_t> unique;out.members.clear();
    for(int32_t i=0;i<out.count;++i){uintptr_t p=0;
        if(!at(out.data,size_t(i)*8,p,e))return false;
        if(!p||!unique.insert(p).second)return fail(e,"Progress array null or duplicate member");
        out.members.push_back(p);
    }
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(toon,0xaf8,data,e)||!at(toon,0xb00,count,e)||!at(toon,0xb04,capacity,e))return false;
    return (data==out.data&&count==out.count&&capacity==out.capacity)||fail(e,"Progress array changed while reading");
}
bool progress_row(uintptr_t p,ProgressRow& out,std::string& e){
    uintptr_t ap=0,link_ap=0,weak_ap=0;
    if(!p||p>UINTPTR_MAX-0xb0)return fail(e,"Progress row extent invalid");
    if(!at(p,0,ap,e)||!at(p,0x18,link_ap,e)||!at(p,0x38,weak_ap,e)||
       !at(p,0x40,out.npc,e)||!at(p,0x48,out.proxy,e))return false;
    if(ap!=image_base+0x172d130||link_ap!=image_base+0x1711930||weak_ap!=image_base+0x16f9de0)
        return fail(e,"Progress row/NPC Weak addresspoints differ");
    if(!out.proxy){if(out.npc)return fail(e,"Progress NPC target without Proxy");}
    else{
        int32_t references=0;
        if(!at(out.proxy,0,out.proxy_target,e)||!at(out.proxy,8,references,e))return false;
        if(references<=0)return fail(e,"Progress NPC Proxy refcount not positive");
        if(out.proxy_target){uintptr_t npc_ap=0,reverse=0;
            if(!out.npc||out.npc>UINTPTR_MAX-0x30||out.proxy_target!=out.npc+0x20)
                return fail(e,"Progress NPC live Weak target differs");
            if(!at(out.npc,0,npc_ap,e)||!at(out.npc,0x28,reverse,e))return false;
            if(npc_ap!=image_base+0x1742980||reverse!=out.proxy)return fail(e,"Progress NPC live AP/Proxy differs");
        }
    }
    if(!quest_weak(p+0x58,out.quest,e)||!at(p,0x50,out.completed,e)||!at(p,0xa8,out.monsters,e)||
       !at(p,0xac,out.visited,e)||!at(p,0xad,out.action,e)||!at(p,0xae,out.complete,e))return false;
    return (out.visited<=1&&out.action<=1&&out.complete<=1)||fail(e,"Progress Instance bool differs");
}
bool progress_fresh(ProgressWork& w,std::string& e){
    stage::ClientWorld current;
    if(!read_world(w.world,true,current,e))return false;
    if(!stage::same_client_owner(w.world.original,current))return fail(e,"Progress current Client owner changed");
    const local::ExternalLink link{1,w.request.npc_key,w.request.npc};
    if(!w.request.fresh(w.request.user,link,e))return fail(e,e.empty()?"Progress fresh owner rejected without cause":e);
    return true;
}
bool progress_unchanged(ProgressWork& w,const ProgressArray& expected,std::string& e){
    ProgressArray actual;
    return progress_fresh(w,e)&&progress_array(w.request.toon,actual,e)&&
        (actual==expected||fail(e,"Progress exact array before changed"));
}
bool progress_unary(stage::Unary f,uintptr_t p,const char* operation,local::CallReceipt& r,std::string& e){
    if(stage::context_call(f,reinterpret_cast<void*>(p),r,e))return true;
    e=std::string(operation)+": "+e;return false;
}
bool progress_pair(stage::QuestResolve f,uintptr_t p,uintptr_t value,const char* operation,local::CallReceipt& r,std::string& e){
    if(stage::quest_link_call(f,reinterpret_cast<void*>(p),reinterpret_cast<void*>(value),r,e))return true;
    e=std::string(operation)+": "+e;return false;
}
bool progress_ensure_body(ProgressWork& w,const ProgressArray& before,std::string& e){
    auto& r=*w.receipt;
    for(auto p:before.members){ProgressRow row;if(!progress_row(p,row,e))return false;
        if(row.proxy_target&&row.npc==w.request.npc){
            if(w.selected)return fail(e,"Progress live NPC row duplicated");w.selected=p;
        }
    }
    if(w.selected){
        if(!progress_unchanged(w,before,e))return false;
        r.progress=w.selected;r.instance=w.selected+0x58;r.existing=true;r.after_confirmed=true;r.count_after=before.count;return true;
    }
    uintptr_t npc_ap=0;
    if(!at(w.request.npc,0,npc_ap,e))return false;
    if(npc_ap!=image_base+0x1742980)return fail(e,"Progress Ensure live NPC AP differs");
    if(!progress_unchanged(w,before,e))return false;
    if(!stage::allocate_progress(api.records.allocate,w.selected,r.allocate,e)){w.unknown=true;return false;}
    r.progress=w.selected;r.instance=w.selected+0x58;
    if(!progress_unchanged(w,before,e)){w.unknown=true;return false;}
    if(!progress_unary(api.progress_construct,w.selected,"Progress Ctor",r.construct,e)){w.unknown=true;return false;}
    ProgressRow constructed;
    if(!progress_row(w.selected,constructed,e)){w.unknown=true;return false;}
    if(constructed.npc||constructed.proxy||constructed.quest.target||constructed.quest.proxy||
       constructed.monsters||constructed.visited||constructed.action||constructed.complete){w.unknown=true;return fail(e,"Progress Ctor default Instance differs");}
    if(!progress_unchanged(w,before,e)){w.unknown=true;return false;}
    if(!progress_pair(api.progress_npc_assign,w.selected+0x38,w.request.npc,"Progress NPC assignment",r.assign,e)){w.unknown=true;return false;}
    ProgressRow assigned;
    if(!progress_row(w.selected,assigned,e)){w.unknown=true;return false;}
    auto expected=constructed;expected.npc=assigned.npc;expected.proxy=assigned.proxy;expected.proxy_target=assigned.proxy_target;
    if(assigned.npc!=w.request.npc||assigned.proxy_target!=w.request.npc+0x20||assigned!=expected){w.unknown=true;return fail(e,"Progress NPC assignment/default fields differ");}
    if(!progress_unchanged(w,before,e)||
       !live_host_state::compare_fixed_i32(w.selected+0x50,assigned.completed,0,e)){w.unknown=true;return false;}
    r.completed_initialized=true;
    if(!progress_unchanged(w,before,e)){w.unknown=true;return false;}
    if(!progress_pair(api.progress_add,w.request.toon+0xaf0,w.selected,"Progress original AddItem",r.append,e)){w.unknown=true;return false;}
    ProgressArray after;ProgressRow final;
    if(!progress_fresh(w,e)||!progress_array(w.request.toon,after,e)||!progress_row(w.selected,final,e)){w.unknown=true;return false;}
    auto members=before.members;members.push_back(w.selected);expected.completed=0;
    if(after.members!=members||final!=expected){w.unknown=true;return fail(e,"Progress appended membership/default readback differs");}
    r.count_after=after.count;r.published=true;r.after_confirmed=true;return true;
}
bool progress_retire_body(ProgressWork& w,const ProgressArray& before,std::string& e){
    auto& r=*w.receipt;
    const auto it=std::find(before.members.begin(),before.members.end(),w.selected);
    if(it==before.members.end())return fail(e,"Progress retire target is not a current Toon member");
    ProgressRow row;if(!progress_row(w.selected,row,e))return false;
    if(row.proxy_target&&row.npc!=w.request.npc)return fail(e,"Progress retire live NPC binding differs");
    const size_t index=size_t(it-before.members.begin()),tail=before.members.size()-index-1;
    if(!progress_unchanged(w,before,e))return false;
    if(tail&&!stage::shift_progress(api.progress_shift,reinterpret_cast<void*>(before.data+index*8),
                                   reinterpret_cast<const void*>(before.data+(index+1)*8),tail*8,r.shift,e)){
        w.unknown=true;return false;
    }
    if(!progress_fresh(w,e)||!live_host_state::compare_fixed_i32(w.request.toon+0xb00,before.count,before.count-1,e)){
        w.unknown=true;return false;
    }
    ProgressArray removed;auto expected=before.members;expected.erase(expected.begin()+index);
    if(!progress_fresh(w,e)||!progress_array(w.request.toon,removed,e)){w.unknown=true;return false;}
    if(removed.data!=before.data||removed.capacity!=before.capacity||removed.members!=expected){w.unknown=true;return fail(e,"Progress remove array readback differs before D0");}
    r.removed=true;r.count_after=removed.count;
    // Until D0 this is a detached live object. Its full row is checked once more.
    ProgressRow current;
    if(!progress_row(w.selected,current,e)||current!=row||!progress_unchanged(w,removed,e)){
        w.unknown=true;if(e.empty())e="Progress detached row changed before D0";return false;
    }
    if(!progress_unary(api.progress_destroy,w.selected,"Progress original D0",r.destroy,e)){w.unknown=true;return false;}
    // Only fresh array pointers are read after D0; selected is now a number.
    ProgressArray after;
    if(!progress_fresh(w,e)||!progress_array(w.request.toon,after,e)){w.unknown=true;return false;}
    if(after!=removed){w.unknown=true;return fail(e,"Progress post-D0 array readback differs");}
    r.after_confirmed=true;return true;
}
bool progress_body(void* user,std::string& e){
    auto& w=*static_cast<ProgressWork*>(user);ProgressArray before;
    if(!progress_fresh(w,e)||!progress_array(w.request.toon,before,e))return false;
    w.receipt->count_before=before.count;
    return w.retire?progress_retire_body(w,before,e):progress_ensure_body(w,before,e);
}
bool progress_exit(void* user,std::string& e){return progress_fresh(*static_cast<ProgressWork*>(user),e);}
bool progress_apply(const ProgressRequest& request,uintptr_t retiring,bool retire,
                    const std::string& origin,const FrameBinding& frame,ProgressReceipt& result,std::string& e){
    result={};e.clear();
    if(busy.test_and_set())return fail(e,"Progress shared Subscriber append scope busy");
    struct Busy{~Busy(){busy.clear();}} release;
    auto owner=std::make_unique<ProgressWork>();auto& w=*owner;
    w.request=request;w.receipt=&result;w.retire=retire;w.selected=retiring;result.progress=retiring;
    w.world.origin=origin;w.world.local_sha=frame.local_sha;w.world.frame=&frame;
    local::SaveScopeReceipt scope;bool ok=false;
    try{
        if(!image_base||!api.progress_construct||!api.progress_destroy||!api.progress_npc_assign||!api.progress_add||!api.progress_shift)
            return fail(e,"Progress original API missing");
        if(!request.toon||(!retire&&!request.npc)||(retire&&!retiring)||request.npc>UINTPTR_MAX-0x30||request.npc_key.empty()||!request.fresh)
            return fail(e,"Progress local request missing");
        if(!read_world(w.world,false,w.world.original,e))return false;
        if(w.world.original.image!=image_base)return fail(e,"Progress current image differs");
        if(blocked_epoch==w.world.original.origin.world_epoch&&blocked_generation==w.world.original.origin.generation)
            return fail(e,"Progress prior outcome unknown: "+blocked_cause);
        const local::SaveScopeApi save{reinterpret_cast<local::SaveScopeCall>(image_base+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(image_base+0x49f6f0)};
        ok=local::with_save_scope(save,w.world.original.origin.mode,progress_body,progress_exit,&w,scope,e);
        if(scope.unknown||(!ok&&(result.allocate.attempted||result.shift.attempted||result.removed||result.destroy.attempted)))w.unknown=true;
        if(ok){stage::ClientWorld closed;
            if(!read_world(w.world,false,closed,e)||!stage::same_client_owner(w.world.original,closed)){
                if(e.empty())e="Progress owner changed after SaveScope";w.unknown=true;ok=false;
            }
        }
    }catch(const std::exception& ex){e+=(e.empty()?"":"; ")+std::string("Progress C++: ")+ex.what();w.unknown=true;}
    catch(...){e+=(e.empty()?"":"; ")+std::string("Progress unknown C++ exception");w.unknown=true;}
    if(w.unknown){
        blocked_epoch=w.world.original.origin.world_epoch;blocked_generation=w.world.original.origin.generation;blocked_cause=e;
        result.unknown=true;result.retained=true;
        w.receipt=nullptr;w.world.frame=nullptr;w.request.fresh=nullptr;w.request.user=nullptr;
        w.retained_next=std::move(retained_progress);retained_progress=std::move(owner);return false;
    }
    result.confirmed=ok&&result.after_confirmed;return result.confirmed;
}
}
bool ensure_progress(const ProgressRequest& request,const std::string& origin,const FrameBinding& frame,ProgressReceipt& r,std::string& e){
    return progress_apply(request,0,false,origin,frame,r,e);
}
bool retire_progress(const ProgressRequest& request,uintptr_t progress,const std::string& origin,const FrameBinding& frame,ProgressReceipt& r,std::string& e){
    return progress_apply(request,progress,true,origin,frame,r,e);
}
// QUEST_PROGRESS_BODY_END

bool apply_impl(const Json& graph,const std::string& origin,const FrameBinding* frame,Json& result,std::string& e){
    result={{"state","rejected"},{"subscriber_creation_attempted",false},{"full_cohort_replication",false}};
    if(busy.test_and_set())return fail(e,"Subscriber append reentrant probe");struct Busy{~Busy(){busy.clear();}} release;
    Work work;work.origin=origin;work.frame=frame;work.receipt=&result;local::SaveScopeReceipt scope;
    try{
        if(!hook_ready||!image_base)return fail(e,"Subscriber append original lookup hook not ready");
        if(!stage::parse_payload(graph,work.payload,e))return false;
        work.local_sha=frame?frame->local_sha:work.payload.baseline_sha;
        if(frame&&!local::additions::frame_binding(graph,frame->context.session,frame->context.snapshot_uuid,frame->common_epoch,e))return false;
        if(!read_world(work,false,work.original,e))return false;
        if(work.original.image!=image_base)return fail(e,"Subscriber append resolved image differs from current world");
        if(blocked_epoch==work.original.origin.world_epoch&&blocked_generation==work.original.origin.generation)return fail(e,"Subscriber append prior outcome unknown: "+blocked_cause);
        result["serialized_subscriber_id"]=work.payload.id;result["source_snapshot_sha256"]=work.payload.baseline_sha;result["actual_client_origin_sha256"]=work.local_sha;result["shared_frame_binding"]=frame!=nullptr;
        if(std::any_of(work.payload.external.begin(),work.payload.external.end(),[](const auto& r){return r.key.kind==stage::ReferenceKind::SharedBuilding;})&&
           !work.building_pins.prepare(work.original.image,work.original.origin,e))return false;
        const local::SaveScopeApi save{reinterpret_cast<local::SaveScopeCall>(image_base+0x4a03a0),reinterpret_cast<local::SaveScopeCall>(image_base+0x49f6f0)};
        const bool done=local::with_save_scope(save,work.original.origin.mode,body,exit_guard,&work,scope,e);
        result["subscriber_creation_attempted"]=work.effect;
        result["save_scope"]={{"enter_returned",scope.enter_returned},{"body_attempted",scope.body_attempted},{"leave_attempted",scope.leave_attempted},{"leave_returned",scope.leave_returned},{"exit_guard_confirmed",scope.exit_guard_confirmed},{"unknown",scope.unknown},{"body_cause",scope.body_cause},{"cleanup_cause",scope.cleanup_cause}};
        if(!done){work.unknown=work.unknown||scope.unknown||work.effect;}
        else{stage::ClientWorld closed;if(!read_world(work,false,closed,e)||!stage::same_client_owner(work.original,closed)||!work.building_pins.verify_used(e))work.unknown=true;else{result["state"]="confirmed";result["closed_world_readback_confirmed"]=true;return true;}}
    }catch(const std::exception& ex){e=std::string("Subscriber append exception: ")+ex.what();work.unknown=work.unknown||work.effect||scope.enter_attempted;}
    catch(...){e="Subscriber append unknown C++ exception";work.unknown=true;}
    if(work.unknown){blocked_epoch=work.original.origin.world_epoch;blocked_generation=work.original.origin.generation;blocked_cause=e;result["state"]="unknown";result["replay_permitted"]=false;lookup_scope.clear();}
    result["subscriber_creation_attempted"]=work.effect;result["cause"]=e;return false;
}
bool apply_graph(const Json& graph,const std::string& origin,Json& result,std::string& e){return apply_impl(graph,origin,nullptr,result,e);}
bool apply_frame(const Json& graph,const std::string& origin,const FrameBinding& frame,Json& result,std::string& e){return apply_impl(graph,origin,&frame,result,e);}
bool probe(const std::filesystem::path& root,const std::string& basename,const std::string& sha,const std::string& origin,Json& result,std::string& e){
    result={{"state","rejected"},{"subscriber_creation_attempted",false}};
    try{if(!filename(basename)||sha.size()!=64)return fail(e,"Subscriber append artifact basename/SHA invalid");
        std::ifstream file(root/"diagnostics"/"subscriber-records"/basename,std::ios::binary);if(!file)return fail(e,"Subscriber append private artifact open failed");
        const std::string content((std::istreambuf_iterator<char>(file)),{});if(file.bad()||hash(content)!=sha)return fail(e,"Subscriber append artifact readback SHA differs");
        return apply_graph(Json::parse(content),origin,result,e);
    }catch(const std::exception& x){return fail(e,std::string("Subscriber artifact read original=")+x.what());}
}

}
