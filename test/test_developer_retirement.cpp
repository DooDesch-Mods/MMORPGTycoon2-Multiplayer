#include "retire.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::developer_staff;
using namespace mt2mp::game::developer_staff::retirement;
int checks=0;
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
Capture base(){
    Capture c;c.image=0x140000000;c.thread=1;c.local.lounge=10;c.local.business=11;c.local.data=12;c.local.indoor=13;c.local.outdoor=14;c.local.capacity=64;
    c.state.business_count=5;c.state.lounge_count=5;
    for(int32_t slot=0;slot<50;++slot){Row r;r.slot=slot;r.id=49-slot;r.employed=slot<5;r.active=slot<=5;c.state.rows.push_back(r);c.local.developers.push_back(100+slot);c.local.inside.push_back(100+slot);}
    return c;
}
int main(){try{
    auto before=base();Details old;Plan p;std::string e;
    check(prepare(before,old,5,44,p,e)&&p.route==Route::IndoorIdle,"idle plan");
    auto after=before;after.state.rows[5].active=false;
    check(effect(before,old,after,old,p,e),"idle exact effect");
    check(!effect(before,old,before,old,p,e),"active readback must change");
    auto changed=after;changed.pin.ledger.cash=1;check(!effect(before,old,changed,old,p,e),"ledger mutation");
    changed=after;changed.state.rows[6].active=true;check(!effect(before,old,changed,old,p,e),"other actor mutation");
    changed=after;changed.local.developers[5]=999;check(!effect(before,old,changed,old,p,e),"receiver replacement");
    changed=after;changed.local.inside.pop_back();check(!effect(before,old,changed,old,p,e),"manager corruption");
    check(!prepare(before,old,5,5,p,e),"ID is not slot");
    check(!prepare(before,old,50,0,p,e),"slot bounds");
    auto bad=before;bad.state.business_count=6;check(!prepare(bad,old,5,44,p,e),"count propagation pending");
    bad=before;bad.state.rows[5].employed=true;check(!prepare(bad,old,5,44,p,e),"still employed");
    bad=before;bad.state.rows[5].active=false;check(!prepare(bad,old,5,44,p,e),"already retired");
    Details unsafe=old;unsafe.job=90;check(!prepare(before,unsafe,5,44,p,e),"raw job must be absent");
    unsafe=old;unsafe.carried=90;check(!prepare(before,unsafe,5,44,p,e),"carried monster must be absent");
    unsafe=old;unsafe.quadtree=90;check(!prepare(before,unsafe,5,44,p,e),"indoor quadtree must be absent");
    unsafe=old;unsafe.moving=1;check(!prepare(before,unsafe,5,44,p,e),"idle movement must be absent");
    bad=before;bad.state.rows[5].job_link=true;check(!prepare(bad,old,5,44,p,e),"pointer-free job witness");
    bad=before;bad.state.rows[5].state=6;check(!prepare(bad,old,5,44,p,e),"tech state remains open");
    before.local.inside.erase(before.local.inside.begin()+5);before.local.outside.push_back(105);before.state.rows[5].state=4;before.state.rows[5].on_map=true;
    old.quadtree=500;old.destination=600;old.destination_index=2;old.current=300;old.current_index=1;old.moving=1;old.tick_delay=2;
    check(prepare(before,old,5,44,p,e)&&p.route==Route::Arriving,"no-job arrival plan");
    after=before;after.state.rows[5].state=0;after.state.rows[5].on_map=false;after.state.rows[5].active=false;after.local.outside.clear();after.local.inside.push_back(105);
    auto now=old;now.quadtree=0;now.moving=0;now.current=600;now.current_index=2;now.destination=0;now.tick_delay=0;
    check(effect(before,old,after,now,p,e),"exact original arrival closure");
    auto wrong=now;wrong.current=300;check(!effect(before,old,after,wrong,p,e),"arrival current destination must transfer");
    wrong=now;wrong.destination=600;check(!effect(before,old,after,wrong,p,e),"arrival destination must clear");
    wrong=now;wrong.quadtree=500;check(!effect(before,old,after,wrong,p,e),"arrival quadtree must clear");
    wrong=now;wrong.moving=1;check(!effect(before,old,after,wrong,p,e),"arrival moving flag must clear");
    changed=after;changed.state.rows[5].position[0]=1;check(!effect(before,old,changed,now,p,e),"unexpected pose write");
    bad=before;bad.state.rows[5].state=2;check(!prepare(bad,old,5,44,p,e),"in-flight work stays open");
    bad=before;bad.state.rows[5].state=5;check(prepare(bad,old,5,44,p,e),"tech return arrival");
    old.tick_delay=-2;now.tick_delay=-2;check(effect(bad,old,after,now,p,e),"negative tick delay preserved");
    std::cout<<checks<<" retirement checks passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<"retirement check failed: "<<ex.what()<<"\n";return 1;}}
