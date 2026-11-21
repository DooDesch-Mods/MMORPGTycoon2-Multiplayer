#include "startup_calendar_normalization.h"
#include "native_open_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/core/hash.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vector>

namespace mt2mp::game::startup_calendar {
namespace {
bool rejected(std::string& e,const std::string& cause){e=cause;return false;}
uint32_t original_call(uintptr_t entry,uintptr_t receiver) noexcept {
    __try {reinterpret_cast<void(*)(void*,void*)>(entry)(reinterpret_cast<void*>(receiver),nullptr);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct NativePort {
    placement_ui_probe::ProcessMemory memory;
    uintptr_t image=0;
    template<class T> bool at(uintptr_t base,size_t offset,T& out,std::string& e) {
        if(!base||base>UINTPTR_MAX-offset)return rejected(e,"startup calendar native address overflows");
        return memory.copy(base+offset,&out,sizeof out,e);
    }
    bool ap(uintptr_t object,uintptr_t rva,std::string& e) {
        uintptr_t actual=0;if(!at(object,0,actual,e))return false;
        return actual==image+rva||rejected(e,"startup calendar native class differs RVA="+std::to_string(rva));
    }
    bool read(State& s,std::string& e) {
        s={};std::string executable;
        if(!native::read_resolver_receipt(s.image,s.thread,executable,e))return false;
        if(!Config::instance().is_host||!net::session_wire::connected()||s.thread!=GetCurrentThreadId()||
           s.image!=uintptr_t(GetModuleHandleW(nullptr))||
           executable!="7efd8ee7b97f78362e7a2f8116b327fa6b756bdce23114b68143c4c9d2dd50eb")
            return rejected(e,"startup calendar requires exact connected Host/Mainthread/executable");
        image=s.image;
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,e))return false;
        s.world={uintptr_t(world.game),uintptr_t(world.mode),uintptr_t(world.state),uintptr_t(world.world),uintptr_t(world.map)};
        s.generation=world.generation;s.epoch=world.world_epoch;s.uuid=world.uuid;s.sha=world.sha256;
        native::WorldStatus status;if(!native::read_world_status(status,e))return false;
        if(!status.world_present||status.active_mode_index!=3||status.title_transition_pending||status.quitting||
           status.saves_in_progress||status.edit_operations||status.map_task_pending)
            return rejected(e,"startup calendar requires idle loaded World with no Save/Edit/Map task");
        if(!at(image,0x13eb9a0,s.clock,e)||!ap(s.clock,0x174fd70,e)||
           !at(image,0x13eb220,s.business,e)||!ap(s.business,0x16dfb80,e)||
           !at(s.business,0x68,s.history,e)||!at(s.business,0x70,s.history_count,e)||
           !at(s.business,0x74,s.history_capacity,e)||!at(s.business,0x100,s.record,e))return false;
        if(s.business>UINTPTR_MAX-0x60)return rejected(e,"startup calendar History address overflows");
        if(!ap(s.business+0x60,0x16f2620,e))return false;
        if(s.history_count>0&&!at(s.history,0,s.member,e))return false;
        if(!ap(s.record,0x1718fa0,e)||!at(image,0x13eb500,s.software,e))return false;
        if(s.software&&(!ap(s.software,0x171b7f0,e)||!at(s.software,0x88,s.software_state,e)))return false;
        if(!memory.copy(s.clock,s.clock_bytes.data(),s.clock_bytes.size(),e)||
           !memory.copy(s.business,s.business_bytes.data(),s.business_bytes.size(),e)||
           !memory.copy(s.record,s.record_bytes.data(),s.record_bytes.size(),e))return false;
        native::DayTickState calls;native::read_day_tick_state(calls);
        for(size_t i=0;i<calls.ticks.size();++i){s.day_calls[i*3]=calls.ticks[i].entered;
            s.day_calls[i*3+1]=calls.ticks[i].forwarded;s.day_calls[i*3+2]=calls.ticks[i].accepted;}
        return true;
    }
    bool verify_code(std::string& e) {
        struct Code {uintptr_t rva;size_t size;const char* hash;};
        // Both functions ignore their vsObjectContext argument. The guarded shape
        // bypasses Clock recovery and Business record creation/initial-loan branches.
        constexpr Code pins[]={{0x25cce0,336,"5f1a6e9aeb5fef50c30bfb448026b96ebc8646e376a9e9b3ffc2c9f64875dc95"},
            {0x56b80,400,"da23295c644adf543c881fa9541bd11c42484111628baf5143204b9012c81f67"},
            {0x55c90,80,"83bf47592da33b37d6398bf9efc2dc5d6fd9058eda423ed7ebd539158ad5e8cd"},
            {0xb56c70,251,"c85255076535d0971ff3fd53c9372455c068054477aa8d0314919b24ce45268c"}};
        for(const auto& pin:pins){
            std::vector<unsigned char> bytes(pin.size);
            if(!memory.copy(image+pin.rva,bytes.data(),bytes.size(),e))return false;
            if(mt2mp::sha256(bytes)!=pin.hash)return rejected(e,"startup calendar original body differs RVA="+std::to_string(pin.rva));
        }
        return true;
    }
    bool invoke(uintptr_t rva,uintptr_t receiver,std::string& e) {
        const auto code=original_call(image+rva,receiver);
        return !code||rejected(e,"startup calendar original failed RVA="+std::to_string(rva)+" SEH="+std::to_string(code)+"; mutation may have started, no retry");
    }
    bool resolve_clock(const State& s,std::string& e){return invoke(0x25cce0,s.clock,e);}
    bool resolve_business(const State& s,std::string& e){return invoke(0x56b80,s.business,e);}
};
Result normalize_loaded(std::string& error){NativePort port;return normalize(port,error);}
}
}
