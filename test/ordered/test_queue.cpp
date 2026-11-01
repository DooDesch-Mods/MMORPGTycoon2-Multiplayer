#include "tick_commands.h"
#include <iostream>
#include <deque>
using namespace mt2mp::action_session;
namespace tc=mt2mp::action_session::tick_commands;
using Intent=mt2mp::game::time_session::Intent;
int checks=0;
void need(bool good,const std::string& error){++checks;if(!good)throw std::runtime_error(error);}
tc::Config config(uint64_t peer=1){tc::Config c;c.context={77,peer,1,3,peer==1?Role::Host:Role::Client,"snapshot-A"};return c;}
bool valid(const tc::Input&,std::string&){return true;}
tc::Input action(uint64_t peer,uint64_t seq,Action kind=Action::PlaceBuilding){Request r;r.peer=peer;r.sequence=seq;r.action=kind;r.action_uuid="action-"+std::to_string(peer)+"-"+std::to_string(seq);r.snapshot_uuid="snapshot-A";r.target_action_uuid="previous-action";r.canonical_record_key="saved:building[2]";return {77,"snapshot-A",r};}
tc::Input time(uint64_t peer,uint64_t seq,Intent intent=Intent::Pause){return {77,"snapshot-A",tc::TimeRequest{peer,seq,0,intent}};}
struct Native {
    std::vector<tc::Assigned> commands;
    std::vector<uint64_t> ticks;
    bool current(std::string&){return true;}
    Outcome execute(const tc::Assigned& a,std::string&){commands.push_back(a);return Outcome::Confirmed;}
    Outcome advance(uint64_t tick,std::string&){ticks.push_back(tick);return Outcome::Confirmed;}
    bool consume(tc::Queue& queue,uint64_t tick,tc::Receipt& receipt,std::string& error){return queue.consume(tick,[&](std::string& e){return current(e);},[&](const tc::Assigned& a,std::string& e){return execute(a,e);},[&](uint64_t n,std::string& e){return advance(n,e);},receipt,error);}
};
#include "retained_completion_cases.inc"
int main(){try{
    retained_completion_cases::run();
    std::string error;tc::Assigned assigned;tc::Batch batch;tc::Receipt receipt;
    {tc::Queue host(config(),valid),client(config(2),valid);Native h,c;
     const auto a=action(2,1),pause=time(1,1),undo=action(1,1,Action::UndoBuilding);
     need(host.offer(2,a,assigned,error)==tc::Admission::Accepted&&assigned.order==1,error);
     need(host.offer(1,pause,assigned,error)==tc::Admission::Accepted&&assigned.order==2,error);
     need(host.offer(1,undo,assigned,error)==tc::Admission::Accepted&&assigned.order==3,error);
     need(host.offer(2,a,assigned,error)==tc::Admission::Duplicate&&assigned.order==1,"same request allocated another order");
     need(host.seal(batch,error)&&batch.commands.size()==3,error);
     need(client.admit(1,batch,error)==tc::Admission::Accepted,error);
     need(h.consume(host,1,receipt,error),error);need(c.consume(client,1,receipt,error),error);
     need(h.commands==c.commands&&h.commands[0].input==a&&h.commands[1].input==pause&&h.commands[2].input==undo,"time/gameplay order or Undo target lost");
     need(host.acknowledge(2,receipt,error)&&host.acknowledge(2,receipt,error),error);
     need(client.admit(1,batch,error)==tc::Admission::Duplicate,error);need(c.consume(client,1,receipt,error)&&c.ticks.size()==1&&c.commands.size()==3,"duplicate tick replayed native calls");
     need(host.seal(batch,error)&&batch.tick==2&&batch.previous_order==3&&batch.commands.empty(),"empty tick omitted or changed command order");
     need(client.admit(1,batch,error)==tc::Admission::Accepted,error);need(h.consume(host,2,receipt,error)&&c.consume(client,2,receipt,error),error);
     need(h.ticks.size()==2&&c.ticks.size()==2&&c.commands.size()==3,"empty boundary did not advance exactly once");
    }
    {tc::Queue host(config(),valid);auto a=action(2,1);need(host.offer(1,a,assigned,error)==tc::Admission::Rejected&&!host.state().locked,"untrusted request admitted");
     need(host.offer(2,action(2,2),assigned,error)==tc::Admission::Rejected&&!host.state().locked,"sequence gap admitted");
     need(host.offer(2,a,assigned,error)==tc::Admission::Accepted,error);std::get<Request>(a.command).building.x=3;
     need(host.offer(2,a,assigned,error)==tc::Admission::Rejected&&host.state().locked&&error.find("Conflicting")!=std::string::npos,"conflicting duplicate not locked");}
    {tc::Queue host(config(),valid),client(config(2),valid);Native native;
     need(host.seal(batch,error),error);auto wrong=batch;wrong.peer_mask=7;
     need(client.admit(1,wrong,error)==tc::Admission::Rejected,"wrong frozen membership admitted");need(client.admit(2,batch,error)==tc::Admission::Rejected,"non-Host batch admitted");
     need(client.admit(1,batch,error)==tc::Admission::Accepted,error);wrong=batch;wrong.previous_order=1;
     need(client.admit(1,wrong,error)==tc::Admission::Rejected&&client.state().locked,"conflicting batch not locked");need(!native.consume(client,1,receipt,error)&&native.ticks.empty(),"locked client ran tick");}
    {tc::Queue host(config(),valid),client(config(2),valid);Native native;
     need(host.offer(2,action(2,1),assigned,error)==tc::Admission::Accepted,error);need(host.seal(batch,error),error);
     auto wrong=batch;std::get<Request>(wrong.commands[0].input.command).snapshot_uuid="other";
     need(client.admit(1,wrong,error)==tc::Admission::Rejected&&client.state().assigned_order==0,"failed batch partly advanced sequence");
     need(client.admit(1,batch,error)==tc::Admission::Accepted,error);need(native.consume(client,1,receipt,error),error);}
    {tc::Queue host(config(),valid);Native native;int calls=0;
     need(host.offer(1,action(1,1),assigned,error)==tc::Admission::Accepted,error);need(host.offer(2,time(2,1),assigned,error)==tc::Admission::Accepted,error);need(host.seal(batch,error),error);
     auto execute=[&](const tc::Assigned&,std::string& cause){++calls;if(calls==1)return Outcome::Confirmed;cause="original uncertain effect";return Outcome::Unknown;};
     auto current=[](std::string&){return true;};auto tick=[&](uint64_t n,std::string& e){return native.advance(n,e);};
     need(!host.consume(1,current,execute,tick,receipt,error)&&calls==2&&receipt.completed_commands==1&&!receipt.tick_attempted&&receipt.outcome==Outcome::Unknown,"partial failure receipt wrong");
     need(error.find("original uncertain effect")!=std::string::npos&&host.state().locked,"original failure lost");
     need(!host.consume(1,current,execute,tick,receipt,error)&&calls==2&&native.ticks.empty(),"uncertain native calls replayed");need(!host.seal(batch,error),"failed queue advanced");}
    {tc::Queue host(config(),valid);int calls=0;need(host.seal(batch,error),error);
     need(!host.consume(1,[](std::string&){return true;},[](const tc::Assigned&,std::string&){return Outcome::Confirmed;},[&](uint64_t,std::string&)->Outcome{++calls;throw std::runtime_error("original tick throw");},receipt,error),"tick exception became success");
     need(receipt.tick_attempted&&!receipt.tick_completed&&receipt.outcome==Outcome::Unknown&&calls==1&&error.find("original tick throw")!=std::string::npos,"tick exception evidence differs");}
    {tc::Queue host(config(),valid);need(host.seal(batch,error),error);int calls=0;
     need(!host.consume(1,[](std::string& cause){cause="native owner changed";return false;},[&](const tc::Assigned&,std::string&){++calls;return Outcome::Confirmed;},[&](uint64_t,std::string&){++calls;return Outcome::Confirmed;},receipt,error)&&calls==0&&receipt.outcome==Outcome::RejectedNoEffect,"changed owner executed");}
    // 50ms delivery + 50ms acknowledgement latency at 64 boundaries per second.
    // Each peer still consumes one original tick per scheduling quantum after fill.
    {tc::Queue host(config(),valid),client(config(2),valid);Native h,c;size_t max_pending=0;
     struct Packet {double at;tc::Batch value;};struct Ack {double at;tc::Receipt value;};std::deque<Packet> packets;std::deque<Ack> acknowledgements;
     for(uint64_t quantum=0;quantum<72;++quantum){const double now=double(quantum)*1000.0/64.0;
        while(!acknowledgements.empty()&&acknowledgements.front().at<=now){need(host.acknowledge(2,acknowledgements.front().value,error),error);acknowledgements.pop_front();}
        if(quantum<64){if(quantum%8==0){const auto seq=quantum/8+1;need(host.offer(1,time(1,seq,Intent::Fast),assigned,error)==tc::Admission::Accepted,error);}
            need(host.seal(batch,error),error);packets.push_back({now+50,batch});need(h.consume(host,batch.tick,receipt,error),error);}
        while(!packets.empty()&&packets.front().at<=now){auto arriving=packets.front().value;packets.pop_front();need(client.admit(1,arriving,error)==tc::Admission::Accepted,error);need(c.consume(client,arriving.tick,receipt,error),error);acknowledgements.push_back({now+50,receipt});}
        max_pending=std::max(max_pending,host.state().in_flight);
     }
     need(h.ticks.size()==64&&c.ticks.size()==64&&h.commands==c.commands,"100ms RTT reduced tick rate or changed command ordering");
     need(max_pending>=7&&max_pending<=9&&host.state().in_flight==0,"ACKs became per-tick barrier or did not retire");
     std::cout<<"100ms RTT: 64 Host ticks and 64 Client ticks; peak "<<max_pending<<" in flight\n";
    }
    {tc::Queue host(config(),valid);Native native;std::vector<tc::Receipt> replies;
     for(size_t n=0;n<tc::Queue::in_flight_limit;++n){need(host.seal(batch,error),error);need(native.consume(host,batch.tick,receipt,error),error);replies.push_back(receipt);}
     need(!host.seal(batch,error)&&!host.state().locked,"bounded no-ACK backpressure failed");need(host.acknowledge(2,replies.front(),error),error);need(host.seal(batch,error),error);
     auto failed=replies[1];failed.outcome=Outcome::Unknown;failed.tick_completed=false;failed.cause="client native exception";
     need(!host.acknowledge(2,failed,error)&&host.state().locked&&error.find("client native exception")!=std::string::npos,"remote failure did not stop progress");}
    {tc::Queue host(config(),valid);Native native;need(host.seal(batch,error),error);need(host.seal(batch,error),error);
     need(!native.consume(host,2,receipt,error)&&native.ticks.empty(),"future admitted tick executed before earlier tick");need(native.consume(host,1,receipt,error)&&native.consume(host,2,receipt,error),error);}
    {tc::Queue host(config(),valid),client(config(2),valid);Native native;
     tc::Batch one,two;need(host.seal(one,error)&&host.seal(two,error),error);
     need(client.admit(1,one,error)==tc::Admission::Accepted&&client.admit(1,two,error)==tc::Admission::Accepted,error);
     need(!native.consume(client,2,receipt,error)&&native.ticks.empty(),"buffered future tick skipped earlier boundary");
     need(native.consume(client,1,receipt,error)&&native.consume(client,2,receipt,error),error);need(native.ticks==std::vector<uint64_t>{1,2},"buffered Client ticks not ordered");}
    {int validations=0;tc::Queue host(config(),[&](const tc::Input&,std::string& cause){++validations;cause="existing action preflight rejected";return false;});
     need(host.offer(1,action(1,1),assigned,error)==tc::Admission::Rejected&&validations==1&&host.state().assigned_order==0&&host.state().queued==0,"failed preflight assigned an action");need(error.find("existing action preflight rejected")!=std::string::npos,"preflight cause lost");}
    {tc::Queue host(config(),valid);int calls=0;need(host.offer(1,action(1,1),assigned,error)==tc::Admission::Accepted,error);need(host.offer(2,action(2,1),assigned,error)==tc::Admission::Accepted,error);need(host.seal(batch,error),error);
     need(!host.consume(1,[](std::string&){return true;},[&](const tc::Assigned&,std::string& cause){if(++calls==1)return Outcome::Confirmed;cause="second command rejected before its write";return Outcome::RejectedNoEffect;},[](uint64_t,std::string&){return Outcome::Confirmed;},receipt,error),"partial rejected batch became success");
     need(receipt.completed_commands==1&&receipt.outcome==Outcome::Unknown&&!receipt.tick_attempted,"partly mutated batch reported no effect");}
    {auto four=config();four.context.peer_mask=15;tc::Queue host(four,valid);Native native;
     need(host.seal(batch,error)&&native.consume(host,1,receipt,error),error);need(host.acknowledge(2,receipt,error)&&host.acknowledge(3,receipt,error),error);need(host.state().in_flight==1,"missing fourth peer receipt ignored");
     need(host.seal(batch,error)&&batch.tick==2,"four-player pending ACK introduced per-tick RTT barrier");need(host.acknowledge(4,receipt,error)&&host.state().in_flight==1,"fourth peer did not retire completed first tick");}
    std::cout<<checks<<" shared tick queue checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}}
