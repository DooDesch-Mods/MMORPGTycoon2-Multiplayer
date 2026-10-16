#include "developer_staff/construction_native.h"
#include "developer_staff/construction_calls.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace mt2mp {
std::string sha256(const std::vector<unsigned char>& bytes){
    if(bytes[0]==1)return "3ef8984fc7bcd2c89ab2f9d39bea0ff2a211505c3220437090b60256d90710db";
    if(bytes[0]==2)return "7148fdbcbc456e6c0f4c369e2b9e67245199dc2f5fd13e05d55219b132f29381";
    if(bytes[0]==3)return "02c14027a44e6a99f18861a97e86af62149d93ac17fa1dde7a71984e3b394290";
    return "mismatch";
}}
namespace fixture {
namespace action_session=mt2mp::action_session;
namespace staff=mt2mp::game::developer_staff;
using Capture=staff::Capture;using ActorPose=staff::construction::ActorPose;using Call=staff::construction::Call;
bool fail(std::string& e,const std::string& why){e=why;return false;}
#include "loaded_actor.inc"
struct Extra {uint8_t box=0,flag=0;uintptr_t data=9000;int32_t capacity=4;};
Capture fixture_capture;std::array<Actor,50> actors;std::array<Extra,50> extras;
staff::job_census::Census census;
bool authorized=true,body_bad=false,read_bad=false,call_bad=false,side_effect=false,queue_effect=false,array_effect=false;
int calls=0,reads=0,fail_read=0,body_reads=0;
void reset(){
    fixture_capture={};fixture_capture.image=0x140000000;fixture_capture.thread=7;fixture_capture.state.rows.resize(50);fixture_capture.local.developers.resize(50);
    actors={};extras={};census={};authorized=true;body_bad=read_bad=call_bad=side_effect=queue_effect=array_effect=false;
    calls=reads=fail_read=body_reads=0;
    for(size_t i=0;i<50;++i){fixture_capture.state.rows[i].id=int32_t(i);fixture_capture.local.developers[i]=1000+i;actors[i].pose.id=int32_t(i);}
    actors[0].moving=1;actors[0].steps=1;actors[0].index=0;
}
bool fixture_read_process(Capture& out,std::string& e){++reads;if(fail_read==reads)return fail(e,"original Capture read failed");out=fixture_capture;return true;}
bool client(const action_session::Context& context,const action_session::Binding&,const Capture& c,std::string& e){return (authorized&&context.role==action_session::Role::Client&&c.pin.time_scale==0)||fail(e,"original Client binding/Clock guard");}
bool owners(const Capture& a,const Capture& b,std::string& e){return (a.image==b.image&&a.pin.ledger==b.pin.ledger&&a.pin.time_scale==b.pin.time_scale)||fail(e,"original owner/ledger changed");}
bool actor(const Capture&,size_t slot,Actor& out,std::string&){out=actors.at(slot);return true;}
template<class T>bool at(uintptr_t receiver,size_t offset,T& out,std::string& e){
    if(read_bad)return fail(e,"original auxiliary read failed");
    const auto& value=extras.at(receiver-1000);const void* source=nullptr;size_t size=0;
    if(offset==0x2a0){source=&value.box;size=sizeof value.box;}else if(offset==0x47f){source=&value.flag;size=sizeof value.flag;}
    else if(offset==0x320){source=&value.data;size=sizeof value.data;}else if(offset==0x32c){source=&value.capacity;size=sizeof value.capacity;}
    if(!source||size!=sizeof out)return fail(e,"unexpected field access");std::memcpy(&out,source,size);return true;
}
struct Memory {bool copy(uintptr_t address,void* out,size_t size,std::string& e){
    ++body_reads;unsigned char marker=0;
    if(address==fixture_capture.image+0x3b3740&&size==32)marker=1;
    if(address==fixture_capture.image+0x3b3210&&size==192)marker=2;
    if(address==fixture_capture.image+0xc406b0&&size==16)marker=3;
    if(!marker)return fail(e,"unexpected body span");std::memset(out,0,size);static_cast<unsigned char*>(out)[0]=body_bad?0:marker;return true;
}} memory;
constexpr int copy=0;
namespace job_census {
using Census=staff::job_census::Census;
bool read(int,const Capture&,Census& out,std::string&){out=census;return true;}
}
uint32_t fixture_stop_loaded_movement(uintptr_t image,uintptr_t receiver,Call* result){
    if(image!=fixture_capture.image)throw std::runtime_error("wrong image");++calls;result->attempted=true;
    auto& a=actors.at(receiver-1000);a.moving=0;
    if(call_bad){result->exception=0xc0000005;return result->exception;}
    a.steps=0;extras.at(receiver-1000).box=0;extras.at(receiver-1000).flag=0;
    if(side_effect)fixture_capture.state.rows[4].state=9;
    if(queue_effect)census.capacity=8;
    if(array_effect)extras.at(receiver-1000).data+=8;
    result->returned=true;return 0;
}
#define read_process fixture_read_process
#define stop_loaded_movement fixture_stop_loaded_movement
#include "developer_staff/loaded_movement.inc"
#undef stop_loaded_movement
#undef read_process
}
namespace {size_t checks=0;void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}}
int main(){try{
    using namespace fixture;action_session::Context context;context.role=action_session::Role::Client;action_session::Binding binding;std::string e;bool attempted=false;
    const auto run=[&](){return settle_loaded_client(context,binding,attempted,e);};
    reset();const auto before=fixture_capture;need(run()&&attempted&&calls==1&&body_reads==3&&fixture_capture.state==before.state,"loaded single waypoint did not settle: "+e);
    need(!actors[0].moving&&!actors[0].steps&&actors[0].index==0,"loaded waypoint not quiet");
    need(run()&&!attempted&&calls==1&&body_reads==3,"already quiet pool repeated original cleanup");
    for(uint8_t box=0;box<2;++box)for(uint8_t flag=0;flag<2;++flag){reset();extras[0].box=box;extras[0].flag=flag;need(run()&&!extras[0].box&&!extras[0].flag,"Boolean auxiliary flags not cleared by original closure");}
    for(int field=0;field<16;++field){reset();auto& a=actors[0];auto& x=extras[0];
        switch(field){case 0:a.current=1;break;case 1:a.destination=1;break;case 2:a.task=1;break;case 3:a.road=1;break;
        case 4:a.node=1;break;case 5:a.path=1;break;case 6:a.pending=1;break;case 7:a.moving=2;break;
        case 8:a.steps=2;break;case 9:a.index=1;break;case 10:x.box=2;break;case 11:x.flag=2;break;
        case 12:x.data=0;break;case 13:x.capacity=0;break;case 14:a.steps=-1;break;case 15:a.index=-1;break;}
        need(!run()&&!attempted&&calls==0&&e.find("not quiescent")!=std::string::npos,"unsupported loaded graph admitted");
    }
    reset();actors[49].task=1;need(!run()&&!attempted&&calls==0,"later unsupported actor allowed partial cleanup");
    reset();context.role=action_session::Role::Host;need(!run()&&!attempted&&calls==0,"Host admitted to Client cleanup");context.role=action_session::Role::Client;
    reset();authorized=false;need(!run()&&!attempted&&calls==0&&e=="original Client binding/Clock guard","Client owner guard lost");
    reset();fixture_capture.pin.time_scale=1;need(!run()&&!attempted&&calls==0,"unpaused Client admitted");
    reset();body_bad=true;need(!run()&&!attempted&&calls==0&&e.find("original body differs")!=std::string::npos,"body mismatch performed effects");
    reset();read_bad=true;need(!run()&&!attempted&&calls==0&&e=="original auxiliary read failed","original memory cause lost");
    reset();fail_read=2;need(!run()&&!attempted&&calls==0&&e=="original Capture read failed","pre-effect reread failure mutated");
    reset();call_bad=true;need(!run()&&attempted&&calls==1&&e.find("unconfirmed SEH=")!=std::string::npos,"partial original exception became success");
    reset();side_effect=true;need(!run()&&attempted&&e.find("full50 state")!=std::string::npos,"unrelated actor effect accepted");
    reset();queue_effect=true;need(!run()&&attempted&&e.find("JobQueue")!=std::string::npos,"Job side effect accepted");
    reset();array_effect=true;need(!run()&&attempted&&e.find("travel-array allocation")!=std::string::npos,"array allocation change accepted");
    reset();actors[1].moving=1;actors[1].steps=1;need(run()&&calls==2,"two separately owned cached actors not settled");
    std::cout<<"PASS "<<checks<<" actual loaded-movement helper cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
