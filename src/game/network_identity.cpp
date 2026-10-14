#include "network_identity.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/game/normal_load_identity.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/core/hash.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <vector>
#include <limits>

namespace mt2mp::game::network_identity {
namespace {
constexpr uintptr_t capture_rva=0xddda40,node_property=0x13bf740,property_vtable=0x1710270;
constexpr uintptr_t map_slot=0x13df8c0,map_vtable=0x1742860,region_vtable=0x1787930;
constexpr uintptr_t manager_vtable=0x17141e0,array_vtable=0x172db20,node_vtable=0x16fc700,shard_vtable=0x174ad20;
constexpr uintptr_t contains_rva=0x89e690,lookup_rva=0x47ce20,owns_rva=0x2a4730,context_lookup_rva=0x89e0f0;
std::atomic<uintptr_t> image{0};
std::atomic<bool> capture_active{false},client_active{false};
std::atomic<uint64_t> selector_reads{0},non_node_callbacks{0},client_generation{0};
#ifdef MT2MP_NETWORK_IDENTITY_TEST
std::atomic<uint64_t> test_read_queries{0};
uintptr_t test_base=0; testing::Contains test_contains=nullptr; testing::Lookup test_lookup=nullptr;
testing::SelectorBarrier test_selector_barrier=nullptr;
testing::Owns test_owns=nullptr; testing::ContextLookup test_context_lookup=nullptr;
#endif
struct Binding {
    int32_t id=0,shard_id=0,region_id=-1;
    uint64_t uid=0;
    void* node=nullptr; void* region=nullptr; void* shard=nullptr; const void* manager=nullptr; void* context=nullptr;
};
struct Roots { void* map=nullptr; void* shard=nullptr; void** array=nullptr; int32_t count=0; std::vector<void*> regions; };
struct WriterBinding { void* stream=nullptr; void* consumer=nullptr; void* file=nullptr; void* task=nullptr; };
struct Pending { const void* property=nullptr; const void* manager=nullptr; void* writer=nullptr; WriterBinding file_binding; void* context=nullptr; DWORD thread=0; std::vector<Binding> nodes; };
// PE SizeOfImage for the exact supported executable bounds diagnostic RVAs.
constexpr uintptr_t diagnostic_image_bytes=30986240;
struct ContextProbe {
    uint64_t caller_rva=0,typed_vtable_rva=0,mode_vtable_rva=0;
    uint32_t context_ordinal=0;
    bool callback_context_present=false,mode_slot_readable=false,mode_present=false,mode_vtable_attempted=false,mode_vtable_readable=false,mode_vtable_in_image=false,mode_vtable_matches=false;
    bool mode_context_attempted=false,mode_context_readable=false,mode_context_present=false,mode_context_matches=false;
    bool object_context_readable=false,object_context_present=false,object_context_matches=false;
};
uint64_t diagnostic_rva(uintptr_t pointer) { const auto base=image.load(); return pointer>=base&&pointer-base<diagnostic_image_bytes?pointer-base:0; }
struct ContextOwner { DWORD thread=0; uint32_t ordinal=0; };
struct Shared {
    std::mutex mutex;
    DWORD main_thread=0,first_observer_thread=0,last_observer_thread=0;
    uint64_t obsolete_callbacks=0,typed_worker_callbacks=0,typed_main_callbacks=0,duplicate_callbacks=0;
    void* client_load_mode=nullptr;
    std::map<void*,ContextOwner> context_owners;
    std::set<DWORD> observer_threads;
    ContextProbe first_context_probe,last_context_probe;
    std::string state="idle",uuid,sha,error,role,keys_digest,filename,foreign_reason,invalidation_reason;
    uint32_t expected=0;
    uint64_t next_ticket=1,completed_arrays=0,discarded=0,foreign_writers=0;
    size_t pending_nodes=0;
    Roots roots;
    std::map<uint64_t,Pending> pending;
    std::map<int32_t,Binding> bindings;
    std::map<std::pair<void*,int32_t>,void*> shards;
    std::set<void*> contexts;
};
// Save callbacks can outlive orderly DLL shutdown; no destructor runs under process-detach loader lock.
Shared& shared() { static auto* value=new Shared; return *value; }
bool valid_sha(const std::string& value) { return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos; }
bool valid_uuid(const std::string& value) { return value.size()==36&&value[8]=='-'&&value[13]=='-'&&value[18]=='-'&&value[23]=='-'&&value.find_first_not_of("0123456789abcdef-")==std::string::npos; }
bool fail_locked(Shared& s,const std::string& cause,std::string* error=nullptr) {
    if(s.state!="failed") {
        s.state="failed"; s.error=cause; s.keys_digest.clear(); capture_active=false; client_active=false;
        LOG_ERROR("[NetworkIdentity] Rejected role={} uuid={}: {}",s.role,s.uuid,cause);
    } else ++s.discarded;
    if(error) *error=s.error;
    return false;
}
bool readable(const void* pointer,size_t bytes) {
    if(!pointer||!bytes) return false;
    auto address=reinterpret_cast<uintptr_t>(pointer);
    if(bytes>(std::numeric_limits<uintptr_t>::max)()-address) return false;
    const auto end=address+bytes;
    while(address<end) {
        MEMORY_BASIC_INFORMATION memory{};
#ifdef MT2MP_NETWORK_IDENTITY_TEST
        ++test_read_queries;
#endif
        if(!VirtualQuery(reinterpret_cast<void*>(address),&memory,sizeof(memory))||memory.State!=MEM_COMMIT||memory.Protect&(PAGE_GUARD|PAGE_NOACCESS)) return false;
        const DWORD protection=memory.Protect&0xff;
        if(protection!=PAGE_READONLY&&protection!=PAGE_READWRITE&&protection!=PAGE_WRITECOPY&&protection!=PAGE_EXECUTE_READ&&protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY) return false;
        const auto next=reinterpret_cast<uintptr_t>(memory.BaseAddress)+memory.RegionSize;
        if(next<=address) return false;
        address=std::min(end,next);
    }
    return true;
}
DWORD copy_native(const void* pointer,void* output,size_t bytes) {
    __try { std::memcpy(output,pointer,bytes); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
template<class T> bool read_at(const void* pointer,size_t offset,T& output) {
    const auto address=reinterpret_cast<uintptr_t>(pointer);
    if(!pointer||offset>(std::numeric_limits<uintptr_t>::max)()-address) return false;
    auto* target=reinterpret_cast<const void*>(address+offset);
    return readable(target,sizeof(T))&&copy_native(target,&output,sizeof(T))==0;
}
bool vtable(const void* pointer,uintptr_t offset) { uintptr_t value=0; return read_at(pointer,0,value)&&value==image.load()+offset; }
DWORD contains_native(void* context,const void* object,int32_t& id,bool& result) {
    __try {
#ifdef MT2MP_NETWORK_IDENTITY_TEST
        result=test_contains(context,object,&id);
#else
        result=reinterpret_cast<bool(*)(void*,const void*,int32_t*)>(image.load()+contains_rva)(context,object,&id);
#endif
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
DWORD lookup_native(uint64_t uid,void*& result) {
    __try {
#ifdef MT2MP_NETWORK_IDENTITY_TEST
        result=test_lookup(uid);
#else
        result=reinterpret_cast<void*(*)(uint64_t)>(image.load()+lookup_rva)(uid);
#endif
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
DWORD owns_native(const void* manager,void* node,bool& result) {
    __try {
#ifdef MT2MP_NETWORK_IDENTITY_TEST
        result=test_owns(manager,node);
#else
        result=reinterpret_cast<bool(*)(const void*,void*)>(image.load()+owns_rva)(manager,node);
#endif
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
DWORD context_native(void* context,int32_t id,void*& result) {
    __try {
#ifdef MT2MP_NETWORK_IDENTITY_TEST
        result=test_context_lookup(context,id);
#else
        result=reinterpret_cast<void*(*)(void*,int32_t)>(image.load()+context_lookup_rva)(context,id);
#endif
        return 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
bool main_gate(Shared& s,bool world,std::string& error,bool idle_save=false,bool client_load_entry=false) {
    if(!image.load()||!native::ready()) return fail_locked(s,"exact native build resolver unavailable",&error);
    native::WorldStatus status;
    if(!native::read_world_status(status,error)) return fail_locked(s,"mainthread gate: "+error,&error);
    if(s.main_thread&&s.main_thread!=GetCurrentThreadId()) return fail_locked(s,"native game thread identity changed",&error);
    s.main_thread=GetCurrentThreadId();
    if(world&&(!native::in_game()||!status.world_present||status.quitting)) return fail_locked(s,"active native root world required",&error);
    if(client_load_entry) {
        const bool stable=status.active_mode_index==status.requested_mode_index&&!status.quitting&&!status.title_transition_pending&&!status.crash_report_active;
        const bool title=stable&&status.active_mode_index==2&&!status.world_present&&status.saves_in_progress==-1;
        const bool game=stable&&status.active_mode_index==3&&status.world_present&&native::in_game()&&status.saves_in_progress==0;
        if(!title&&!game) return fail_locked(s,"client pre-load entry requires stable title2/no-world/saves=-1 or active game3/saves=0; active="+std::to_string(status.active_mode_index)+" requested="+std::to_string(status.requested_mode_index)+" world="+std::to_string(status.world_present)+" quitting="+std::to_string(status.quitting)+" title_pending="+std::to_string(status.title_transition_pending)+" crash="+std::to_string(status.crash_report_active)+" saves="+std::to_string(status.saves_in_progress),&error);
        return true;
    }
    if(idle_save&&status.saves_in_progress!=0) return fail_locked(s,"native save still active actual="+std::to_string(status.saves_in_progress)+" required=0",&error);
    return true;
}
bool roots_read(Roots& roots,std::string& cause) {
    const auto base=image.load();
    if(!read_at(reinterpret_cast<void*>(base),map_slot,roots.map)||!vtable(roots.map,map_vtable)) { cause="root map singleton/vtable differs"; return false; }
    roots.shard=static_cast<uint8_t*>(roots.map)+0x1c0;
    int32_t type=-1,allocated=-1;
    if(!vtable(roots.shard,shard_vtable)||!read_at(roots.shard,0x48,type)||type!=0||!read_at(roots.map,0x358,roots.array)||!read_at(roots.map,0x360,roots.count)||!read_at(roots.map,0x364,allocated)||roots.count<=0||roots.count>int32_t(maximum_regions)||allocated<roots.count||!readable(roots.array,size_t(roots.count)*sizeof(void*))) {
        cause="root shard/region array guard failed count="+std::to_string(roots.count)+" allocated="+std::to_string(allocated); return false;
    }
    roots.regions.clear(); roots.regions.reserve(size_t(roots.count));
    for(int32_t index=0;index<roots.count;++index) {
        void* region=nullptr; void* shard=nullptr; int32_t id=-1;
        if(!read_at(roots.array,size_t(index)*sizeof(void*),region)||!vtable(region,region_vtable)||!read_at(region,0x70,shard)||shard!=roots.shard||!read_at(region,0x834,id)||id<0||std::find(roots.regions.begin(),roots.regions.end(),region)!=roots.regions.end()) {
            cause="root region identity/membership differs index="+std::to_string(index); return false;
        }
        roots.regions.push_back(region);
    }
    return true;
}
bool node_read(void* node,const Roots& roots,Binding& binding,std::string& cause) {
    binding.node=node;
    if(!vtable(node,node_vtable)||!read_at(node,0x18,binding.uid)||!binding.uid||!read_at(node,0x178,binding.region)||!read_at(node,0x170,binding.shard)||binding.shard!=roots.shard||std::find(roots.regions.begin(),roots.regions.end(),binding.region)==roots.regions.end()||!vtable(binding.region,region_vtable)||!read_at(binding.region,0x834,binding.region_id)||binding.region_id<0) {
        cause="typed node UID/root/region guard failed"; return false;
    }
    void* shard=nullptr; void* manager=nullptr; void* owner=nullptr;
    if(!read_at(binding.region,0x70,shard)||shard!=binding.shard||!read_at(binding.region,0x978,manager)||!vtable(manager,manager_vtable)||!read_at(manager,0x168,owner)||owner!=binding.region) { cause="node region/manager owner differs"; return false; }
    binding.manager=manager;
    void** nodes=nullptr; int32_t count=-1,allocated=-1;
    if(!read_at(manager,0x20,nodes)||!read_at(manager,0x28,count)||!read_at(manager,0x2c,allocated)||count<0||count>int32_t(maximum_nodes)||allocated<count||(count&&!readable(nodes,size_t(count)*sizeof(void*)))) { cause="OwnsNode array bounds invalid count="+std::to_string(count)+" allocated="+std::to_string(allocated); return false; }
    bool owns=false; const auto exception=owns_native(manager,node,owns);
    if(exception||!owns) { cause="native OwnsNode rejected exception="+std::to_string(exception); return false; }
    return true;
}
bool array_read(const void* manager,std::vector<void*>& nodes,std::string& cause) {
    if(!vtable(manager,manager_vtable)) { cause="network manager vtable differs"; return false; }
    auto* array=static_cast<const uint8_t*>(manager)+0x18;
    void** data=nullptr; int32_t count=-1,allocated=-1;
    if(!vtable(array,array_vtable)||!read_at(array,8,data)||!read_at(array,0x10,count)||!read_at(array,0x14,allocated)||count<0||count>int32_t(maximum_nodes)||allocated<count||(count&&!readable(data,size_t(count)*sizeof(void*)))) { cause="node array guard count="+std::to_string(count)+" allocated="+std::to_string(allocated); return false; }
    nodes.clear(); nodes.reserve(size_t(count));
    for(int32_t index=0;index<count;++index) {
        void* node=nullptr;
        if(!read_at(data,size_t(index)*sizeof(void*),node)) { cause="node array slot unreadable index="+std::to_string(index); return false; }
        if(node) nodes.push_back(node);
    }
    return true;
}
bool census(const Roots& roots,std::map<uint64_t,Binding>& nodes,std::string& cause) {
    nodes.clear(); std::set<int32_t> region_ids;
    for(auto* region:roots.regions) {
        int32_t region_id=-1; void* manager=nullptr;
        if(!read_at(region,0x834,region_id)||!region_ids.insert(region_id).second||!read_at(region,0x978,manager)) { cause="root region id duplicate or manager unreadable"; return false; }
        if(!manager) continue;
        std::vector<void*> array;
        if(!array_read(manager,array,cause)) return false;
        for(void* node:array) {
            Binding binding;
            if(!node_read(node,roots,binding,cause)) return false;
            if(binding.region!=region) { cause="census node region differs from containing array"; return false; }
            if(nodes.size()>=maximum_nodes||!nodes.emplace(binding.uid,binding).second) { cause="root node UID duplicate or count exceeds65536"; return false; }
        }
    }
    return true;
}
bool add_context(Shared& s,void* context,std::string& cause) {
    if(!context) { cause="object context missing"; return false; }
    if(!s.contexts.contains(context)&&s.contexts.size()>=maximum_contexts) { cause="context count exceeds64"; return false; }
    s.contexts.insert(context); return true;
}
bool property_guard(const void* property,const void* manager,std::string& cause) {
    uint64_t offset=0; uint8_t skipped=1;
    if(property!=reinterpret_cast<void*>(image.load()+node_property)||!vtable(property,property_vtable)||!read_at(property,0x38,offset)||offset!=0x18||!read_at(property,0x20,skipped)||skipped!=0||!vtable(manager,manager_vtable)) { cause="typed node property/offset/skipflag/manager differs"; return false; }
    return true;
}
enum class WriterScope { Own, Foreign, Invalid };
WriterScope writer_scope(const Shared& s,void* writer,WriterBinding& binding,std::string& cause) {
    auto& stream=binding.stream; auto& consumer=binding.consumer; auto& file=binding.file; auto& task=binding.task;
    uintptr_t consumer_vptr=0;
    if(!read_at(writer,8,stream)||!vtable(stream,0x1736320)||!read_at(stream,0x18,consumer)||!read_at(consumer,0,consumer_vptr)) {
        cause="save writer serialiser/consumer chain unreadable or vtable differs"; return WriterScope::Invalid;
    }
    if(consumer_vptr==image.load()+0x1742fd0) { cause="metadata direct-file writer"; return WriterScope::Foreign; }
    if(consumer_vptr!=image.load()+0x1720790||!read_at(consumer,8,file)||!read_at(consumer,0x10,task)) {
        cause="save writer compressed consumer/file chain unreadable or vtable differs"; return WriterScope::Invalid;
    }
    if(file&&task) { cause="save writer direct file and task both present"; return WriterScope::Invalid; }
    if(!file) {
        uint8_t active=0;
        if(!task||!vtable(task,0x1733bb0)||!read_at(task,0x48,active)||active!=1) { cause="save writer task vtable/producer lifetime differs"; return WriterScope::Invalid; }
        if(!read_at(task,0x10,file)||!file) { cause="save writer active task file absent/unreadable"; return WriterScope::Invalid; }
    }
    if(!vtable(file,0x1742fd0)) { cause="save writer owned file vtable differs"; return WriterScope::Invalid; }
    const char* text=nullptr; size_t bytes=0; int32_t mode=-1;
    if(!read_at(file,8,text)||!read_at(file,0x10,bytes)) { cause="native save filename header unreadable"; return WriterScope::Invalid; }
    if(bytes!=s.filename.size()) { cause="foreign native filename length"; return WriterScope::Foreign; }
    if(bytes>256||!readable(text,bytes)) { cause="native save filename bytes unreadable or exceed256"; return WriterScope::Invalid; }
    std::array<char,256> value{};
    const auto exception=copy_native(text,value.data(),bytes);
    if(exception) { cause="native save filename read exception="+std::to_string(exception); return WriterScope::Invalid; }
    if(memcmp(value.data(),s.filename.data(),bytes)) { cause="foreign native filename/UUID"; return WriterScope::Foreign; }
    if(!read_at(file,0x70,mode)||mode!=4) { cause="owned native file write mode actual="+std::to_string(mode)+" required=4"; return WriterScope::Invalid; }
    return WriterScope::Own;
}
void reset(Shared& s,const std::string& uuid,const std::string& sha,uint32_t expected,const std::string& role) {
    s.uuid=uuid; s.sha=sha; s.expected=expected; s.role=role; s.error.clear(); s.keys_digest.clear(); s.invalidation_reason.clear(); s.discarded=0;
    ++client_generation; s.first_context_probe={}; s.last_context_probe={}; s.context_owners.clear(); s.observer_threads.clear(); s.client_load_mode=nullptr; s.first_observer_thread=s.last_observer_thread=0; s.obsolete_callbacks=s.typed_worker_callbacks=s.typed_main_callbacks=s.duplicate_callbacks=0;
    s.pending.clear(); s.pending_nodes=0; s.bindings.clear(); s.shards.clear(); s.contexts.clear(); s.completed_arrays=0; s.roots={};
    s.filename="user/save/MPTestSession/snapshot-"+uuid+".vrz"; s.foreign_writers=0; s.foreign_reason.clear();
    selector_reads=0; non_node_callbacks=0;
}
bool publish(Shared& s,const Roots& roots,std::string& error) {
    std::map<uint64_t,Binding> live; std::string cause;
    if(!census(roots,live,cause)) return fail_locked(s,"mainthread census: "+cause,&error);
    if(s.bindings.size()!=s.expected||live.size()!=s.expected) return fail_locked(s,"identity count staged="+std::to_string(s.bindings.size())+" native_root="+std::to_string(live.size())+" expected="+std::to_string(s.expected)+" completed_arrays="+std::to_string(s.completed_arrays),&error);
    std::set<uint64_t> uids; std::vector<uint8_t> digest_bytes(s.sha.begin(),s.sha.end());
    for(auto& [id,binding]:s.bindings) {
        void* resolved=nullptr; const auto exception=lookup_native(binding.uid,resolved);
        if(exception||resolved!=binding.node) return fail_locked(s,"node lifetime/UID lookup differs id="+std::to_string(id)+" exception="+std::to_string(exception),&error);
        const auto found=live.find(binding.uid);
        if(found==live.end()||found->second.node!=binding.node||!uids.insert(binding.uid).second) return fail_locked(s,"node UID census missing/replaced/aliased id="+std::to_string(id),&error);
        const auto& actual=found->second;
        if(s.role=="host") {
            if(binding.region!=actual.region||binding.shard!=actual.shard||binding.region_id!=actual.region_id||binding.manager!=actual.manager) return fail_locked(s,"captured node membership changed id="+std::to_string(id),&error);
        } else {
            int32_t shard_id=0;
            for(const auto& [key,pointer]:s.shards) if(key.first==binding.context&&pointer==roots.shard) {
                if(shard_id&&shard_id!=key.second) return fail_locked(s,"root shard serialized id ambiguous",&error);
                shard_id=key.second;
            }
            if(!shard_id) return fail_locked(s,"client root shard binding missing id="+std::to_string(id),&error);
            binding.region=actual.region; binding.shard=actual.shard; binding.region_id=actual.region_id; binding.manager=actual.manager; binding.shard_id=shard_id;
        }
        for(int32_t value:{id,binding.shard_id,binding.region_id}) for(int shift=24;shift>=0;shift-=8) digest_bytes.push_back(uint8_t(uint32_t(value)>>shift));
    }
    s.keys_digest=sha256(digest_bytes); s.roots=roots; error.clear();
    s.state=s.role=="host"?"host_ready":"client_ready"; capture_active=false; client_active=false;
    LOG_INFO("[NetworkIdentity] Ready role={} uuid={} sha={} nodes={} key_digest={}",s.role,s.uuid,s.sha,s.bindings.size(),s.keys_digest);
    return true;
}
}
bool initialize(std::string& error) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    if(image.load()) { error.clear(); return true; }
    if(!native::ready()) return fail_locked(s,"exact native resolver unavailable during initialize",&error);
#ifdef MT2MP_NETWORK_IDENTITY_TEST
    const auto base=test_base;
#else
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
#endif
    if(!base||native::entrypoint(native::EntryPoint::GameUpdate)!=reinterpret_cast<void*>(base+0x463ce0)) return fail_locked(s,"native module/entrypoint mismatch",&error);
    struct Guard { uintptr_t rva; const char* bytes; };
    const Guard guards[]={
        {capture_rva,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x48"},
        {contains_rva,"\x57\x56\x53\x48\x83\xec\x20\x48\x89\xce\x48\x89\xd3\x4c\x89\xc7"},
        {lookup_rva,"\x53\x45\x31\xc0\x48\x85\xc9\x74\x31\x48\x83\x3d\xa7\x95\x5b\x01"},
        {owns_rva,"\x4c\x63\x41\x28\x45\x85\xc0\x7e\x27\x48\x8b\x41\x20\x4a\x8d\x0c"},
        {context_lookup_rva,"\x53\x48\x8b\x09\x48\x83\x79\x18\x00\x41\x89\xd0\x75\x32\x48\x8b"}};
    for(const auto& guard:guards) {
        uint8_t bytes[16]{};
        if(!readable(reinterpret_cast<void*>(base+guard.rva),16)||copy_native(reinterpret_cast<void*>(base+guard.rva),bytes,16)||memcmp(bytes,guard.bytes,16)) return fail_locked(s,"entry prologue mismatch rva="+std::to_string(guard.rva),&error);
    }
    image=base; s.state="idle"; s.error.clear(); error.clear(); return true;
}
void* capture_entrypoint() { return image.load()?reinterpret_cast<void*>(image.load()+capture_rva):nullptr; }
bool begin_host_capture(const std::string& uuid,uint32_t expected,std::string& error) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    if(capture_active||client_active||!s.pending.empty()) return fail_locked(s,"identity operation or original callback still active",&error);
    if(!valid_uuid(uuid)||expected>maximum_nodes) return fail_locked(s,"host capture UUID/count invalid",&error);
    reset(s,uuid,"",expected,"host"); s.state="capturing";
    if(!main_gate(s,true,error,true)) return false;
    std::string cause; std::map<uint64_t,Binding> live;
    if(!roots_read(s.roots,cause)||!census(s.roots,live,cause)) return fail_locked(s,"host capture root baseline: "+cause,&error);
    if(live.size()!=expected) return fail_locked(s,"host native_root_nodes="+std::to_string(live.size())+" expected="+std::to_string(expected),&error);
    capture_active=true; error.clear(); return true;
}
uint64_t capture_before(const void* property,const void* manager,void* writer,void* context) noexcept {
    if(!capture_active.load()) return 0;
    auto& s=shared(); std::lock_guard lock(s.mutex);
    uint64_t ticket=0;
    try {
        if(!capture_active.load()) return 0;
        std::string cause;
        WriterBinding file_binding; const auto scope=writer_scope(s,writer,file_binding,cause);
        if(scope==WriterScope::Foreign) {
            ++s.foreign_writers;
            if(s.foreign_reason!=cause) { s.foreign_reason=cause; LOG_INFO("[NetworkIdentity] Foreign writer skipped uuid={} reason={} count={}; native save unchanged",s.uuid,cause,s.foreign_writers); }
            return 0;
        }
        if(scope==WriterScope::Invalid) { fail_locked(s,cause); return 0; }
        if(s.pending.size()>=maximum_pending_arrays) { fail_locked(s,"pending array count exceeds64"); return 0; }
        Pending pending; pending.property=property; pending.manager=manager; pending.writer=writer; pending.file_binding=file_binding; pending.context=context; pending.thread=GetCurrentThreadId();
        ticket=s.next_ticket++; s.pending.emplace(ticket,pending);
        if(!property_guard(property,manager,cause)||!add_context(s,context,cause)) { fail_locked(s,cause); return ticket; }
        std::vector<void*> nodes;
        if(!array_read(manager,nodes,cause)) { fail_locked(s,cause); return ticket; }
        for(auto* node:nodes) {
            if(s.bindings.size()+s.pending_nodes+pending.nodes.size()>=maximum_nodes) { fail_locked(s,"host staged plus pending node count exceeds65536"); return ticket; }
            Binding binding; bool present=false;
            if(!node_read(node,s.roots,binding,cause)) { fail_locked(s,"host pre: "+cause); return ticket; }
            if(binding.manager!=manager) { fail_locked(s,"host node parent manager differs from property receiver"); return ticket; }
            const auto exception=contains_native(context,node,binding.id,present);
            if(exception||!present||binding.id<=0) { fail_locked(s,"host explicit node id absent/invalid exception="+std::to_string(exception)); return ticket; }
            bool shard_present=false; const auto shard_exception=contains_native(context,binding.shard,binding.shard_id,shard_present);
            if(shard_exception||!shard_present||binding.shard_id<=0) { fail_locked(s,"host root shard serialized id absent/invalid exception="+std::to_string(shard_exception)); return ticket; }
            binding.context=context; pending.nodes.push_back(binding);
        }
        if(s.bindings.size()+pending.nodes.size()>maximum_nodes) { fail_locked(s,"host staged node count exceeds65536"); return ticket; }
        s.pending_nodes+=pending.nodes.size(); s.pending.at(ticket)=std::move(pending); return ticket;
    } catch(const std::exception& error) { fail_locked(s,std::string("host capture_before exception: ")+error.what()); return ticket; }
}
void capture_after(uint64_t ticket,const void* property,const void* manager,void* writer,void* context,bool original_result) noexcept {
    if(!ticket) return;
    auto& s=shared(); std::lock_guard lock(s.mutex);
    try {
        const auto found=s.pending.find(ticket);
        if(found==s.pending.end()) { fail_locked(s,"host capture_after unknown/repeated ticket"); return; }
        Pending pending=std::move(found->second); s.pending_nodes-=pending.nodes.size(); s.pending.erase(found);
        if(s.state=="failed"||s.state=="invalidated") { ++s.discarded; return; }
        if(!original_result) { fail_locked(s,"host native SaveStream returned false"); return; }
        if(pending.property!=property||pending.manager!=manager||pending.writer!=writer||pending.context!=context) { fail_locked(s,"host capture_after property/manager/writer/context identity differs"); return; }
        if(pending.thread!=GetCurrentThreadId()) { fail_locked(s,"host capture_after worker thread changed"); return; }
        std::string cause; std::vector<void*> actual;
        WriterBinding after_writer;
        if(writer_scope(s,writer,after_writer,cause)!=WriterScope::Own) { fail_locked(s,"owned writer changed during original: "+cause); return; }
        if(after_writer.stream!=pending.file_binding.stream||after_writer.consumer!=pending.file_binding.consumer||after_writer.file!=pending.file_binding.file) { fail_locked(s,"owned stream/consumer/file identity changed during original"); return; }
        if(pending.file_binding.task&&after_writer.task!=pending.file_binding.task) { fail_locked(s,"owned writer task replaced or reverted during original"); return; }
        if(!property_guard(property,manager,cause)||!array_read(manager,actual,cause)) { fail_locked(s,"host post array guard: "+cause); return; }
        if(actual.size()!=pending.nodes.size()) { fail_locked(s,"host array nodes changed actual="+std::to_string(actual.size())+" expected="+std::to_string(pending.nodes.size())); return; }
        for(size_t index=0;index<actual.size();++index) {
            const auto& before=pending.nodes[index]; Binding after; bool present=false; int32_t id=0,shard_id=0;
            if(actual[index]!=before.node||!node_read(actual[index],s.roots,after,cause)||after.uid!=before.uid||after.region!=before.region||after.shard!=before.shard||after.region_id!=before.region_id||after.manager!=before.manager) { fail_locked(s,"host node identity/membership changed during original"); return; }
            const auto exception=contains_native(context,after.node,id,present);
            bool shard_present=false; const auto shard_exception=contains_native(context,after.shard,shard_id,shard_present);
            if(exception||shard_exception||!present||!shard_present||id!=before.id||shard_id!=before.shard_id) { fail_locked(s,"host serialized id changed during original exception="+std::to_string(exception?exception:shard_exception)); return; }
            if(!s.bindings.emplace(before.id,before).second) { fail_locked(s,"host duplicate serialized node id="+std::to_string(before.id)); return; }
        }
        ++s.completed_arrays;
    } catch(const std::exception& error) { fail_locked(s,std::string("host capture_after exception: ")+error.what()); }
}
void original_failed(uint64_t ticket,const std::string& cause) noexcept {
    if(!ticket) return;
    auto& s=shared(); std::lock_guard lock(s.mutex);
    const auto found=s.pending.find(ticket);
    if(found==s.pending.end()) { fail_locked(s,"original failure references unknown/repeated ticket: "+cause); return; }
    s.pending_nodes-=found->second.nodes.size(); s.pending.erase(found);
    fail_locked(s,"native save original exception: "+cause);
}
bool seal_host_capture(const std::string& uuid,const std::string& sha,std::string& error) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    if(s.state=="failed") { error=s.error; return false; }
    if(!main_gate(s,true,error,true)) return false;
    if(!capture_active||s.role!="host"||s.uuid!=uuid||!valid_sha(sha)||!s.pending.empty()) return fail_locked(s,"host file seal UUID/SHA/phase/inflight differs",&error);
    Roots roots; std::string cause;
    if(!roots_read(roots,cause)||roots.map!=s.roots.map||roots.shard!=s.roots.shard||roots.array!=s.roots.array||roots.regions!=s.roots.regions) return fail_locked(s,"host world identity changed before file seal: "+cause,&error);
    s.sha=sha; return publish(s,roots,error);
}
bool begin_client_load(const std::string& uuid,const std::string& sha,uint32_t expected,std::string& error) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    if(capture_active||client_active||!s.pending.empty()) return fail_locked(s,"identity operation or original callback still active",&error);
    if(!valid_uuid(uuid)||!valid_sha(sha)||expected>maximum_nodes) return fail_locked(s,"client load UUID/SHA/count invalid",&error);
    reset(s,uuid,sha,expected,"client"); s.state="loading";
    if(!main_gate(s,false,error,true,true)) return false;
    client_active=true; error.clear(); return true;
}
void observe_client_registered(void* context,int32_t id,void* object,uint64_t caller_rva) noexcept {
    if(!client_active.load()) return;
    const auto generation=client_generation.load();
    uintptr_t actual=0; ++selector_reads;
    const auto exception=copy_native(object,&actual,sizeof(actual));
    const auto base=image.load();
    if(!exception&&actual!=base+node_vtable&&actual!=base+shard_vtable) { ++non_node_callbacks; return; }
#ifdef MT2MP_NETWORK_IDENTITY_TEST
    if(test_selector_barrier) test_selector_barrier();
#endif
    auto& s=shared(); std::lock_guard lock(s.mutex);
    try {
        if(generation!=client_generation.load()) {
            if(++s.obsolete_callbacks==1) LOG_INFO("[NetworkIdentity] Obsolete typed callback skipped: load generation changed");
            return;
        }
        if(!client_active.load()) return;
        if(exception) { fail_locked(s,"client fast vptr unreadable exception="+std::to_string(exception)); return; }
        const auto thread=GetCurrentThreadId();
        if(!s.first_observer_thread) s.first_observer_thread=thread;
        s.last_observer_thread=thread; s.observer_threads.insert(thread);
        if(thread==s.main_thread) ++s.typed_main_callbacks; else ++s.typed_worker_callbacks;
        void* mode=nullptr; void* live_context=nullptr; void* object_context=nullptr; uintptr_t mode_vtable=0;
        auto& probe=s.last_context_probe; probe={};
        probe.caller_rva=caller_rva<diagnostic_image_bytes?caller_rva:0; probe.typed_vtable_rva=diagnostic_rva(actual); probe.callback_context_present=context!=nullptr;
        const auto known_context=s.context_owners.find(context); if(known_context!=s.context_owners.end()) probe.context_ordinal=known_context->second.ordinal;
        probe.mode_slot_readable=read_at(reinterpret_cast<void*>(base),0x13df320,mode); probe.mode_present=mode!=nullptr;
        if(probe.mode_slot_readable&&probe.mode_present) {
            probe.mode_vtable_attempted=true; probe.mode_vtable_readable=read_at(mode,0,mode_vtable);
            if(probe.mode_vtable_readable) { probe.mode_vtable_rva=diagnostic_rva(mode_vtable); probe.mode_vtable_in_image=probe.mode_vtable_rva!=0; probe.mode_vtable_matches=mode_vtable==base+0x16e6730; }
            if(probe.mode_vtable_matches) { probe.mode_context_attempted=true; probe.mode_context_readable=read_at(mode,0xd8,live_context); probe.mode_context_present=live_context!=nullptr; probe.mode_context_matches=live_context==context; }
        }
        probe.object_context_readable=read_at(object,8,object_context); probe.object_context_present=object_context!=nullptr; probe.object_context_matches=object_context==context;
        if(s.typed_worker_callbacks+s.typed_main_callbacks==1) s.first_context_probe=probe;
        if(!probe.mode_slot_readable) { fail_locked(s,"typed client mode slot13df320 unreadable"); return; }
        if(!probe.mode_present) { fail_locked(s,"typed client mode slot13df320 null"); return; }
        if(!probe.mode_vtable_readable) { fail_locked(s,"typed client mode vtable unreadable"); return; }
        if(!probe.mode_vtable_matches) { fail_locked(s,"typed client mode vtable differs expected_rva=24012592 actual_image_rva="+std::to_string(probe.mode_vtable_rva)+" in_image="+std::to_string(probe.mode_vtable_in_image)); return; }
        if(!probe.mode_context_readable) { fail_locked(s,"typed client Mode+d8 context unreadable"); return; }
        if(!probe.mode_context_present) { fail_locked(s,"typed client Mode+d8 context null"); return; }
        if(!probe.mode_context_matches) { fail_locked(s,"typed client Mode+d8 callback context mismatch"); return; }
        if(s.client_load_mode&&s.client_load_mode!=mode) { fail_locked(s,"typed client load mode receiver changed"); return; }
        s.client_load_mode=mode;
        if(!probe.object_context_readable) { fail_locked(s,"typed client object context backpointer unreadable after Original"); return; }
        if(!probe.object_context_matches) { fail_locked(s,"typed client object context backpointer differs after Original"); return; }
        const auto owner=s.context_owners.find(context);
        if(owner!=s.context_owners.end()&&owner->second.thread!=thread) { fail_locked(s,"typed client context observer thread changed actual="+std::to_string(thread)+" expected="+std::to_string(owner->second.thread)+" context_ordinal="+std::to_string(owner->second.ordinal)); return; }
        if(owner==s.context_owners.end()) {
            if(s.context_owners.size()>=maximum_contexts) { fail_locked(s,"typed client context owner count exceeds64"); return; }
            s.context_owners.emplace(context,ContextOwner{thread,uint32_t(s.context_owners.size()+1)});
        }
        std::string cause;
        if(id<=0) { fail_locked(s,"client serialized id actual="+std::to_string(id)+" required>0"); return; }
        if(!add_context(s,context,cause)) { fail_locked(s,"client context invalid: "+cause); return; }
        void* registered=nullptr; const auto lookup_exception=context_native(context,id,registered);
        if(lookup_exception||registered!=object) { fail_locked(s,"client original AddObject lookup differs exception="+std::to_string(lookup_exception)); return; }
        if(actual==base+shard_vtable) {
            if(s.shards.size()>=maximum_regions) { fail_locked(s,"client serialized shard count exceeds16384"); return; }
            if(!s.shards.emplace(std::make_pair(context,id),object).second) { ++s.duplicate_callbacks; fail_locked(s,"client serialized shard duplicate id="+std::to_string(id)+" context_ordinal="+std::to_string(s.context_owners.at(context).ordinal)); }
            return;
        }
        Binding binding; binding.id=id; binding.node=object; binding.context=context;
        if(!read_at(object,0x18,binding.uid)||!binding.uid) { fail_locked(s,"client typed node UID invalid"); return; }
        if(s.bindings.size()>=maximum_nodes) { fail_locked(s,"client serialized node count exceeds65536"); return; }
        const auto prior=s.bindings.find(id);
        if(prior!=s.bindings.end()) { ++s.duplicate_callbacks; fail_locked(s,"client serialized node duplicate id="+std::to_string(id)+" context_ordinal="+std::to_string(s.context_owners.at(context).ordinal)+" same_object="+std::to_string(prior->second.node==object)+" same_context="+std::to_string(prior->second.context==context)); return; }
        s.bindings.emplace(id,binding);
    } catch(const std::exception& error) { fail_locked(s,std::string("client registration exception: ")+error.what()); }
}
bool finalize_client_load(std::string& error) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    if(s.state=="failed") { error=s.error; return false; }
    if(!main_gate(s,true,error)) return false;
    if(!client_active||s.role!="client") return fail_locked(s,"client finalize outside pending normal load",&error);
    const auto observed=normal_load_identity::status();
    if(observed.snapshot_sha256!=s.sha||observed.snapshot_uuid!=s.uuid) return fail_locked(s,"normal load UUID/SHA differs from staged node baseline",&error);
    Roots roots; std::string cause;
    if(!roots_read(roots,cause)) return fail_locked(s,cause,&error);
    return publish(s,roots,error);
}
void cancel_pending(const std::string& reason) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    if(capture_active||client_active||!s.pending.empty()) fail_locked(s,"pending identity cancelled: "+reason);
    else LOG_INFO("[NetworkIdentity] Cancellation skipped: no pending identity operation");
}
void invalidate_world(const std::string& reason) {
    auto& s=shared(); std::lock_guard lock(s.mutex);
    const auto previous=s.state; const auto bindings=s.bindings.size();
    capture_active=false; client_active=false; ++client_generation;
    s.context_owners.clear(); s.client_load_mode=nullptr;
    s.state="invalidated"; s.invalidation_reason=reason.empty()?"world invalidation requested without cause":reason;
    s.keys_digest.clear(); s.bindings.clear(); s.shards.clear(); s.contexts.clear(); s.roots=Roots{};
    for(auto& [ticket,pending]:s.pending) {
        pending.nodes.clear(); pending.file_binding=WriterBinding{};
        pending.property=nullptr; pending.manager=nullptr; pending.writer=nullptr; pending.context=nullptr;
    }
    s.pending_nodes=0;
    LOG_INFO("[NetworkIdentity] World invalidated role={} uuid={} previous={} cleared_nodes={} remaining_originals={} reason={}",s.role,s.uuid,previous,bindings,s.pending.size(),s.invalidation_reason);
}
nlohmann::json status_json(uint32_t limit) {
    if(limit>maximum_sample_limit) throw std::invalid_argument("network identity sample limit actual="+std::to_string(limit)+" required<=256");
    auto& s=shared(); std::lock_guard lock(s.mutex); nlohmann::json nodes=nlohmann::json::array();
    const auto returned=std::min(s.bindings.size(),size_t(limit)); size_t index=0;
    for(const auto& [id,binding]:s.bindings) {
        if(index++>=returned) break;
        nodes.push_back({{"node_link_id",id},{"shard_link_id",binding.shard_id},{"region_id",binding.region_id},{"membership_kind",binding.region&&binding.shard_id>0?"root":"unresolved"}});
    }
    const auto probe_json=[](const ContextProbe& p) -> nlohmann::json { return {{"caller_rva",p.caller_rva},{"caller_known",p.caller_rva!=0},{"typed_vtable_rva",p.typed_vtable_rva},{"context_ordinal",p.context_ordinal},{"callback_context_present",p.callback_context_present},
        {"mode_slot_readable",p.mode_slot_readable},{"mode_present",p.mode_present},{"mode_vtable_attempted",p.mode_vtable_attempted},{"mode_vtable_readable",p.mode_vtable_readable},{"mode_vtable_rva",p.mode_vtable_rva},{"mode_vtable_in_image",p.mode_vtable_in_image},{"mode_vtable_matches",p.mode_vtable_matches},
        {"mode_context_attempted",p.mode_context_attempted},{"mode_context_readable",p.mode_context_readable},{"mode_context_present",p.mode_context_present},{"mode_context_matches",p.mode_context_matches},
        {"object_context_readable",p.object_context_readable},{"object_context_present",p.object_context_present},{"object_context_matches",p.object_context_matches}}; };
    return {{"schema_version",1},{"state",s.state},{"role",s.role},{"uuid",s.uuid},{"snapshot_sha256",s.sha},{"expected_nodes",s.expected},
        {"staged_nodes",s.bindings.size()},{"ready",s.state=="host_ready"||s.state=="client_ready"},{"keys_sha256",s.keys_digest},
        {"pending_arrays",s.pending.size()},{"pending_nodes",s.pending_nodes},{"completed_arrays",s.completed_arrays},{"contexts",s.contexts.size()},{"staged_shards",s.shards.size()},
        {"first_context_probe",probe_json(s.first_context_probe)},{"last_context_probe",probe_json(s.last_context_probe)},{"bound_main_thread",s.main_thread},{"first_observer_thread",s.first_observer_thread},{"last_observer_thread",s.last_observer_thread},{"observer_threads",s.observer_threads},{"typed_worker_callbacks",s.typed_worker_callbacks},{"typed_main_callbacks",s.typed_main_callbacks},{"obsolete_callbacks",s.obsolete_callbacks},{"duplicate_callbacks",s.duplicate_callbacks},{"client_context_owners",s.context_owners.size()},{"selector_reads",selector_reads.load()},{"non_node_callbacks",non_node_callbacks.load()},{"first_failure",s.error},{"invalidation_reason",s.invalidation_reason},{"discarded_after_failure",s.discarded},
        {"foreign_writers_skipped",s.foreign_writers},{"last_foreign_writer_reason",s.foreign_reason},{"capture_file_bound",s.role=="host"&&s.completed_arrays>0},
        {"root_only",true},{"node_limit",limit},{"node_returned_count",returned},{"nodes_truncated",returned<s.bindings.size()},{"nodes",nodes}};
}
#ifdef MT2MP_NETWORK_IDENTITY_TEST
namespace testing {
void backend(uintptr_t base,Contains contains,Lookup lookup,Owns owns,ContextLookup context_lookup) {
    test_base=base; test_contains=contains; test_lookup=lookup; test_owns=owns; test_context_lookup=context_lookup;
}
uint64_t read_queries() { return test_read_queries.load(); }
void selector_barrier(SelectorBarrier callback) { test_selector_barrier=callback; }
}
#endif
}
