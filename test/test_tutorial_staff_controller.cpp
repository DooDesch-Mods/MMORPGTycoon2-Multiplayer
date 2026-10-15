#include "typed_action/action_session.h"
#include "tutorial_replication/staff_action.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace staff=mt2mp::game::tutorial_replication::staff;
namespace {
void need(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* snapshot="22222222-2222-4222-8222-222222222222";
sp::Row created(){sp::Row r;r.project_present=r.purchased=true;return r;}
Ledger initial(){Ledger v;v.cash=v.record_cash=100;v.record_present=1;v.flat_day=v.record_day=1;return v;}
struct LocalSession:Session {
    Context state;
    explicit LocalSession(uint64_t peer):state{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string& e)override{out=state;e.clear();return true;}
};
struct LocalEngine:Engine {
    sp::Row row=created();Ledger money=initial();Evidence receipt;int starts=0,adoptions=0;
    bool reject=false,partial=false,wait=false,corrupt_retirement=false;int price=0;
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=money;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string& e)override{e="unexpected Remove";return false;}
    Outcome execute_action(const Binding&,const Request& request,std::string& e)override{
        need(request.action==Action::SetStartingpointWorkers,"wrong Host action");
        if(reject){e="original current project already complete";return Outcome::RejectedNoEffect;}
        ++starts;receipt={};receipt.before=money;receipt.startingpoint_before=row;
        row.desired_workers=request.desired_workers;
        receipt.after=money;receipt.startingpoint_after=row;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request& request,const Evidence& host,std::string& e)override{
        need(request.action==Action::SetStartingpointWorkers,"wrong Client action");
        if(row!=host.startingpoint_before||money!=host.before){e="actual Client BEFORE row/ledger differs";return Outcome::RejectedNoEffect;}
        ++adoptions;money=host.after;row=*host.startingpoint_after;receipt=host;receipt.retirement_confirmed=false;
        if(partial){e="original Client allocation readback failed";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{
        if(wait){e="Host BEFORE frame pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(corrupt_retirement)++receipt.after.line_counts[3];return true;}
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;std::deque<Packet>& packets;
    Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}
};
void flow(int mode){
    LocalSession hs(1),cs(2);LocalEngine he,ce;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    he.price=ce.price=mode==5?10:0;he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.corrupt_retirement=mode==4;
    Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    need(host.start(0,e)&&client.start(0,e),e);need(client.submit_startingpoint_workers(id,1,0,e),e);
    need(he.starts==0&&ce.starts==0&&ce.adoptions==0&&ce.money==initial(),"click changed state before Host execution");
    const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing serialized action packet");auto p=packets.front();packets.pop_front();return (p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
    const auto accepted=deliver();
    if(mode==4){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&host.status().confirmed==0&&
        host.status().cause.find("retirement")!=std::string::npos,"altered retirement ledger was confirmed or lost its cause");return;}
    need(accepted,e);
    if(mode==1){need(deliver(),e);need(!he.starts&&!ce.adoptions&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("already complete")!=std::string::npos,"Host rejection cause/effects wrong");return;}
    need(he.starts==1&&!ce.adoptions,"Host request did not execute once before application");const auto result=packets.front();
    const auto applied=deliver();
    if(mode==2){need(!applied&&ce.adoptions==1&&client.status().locked,"partial Client effect escaped Unknown");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"partial effect ACK became success");return;}
    need(applied,e);
    if(mode==3){need(!ce.adoptions&&packets.empty(),"missing BEFORE frame ACKed");ce.wait=false;need(client.tick(2,e),e);}
    need(ce.adoptions==1&&ce.money==he.money&&ce.money==initial()&&ce.row.desired_workers==1&&!client.status().confirmed,"Client ACK before exact AFTER or completion");
    need(deliver(),e);need(deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1,"required two-peer ACK completion missing");
    need(host.receive(request.sender,request.bytes.data(),request.bytes.size(),3,e),e);
    need(client.receive(result.sender,result.bytes.data(),result.bytes.size(),3,e),e);
    need(he.starts==1&&ce.adoptions==1,"duplicate serialized Request/HostResult replayed Start");
    need(!client.submit_startingpoint_workers(id,1,4,e),"UUID resubmitted after completion");
}
void codec(){
    Message m;m.session=5;m.request.action=Action::SetStartingpointWorkers;m.request.peer=2;m.request.sequence=1;m.request.desired_workers=1;
    m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key="tech:startingpoint";
    std::string e;std::vector<uint8_t> bytes;Message decoded;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"schema22 Request roundtrip failed");
    auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==22,"Staff did not bind schema22");
    const auto reject=[&](nlohmann::json j){const auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"corrupt Staff wire accepted");};
    auto bad=raw;bad["schema"]=21u;reject(bad);bad=raw;bad["request"]["action"]=19u;reject(bad);
    for(const auto& value:{nlohmann::json(-1),nlohmann::json(51),nlohmann::json(true),nlohmann::json(1.0),nlohmann::json("1")}){bad=raw;bad["request"]["desired_workers"]=value;reject(bad);}
    bad=raw;bad["request"]["price"]=0;reject(bad);bad=raw;bad["request"]["canonical_record_key"]="tech:other";reject(bad);
    m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.before=m.evidence.after=initial();m.evidence.startingpoint_before=created();m.evidence.startingpoint_after=created();m.evidence.startingpoint_after->desired_workers=1;m.evidence.retirement_confirmed=true;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"schema22 exact no-cost result rejected");
    ++m.evidence.after.line_counts[19];need(!encode(m,bytes,e),"foreign ledger mutation accepted");--m.evidence.after.line_counts[19];
    m.evidence.startingpoint_after->work=1;need(!encode(m,bytes,e),"work/progress mutation accepted as Staff");
    m.evidence.startingpoint_after->work=0;m.evidence.startingpoint_after->desired_workers=2;need(!encode(m,bytes,e),"different desired workers confirmed");
}

}
int main(){try{codec();for(int mode=0;mode<5;++mode)flow(mode);std::cout<<"PASS Staff strict codec and 5 serialized Controller scenarios\n";return 0;}
catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
