#include "../../src/game/typed_action/periodic_requests.h"
#include "native_steps_draft.h"
#include <iostream>
using namespace mt2mp::action_session;
namespace tc=mt2mp::action_session::tick_commands;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
struct Fixture {
    struct Save {uint32_t kind=0;bool option=false;std::string filename;};
    struct Controller {
        struct Status {bool pending=false;} value;
        int sent=0;bool send_ok=true;
        Status status()const{return value;}
        bool send_ordered_periodic(const tc::wire::PeriodicRequest&,std::string& error){++sent;if(!send_ok)error="send failed";return send_ok;}
    } controller_;
    Context context_{};tc::PeriodicRequests periodic_requests_;
    std::optional<tc::wire::PeriodicRequest> client_periodic_outbound_;
    std::optional<Save> save_active_;
    std::function<bool(std::string&)> periodic_trigger_;
    bool busy_state=false,quit_pending_=false;uint64_t clock=0;
    bool ordinary_save_pending()const{return busy_state;}
    bool snapshot_active()const{return false;}
    uint64_t now_()const{return clock;}
#include "ordered_periodic_service_body.inc"
};
int main()try{
    std::string error;MT2FW_MT2_StepReceipt receipt{};receipt.tick=10;
    Fixture client;client.context_.role=Role::Client;client.client_periodic_outbound_=tc::wire::PeriodicRequest{1,10,2,2,"world"};
    need(client.periodic_service(receipt,error)&&client.controller_.sent==1&&!client.client_periodic_outbound_,"Client request not sent once");
    need(client.periodic_service(receipt,error)&&client.controller_.sent==1,"Client request replayed");
    Fixture host;host.context_={1,1,1,3,Role::Host,"world"};tc::wire::PeriodicRequest due{1,10,2,2,"world"};
    need(host.periodic_requests_.receive(host.context_,2,due,1,50,error),"Host rejected valid request");int calls=0;
    host.periodic_trigger_=[&](std::string&){++calls;host.save_active_=Fixture::Save{MT2FW_MT2_SAVE_PERIODIC,false,"periodic"};return true;};
    host.clock=2;host.busy_state=true;need(host.periodic_service(receipt,error)&&calls==0,"busy prior Save was coalesced");
    host.busy_state=false;need(host.periodic_service(receipt,error)&&calls==1,"original Host trigger not called");
    need(host.periodic_service(receipt,error)&&calls==1,"original Host trigger replayed");
    Fixture failed;failed.context_={1,1,1,3,Role::Host,"world"};failed.clock=2;
    need(failed.periodic_requests_.receive(failed.context_,2,due,1,50,error),"failure setup");
    failed.periodic_trigger_=[](std::string& cause){cause="original trigger failed";return false;};
    need(!failed.periodic_service(receipt,error)&&error=="original trigger failed"&&failed.periodic_requests_.trigger_count()==0,"trigger failure was hidden");
    std::cout<<"actual periodic service checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
