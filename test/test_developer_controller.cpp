#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void check(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* snapshot="22222222-2222-4222-8222-222222222222";
staff::PoolState pool(bool hire_after=false,bool fired=false){staff::PoolState s;s.count=hire_after?2:1;
    for(int32_t i=0;i<50;++i){staff::PoolRow r;r.id=49-i;r.employed=i<s.count;r.active=r.employed||(fired&&i==1);s.rows.push_back(r);s.indoor.push_back(r.id);}return s;}
Ledger money(bool hired=false){Ledger l;l.cash=l.record_cash=hired?500:1000;l.record_present=1;l.record_day=l.flat_day=1;if(hired){l.record_expense=500;l.line_counts[10]=1;l.line_income[10]=-500;}return l;}
struct LocalSession:Session{Context context;explicit LocalSession(uint64_t peer):context{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string&)override{out=context;return true;}};
struct EngineFixture:Engine {
    bool hire,reject=false,partial=false,wait=false,corrupt_retirement=false;unsigned executions=0,applications=0;
    Ledger cash=money();staff::PoolState state;Evidence receipt;
    explicit EngineFixture(bool h):hire(h),state(pool(!h)){}
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string& e)override{e="unexpected Remove";return false;}
    Outcome execute_action(const Binding&,const Request& request,std::string& e)override{
        check(request.action==(hire?Action::HireDeveloper:Action::FireDeveloper),"wrong Host action");
        if(reject){e="original native pool at boundary";return Outcome::RejectedNoEffect;}
        ++executions;receipt={};receipt.before=cash;receipt.developers_before=state;
        state=hire?pool(true):pool(false,true);if(hire)cash=money(true);
        receipt.after=cash;receipt.developers_after=state;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& e)override{
        if(host.before!=cash||host.developers_before!=state){e="actual Client full BEFORE differs";return Outcome::RejectedNoEffect;}
        ++applications;cash=host.after;state=*host.developers_after;receipt=host;receipt.retirement_confirmed=false;
        if(partial){e="original Client employment readback failed";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{if(wait){e="Host BEFORE frame pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(corrupt_retirement)receipt.developers_after->rows[49].position[0]=1;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct Wire:Transport{uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}};
void flow(bool hire,int mode){LocalSession hs(1),cs(2);EngineFixture he(hire),ce(hire);std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.corrupt_retirement=mode==4;
    Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    check(host.start(0,e)&&client.start(0,e),e);check(client.submit_developer(hire,id,0,e),e);
    check(!he.executions&&!ce.applications&&ce.cash==money(),"UI submit changed native state before Host execution");
    const auto original=packets.front();auto deliver=[&]{auto p=packets.front();packets.pop_front();return(p.to==1?host:client).receive(p.from,p.bytes.data(),p.bytes.size(),1,e);};
    const bool accepted=deliver();
    if(mode==4){check(host.status().locked&&!host.status().confirmed&&host.status().cause.find("retirement")!=std::string::npos&&he.executions==1,"changed pool at receipt retirement was confirmed");return;}
    check(accepted,e);
    if(mode==1){check(deliver(),e);check(!he.executions&&!ce.applications&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("pool at boundary")!=std::string::npos,"Host no-effect cause was lost");return;}
    const auto result=packets.front();const bool applied=deliver();
    if(mode==2){check(!applied&&ce.applications==1&&client.status().locked,"partial native Client apply escaped Unknown");check(deliver(),e);check(host.status().locked&&!host.status().confirmed,"Client Unknown became completed Hire/Fire");return;}
    check(applied,e);
    if(mode==3){check(!ce.applications&&packets.empty(),"Client applied without exact BEFORE authority");ce.wait=false;check(client.tick(2,e),e);}
    check(ce.applications==1&&ce.cash==he.cash&&ce.state==he.state&&!client.status().confirmed,"Client acknowledged before exact full50 and Ledger AFTER");
    if(mode==5){packets.clear();host.tick(60000,e);check(host.status().locked&&host.status().outcome==Outcome::Unknown&&!host.status().confirmed&&host.status().cause.find("ACK")!=std::string::npos,"lost ACK became success/retry");check(he.executions==1&&ce.applications==1,"lost ACK replayed native command");return;}
    check(deliver(),e);check(deliver(),e);check(host.status().confirmed==1&&client.status().confirmed==1&&host.status().ack_mask==3,"two-peer explicit actual-apply ACK completion missing");
    check(host.receive(original.from,original.bytes.data(),original.bytes.size(),3,e),e);check(client.receive(result.from,result.bytes.data(),result.bytes.size(),3,e),e);
    check(he.executions==1&&ce.applications==1,"serialized duplicate replayed original Hire/Fire");check(!client.submit_developer(hire,id,4,e),"consumed action UUID resubmitted");
}
void codec(){for(bool hire:{true,false}){Message m;m.session=5;m.request.action=hire?Action::HireDeveloper:Action::FireDeveloper;m.request.peer=2;m.request.sequence=1;m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key="staff:developers";
    std::string e;std::vector<uint8_t> bytes;Message out;
    check(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&m==out,"Hire/Fire request roundtrip failed");auto raw=nlohmann::json::parse(bytes);check(raw.at("schema")==unsigned(hire?23:24),"wrong fixed action schema");
    const auto reject=[&](nlohmann::json bad){const auto text=bad.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"malformed Hire/Fire schema accepted");};
    auto bad=raw;bad["schema"]=uint64_t(hire?24:23);reject(bad);bad=raw;bad["request"]["action"]=20u;reject(bad);bad=raw;bad["request"]["desired_workers"]=1;reject(bad);bad=raw;bad["request"]["canonical_record_key"]="staff:other";reject(bad);
    m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence.before=money();m.evidence.after=money(hire);
    m.evidence.developers_before=pool(!hire);m.evidence.developers_after=hire?pool(true):pool(false,true);m.evidence.retirement_confirmed=true;
    check(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&m==out,"full50 result roundtrip failed: "+e);check(bytes.size()<max_payload,"full50 evidence exceeded existing action bound");
    raw=nlohmann::json::parse(bytes);bad=raw;bad["evidence"]["developers_after"]["rows"][49][6]=1;reject(bad);bad=raw;bad["evidence"]["developers_after"]["rows"][1][0]=49;reject(bad);
    bad=raw;bad["evidence"]["after"]["line_counts"][19]=1;reject(bad);bad=raw;bad["evidence"].erase("developers_before");reject(bad);
    m.evidence.developers_after.reset();check(!encode(m,bytes,e),"missing pool evidence accepted on encode");
}}
}
int main(){try{codec();for(bool hire:{true,false})for(int mode=0;mode<6;++mode)flow(hire,mode);std::cout<<checks<<" actual Hire/Fire codec/Controller assertions in 12 serialized flows passed\n";return 0;}catch(const std::exception& ex){std::cerr<<"error: "<<ex.what()<<'\n';return 1;}}
