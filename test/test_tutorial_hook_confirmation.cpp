#include "tutorial_replication/owned_sequence_entry.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using Owner=mt2mp::game::tutorial_sync::OwnedSequenceEntry;
Owner owned,owned_commit,owned_pay,owned_staff,owned_display,owned_scenario_start,owned_advance;
int failed_physical=-99,public_resolver_calls=0;
volatile bool game_thread_available=false;
std::vector<int> physical_calls;
struct Memory {
    int reads=0;
    bool copy(uintptr_t address,void* out,size_t count,std::string& error){
        ++reads;
        if(!address||!out||count!=1){error="physical verifier memory arguments differ";return false;}
        *static_cast<unsigned char*>(out)=0xe9;return true;
    }
} memory;
bool check_owner(const Owner& owner,const Owner& expected,uintptr_t target,int kind,std::string& error){
    physical_calls.push_back(kind);
    if(&owner!=&expected||owner.target!=target||!target||!owner.detour||!owner.trampoline||
       !owner.original_verified||!owned.enabled||!owned_commit.enabled||!owned_pay.enabled||!owned_staff.enabled||!owned_display.enabled||!owned_scenario_start.enabled||!owned_advance.enabled){
        error="physical verifier requires its exact enabled owner";return false;}
    if(failed_physical==kind){error="physical verifier failure "+std::to_string(kind);return false;}
    return true;
}
// Loader initialization has no current game-thread resolver receipt.
// Calling this public mutation guard must make the regression fail.
[[maybe_unused]] bool verify_pay_entry(uintptr_t,std::string& error){
    ++public_resolver_calls;if(game_thread_available)return true;
    error="engine operation rejected caller is not bound game thread";return false;
}
namespace start_intent {
template<class Copy>bool verify_pay(const Owner* owner,uintptr_t target,Copy&& copy,std::string& error){
    if(!owner){error="physical Pay owner absent";return false;}
    if(!check_owner(*owner,owned_pay,target,1,error))return false;
    unsigned char byte=0;return copy(target,&byte,size_t(1),error);
}
}
bool verify_sequence_entry(uintptr_t target,std::string& error){
    if(!check_owner(owned,owned,target,2,error))return false;
    unsigned char byte=0;return memory.copy(target,&byte,size_t(1),error);
}
namespace tutorial_sync {
template<class Copy>bool verify_owned_commit_entry(const Owner& owner,uintptr_t target,Copy&& copy,std::string& error){
    if(!check_owner(owner,owned_commit,target,3,error))return false;
    unsigned char byte=0;return copy(target,&byte,size_t(1),error);
}
}
namespace staff {
template<class Copy>bool verify_owned_allocate(const Owner& owner,uintptr_t target,Copy&& copy,std::string& error){
    if(!check_owner(owner,owned_staff,target,4,error))return false;
    unsigned char byte=0;return copy(target,&byte,size_t(1),error);
}
}
namespace staff_display {
template<class Copy>bool verify_owned_display_entry(const Owner& owner,uintptr_t target,Copy&& copy,std::string& error){
    if(!check_owner(owner,owned_display,target,0,error))return false;
    unsigned char byte=0;return copy(target,&byte,size_t(1),error);
}
}
namespace tutorial_sync {
template<class Copy>bool verify_owned_scenario_start_entry(const Owner& o,uintptr_t p,Copy&& copy,std::string& e){if(!check_owner(o,owned_scenario_start,p,-2,e))return false;unsigned char b=0;return copy(p,&b,size_t(1),e);}
template<class Copy>bool verify_owned_advance_entry(const Owner& o,uintptr_t p,Copy&& copy,std::string& e){if(!check_owner(o,owned_advance,p,-1,e))return false;unsigned char b=0;return copy(p,&b,size_t(1),e);}
}
#include "tutorial_hook_confirmation.inc"
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void reset(int physical_failure=-99){
    owned={0x100,0x200,0x300,true,false};owned_commit={0x400,0x500,0x600,true,false};owned_pay={0x700,0x800,0x900,true,false};
    owned_staff={0xa00,0xb00,0xc00,true,false};owned_display={0xd00,0xe00,0xf00,true,false};owned_scenario_start={0x1000,0x1100,0x1200,true,false};owned_advance={0x1300,0x1400,0x1500,true,false};
    failed_physical=physical_failure;public_resolver_calls=0;physical_calls.clear();memory.reads=0;game_thread_available=false;
}
void check(){
    reset();std::string error;
    const bool confirmed=hooks_enabled(error);
    need(confirmed,"loader-thread hook confirmation failed: "+error);
    need(public_resolver_calls==0,"hook confirmation called public game-thread resolver");
    need(owned.enabled&&owned_commit.enabled&&owned_pay.enabled&&owned_staff.enabled&&owned_display.enabled&&owned_scenario_start.enabled&&owned_advance.enabled,"confirmed owner flags not enabled");
    need(physical_calls==std::vector<int>({-2,-1,0,1,2,3,4})&&memory.reads==7,"physical verification order or raw memory callback differs");
    for(int kind=-2;kind<=4;++kind){
        reset(kind);error.clear();
        need(!hooks_enabled(error),"physical failure falsely confirmed");
        need(error=="physical verifier failure "+std::to_string(kind),"physical failure cause lost: "+error);
        need(!owned.enabled&&!owned_commit.enabled&&!owned_pay.enabled&&!owned_staff.enabled&&!owned_display.enabled&&!owned_scenario_start.enabled&&!owned_advance.enabled,"physical failure retained an enabled owner");
        need(public_resolver_calls==0,"physical failure path called public resolver");
        need(physical_calls.size()==size_t(kind+3)&&memory.reads==kind+2,"verification continued after physical failure");
    }
}
}
int main(){try{check();std::cout<<"PASS actual hooks_enabled: loader success and Start/Advance/Display/Pay/Sequence/Commit/Staff rollback\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
