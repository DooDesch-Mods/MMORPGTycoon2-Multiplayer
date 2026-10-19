#include "test_dungeon_sound_fixture.h"
#include <iostream>
struct SessionPort:Session{Context value;explicit SessionPort(uint64_t peer):value{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot_id}{}
 bool current(Context& out,std::string&)override{out=value;return true;}};
struct EnginePort:Engine{
 DungeonSoundState row=before();Evidence receipt;unsigned executed=0,applied=0;int mode=0;
 bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
 bool current(const Binding&,const std::string&,std::string&)override{return true;}
 bool ledger(const Binding&,Ledger& out,std::string&)override{out=executed||applied?receipt.after:money();return true;}
 Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong family");}
 Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong family");}
 bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong family");}
 Outcome execute_action(const Binding&,const Request& r,std::string& e)override{
  need(r.action==Action::SetDungeonAtmosphere&&r.dungeon_sound==intent()&&r.canonical_record_key==intent().key,"wrong Dungeon sound intent");
  if(mode==1){e="existing Dungeon sound slot differs";return Outcome::RejectedNoEffect;}
  ++executed;receipt.before=money();receipt.after=money();receipt.dungeon_sound_before=row;row=after(row);receipt.dungeon_sound_after=row;return Outcome::Confirmed;
 }
 Outcome apply_action(const Binding&,const Request& r,const Evidence& host,std::string& e)override{
  need(r.dungeon_sound==intent()&&row==host.dungeon_sound_before,"Client prior Dungeon sound row differs");++applied;row=host.dungeon_sound_after;receipt=host;receipt.retirement_confirmed=false;
  if(mode==2){e="original target mutation failed";return Outcome::Unknown;}return Outcome::Confirmed;
 }
 bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
 bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)++receipt.dungeon_sound_after.atmosphere;return true;}
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport{uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
 bool claim(Handler,std::string&)override{return true;}
 bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}};
void flow(int mode){SessionPort hs(1),cs(2);EnginePort he,ce;he.mode=mode==1||mode==3?mode:0;ce.mode=mode==2?2:0;
 std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
 need(host.start(0,e)&&client.start(0,e),e);need(client.submit_dungeon_sound(action_id,intent(),0,e),e);
 need(!he.executed&&!ce.applied&&ce.row==before(),"Dungeon sound capture mutated before ordering");auto request_packet=packets.front();
 auto deliver=[&]{need(!packets.empty(),"missing Dungeon sound packet");auto p=packets.front();packets.pop_front();return(p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
 auto ok=deliver();if(mode==3){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&!ce.applied,"changed retirement row accepted");return;}
 need(ok,e);if(mode==1){need(deliver(),e);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("existing Dungeon sound slot differs")!=std::string::npos,"rejected Dungeon sound lost cause or mutated");return;}
 need(he.executed==1&&!ce.applied,"Dungeon sound Host did not precede Client");auto result_packet=packets.front();ok=deliver();
 if(mode==2){need(!ok&&client.status().locked&&ce.applied==1,"partial Dungeon sound escaped Unknown");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"failed Client ACK confirmed");return;}
 need(ok&&ce.applied==1&&ce.row==he.row&&!client.status().confirmed,e);
 Message ack;need(decode(packets.front().bytes.data(),packets.front().bytes.size(),ack,e),e);
 need(ack.evidence.dungeon_sound_before==before()&&ack.evidence.dungeon_sound_after==after(before()),"Dungeon sound full row not retained on ACK");
 if(mode==4){++ack.evidence.dungeon_sound_after.dungeon.global_random[0];need(encode(ack,packets.front().bytes,e),e);need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"changed Host RNG ACK accepted");return;}
 need(deliver()&&deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1,"Dungeon sound full peer ACK missing");
 need(host.receive(request_packet.sender,request_packet.bytes.data(),request_packet.bytes.size(),3,e),e);
 need(client.receive(result_packet.sender,result_packet.bytes.data(),result_packet.bytes.size(),3,e),e);
 need(he.executed==1&&ce.applied==1&&!client.submit_dungeon_sound(action_id,intent(),4,e),"Dungeon sound duplicate replayed original");
}
void host_flow(){SessionPort hs(1),cs(2);EnginePort he,ce;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
 Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;need(host.start(0,e)&&client.start(0,e),e);
 need(host.submit_dungeon_sound(action_id,intent(),0,e)&&he.executed==1&&!ce.applied,"Host-origin list ordering");
 auto deliver=[&]{need(!packets.empty(),"missing Host-origin packet");auto p=packets.front();packets.pop_front();return(p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
 need(deliver()&&deliver()&&deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1&&he.executed==1&&ce.applied==1,"Host-origin ACK3 original once");
 need(!host.submit_dungeon_sound(action_id,intent(),4,e)&&he.executed==1&&ce.applied==1,"Host-origin duplicate replay");
}
int main(){try{for(int32_t row=0;row<6;++row){selected_atmosphere=row;for(bool equal:{false,true})for(bool sandbox_mode:{false,true})for(int32_t count:{1,3}){equal_apply=equal;sandbox=sandbox_mode;previous_count=count;for(int mode=0;mode<5;++mode)flow(mode);host_flow();}}std::cout<<"288 actual Dungeon sound Apply Controller ordering/ACK/no-replay cases passed\n";return 0;}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
