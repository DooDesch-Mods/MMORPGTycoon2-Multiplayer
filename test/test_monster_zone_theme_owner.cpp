#include "monster_zone_theme_find.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
namespace monster_zone_theme_find=mt2mp::game::monster_zone_theme_find;
struct Memory {int process=0;};
struct Model {
    uintptr_t found=0x5000,ap=0x1417321a0;std::string id="character_theme";
    uint32_t construct_error=0,find_error=0,cleanup_error=0;
    unsigned constructs=0,finds=0,cleanups=0,reads=0,guards=0;
    bool changed_origin=false,bad_code=false;uint32_t caller=7;
} model;
struct Runtime {uintptr_t image=0x140000000;uint32_t thread=7;bool ready=true,capture_fault=false;} state;
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
uint32_t GetCurrentThreadId(){return model.caller;}
constexpr const char* exe_sha="fixture";
namespace a {struct Binding {};}
namespace host {
struct LookupApi {};struct NativeOrigin {};
struct WorldState {uintptr_t root=0x100;struct Origin {uintptr_t map=0x200,mode=0x300;unsigned generation=1;} origin;};
bool resolve_lookup(int,uintptr_t,const char*,LookupApi&,std::string&){return true;}
struct Reader {
    template<class... T> explicit Reader(T&&...){}
    bool current(WorldState& out,bool,std::string&){out={};if(++model.reads>1&&model.changed_origin)out.origin.generation=2;return true;}
};
}
namespace action_runtime {
bool bound(const host::WorldState&,const a::Binding&,const std::string&,std::string&){return true;}
bool same_world(const host::WorldState& x,const host::WorldState& y){return x.origin.generation==y.origin.generation;}
}
namespace q {struct World {uintptr_t image=0,root=0,map=0,region=0;int32_t saved_region=0;};}
namespace selection {
struct Catalogue {};
template<class M>bool read(M&,uintptr_t,int32_t,Catalogue&,std::string&){return true;}
}
namespace d {struct ReadMemory {Memory& memory;};}
namespace region_saved_identity {
template<class F>bool resolve(uintptr_t,uintptr_t,uintptr_t,int32_t,uintptr_t,uintptr_t& out,F&&,std::string&){out=0x4000;return true;}
}
struct Pin {host::WorldState world;q::World region;uintptr_t window=0;selection::Catalogue catalogue;int32_t min_level=0,max_level=0;};
template<class T>bool field(Memory&,uintptr_t p,size_t o,T& out,std::string& e){
    if(p==0x4000){if(o==0x850||o==0x838){out=1;return true;}if(o==0x83c){out=2;return true;}}
    if(p==model.found&&o==0){out=static_cast<T>(model.ap);return true;}
    return fail(e,"fixture unexpected field - Mode ownership must not be used");
}
bool name(Memory&,uintptr_t p,std::string& out,std::string& e){if(p!=model.found+0x1b0)return fail(e,"fixture wrong name field");out=model.id;return true;}
bool code(Memory&,size_t index,bool hooked,std::string& e){++model.guards;return (index>=12&&index<16&&!hooked&&!model.bad_code)||fail(e,"original Find body changed");}
struct ThemeNative {
    uintptr_t image;
    uint32_t construct(){++model.constructs;return model.construct_error;}
    uint32_t find(uintptr_t& out){++model.finds;out=model.found;return model.find_error;}
    uint32_t cleanup(){++model.cleanups;return model.cleanup_error;}
};
#include "zone_theme_owner_body.inc"
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
void reset(){model={};state={};}
bool run(Pin& out,std::string& e,uintptr_t expected=0){Memory m;return pin(m,a::Binding{},"fixture",67,0,out,e,expected);}
int main(){try{
    {reset();Pin p;std::string e;require(run(p,e,0x5000)&&p.window==0x5000&&model.reads==2&&model.guards==4&&model.constructs==1&&model.finds==1&&model.cleanups==1,"native-measured Registry-only owner rejected");}
    {reset();model.found=0;Pin p;std::string e;require(!run(p,e)&&e=="Zone original Window.Find character_theme absent"&&!state.capture_fault&&model.cleanups==1,"null Find result accepted or leaked");}
    {reset();model.ap=state.image+1;Pin p;std::string e;require(!run(p,e)&&e=="Zone original Window.Find CharacterTheme type differs","wrong type accepted");}
    {reset();model.id="monster_placement";Pin p;std::string e;require(!run(p,e)&&e=="Zone original Window.Find CharacterTheme name differs","wrong name accepted");}
    {reset();Pin p;std::string e;require(!run(p,e,0x6000)&&e=="Zone original Window.Find differs from actual brush-linked window","different actual brush owner accepted");}
    {reset();model.changed_origin=true;Pin p;std::string e;require(!run(p,e)&&e=="Zone full Origin changed during pin","changed Origin accepted");}
    {reset();model.bad_code=true;Pin p;std::string e;require(!run(p,e)&&e=="original Find body changed"&&model.constructs==0,"changed original code called");}
    {reset();model.caller=8;Pin p;std::string e;require(!run(p,e)&&model.constructs==0,"off-thread Find called");}
    {reset();model.cleanup_error=5;Pin p;std::string e;require(!run(p,e)&&state.capture_fault&&model.cleanups==1&&e.find("cleanup_exception=5")!=std::string::npos,"cleanup failure lost");}
    {reset();model.find_error=6;Pin p;std::string e;require(!run(p,e)&&state.capture_fault&&model.cleanups==1&&e.find("exception=6")!=std::string::npos,"failed Find skipped cleanup");}
    {reset();model.construct_error=7;Pin p;std::string e;require(!run(p,e)&&state.capture_fault&&model.finds==0&&model.cleanups==0,"failed constructor reused incomplete object");}
    {reset();model.find_error=6;model.cleanup_error=5;Pin p;std::string e;require(!run(p,e)&&e.find("exception=6 cleanup_exception=5")!=std::string::npos,"original failure overwritten by cleanup");}
    {reset();state.capture_fault=true;Pin p;std::string e;require(!run(p,e)&&model.constructs==0&&e.find("no replay")!=std::string::npos,"unresolved temporary lifetime replayed");}
    std::cout<<"PASS 13 original Find/full-pin cases\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
