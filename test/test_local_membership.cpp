#include "mt2mp/net/local_membership.h"
#include "mt2mp/core/config.h"
#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::net::local_membership;
unsigned checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
namespace context_fixture {
bool physical=true,ready=true;uint64_t generation=2,epoch=7;uint8_t mask=7;
std::string snapshot="11111111-1111-4111-8111-111111111111";
}
namespace mt2mp {
Config& Config::instance(){static Config value;return value;}
}
namespace mt2mp::net {
struct SidecarClient {
    static SidecarClient& instance(){static SidecarClient value;return value;}
    bool connected()const{return context_fixture::physical;}
    bool ready()const{return context_fixture::ready;}
    std::string failure_reason()const{return "original fixture transport failure";}
};
bool local_session_connected(){return context_fixture::physical;}
std::string current_snapshot_id(){return context_fixture::snapshot;}
uint64_t current_world_epoch(){return context_fixture::epoch;}
uint8_t local_session_peer_mask(){return context_fixture::mask;}
uint64_t local_membership_generation(){return context_fixture::generation;}
namespace session_wire {
bool local(){return Config::instance().local_test;}
uint64_t current_session=99;
#include "session_wire_generation.inc"
}
}
namespace mt2mp::action_session {
class LocalSession {public:bool current(Context&,std::string&);};
#include "local_session_context.inc"
}
void context_contract(){
    auto& config=mt2mp::Config::instance();config.local_test=true;config.session_id=42;config.peer_id=2;config.is_host=false;
    namespace wire=mt2mp::net::session_wire;
    mt2mp::action_session::LocalSession provider;mt2mp::action_session::Context out;std::string error;
    require(wire::session_id()==2,"wire retained fixed relay identity after logical binding");
    require(provider.current(out,error)&&out.session==2&&out.local_peer==2&&out.peer_mask==7&&
            out.role==mt2mp::action_session::Role::Client&&out.snapshot_uuid==context_fixture::snapshot,
            "actual local action provider differs from wire generation/snapshot/role");
    context_fixture::generation=0;
    require(!provider.current(out,error)&&error.find("authority")!=std::string::npos,"action context admitted no bound Host generation");
    context_fixture::generation=3;
    require(provider.current(out,error)&&out.session==3&&config.session_id==42,"new action session rewrote relay identity or retained old generation");
    context_fixture::physical=false;
    require(!provider.current(out,error)&&error.find("original fixture transport failure")!=std::string::npos,"action provider lost transport failure");
    context_fixture::physical=true;context_fixture::ready=false;
    require(!provider.current(out,error),"unready physical transport admitted action context");
    context_fixture::ready=true;config.local_test=false;
    require(!provider.current(out,error),"local provider admitted Steam path");
    require(wire::session_id()==99,"local generation replaced Steam logical session");
}
int main()try{
    Registry host,client;std::string error;bool fresh=false;
    require(!host.initialize(0,10,error)&&!error.empty(),"zero local slot admitted");
    require(!host.initialize(5,10,error),"fifth local slot admitted");
    require(!host.initialize(1,0,error),"zero process identity admitted");
    require(host.initialize(1,10,error),"Host initialization failed");
    require(!host.observe(1,11,1,fresh,error),"self sender admitted");
    require(!host.observe(2,10,1,fresh,error),"duplicate local process admitted");
    require(!host.observe(2,20,0,fresh,error),"zero sequence admitted");
    require(host.observe(2,20,5,fresh,error)&&fresh,"Client handshake failed");
    require(host.observe(2,20,5,fresh,error)&&!fresh,"duplicate packet reapplied");
    require(host.observe(2,20,4,fresh,error)&&!fresh,"old packet reapplied");
    require(host.observe(3,30,1,fresh,error)&&fresh,"third physical peer failed");
    require(host.observe(4,40,1,fresh,error)&&fresh,"fourth physical peer failed");
    require(host.commit_host(error)&&host.mask()==15&&host.generation()==1&&!host.changed(),"Host roster omitted peers");
    for(uint64_t slot=2;slot<=4;++slot){
        Registry peer;
        require(peer.initialize(slot,slot*10,error),"Client slot initialization failed");
        require(peer.observe(1,10,1,fresh,error)&&fresh,"Client Host handshake failed");
        require(peer.stage(host.generation(),host.bound(),error)&&peer.adopt_client(error)&&peer.mask()==15,"Client did not adopt the complete four-peer roster");
        require(!peer.commit_host(error),"Client committed Host roster");
    }
    require(client.initialize(2,20,error)&&client.observe(1,10,1,fresh,error),"Client initialization failed");
    require(client.stage(1,host.bound(),error)&&client.adopt_client(error),"initial Client adoption failed");
    const auto original=host.bound();
    auto invalid=original;invalid[0]=99;
    require(!client.stage(2,invalid,error),"reserved roster slot admitted");
    invalid=original;invalid[3]=20;
    require(!client.stage(2,invalid,error),"duplicate process identities admitted");
    invalid=original;invalid[1]=11;
    require(!client.stage(2,invalid,error),"foreign Host roster admitted");
    invalid=original;invalid[4]=0;
    require(!client.stage(1,invalid,error),"same-generation roster rewritten");
    require(!client.observe(1,11,2,fresh,error)&&client.host_instance()==10,"Host migration replaced original authority");
    require(host.observe(2,21,1,fresh,error)&&fresh&&host.changed(),"Client process restart did not reset sequence and request a new world");
    require(host.mask()==15&&host.bound()==original,"restart replaced active binding before world retirement");
    require(host.retirement_mask()==13,"restarted process inherited old process retirement obligations");
    require(host.commit_host(error)&&host.generation()==2&&host.bound()[2]==21&&!host.changed(),"Host restart roster not committed");
    require(client.stage(2,host.bound(),error)&&!client.adopt_client(error)&&error.starts_with("info:"),"old Client process adopted replacement identity");
    Registry restarted;
    require(restarted.initialize(2,21,error)&&restarted.observe(1,10,1,fresh,error)&&restarted.stage(2,host.bound(),error)&&restarted.adopt_client(error),"new process could not adopt refreshed roster");
    require(host.observe(3,30,2,fresh,error,false)&&fresh&&host.changed(),"Client leave did not request a barrier");
    require(host.retirement_mask()==11,"departed Client retained retirement obligation");
    require(host.commit_host(error)&&host.mask()==11,"departed Client retained new world membership");
    require(host.observe(3,30,3,fresh,error,true)&&fresh&&host.changed(),"same-process lobby rejoin not detected");
    require(host.commit_host(error)&&host.mask()==15,"same-process rejoin roster missing");
    host.departed(4);
    require(host.changed()&&host.retirement_mask()==7,"heartbeat loss retained physical retirement obligation");
    require(host.commit_host(error)&&host.mask()==7,"heartbeat loss not reflected in new binding");
    require(restarted.stage(host.generation(),host.bound(),error),"latest roster staging failed");
    require(restarted.mask()==15,"pending roster bypassed world retirement");
    require(restarted.adopt_client(error)&&restarted.mask()==7,"retired Client did not apply refreshed roster");
    require(restarted.stage(1,original,error)&&restarted.mask()==7,"retired generation restored old membership");
    invalid=host.bound();invalid[3]=0;
    require(restarted.stage(6,invalid,error),"future roster stage failed");
    require(!restarted.stage(6,host.bound(),error),"conflicting pending generation admitted");
    require(!restarted.stage(0,original,error),"unbound generation carried active identities");
    require(!host.stage(7,original,error),"Host adopted remote roster");
    context_contract();
    std::cout<<checks<<" actual local membership/context lifecycle cases passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
