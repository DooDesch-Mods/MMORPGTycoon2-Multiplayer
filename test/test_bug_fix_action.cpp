#include "typed_action/action_session.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace bugs=mt2mp::game::bug_authority;
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void need(bool okay,const std::string& cause){++checks;if(!okay)throw std::runtime_error(cause);}
constexpr const char* action_id="11111111-1111-4111-8111-111111111111";
constexpr const char* snapshot="22222222-2222-4222-8222-222222222222";
constexpr const char* parent="action:33333333-3333-4333-8333-333333333333";
bugs::Row bug(){bugs::Row row;row.parent_key=parent;row.birth={bugs::BirthKind::Snapshot,std::string(64,'a'),0,0};row.value.known=true;row.value.variant=0;return row;}
bugs::Birth job_birth(){return {bugs::BirthKind::Snapshot,std::string(64,'a'),0,91};}
BugFixIntent intent(bool schedule){const auto row=bug();return {{row.parent_kind,row.parent_key,row.birth},schedule,schedule?std::nullopt:std::optional(job_birth())};}
staff::PoolState pool(bool has_job,bool assigned){
    staff::PoolState out;out.count=1;out.construction.emplace();out.construction->work.emplace();
    for(int32_t id=0;id<50;++id){staff::PoolRow row;row.id=id;row.active=row.employed=id==0;row.on_map=row.job_link=assigned&&id==0;
        if(row.job_link)row.state=2;out.rows.push_back(row);(row.on_map?out.outdoor:out.indoor).push_back(id);
        staff::construction::ActorPose pose;pose.id=id;out.construction->actors.push_back(pose);}
    if(has_job){bugs::JobRow job;job.bug=intent(false).bug;job.birth=job_birth();job.developer_id=assigned?0:-1;out.construction->work->jobs.push_back(job);
        bugs::ParentVisual p;p.key=parent;out.construction->work->parents.push_back(p);}
    return out;
}
Evidence evidence(bool schedule,bool assigned=false){
    Evidence out;out.bug_before=bug();out.bug_after=*out.bug_before;out.bug_after->value.manual=true;
    out.developers_before=pool(!schedule,assigned);out.developers_after=out.developers_before;
    auto& work=*out.developers_after->construction->work;
    if(schedule){bugs::JobRow job;job.bug=intent(true).bug;job.birth={bugs::BirthKind::Action,action_id,0,1};work.jobs.push_back(job);
        bugs::ParentVisual p;p.key=parent;work.parents.push_back(p);work.created.value.today=work.created.value.total=work.created.highest=1;}
    else if(assigned){work.jobs[0].type=0;work.jobs[0].valid=false;}else work.jobs.clear();
    return out;
}
Message message(bool schedule,bool result=false,bool assigned=false){Message m;m.session=5;m.request.action=Action::SetBugFix;m.request.peer=2;m.request.sequence=1;
    m.request.action_uuid=action_id;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key=parent;m.request.bug_fix=intent(schedule);
    if(result){m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence=evidence(schedule,assigned);m.evidence.retirement_confirmed=true;}return m;}
