#include "control_input/control_lease.h"
#include "control_input/control_replica.h"
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mt2mp::control_input;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
struct Port {
    ActorState actor{1,2,{{0,0,0},10,0,false},true,false,false,true,{}};
    bool fail_after_position{},fail_target{},transfer{};
    int writes{},jumps{};
    bool read(const Target&,ActorState& out,std::string& e){if(fail_after_position&&writes){e="retained native callback read failure";return false;}out=actor;return true;}
    bool target_exists(const Target&,std::string&){return true;}
    bool scaled_dt(float dt,float& out,std::string&){out=dt;return true;}
    bool propose(const Sample& sample,const Motion& motion,const BasisInput& input,float dt,Proposal& out,uint32_t&,std::string&){return propose_basis(sample,motion,input,dt,out,[](Vec3& v){const auto inverse=1/std::sqrt(v.x*v.x+v.z*v.z);v.x*=inverse;v.z*=inverse;return true;});}
    bool jump(const Target&,uint32_t&,std::string&){++jumps;return true;}
    bool constrain(const Target&,Vec3&,float& height,uint32_t&,std::string&){height=0;return true;}
    bool position(const Target&,const Vec3& value,uint32_t&,std::string&){++writes;actor.sample.position=value;if(transfer)actor.uid=9;return true;}
    bool facing(const Target&,const Motion& motion,uint32_t&,std::string&){actor.moving=moving(motion);return true;}
    bool set_target(const Target&,const std::optional<Target>& target,uint32_t&,std::string& error){if(fail_target){error="retained target failure";return false;}actor.target=target;return true;}
};
int main(){try{
    const Target toon{mt2mp::game::interventions::Family::Toon,"subscriber:1"};
    const Target other{mt2mp::game::interventions::Family::Toon,"subscriber:2"};
    const std::string id="11111111-1111-4111-8111-111111111111";
    Intent acquire;acquire.actor=toon;acquire.lease=id;std::string error;
    mt2mp::game::interventions::Intent wire;wire.target=toon;wire.command=mt2mp::game::interventions::Command::ControlAcquire;wire.control.emplace();wire.control->lease=id;
    require(mt2mp::game::interventions::codec::intent(mt2mp::game::interventions::encode(wire))==wire,"Control intent strict roundtrip");
    auto malformed=mt2mp::game::interventions::encode(wire);malformed["control"]["basis"][0]=2;bool rejected=false;try{(void)mt2mp::game::interventions::codec::intent(malformed);}catch(const std::exception&){rejected=true;}require(rejected,"Control non-native basis rejected");
    Leases leases;require(leases.commit(2,id,acquire,100,error),"acquire");
    require(!leases.preflight(3,id,acquire,error),"exclusive actor ownership");
    Intent input=acquire;input.operation=Operation::Input;input.input_sequence=1;input.input.axis1=1;input.selected=other;input.jump_sequence=1;
    require(leases.commit(2,"ordered-input",input,100,error),"ordered input");
    require(!leases.commit(2,"ordered-input",input,100,error),"duplicate input rejected");
    require(!leases.preflight(3,"ordered-input",input,error),"foreign peer rejected");
    Port port;StepReceipt receipt;
    require(leases.step(port,2,1,101,0.1f,receipt,error),"first step");
    require(receipt.confirmed&&port.actor.sample.position.x==0.3f&&port.actor.target==other&&port.jumps==1,"native callback movement/target/jump");
    const auto pose=port.actor.sample.position;
    require(!leases.step(port,2,1,102,0.1f,receipt,error)&&port.actor.sample.position==pose,"same frame never reruns");
    require(leases.step(port,2,2,102,0.1f,receipt,error)&&port.jumps==1,"held sample does not repeat jump");
    require(leases.step(port,2,3,1100,0.1f,receipt,error)&&leases.find(2)->expired,"stale input decelerates");
    require(leases.find(2)->motion.velocity.x==3.0f,"stale input uses native deceleration");
    input.input_sequence=2;input.jump_sequence=2;require(leases.commit(2,"input-2",input,1101,error),"fresh sample recovers");
    require(leases.step(port,2,4,1101,0,receipt,error)&&port.jumps==1,"paused native delta does not jump");
    require(leases.step(port,2,5,1102,0.1f,receipt,error)&&port.jumps==1,"paused jump is not delayed");
    Intent release=acquire;release.operation=Operation::Release;require(leases.commit(2,"release",release,1103,error),"release after original readback");
    require(!leases.preflight(2,"stale-input",input,error),"released sample cannot reacquire");
    auto broken=acquire;broken.input.axis1=std::numeric_limits<float>::quiet_NaN();require(!valid(broken),"nonfinite wire input");
    broken=acquire;broken.input.forward=broken.input.right;require(!valid(broken),"nonorthogonal basis rejected");
    Leases failure;require(failure.commit(2,id,acquire,0,error),"failure acquire");input.input_sequence=1;input.jump_sequence=0;require(failure.commit(2,"input",input,0,error),"failure input");
    Port partial;partial.fail_after_position=true;
    require(!failure.step(partial,2,1,1,0.1f,receipt,error)&&receipt.unknown&&failure.locked(),"partial callback failure locks");
    require(failure.find(2)->motion.velocity==Vec3{}&&failure.find(2)->step_frame==0,"unconfirmed motion not committed");
    require(error=="retained native callback read failure","native cause retained");
    const auto attempts=partial.writes;require(!failure.step(partial,2,2,2,0.1f,receipt,error)&&partial.writes==attempts,"Unknown never retries");
    Port reused;reused.transfer=true;Motion motion;BasisInput axes;axes.axis1=1;
    require(!step_host(reused,toon,motion,axes,{},0.1f,receipt,error)&&receipt.unknown,"full UID replacement after callback rejected");
    Port target_failure;target_failure.fail_target=true;require(!step_host(target_failure,toon,motion,axes,other,0,receipt,error)&&receipt.unknown,"dt0 target failure remains Unknown");
    Proposal p;Sample sample{{},2.25f,0,false};BasisInput diagonal;diagonal.axis1=1;diagonal.negative_axis0=1;
    require(propose_basis(sample,{},diagonal,1,p,[](Vec3&){return true;})&&p.next.velocity.x==2.25f&&p.next.velocity.z==2.25f,"native diagonal speed preserved");
    sample.action_timer=1;require(propose_basis(sample,Motion{{3,0,4},false},diagonal,0.1f,p,[](Vec3&){return true;})&&p.next.velocity.x==0&&p.next.velocity.z==0,"action lock stops horizontal input");
    std::cout<<"Control lease and host native-step fixtures passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
