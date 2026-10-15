#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace {
void need(bool okay,const std::string& why){if(!okay)throw std::runtime_error(why);}
constexpr const char* id="11111111-1111-1111-1111-111111111111";
constexpr const char* snapshot="22222222-2222-2222-2222-222222222222";
sp::Row before(){sp::Row r;r.project_present=true;r.purchased=true;r.work=182;r.lines=182;r.desired_workers=1;return r;}
sp::Row after(){sp::Row r;r.purchased=true;r.deployed=true;r.inventory_count=r.release_feature_count=r.release_change_count=1;r.release_counter=1;r.commit_bugs=sp::CommitBugs{};return r;}
struct LocalSession:Session {
    Context state;
    explicit LocalSession(uint64_t peer):state{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string& e)override{out=state;e.clear();return true;}
};
struct LocalEngine:Engine {
    sp::Row row=before();Ledger money;Evidence receipt;int commits=0,adoptions=0;
    bool reject=false,partial=false,wait=false,retirement_change=false;
    LocalEngine(){money.cash=100;money.record_present=1;money.record_cash=100;}
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=money;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string& e)override{e="unexpected remove";return Outcome::RejectedNoEffect;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& e)override{e="unexpected remove";return Outcome::RejectedNoEffect;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string& e)override{e="unexpected remove";return false;}
    Outcome execute_action(const Binding&,const Request& r,std::string& e)override{
        need(r.action==Action::CommitStartingpoint,"wrong Host action");
        if(reject){e="native completion getter false";return Outcome::RejectedNoEffect;}
        ++commits;receipt={};receipt.before=receipt.after=money;receipt.startingpoint_before=row;
        row=after();receipt.startingpoint_after=row;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request& r,const Evidence& host,std::string& e)override{
        need(r.action==Action::CommitStartingpoint,"wrong Client action");
        if(row!=host.startingpoint_before){e="native project owner/readback differs";return Outcome::RejectedNoEffect;}
        ++adoptions;row=after();receipt=host;receipt.retirement_confirmed=false;
        if(partial){e="original Deploy partial readback failure";return Outcome::Unknown;}
        return Outcome::Confirmed;
    }
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{
        if(wait){e="Host before-frame pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;
    }
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{
        receipt.retirement_confirmed=true;
        if(retirement_change)++receipt.startingpoint_after->release_counter;
        return true;
    }
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;std::deque<Packet>& queue;
    Wire(uint64_t p,std::deque<Packet>& q):peer(p),queue(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{queue.push_back({peer,target,bytes});return true;}
};
void flow(int mode){
    LocalSession hs(1),cs(2);LocalEngine he,ce;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    need(host.start(0,e)&&client.start(0,e),e);
    he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.retirement_change=mode==4;
    need(client.submit_commit_startingpoint(id,0,e),e);
    need(!client.submit_commit_startingpoint("33333333-3333-3333-3333-333333333333",0,e),"overlapping click queued");
    need(he.commits==0&&ce.commits==0&&ce.adoptions==0,"Client click mutated before Host request");
    const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"expected action packet");auto p=packets.front();packets.pop_front();return (p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
    const bool host_result=deliver();
    if(mode==4){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&host.status().confirmed==0,"changed retirement row accepted");
        need(host.status().cause.find("retirement")!=std::string::npos,"retirement failure cause lost");return;}
    need(host_result,e);
    if(mode==1){need(deliver(),e);need(he.commits==0&&ce.adoptions==0&&client.status().outcome==Outcome::RejectedNoEffect,"Host rejection lost");
        need(client.status().cause.find("completion getter false")!=std::string::npos,"native rejection cause lost");return;}
    need(he.commits==1&&ce.commits==0&&ce.adoptions==0,"original Host Commit count differs");
    const auto result=packets.front();
    const bool applied=deliver();
    if(mode==2){need(!applied&&ce.adoptions==1&&client.status().locked,"partial Client effect not locked");
        need(deliver(),e);need(host.status().locked,"partial Client effect reported success");return;}
    need(applied,e);
    if(mode==3){need(ce.adoptions==0&&packets.empty(),"before-frame wait ACKed early");ce.wait=false;need(client.tick(2,e),e);}
    need(ce.adoptions==1&&ce.commits==0&&client.status().confirmed==0,"Client confirmed before completion or called Commit");
    need(deliver(),e);need(deliver(),e);
    need(host.status().confirmed==1&&client.status().confirmed==1,"two-peer confirmation missing");
    need(host.receive(request.sender,request.bytes.data(),request.bytes.size(),3,e),e);
    need(client.receive(result.sender,result.bytes.data(),result.bytes.size(),3,e),e);
    need(he.commits==1&&ce.adoptions==1,"duplicate request/result replayed native effect");
    need(!client.submit_commit_startingpoint(id,4,e),"confirmed action UUID resubmitted");
}
void codec(){
    Message m;m.kind=Kind::Request;m.session=5;m.request.action=Action::CommitStartingpoint;
    m.request.peer=2;m.request.sequence=1;m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key="tech:startingpoint";
    std::vector<uint8_t> bytes;std::string e;Message decoded;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"schema20 Request roundtrip");
    auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==20,"wrong schema");
    const auto reject=[&](nlohmann::json j){const auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"malformed schema20 accepted");};
    auto bad=raw;bad["schema"]=19u;reject(bad);
    bad=raw;bad["request"]["command"]="arbitrary";reject(bad);
    bad=raw;bad["request"]["canonical_record_key"]="tech:other";reject(bad);
    bad=raw;bad["evidence"]["startingpoint_before"]=sp::encode(before());reject(bad);
    m.request.price.fields=1;need(!encode(m,bytes,e),"foreign request payload accepted");m.request.price={};
    m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    m.evidence.startingpoint_before=before();m.evidence.startingpoint_after=after();m.evidence.retirement_confirmed=true;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"schema20 Host effect roundtrip");
    raw=nlohmann::json::parse(bytes);bad=raw;bad["evidence"]["startingpoint_after"]["deployed"]=1;reject(bad);
    m.evidence.after.cash=1;need(!encode(m,bytes,e),"Commit money effect accepted");m.evidence.after.cash=0;
    m.evidence.startingpoint_after->commit_bugs.reset();need(!encode(m,bytes,e),"Commit without original bug receipt accepted");
    m.evidence.startingpoint_after=after();m.evidence.startingpoint_before->bugs_total=1;need(!encode(m,bytes,e),"nonzero Map bugs accepted");
    m.evidence.startingpoint_before=sp::Row{};m.evidence.startingpoint_after=before();
    need(!encode(m,bytes,e),"project creation accepted as Commit effect");
    m.evidence.startingpoint_after=after();
    m.evidence.startingpoint_before=before();m.evidence.startingpoint_before->release_counter=1;m.evidence.startingpoint_after->release_counter=2;
    need(!encode(m,bytes,e),"noninitial Release accepted");
}
}
int main(){try{codec();for(int mode=0;mode<5;++mode)flow(mode);std::cout<<"PASS schema20 exact fixed intent, native receipt, two-peer ACK, before-frame wait, rejection, Unknown and duplicate no-replay\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
