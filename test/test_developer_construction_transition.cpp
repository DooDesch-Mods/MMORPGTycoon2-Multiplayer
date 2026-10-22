#include "developer_staff/construction_native.h"
#include "developer_staff/construction_calls.h"
#include "developer_staff/construction_plan.h"
#include "developer_staff/scalar.h"
#include "region_saved_identity.h"
#include "live_host_state/memory_query_timing.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace mt2mp::game;
using namespace mt2mp::game::developer_staff;
namespace action=mt2mp::action_session;
namespace mt2mp::game::developer_staff::construction {
#include "construction_transition_actor.inc"
}
namespace fixture {
constexpr uintptr_t image=0x10000000,map=0x20000000,root=map+0x1c0,region=0x30000000,
    saved=0x40000000,owned=0x40001000,first_actor=0x50000000;
struct Model {
    Capture current;
    std::vector<construction::Actor> actors;
    std::vector<construction::JobState> jobs;
    std::map<uintptr_t,unsigned char> bytes;
    std::vector<std::string> calls;
    bool fail_region=false,bad_region=false,bad_shard=false,wrong_membership=false,missing_quadtree=false;
} m;
template<class T>void put(uintptr_t address,const T& value){
    const auto* data=reinterpret_cast<const unsigned char*>(&value);
    for(size_t i=0;i<sizeof value;++i)m.bytes[address+i]=data[i];
}
size_t slot(uintptr_t actor){
    const auto found=std::find(m.current.local.developers.begin(),m.current.local.developers.end(),actor);
    if(found==m.current.local.developers.end())throw std::runtime_error("unregistered actor callback");
    return size_t(found-m.current.local.developers.begin());
}
void need(bool condition,const std::string& cause){if(!condition)throw std::runtime_error(cause);}
bool read(uintptr_t address,void* output,size_t count,std::string& error){
    for(size_t i=0;i<count;++i)if(!m.bytes.contains(address+i)){error="actual fixture field missing";return false;}
    for(size_t i=0;i<count;++i)static_cast<unsigned char*>(output)[i]=m.bytes.at(address+i);
    return true;
}
}
namespace mt2mp::game::developer_staff {
bool read_process(Capture& out,std::string&){out=fixture::m.current;return true;}
}
namespace mt2mp::game::bug_authority {
bool preflight_work(const Capture&,const WorkState&,std::string& error){error="unexpected Bug-work frame";return false;}
bool begin_work(const Capture&,const WorkState&,bool&,std::string& error){error="unexpected Bug-work begin";return false;}
bool work_actor_links(const Capture&,const WorkState&,uintptr_t,uintptr_t,bool,bool&,std::string& error){error="unexpected Bug-work pair";return false;}
bool finish_work(const Capture&,const WorkState&,bool&,std::string& error){error="unexpected Bug-work finish";return false;}
uint32_t work_region(uintptr_t,uintptr_t actor,uintptr_t selected,construction::Call* out){
    auto& m=fixture::m;out->attempted=true;m.calls.push_back("region");
    if(m.fail_region){out->exception=7;return 7;}
    const auto slot=fixture::slot(actor);
    fixture::need(!m.current.state.rows[slot].on_map&&!m.actors[slot].quadtree,"SetRegion ran after map insertion");
    fixture::need(selected==fixture::region,"SetRegion received an unresolved target");
    m.actors[slot].region=m.bad_region?selected+8:selected;
    m.actors[slot].shard=m.bad_shard?0:fixture::root;
    m.actors[slot].pose.region_id=60;
    fixture::put(actor+0x178,m.actors[slot].region);fixture::put(actor+0x170,m.actors[slot].shard);
    out->returned=true;return 0;
}
}
namespace mt2mp::game::developer_staff::construction {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool at(uintptr_t owner,size_t offset,T& out,std::string& error){return fixture::read(owner+offset,&out,sizeof out,error);}
bool client(const action::Context&,const action::Binding&,const Capture&,std::string&){return true;}
bool owners(const Capture& a,const Capture& b,std::string& error){
    return (a.pin.origin.map==b.pin.origin.map&&a.pin.origin.world_epoch==b.pin.origin.world_epoch&&
        a.pin.origin.generation==b.pin.origin.generation&&a.local.developers==b.local.developers)||fail(error,"actual fixture owner drift");
}
bool graph(const Capture&,const std::vector<LocalJob>&,State& out,job_census::Census&,
           std::vector<Actor>& actors,std::string&){
    actors=fixture::m.actors;out={};out.jobs=fixture::m.jobs;
    for(const auto& actor:actors)out.actors.push_back(actor.pose);
    return true;
}
bool code(const Capture&,uintptr_t& dp,uintptr_t& jp,std::string&){dp=1;jp=2;return true;}
#include "construction_transition_quiet.inc"
#include "construction_transition_member_ids.inc"
#include "construction_transition_actor_slot.inc"
#include "construction_transition_work.inc"
#include "construction_transition_call.inc"
bool field(Work& w,size_t slot,scalar::Field kind,int32_t value,std::string&){
    w.attempted=true;fixture::m.calls.push_back(kind==scalar::Field::State?"state":"active");
    if(kind==scalar::Field::State)fixture::m.current.state.rows[slot].state=value;
    else fixture::m.current.state.rows[slot].active=value!=0;
    return true;
}
bool pair(Work& w,uintptr_t actor,const LocalJob&,bool attach,std::string&){
    w.attempted=true;auto& m=fixture::m;m.calls.push_back("pair");const auto slot=fixture::slot(actor);
    fixture::need(m.current.state.rows[slot].on_map&&m.calls.size()>=2&&m.calls[m.calls.size()-2]=="outside","Job attached before map/Lounge callbacks");
    m.current.state.rows[slot].job_link=attach;m.jobs[0].developer_id=attach?int32_t(slot):-1;return true;
}
struct Target {struct Binding{uintptr_t object=0;}binding;};
bool building(const Capture&,const LocalJob&,Target&,std::string& error){return fail(error,"unexpected Building activation");}
bool retire(Work&,const LocalJob&,std::string& error){return fail(error,"unexpected Job retirement");}
uint32_t active_property(uintptr_t,uintptr_t,Call* out){out->exception=1;return 1;}
uint32_t pose(uintptr_t,uintptr_t actor,const ActorPose* target,const std::array<float,3>* position,Call* out){
    auto& m=fixture::m;const auto slot=fixture::slot(actor);out->attempted=true;m.calls.push_back("pose");
    fixture::need(m.actors[slot].shard==fixture::root&&m.actors[slot].region==fixture::region,"pose ran without confirmed Region/root");
    m.actors[slot].pose=*target;m.current.state.rows[slot].position=*position;out->returned=true;return 0;
}
uint32_t unary(uintptr_t,Unary command,uintptr_t actor,Call* out){
    auto& m=fixture::m;const auto slot=fixture::slot(actor);out->attempted=true;
    fixture::need(command==Unary::OnMap||command==Unary::OffMap,"unexpected unary callback");
    m.calls.push_back(command==Unary::OnMap?"onmap":"offmap");
    fixture::need(m.actors[slot].shard==fixture::root&&m.actors[slot].region==fixture::region,"OnMap ran without Region/root");
    fixture::need(m.current.state.rows[slot].position==std::array<float,3>{10,20,30},"OnMap ran before pose");
    m.current.state.rows[slot].on_map=command==Unary::OnMap;
    m.actors[slot].quadtree=command==Unary::OnMap&&!m.missing_quadtree?0x70000000:0;
    out->returned=true;return 0;
}
uint32_t membership(uintptr_t,uintptr_t,uintptr_t actor,bool outside,Call* out){
    auto& m=fixture::m;out->attempted=true;m.calls.push_back("outside");
    fixture::need(outside&&m.current.state.rows[fixture::slot(actor)].on_map,"Lounge exit ran before OnMap");
    if(!m.wrong_membership){std::erase(m.current.local.inside,actor);m.current.local.outside.push_back(actor);}
    out->returned=true;return 0;
}
uint32_t reorder(uintptr_t,uintptr_t,uintptr_t,Call* out){out->exception=1;return 1;}
#include "construction_transition_target_plan.inc"
#include "construction_transition_apply.inc"
}
namespace {
construction::LocalJob link;
PoolState target;
void reset(bool root_present=false){
    using namespace fixture;m={};target={};link={"action:11111111-1111-4111-8111-111111111111",0x80000000,123,true,false,0x81000000,0x82000000,region};
    auto& c=m.current;c.image=image;c.thread=1;c.pin.origin.map=reinterpret_cast<void*>(map);
    c.pin.origin.world_epoch=2;c.pin.origin.generation=2;c.state.business_count=c.state.lounge_count=1;
    c.local.indoor=0x60000000;c.local.outdoor=0x60001000;c.local.lounge=0x60002000;
    target.count=1;target.construction.emplace();
    m.jobs={{link.building_key,true,false,-1}};
    for(int32_t id=0;id<50;++id){
        Row row;row.id=row.slot=id;row.employed=row.active=id==0;c.state.rows.push_back(row);
        const uintptr_t actor=first_actor+uintptr_t(id)*0x1000;
        c.local.developers.push_back(actor);c.local.inside.push_back(actor);
        construction::Actor observed;observed.pose.id=id;observed.shard=root_present?root:0;m.actors.push_back(observed);
        put(actor+0x4d0,uintptr_t(0));put(actor+0x170,observed.shard);put(actor+0x178,uintptr_t(0));
        target.rows.push_back({id,0,id==0,id==0,false,false,{}});target.indoor.push_back(id);
        target.construction->actors.push_back(observed.pose);
    }
    target.rows[0].state=1;target.rows[0].on_map=true;target.rows[0].job_link=true;target.rows[0].position={10,20,30};
    target.indoor.erase(target.indoor.begin());target.outdoor={0};target.construction->actors[0].region_id=60;
    target.construction->jobs={{link.building_key,true,false,0}};
    put(map,image+0x1742860);put(root,image+0x174ad20);put(root+0x48,int32_t(0));
    put(map+0x230,image+0x16f04e0);put(map+0x238,saved);put(map+0x240,int32_t(1));put(map+0x244,int32_t(1));
    put(map+0x358,owned);put(map+0x360,int32_t(1));put(map+0x364,int32_t(1));
    put(saved,region);put(owned,region);put(region,image+0x1787930);put(region+0x70,root);put(region+0x834,int32_t(60));
}
bool plan(std::string& error){
    PoolState expanded;construction::State actual;job_census::Census jobs;std::vector<construction::Actor> actors;
    return construction::target_plan(fixture::m.current,target,{link},expanded,actual,jobs,actors,error);
}
bool apply(bool& attempted,bool& unknown,std::string& error){
    const Capture before=fixture::m.current;
    return construction::apply(action::Context{},action::Binding{},before,target,{link},attempted,unknown,error);
}
}
int main(){try{
    using namespace fixture;std::string error;bool attempted=false,unknown=false;
    for(bool root_present:{false,true}){
        reset(root_present);need(plan(error),"production target_plan rejects initial indoor actor: "+error);
        need(apply(attempted,unknown,error)&&attempted&&!unknown,"production apply failed initial deployment: "+error);
        need(m.calls==std::vector<std::string>{"state","region","pose","onmap","outside","pair"},"original call ordering differs");
        need(m.actors[0].shard==root&&m.actors[0].region==region&&m.actors[0].quadtree&&m.current.state.rows[0].job_link,"deployment lost ownership, quadtree or Job");
    }
    for(int defect=0;defect<9;++defect){
        reset();error.clear();
        if(defect==0)m.actors[0].quadtree=0x1234;
        if(defect==1){m.current.state.rows[0].on_map=true;std::erase(m.current.local.inside,first_actor);m.current.local.outside.push_back(first_actor);}
        if(defect==2)m.current.state.rows[0].job_link=true;
        if(defect==3)target.construction->actors[0].region_id=-1;
        if(defect==4)target.construction->actors[0].region_id=61;
        if(defect==5)put(region+0x70,root+8);
        if(defect==6)target.construction->jobs[0].developer_id=-1;
        if(defect==7){target.indoor.push_back(0);target.outdoor.clear();}
        if(defect==8)m.current.state.rows[0].state=4;
        need(!apply(attempted,unknown,error)&&!error.empty()&&!attempted&&!unknown&&m.calls.empty(),"invalid ownership/Region/Job/membership mutated before rejection defect="+std::to_string(defect));
    }
    for(int defect=0;defect<3;++defect){
        reset();error.clear();m.fail_region=defect==0;m.bad_region=defect==1;m.bad_shard=defect==2;
        need(!apply(attempted,unknown,error)&&attempted&&unknown,"failed original Region transition was not Unknown");
        need(m.calls==std::vector<std::string>{"state","region"},"pose/map mutation continued after failed Region transition");
        need(error.find(defect==0?"SetRegion unconfirmed SEH=7":"SetRegion ownership readback differs")!=std::string::npos,"Region failure cause was lost: "+error);
    }
    reset();m.missing_quadtree=true;error.clear();
    need(!apply(attempted,unknown,error)&&attempted&&unknown,"missing quadtree returned success");
    need(error=="Construction final Developer quadtree/map presence differs","final quadtree guard was bypassed: "+error);
    reset();m.wrong_membership=true;error.clear();
    need(!apply(attempted,unknown,error)&&attempted&&unknown,"missing Lounge membership returned success");
    need(error=="Construction final actor Region/pose/Job/Building/ordered manager frame differs","final membership guard was bypassed: "+error);
    std::cout<<"Production target_plan/apply initial-owner transition cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
