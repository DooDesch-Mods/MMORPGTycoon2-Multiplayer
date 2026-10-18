#include <atomic>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
namespace fixture {
unsigned checks=0,reads=0;uint32_t thread=7;bool read_ok=true;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
}
uint32_t GetCurrentThreadId(){return fixture::thread;}
namespace native {
struct WorldStatus {int active_mode_index=2;bool title_transition_pending=false;int32_t saves_in_progress=-1;} world;
bool read_world_status(WorldStatus& out,std::string& e){++fixture::reads;if(!fixture::read_ok){e="original native WorldStatus read failure";return false;}out=world;return true;}
}
namespace actual {
struct Owner {
    uint32_t thread=7;
    std::optional<int> pending=5;
    std::map<int,int> operations{{1,2}},fixture_receipts{{3,4}};
    std::string cause="retained original gameplay failure";
    bool fixture_locked=true;
    bool operator==(const Owner&)const=default;
};
std::atomic<Owner*> owner{nullptr};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
#include "bug_manual_retirement.inc"
}
int main(){try{
    using fixture::need;std::string e;
    for(int32_t saves:{int32_t(-1),int32_t(-2),int32_t(0)}){
        actual::Owner state;actual::owner=&state;fixture::thread=7;fixture::read_ok=true;fixture::reads=0;native::world={2,false,saves};e.clear();
        need(actual::retire_lobby(e),"actual idle Title sentinel rejected: "+e);
        need(fixture::reads==1&&!state.pending&&state.operations.empty()&&state.fixture_receipts.empty()&&state.cause.empty()&&!state.fixture_locked,"confirmed Title did not clear only BugButton lobby bookkeeping");
    }
    for(int guard=0;guard<8;++guard){
        actual::Owner state;const auto before=state;actual::owner=&state;fixture::thread=7;fixture::reads=0;fixture::read_ok=true;native::world={2,false,-1};e.clear();
        switch(guard){
        case 0:native::world.active_mode_index=3;break;
        case 1:native::world.active_mode_index=-1;break;
        case 2:native::world.title_transition_pending=true;break;
        case 3:native::world.saves_in_progress=1;break;
        case 4:native::world.saves_in_progress=2;break;
        case 5:native::world.saves_in_progress=std::numeric_limits<int32_t>::max();break;
        case 6:fixture::thread=8;break;
        case 7:fixture::read_ok=false;break;
        }
        need(!actual::retire_lobby(e)&&state==before,"rejected Title/thread/read/save guard cleared retained state");
        need(fixture::reads==(guard==6?0u:1u),"wrong-thread retirement performed a native read or repeated observation");
        if(guard==6)need(e=="BugButton lobby retirement requires Mainthread","Mainthread cause changed");
        else if(guard==7)need(e=="original native WorldStatus read failure","original native read cause lost");
        else need(e=="BugButton lobby retirement requires idle Title","Title/transition/positive-save cause changed");
    }
    actual::owner=nullptr;fixture::reads=0;need(actual::retire_lobby(e)&&!fixture::reads,"unbound retirement acquired a native owner");
    std::cout<<fixture::checks<<" actual BugButton retirement checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"error after "<<fixture::checks<<" checks: "<<x.what()<<'\n';return 1;}}
