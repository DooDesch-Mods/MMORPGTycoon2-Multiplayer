#include "toon_weapon_visual/visual.h"
#include "live_host_state/day_rollover/economy/monetary.h"
#include <iostream>
#include <stdexcept>
namespace toon_weapon_visual=mt2mp::game::toon_weapon_visual;
namespace day=mt2mp::game::live_host_state::day;
namespace {
unsigned refreshes=0,stores=0,checks=0;bool refresh_ok=true,store_ok=true;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
bool fail(std::string& error,const char* cause){error=cause;return false;}
bool at(uintptr_t,size_t,uintptr_t& out,std::string&){out=99;return true;}
bool store_toon_weapon(int,uintptr_t,int,int,int,std::string& error){++stores;if(!store_ok)error="original tuple failure";return store_ok;}
}
namespace mt2mp::game::toon_weapon_visual {
bool refresh(uintptr_t,Receipt& receipt,std::string& error){++refreshes;receipt.owned=receipt.confirmed=refresh_ok;if(!refresh_ok)error="original visual failure";return refresh_ok;}
}
bool actual_seam(bool equal,std::string& e){
    std::optional<int> actual=1,target=equal?1:2;uintptr_t subscriber=1,toon=99;int p=0,weapon_fence=0;
#include "weapon_refresh_branch.inc"
#include "weapon_refresh_store.inc"
    return true;
}
#include "weapon_economy_scope.inc"
int main(){try{
    std::string error;
    need(actual_seam(true,error)&&refreshes==1&&!stores,"equal tuple skipped actual visual refresh");
    need(actual_seam(false,error)&&refreshes==2&&stores==1,"changed tuple did not refresh after original stores");
    store_ok=false;need(!actual_seam(false,error)&&stores==2&&refreshes==2&&error=="original tuple failure","failed tuple continued visual effects");
    store_ok=true;refresh_ok=false;need(!actual_seam(false,error)&&error=="original visual failure","visual failure became scalar success");
    need(!actual_seam(true,error)&&error=="original visual failure","equal tuple swallowed visual failure");
    day::economy::State target;const day::economy::State* slot=nullptr;
    {WeaponEconomyScope scope(slot,&target);need(slot==&target,"admitted Economy scope absent");}
    need(!slot,"normal adoption leaked queued Economy authority");
    try{WeaponEconomyScope scope(slot,&target);throw std::runtime_error("native failure");}catch(const std::runtime_error&){need(!slot,"failed adoption leaked queued Economy authority");}
    std::cout<<checks<<" actual tuple/Bridge boundary checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
