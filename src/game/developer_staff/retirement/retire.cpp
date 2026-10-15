#include "retire.h"
#include <algorithm>
#include <cmath>
namespace mt2mp::game::developer_staff::retirement {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
bool common(const Capture& a,const Capture& b){
    return same_origin(a.pin.origin,b.pin.origin)&&a.pin.ledger==b.pin.ledger&&a.pin.time_scale==b.pin.time_scale&&a.pin.pause_scale==b.pin.pause_scale&&
        a.image==b.image&&a.thread==b.thread&&a.local.lounge==b.local.lounge&&a.local.business==b.local.business&&a.local.data==b.local.data&&
        a.local.capacity==b.local.capacity&&a.local.indoor==b.local.indoor&&a.local.outdoor==b.local.outdoor&&a.local.developers==b.local.developers;
}
}
bool prepare(const Capture& c,const Details& d,size_t slot,int32_t id,Plan& p,std::string& e){
    p={};e.clear();
    if(c.state.rows.size()!=50||c.local.developers.size()!=50||slot>=50||c.state.business_count!=c.state.lounge_count)
        return fail(e,"Developer retirement requires settled actual50 pool/count");
    const auto& r=c.state.rows[slot];const auto receiver=c.local.developers[slot];
    const auto inside=std::count(c.local.inside.begin(),c.local.inside.end(),receiver);
    const auto outside=std::count(c.local.outside.begin(),c.local.outside.end(),receiver);
    if(!receiver||r.slot!=int32_t(slot)||r.id!=id||r.employed||!r.active||r.job_link||d.job||d.carried||d.moving>1||!std::isfinite(d.tick_delay))
        return fail(e,"Developer retirement requires exact fired active actor without job/carried monster");
    if(!r.on_map&&inside==1&&outside==0&&r.state==0&&!d.quadtree&&!d.moving){p={slot,id,Route::IndoorIdle};return true;}
    if(r.on_map&&inside==0&&outside==1&&(r.state==4||r.state==5)){p={slot,id,Route::Arriving};return true;}
    return fail(e,"Developer retirement shape is neither indoor idle nor original no-job return arrival");
}
bool effect(const Capture& before,const Details& old,const Capture& after,const Details& now,const Plan& p,std::string& e){
    e.clear();if(!common(before,after)||before.state.business_count!=after.state.business_count||before.state.lounge_count!=after.state.lounge_count)
        return fail(e,"Developer retirement changed Origin/owners/full ledger/Clock/count");
    if(p.slot>=50||before.state.rows.size()!=50||after.state.rows.size()!=50)return fail(e,"Developer retirement after pool size differs");
    auto expected=before.state;auto& row=expected.rows[p.slot];row.active=false;
    auto inside=before.local.inside,outside=before.local.outside;auto detail=old;
    if(p.route==Route::Arriving){
        row.state=0;row.on_map=false;
        const auto receiver=before.local.developers[p.slot];
        outside.erase(std::remove(outside.begin(),outside.end(),receiver),outside.end());inside.push_back(receiver);
        detail.quadtree=0;detail.moving=0;detail.current=old.destination;detail.current_index=old.destination_index;detail.destination=0;
        detail.tick_delay=std::min(0.0f,old.tick_delay);
    }
    if(after.state!=expected||after.local.inside!=inside||after.local.outside!=outside||now!=detail)
        return fail(e,"Developer retirement exact actor/manager/map/movement closure differs");
    return true;
}
}
