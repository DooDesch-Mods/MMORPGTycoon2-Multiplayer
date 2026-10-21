#include "move_selection_refresh.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace refresh=mt2mp::game::move_selection_refresh;
unsigned checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
refresh::Selection selection(){
    refresh::Selection s;s.receiver=100;s.info_window=200;s.focused=300;
    s.lists[0]={11};s.lists[1]={300,400};s.lists[2]={300,400};s.lists[3]={500};
    s.filters[2]=1;return s;
}
struct Fixture {
    refresh::Selection before=selection(),after=selection(),current=selection();
    refresh::Receipt receipt;
    std::array<float,3> real_pose{3860.552f,25.305f,7428.148f};
    std::array<float,3> outline_pose{3820.552f,28.150f,7428.148f};
    std::array<float,3> camera{100,200,300};
    unsigned calls=0,reads=0;bool call_ok=true,read_ok=true;std::string error;
    bool run(uintptr_t target=300){
        return refresh::retained(before,after,target,receipt,
            [&](refresh::Receipt& r,std::string& e){
                ++calls;r.attempted=true;
                if(!call_ok){r.exception=0xdead;e="native refresh fault";return false;}
                outline_pose=real_pose;r.returned=true;return true;
            },
            [&](refresh::Selection& s,std::string& e){++reads;if(!read_ok){e="selection read failed";return false;}s=current;return true;},error);
    }
};
int main()try{
    Fixture retained;const auto original_selection=retained.current;const auto camera=retained.camera;const auto real_pose=retained.real_pose;
    require(retained.outline_pose!=real_pose,"fixture represents stale retained outline after remote Move");
    require(retained.run()&&retained.calls==1&&retained.reads==1,"same selected target invokes original refresh once");
    require(retained.receipt.required&&retained.receipt.attempted&&retained.receipt.returned&&retained.receipt.selection_preserved,"refresh publishes exact receipt");
    require(retained.outline_pose==real_pose,"retained outline receives current native model pose");
    require(retained.current==original_selection&&retained.camera==camera&&retained.real_pose==real_pose,"refresh keeps selection, camera and real pose");
    require(!retained.run()&&retained.calls==1,"attempted refresh cannot replay");
    Fixture newly_selected;newly_selected.before.lists[2].clear();
    require(newly_selected.run()&&!newly_selected.receipt.required&&newly_selected.calls==0,"new original selection needs no retained-outline refresh");
    Fixture removed;removed.after.lists[2].clear();
    require(removed.run()&&!removed.receipt.required&&removed.calls==0,"no reintroduction after target leaves visible selection");
    Fixture hidden;hidden.before.hide_selected_outline=1;hidden.after.hide_selected_outline=1;
    require(hidden.run()&&!hidden.receipt.required&&hidden.calls==0,"hidden selected target stays hidden");
    Fixture highlighted;highlighted.before.lists[2].clear();highlighted.after.lists[2].clear();
    highlighted.before.lists[3]={300};highlighted.after.lists[3]={300};highlighted.current=highlighted.after;
    require(highlighted.run()&&highlighted.calls==1,"retained highlighted target refreshes without selecting it");
    Fixture foreign;foreign.after.receiver=101;
    require(!foreign.run()&&foreign.calls==0,"changed native SelectionManager blocks refresh");
    Fixture queued;queued.after.queue_mode=1;
    require(!queued.run()&&queued.calls==0,"selection queue boundary blocks refresh");
    Fixture reentrant;reentrant.after.selection_in_progress=1;
    require(!reentrant.run()&&reentrant.calls==0,"selection reentry blocks refresh");
    Fixture fault;fault.call_ok=false;
    require(!fault.run()&&fault.receipt.exception==0xdead&&!fault.receipt.returned&&fault.reads==0&&fault.error=="native refresh fault","native original failure remains visible");
    Fixture unreadable;unreadable.read_ok=false;
    require(!unreadable.run()&&!unreadable.receipt.selection_preserved&&unreadable.error=="selection read failed","missing readback never confirms selection preservation");
    Fixture changed;changed.current.lists[1].pop_back();
    require(!changed.run()&&changed.error=="move outline refresh changed local selection","unrelated selected member cannot disappear");
    Fixture focus;focus.current.focused=400;
    require(!focus.run(),"focused selection pointer must remain unchanged");
    Fixture window;window.current.info_window=201;
    require(!window.run(),"native info window must remain unchanged");
    std::cout<<checks<<" retained Move selection outline checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
