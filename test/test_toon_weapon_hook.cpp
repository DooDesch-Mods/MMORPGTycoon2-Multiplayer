#include "toon_weapon_visual/visual.h"
#include <atomic>
#include <Windows.h>
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::toon_weapon_visual;
namespace {
uintptr_t image=0;
std::atomic<bool> ready=true;
thread_local bool entered=false;
bool owned=true,rejected=false,partial=false,throw_owner=false;
int originals=0,removes=0,attachments=0,matrices=0;
bool actor_present=true;
Receipt hook_receipt;
bool empty_stack=false,fail_stack_module=false;unsigned module_queries=0;
USHORT WINAPI RtlCaptureStackBackTrace(DWORD skip,DWORD count,PVOID* frames,PDWORD hash){
    if(empty_stack)return 0;
    return ::RtlCaptureStackBackTrace(skip,count,frames,hash);
}
BOOL WINAPI fixture_module(DWORD flags,LPCWSTR address,HMODULE* module){
    if(fail_stack_module&&module_queries++==1){SetLastError(ERROR_INVALID_ADDRESS);return FALSE;}
    return ::GetModuleHandleExW(flags,address,module);
}
void original(uintptr_t,bool){++originals;}
void publish(uintptr_t,const Receipt& r)noexcept{hook_receipt=r;}
struct Native final:Calls {
    bool applied=false;
    bool pin(uintptr_t toon,Lease& lease,bool& actual,std::string& error)override{
        actual=owned;lease.toon=toon;lease.subscriber=2;lease.subscriber_id=158;
        if(rejected)lease.authority_failure={{"selected","committed"},{"subscriber_id",158}};
        if(throw_owner)throw std::runtime_error("owner boundary threw");
        if(rejected){error="exact Client locked cause";return false;}return true;
    }
    bool plan(uintptr_t toon,Plan& p,std::string&)override{p.toon=toon;p.actor=actor_present?3:0;if(actor_present)p.parts.emplace_back();p.variant=7;return true;}
    bool scene(Plan& p,Call& c,std::string&)override{p.scene=4;c.attempted=c.returned=true;return true;}
    bool matches(const Plan& p,bool& same,std::string&)override{same=!p.actor||applied;return true;}
    bool remove(const Plan&,Call& c,std::string&)override{++removes;c.attempted=c.returned=true;return true;}
    bool cache(const Plan&,Call&,std::string&)override{throw std::runtime_error("unexpected cache");}
    bool attach(const Plan&,const Part&,Call& c,std::string& error)override{++attachments;c.attempted=true;if(partial){error="partial original attachment";return false;}
        c.returned=true;applied=true;return true;}
    bool matrix(const Plan&,const Part&,Call& c,std::string&)override{++matrices;c.attempted=c.returned=true;return true;}
};
#define GetModuleHandleExW fixture_module
#include "toon_weapon_equip_body.inc"
#undef GetModuleHandleExW
void reset(){ready=true;entered=false;owned=true;rejected=partial=throw_owner=false;originals=removes=attachments=matrices=0;actor_present=true;hook_receipt={};empty_stack=fail_stack_module=false;module_queries=0;}
int checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
}
int main(){try{
    reset();owned=false;equip(1,true);require(originals==1&&removes==0&&hook_receipt.forwarded,"unjoined/Host exact original once");
    reset();ready=false;equip(1,true);require(originals==1&&removes==0,"startup original once");
    reset();equip(1,true);require(!originals&&removes==1&&attachments==1&&hook_receipt.confirmed&&!hook_receipt.unknown&&!entered,"owned original Equip replaced by deterministic attachment");
    require(hook_receipt.native_callers.is_null(),"successful hook collected failure stack");
    reset();partial=true;equip(1,true);require(!originals&&attachments==1&&hook_receipt.unknown&&hook_receipt.cause=="partial original attachment"&&!entered,"partial mutation no fallback/replay");
    reset();rejected=true;equip(1,true);require(!originals&&!removes&&hook_receipt.owned&&hook_receipt.cause=="exact Client locked cause","known locked Client suppresses original");
    require(hook_receipt.subscriber_id==158&&hook_receipt.authority_failure.at("subscriber_id")==158,
        "failed pin lost identified Subscriber or authority receipt");
    require(hook_receipt.native_callers.at("captured").get<unsigned>()>0&&
        !hook_receipt.native_callers.at("frames").empty()&&hook_receipt.native_callers.at("frames").size()<=12,
        "failed owned hook did not retain bounded actual module-relative callers");
    reset();owned=false;rejected=true;equip(1,true);require(originals==1&&hook_receipt.forwarded&&hook_receipt.cause=="exact Client locked cause","unrelated failed owner original preserved with cause");
    reset();rejected=true;empty_stack=true;equip(1,true);
    require(hook_receipt.cause=="exact Client locked cause"&&hook_receipt.native_callers.at("captured")==0&&
        hook_receipt.native_callers.at("cause")=="Weapon caller capture returned no frames","empty stack concealed or replaced original cause");
    reset();rejected=true;fail_stack_module=true;equip(1,true);
    require(hook_receipt.cause=="exact Client locked cause"&&hook_receipt.native_callers.at("module_lookup_failures")==1&&
        hook_receipt.native_callers.at("first_lookup_error")==ERROR_INVALID_ADDRESS&&
        hook_receipt.native_callers.at("cause")=="Weapon caller module lookup incomplete","degraded module lookup concealed or replaced original cause");
    reset();throw_owner=true;equip(1,true);require(!originals&&!removes&&hook_receipt.owned&&hook_receipt.cause.find("owner boundary threw")!=std::string::npos,"owned callback throw does not randomly fallback");
    reset();entered=true;owned=false;equip(1,true);require(originals==1&&hook_receipt.forwarded,"nested unrelated owner still original");
    reset();entered=true;equip(1,true);require(!originals&&!removes&&hook_receipt.cause.find("nested owned")!=std::string::npos,"nested owned boundary rejects before mutation");
    reset();actor_present=false;equip(1,false);require(hook_receipt.confirmed&&hook_receipt.deferred&&!removes&&!attachments&&!originals,"absent actor hook defers without fallback RNG");
    actor_present=true;equip(1,false);require(hook_receipt.confirmed&&!hook_receipt.deferred&&attachments==1&&matrices==1&&!originals,"late MakeActor Equip reconciles after earlier actor-absent receipt");
    equip(1,true);require(hook_receipt.confirmed&&attachments==2&&matrices==2&&!originals,"costume or rez rebuilt actor at same receiver invokes fresh exact attachment and matrix");
    std::cout<<checks<<" actual Equip boundary checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
