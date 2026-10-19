#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace {
int checks=0;void need(bool ok,const std::string& error){++checks;if(!ok)throw std::runtime_error(error);}
const std::string snapshot="11111111-1111-4111-8111-111111111111",token="22222222-2222-4222-8222-222222222222";
const std::string target=mt2mp::game::network_object_identity::birth(token,mt2mp::game::network_object_identity::Kind::Node);
NetworkRemoveIntent intent(){return {63,std::string(64,'a'),100};}
Evidence result(){Evidence e;e.before.record_present=1;e.before.cash=e.before.record_cash=1000;e.after=e.before;
    e.after.cash=e.after.record_cash=1100;e.after.record_income=100;e.after.line_counts[6]=1;e.after.line_income[6]=100;
    e.network_removal_before=NetworkRemoval{{3,2,5,std::string(64,'a')},std::string(64,'c')};
    e.network_removal_after=NetworkRemoval{{2,1,2,std::string(64,'b')},std::string(64,'d')};e.target_retired=e.undo_confirmed=e.selection_confirmed=true;return e;}
struct S:Session {Context context;S(uint64_t peer):context{17,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=context;return true;}};
struct E:Engine {Evidence receipt;Ledger cash=result().before;int calls=0;int mode=0;
    bool bind(const std::string&,Binding& out,std::string&)override{out={2,8};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong old Remove dispatch");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong old Remove apply");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong old Remove observe");}
    Outcome execute_action(const Binding&,const Request& r,std::string&)override{need(r.action==Action::RemoveNetworkObject,"wrong new action");++calls;receipt=result();cash=receipt.after;return Outcome::Confirmed;}
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& error)override{++calls;receipt=host;receipt.retirement_confirmed=false;cash=receipt.after;
        if(mode==1){error="original interrupted";return Outcome::Unknown;}if(mode==2)receipt.network_removal_after->identities_sha256=std::string(64,'e');return Outcome::Confirmed;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)receipt.after.line_counts[19]++;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct T:Transport{uint64_t peer;std::deque<Packet>& queue;T(uint64_t id,std::deque<Packet>& q):peer(id),queue(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{queue.push_back({peer,to,bytes});return true;}};
void flow(int mode){S hs(1),cs(2);E he,ce;he.mode=mode==3?3:0;ce.mode=mode;std::deque<Packet> queue;T hw(1,queue),cw(2,queue);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    need(host.start(0,error)&&client.start(0,error),error);need(client.submit_network_remove(token,target,intent(),0,error),error);need(!he.calls&&!ce.calls,"capture executed before Host ordering");
    const auto request=queue.front();auto deliver=[&](){need(!queue.empty(),"missing packet");const auto p=queue.front();queue.pop_front();return(p.to==1?host:client).receive(p.from,p.bytes.data(),p.bytes.size(),1,error);};
    const bool accepted=deliver();if(mode==3){need(host.status().locked&&!host.status().confirmed,"changed retirement accepted");return;}need(accepted,error);
    const auto host_result=queue.front();const bool applied=deliver();if(mode==1||mode==2){need(!applied&&client.status().locked&&!client.status().confirmed,"unknown or changed graph ACKed");need(deliver(),error);need(host.status().locked&&!host.status().confirmed,"failed application became full ACK");return;}
    need(deliver()&&deliver(),error);need(host.status().confirmed==1&&client.status().confirmed==1&&host.status().ack_mask==3,"full retirement ACK missing");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,error)&&client.receive(host_result.from,host_result.bytes.data(),host_result.bytes.size(),2,error),error);
    need(he.calls==1&&ce.calls==1,"duplicate repeated native removal");need(!client.submit_network_remove(token,target,intent(),3,error),"retired token replayed");}
void codec(){Message m;m.session=17;m.request.action=Action::RemoveNetworkObject;m.request.peer=2;m.request.sequence=1;m.request.snapshot_uuid=snapshot;m.request.action_uuid=token;m.request.canonical_record_key=target;m.request.network_remove=intent();
    std::vector<uint8_t> bytes;Message round;std::string error;need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),round,error)&&round==m,error);
    auto j=nlohmann::json::parse(bytes);need(j["schema"]==35,"incoherent schema");j["request"]["network_remove"]["quoted_refund"]=-1;auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),round,error),"negative quote accepted");
    j=nlohmann::json::parse(bytes);j["request"]["network_remove"]["extra"]=0;text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),round,error),"extra request fields accepted");
    m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence=result();m.evidence.retirement_confirmed=true;
    need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),round,error)&&round==m,error);
    ++m.evidence.after.line_counts[19];need(!encode(m,bytes,error),"unrelated full Ledger category accepted");m.evidence=result();m.evidence.retirement_confirmed=true;m.evidence.network_removal_after->graph.nodes=4;
    need(!encode(m,bytes,error),"new native node during removal accepted");
}
}
int main()try{codec();for(int mode=0;mode<4;++mode)flow(mode);std::cout<<checks<<" Network Remove codec/Controller assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
