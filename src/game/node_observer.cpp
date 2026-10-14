#include "node_observer.h"
#include "ui_network_runtime.h"
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
#include <cstring>
#include <mutex>
#include <exception>
namespace mt2mp::game::node_observer {
namespace {
constexpr char sha[]="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct Pending {Ticket ticket=0;Leaf leaf{};Observation sample;uintptr_t object=0;uint64_t uid=0;};
struct Owner {
    std::mutex mutex;Status status;uintptr_t image=0;uint64_t sequence=0;
    ui_network_runtime::Admission admission;
    std::array<Pending,32> pending{};
};
Owner& state(){static Owner value;return value;}
bool fail(Owner& s,std::string& e,const std::string& c){s.status.cause=c;++s.status.rejected;e=c;return false;}
bool same(const ui_network_runtime::Admission& a,const ui_network_runtime::Admission& b){
    return a.binding==b.binding&&a.snapshot==b.snapshot&&a.action_key==b.action_key&&a.type==b.type&&a.key==b.key&&
        a.region_id==b.region_id&&a.local_uid==b.local_uid&&a.object==b.object&&a.manager==b.manager&&a.region==b.region&&a.definition==b.definition&&a.topology_sha256==b.topology_sha256;
}
DWORD copy(const void* from,void* to,size_t bytes)noexcept{
    __try{std::memcpy(to,from,bytes);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD scalars(uintptr_t image,uintptr_t object,uint64_t uid,Scalars* out)noexcept{
    __try{
        const auto p=reinterpret_cast<const unsigned char*>(object);
        if(*reinterpret_cast<const uintptr_t*>(p)!=image+0x16fc700||*reinterpret_cast<const uint64_t*>(p+0x18)!=uid)return ERROR_INVALID_DATA;
        std::memcpy(&out->values[0],p+0x250,4);std::memcpy(&out->values[1],p+0x2a0,4);
        std::memcpy(&out->values[2],p+0x2a4,4);std::memcpy(&out->values[3],p+0x2e4,4);
        out->flags={p[0x2e8],p[0x2ea],p[0x2ec]};return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD sample(uintptr_t image,uintptr_t object,uint64_t uid,Scalars& out)noexcept{
    const auto fault=scalars(image,object,uid,&out);if(fault)return fault;
    for(float v:out.values)if(!std::isfinite(v))return ERROR_INVALID_DATA;
    for(uint8_t v:out.flags)if(v>1)return ERROR_INVALID_DATA;
    return 0;
}
bool main(Owner& s,std::string& e){return (s.status.prepared&&GetCurrentThreadId()==s.status.main_thread)||fail(s,e,"node observer bound main thread differs/unprepared");}
bool fresh(Owner& s,std::string& e){
    s.status.fresh_owner_confirmed=false;ui_network_runtime::Admission current;
    const bool okay=s.status.snapshot_binding?ui_network_runtime::refresh_snapshot_node(s.admission,current,e):ui_network_runtime::admitted_node(s.status.action_uuid,current,e);
    if(!okay)return fail(s,e,"node observer fresh owner rejected: "+e);
    if(!same(current,s.admission))return fail(s,e,"node observer action owner/epoch/generation replaced");
    s.status.fresh_owner_confirmed=true;return true;
}
}
bool prepare(uintptr_t image,const std::string& exe_sha,uint32_t thread,std::string& e){
    auto& s=state();std::lock_guard lock(s.mutex);e.clear();
    if(s.status.prepared||!image||exe_sha!=sha||!thread||GetCurrentThreadId()!=thread||!native::ready()||
       uintptr_t(native::entrypoint(native::EntryPoint::GameUpdate))!=image+0x463ce0)return fail(s,e,"node observer exact build/mainthread preparation rejected");
    constexpr unsigned char guards[5][16]={
        {0x57,0x56,0x53,0x48,0x81,0xec,0xb0,0,0,0,0x0f,0x11,0xb4,0x24,0x80,0},
        {0x56,0x53,0x48,0x81,0xec,0x48,1,0,0,0x0f,0x11,0xb4,0x24,0x20,1,0},
        {0x53,0x48,0x83,0xec,0x20,0x66,0x0f,0xef,0xd2,0x66,0x0f,0xef,0xc9,0x48,0x8b,0x81},
        {0xf3,0x0f,0x5d,0x89,0x50,2,0,0,0xf3,0x0f,0x10,0x81,0xa4,2,0,0},
        {0xf3,0x0f,0x10,0x81,0xa4,2,0,0,0xf3,0x0f,0x58,0xc1,0xf3,0x0f,0x11,0x81}};
    for(size_t i=0;i<rvas.size();++i){unsigned char actual[16]{};const auto fault=copy(reinterpret_cast<void*>(image+rvas[i]),actual,16);
        if(fault||std::memcmp(actual,guards[i],16))return fail(s,e,"node observer entry bytes differ index="+std::to_string(i)+" read_fault="+std::to_string(fault));}
    s.image=image;s.status.prepared=true;s.status.main_thread=thread;return true;
}
bool set_hook_mask(uint32_t mask,std::string& e){auto& s=state();std::lock_guard lock(s.mutex);if(!main(s,e))return false;
    if(mask!=0&&mask!=31)return fail(s,e,"node observer all five installed hook receipts required");
    s.status.hook_mask=mask;if(!mask)s.status.bound=false;e.clear();return true;}
bool bind_action(const std::string& id,std::string& e){auto& s=state();std::lock_guard lock(s.mutex);if(!main(s,e))return false;
    if(s.status.bound||s.status.inflight||s.status.hook_mask!=31||s.status.binding_revision==UINT64_MAX)return fail(s,e,"node observer bind requires idle installed hooks/nonexhausted binding revision");
    ui_network_runtime::Admission a;if(!ui_network_runtime::admitted_node(id,a,e))return fail(s,e,"node observer initial admission rejected: "+e);
    if(a.action_key.empty()||!a.object||!a.local_uid||!a.binding.native_epoch||!a.binding.native_generation||a.snapshot.empty())return fail(s,e,"node observer admission incomplete");
    Scalars initial;const auto fault=sample(s.image,a.object,a.local_uid,initial);if(fault)return fail(s,e,"node observer initial scalar read failed code="+std::to_string(fault));
    s.admission=a;++s.status.binding_revision;s.status.leaves={};s.status.action_uuid=id;s.status.snapshot=a.snapshot;s.status.snapshot_binding=false;s.status.region_id=a.region_id;s.status.node_index=-1;s.status.bound=true;s.status.fresh_owner_confirmed=true;s.status.cause.clear();e.clear();return true;
}
bool bind_snapshot(const std::string& snapshot,int32_t region,int32_t index,const std::string& digest,std::string& e){
    auto& s=state();std::lock_guard lock(s.mutex);if(!main(s,e))return false;
    if(s.status.bound||s.status.inflight||s.status.hook_mask!=31||s.status.binding_revision==UINT64_MAX)return fail(s,e,"node observer snapshot bind requires idle installed hooks/nonexhausted binding revision");
    ui_network_runtime::Admission a;
    if(!ui_network_runtime::admitted_snapshot_node(snapshot,region,index,digest,a,e))return fail(s,e,"node observer snapshot admission rejected: "+e);
    if(!a.object||!a.local_uid||!a.binding.native_epoch||!a.binding.native_generation||a.snapshot!=snapshot||a.region_id!=region||a.topology_sha256.empty())return fail(s,e,"node observer snapshot admission incomplete");
    Scalars initial;const auto fault=sample(s.image,a.object,a.local_uid,initial);if(fault)return fail(s,e,"node observer snapshot scalar read failed code="+std::to_string(fault));
    s.admission=a;++s.status.binding_revision;s.status.leaves={};s.status.action_uuid.clear();s.status.snapshot=a.snapshot;s.status.snapshot_binding=true;s.status.region_id=region;s.status.node_index=index;s.status.bound=true;s.status.fresh_owner_confirmed=true;s.status.cause.clear();e.clear();return true;
}
Ticket enter(Leaf leaf,void* receiver,uintptr_t caller,float arg)noexcept{
    auto& s=state();std::lock_guard lock(s.mutex);
    if(!s.status.bound||s.status.hook_mask!=31)return 0;
    if(uintptr_t(receiver)!=s.admission.object){++s.status.foreign_receivers;return 0;}
    const auto index=size_t(leaf);if(index>=5){++s.status.rejected;++s.status.invalid_leaf;return 0;}
    if(s.sequence==UINT64_MAX){++s.status.rejected;++s.status.ticket_exhausted;return 0;}
    Pending* pending=nullptr;for(auto& row:s.pending)if(!row.ticket){pending=&row;break;}
    if(!pending){++s.status.rejected;++s.status.observation_capacity;return 0;}
    auto& count=s.status.leaves[index];++count.entered;
    const auto thread=GetCurrentThreadId();if(thread!=s.status.main_thread)++count.foreign_thread;
    *pending={};pending->ticket=++s.sequence;pending->leaf=leaf;pending->object=s.admission.object;pending->uid=s.admission.local_uid;
    auto& o=pending->sample;o.sequence=pending->ticket;o.thread=thread;
    if(caller>=30986240){++s.status.invalid_caller;caller=0;}o.caller_rva=caller;o.argument_present=std::isfinite(arg);o.argument=o.argument_present?arg:0;
    o.read_before_fault=sample(s.image,pending->object,pending->uid,o.before);o.before_present=!o.read_before_fault;
    ++s.status.inflight;return pending->ticket;
}
void leave(Ticket ticket,bool returned,uint32_t fault)noexcept{
    if(!ticket)return;auto& s=state();std::lock_guard lock(s.mutex);Pending* p=nullptr;
    for(auto& row:s.pending)if(row.ticket==ticket){p=&row;break;}
    if(!p){++s.status.rejected;++s.status.unknown_ticket;return;}
    auto& o=p->sample;auto& count=s.status.leaves[size_t(p->leaf)];o.original_returned=returned;o.original_fault=fault;
    if(returned&&!fault)++count.returned;else ++count.original_failed;
    o.read_after_fault=sample(s.image,p->object,p->uid,o.after);o.after_present=!o.read_after_fault;
    if(!o.before_present||!o.after_present)++count.read_failed;
    if(o.before_present&&o.after_present&&o.before!=o.after)++count.changed;
    if(!count.first.sequence)count.first=o;count.last=o;*p={};--s.status.inflight;
}
bool read_status(Status& out,std::string& e){auto& s=state();std::lock_guard lock(s.mutex);if(!main(s,e)){out=s.status;return false;}
    if(s.status.bound&&!fresh(s,e)){s.status.bound=false;out=s.status;return false;}out=s.status;e.clear();return true;}
bool suspend(const std::string& reason,std::string& e){auto& s=state();std::lock_guard lock(s.mutex);if(!main(s,e))return false;
    s.status.bound=false;s.status.fresh_owner_confirmed=false;s.status.cause=reason;
    if(s.status.inflight)return fail(s,e,"node observer suspended with inflight native calls="+std::to_string(s.status.inflight));e.clear();return true;}
void* entrypoint(Leaf leaf)noexcept{auto& s=state();std::lock_guard lock(s.mutex);const auto index=size_t(leaf);return s.status.prepared&&index<5?reinterpret_cast<void*>(s.image+rvas[index]):nullptr;}
}
