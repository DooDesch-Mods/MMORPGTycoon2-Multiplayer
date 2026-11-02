#include "tick_wire.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace tc=mt2mp::action_session::tick_commands;
namespace ts=mt2mp::game::time_session;
namespace {
int checks=0;
void need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
const char* world="11111111-1111-4111-8111-111111111111";
const char* uuid="22222222-2222-4222-8222-222222222222";
LogoIntent logo(){
    auto token=[](int type,const std::string& text){return nlohmann::json::array({type,uint64_t(0),text});};
    auto records=nlohmann::json::array();
    records.push_back(nlohmann::json::array({int64_t(-1),uint64_t(0),token(1,"mmoLogo"),nlohmann::json::array()}));
    records.push_back(nlohmann::json::array({int64_t(0),uint64_t(0),token(1,"title"),nlohmann::json::array({token(2,"Shared world")})}));
    return {std::string(64,'a'),std::string(64,'b'),records};
}
LogoIntent large_logo(){
    using J=nlohmann::json;
    auto token=[](int type,const std::string& value){return J::array({type,uint64_t(0),value});};
    J tokens=J::array();for(int i=0;i<5;++i)tokens.push_back(token(2,std::string(6800,'x')));
    return {std::string(64,'a'),std::string(64,'b'),J::array({J::array({int64_t(-1),uint64_t(0),token(1,"mmoLogo"),tokens})})};
}
struct S:Session {
    Context context;
    explicit S(uint64_t peer):context{17,peer,1,3,peer==1?Role::Host:Role::Client,world}{}
    bool current(Context& out,std::string&)override{out=context;return true;}
};
struct E:Engine {
    int legacy=0;bool changed=false;
    bool bind(const std::string&,Binding& out,std::string&)override{out={2,4};return true;}
    bool current(const Binding&,const std::string&,std::string& cause)override{if(changed){cause="actual world changed";return false;}return true;}
    bool ledger(const Binding&,Ledger&,std::string&)override{++legacy;return false;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{++legacy;return Outcome::Unknown;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{++legacy;return Outcome::Unknown;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{++legacy;return false;}
    bool retire(const Binding&,const Request&,std::string&)override{++legacy;return false;}
    bool receive_authority_state(uint64_t,const uint8_t*,size_t,bool&,std::string&)override{++legacy;return false;}
};
struct T:Transport {
    Handler handler;bool failed=false;int claims=0;
    struct Sent {uint64_t target;std::vector<uint8_t> bytes;};std::vector<Sent> packets;
    bool claim(Handler h,std::string&)override{handler=std::move(h);++claims;return true;}
    bool send(uint64_t peer,const std::vector<uint8_t>& bytes,std::string& cause)override{packets.push_back({peer,bytes});if(failed){cause="retained transport failure";return false;}return true;}
};
struct Peer {
    S session;E engine;T transport;Controller controller;int commands=0,steps=0;bool fail_command=false,reenter=false;
    explicit Peer(uint64_t id):session(id),controller(session,engine,transport,{},1000){}
    bool start(std::string& cause){return controller.start_ordered({session.context},[](const tc::Input&,std::string&){return true;},
        [&](tc::Queue& queue,const tc::Assigned& command,std::string& error){
            ++commands;Context c;uint64_t tick=0;
            need(queue.current_execution(command,c,tick,error)&&c==session.context,"execution lacks actual queue capability");
            const auto status=controller.status();
            need(status.pending&&status.host_revision==command.order,"real executing order absent from status");
            if(const auto* r=std::get_if<Request>(&command.input.command))need(status.action_uuid==r->action_uuid&&r->host_revision==0,"executing UUID or observed revision changed");
            if(fail_command){error="original native interrupted";return Outcome::Unknown;}
            if(reenter){std::string nested;controller.tick(20,nested);}
            return Outcome::Confirmed;
        },[&](uint64_t,std::string&){++steps;return Outcome::Confirmed;},0,cause);}
    bool deliver(Peer& other,size_t packet,std::string& cause){const auto& p=transport.packets.at(packet);return other.controller.receive(session.context.local_peer,p.bytes.data(),p.bytes.size(),10,cause);}
};
void pair(){
    auto host=std::make_unique<Peer>(1),client=std::make_unique<Peer>(2);std::string cause;
    need(host->start(cause)&&client->start(cause),"ordered start failed");
    need(host->controller.ordered_mode()&&host->transport.claims==1,"mode or channel ownership absent");
    need(client->controller.submit_logo(uuid,logo(),1,cause),"existing Logo submit failed");
    need(!client->controller.submit_logo(uuid,logo(),2,cause)&&client->controller.status().pending,"pending input dropped");
    need(client->commands==0&&host->commands==0,"submit executed legacy path");
    need(client->deliver(*host,0,cause)&&client->deliver(*host,0,cause),"input/duplicate receive failed");
    tc::Batch batch;need(host->controller.ordered_seal(batch,10,cause)&&batch.commands.size()==1,"Host did not assign one immutable command");
    need(host->deliver(*client,0,cause),"Client batch admission failed");
    tc::Receipt hr,cr;
    need(host->controller.ordered_consume(1,hr,20,cause)&&client->controller.ordered_consume(1,cr,20,cause),"independent consumption failed");
    need(hr==cr&&host->commands==1&&client->commands==1&&host->steps==1&&client->steps==1,"independent commands/tick differ");
    need(client->deliver(*host,1,cause),"real peer completion failed");
    need(!client->controller.status().pending&&host->controller.can_retire_lobby(),"pending/retirement remained after completion");
    need(client->controller.ordered_consume(1,cr,21,cause)&&client->commands==1&&client->steps==1,"duplicate boundary replayed");
    need(client->controller.submit_ordered_time(ts::Intent::Fast,22,cause),"time input failed");
    need(client->deliver(*host,3,cause),"time transport failed");
    need(host->controller.ordered_seal(batch,23,cause)&&batch.commands.size()==1&&std::holds_alternative<tc::TimeRequest>(batch.commands[0].input.command),"time did not share Host order");
    need(host->engine.legacy==0&&client->engine.legacy==0,"legacy state copy or executor called");
}
void periodic_pair(){
    auto host=std::make_unique<Peer>(1),client=std::make_unique<Peer>(2);std::string cause;int received=0;
    need(host->start(cause)&&client->start(cause),"periodic pair startup failed");
    need(host->controller.bind_ordered_periodic([&](uint64_t peer,const tc::wire::PeriodicRequest& value,std::string&){
        ++received;return peer==2&&value.tick==10&&value.invocation==7;
    },cause),"periodic receiver bind failed");
    tc::wire::PeriodicRequest due{17,10,7,2,world};
    need(client->controller.send_ordered_periodic(due,cause),"Client periodic send failed");
    need(client->deliver(*host,0,cause)&&received==1,"authenticated periodic request not delivered");
    need(host->commands==0&&client->commands==0&&host->steps==0&&client->steps==0,"periodic transport executed a native action");
    due.peer=1;need(!client->controller.send_ordered_periodic(due,cause),"Client spoofed a Host periodic request");
}
void failures(){
    std::string cause;tc::Batch b;tc::Receipt r;
    auto failed=std::make_unique<Peer>(1);need(failed->start(cause),"failure start");
    failed->fail_command=true;
    need(failed->controller.submit_logo(uuid,logo(),1,cause)&&failed->controller.ordered_seal(b,2,cause),"failure admission");
    need(!failed->controller.ordered_consume(1,r,3,cause)&&cause.find("original native interrupted")!=std::string::npos&&failed->steps==0,"native failure hidden or tick advanced");
    need(!failed->controller.ordered_consume(1,r,4,cause)&&failed->commands==1,"unknown action replayed");
    auto send=std::make_unique<Peer>(2);need(send->start(cause),"send start");send->transport.failed=true;
    need(!send->controller.submit_logo(uuid,logo(),1,cause)&&send->controller.status().locked&&send->controller.status().pending&&cause.find("retained transport failure")!=std::string::npos,"failed send lost cause/pending");
    auto timed=std::make_unique<Peer>(2);need(timed->start(cause)&&timed->controller.submit_logo(uuid,logo(),1,cause),"timeout admission");
    need(!timed->controller.tick(1001,cause)&&timed->commands==0&&timed->controller.status().locked,"pending timeout replayed or ignored");
    auto changed=std::make_unique<Peer>(1);need(changed->start(cause),"changed start");changed->session.context.session++;
    need(!changed->controller.ordered_seal(b,1,cause)&&changed->commands==0,"changed session admitted");
    auto legacy=std::make_unique<Peer>(2);need(legacy->start(cause),"legacy reject start");
    Message old;old.session=17;old.request.action=Action::StartStartingpoint;old.request.action_uuid=uuid;old.request.snapshot_uuid=world;old.request.peer=1;old.request.sequence=1;old.request.canonical_record_key="tech:startingpoint";
    std::vector<uint8_t> bytes;need(encode(old,bytes,cause),"legacy fixture invalid");
    need(!legacy->controller.receive(1,bytes.data(),bytes.size(),1,cause)&&legacy->engine.legacy==0,"legacy packet reached authority handler");
    auto reentrant=std::make_unique<Peer>(1);need(reentrant->start(cause),"reentrant start");reentrant->reenter=true;
    need(reentrant->controller.submit_logo(uuid,logo(),1,cause)&&reentrant->controller.ordered_seal(b,2,cause),"reentrant admission");
    need(!reentrant->controller.ordered_consume(1,r,3,cause)&&reentrant->steps==0&&reentrant->commands==1,"reentrant Controller advanced tick");
    auto stale=std::make_unique<Peer>(1);need(stale->start(cause),"world start");
    need(stale->controller.submit_logo(uuid,logo(),1,cause)&&stale->controller.ordered_seal(b,2,cause),"world admission");stale->engine.changed=true;
    need(!stale->controller.ordered_consume(1,r,3,cause)&&stale->commands==0,"changed native binding executed");
}
void backpressure(){
    auto host=std::make_unique<Peer>(1),client=std::make_unique<Peer>(2);std::string cause;tc::Batch a,b;
    need(host->start(cause)&&client->start(cause),"capacity start");
    need(host->controller.submit_logo(uuid,large_logo(),1,cause),"large Host input rejected");
    need(client->controller.submit_logo("33333333-3333-4333-8333-333333333333",large_logo(),1,cause)&&client->deliver(*host,0,cause),"large Client input rejected");
    need(host->controller.ordered_seal(a,2,cause)&&a.commands.size()==1&&host->controller.ordered_state().assigned_order==1,"byte limit assigned or lost second input");
    need(host->controller.ordered_seal(b,3,cause)&&b.commands.size()==1&&b.commands[0].order==2&&std::get<Request>(b.commands[0].input.command).peer==2,"retained byte-backpressure input lost or reordered");
    need(host->commands==0&&client->commands==0&&!host->controller.can_retire_lobby(),"seal executed native or retired outstanding input");
    auto waiting=std::make_unique<Peer>(1),sender=std::make_unique<Peer>(2);
    need(waiting->start(cause)&&sender->start(cause)&&sender->controller.submit_logo(uuid,logo(),1,cause)&&sender->deliver(*waiting,0,cause),"waiting fixture");
    need(!waiting->controller.tick(1010,cause)&&cause.find("assignment timeout")!=std::string::npos&&waiting->commands==0,"remote unassigned input timed out silently");
}
void host_ack(){
    auto host=std::make_unique<Peer>(1),client=std::make_unique<Peer>(2);std::string cause;tc::Batch b;tc::Receipt r;
    need(host->start(cause)&&client->start(cause)&&host->controller.submit_logo(uuid,logo(),1,cause),"Host local admission");
    need(host->controller.ordered_seal(b,2,cause)&&host->deliver(*client,0,cause),"Host local batch");
    need(host->controller.ordered_consume(1,r,3,cause)&&host->controller.status().pending&&host->controller.status().ack_mask==1,"Host prematurely completed before peer receipt");
    need(client->controller.ordered_consume(1,r,4,cause)&&client->deliver(*host,0,cause),"Host local completion");
    need(!host->controller.status().pending&&host->controller.status().confirmed==1&&host->controller.status().ack_mask==3,"Host exact completion mask missing");
}
}
int main(){try{pair();periodic_pair();failures();backpressure();host_ack();std::cout<<checks<<" ordered checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
