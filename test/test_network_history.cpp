#include "typed_action/action_session.h"
#include "network_history_graph.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace identity=mt2mp::game::network_object_identity;
namespace topology=mt2mp::game::network_history_graph;
namespace {
int checks=0;void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
const std::string snapshot="11111111-1111-4111-8111-111111111111",token="22222222-2222-4222-8222-222222222222",source="33333333-3333-4333-8333-333333333333";
Evidence result(uint8_t kind=8,bool undo=true,bool junction=false){Evidence out;out.before.record_present=1;out.before.cash=out.before.record_cash=1000;out.after=out.before;
    const bool destroying=undo!=(kind==129);const int32_t amount=destroying?100:-100;
    out.after.cash+=amount;out.after.record_cash+=amount;out.after.record_income=amount;out.after.line_counts[6]=1;out.after.line_income[6]=amount;
    NetworkRemoval large{{3,2,5,std::string(64,'a')},std::string(64,'c')},small{{3,2,5,std::string(64,'b')},std::string(64,'d')};
    if(kind==8)--small.graph.nodes;else if(kind==9){--small.graph.pipes;if(junction)--small.graph.nodes;}else{--small.graph.nodes;--small.graph.pipes;small.graph.connections-=2;}
    out.network_removal_before=destroying?large:small;out.network_removal_after=destroying?small:large;out.network_history=NetworkHistoryProof{kind,100,junction};out.undo_confirmed=true;return out;
}
struct S:Session{Context context;explicit S(uint64_t peer):context{17,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=context;return true;}};
struct E:Engine{Evidence receipt;Ledger cash=result().before;int calls=0,mode=0;uint8_t source_kind=8;
    bool bind(const std::string&,Binding& out,std::string&)override{out={2,8};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong Remove dispatch");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong Remove apply");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong Remove observe");}
    Outcome execute_action(const Binding&,const Request& request,std::string&)override{need(network_history(request),"wrong original history dispatch");++calls;receipt=result(source_kind);cash=receipt.after;return Outcome::Confirmed;}
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& error)override{++calls;receipt=host;receipt.retirement_confirmed=false;cash=receipt.after;
        if(mode==1){error="fixture original whole graph interrupted";return Outcome::Unknown;}if(mode==2)receipt.network_removal_after->identities_sha256=std::string(64,'e');return Outcome::Confirmed;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)receipt.network_history->amount++;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct T:Transport{uint64_t peer;std::deque<Packet>& queue;T(uint64_t id,std::deque<Packet>& packets):peer(id),queue(packets){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{queue.push_back({peer,to,bytes});return true;}};
void flow(int mode,uint8_t kind=8){S hs(1),cs(2);E he,ce;he.mode=mode==3?3:0;he.source_kind=kind;ce.mode=mode;std::deque<Packet> queue;T hw(1,queue),cw(2,queue);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
    need(host.start(0,error)&&client.start(0,error),error);need(client.submit_building_history(Action::UndoBuilding,token,source,"network:"+source,0,error),error);need(!he.calls&&!ce.calls,"UI intent ran the original before Host ordering");
    const auto request=queue.front();const auto deliver=[&](){need(!queue.empty(),"missing packet");const auto packet=queue.front();queue.pop_front();return(packet.to==1?host:client).receive(packet.from,packet.bytes.data(),packet.bytes.size(),1,error);};
    const bool accepted=deliver();if(mode==3){need(host.status().locked&&!host.status().confirmed,"changed canonical retirement proof accepted");return;}need(accepted,error);
    const auto host_result=queue.front();const bool applied=deliver();if(mode==1||mode==2){need(!applied&&client.status().locked&&!client.status().confirmed,"partial or changed full graph received ACK");need(deliver(),error);need(host.status().locked&&!host.status().confirmed,"failed original became completed history");return;}
    need(deliver()&&deliver(),error);need(host.status().confirmed==1&&client.status().confirmed==1&&host.status().ack_mask==3,"full canonical retirement ACK missing");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,error)&&client.receive(host_result.from,host_result.bytes.data(),host_result.bytes.size(),2,error),error);
    need(he.calls==1&&ce.calls==1,"duplicate repeated the original whole DoGraph");need(!client.submit_building_history(Action::UndoBuilding,token,source,"network:"+source,3,error),"retired history token replayed");
}
void codec(){std::string error;std::vector<uint8_t> bytes;Message round;
    for(uint8_t kind:{uint8_t(8),uint8_t(9),uint8_t(129)})for(bool undo:{false,true})for(bool junction:{false,true}){if(junction&&kind!=9)continue;
        Message message;message.session=17;message.request.action=undo?Action::UndoBuilding:Action::RedoBuilding;message.request.peer=2;message.request.sequence=1;message.request.snapshot_uuid=snapshot;message.request.action_uuid=token;message.request.target_action_uuid=source;message.request.canonical_record_key="network:"+source;
        need(encode(message,bytes,error)&&decode(bytes.data(),bytes.size(),round,error)&&round==message,error);need(nlohmann::json::parse(bytes)["schema"]==35,"Network history needs existing schema35");
        message.kind=Kind::HostResult;message.revision=1;message.required_mask=3;message.outcome=Outcome::Confirmed;message.evidence=result(kind,undo,junction);message.evidence.retirement_confirmed=true;
        need(encode(message,bytes,error)&&decode(bytes.data(),bytes.size(),round,error)&&round==message,error);
        auto invalid=nlohmann::json::parse(bytes);invalid["evidence"]["network_history"]["source"]=true;auto text=invalid.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),round,error),"Boolean source kind accepted");
        invalid=nlohmann::json::parse(bytes);invalid["evidence"]["network_history"]["extra"]=0;text=invalid.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),round,error),"unknown native source proof accepted");
        ++message.evidence.after.line_counts[19];need(!encode(message,bytes,error),"unrelated full Ledger category accepted");
        message.evidence=result(kind,undo,junction);message.evidence.network_history->source=1;need(!encode(message,bytes,error),"foreign original source family accepted");
    }
}
void registry(){identity::Registry registry;identity::Scope scope{snapshot,2,8,0x30000};const auto id=identity::birth(source,identity::Kind::Node);
    identity::Entry original{identity::Kind::Node,0x40000,0x50000,0x60000,77,9,false};std::string error;need(registry.seal(scope,{{id,original}},{},error),error);
    auto restored=original;restored.pointer=0x70000;need(!registry.restore_history(scope,id,restored,error),"live key restored without retired original");
    need(registry.retire(scope,id,original.pointer,original.uid,error),error);auto foreign=restored;foreign.uid++;need(!registry.restore_history(scope,id,foreign,error),"foreign replacement UID accepted");
    foreign=restored;foreign.manager++;need(!registry.restore_history(scope,id,foreign,error),"foreign restored native owner accepted");
    need(registry.restore_history(scope,id,restored,error),error);identity::Entry actual;need(registry.lookup(scope,id,actual,error)&&actual==restored,error);
    need(!registry.restore_history(scope,id,restored,error),"original restoration repeated");
}
void graph(){using json=nlohmann::json;namespace native=mt2mp::game::connected_network_native;
    native::Graph value;native::Node a,b;a.object=0x100000;a.uid=70001;b.object=0x200000;b.uid=70002;
    native::Pipe pipe;pipe.object=0x300000;pipe.uid=70003;value.nodes={a,b};value.pipe_values={pipe};
    value.node_keys={{a.object,json::array({9,0})},{b.object,json::array({9,1})}};value.pipe_keys={{pipe.object,json::array({9,0})}};
    const auto node=[&](int position){return json::array({"Junction","item_junction",position,json::array({1,2}),0,json::array({0,0,0,0}),json::array({false,false,false}),json::array({json::array({9,0})}),json::array()});};
    const auto cable=json::array({"Cable","item_cable",100,json::array({1,2}),false,json::array({9,0}),json::array({9,1}),json::array({0,0,0}),json::array({1,0,0}),json::array(),json::array(),1,2,json::array()});
    value.rows=json::array({json::array({9,true,json::array({node(1),node(2)}),json::array({cable}),json::array({json::array({9,0})})})});
    const std::map<uint64_t,std::string> ids{{a.uid,identity::saved(std::string(64,'a'),identity::Kind::Node,1)},{b.uid,identity::saved(std::string(64,'a'),identity::Kind::Node,2)},{pipe.uid,identity::birth(source,identity::Kind::Pipe)}};
    const auto key=[&](uintptr_t,uint64_t uid,std::string& out,std::string& error){const auto found=ids.find(uid);if(found==ids.end()){error="fixture foreign local full UID";return false;}out=found->second;return true;};
    json original,reordered;std::string error;need(topology::canonical(value,key,original,error),error);
    auto swapped=value;swapped.nodes={b,a};swapped.node_keys[a.object]=json::array({9,1});swapped.node_keys[b.object]=json::array({9,0});
    swapped.rows[0][2]=json::array({node(2),node(1)});swapped.rows[0][3][0][5]=json::array({9,1});swapped.rows[0][3][0][6]=json::array({9,0});
    need(topology::canonical(swapped,key,reordered,error)&&reordered==original,"native manager reordering changed the canonical complete graph");
    need(original.dump().find("70001")==std::string::npos&&original.dump().find("1048576")==std::string::npos,"private native UID/pointer entered canonical topology");
    swapped=value;swapped.rows[0][2][0][5][0]=50;need(topology::canonical(swapped,key,reordered,error)&&reordered!=original&&topology::shape(reordered)==topology::shape(original),"current simulation values were lost from full digest or treated as original geometry");
    swapped=value;swapped.rows[0][2][0][2]=99;need(topology::canonical(swapped,key,reordered,error)&&topology::shape(reordered)!=topology::shape(original),"changed native geometry accepted as original source");
    swapped=value;swapped.rows[0][3][0][5]=json::array({9,99});need(!topology::canonical(swapped,key,reordered,error),"foreign reciprocal endpoint accepted");
    swapped=value;swapped.nodes[0].uid=999;need(!topology::canonical(swapped,key,reordered,error),"foreign original local UID accepted");
}
}
int main()try{codec();registry();graph();for(int mode=0;mode<4;++mode)flow(mode);flow(0,9);flow(0,129);std::cout<<checks<<" Network original history codec/Controller/canonical retirement assertions passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
