#include "game/tutorial_movement/camera_projection.h"
#include <iostream>
#include <vector>
using namespace mt2mp::game::tutorial_movement;
int checks=0;void require(bool v){++checks;if(!v)throw std::runtime_error("camera check "+std::to_string(checks));}
struct Harness{
    Snapshot actual;uint64_t now=0;bool observe_ok=true,send_ok=true,position_pending=false,position_ok=true;std::vector<CameraInput> calls;
    CameraFixture fixture;
    Harness(int index):fixture({this,[](void* p,int32_t,Snapshot& s,std::string& e){auto& h=*static_cast<Harness*>(p);s=h.actual;if(!h.observe_ok)e="lost native world";return h.observe_ok;},
        [](void* p,const Snapshot&,Snapshot& s,std::string&){s=static_cast<Harness*>(p)->actual;return true;},
        [](void* p,const Snapshot&,CameraInput kind,std::string& e){auto& h=*static_cast<Harness*>(p);h.calls.push_back(kind);if(!h.send_ok)e="original ingress failed";return h.send_ok;},
        [](void* p,const Snapshot&,bool& pending,std::string& e){auto& h=*static_cast<Harness*>(p);pending=h.position_pending;if(!h.position_ok)e="position preparation failed";return h.position_ok;},
        [](void* p,const Snapshot&,std::string& e){auto& h=*static_cast<Harness*>(p);if(!h.position_ok)e="position readback failed";return h.position_ok;},
        [](void* p){return static_cast<Harness*>(p)->now;}}){actual.origin="world";actual.epoch=1;actual.generation=2;actual.window=3;actual.hwnd=4;actual.window_id=5;actual.keyboard_focus=true;actual.input_enabled=true;actual.tutorial_index=index;}
    void release_step(){actual.tutorial_index=camera_step(actual.tutorial_index)->next;actual.threshold_reached=actual.ordered_release=true;++actual.history_count;++actual.granted;++actual.released;}
};
void run(){std::string error;
    for(int index:{8,10,12}){
        const auto* step=camera_step(index);Snapshot s;s.origin="world";s.epoch=1;s.generation=2;
        nlohmann::json release={{"asset","scenarios/initial_tutorial"},{"index",index},{"step_id",step->id},{"message_type",step->type},
            {"clear_undo",true},{"clear_attempted",true},{"clear_returned",true},{"advance_attempted",true},{"advance_returned",true},{"undo_after",0},{"redo_after",0},{"cause",""}};
        nlohmann::json row={{"scenario_id","initial_tutorial"},{"complete",false},{"sequence_complete",false},{"step_count",107},
            {"current_step_index",index},{"next_step_index",index+1},{"current_step_type","mmoStepMessage"},{"current_step_id",step->id},{"current_step_complete",false},{"history_count",0}};
        nlohmann::json p={{"schema",2},{"ordered_message",{{"fault",""},{"granted",0},{"release_attempted",0},{"last_release",release}}},
            {"tutorial_observer",{{"mainthread_confirmed",true},{"lifetime_confirmed",true},{"snapshot_uuid","world"},{"world_epoch",1},{"world_generation",2},{"initial_tutorial_complete",false},{"tutorials",nlohmann::json::array({row})}}}};
        require(camera_projection(p,index,s,error));require(!s.threshold_reached);
        auto& next=p["tutorial_observer"]["tutorials"][0];next["current_step_index"]=step->next;next["next_step_index"]=step->next+1;next["current_step_id"]=step->next_id;
        require(camera_projection(p,index,s,error));require(s.ordered_release);
        p["ordered_message"]["last_release"]["message_type"]=19;require(!camera_projection(p,index,s,error));p["ordered_message"]["last_release"]["message_type"]=step->type;
        p["ordered_message"]["last_release"]["clear_returned"]=false;require(!camera_projection(p,index,s,error));p["ordered_message"]["last_release"]["clear_returned"]=true;
        p["tutorial_observer"]["world_epoch"]=3;require(!camera_projection(p,index,s,error));
    }
    for(int index:{8,10,12}){
        Harness h(index);require(h.fixture.start(index,"once","world",0,error));
        require(!h.fixture.start(index,"duplicate","world",0,error));
        if(index==10){require(h.calls.empty());require(h.fixture.frame(1,error));require(h.calls.size()==1);require(h.fixture.polled(CameraInput::RotateDown,1,error));h.actual.key_down=true;h.actual.axis=1;h.actual.mouse_relative=true;h.actual.mouse_focus=true;}
        require(h.fixture.frame(index==10?2:1,error));require(h.fixture.receipt().pulses_attempted==1);
        require(h.fixture.frame(index==10?3:2,error));require(h.fixture.receipt().pulses_attempted==1);
        require(h.fixture.polled(index==10?CameraInput::RotateMotion:index==8?CameraInput::WheelIn:CameraInput::WheelOut,3,error));
        h.release_step();require(h.fixture.frame(4,error));
        if(index==10){require(h.fixture.receipt().up_attempted);require(h.fixture.polled(CameraInput::RotateUp,4,error));h.actual.key_down=false;h.actual.axis=0;require(h.fixture.frame(5,error));}
        require(h.fixture.receipt().confirmed&&!h.fixture.receipt().active);require(h.fixture.receipt().button_released);
        const auto count=h.calls.size();require(h.fixture.frame(6,error));require(h.calls.size()==count);
        require(!h.fixture.start(index,"retry","world",7,error));
    }
    {Harness h(10);require(h.fixture.start(10,"once","world",0,error));require(h.fixture.frame(1,error));h.observe_ok=false;
        require(!h.fixture.frame(2,error));require(h.fixture.receipt().up_attempted);require(h.calls.size()==2);
        require(h.fixture.polled(CameraInput::RotateUp,2,error));require(!h.fixture.frame(3,error));require(!h.fixture.receipt().active);
        require(h.fixture.receipt().cause=="lost native world");require(h.calls.size()==2);}
    {Harness h(10);require(h.fixture.start(10,"once","world",0,error));require(h.fixture.frame(1,error));h.now=15000;
        require(!h.fixture.frame(2,error));require(h.fixture.receipt().up_attempted);h.now=18000;
        require(!h.fixture.frame(3,error));require(!h.fixture.receipt().active);require(!h.fixture.receipt().release_cause.empty());require(h.calls.size()==2);}
    {Harness h(8);require(h.fixture.start(8,"once","world",0,error));h.send_ok=false;
        require(!h.fixture.frame(1,error));require(h.fixture.receipt().pulses_attempted==1&&h.fixture.receipt().pulses_returned==0);
        require(!h.fixture.frame(2,error));require(h.calls.size()==1);}
    {Harness h(8);h.actual.key_down=true;require(!h.fixture.start(8,"once","world",0,error));require(h.calls.empty());}
    {Harness h(10);require(h.fixture.start(10,"once","world",0,error));require(h.fixture.frame(1,error));h.send_ok=false;h.observe_ok=false;
        require(!h.fixture.frame(2,error));require(h.calls.size()==2);require(h.fixture.receipt().up_attempted&&!h.fixture.receipt().up_returned);
        require(!h.fixture.frame(3,error));require(h.calls.size()==2);h.now=3000;require(!h.fixture.frame(4,error));require(!h.fixture.receipt().active);
        require(!h.fixture.start(12,"next","world",4,error));}
    {Harness h(10);h.position_pending=true;require(h.fixture.start(10,"once","world",0,error));
        require(h.fixture.frame(1,error)&&h.calls.empty());require(h.fixture.polled(CameraInput::Position,1,error));
        require(h.fixture.frame(1,error)&&h.calls.empty());require(h.fixture.frame(2,error)&&h.calls.size()==1&&h.calls[0]==CameraInput::RotateDown);}
    {Harness h(10);h.position_pending=true;require(h.fixture.start(10,"once","world",0,error));
        require(h.fixture.polled(CameraInput::Position,1,error));h.position_ok=false;
        require(!h.fixture.frame(2,error)&&h.calls.empty()&&!h.fixture.receipt().button_attempted);}
    {Harness h(8);require(!h.fixture.start(8,"once","other",0,error));require(h.calls.empty());}
    std::cout<<checks<<" actual camera input checks passed\n";
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
