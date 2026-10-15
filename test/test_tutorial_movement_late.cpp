#include "movement_input.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mt2mp::game::tutorial_movement;
namespace {
constexpr auto token="11111111-1111-4111-8111-111111111111",origin="22222222-2222-4222-8222-222222222222";
unsigned tests=0;
void require(bool ok,const char* cause){if(!ok)throw std::runtime_error(cause);}
void pass(const char* name){++tests;std::cout<<"PASS "<<name<<'\n';}
struct Native {
    uint64_t now=10;bool read_failed=false,up_failed=false;
    std::vector<bool> posts;
    Snapshot actual{origin,1,2,3,4,5,0,0,false,true,true};
    Services services(){return {this,[](void*){return uint32_t(7);},[](void* p){return static_cast<Native*>(p)->now;},
        [](void* p,Snapshot& out,std::string& error){auto& n=*static_cast<Native*>(p);if(n.read_failed){error="original native read failure";return false;}out=n.actual;return true;},
        [](void* p,const Snapshot& owner,bool down,std::string& error){auto& n=*static_cast<Native*>(p);require(owner.hwnd==4,"release retargeted");n.posts.push_back(down);if(!down&&n.up_failed){error="original Up failure";return false;}return true;}};}
};
struct Case {
    Native native;Fixture fixture;std::string error;
    Case(){require(fixture.initialize(7,native.services(),error)&&fixture.start(token,origin,7360,error),"fixture initialization/admission failed");}
    void down(float progress=0){require(fixture.poll(5,0,26,'w',true,false,7361,error),"Down poll failed");native.actual.key_down=true;native.actual.axis=1;native.actual.progress=progress;}
    void up(float progress){require(fixture.poll(5,0,26,'w',false,false,7362,error),"Up poll failed");native.actual.key_down=false;native.actual.axis=0;native.actual.progress=progress;}
    void pair_once(){require(native.posts==std::vector<bool>({true,false}),"input pair replayed or release missing");}
};
void checks(){
    {Case c;c.down();require(c.fixture.frame(7361,c.error)&&c.error.empty()&&!c.fixture.receipt().progress_changed&&c.fixture.receipt().axis_down,"first native Down with delayed presentation failed");c.pair_once();
        c.up(4.3394999504089355f);require(c.fixture.frame(7362,c.error)&&c.fixture.receipt().phase==Phase::Confirmed&&c.fixture.receipt().axis_released&&c.fixture.receipt().progress_changed,"actual retained late-progress order was not confirmed");c.pair_once();pass("retained Down axis1/progress0 then Up axis0/progress4.339 confirms without second Up");}
    {Case c;c.down();require(c.fixture.frame(7361,c.error),"Down frame failed");c.up(0);require(c.fixture.frame(7362,c.error)&&c.fixture.receipt().phase==Phase::UpQueued&&c.fixture.receipt().axis_released&&!c.fixture.receipt().progress_changed,"released input with pending presentation was falsely failed/confirmed");
        c.native.now=500;c.native.actual.progress=4.3394999504089355f;require(c.fixture.frame(7363,c.error)&&c.fixture.receipt().phase==Phase::Confirmed,"later bounded native display did not confirm");c.pair_once();pass("released input can wait for the next native Meter frame within existing deadline");}
    {Case c;c.down();require(c.fixture.frame(7361,c.error),"Down frame failed");c.up(0);require(c.fixture.frame(7362,c.error),"early missing progress failed");c.native.now=1010;
        require(!c.fixture.frame(7363,c.error)&&c.error=="tutorial movement progress not observed before release deadline; no replay"&&c.fixture.receipt().axis_released,"missing progress lacked distinct bounded cause");c.pair_once();
        require(!c.fixture.start(token,origin,7400,c.error),"timeout replay admitted");c.pair_once();pass("missing progress expires distinctly after native release and token stays consumed");}
    {Case c;c.down();require(c.fixture.frame(7361,c.error),"Down frame failed");c.up(0);require(c.fixture.frame(7362,c.error),"early missing progress failed");c.native.now=1011;c.native.actual.progress=4.3394999504089355f;
        require(!c.fixture.frame(7363,c.error)&&!c.fixture.receipt().progress_changed&&c.fixture.receipt().phase==Phase::Failed,"display arriving after deadline was accepted");c.pair_once();pass("progress first observed after the deadline cannot turn the receipt green");}
    {Case c;c.down(2);require(c.fixture.frame(7361,c.error),"immediate progress failed");c.up(0);require(c.fixture.frame(7362,c.error)&&c.fixture.receipt().progress_changed&&c.fixture.receipt().phase==Phase::Confirmed,"confirmed earlier progress was overwritten by later frame");c.pair_once();pass("observed native progress is latched without requiring a second input frame");}
    {Case c;require(!c.fixture.frame(7361,c.error)&&c.error.find("Down/axis")!=std::string::npos,"missing actual Down/axis was tolerated");c.pair_once();pass("first completed native frame still requires Down and positive axis");}
    {Case c;c.down();c.native.up_failed=true;require(!c.fixture.frame(7361,c.error)&&c.error=="original Up failure","Up failure cause was replaced with presentation wait");c.native.actual.progress=4.3394999504089355f;
        require(!c.fixture.frame(7362,c.error)&&c.error=="original Up failure"&&c.fixture.receipt().phase==Phase::Failed,"late progress erased release failure");c.pair_once();pass("late progress never masks original release failure or retries Up");}
    {Case c;c.down();require(c.fixture.frame(7361,c.error),"Down frame failed");c.native.read_failed=true;require(!c.fixture.frame(7362,c.error)&&c.error=="original native read failure","post-Up read failure was replaced");c.native.read_failed=false;c.up(4.3394999504089355f);
        require(!c.fixture.frame(7363,c.error)&&c.error=="original native read failure"&&c.fixture.receipt().phase==Phase::Failed,"late progress erased native read failure");c.pair_once();pass("late native display and release do not erase the original read failure");}
    {Case c;c.down();require(c.fixture.frame(7361,c.error),"Down frame failed");c.native.actual.progress=4.3394999504089355f;c.native.actual.generation=3;
        require(!c.fixture.frame(7362,c.error)&&!c.fixture.receipt().progress_changed,"progress from another actual native generation was latched");c.pair_once();pass("late display requires the same exact native owner");}
    {Case c;c.down();require(c.fixture.frame(7361,c.error),"Down frame failed");c.native.now=1010;
        require(!c.fixture.frame(7362,c.error)&&c.error=="tutorial movement release not acknowledged; no replay","unreleased input was mislabeled as missing presentation");c.pair_once();pass("missing native Up keeps its original release timeout cause");}
}
}
int main(){try{checks();std::cout<<tests<<" late-progress scenarios passed\n";return 0;}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
