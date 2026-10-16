#include "bug_authority/parent_visual.h"
#include "bug_authority/job_lifetime.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::bug_authority;
namespace {
int checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Services:ParentServices {
    ParentVisual value;int effects=0;bool bad_state=false,bad_refresh=false,throws=false,drift=false;
    bool authorize(const ParentOwner&,std::string&)override{return true;}
    bool observe(const ParentOwner&,ParentVisual& out,std::string&)override{out=value;if(drift&&effects)out.state=0;return true;}
    bool state(const ParentOwner&,int32_t state,JobCall& call,std::string& error)override{
        call.attempted=true;++effects;value.state=state;if(throws)throw std::runtime_error("original enum exception");
        call.record_confirmed=call.accepted=call.returned=call.cleanup_confirmed=!bad_state;
        if(bad_state)error="original enum failure";return !bad_state;
    }
    bool detail(const ParentOwner&,const ParentVisual&,const ParentVisual& target,bool& attempted,std::string&)override{
        attempted=true;++effects;value=target;return true;
    }
    bool refresh(const ParentOwner&,JobCall& call,std::string& e)override{call.attempted=true;++effects;call.returned=!bad_refresh;if(bad_refresh)e="original refresh failure";return !bad_refresh;}
};
void run(){ParentOwner owner{1,2,3,ParentKind::Building,"action:parent"};ParentVisual target;target.kind=owner.kind;target.key=owner.key;
    Services service;service.value=target;ParentReceipt receipt;
    require(apply_parent_visual(service,owner,target,receipt)&&receipt.confirmed&&!receipt.attempted,"equal parent no effect");
    require(!apply_parent_visual(service,owner,target,receipt)&&service.effects==0,"consumed receipt cannot replay");
    service={};service.value=target;target.state=1;target.bandwidth=4;target.user_set=true;receipt={};
    require(apply_parent_visual(service,owner,target,receipt)&&service.value==target,"maintenance uses original state and detail");
    require(receipt.attempted&&receipt.confirmed&&!receipt.unknown&&service.effects==3,"parent exact three effects confirmed");
    target.state=2;target.bandwidth=8;target.user_set=false;receipt={};require(apply_parent_visual(service,owner,target,receipt),"online completion projects exact state");
    for(int which=0;which<4;++which){Services broken;broken.value=target;auto next=target;next.state=1;
        broken.bad_state=which==0;broken.bad_refresh=which==1;broken.throws=which==2;broken.drift=which==3;receipt={};
        require(!apply_parent_visual(broken,owner,next,receipt),"failed original effect surfaces");
        require(receipt.attempted&&receipt.unknown&&!receipt.confirmed&&!receipt.cause.empty(),"failed attempted parent remains Unknown");
    }
    service={};owner.kind=ParentKind::Npc;target={};target.kind=owner.kind;target.key=owner.key;service.value=target;
    target.state=1;target.tint_override=true;target.tint={.3f,.5f,.7f,1};receipt={};require(apply_parent_visual(service,owner,target,receipt)&&service.value==target,"NPC exact tint state copied");
    target.state=2;target.tint_override=false;receipt={};require(apply_parent_visual(service,owner,target,receipt)&&service.value==target,"NPC completion restores actual tint flag");
    auto bad=target;bad.key="other";receipt={};require(!apply_parent_visual(service,owner,bad,receipt)&&!receipt.attempted,"foreign parent rejected before mutation");
    JobLifetimes jobs;std::string error;JobRow row;row.birth={BirthKind::Publication,"snapshot",3,1};row.bug={ParentKind::Building,"parent",{BirthKind::Action,"action",0,1}};
    require(jobs.born({row,11,12,13,false},error),"original witnessed Job birth accepted");
    require(!jobs.born({row,14,12,13,false},error),"same birth never aliases another receiver");
    auto next=row;next.birth.occurrence=2;require(!jobs.born({next,11,12,13,false},error),"same receiver cannot own two live births");
    jobs.destroyed(11);require(!jobs.pointer(11)&&jobs.consumed(row.birth),"original destructor closes pointer but consumes identity");
    require(jobs.born({next,11,12,13,false},error),"allocator reuse requires new witnessed birth");
    require(!jobs.born({row,15,12,13,false},error),"retired birth replay denied");
    JobRow aborted;aborted.birth={BirthKind::Snapshot,std::string(64,'a'),0,9};aborted.type=0;aborted.valid=false;aborted.developer_id=0;
    require(jobs.born({aborted,21,0,0,false},error),"original saved aborted Job can lack destroyed Bug");
    aborted.birth.occurrence=10;aborted.developer_id=-1;require(!jobs.born({aborted,22,0,0,false},error),"unassigned empty invalid occurrence not published");
}
}
int main(){try{run();std::cout<<checks<<" Bug parent/lifetime checks PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"check "<<checks<<": "<<e.what()<<'\n';return 1;}}
