#include "boss_placement_transaction.h"
#include <iostream>
#include <vector>
using namespace mt2mp::game;
using Intent=mt2mp::action_session::BossPlacementIntent;
namespace {
int failures=0,cases=0;
void check(bool value,const char* label){++cases;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
Intent intent(){Intent value;value.type=3;value.region=17;value.character_sha=std::string(64,'a');value.pose[3]=std::bit_cast<uint32_t>(1.f);for(size_t i=7;i<10;++i)value.pose[i]=std::bit_cast<uint32_t>(1.f);value.point[0]=std::bit_cast<uint32_t>(13.f);return value;}
struct Backend {
    std::vector<std::string> calls;std::string fail;bool paid=true;uintptr_t selected=18,boss=91;
    bool enter(const char* name,std::string& error){calls.emplace_back(name);if(fail==name){error=std::string("original ")+name+" cause";return false;}return true;}
    bool prepare(const Intent&,std::string& error){return enter("prepare",error);}
    bool purchase(const Intent&,bool& out,std::string& error){out=paid;return enter("purchase",error);}
    bool region(const Intent&,uintptr_t& out,std::string& error){out=selected;return enter("region",error);}
    bool add(uintptr_t region,const Intent&,uintptr_t& out,std::string& error){if(region!=selected){error="wrong Region";return false;}out=boss;return enter("add",error);}
    bool confirm(uintptr_t result,std::string& error){if(result!=boss){error="wrong native RAX";return false;}return enter("confirm",error);}
};
}
int main(){
    const auto source=intent();std::string error;
    check(boss_placement_reader::valid(source),"valid semantic placement");
    std::array<unsigned char,0x6c> pose{};check(boss_placement_reader::transform(source,pose,error),"complete semantic transform constructed");
    check(!std::memcmp(pose.data(),source.pose.data(),40)&&pose[0x28]==1,"native transform cache dirty, exact authored values");
    for(size_t index=0;index<10;++index){auto bad=source;bad.pose[index]=0x7f800000;check(!boss_placement_reader::valid(bad),"nonfinite pose rejected");}
    for(size_t index=0;index<3;++index){auto bad=source;bad.point[index]=0x7fc00000;check(!boss_placement_reader::valid(bad),"nonfinite independent release point rejected");}
    auto bad=source;bad.pose[3]=0;check(!boss_placement_reader::valid(bad),"invalid quaternion rejected");bad=source;bad.pose[8]=0;check(!boss_placement_reader::valid(bad),"zero scale rejected");
    Backend native;boss_placement_transaction::Receipt receipt;
    check(boss_placement_transaction::execute(native,source,receipt,error),"native core completes");
    check(native.calls==std::vector<std::string>{"prepare","purchase","region","add","confirm"},"exact native order once, no whole release or second AddBoss");
    check(receipt.attempted&&receipt.purchased&&receipt.region_returned&&receipt.add_returned&&receipt.confirmed&&!receipt.unknown&&receipt.returned_boss==91,"explicit original RAX receipt");
    const auto completed=native.calls;check(!boss_placement_transaction::execute(native,source,receipt,error)&&native.calls==completed,"confirmed transaction cannot replay");
    for(const auto stage:{"prepare","purchase","region","add","confirm"}){
        Backend failing;failing.fail=stage;boss_placement_transaction::Receipt result;
        check(!boss_placement_transaction::execute(failing,source,result,error)&&error==std::string("original ")+stage+" cause","first native cause retained");
        check(result.attempted==(failing.fail!="prepare")&&result.unknown==result.attempted&&!result.confirmed,"attempted mutation differs from never executed");
        if(result.attempted){const auto prior=failing.calls;check(!boss_placement_transaction::execute(failing,source,result,error)&&failing.calls==prior,"unknown native core cannot replay");}
    }
    Backend denied;denied.paid=false;boss_placement_transaction::Receipt denied_receipt;
    check(!boss_placement_transaction::execute(denied,source,denied_receipt,error)&&denied.calls==std::vector<std::string>{"prepare","purchase"}&&!denied_receipt.purchased&&!denied_receipt.add_returned,"unaffordable native Purchase cannot create a Boss");
    Backend absent;absent.boss=0;boss_placement_transaction::Receipt absent_receipt;
    check(!boss_placement_transaction::execute(absent,source,absent_receipt,error)&&absent_receipt.purchased&&absent_receipt.unknown&&!absent_receipt.confirmed,"null AddBoss after charge stays unknown");
    Backend invalid;boss_placement_transaction::Receipt invalid_receipt;
    check(!boss_placement_transaction::execute(invalid,{},invalid_receipt,error)&&invalid.calls.empty()&&!invalid_receipt.attempted,"invalid intent never reaches native core");
    std::cout<<cases<<" Boss placement transaction checks, "<<failures<<" failures\n";return failures?1:0;
}