void codec(){std::string error;std::vector<uint8_t> bytes;Message out;
    for(bool schedule:{true,false})for(bool result:{false,true}){
        auto m=message(schedule,result);need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),out,error)&&m==out,"Bug Fix roundtrip: "+error);
        auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==26&&bytes.size()<max_payload,"schema26/full-pool payload bound differs");
        const auto reject=[&](nlohmann::json value){const auto text=value.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,error),"malformed Bug Fix accepted");};
        auto bad=raw;bad["schema"]=25u;reject(bad);bad=raw;bad["request"]["action"]=23u;reject(bad);
        bad=raw;bad["request"]["bug_fix"]["schedule"]=1;reject(bad);bad=raw;bad["request"]["bug_fix"]["bug"][0]=3;reject(bad);
        bad=raw;bad["request"]["bug_fix"]["bug"].push_back(0);reject(bad);bad=raw;bad["request"]["canonical_record_key"]="tech:startingpoint";reject(bad);
        bad=raw;bad["request"]["extra"]=false;reject(bad);
        if(schedule){bad=raw;bad["request"]["bug_fix"]["expected_job"]=nlohmann::json::array({0,std::string(64,'a'),0,91});reject(bad);}
        else{bad=raw;bad["request"]["bug_fix"]["expected_job"]=nullptr;reject(bad);bad=raw;bad["request"]["bug_fix"]["expected_job"][3]=uint64_t(INT32_MAX)+1;reject(bad);}
    }
    auto good=message(true,true);
    const auto reject=[&](Message m,const char* cause){need(!encode(m,bytes,error),cause);};
    auto m=good;m.evidence.bug_after->value.known=false;reject(m,"unknown selected Bug accepted");
    m=good;m.evidence.bug_after->value.manual=false;reject(m,"manual flag not set");
    m=good;m.evidence.bug_after->value.reproducibility=0.5f;reject(m,"unrelated Bug value changed");
    m=good;m.evidence.bug_after->birth.occurrence=1;reject(m,"selected Bug identity changed");
    m=good;m.evidence.after.cash=1;reject(m,"ledger changed");
    m=good;m.evidence.undo_confirmed=true;reject(m,"undo side effect accepted");
    m=good;m.evidence.developers_after->rows[40].position[0]=1;reject(m,"unrelated actor position changed");
    m=good;m.evidence.developers_after->construction->work->jobs[0].birth.occurrence=0;reject(m,"wrong action occurrence accepted");
    m=good;m.evidence.developers_after->construction->work->jobs[0].birth.source=snapshot;reject(m,"foreign action Job accepted");
    m=good;m.evidence.developers_after->construction->work->created.value.today=0;reject(m,"missing created counter accepted");
    m=good;m.evidence.developers_after->construction->work->fixed.value.total=1;reject(m,"fixed counter changed");
    m=good;m.evidence.developers_after->construction->work->created.day=1;reject(m,"counter day changed");
    m=good;m.evidence.developers_after->construction->work->parents.clear();reject(m,"new selected Parent observation missing");
    m=good;m.evidence.bug_before.reset();reject(m,"single Bug evidence accepted");
    m=message(false,true,true);need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),out,error)&&m==out,"assigned Abort roundtrip: "+error);
    m.evidence.bug_before->value.debugged=m.evidence.bug_after->value.debugged=true;
    need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),out,error)&&m==out,"assigned debugging Cancel rejected");
    auto bad=m;bad.evidence.developers_after->construction->work->jobs[0].type=7;reject(bad,"Cancel did not abort selected Job");
    bad=m;bad.evidence.developers_after->construction->work->parents[0].state=1;reject(bad,"Cancel mutated Parent state");
    bad=message(false,true);bad.evidence.developers_after->construction->work->parents.clear();reject(bad,"Cancel dropped retained Parent");
    bad=message(false,true);bad.request.bug_fix.expected_job->occurrence=92;reject(bad,"stale Cancel occurrence accepted");
    m=good;m.request.action=Action::StartStartingpoint;m.request.canonical_record_key="tech:startingpoint";m.request.bug_fix={};reject(m,"Bug evidence leaked to other action");
    m=message(true);m.request.action=Action::StartStartingpoint;m.request.canonical_record_key="tech:startingpoint";reject(m,"Bug intent leaked to other action");
    for(bool hire:{true,false}){m=message(true);m.request.action=hire?Action::HireDeveloper:Action::FireDeveloper;m.request.bug_fix={};m.request.canonical_record_key="staff:developers";
        need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),out,error)&&m==out&&nlohmann::json::parse(bytes)["schema"]==(hire?23:24),"prior Hire/Fire request schema changed");}
}
struct SessionFixture:Session{Context context;explicit SessionFixture(uint64_t peer):context{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string&)override{out=context;return true;}};
struct EngineFixture:Engine{
    bool schedule=true,assigned=false,reject=false,partial=false,wait=false,corrupt=false;unsigned executed=0,applied=0;Evidence receipt;
    bool bind(const std::string&,Binding& b,std::string&)override{b={1,1};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& l,std::string&)override{l={};return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string& e)override{e="unexpected remove";return Outcome::RejectedNoEffect;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& e)override{e="unexpected remove";return Outcome::RejectedNoEffect;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{return false;}
    Outcome execute_action(const Binding&,const Request& r,std::string& e)override{
        need(r.action==Action::SetBugFix&&r.bug_fix==intent(schedule),"wrong ordered Bug action");
        if(reject){e="exact stale Job";return Outcome::RejectedNoEffect;}++executed;receipt=evidence(schedule,assigned);return Outcome::Confirmed;}
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& e)override{
        ++applied;receipt=host;receipt.retirement_confirmed=false;if(partial){e="exact partial Abort";return Outcome::Unknown;}return Outcome::Confirmed;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(corrupt)receipt.bug_after->value.variant=1;return true;}
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{if(wait){e="BEFORE pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct Wire:Transport{uint64_t peer;std::deque<Packet>& q;Wire(uint64_t p,std::deque<Packet>& packets):peer(p),q(packets){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& b,std::string&)override{q.push_back({peer,target,b});return true;}};
void flow(bool schedule,int mode){SessionFixture hs(1),cs(2);EngineFixture he,ce;he.schedule=ce.schedule=schedule;he.assigned=ce.assigned=!schedule&&mode==6;
    he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.corrupt=mode==4;
    std::deque<Packet> q;Wire hw(1,q),cw(2,q);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    need(host.start(0,e)&&client.start(0,e),e);need(client.submit_bug_fix(action_id,intent(schedule),0,e),e);
    need(!he.executed&&!ce.applied,"submit performed native effect");const auto request=q.front();
    const auto deliver=[&]{auto p=q.front();q.pop_front();return(p.to==1?host:client).receive(p.from,p.bytes.data(),p.bytes.size(),1,e);};
    const auto accepted=deliver();if(mode==4){need(host.status().locked&&he.executed==1&&host.status().cause.find("retirement")!=std::string::npos,"Bug retirement drift confirmed");return;}need(accepted,e);
    if(mode==1){need(deliver(),e);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("exact stale Job")!=std::string::npos,"stale request cause lost");return;}
    const auto result=q.front();const auto applied=deliver();
    if(mode==2){need(!applied&&client.status().locked&&ce.applied==1,"partial Client failure did not lock");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"Unknown confirmed");return;}need(applied,e);
    if(mode==3){need(!ce.applied&&q.empty(),"applied before BEFORE authority");ce.wait=false;need(client.tick(2,e),e);}
    if(mode==5){q.clear();host.tick(60000,e);need(host.status().locked&&host.status().outcome==Outcome::Unknown&&he.executed==1&&ce.applied==1,"lost ACK replayed/confirmed");return;}
    need(deliver()&&deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1&&host.status().ack_mask==3,"once ordered ACK3 missing");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),3,e)&&client.receive(result.from,result.bytes.data(),result.bytes.size(),3,e),e);
    need(he.executed==1&&ce.applied==1&&!client.submit_bug_fix(action_id,intent(schedule),4,e),"duplicate action replayed");
}
}
int main(){try{codec();for(bool schedule:{true,false})for(int mode=0;mode<7;++mode)flow(schedule,mode);
    std::cout<<checks<<" actual Bug Fix codec/Controller checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
