#include "live_host_state/memory_query_timing.h"
#include "read.h"
#include "initial_read.h"
#include "mt2mp/net/snapshot_sync.h"
#include "native_steps_draft.h"
#include <algorithm>
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
bool read_process_loaded(const std::string& snapshot,const action_session::Binding& binding,Capture& out,std::string& e){
    if(!net::ordered_snapshot_selected())return read_process(out,e);
    live_host_state::apply_timing::PageReadScope pages;
    net::OrderedSnapshotIdentity before,after;MT2FW_MT2_StepReceipt receipt{},again{};
    if(!net::read_ordered_initial_bootstrap(before,receipt,e))return false;
    if(before.snapshot_uuid!=snapshot||before.native_generation!=binding.native_generation||before.world_epoch!=binding.native_epoch){
        e="Initial Developer census snapshot/Binding differs from agreed native bootstrap";return false;
    }
    ProcessPort port;Capture current;if(!initial_detail::read(port,current,e))return false;
    if(current.thread!=receipt.thread||current.pin.origin.uuid!=snapshot||current.pin.origin.sha256!=before.sha256||
       current.pin.origin.bytes!=before.bytes||current.pin.origin.generation!=before.native_generation||current.pin.origin.world_epoch!=before.world_epoch){
        e="Initial Developer census native Origin differs from agreed bootstrap";return false;
    }
    if(!net::read_ordered_initial_bootstrap(after,again,e))return false;
    if(after.session!=before.session||after.local_peer!=before.local_peer||after.members!=before.members||after.peer_mask!=before.peer_mask||
       after.snapshot_uuid!=before.snapshot_uuid||after.sha256!=before.sha256||after.bytes!=before.bytes||after.seed!=before.seed||
       after.native_generation!=before.native_generation||after.world_epoch!=before.world_epoch||again.lease!=receipt.lease||again.thread!=receipt.thread||
       !std::equal(std::begin(receipt.initial_witness_sha256),std::end(receipt.initial_witness_sha256),std::begin(again.initial_witness_sha256))){
        e="Initial Developer bootstrap owner changed during native census";return false;
    }
    out=std::move(current);e.clear();return true;
}
bool read_process(Capture& out,std::string& e){live_host_state::apply_timing::PageReadScope pages;ProcessPort port;return read(port,out,e);}
}
