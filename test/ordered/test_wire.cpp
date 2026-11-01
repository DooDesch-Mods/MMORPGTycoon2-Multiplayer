#include "tick_wire.h"
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace mt2mp::action_session;
namespace tc=mt2mp::action_session::tick_commands;
namespace wire=tc::wire;
namespace ts=mt2mp::game::time_session;
unsigned checks=0;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
const std::string world="11111111-1111-4111-8111-111111111111";
std::string id(uint64_t sequence){std::ostringstream s;s<<"22222222-2222-4222-8222-"<<std::hex<<std::setfill('0')<<std::setw(12)<<sequence;return s.str();}
Context context(bool host=true){return {91,host?1u:2u,1,3,host?Role::Host:Role::Client,world};}
tc::Input action(uint64_t peer=2,uint64_t sequence=1){Request r;r.peer=peer;r.sequence=sequence;r.action=Action::StartStartingpoint;r.action_uuid=id(sequence);r.snapshot_uuid=world;r.canonical_record_key="tech:startingpoint";return {91,world,r};}
tc::Input time(uint64_t peer=2,uint64_t sequence=1){return {91,world,tc::TimeRequest{peer,sequence,0,ts::Intent::Fast}};}
tc::Input large(uint64_t sequence,size_t text=34000){
    auto in=action(2,sequence);auto& r=std::get<Request>(in.command);r.action=Action::SetLogo;r.canonical_record_key="software:logo";
    using J=nlohmann::json;auto token=[](int type,const std::string& value){return J::array({type,uint64_t(0),value});};
    J tokens=J::array();while(text){const auto n=std::min(text,size_t(7000));tokens.push_back(token(2,std::string(n,'x')));text-=n;}
    r.logo={std::string(64,'a'),std::string(64,'b'),J::array({J::array({int64_t(-1),uint64_t(0),token(1,"mmoLogo"),tokens})})};return in;
}
bool valid(const tc::Input&,std::string&){return true;}
struct Local:Session {Context value=context();bool okay=true;bool current(Context& out,std::string& cause)override{if(!okay){cause="loaded ACK missing";return false;}out=value;return true;}};
struct Link:Transport {int calls=0;bool okay=true;uint64_t target=0;std::vector<uint8_t> bytes;
    bool claim(Handler,std::string&)override{throw std::runtime_error("Wire must not claim a second handler");}
    bool send(uint64_t peer,const std::vector<uint8_t>& data,std::string& cause)override{++calls;target=peer;bytes=data;if(!okay){cause="retained transport failure";return false;}return true;}};
