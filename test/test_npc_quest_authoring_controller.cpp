#include "npc_quest_authoring_fixture.h"
#include <iostream>
struct SessionPort:Session{Context value;explicit SessionPort(uint64_t peer):value{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot_id}{}
 bool current(Context& out,std::string&)override{out=value;return true;}};
struct EnginePort:Engine{
 NpcQuestState row=before();Evidence receipt;unsigned executed=0,applied=0;int mode=0;
 bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
 bool current(const Binding&,const std::string&,std::string&)override{return true;}
 bool ledger(const Binding&,Ledger& out,std::string&)override{out={};return true;}
 Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong family");}
 Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong family");}
 bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong family");}
 Outcome execute_action(const Binding&,const Request& r,std::string& e)override{
  need(r.action==Action::SetNpcQuestDestination&&r.npc_quest==intent()&&r.canonical_record_key==owner_key,"wrong Quest intent");
  if(mode==1){e="existing Quest slot differs";return Outcome::RejectedNoEffect;}
  ++executed;receipt.quest_before=row;row=after(row);receipt.quest_after=row;receipt.undo_confirmed=true;return Outcome::Confirmed;
 }
 Outcome apply_action(const Binding&,const Request& r,const Evidence& host,std::string& e)override{
  need(r.npc_quest==intent()&&row==host.quest_before,"Client prior Quest row differs");++applied;row=host.quest_after;receipt=host;receipt.retirement_confirmed=false;
  if(mode==2){e="original target mutation failed";return Outcome::Unknown;}return Outcome::Confirmed;
 }
 bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
 bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)++receipt.quest_after.record.xp_reward;return true;}
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport{uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
 bool claim(Handler,std::string&)override{return true;}
 bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}};
void flow(int mode){SessionPort hs(1),cs(2);EnginePort he,ce;he.mode=mode==1||mode==3?mode:0;ce.mode=mode==2?2:0;
 std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
 need(host.start(0,e)&&client.start(0,e),e);need(client.submit_npc_quest(action_id,owner_key,intent(),0,e),e);
 need(!he.executed&&!ce.applied&&ce.row==before(),"Quest capture mutated before ordering");auto request_packet=packets.front();
 auto deliver=[&]{need(!packets.empty(),"missing Quest packet");auto p=packets.front();packets.pop_front();return(p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
 auto ok=deliver();if(mode==3){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&!ce.applied,"changed retirement row accepted");return;}
 need(ok,e);if(mode==1){need(deliver(),e);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("existing Quest slot differs")!=std::string::npos,"rejected Quest lost cause or mutated");return;}
 need(he.executed==1&&!ce.applied,"Quest Host did not precede Client");auto result_packet=packets.front();ok=deliver();
 if(mode==2){need(!ok&&client.status().locked&&ce.applied==1,"partial Quest escaped Unknown");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"failed Client ACK confirmed");return;}
 need(ok&&ce.applied==1&&ce.row==he.row&&!client.status().confirmed,e);
 Message ack;need(decode(packets.front().bytes.data(),packets.front().bytes.size(),ack,e),e);
 need(ack.evidence.quest_before==before()&&ack.evidence.quest_after==after(before()),"Quest full row not retained on ACK");
 if(mode==4){++ack.evidence.quest_after.global_random[0];need(encode(ack,packets.front().bytes,e),e);need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"changed Host RNG ACK accepted");return;}
 need(deliver()&&deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1,"Quest full peer ACK missing");
 need(host.receive(request_packet.sender,request_packet.bytes.data(),request_packet.bytes.size(),3,e),e);
 need(client.receive(result_packet.sender,result_packet.bytes.data(),result_packet.bytes.size(),3,e),e);
 need(he.executed==1&&ce.applied==1&&!client.submit_npc_quest(action_id,owner_key,intent(),4,e),"Quest duplicate replayed original");
}
int main(){try{for(int mode=0;mode<5;++mode)flow(mode);std::cout<<"5 actual Quest Controller ordering/ACK/no-replay cases passed\n";return 0;}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
