#include "mt2mp/hooks/framework_backend.h"
#include <cstring>
#include <limits>
#include <set>
#include <vector>
#include <algorithm>

namespace mt2mp::hooks::framework {
namespace {
struct Target { const char* name; uintptr_t rva; bool shared; };
constexpr Target catalog[]{
#include "framework_catalog.inc"
};
constexpr char game_sha[]="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
const Target* find(uintptr_t image, uintptr_t target) {
    if(target<image)return nullptr;
    for(const auto& entry:catalog)if(entry.rva==target-image)return &entry;
    return nullptr;
}
const Target* containing(uintptr_t image, uintptr_t address, size_t length) {
    if(address<image||!length||length>4096)return nullptr;
    const auto rva=address-image;
    const Target* nearest=nullptr;
    for(const auto& entry:catalog)
        if(entry.rva<=rva&&(!nearest||entry.rva>nearest->rva))nearest=&entry;
    if(!nearest||rva-nearest->rva>=4096||length>4096-(rva-nearest->rva)||
       address>(std::numeric_limits<uintptr_t>::max)()-length)return nullptr;
    return nearest;
}
template<size_t N> bool equal(const char (&actual)[N],const char* expected) {
    const auto length=std::strlen(expected);
    return length<N&&std::memcmp(actual,expected,length)==0&&actual[length]==0;
}
std::string cause(const MT2FW_MT2_RouteReceipt& value) {
    const auto end=static_cast<const char*>(std::memchr(value.cause,0,sizeof value.cause));
    return end?std::string(value.cause,end):"Framework returned an unterminated route cause";
}
MT2FW_MT2_RouteReceipt fresh() { MT2FW_MT2_RouteReceipt result{};result.size=sizeof result;return result; }
MT2FW_MT2_FreeReceipt fresh_free() { MT2FW_MT2_FreeReceipt result{};result.size=sizeof result;return result; }
template<size_t N> bool same_text(const char (&a)[N],const char (&b)[N]) {
    return std::memchr(a,0,N)&&std::memchr(b,0,N)&&std::strcmp(a,b)==0;
}
bool same_module(const MT2FW_MT2_ModuleProof& a,const MT2FW_MT2_ModuleProof& b) {
    return a.base==b.base&&a.bytes==b.bytes&&a.pinned==b.pinned&&a.version_present==b.version_present&&
        a.version_error==b.version_error&&std::memcmp(a.version,b.version,sizeof a.version)==0&&
        same_text(a.sha256,b.sha256)&&same_text(a.path,b.path);
}
bool same_export(const MT2FW_MT2_ExportProof& a,const MT2FW_MT2_ExportProof& b) {
    return a.resolved==b.resolved&&a.owner==b.owner&&a.export_rva==b.export_rva&&a.forwarded==b.forwarded&&same_text(a.forwarder,b.forwarder);
}
bool module_proof(const MT2FW_MT2_ModuleProof& value) {
    return value.base&&value.bytes&&value.pinned==1&&value.sha256[64]==0&&std::strlen(value.sha256)==64&&
        value.path[0]&&std::memchr(value.path,0,sizeof value.path);
}
std::string free_cause(const MT2FW_MT2_FreeReceipt& value) {
    const auto end=static_cast<const char*>(std::memchr(value.cause,0,sizeof value.cause));
    return end?std::string(value.cause,end):"Framework returned an unterminated free cause";
}
}
Backend& process_backend(){static Backend value;return value;}
MH_STATUS Backend::fail(const char* operation,MH_STATUS status,const std::string& message,Entry* entry) {
    error_=message;
    if(entry){entry->blocked=true;if(entry->cause.empty())entry->cause=message;}
    if(log_)log_(user_,operation,error_);
    return status;
}
bool Backend::current(const char* operation) {
    if(!bound_){fail(operation,MH_ERROR_NOT_INITIALIZED,"Framework hook backend is not bound");return false;}
    if(GetCurrentThreadId()!=thread_){fail(operation,MH_UNKNOWN,"Framework hook operation is outside the bound game thread");return false;}
    return true;
}
bool Backend::bind(const MT2FW_MT2_RoutesV1& routes,uintptr_t image,const char* owner,
                   uint32_t thread,Reclaim reclaim,Log log,void* user) {
    if(bound_){fail("bind",MH_ERROR_ALREADY_INITIALIZED,"Framework hook backend cannot rebind");return false;}
    // The caller obtains this service with the exact Host pointer in an active callback.
    if(!log){error_="Framework hook backend requires a failure logger";return false;}
    log_=log;user_=user;
    if(routes.size!=sizeof routes||routes.version!=MT2FW_MT2_ROUTES_VERSION||!routes.context||
       !routes.hook_prepare||!routes.hook_enable||!routes.hook_remove||!routes.route_read||!routes.code_verify||
       !image||!owner||std::strcmp(owner,"doodesch.multiplayer")||!reclaim||!thread||GetCurrentThreadId()!=thread) {
        fail("bind",MH_ERROR_NOT_INITIALIZED,"Framework hook service, owner, image, reclamation gate or thread differs");return false;
    }
    for(const auto& item:catalog)if(image>(std::numeric_limits<uintptr_t>::max)()-item.rva){
        fail("bind",MH_ERROR_NOT_EXECUTABLE,"Framework hook image plus catalog RVA overflows");return false;}
    api_=routes;image_=image;owner_=owner;thread_=thread;reclaim_=reclaim;bound_=true;error_.clear();return true;
}
MH_STATUS Backend::initialize(){if(!current("initialize"))return MH_ERROR_NOT_INITIALIZED;error_.clear();return MH_OK;}
bool Backend::bind_batch(const MT2FW_MT2_RoutesV2& value) {
    if(!current("bind_batch"))return false;
    const auto& v=value.v1;
    if(batch_||v.size!=sizeof(value)||v.version!=MT2FW_MT2_ROUTES_BATCH_VERSION||v.context!=api_.context||
       v.create_subscribe!=api_.create_subscribe||v.create_unsubscribe!=api_.create_unsubscribe||
       v.remove_bind!=api_.remove_bind||v.remove_unbind!=api_.remove_unbind||
       v.hook_prepare!=api_.hook_prepare||v.hook_enable!=api_.hook_enable||v.hook_remove!=api_.hook_remove||
       v.route_read!=api_.route_read||v.code_verify!=api_.code_verify||!value.hook_enable_batch){
        fail("bind_batch",MH_ERROR_NOT_INITIALIZED,"Framework batch service differs from the bound owner and immediate routes");return false;
    }
    batch_=value.hook_enable_batch;error_.clear();return true;
}
bool Backend::discover_batch(MT2FW_MT2_QueryRoutesFn query,const MT2FW_Host* host){
    if(!current("discover_batch"))return false;
    if(!query||!host){fail("discover_batch",MH_ERROR_NOT_INITIALIZED,"Framework batch query requires the authenticated Host and export");return false;}
    MT2FW_MT2_RoutesV2 value;std::memset(&value,0,sizeof(value));
    const auto result=query(host,MT2FW_MT2_ROUTES_BATCH_VERSION,&value.v1,sizeof(value));
    const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
    if(result==MT2FW_MT2_INVALID&&std::all_of(bytes,bytes+sizeof(value),[](unsigned char byte){return byte==0;})){
        error_.clear();return true;
    }
    if(result!=MT2FW_MT2_OK){fail("discover_batch",MH_ERROR_NOT_INITIALIZED,"Framework batch discovery failed or wrote rejected output result="+std::to_string(result));return false;}
    return bind_batch(value);
}
bool Backend::bind_ownership(const MT2FW_MT2_OwnershipV1& value) {
    if(!current("bind_ownership"))return false;
    if(ownership_bound_){fail("bind_ownership",MH_ERROR_ALREADY_INITIALIZED,"Framework ownership service cannot rebind");return false;}
    // Extension context is a different service owner, obtained with the same exact Host.
    if(value.size!=sizeof value||value.version!=MT2FW_MT2_OWNERSHIP_VERSION||!value.context||image_>(std::numeric_limits<uintptr_t>::max)()-0x1c52a70||
       !value.hook_disable||!value.hook_resume||!value.free_prepare||!value.free_enable||
       !value.free_disable||!value.free_read||!value.free_remove){
        fail("bind_ownership",MH_ERROR_NOT_INITIALIZED,"Framework ownership service is incomplete or differs");return false;
    }
    ownership_=value;ownership_bound_=true;error_.clear();return true;
}
bool Backend::receipt(const MT2FW_MT2_RouteReceipt& r,uintptr_t target,const Entry* entry,uint32_t state)const {
    const auto* item=find(image_,target);
    if(!item||r.size!=sizeof r||r.state!=state||r.entry!=target||r.rva!=item->rva||!equal(r.game_sha256,game_sha))return false;
    if(state==MT2FW_MT2_ROUTE_REMOVED)return entry&&r.generation==entry->generation&&!r.owner[0]&&!r.detour&&!r.trampoline;
    if(!entry)return item->shared?(equal(r.owner,"framework")&&r.generation&&r.detour&&r.trampoline):
        (state==MT2FW_MT2_ROUTE_PRISTINE&&!r.owner[0]&&!r.detour&&!r.trampoline);
    return r.generation&&(!entry->generation||r.generation==entry->generation)&&
        equal(r.owner,owner_.c_str())&&r.detour==entry->detour&&r.trampoline&&
        (!entry->trampoline||r.trampoline==entry->trampoline);
}
MH_STATUS Backend::create(void* address,void* detour,void** original) {
    if(!current("prepare"))return MH_ERROR_NOT_INITIALIZED;
    const auto target=reinterpret_cast<uintptr_t>(address);const auto* item=find(image_,target);
    if(!item)return fail("prepare",MH_ERROR_UNSUPPORTED_FUNCTION,"Target is not in the pinned Framework EXE catalog; no import or dynamic hook permitted");
    if(item->shared)return fail("prepare",MH_ERROR_UNSUPPORTED_FUNCTION,"Shared Framework target requires its typed route: "+std::string(item->name));
    if(!detour||!original)return fail("prepare",MH_ERROR_NOT_EXECUTABLE,"Hook detour and original output are required");
    if(entries_.contains(target))return fail("prepare",MH_ERROR_ALREADY_CREATED,"Hook prepare already attempted; no replay: "+std::string(item->name));
    auto& entry=entries_[target];entry.name=item->name;entry.detour=reinterpret_cast<uintptr_t>(detour);
    auto observed=fresh();const auto result=api_.hook_prepare(api_.context,entry.name.c_str(),detour,&observed);
    if(result!=MT2FW_MT2_OK||!receipt(observed,target,&entry,MT2FW_MT2_ROUTE_PREPARED)) {
        entry.owned=equal(observed.owner,owner_.c_str());entry.state=MT2FW_MT2_ROUTE_UNKNOWN;
        return fail("prepare",MH_UNKNOWN,"Framework prepare failed or receipt differs: "+entry.name+" result="+std::to_string(result)+" cause="+cause(observed),&entry);
    }
    entry.owned=true;entry.state=observed.state;entry.generation=observed.generation;entry.trampoline=observed.trampoline;
    *original=reinterpret_cast<void*>(entry.trampoline);
    error_.clear();return MH_OK;
}
MH_STATUS Backend::enable_one(uintptr_t target,Entry& entry) {
    if(entry.blocked)return fail("enable",MH_UNKNOWN,"Hook has a retained failure; no replay: "+entry.cause);
    if(!entry.owned)return fail("enable",MH_ERROR_NOT_CREATED,"Hook is not owned by this plugin");
    auto before=fresh();auto result=api_.route_read(api_.context,entry.name.c_str(),&before);
    if(result!=MT2FW_MT2_OK||!receipt(before,target,&entry,entry.state))
        return fail("enable",MH_UNKNOWN,"Framework current route differs before enable: "+entry.name+" cause="+cause(before),&entry);
    if(entry.state==MT2FW_MT2_ROUTE_ENABLED)return MH_ERROR_ENABLED;
    const bool resume=entry.state==MT2FW_MT2_ROUTE_DISABLED_RETAINED;
    if(entry.state!=MT2FW_MT2_ROUTE_PREPARED&&!resume)return fail("enable",MH_UNKNOWN,"Hook is not prepared or disabled-retained",&entry);
    if(resume&&!ownership_bound_)return fail("enable",MH_ERROR_UNSUPPORTED_FUNCTION,"Retained resume requires the ownership service");
    auto after=fresh();result=resume?ownership_.hook_resume(ownership_.context,entry.name.c_str(),&after):api_.hook_enable(api_.context,entry.name.c_str(),&after);
    if(result!=MT2FW_MT2_OK||!receipt(after,target,&entry,MT2FW_MT2_ROUTE_ENABLED))
        return fail("enable",MH_UNKNOWN,"Framework enable failed or receipt differs: "+entry.name+" result="+std::to_string(result)+" cause="+cause(after),&entry);
    entry.state=after.state;error_.clear();return MH_OK;
}
MH_STATUS Backend::enable(void* address) {
    if(!current("enable"))return MH_ERROR_NOT_INITIALIZED;
    if(address){auto it=entries_.find(reinterpret_cast<uintptr_t>(address));if(it==entries_.end())return fail("enable",MH_ERROR_NOT_CREATED,"Target has no local prepare receipt");return enable_one(it->first,it->second);}
    if(batch_){
        std::vector<uintptr_t> prepared;prepared.reserve(MT2FW_MT2_ROUTES_BATCH_MAX);
        const auto flush=[&](){if(prepared.empty())return MH_OK;
            const auto result=enable_prepared_batch(prepared);prepared.clear();return result;};
        for(auto& [target,entry]:entries_){
            if(entry.owned&&!entry.blocked&&entry.state==MT2FW_MT2_ROUTE_PREPARED){
                prepared.push_back(target);
                if(prepared.size()==MT2FW_MT2_ROUTES_BATCH_MAX){const auto result=flush();if(result!=MH_OK)return result;}
                continue;
            }
            // Keep receipt validation and retained resume in the original order.
            const auto result=flush();if(result!=MH_OK)return result;
            if(entry.state==MT2FW_MT2_ROUTE_REMOVED)continue;
            const auto enabled=enable_one(target,entry);if(enabled!=MH_OK&&enabled!=MH_ERROR_ENABLED)return enabled;
        }
        const auto result=flush();if(result!=MH_OK)return result;
        error_.clear();return MH_OK;
    }
    // Legacy all means only this backend's prepared entries. It never reaches the SDK.
    for(auto& [target,entry]:entries_){if(entry.state==MT2FW_MT2_ROUTE_REMOVED)continue;
        const auto result=enable_one(target,entry);if(result!=MH_OK&&result!=MH_ERROR_ENABLED)return result;}
    error_.clear();return MH_OK;
}
MH_STATUS Backend::enable_prepared_batch(std::span<const uintptr_t> targets) {
    if(!current("enable_batch"))return MH_ERROR_NOT_INITIALIZED;
    if(targets.empty()||targets.size()>MT2FW_MT2_ROUTES_BATCH_MAX)
        return fail("enable_batch",MH_ERROR_UNSUPPORTED_FUNCTION,"Prepared batch count must be 1..128");
    // Older providers retain the original immediate operation and failure order.
    if(!batch_){
        for(auto target:targets){const auto result=enable(reinterpret_cast<void*>(target));if(result!=MH_OK)return result;}
        return MH_OK;
    }
    std::set<uintptr_t> unique;
    std::vector<Entry*> entries;std::vector<const char*> names;
    std::vector<MT2FW_MT2_RouteReceipt> observed(targets.size());
    for(auto target:targets){
        const auto found=entries_.find(target);
        if(!unique.insert(target).second||found==entries_.end())
            return fail("enable_batch",MH_ERROR_NOT_CREATED,"Prepared batch contains a duplicate or unknown local target RVA="+std::to_string(target>=image_?target-image_:target));
        auto& entry=found->second;
        if(entry.blocked||!entry.owned||entry.state!=MT2FW_MT2_ROUTE_PREPARED)
            return fail("enable_batch",MH_UNKNOWN,"Prepared batch target is not an unblocked owned prepared route: "+entry.name+" cause="+entry.cause);
        auto before=fresh();const auto result=api_.route_read(api_.context,entry.name.c_str(),&before);
        if(result!=MT2FW_MT2_OK||!receipt(before,target,&entry,MT2FW_MT2_ROUTE_PREPARED))
            return fail("enable_batch",MH_UNKNOWN,"Framework current route differs before batch enable: "+entry.name+" cause="+cause(before),&entry);
        entries.push_back(&entry);names.push_back(entry.name.c_str());
    }
    for(auto& value:observed)value=fresh();
    const auto result=batch_(api_.context,names.data(),static_cast<uint32_t>(names.size()),observed.data(),static_cast<uint32_t>(observed.size()));
    std::string failure;
    for(size_t i=0;i<entries.size();++i){
        auto& entry=*entries[i];entry.batch_receipt=observed[i];
        const bool enabled=receipt(observed[i],targets[i],&entry,MT2FW_MT2_ROUTE_ENABLED);
        if(((result!=MT2FW_MT2_OK&&!cause(observed[i]).empty())||(result==MT2FW_MT2_OK&&!enabled))&&failure.empty())
            failure="Framework batch enable failed or receipt differs: "+entry.name+" index="+std::to_string(i)+
                " result="+std::to_string(result)+" cause="+cause(observed[i]);
    }
    if(result!=MT2FW_MT2_OK&&failure.empty())failure="Framework batch enable failed result="+std::to_string(result)+"; provider returned no cause";
    if(!failure.empty()){
        for(size_t i=0;i<entries.size();++i){auto& entry=*entries[i];
            const auto state=observed[i].state;
            entry.state=(result!=MT2FW_MT2_OK&&(state==MT2FW_MT2_ROUTE_PREPARED||state==MT2FW_MT2_ROUTE_UNKNOWN)&&
                receipt(observed[i],targets[i],&entry,state))?state:MT2FW_MT2_ROUTE_UNKNOWN;
            entry.blocked=true;entry.cause=failure+"; target="+entry.name+" cause="+cause(observed[i]);
        }
        return fail("enable_batch",MH_UNKNOWN,failure);
    }
    for(auto* entry:entries)entry->state=MT2FW_MT2_ROUTE_ENABLED;
    error_.clear();return MH_OK;
}
bool Backend::last_batch_receipt(void* target,MT2FW_MT2_RouteReceipt& out)const {
    const auto found=entries_.find(reinterpret_cast<uintptr_t>(target));
    if(found==entries_.end()||!found->second.batch_receipt)return false;
    out=*found->second.batch_receipt;return true;
}
MH_STATUS Backend::disable(void* address) {
    if(!current("disable"))return MH_ERROR_NOT_INITIALIZED;
    if(address){auto it=entries_.find(reinterpret_cast<uintptr_t>(address));if(it==entries_.end())return fail("disable",MH_ERROR_NOT_CREATED,"Target has no local prepare receipt");return disable_one(it->first,it->second);}
    for(auto& [target,entry]:entries_){if(entry.state==MT2FW_MT2_ROUTE_REMOVED)continue;auto result=disable_one(target,entry);if(result!=MH_OK&&result!=MH_ERROR_DISABLED)return result;}
    error_.clear();return MH_OK;
}
MH_STATUS Backend::disable_one(uintptr_t target,Entry& entry) {
    if(entry.blocked)return fail("disable",MH_UNKNOWN,"Hook failure is retained: "+entry.cause);
    if(!entry.owned)return fail("disable",MH_ERROR_NOT_CREATED,"Hook is not owned by this plugin");
    auto before=fresh();const auto read=api_.route_read(api_.context,entry.name.c_str(),&before);
    if(read!=MT2FW_MT2_OK||!receipt(before,target,&entry,entry.state))
        return fail("disable",MH_UNKNOWN,"Framework current route differs before disable: "+cause(before),&entry);
    if(entry.state==MT2FW_MT2_ROUTE_PREPARED||entry.state==MT2FW_MT2_ROUTE_DISABLED_RETAINED)return MH_ERROR_DISABLED;
    if(!ownership_bound_)return fail("disable",MH_ERROR_UNSUPPORTED_FUNCTION,"Retaining disable requires the ownership service");
    if(entry.state!=MT2FW_MT2_ROUTE_ENABLED)return fail("disable",MH_UNKNOWN,"Hook is not enabled",&entry);
    auto after=fresh();const auto result=ownership_.hook_disable(ownership_.context,entry.name.c_str(),&after);
    if(result!=MT2FW_MT2_OK||!receipt(after,target,&entry,MT2FW_MT2_ROUTE_DISABLED_RETAINED))
        return fail("disable",MH_UNKNOWN,"Framework retaining disable failed or receipt differs: "+entry.name+" result="+std::to_string(result)+" cause="+cause(after),&entry);
    entry.state=after.state;error_.clear();return MH_OK;
}
MH_STATUS Backend::remove(void* address) {
    if(!current("remove"))return MH_ERROR_NOT_INITIALIZED;
    const auto target=reinterpret_cast<uintptr_t>(address);auto it=entries_.find(target);
    if(it==entries_.end())return fail("remove",MH_ERROR_NOT_CREATED,"Remove requires one locally prepared target; all/foreign removal forbidden");
    auto& entry=it->second;
    if(entry.blocked)return fail("remove",MH_UNKNOWN,"Hook failure is retained; no automatic removal: "+entry.cause);
    if(!entry.owned||entry.state==MT2FW_MT2_ROUTE_REMOVED)return fail("remove",MH_ERROR_NOT_CREATED,"Hook is not retained by this owner");
    std::string why;if(!reclaim_(user_,target,why))return fail("remove",MH_UNKNOWN,"Hook reclamation blocked: "+(why.empty()?"feature did not confirm quiescence and original release":why));
    auto before=fresh();auto result=api_.route_read(api_.context,entry.name.c_str(),&before);
    if(result!=MT2FW_MT2_OK||!receipt(before,target,&entry,entry.state))
        return fail("remove",MH_UNKNOWN,"Framework current owned route differs before removal: "+cause(before),&entry);
    auto after=fresh();result=api_.hook_remove(api_.context,entry.name.c_str(),&after);
    if(result!=MT2FW_MT2_OK||!receipt(after,target,&entry,MT2FW_MT2_ROUTE_REMOVED))
        return fail("remove",MH_UNKNOWN,"Framework removal failed or receipt differs: "+entry.name+" result="+std::to_string(result)+" cause="+cause(after),&entry);
    entry.state=after.state;entry.owned=false;error_.clear();return MH_OK;
}
size_t Backend::retained()const noexcept {size_t count=(free_attempted_&&(free_blocked_||free_.state!=MT2FW_MT2_ROUTE_REMOVED))?1u:0u;for(const auto& [target,entry]:entries_){(void)target;if(entry.owned||entry.blocked)++count;}return count;}
MH_STATUS Backend::cleanup() {
    if(!current("cleanup"))return MH_ERROR_NOT_INITIALIZED;
    if(retained())return fail("cleanup",MH_UNKNOWN,"Framework hook originals or failed attempts remain retained; explicitly retire each quiescent owner first");
    error_.clear();return MH_OK;
}
bool Backend::read_route(void* address,MT2FW_MT2_RouteReceipt& out) {
    if(!current("route_read"))return false;
    const auto target=reinterpret_cast<uintptr_t>(address);const auto* item=find(image_,target);
    if(!item){fail("route_read",MH_ERROR_UNSUPPORTED_FUNCTION,"Target has no pinned catalog name");return false;}
    const auto found=entries_.find(target);const Entry* entry=found==entries_.end()?nullptr:&found->second;
    if(entry&&entry->blocked){fail("route_read",MH_UNKNOWN,"Retained hook failure: "+entry->cause);return false;}
    auto observed=fresh();const auto result=api_.route_read(api_.context,item->name,&observed);
    const auto expected=entry?entry->state:(item->shared?MT2FW_MT2_ROUTE_ENABLED:MT2FW_MT2_ROUTE_PRISTINE);
    if(result!=MT2FW_MT2_OK||!receipt(observed,target,entry,expected)){
        fail("route_read",MH_UNKNOWN,"Framework route read failed or receipt differs: "+std::string(item->name)+" cause="+cause(observed));return false;}
    out=observed;error_.clear();return true;
}
bool Backend::verify_code(uintptr_t address,const void* expected,size_t length,std::string& error) {
    if(!current("code_verify")){error=error_;return false;}
    const auto* item=containing(image_,address,length);
    if(!item||!expected){fail("code_verify",MH_ERROR_UNSUPPORTED_FUNCTION,"Code window has no bounded catalog entry or expected bytes");error=error_;return false;}
    const auto target=image_+item->rva;
    const auto found=entries_.find(target);const Entry* entry=found==entries_.end()?nullptr:&found->second;
    if(entry&&entry->blocked){fail("code_verify",MH_UNKNOWN,"Retained hook failure: "+entry->cause);error=error_;return false;}
    auto observed=fresh();
    const auto result=api_.code_verify(api_.context,item->name,static_cast<uint32_t>(address-target),expected,static_cast<uint32_t>(length),&observed);
    const auto state=entry?entry->state:(item->shared?MT2FW_MT2_ROUTE_ENABLED:MT2FW_MT2_ROUTE_PRISTINE);
    if(result!=MT2FW_MT2_OK||!receipt(observed,target,entry,state)){
        fail("code_verify",MH_UNKNOWN,"Framework pristine/live code verification failed or receipt differs: "+std::string(item->name)+" result="+std::to_string(result)+" cause="+cause(observed));error=error_;return false;
    }
    error_.clear();error.clear();return true;
}
bool verify_code(uintptr_t address,const void* expected,size_t length,std::string& error) {
    return process_backend().verify_code(address,expected,length,error);
}
bool Backend::free_fail(const char* operation,const std::string& why,std::string& error,bool block) {
    if(block){free_blocked_=true;if(free_failure_.empty())free_failure_=why;}
    fail(operation,MH_UNKNOWN,why);error=error_;return false;
}
bool Backend::free_receipt(const MT2FW_MT2_FreeReceipt& value,uint32_t state,uintptr_t replacement)const {
    if(value.size!=sizeof value||value.state!=state||!value.generation||!equal(value.game_sha256,game_sha)||
       value.slot!=image_+0x1c52a00||value.malloc_slot!=image_+0x1c52a70||!value.original||!value.malloc_original||
       value.replacement!=replacement||value.original==replacement||!value.live_confirmed||!value.initial_protection||
       value.current_protection!=value.initial_protection||
       value.current!=(state==MT2FW_MT2_ROUTE_ENABLED?replacement:value.original))return false;
    if(state==MT2FW_MT2_ROUTE_REMOVED){if(value.owner[0])return false;}
    else if(!equal(value.owner,owner_.c_str()))return false;
    if(!module_proof(value.imported_crt)||!module_proof(value.free_provider)||!module_proof(value.malloc_provider)||
       value.free_export.resolved!=value.original||value.free_export.owner!=value.free_provider.base||
       value.malloc_export.resolved!=value.malloc_original||value.malloc_export.owner!=value.malloc_provider.base||
       !std::memchr(value.free_export.forwarder,0,sizeof value.free_export.forwarder)||
       !std::memchr(value.malloc_export.forwarder,0,sizeof value.malloc_export.forwarder))return false;
    if(state==MT2FW_MT2_ROUTE_ENABLED&&(!value.exchange_attempted||!value.exchanged||!value.protection_restored))return false;
    if(free_.generation&&(value.generation!=free_.generation||value.original!=free_.original||
       value.initial_protection!=free_.initial_protection||value.malloc_original!=free_.malloc_original||
       !same_module(value.imported_crt,free_.imported_crt)||!same_module(value.free_provider,free_.free_provider)||
       !same_module(value.malloc_provider,free_.malloc_provider)||!same_export(value.free_export,free_.free_export)||
       !same_export(value.malloc_export,free_.malloc_export)))return false;
    return true;
}
bool Backend::prepare_free(MT2FW_MT2_FreeFn replacement,MT2FW_MT2_FreeReceipt& out,std::string& error) {
    if(!current("free_prepare")){error=error_;return false;}
    if(!ownership_bound_||!replacement)return free_fail("free_prepare","Ownership service and exact existing free observer are required",error,false);
    if(free_attempted_)return free_fail("free_prepare","Free prepare already attempted; no replay",error,false);
    free_attempted_=true;auto observed=fresh_free();
    const auto result=ownership_.free_prepare(ownership_.context,replacement,&observed);out=observed;
    if(result!=MT2FW_MT2_OK||!free_receipt(observed,MT2FW_MT2_ROUTE_PREPARED,reinterpret_cast<uintptr_t>(replacement)))
        return free_fail("free_prepare","Framework free prepare failed or receipt differs: result="+std::to_string(result)+" cause="+free_cause(observed),error,true);
    free_=observed;error_.clear();error.clear();return true;
}
bool Backend::read_free(MT2FW_MT2_FreeReceipt& out,std::string& error) {
    if(!current("free_read")){error=error_;return false;}
    if(!ownership_bound_||!free_attempted_||!free_.generation)return free_fail("free_read","No prepared Framework free receipt",error,false);
    if(free_blocked_)return free_fail("free_read","Free failure is retained; no replay: "+free_failure_,error,false);
    if(free_.state==MT2FW_MT2_ROUTE_REMOVED)return free_fail("free_read","Free metadata is retired",error,false);
    auto observed=fresh_free();const auto result=ownership_.free_read(ownership_.context,&observed);out=observed;
    if(result!=MT2FW_MT2_OK||!free_receipt(observed,free_.state,free_.replacement))
        return free_fail("free_read","Framework live free receipt differs: result="+std::to_string(result)+" cause="+free_cause(observed),error,true);
    free_=observed;error_.clear();error.clear();return true;
}
bool Backend::free_change(const char* operation,uint32_t state,MT2FW_MT2_Result (MT2FW_CALL *call)(void*,MT2FW_MT2_FreeReceipt*),MT2FW_MT2_FreeReceipt& out,std::string& error) {
    MT2FW_MT2_FreeReceipt before{};const bool admitted=read_free(before,error);out=before;if(!admitted)return false;
    if((state==MT2FW_MT2_ROUTE_ENABLED||state==MT2FW_MT2_ROUTE_REMOVED)&&
       before.state!=MT2FW_MT2_ROUTE_PREPARED&&before.state!=MT2FW_MT2_ROUTE_DISABLED_RETAINED)
        return free_fail(operation,"Free transition requires a prepared or disabled-retained import",error,false);
    if(state==MT2FW_MT2_ROUTE_REMOVED){std::string why;if(!reclaim_(user_,free_.slot,why))
        return free_fail(operation,"Free metadata reclamation blocked: "+(why.empty()?"feature did not confirm original release":why),error,false);}
    auto observed=fresh_free();const auto result=call(ownership_.context,&observed);out=observed;
    if(result!=MT2FW_MT2_OK||!free_receipt(observed,state,free_.replacement))
        return free_fail(operation,"Framework free transition failed or receipt differs: result="+std::to_string(result)+" cause="+free_cause(observed),error,true);
    free_=observed;error_.clear();error.clear();return true;
}
bool Backend::enable_free(MT2FW_MT2_FreeReceipt& out,std::string& error) {
    return free_change("free_enable",MT2FW_MT2_ROUTE_ENABLED,ownership_.free_enable,out,error);
}
bool Backend::disable_free(MT2FW_MT2_FreeReceipt& out,std::string& error) {
    return free_change("free_disable",MT2FW_MT2_ROUTE_DISABLED_RETAINED,ownership_.free_disable,out,error);
}
bool Backend::remove_free(MT2FW_MT2_FreeReceipt& out,std::string& error) {
    return free_change("free_remove",MT2FW_MT2_ROUTE_REMOVED,ownership_.free_remove,out,error);
}
const char* status_text(MH_STATUS value)noexcept {
    switch(value){case MH_OK:return "MH_OK (plugin-owned Framework operation)";case MH_UNKNOWN:return "Framework hook failure; inspect retained backend cause";
    case MH_ERROR_ALREADY_INITIALIZED:return "Framework backend already bound";case MH_ERROR_NOT_INITIALIZED:return "Framework backend unavailable or wrong thread";
    case MH_ERROR_ALREADY_CREATED:return "Hook preparation already attempted; no replay";case MH_ERROR_NOT_CREATED:return "No local owned hook";
    case MH_ERROR_ENABLED:return "Own hook already enabled";case MH_ERROR_DISABLED:return "Own hook is already disabled or removed";
    case MH_ERROR_NOT_EXECUTABLE:return "Invalid target or publication arguments";case MH_ERROR_UNSUPPORTED_FUNCTION:return "Typed shared route, catalog admission or retaining-disable service required";
    default:return "Unsupported hook compatibility status";}
}
}
