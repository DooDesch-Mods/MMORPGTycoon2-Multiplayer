#include "toon_weapon_visual/visual.h"
#include "live_host_state/day_rollover/economy/monetary.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace toon_weapon_visual=mt2mp::game::toon_weapon_visual;
namespace day=mt2mp::game::live_host_state::day;
using WeaponToon=mt2mp::subscriber_quest_state::Toon;
namespace {
unsigned refreshes=0,stores=0,checks=0;bool refresh_ok=true,store_ok=true;
std::vector<std::string> order;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
bool at(uintptr_t,size_t,uintptr_t& out,std::string&){out=99;return true;}
bool store_toon_weapon(int,uintptr_t,const WeaponToon&,const WeaponToon&,int,std::string& error){++stores;order.push_back("store");if(!store_ok)error="original tuple failure";return store_ok;}
#include "weapon_same_tuple.inc"
}
namespace mt2mp::game::toon_weapon_visual {
bool refresh(uintptr_t,Receipt& receipt,std::string& error){++refreshes;order.push_back("refresh");receipt.owned=receipt.confirmed=refresh_ok;if(!refresh_ok)error="original visual failure";return refresh_ok;}
}
bool actual_seam(const std::optional<WeaponToon>& old,const std::optional<WeaponToon>& actual,
                 const std::optional<WeaponToon>& target,std::string& e){
    uintptr_t subscriber=1,toon=99;int p=0,weapon_fence=0;
#include "weapon_refresh_branch.inc"
    if(!actual||!target)return false;
#include "weapon_refresh_store.inc"
    return true;
}
#include "weapon_economy_scope.inc"
namespace matrix_reader {
using namespace toon_weapon_visual;
struct Memory {bool copy(uintptr_t address,void* out,size_t count,std::string&){std::memcpy(out,reinterpret_cast<const void*>(address),count);return true;}} memory;
unsigned calls=0;
uint32_t matrix_call(void* transform,const void* bone,void* expected){
    ++calls;static_cast<unsigned char*>(transform)[0]=99;
    std::memcpy(expected,bone,sizeof(float)*16);return 0;
}
bool returned(Call& call,std::string&,const char*){call.returned=true;return true;}
struct Base {virtual ~Base()=default;virtual bool matrix(const Plan&,const Part&,Call&,std::string&)=0;};
struct Native:Base {
#include "weapon_matrix_body.inc"
};
}
int main(){try{
    std::string error;WeaponToon old;old.weapon_ilvl=14;old.weapon_category="swords";old.weapon_variant_id=-1889777055;
    need(actual_seam(old,old,old,error)&&!refreshes&&!stores,"unchanged row does not request a visual rebuild");
    for(unsigned change=0;change<5;++change){auto next=old;
        if(change==0)next.xp_bits=123;
        if(change==1)next.seconds_to_level_bits=456;
        if(change==2)next.gold=50;
        if(change==3)next.quests_completed=4;
        if(change==4)next.level=2;
        const auto count=refreshes;
        need(actual_seam(old,old,next,error)&&refreshes==count,"non-weapon Quest scalar does not reconcile weapon");
    }
    for(unsigned field=0;field<3;++field){auto next=old;
        if(field==0)++next.weapon_ilvl;
        if(field==1)next.weapon_category="bows";
        if(field==2)next.weapon_variant_id=1889777055;
        order.clear();const auto count=refreshes;
        need(actual_seam(old,old,next,error)&&refreshes==count+1&&order==std::vector<std::string>({"store","refresh"}),"each real tuple field refreshes after store");
    }
    auto next=old;next.weapon_ilvl=15;const auto prior=refreshes;
    need(actual_seam(old,next,next,error)&&refreshes==prior+1,"already loaded changed authoritative tuple still reconciles");
    need(actual_seam(std::nullopt,old,old,error)&&refreshes==prior+2,"new Toon with identical tuple gets explicit reconcile");
    need(actual_seam(std::nullopt,old,old,error)&&refreshes==prior+3,"same numeric receiver reused for a new Toon is not cached");
    need(actual_seam(old,std::nullopt,std::nullopt,error)&&refreshes==prior+3,"removed Toon needs no receiver refresh");
    store_ok=false;order.clear();need(!actual_seam(old,old,next,error)&&error=="original tuple failure"&&order==std::vector<std::string>({"store"}),"failed tuple prevents visual effects");
    store_ok=true;refresh_ok=false;
    need(!actual_seam(old,old,next,error)&&error=="original visual failure","changed tuple retains visual failure");
    need(!actual_seam(std::nullopt,old,old,error)&&error=="original visual failure","new Toon retains visual failure");refresh_ok=true;
    // Native::matrix is a verifier. Its original calls receive private transform
    // and expected buffers; moving bone/instance matrices are external inputs.
    std::array<unsigned char,0x6c> transform{};std::array<unsigned char,0x100> skeleton{};
    std::array<float,16> bone{},instance{};uintptr_t pointer=uintptr_t(bone.data());std::memcpy(skeleton.data()+0x18,&pointer,sizeof pointer);
    pointer=uintptr_t(instance.data());std::memcpy(skeleton.data()+0xe8,&pointer,sizeof pointer);
    toon_weapon_visual::Plan plan;plan.skeleton=uintptr_t(skeleton.data());toon_weapon_visual::Part part;part.transform=uintptr_t(transform.data());part.bone=0;
    matrix_reader::Native reader;toon_weapon_visual::Call call;
    need(reader.matrix(plan,part,call,error)&&call.returned&&transform[0]==0,"actual verifier mutates only its local transform copy");
    bone[12]=instance[12]=57;bone[13]=instance[13]=11;const auto before_instance=instance;const auto before_bone=bone;const auto count=refreshes;
    next=old;next.xp_bits=999;need(actual_seam(old,old,next,error)&&refreshes==count,"moving armed Actor with unchanged tuple does not reattach for XP");
    call={};need(reader.matrix(plan,part,call,error)&&instance==before_instance&&bone==before_bone&&transform[0]==0,"actual verifier observes advanced matrices without updating game storage");
    instance[12]=58;call={};need(!reader.matrix(plan,part,call,error)&&error=="Weapon attachment actual matrix differs from original bone/transform composition","matrix mismatch remains detectable by actual diagnostic path");
    day::economy::State target;const day::economy::State* slot=nullptr;
    {WeaponEconomyScope scope(slot,&target);need(slot==&target,"admitted Economy scope absent");}
    need(!slot,"normal adoption leaked queued Economy authority");
    try{WeaponEconomyScope scope(slot,&target);throw std::runtime_error("native failure");}catch(const std::runtime_error&){need(!slot,"failed adoption leaked queued Economy authority");}
    std::cout<<checks<<" actual tuple/Bridge/matrix boundary checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
