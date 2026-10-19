#include "test_ability_membership_fixture.h"
#include <iostream>
struct SessionPort:Session {Context value;explicit SessionPort(uint64_t peer):value{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot_id}{}
 bool current(Context& out,std::string&)override{out=value;return true;}};
struct EnginePort:Engine {
 AbilityMembershipState row=before();Evidence receipt;unsigned executed=0,applied=0;int mode=0;
 bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
 bool current(const Binding&,const std::string&,std::string&)override{return true;}
 bool ledger(const Binding&,Ledger& out,std::string&)override{out=money();return true;}
 Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong remove dispatch");}
 Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong apply remove dispatch");}
 bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong observe remove dispatch");}
 Outcome execute_action(const Binding&,const Request& request,std::string& error)override{
  if(mode==1){error="original complete Ability authoring graph dependency is absent";return Outcome::RejectedNoEffect;}
  ++executed;receipt.before=receipt.after=money();receipt.ability_membership_before=row;
  need(request.action==Action::EditAbilityMembership,"Wrong Ability authoring dispatch");row=after(request.ability_membership,row);
  receipt.ability_membership_after=row;return Outcome::Confirmed;
 }
 Outcome apply_action(const Binding&,const Request&,const Evidence& authority,std::string& error)override{
  need(row==authority.ability_membership_before,"Client complete original scene witness differs");++applied;row=authority.ability_membership_after;receipt=authority;receipt.retirement_confirmed=false;
  if(mode==2){error="native Ability authoring graph partially executed";return Outcome::Unknown;}return Outcome::Confirmed;
 }
 bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
 bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)receipt.ability_membership_after.complete.users_sha=std::string(64,'e');return true;}
};
struct Packet {uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& rows):peer(p),packets(rows){}
 bool claim(Handler,std::string&)override{return true;}
 bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}};
void flow(AbilityMembershipCommand command,bool host_origin,int mode){SessionPort hs(1),cs(2);EnginePort he,ce;he.mode=mode==1||mode==3?mode:0;ce.mode=mode==2?2:0;
 std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string error;
 need(host.start(0,error)&&client.start(0,error),error);auto& sender=host_origin?host:client;
 const bool submitted=sender.submit_ability_membership(action_id,intent(command),0,error);
 need(submitted||(mode==3&&host_origin&&host.status().locked),error);need(!ce.applied,"Capture changed Client before Host ordering");
 if(!host_origin)need(!he.executed,"Client capture changed Host before request delivery");
 std::optional<Packet> original_request,original_result;size_t delivered=0;
 while(!packets.empty()){
  need(++delivered<=8,"Unbounded Ability authoring action packet exchange");auto packet=packets.front();packets.pop_front();
  Message message;need(decode(packet.bytes.data(),packet.bytes.size(),message,error),error);
  if(message.kind==Kind::Request)original_request=packet;if(message.kind==Kind::HostResult)original_result=packet;
  const bool accepted=(packet.target==1?host:client).receive(packet.sender,packet.bytes.data(),packet.bytes.size(),1,error);
  if(!accepted)need((mode==2&&client.status().locked)||(mode==3&&host.status().locked),"Unexpected Controller delivery failure: "+error);
 }
 if(mode==1){need(!he.executed&&!ce.applied&&sender.status().outcome==Outcome::RejectedNoEffect,"Original rejected graph mutated or lost failure state");return;}
 if(mode==2){need(he.executed==1&&ce.applied==1&&host.status().locked&&client.status().locked&&!host.status().confirmed,"Unknown native Client effect escaped locked state");return;}
 if(mode==3){need(he.executed==1&&!ce.applied&&host.status().locked&&!host.status().confirmed,"Changed retirement witness was broadcast as confirmed");return;}
 need(he.executed==1&&ce.applied==1&&he.row==ce.row&&host.status().confirmed==1&&client.status().confirmed==1,"Two-peer complete Ability authoring ACK did not confirm exactly once");
 if(original_request){const auto& p=*original_request;need(host.receive(p.sender,p.bytes.data(),p.bytes.size(),3,error),error);}
 if(original_result){const auto& p=*original_result;need(client.receive(p.sender,p.bytes.data(),p.bytes.size(),3,error),error);}
 need(he.executed==1&&ce.applied==1,"Duplicate delivery replayed an original Ability authoring graph");
}
int main()try{unsigned cases=0;for(const auto command:{AbilityMembershipCommand::Create,AbilityMembershipCommand::Remove,AbilityMembershipCommand::Dock,AbilityMembershipCommand::Copy})for(bool host:{false,true})for(int mode=0;mode<4;++mode){flow(command,host,mode);++cases;}
 std::cout<<cases<<" actual Ability authoring original authoring Controller ordering, ACK, no-replay and failure flows passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