int main()try{
    std::string error;std::vector<uint8_t> bytes;wire::Packet decoded;tc::Assigned assigned;tc::Batch batch;
    const auto roundtrip=[&](const wire::Packet& p){need(wire::encode(p,bytes,error),error);need(wire::decode(bytes.data(),bytes.size(),decoded,error),error);need(decoded==p,"packet changed in roundtrip");};
    roundtrip({3,action()});const auto gameplay=bytes;
    auto embedded=std::make_unique<Message>();embedded->session=91;embedded->request=std::get<Request>(action().command);std::vector<uint8_t> existing;
    need(encode(*embedded,existing,error),error);need(std::vector<uint8_t>(gameplay.begin()+57,gameplay.end())==existing,"Gameplay body differs from unchanged production Request codec");
    roundtrip({3,time()});const auto time_bytes=bytes;
    ts::Message original;original.epoch=91;original.peer=2;original.sequence=1;original.intent=ts::Intent::Fast;std::array<uint8_t,ts::wire_size> original_bytes{};
    need(ts::encode(original,original_bytes,error),error);need(std::equal(bytes.begin()+57,bytes.end(),original_bytes.begin(),original_bytes.end()),"Time body differs from unchanged production Request codec");
    {auto request=action();auto& r=std::get<Request>(request.command);r.action=Action::UndoBuilding;r.canonical_record_key="action:"+id(99);r.target_action_uuid=id(99);roundtrip({3,request});}
    {auto request=action();std::get<Request>(request.command).action=Action(255);need(!wire::encode({3,request},bytes,error)&&bytes.empty(),"Unknown action bypassed existing validator");}
    {auto request=action();std::get<Request>(request.command).snapshot_uuid=id(7);need(!wire::encode({3,request},bytes,error),"Nested gameplay snapshot bypassed");}
    {auto request=time();std::get<tc::TimeRequest>(request.command).intent=ts::Intent(255);need(!wire::encode({3,request},bytes,error),"Unknown time intent bypassed existing validator");}
    roundtrip({3,tc::Batch{91,1,0,3,world,{{1,action()},{2,time()}}}});
    tc::Receipt receipt{91,1,2,world,2,Outcome::Confirmed,true,true,{}};roundtrip({3,receipt});
    receipt.outcome=Outcome::Unknown;receipt.tick_completed=false;receipt.cause="original callback timed out - do not replay";roundtrip({3,receipt});
    receipt.outcome=Outcome::RejectedNoEffect;receipt.completed_commands=0;receipt.tick_attempted=false;roundtrip({3,receipt});
    receipt.outcome=Outcome::Pending;need(!wire::encode({3,receipt},bytes,error),"Pending completion accepted");
    receipt.outcome=Outcome::Unknown;receipt.cause=std::string(1025,'x');need(!wire::encode({3,receipt},bytes,error),"Oversize failure cause truncated or admitted");
    // Every truncation fails without replacing an already valid decoded packet.
    roundtrip({3,time()});const auto retained=decoded;
    for(size_t n=0;n<time_bytes.size();++n)need(!wire::decode(time_bytes.data(),n,decoded,error)&&decoded==retained,"Truncation admitted or mutated output");
    auto bad=time_bytes;bad.push_back(0);need(!wire::decode(bad.data(),bad.size(),decoded,error),"Trailing byte accepted");
    for(const size_t offset:{size_t(4),size_t(5),size_t(6),size_t(7),size_t(52)}){bad=time_bytes;bad[offset]=255;need(!wire::decode(bad.data(),bad.size(),decoded,error),"Bad envelope byte accepted");}
    bad=time_bytes;bad[53]=255;bad[54]=255;need(!wire::decode(bad.data(),bad.size(),decoded,error),"Declared large body length accepted");
    bad=gameplay;bad[57]='[';need(!wire::decode(bad.data(),bad.size(),decoded,error),"Malformed embedded gameplay accepted");
    bad=time_bytes;bad[57+8]=92;need(!wire::decode(bad.data(),bad.size(),decoded,error),"Embedded time epoch differs");
    bad=time_bytes;bad[57+16]=3;need(!wire::decode(bad.data(),bad.size(),decoded,error),"Embedded peer outside mask accepted");
    {tc::Queue host({context()},valid),client({context(false)},valid);Local hs,cs;cs.value=context(false);Link link;
     need(wire::receive(host,hs,context(),2,gameplay.data(),gameplay.size(),assigned,error)==tc::Admission::Accepted,error);
     need(assigned.order==1&&host.state().queued==1,"Remote request not assigned once");
     need(wire::receive(host,hs,context(),2,gameplay.data(),gameplay.size(),assigned,error)==tc::Admission::Duplicate,error);
     need(wire::receive(host,hs,context(),2,time_bytes.data(),time_bytes.size(),assigned,error)==tc::Admission::Accepted&&assigned.order==2,error);
     need(wire::seal(host,batch,bytes,error)&&batch.commands.size()==2,error);const auto packet=bytes;
     need(wire::send(link,hs,context(),0,packet,error)&&link.calls==1&&link.bytes==packet,"Existing broadcast transport not used");
     need(wire::receive(client,cs,context(false),1,packet.data(),packet.size(),assigned,error)==tc::Admission::Accepted,error);
     need(wire::receive(client,cs,context(false),1,packet.data(),packet.size(),assigned,error)==tc::Admission::Duplicate,error);
     std::vector<uint64_t> orders;int ticks=0;
     const auto consume=[&](tc::Queue& q,tc::Receipt& out){return q.consume(1,[](std::string&){return true;},[&](const tc::Assigned& a,std::string&){orders.push_back(a.order);return Outcome::Confirmed;},[&](uint64_t,std::string&){++ticks;return Outcome::Confirmed;},out,error);};
     tc::Receipt host_receipt,client_receipt;need(consume(host,host_receipt)&&consume(client,client_receipt),error);
     need((orders==std::vector<uint64_t>{1,2,1,2})&&ticks==2,"Shared Request/Time order or native call count differs");
     need(consume(client,client_receipt)&&ticks==2&&orders.size()==4,"Completed duplicate replayed native effects");
     need(wire::encode({3,client_receipt},bytes,error),error);
     need(wire::send(link,cs,context(false),1,bytes,error),error);
     need(wire::receive(host,hs,context(),2,bytes.data(),bytes.size(),assigned,error)==tc::Admission::Accepted&&host.state().in_flight==0,error);
     need(wire::receive(host,hs,context(),2,bytes.data(),bytes.size(),assigned,error)==tc::Admission::Accepted,error);
     need(!wire::send(link,hs,context(),1,packet,error),"Host batch sent to itself");
     need(!wire::send(link,cs,context(false),1,packet,error),"Client forged Host batch send");
     need(wire::receive(host,hs,context(),2,packet.data(),packet.size(),assigned,error)==tc::Admission::Rejected,"Host accepted remote batch");
     need(wire::receive(client,cs,context(false),3,packet.data(),packet.size(),assigned,error)==tc::Admission::Rejected,"Unmapped peer admitted");
     hs.value.session=92;need(wire::receive(host,hs,context(),2,gameplay.data(),gameplay.size(),assigned,error)==tc::Admission::Rejected,"Retired session admitted");hs.value=context();
     hs.okay=false;need(!wire::send(link,hs,context(),0,packet,error)&&error=="loaded ACK missing","Unloaded snapshot sent or lost cause");hs.okay=true;
     link.okay=false;const int calls=link.calls;need(!wire::send(link,hs,context(),0,packet,error)&&link.calls==calls+1&&error=="retained transport failure"&&packet==link.bytes,"Send failure retried or changed retained packet");
    }
    {tc::Queue host({context()},valid);uint64_t accepted=0;
     for(uint64_t i=1;i<=tc::Queue::batch_limit;++i){const auto admission=wire::offer(host,2,large(i),assigned,error);if(admission==tc::Admission::Rejected)break;need(admission==tc::Admission::Accepted,error);++accepted;}
     need(accepted>0&&accepted<tc::Queue::batch_limit,"Fixture did not reach real byte bound before command bound");
     need(host.state().queued==accepted&&host.state().assigned_order==accepted&&error.find("byte budget")!=std::string::npos,"Byte backpressure committed rejected input");
     need(wire::offer(host,2,large(accepted),assigned,error)==tc::Admission::Duplicate,"Duplicate used full packet capacity");
     const auto next=large(accepted+1);need(wire::seal(host,batch,bytes,error)&&batch.commands.size()==accepted&&bytes.size()<=max_transport_payload,error);
     need(wire::offer(host,2,next,assigned,error)==tc::Admission::Accepted&&assigned.order==accepted+1,"Unassigned input could not retry after prefix seal");
     need(wire::seal(host,batch,bytes,error)&&batch.tick==2&&batch.previous_order==accepted&&batch.commands.size()==1,"Split batch changed ordering");
     std::cout<<"Actual Request bytes="<<existing.size()<<", first batch commands="<<accepted<<'\n';
    }
    {tc::Queue host({context()},valid);for(uint64_t i=1;i<=2;++i)need(host.offer(2,large(i),assigned,error)==tc::Admission::Accepted,error);
     const auto state=host.state();need(!wire::seal(host,batch,bytes,error)&&bytes.empty()&&host.state().queued==state.queued&&host.state().next_assignment_tick==state.next_assignment_tick&&host.state().in_flight==0,"Oversize bypass admission sealed an unsendable batch");}
    {tc::Batch exact{91,1,0,3,world,{{1,large(1,30000)},{2,large(2,30000)}}};
     need(wire::encode({3,exact},bytes,error),error);const auto room=max_transport_payload-bytes.size();
     exact.commands[1].input=large(2,30000+room);
     // Growing through a token boundary adds one token's JSON envelope as well.
     while(!wire::encode({3,exact},bytes,error)){auto& text=std::get<Request>(exact.commands[1].input.command).logo.records[0][3].back()[2].get_ref<std::string&>();need(!text.empty(),"Boundary fixture exhausted text");text.pop_back();}
     auto& text=std::get<Request>(exact.commands[1].input.command).logo.records[0][3].back()[2].get_ref<std::string&>();
     need(bytes.size()==max_transport_payload,"Exact wire boundary fixture differs");need(wire::decode(bytes.data(),bytes.size(),decoded,error),error);
     text.push_back('x');need(!wire::encode({3,exact},bytes,error)&&bytes.empty(),"65537-byte batch accepted");
     auto big=large(1,57000);auto message=std::make_unique<Message>();message->session=91;message->request=std::get<Request>(big.command);
     need(encode(*message,existing,error),error);need(existing.size()<max_payload,"Near-maximum Request fixture too large");
     auto& tail=message->request.logo.records[0][3].back()[2].get_ref<std::string&>();tail.append(max_payload-existing.size(),'x');
     need(encode(*message,existing,error)&&existing.size()==max_payload,"60000-byte existing Request not preserved");
     big.command=message->request;roundtrip({3,big});
     tail.push_back('x');need(!encode(*message,existing,error),"Existing 60001-byte Request guard bypassed");}
    {tc::Queue host({context()},valid);int calls=0;const auto reject=[&](const tc::Batch& b,std::string& cause){++calls;need(b.tick==1&&b.previous_order==0&&b.peer_mask==3&&b.commands.back().order==1,"Capacity lacked final batch identity");cause="capacity full";return false;};
     need(host.offer(2,time(),assigned,error,reject)==tc::Admission::Rejected&&calls==1&&host.state().queued==0&&host.state().assigned_order==0&&!host.state().locked,"Capacity rejection mutated assignment");
     need(host.offer(2,time(),assigned,error)==tc::Admission::Accepted,error);
     need(host.offer(2,time(),assigned,error,reject)==tc::Admission::Duplicate&&calls==1,"Duplicate invoked capacity");
     need(!host.seal(batch,error,[](const tc::Batch&,std::string&)->bool{throw std::runtime_error("capacity exception retained");})&&host.state().locked&&host.state().in_flight==0&&error.find("capacity exception retained")!=std::string::npos,"Capacity exception ignored or sealed");}
    {tc::Queue host({context()},valid);const auto reenter=[&](const tc::Batch&,std::string& cause){tc::Batch other;need(!host.seal(other,cause),"Reentrant seal succeeded");return true;};
     need(host.offer(2,time(),assigned,error,reenter)==tc::Admission::Rejected&&host.state().locked&&host.state().queued==0,"Reentrant capacity callback committed");}
    std::cout<<checks<<" actual production queue/codec/wire checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<'\n';return 1;}
