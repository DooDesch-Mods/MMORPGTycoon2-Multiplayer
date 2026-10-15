#include "read.h"
#include "native_open_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include <cmath>
#include <windows.h>
namespace mt2mp::game::developer_staff {
namespace {
struct ProcessPort final:Port {
    placement_ui_probe::ProcessMemory memory;
    bool resolver(uintptr_t& image,uint32_t& thread,std::string& e)override{
        std::string sha;if(!native::read_resolver_receipt(image,thread,sha,e))return false;
        if(thread!=GetCurrentThreadId()||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){e="Developer native read requires exact EXE and actual bound Mainthread";return false;}return true;
    }
    bool pin(Pin& p,std::string& e)override{
        p={};if(!native_open_runtime::read_current_world(p.origin,e))return false;
        native::WorldStatus world;if(!native::read_world_status(world,e))return false;
        if(world.active_mode_index!=3||world.title_transition_pending||world.saves_in_progress>0||world.edit_operations){e="Developer read requires actual idle InGame World";return false;}
        native::SimulationState s;if(!native::read_simulation_state(s,e))return false;
        p.ledger={s.cash,s.clock.flat_day,s.record_present?1:0,s.record_day,s.record_cash,s.record_income,s.record_expense,s.line_counts,s.line_income};
        p.time_scale=s.clock.time_scale;p.pause_scale=s.clock.pause_scale;
        if(!std::isfinite(p.time_scale)||!std::isfinite(p.pause_scale)){e="Developer actual Clock is not finite";return false;}return true;
    }
    bool copy(uintptr_t address,void* out,size_t count,std::string& e)override{return memory.copy(address,out,count,e);}
};
}
bool read_process(Capture& out,std::string& e){ProcessPort port;return read(port,out,e);}
}
