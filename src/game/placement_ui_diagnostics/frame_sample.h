#pragma once
#include "placement_ui_probe/ui_probe.h"
#include "vendor/nlohmann/json.hpp"
#include <cmath>
#include <optional>
namespace mt2mp::game::placement_ui_diagnostics::frame_readback {
inline nlohmann::json sample(placement_ui_probe::Memory& memory,const placement_ui_probe::Binding& binding,
                            uintptr_t receiver,std::optional<float> dt,uint64_t completed,const char* boundary,std::string& error) {
    using json=nlohmann::json;
    json out={{"boundary",boundary},{"completed_frame",completed},{"dt",dt&&std::isfinite(*dt)?json(*dt):json(nullptr)},{"dt_measured",dt.has_value()},
        {"expected_receiver",binding.behaviour},{"actual_receiver",receiver},{"receiver_matches",receiver==binding.behaviour},
        {"read_confirmed",false},{"cause",""}};
    auto finish=[&](const std::string& cause){error=cause;out["cause"]=cause;return out;};
    auto copy=[&](uintptr_t address,void* to,size_t bytes)->bool{
        if(!address||address>UINTPTR_MAX-(bytes-1)){error="build_ui_frame_read_address_overflow";return false;}
        return memory.copy(address,to,bytes,error);
    };
    auto field=[&]<class T>(uintptr_t object,size_t offset,T& value)->bool{
        if(!object||object>UINTPTR_MAX-offset){error="build_ui_frame_field_address_overflow";return false;}
        return copy(object+offset,&value,sizeof value);
    };
    if(dt&&!std::isfinite(*dt))return finish("build_ui_frame_dt_not_finite");
    uintptr_t cursor=0,ap=0,active=0,capture=0,trap=0;
    if(!field(binding.context.image,0x13df9d0,cursor)||!field(cursor,0,ap))return finish(error);
    out["current_cursor"]=cursor;out["cursor_class_addresspoint"]=ap;out["cursor_matches_binding"]=cursor==binding.cursor;
    if(ap!=binding.context.image+0x1786dd0)return finish("build_ui_frame_current_cursor_class_differs");
    std::array<float,3> world_pick{};std::array<float,2> screen{};std::array<uint8_t,3> ui_flags{};std::array<uint8_t,2> input{};
    uint8_t enabled=0,input_gate=0,trap_gate=0;
    if(!field(cursor,0x190,active)||!field(cursor,0x180,capture)||!field(cursor,0x1d8,trap)||
       !field(cursor,0x1e4,world_pick)||!field(cursor,0x1f0,screen)||!field(cursor,0x174,ui_flags)||
       !field(cursor,0x214,input)||!field(cursor,0x257,enabled)||!field(cursor,0x256,input_gate)||!field(cursor,0x1e0,trap_gate))return finish(error);
    out["active_behaviour"]=active;out["active_matches_expected_receiver"]=active==binding.behaviour;
    out["cursor_world_pick"]=world_pick;out["cursor_screen_position"]=screen;out["cursor_ui_restrictions"]=ui_flags;
    out["cursor_input_down_drag"]=input;out["cursor_enabled_257"]=enabled;out["cursor_input_gate_256"]=input_gate;
    out["cursor_window_capture_180"]=capture;out["cursor_trap_1d8"]=trap;out["cursor_trap_gate_1e0"]=trap_gate;
    if(active){uintptr_t active_ap=0;if(!field(active,0,active_ap))return finish(error);out["active_class_addresspoint"]=active_ap;}
    if(trap){uintptr_t trap_ap=0;if(!field(trap,0,trap_ap))return finish(error);out["cursor_trap_class_addresspoint"]=trap_ap;}
    if(capture){uintptr_t capture_ap=0;if(!field(capture,0,capture_ap))return finish(error);out["cursor_window_capture_class_addresspoint"]=capture_ap;}
    if(receiver){uintptr_t receiver_ap=0;if(!field(receiver,0,receiver_ap))return finish(error);out["actual_receiver_class_addresspoint"]=receiver_ap;}
    for(float value:world_pick)if(!std::isfinite(value))return finish("build_ui_frame_cursor_world_pick_not_finite");
    if(!field(binding.behaviour,0,ap))return finish(error);
    out["expected_build_class_addresspoint"]=ap;
    if(ap!=binding.context.image+(binding.family==placement_ui_probe::Family::Scenery?0x17396b0:0x17327d0))return finish("build_ui_frame_expected_build_class_differs");
    std::array<uint8_t,3> flags{};uint8_t obstruction=0;uintptr_t ghost=0,blueprint=0;
    if(!field(binding.behaviour,binding.family==placement_ui_probe::Family::Scenery?0x6c0:0x660,flags)||!field(binding.behaviour,binding.family==placement_ui_probe::Family::Scenery?0x6b9:0x659,obstruction)||
       !field(binding.behaviour,binding.family==placement_ui_probe::Family::Scenery?0xb58:0x6b8,ghost)||!field(binding.behaviour,binding.family==placement_ui_probe::Family::Scenery?0xb50:0x6c0,blueprint))return finish(error);
    out["build_active_pending_abort"]=flags;out["placement_obstruction_291"]=obstruction;out["ghost"]=ghost;out["blueprint"]=blueprint;
    if(!ghost)return finish("build_ui_frame_native_ghost_absent");
    if(!field(ghost,0,ap))return finish(error);out["ghost_class_addresspoint"]=ap;
    if(ap!=binding.context.image+0x1705230)return finish("build_ui_frame_native_ghost_class_differs");
    std::array<float,3> position{},scale{};std::array<float,4> rotation{};
    if(!field(ghost,0x110,position)||!field(ghost,0x100,rotation)||!field(ghost,0x11c,scale))return finish(error);
    out["ghost_pose"]={{"position",position},{"rotation",rotation},{"scale",scale}};
    for(float value:position)if(!std::isfinite(value))return finish("build_ui_frame_ghost_position_not_finite");
    for(float value:rotation)if(!std::isfinite(value))return finish("build_ui_frame_ghost_rotation_not_finite");
    for(float value:scale)if(!std::isfinite(value))return finish("build_ui_frame_ghost_scale_not_finite");
    out["read_confirmed"]=true;error.clear();return out;
}
}
