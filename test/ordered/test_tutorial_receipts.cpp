#include "game/tutorial_replication/ordered_message_receipts.h"
#include <iostream>
using namespace mt2mp::game::tutorial_replication::ordered_message;
namespace a=mt2mp::action_session;
int checks=0;void require(bool value){++checks;if(!value)throw std::runtime_error("readback check "+std::to_string(checks));}
a::Request request(int index,uint64_t peer=1){a::Request r;r.action=a::Action::TutorialAcknowledge;r.peer=peer;r.sequence=1;r.snapshot_uuid="world";
    r.action_uuid="action-"+std::to_string(index)+"-"+std::to_string(peer);r.tutorial={"scenarios/initial_tutorial",index==10?"rotate":"zoomout",index,index==10?2:4,true};return r;}
Release release(const a::Request& r){Release v;v.step=r.tutorial;v.clear_attempted=v.clear_returned=v.advance_attempted=v.advance_returned=true;
    v.undo_before=v.redo_before=v.undo_after=v.redo_after=0;return v;}
int main(){try{
    std::string e;Readback b;auto rotate=request(10),zoom=request(12,2),duplicate=request(10,2);
    require(b.grant(rotate,false,e));require(b.retire(rotate,e));require(b.released(release(rotate),e));
    const auto first=b.status()["releases"][0];require(first["index"]==10&&first["action_uuid"]==rotate.action_uuid&&first["release_sequence"]==1);
    require(b.grant(duplicate,true,e));require(b.retire(duplicate,e));require(!b.retire(duplicate,e));
    require(b.grant(zoom,false,e));require(b.retire(zoom,e));require(b.released(release(zoom),e));
    auto value=b.status();require(value["releases"].size()==2&&value["releases"][0]==first);require(value["releases"][1]["index"]==12);
    require(value["grants"][0]["granted_by_peer"]==value["grants"][0]["retired_by_peer"]);
    require(value["grants"][0]["granted_by_peer"][1]==1&&value["grants"][0]["granted_by_peer"][2]==1);
    require(value["grants"][0]["first_action_uuid"]==rotate.action_uuid&&value["grants"][0]["last_action_uuid"]==duplicate.action_uuid);
    require(!b.released(release(rotate),e));require(!b.grant(zoom,false,e));
    auto wrong=zoom;wrong.snapshot_uuid="other";require(!b.grant(wrong,true,e));require(!b.retire(wrong,e));
    wrong=zoom;wrong.tutorial.message_type=2;require(!b.grant(wrong,true,e));
    Readback fresh;require(!fresh.released(release(rotate),e));require(!fresh.grant(rotate,true,e));
    require(fresh.grant(rotate,false,e));auto broken=release(rotate);broken.advance_returned=false;require(!fresh.released(broken,e));
    require(fresh.status()["releases"].empty());broken=release(rotate);broken.redo_after=1;require(!fresh.released(broken,e));
    require(fresh.status()["releases"].empty());require(fresh.released(release(rotate),e));
    fresh.reset();require(fresh.status()["releases"].empty()&&fresh.status()["grants"].empty());
    for(int i=0;i<124;++i){auto r=request(i<107?i:i-107);if(i>=107)r.tutorial.asset="scenarios/leveltwo_tutorial";
        require(fresh.grant(r,false,e));require(fresh.retire(r,e));require(fresh.released(release(r),e));}
    require(fresh.status()["releases"].size()==124);fresh.reset();require(fresh.grant(rotate,false,e));
    std::cout<<checks<<" actual bounded readback checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
