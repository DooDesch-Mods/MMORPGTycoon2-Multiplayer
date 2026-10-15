#include "movement_input.h"
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace mt2mp::game::tutorial_movement;
namespace mt2mp::game::tutorial_movement {unsigned run_focus_checks();}
namespace {
constexpr auto token="11111111-1111-4111-8111-111111111111";
constexpr auto origin="22222222-2222-4222-8222-222222222222";
unsigned count=0;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* label){++count;std::cout<<"PASS "<<label<<'\n';}
struct Native {
    uint32_t thread=7;uint64_t now=10;bool read_failure=false,empty_read_failure=false,read_throw=false,down_failure=false,up_failure=false,unknown_down=false;
    std::vector<bool> posted;
    Snapshot actual{origin,1,2,3,4,5,0,0,false,true,true};
    Services services(){return {this,[](void* p){return static_cast<Native*>(p)->thread;},[](void* p){return static_cast<Native*>(p)->now;},
        [](void* p,Snapshot& out,std::string& error){auto& n=*static_cast<Native*>(p);if(n.read_throw)throw std::runtime_error("original native read exception");if(n.empty_read_failure){error.clear();return false;}if(n.read_failure){error="original native read failed";return false;}out=n.actual;return true;},
        [](void* p,const Snapshot& owner,bool down,std::string& error){auto& n=*static_cast<Native*>(p);require(owner.hwnd==4,"release followed changed window");n.posted.push_back(down);
            if(down&&n.unknown_down)throw std::runtime_error("original down unknown");
            if(down?n.down_failure:n.up_failure){error=down?"original down queue failed":"original up queue failed";return false;}return true;}};}
};
struct Case {
    Native native;Fixture fixture;std::string error;
    Case(){require(fixture.initialize(7,native.services(),error),"initialize failed");}
    void start(){require(fixture.start(token,origin,10,error),"queue failed");}
    void down(){require(fixture.poll(5,0,26,'w',true,false,11,error),"down poll failed");native.actual.key_down=true;native.actual.axis=1;native.actual.progress=2;}
    void up(){require(fixture.poll(5,0,26,'w',false,false,12,error),"up poll failed");native.actual.key_down=false;native.actual.axis=0;}
};
void checks(){
    {Case c;c.start();require(c.native.posted==std::vector<bool>{true}&&c.fixture.unresolved(),"queue incorrectly claims native effect");c.down();require(c.fixture.frame(11,c.error),"first native frame failed");
        require(c.native.posted==std::vector<bool>({true,false})&&c.fixture.receipt().phase==Phase::UpQueued,"automatic one-frame release absent");
        c.up();require(c.fixture.frame(12,c.error)&&c.fixture.receipt().phase==Phase::Confirmed&&!c.fixture.unresolved(),"actual down/progress/up readback did not confirm");
        require(c.fixture.poll(5,0,26,'w',true,false,20,c.error)&&c.fixture.receipt().phase==Phase::Confirmed,"later ordinary input changed closed receipt");
        require(!c.fixture.start(token,origin,20,c.error)&&c.native.posted.size()==2,"consumed token replayed input");pass("one pair, positive native progress and release required; token never replays");}
    {Case c;c.native.thread=8;require(!c.fixture.start(token,origin,10,c.error)&&c.native.posted.empty(),"worker queued key");pass("wrong thread has no effect");}
    {Case c;c.native.read_failure=true;require(!c.fixture.start(token,origin,10,c.error)&&c.error=="original native read failed"&&c.native.posted.empty(),"read failure lost");pass("actual admission cause survives without input");}
    {Case c;c.native.actual.origin=token;require(!c.fixture.start(token,origin,10,c.error)&&c.native.posted.empty(),"wrong origin accepted");pass("exact native origin required");}
    {Case c;c.native.actual.key_down=true;require(!c.fixture.start(token,origin,10,c.error)&&c.native.posted.empty(),"held key accepted");pass("already held input rejected");}
    {for(unsigned missing=0;missing<2;++missing){Case c;if(!missing)c.native.actual.keyboard_focus=false;else c.native.actual.input_enabled=false;require(!c.fixture.start(token,origin,10,c.error)&&c.native.posted.empty(),"missing focus/native input admitted Down");}pass("SDL focus and native Input+C0 both required before Down");}
    {Case c;c.native.actual.progress=99;require(!c.fixture.start(token,origin,10,c.error)&&c.native.posted.empty(),"near-complete step accepted");pass("one-frame fixture does not start near completion");}
    {Case c;c.native.down_failure=true;require(!c.fixture.start(token,origin,10,c.error)&&c.error=="original down queue failed"&&!c.fixture.unresolved(),"definite queue rejection lost");
        require(!c.fixture.start(token,origin,10,c.error)&&c.native.posted.size()==1,"failed input automatically retried");pass("failed down remains consumed without success-shaped fallback");}
    {Case c;c.start();require(!c.fixture.frame(11,c.error)&&c.native.posted==std::vector<bool>({true,false})&&c.fixture.receipt().phase==Phase::Failed,"queued-only input falsely confirmed");pass("no native axis/progress effect fails and still releases");}
    {Case c;c.start();c.down();c.native.actual.progress=0;require(c.fixture.frame(11,c.error)&&c.fixture.receipt().up_queued&&!c.fixture.receipt().progress_changed,"delayed Meter was prematurely rejected");c.up();c.native.now=1010;require(!c.fixture.frame(12,c.error)&&c.error.find("progress not observed before release deadline")!=std::string::npos,"absent progress did not expire");pass("axis down without tutorial delta cannot confirm");}
    {Case c;c.start();c.native.actual.progress=2;require(!c.fixture.frame(11,c.error)&&c.fixture.receipt().up_queued,"progress-only false green");pass("progress change alone cannot confirm an input edge");}
    {Case c;c.start();c.down();require(!c.fixture.poll(5,0,26,'w',true,false,11,c.error),"duplicate down accepted");require(!c.fixture.frame(11,c.error)&&c.fixture.receipt().up_queued,"duplicate did not release");pass("duplicate own edge fails and releases once");}
    {Case c;c.start();require(c.fixture.poll(6,0,26,'w',true,false,11,c.error)&&c.fixture.poll(5,3,26,'w',true,false,11,c.error),"foreign poll changed outcome");require(!c.fixture.frame(11,c.error)&&!c.fixture.receipt().down_polled,"foreign event admitted");pass("other window and physical device cannot acknowledge virtual fixture");}
    {Case c;c.start();c.down();require(c.fixture.frame(11,c.error),"down failed");c.up();c.native.actual.axis=1;require(c.fixture.frame(12,c.error)&&c.fixture.receipt().phase!=Phase::Confirmed&&c.fixture.unresolved(),"up event alone released native axis");c.native.now=2000;
        require(!c.fixture.frame(13,c.error)&&c.error.find("release not acknowledged")!=std::string::npos&&c.native.posted.size()==2,"release timeout hidden or replayed");pass("up event requires native zero axis and timeout never replays");}
    {Case c;c.start();c.down();require(c.fixture.frame(11,c.error),"down failed");c.native.actual.keyboard_focus=false;c.native.actual.input_enabled=false;c.native.actual.key_down=false;c.native.actual.axis=0;
        require(c.fixture.poll(0,0,26,'w',false,false,12,c.error)&&c.fixture.frame(12,c.error)&&c.fixture.receipt().phase==Phase::Confirmed&&!c.fixture.unresolved(),"null-focus Up could not acknowledge actual released input");pass("Up readback survives focus loss with native null-window event and zero axis");}
    {Case c;c.start();c.down();c.native.up_failure=true;require(!c.fixture.frame(11,c.error)&&c.error=="original up queue failed"&&c.fixture.unresolved(),"release failure hidden");c.fixture.frame(12,c.error);require(c.native.posted.size()==2,"release queue retried");pass("original up failure stays visible and held; no retry");}
    {Case c;c.start();c.down();c.native.read_failure=true;require(!c.fixture.frame(11,c.error)&&c.error=="original native read failed"&&c.fixture.receipt().up_queued,"read failure prevented cleanup");pass("read failure after down releases original bound window");}
    {Case c;c.start();c.down();c.native.read_throw=true;require(!c.fixture.frame(11,c.error)&&c.error=="original native read exception"&&c.fixture.receipt().up_queued,"read exception prevented cleanup");pass("read exception after down releases and retains original cause");}
    {Case c;c.start();c.down();c.native.empty_read_failure=true;require(!c.fixture.frame(11,c.error)&&!c.error.empty()&&c.fixture.receipt().up_queued,"empty failure became success-shaped receipt");pass("missing failure cause cannot become confirmed after cleanup");}
    {Case c;c.start();c.down();c.native.actual.hwnd=99;require(!c.fixture.frame(11,c.error)&&c.fixture.receipt().up_queued,"changed world/window followed new target");pass("owner change fails while release keeps original HWND");}
    {Case c;c.start();require(!c.fixture.suspend("original lifecycle retirement",c.error)&&c.error=="original lifecycle retirement"&&c.fixture.receipt().up_queued,"suspend release lost original cause");
        c.fixture.suspend("later cause",c.error);require(c.native.posted.size()==2,"suspend retried up");pass("retirement automatically queues one release and retains first cause");}
    {Case c;c.native.unknown_down=true;require(!c.fixture.start(token,origin,10,c.error)&&c.error=="original down unknown"&&c.native.posted==std::vector<bool>({true,false})&&c.fixture.unresolved(),"unknown down falsely retired");pass("unknown down triggers one release and remains visible");}
}
}
int main(){try{checks();count+=run_focus_checks();std::cout<<count<<" movement fixture scenarios passed\n";return 0;}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
