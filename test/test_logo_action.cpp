#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace {
using json=nlohmann::json;int checks=0;void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
json token(int type,uint32_t bits,const std::string& text){return json::array({type,uint64_t(bits),text});}
LogoIntent intent(){auto records=json::array();records.push_back(json::array({int64_t(-1),uint64_t(0),token(1,0,"mmoLogo"),json::array()}));
    records.push_back(json::array({int64_t(0),uint64_t(0),token(1,0,"title"),json::array({token(2,0,"Shared world")})}));
    return {std::string(64,'a'),std::string(64,'b'),records};}
const char* snapshot="11111111-1111-4111-8111-111111111111";const char* id="22222222-2222-4222-8222-222222222222";
Evidence result(){Evidence e;e.before.cash=e.after.cash=100;e.logo_before=LogoState{true,std::string(64,'a')};e.logo_after=LogoState{true,std::string(64,'b')};return e;}
struct S:Session {Context value;S(uint64_t peer):value{17,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=value;return true;}};
struct E:Engine {Evidence receipt;int calls=0,mode=0;bool bind(const std::string&,Binding& out,std::string&)override{out={2,4};return true;}bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=calls?receipt.after:result().before;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("wrong old Remove execution");}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("wrong old Remove apply");}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("wrong old Remove observation");}
    Outcome execute_action(const Binding&,const Request& request,std::string&)override{need(request.action==Action::SetLogo,"Logo dispatch differs");++calls;receipt=result();return Outcome::Confirmed;}
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& e)override{++calls;receipt=host;receipt.retirement_confirmed=false;
        if(mode==1){e="original adoption interrupted";return Outcome::Unknown;}if(mode==2)receipt.logo_after->sha256=std::string(64,'c');return Outcome::Confirmed;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(mode==3)receipt.after.line_counts[19]++;return true;}};
struct Packet{uint64_t from,to;std::vector<uint8_t> data;};
struct T:Transport {uint64_t peer;std::deque<Packet>& queue;T(uint64_t p,std::deque<Packet>& q):peer(p),queue(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{queue.push_back({peer,to,bytes});return true;}};
void flow(int mode){S hs(1),cs(2);E he,ce;he.mode=mode==3?3:0;ce.mode=mode;std::deque<Packet> queue;T hw(1,queue),cw(2,queue);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    need(host.start(0,e)&&client.start(0,e),e);need(client.submit_logo(id,intent(),0,e),e);need(!he.calls&&!ce.calls,"source applied before Host ordering");const auto request=queue.front();
    auto deliver=[&](){need(!queue.empty(),"missing packet");const auto p=queue.front();queue.pop_front();return(p.to==1?host:client).receive(p.from,p.data.data(),p.data.size(),1,e);};
    const bool ordered=deliver();if(mode==3){need(host.status().locked&&!host.status().confirmed,"unrelated retirement Ledger mutation accepted");return;}need(ordered,e);const auto authority=queue.front();
    const bool applied=deliver();if(mode==1||mode==2){need(!applied&&client.status().locked&&!client.status().confirmed,"Unknown or wrong adopted logo ACKed");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"failed adoption became full ACK");return;}
    need(deliver()&&deliver(),e);need(host.status().ack_mask==3&&host.status().confirmed==1&&client.status().confirmed==1,"full original adoption ACK absent");
    need(host.receive(request.from,request.data.data(),request.data.size(),2,e)&&client.receive(authority.from,authority.data.data(),authority.data.size(),2,e),e);need(he.calls==1&&ce.calls==1,"duplicate replayed owning adoption");need(!client.submit_logo(id,intent(),3,e),"consumed ownership token accepted twice");}
void codec(){auto original=intent();need(valid_logo(original),"captured Logo value invalid");std::vector<mt2mp::subscriber_lifecycle::Record> records;std::string e;
    need(logo_records(original.records,records,e)&&records.size()==2&&records[1].tokens[0].text=="Shared world",e);
    for(int mode=0;mode<5;++mode){auto bad=original;if(mode==0)bad.records[0][2]=token(1,0,"mmoDungeon");if(mode==1)bad.records[1][0]=1;if(mode==2)bad.records[1][1]=2;if(mode==3)bad.records[1][3][0]=token(3,0x7fc00000,"");if(mode==4)bad.records[1][3][0]=token(2,0,std::string("a\0b",3));need(!valid_logo(bad),"unsafe detached value admitted");}
    Message m;m.session=17;m.request.action=Action::SetLogo;m.request.peer=2;m.request.sequence=1;m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key="software:logo";m.request.logo=original;
    std::vector<uint8_t> bytes;Message read;need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),read,e)&&read==m,e);auto j=json::parse(bytes);need(j["schema"]==35,"Logo schema differs");j["request"]["logo"]["pointer"]=7;auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),read,e),"native pointer/extra member admitted");
    m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence=result();m.evidence.retirement_confirmed=true;need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),read,e)&&read==m,e);
    m.evidence.logo_after->sha256=std::string(64,'f');need(!encode(m,bytes,e),"Host result differs from desired detached value");
}
}
int main()try{codec();for(int mode=0;mode<4;++mode)flow(mode);std::cout<<checks<<" Logo value/codec/Controller assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
