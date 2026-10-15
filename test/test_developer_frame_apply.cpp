#include "developer_staff/frame_apply.h"
#include <iostream>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void need(bool b,const char* e){++checks;if(!b)throw std::runtime_error(e);}
staff::PoolState pool(){staff::PoolState p;p.count=2;for(int32_t slot=0;slot<50;++slot){staff::PoolRow r;r.id=49-slot;r.employed=r.active=slot<2;r.state=slot==0?6:0;p.rows.push_back(r);p.indoor.push_back(r.id);}return p;}
struct Calls {
    staff::PoolState current;unsigned pins=0,fields=0,reads=0,invalidations=0;
    bool pin_ok=true,read_ok=true,invalidate_ok=true,wrong_readback=false,throws=false;
    unsigned fail_field=0;
    bool pin(const staff::PoolState& expected,std::string& e){++pins;if(!pin_ok){e="actual native owner changed";return false;}return current==expected;}
    bool scalar(const staff::FieldChange& change,std::string& e){++fields;if(throws)throw std::runtime_error("original property exception");
        if(fields==fail_field){e="original property cleanup failure";return false;}auto& row=current.rows.at(change.slot);
        if(change.field==staff::scalar::Field::State)row.state=change.value;else row.active=change.value!=0;return true;}
    bool read(staff::PoolState& out,std::string& e){++reads;if(!read_ok){e="original full50 read failure";return false;}out=current;if(wrong_readback)out.rows[49].active=true;return true;}
    bool invalidate_display(std::string& e){++invalidations;if(!invalidate_ok)e="actual display origin changed";return invalidate_ok;}
};
}
int main(){try{std::string e;bool attempted=false,unknown=false;const auto before=pool();auto target=before;
    target.rows[0].state=7;target.rows[1].active=false;
    std::vector<staff::FieldChange> changes;
    need(staff::frame_plan(before,target,changes,e)&&changes==std::vector<staff::FieldChange>{{0,staff::scalar::Field::State,7},{1,staff::scalar::Field::Active,0}},"planner lost actual slot/ID independence or original state/active fields");
    Calls c{before};need(staff::apply_frame(before,target,c,attempted,unknown,e)&&attempted&&!unknown&&c.fields==2&&c.current==target&&c.invalidations==1,"original property application did not read full50 before display invalidation");
    c=Calls{target};need(staff::apply_frame(target,target,c,attempted,unknown,e)&&!attempted&&!unknown&&!c.fields&&!c.invalidations,"duplicate pool replayed original properties or invalidated display");
    auto idle=target;idle.rows[0].state=0;c=Calls{target};need(staff::apply_frame(target,idle,c,attempted,unknown,e)&&c.fields==1&&c.current==idle,"natural TechDebug to Idle state was rejected");
    auto measured=before;measured.count=1;measured.indoor.clear();for(size_t slot=0;slot<50;++slot){auto& row=measured.rows[slot];row.id=int32_t(slot);row.employed=row.active=slot==0;measured.indoor.push_back(row.id);}
    auto completed=measured;completed.rows[0].state=0;c=Calls{measured};
    need(staff::apply_frame(measured,completed,c,attempted,unknown,e)&&attempted&&!unknown&&c.fields==1&&c.current==completed,
        "measured ID0 TechDevelop6 to Idle0 with unchanged49 rows was not adopted exactly");
    for(int mutation=0;mutation<8;++mutation){auto bad=target;
        if(mutation==0){bad.count=1;bad.rows[1].employed=false;}
        if(mutation==1)bad.rows[49].position[0]=1;
        if(mutation==2)std::swap(bad.rows[0].id,bad.rows[1].id);
        if(mutation==3)std::swap(bad.indoor[0],bad.indoor[1]);
        if(mutation==4)bad.rows[0].job_link=true;
        if(mutation==5){bad.rows[0].on_map=true;bad.indoor.erase(bad.indoor.begin());bad.outdoor.push_back(bad.rows[0].id);}
        if(mutation==6)bad.rows[0].state=4;
        if(mutation==7)bad.rows[0].state=8;
        c=Calls{before};need(!staff::apply_frame(before,bad,c,attempted,unknown,e)&&!attempted&&!unknown&&!c.fields&&!c.pins&&!e.empty(),"unbound count/identity/map/job transition reached an original property");}
    c=Calls{before};c.pin_ok=false;need(!staff::apply_frame(before,target,c,attempted,unknown,e)&&!attempted&&!unknown&&e=="actual native owner changed"&&!c.fields,"native pin failure was hidden or mutated");
    for(unsigned failure:{1u,2u}){c=Calls{before};c.fail_field=failure;need(!staff::apply_frame(before,target,c,attempted,unknown,e)&&attempted&&unknown&&c.fields==failure&&e=="original property cleanup failure"&&!c.invalidations,"partial original failure claimed rendered/confirmed success");}
    c=Calls{before};c.read_ok=false;need(!staff::apply_frame(before,target,c,attempted,unknown,e)&&attempted&&unknown&&e=="original full50 read failure"&&!c.invalidations,"post-native read cause was hidden");
    c=Calls{before};c.wrong_readback=true;need(!staff::apply_frame(before,target,c,attempted,unknown,e)&&unknown&&!c.invalidations,"wrong full50 readback admitted display cache");
    c=Calls{before};c.invalidate_ok=false;need(!staff::apply_frame(before,target,c,attempted,unknown,e)&&unknown&&e=="actual display origin changed","display owner failure was suppressed");
    c=Calls{before};c.throws=true;need(!staff::apply_frame(before,target,c,attempted,unknown,e)&&unknown&&e.find("original property exception")!=std::string::npos,"original exception escaped or lost its cause");
    std::cout<<"PASS "<<checks<<" actual indoor/tech frame planner/application checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
