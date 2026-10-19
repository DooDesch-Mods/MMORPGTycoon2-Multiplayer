#include "live_host_state/memory_query_timing.h"
#include "native_port.h"
#include "scalar_store.h"
#include "native_open_runtime.h"
#include "typed_action/sidecar_adapter.h"
#include "session_provider.h"
#include "time_integration.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/game/native_engine.h"
#include "placement_ui_probe/ui_probe.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cmath>
namespace mt2mp::game::live_host_state {
namespace {
placement_ui_probe::ProcessMemory memory;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(uintptr_t p,size_t o,T& v,std::string& e){if(!p||p>UINTPTR_MAX-o)return fail(e,"live native address range invalid");return memory.copy(p+o,&v,sizeof v,e);}
bool ap(uintptr_t p,uintptr_t image,uintptr_t addresspoint,std::string& e){uintptr_t actual=0;return at(p,0,actual,e)&&(actual==image+addresspoint||fail(e,"live native class addresspoint differs"));}
bool current_scope(const Context& expected,const Binding& binding,Pin& p,native::WorldStatus& status,std::string& e){p={};uintptr_t image=0;uint32_t thread=0;std::string sha;if(!native::read_resolver_receipt(image,thread,sha,e))return false;
    if(thread!=GetCurrentThreadId()||image!=uintptr_t(GetModuleHandleW(nullptr))||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"live native module/Mainthread receipt differs");
    auto& session=action_session::selected_session();Context current;if(!session.current(current,e)||current!=expected)return fail(e,e.empty()?"live actual session differs from Controller bound context":e);
    building_mixed_identity::opened_file::CurrentWorld world;if(!native_open_runtime::read_current_world(world,e))return false;if(!world.world||!world.state||world.world_epoch!=binding.native_epoch||world.generation!=binding.native_generation)return fail(e,"live actual World/epoch/generation differs");
    if(!native::read_world_status(status,e))return false;
    p.context=current;p.binding=binding;p.common_epoch=net::current_world_epoch();if(!p.common_epoch)return fail(e,"live actual common Sidecar epoch absent");p.image=image;p.thread=thread;p.world=uintptr_t(world.world);
    return true;
}
bool once(const Context& expected,const Binding& binding,Pin& p,float& pause,std::string& e,int32_t expected_edits=0){apply_timing::PageReadScope pages;native::WorldStatus status;
    if(!current_scope(expected,binding,p,status,e)||!native_activity(status.saves_in_progress,status.edit_operations,expected_edits,e))return false;
    const auto image=p.image;
    if(!at(image,0x13df9a0,p.clock,e)||!ap(p.clock,image,0x17499d0,e)||!at(image,0x13df220,p.business,e)||!ap(p.business,image,0x16d97e0,e)||!at(p.business,0x100,p.record,e)||!ap(p.record,image,0x1712c00,e))return false;
    if(p.clock>UINTPTR_MAX-0x60||p.business>UINTPTR_MAX-0x110||p.record>UINTPTR_MAX-0x5d0)return fail(e,"live native fixed owner extent overflows");
    int32_t version=0;if(!at(p.record,0x18,version,e)||version!=2)return fail(e,e.empty()?"live native Record dv requires2":e);
    if(!ap(p.business+0x60,image,0x16ec280,e)||!at(p.business,0x68,p.history,e)||!at(p.business,0x70,p.history_count,e)||!at(p.business,0x74,p.history_capacity,e)||p.history_count<=0||p.history_capacity<p.history_count||!p.history)return fail(e,e.empty()?"live native History shape differs":e);
    size_t memberships=0;for(int32_t i=0;i<p.history_count;++i){uintptr_t member=0;if(!at(p.history,size_t(i)*sizeof(uintptr_t),member,e))return false;if(member==p.record)++memberships;}if(memberships!=1)return fail(e,"live native CurrentRecord is not a unique owned History member measured="+std::to_string(memberships)+" required=1");
    for(size_t i=0;i<6;++i)if(!at(p.clock,0x38+i*4,p.values.calendar[i],e))return false;
    if(!at(p.clock,0x58,p.selected,e)||!at(p.clock,0x5c,pause,e)||!at(p.business,0x38,p.values.cash,e))return false;p.pause=pause;for(size_t i=0;i<5;++i)if(!at(p.record,0x1c+i*4,p.values.record[i],e))return false;
    for(size_t i=0;i<20;++i){const auto line=p.record+0x30+i*0x48;int32_t category=-1;if(!ap(line,image,0x171d0d0,e)||!at(line,0x20,category,e)||category!=int32_t(i)||!at(line,0x18,p.values.lines[i][0],e)||!at(line,0x1c,p.values.lines[i][1],e))return fail(e,e.empty()?"live native category class/index differs category="+std::to_string(i):e);}
    native::SimulationState native_state;if(!native::read_simulation_state(native_state,e))return false;if(ledger(p.values)!=action_session::Ledger{native_state.cash,native_state.clock.flat_day,int32_t(native_state.record_present),native_state.record_day,native_state.record_cash,native_state.record_income,native_state.record_expense,native_state.line_counts,native_state.line_income})return fail(e,"live typed52-slot values differ from existing actual NativeReader");
    return true;
}
uintptr_t address(const Pin& p,size_t slot){if(slot<6)return p.clock+0x38+slot*4;if(slot==6)return p.business+0x38;if(slot<12)return p.record+0x1c+(slot-7)*4;slot-=12;return p.record+0x30+(slot/2)*0x48+0x18+(slot%2)*4;}

}
bool NativePort::publication_ready(uint64_t expected_epoch,bool& waiting,std::string& e){
    waiting=false;
    if(context_.role!=action_session::Role::Host||context_.local_peer!=1)return fail(e,"live publication readiness requires bound Host");
    Pin current;native::WorldStatus status;
    if(!current_scope(context_,binding_,current,status,e))return false;
    if(current.common_epoch!=expected_epoch)return fail(e,"live publication actual common epoch differs from Bridge");
    return publication_activity(status.saves_in_progress,status.edit_operations,waiting,e);
}
bool NativePort::read(Pin& out,std::string& e){out={};Pin first,second;float pause1=0,pause2=0;if(!once(context_,binding_,first,pause1,e)||!once(context_,binding_,second,pause2,e))return false;if(first!=second||pause1!=pause2)return fail(e,"live native roots/fields/session changed during double read");Frame shape;shape.session=context_.session;shape.snapshot=context_.snapshot_uuid;shape.epoch=binding_.native_epoch;shape.source_generation=binding_.native_generation;shape.revision=shape.time_revision=1;shape.values=first.values;shape.selected=first.selected;shape.pause=pause1;if(!valid(shape,e))return false;out=first;e.clear();return true;}
bool NativePort::current_owners(const Pin& expected,std::string& e,int32_t expected_edits){Pin current;float pause=0;if(!once(context_,binding_,current,pause,e,expected_edits))return false;current.values={};auto identity=expected;identity.values={};if(current!=identity||(context_.role==action_session::Role::Client&&current.selected!=0))return fail(e,"live scalar current owner identities/History membership/Clock0 differ");e.clear();return true;}
bool NativePort::store(const Pin& pin,size_t slot,int32_t before,int32_t target,std::string& e){if(slot>=scalar_count||GetCurrentThreadId()!=pin.thread||pin.context!=context_||pin.binding!=binding_||pin.context.role!=action_session::Role::Client||pin.selected!=0)return fail(e,"live fixed scalar store slot/actual Mainthread/Client binding invalid");if(!boundary_)return fail(e,"live actual Client economic dispatch fence provider missing; no store");if(!boundary_->read_current_fence(pin,e))return false;
    if(!current_owners(pin,e))return false;
    return compare_fixed_i32(address(pin,slot),before,target,e);}

bool read_time_authority(const Context& c,uint64_t& revision,std::string& e){revision=0;time_integration::Status s;if(!time_integration::instance().status(s,e))return false;if(!s.initialized||s.runtime.phase!=time_runtime::Phase::Active||!s.runtime.controller.started||s.runtime.controller.write_locked||!s.runtime.epoch)return fail(e,"live Time authority not initialized/Active/started or locked");const auto& state=c.role==action_session::Role::Host?s.runtime.controller.observed:s.runtime.controller.applied;Context actual;if(!action_session::selected_session().current(actual,e)||actual!=c)return fail(e,e.empty()?"live Time Session differs from bound context":e);if(s.runtime.epoch!=net::current_world_epoch()||s.runtime.hook_mask!=3||!state.present||state.uncertain||!state.revision)return fail(e,"live Time authority requires actual current epoch/hooks/state/revision");revision=state.present?state.revision:0;e.clear();return true;}
bool capture_host(const Context& c,const Binding& b,uint64_t revision,uint64_t action_revision,uint64_t time_revision,Frame& out,std::string& e){out={};if(c.role!=action_session::Role::Host||c.local_peer!=1)return fail(e,"live capture requires actual bound Host context");uint64_t actual_time=0;if(!read_time_authority(c,actual_time,e)||actual_time!=time_revision)return fail(e,e.empty()?"live capture actual Time revision differs":e);NativePort reader(c,b,nullptr);Pin pin;if(!reader.read(pin,e))return false;out.session=c.session;out.snapshot=c.snapshot_uuid;out.epoch=pin.common_epoch;out.source_generation=b.native_generation;out.revision=revision;out.base_action_revision=action_revision;out.time_revision=time_revision;out.values=pin.values;out.selected=pin.selected;out.pause=pin.pause;return valid(out,e);}
}

namespace mt2mp::game::live_host_state {
bool NativePort::rebind_snapshot(const Context& c,const Binding& b,std::string& e){NativePort fresh(c,b,boundary_);Pin actual;if(!fresh.read(actual,e))return false;context_=c;binding_=b;e.clear();return true;}
}
