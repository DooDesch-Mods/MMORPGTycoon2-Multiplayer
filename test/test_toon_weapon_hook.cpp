#include "toon_weapon_visual/visual.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::toon_weapon_visual;
namespace {
std::atomic<bool> ready=true;
thread_local bool entered=false;
bool owned=true,rejected=false,partial=false,throw_owner=false;
int originals=0,removes=0,attachments=0;
Receipt hook_receipt;
void original(uintptr_t,bool){++originals;}
void publish(uintptr_t,const Receipt& r)noexcept{hook_receipt=r;}
struct Native final:Calls {
    bool applied=false;
    bool pin(uintptr_t toon,Lease& lease,bool& actual,std::string& error)override{
        actual=owned;lease.toon=toon;lease.subscriber=2;
        if(throw_owner)throw std::runtime_error("owner boundary threw");
        if(rejected){error="exact Client locked cause";return false;}return true;
    }
    bool plan(uintptr_t toon,Plan& p,std::string&)override{p.toon=toon;p.actor=3;p.parts.emplace_back();p.variant=7;return true;}
    bool scene(Plan& p,Call& c,std::string&)override{p.scene=4;c.attempted=c.returned=true;return true;}
    bool matches(const Plan&,bool& same,std::string&)override{same=applied;return true;}
    bool remove(const Plan&,Call& c,std::string&)override{++removes;c.attempted=c.returned=true;return true;}
    bool cache(const Plan&,Call&,std::string&)override{throw std::runtime_error("unexpected cache");}
    bool attach(const Plan&,const Part&,Call& c,std::string& error)override{++attachments;c.attempted=true;if(partial){error="partial original attachment";return false;}
        c.returned=true;applied=true;return true;}
    bool matrix(const Plan&,const Part&,Call& c,std::string&)override{c.attempted=c.returned=true;return true;}
};
#include "toon_weapon_equip_body.inc"
void reset(){ready=true;entered=false;owned=true;rejected=partial=throw_owner=false;originals=removes=attachments=0;hook_receipt={};}
int checks=0;
void require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
}
int main(){try{
    reset();owned=false;equip(1,true);require(originals==1&&removes==0&&hook_receipt.forwarded,"unjoined/Host exact original once");
    reset();ready=false;equip(1,true);require(originals==1&&removes==0,"startup original once");
    reset();equip(1,true);require(!originals&&removes==1&&attachments==1&&hook_receipt.confirmed&&!hook_receipt.unknown&&!entered,"owned original Equip replaced by deterministic attachment");
    reset();partial=true;equip(1,true);require(!originals&&attachments==1&&hook_receipt.unknown&&hook_receipt.cause=="partial original attachment"&&!entered,"partial mutation no fallback/replay");
    reset();rejected=true;equip(1,true);require(!originals&&!removes&&hook_receipt.owned&&hook_receipt.cause=="exact Client locked cause","known locked Client suppresses original");
    reset();owned=false;rejected=true;equip(1,true);require(originals==1&&hook_receipt.forwarded&&hook_receipt.cause=="exact Client locked cause","unrelated failed owner original preserved with cause");
    reset();throw_owner=true;equip(1,true);require(!originals&&!removes&&hook_receipt.owned&&hook_receipt.cause.find("owner boundary threw")!=std::string::npos,"owned callback throw does not randomly fallback");
    reset();entered=true;owned=false;equip(1,true);require(originals==1&&hook_receipt.forwarded,"nested unrelated owner still original");
    reset();entered=true;equip(1,true);require(!originals&&!removes&&hook_receipt.cause.find("nested owned")!=std::string::npos,"nested owned boundary rejects before mutation");
    std::cout<<checks<<" actual Equip boundary checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
