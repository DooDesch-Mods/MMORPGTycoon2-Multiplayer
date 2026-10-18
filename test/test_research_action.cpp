#include "typed_action/action_session.h"
#include "tutorial_replication/start_ledger.h"
#include "tutorial_replication/state.h"
#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace tutorial=mt2mp::game::tutorial_replication;
namespace research=tutorial::research;
namespace start=tutorial::start_intent;
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace {
unsigned checks=0;
void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* snapshot="22222222-2222-4222-8222-222222222222";
research::Row baseline(){research::Row r{"bridges",{}};r.value.release_change_count=4;r.value.release_counter=9;return r;}
research::Row project(){auto r=baseline();r.value.project_present=r.value.purchased=true;return r;}
research::Row complete_project(){auto r=project();r.value.work=21600;r.value.lines=36;r.value.desired_workers=2;r.value.bugs_total=5;r.value.bugs_known=3;return r;}
research::Row deployed(){auto r=baseline();r.value.purchased=r.value.deployed=true;r.value.inventory_count=r.value.release_feature_count=1;++r.value.release_change_count;++r.value.release_counter;r.value.commit_bugs=tutorial::startingpoint::CommitBugs{5,3};return r;}
Ledger initial(){Ledger v;v.cash=v.record_cash=88160;v.record_present=1;v.flat_day=v.record_day=2;v.record_expense=40;v.record_income=200;v.line_counts[3]=2;v.line_income[3]=-40;v.line_counts[19]=7;v.line_income[19]=93;return v;}
Message request(Action action){Message m;m.session=5;m.request.action=action;m.request.peer=2;m.request.sequence=1;m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key="tech:bridges";if(action==Action::SetResearchWorkers)m.request.desired_workers=3;return m;}
Evidence effect(Action action){
    Evidence e;e.before=e.after=initial();std::string cause;
    if(action==Action::StartResearch){e.research_before=baseline();e.research_after=project();need(start::expected_payment(e.before,10000,e.after,cause),cause);}
    else if(action==Action::SetResearchWorkers){e.research_before=complete_project();e.research_after=e.research_before;e.research_after->value.desired_workers=3;}
    else {e.research_before=complete_project();e.research_after=deployed();}
    e.retirement_confirmed=true;return e;
}
void plans(){
    std::string e;research::Plan p;
    need(research::plan(baseline(),baseline(),p,e)&&p==research::Plan::None,e);
    need(research::plan(baseline(),project(),p,e)&&p==research::Plan::Create,e);
    need(research::plan(project(),complete_project(),p,e)&&p==research::Plan::Project,e);
    need(research::plan(complete_project(),deployed(),p,e)&&p==research::Plan::Deploy,"nonempty Release/actual nonzero bugs rejected: "+e);
    auto observed=complete_project(),peer=observed;observed.value.release_counter=560;peer.value.release_counter=440;
    need(observed!=peer&&research::same_state(observed,peer),"process stamp was not isolated from semantic equality");
    for(int field=0;field<5;++field){auto bad=peer;
        if(field==0)++bad.value.work;if(field==1)++bad.value.lines;if(field==2)++bad.value.inventory_count;
        if(field==3)++bad.value.release_change_count;if(field==4)++bad.value.release_feature_count;
        need(!research::same_state(observed,bad),"semantic Research difference ignored with unequal stamps");}
    auto bad=deployed();bad.value.commit_bugs->known=2;need(!research::plan(complete_project(),bad,p,e),"altered known bug receipt accepted");
    bad=deployed();bad.value.commit_bugs->total=6;need(!research::plan(complete_project(),bad,p,e),"altered total bug receipt accepted");
    bad=deployed();bad.value.commit_bugs.reset();need(!research::plan(complete_project(),bad,p,e),"missing actual Commit receipt accepted");
    bad=deployed();bad.value.deployed=false;bad.value.inventory_count=bad.value.release_feature_count=0;bad.value.commit_bugs.reset();need(!research::plan(complete_project(),bad,p,e),"retirement without deployment accepted");
    for(int field=0;field<4;++field){bad=deployed();if(field==0)++bad.value.inventory_count;if(field==1)++bad.value.release_feature_count;if(field==2)++bad.value.release_change_count;if(field==3)++bad.value.release_counter;need(!research::plan(complete_project(),bad,p,e),"invented deployment delta accepted");}
    bad=project();++bad.value.release_counter;need(!research::plan(baseline(),bad,p,e),"creation changed global Release counter");
    bad=complete_project();++bad.value.release_change_count;need(!research::plan(project(),bad,p,e),"project frame changed Release count");
    auto a=complete_project();a.value.release_counter=UINT32_MAX;bad=deployed();bad.value.release_counter=0;need(!research::plan(a,bad,p,e),"Release counter wrapped");
    a=complete_project();a.value.release_change_count=4096;bad=deployed();bad.value.release_change_count=4096;need(!research::plan(a,bad,p,e),"Release count bound bypassed");
    bad=project();bad.key="startingpoint";need(!research::valid(bad,e),"Startingpoint admitted as Research");
    bad=project();bad.value.work=std::numeric_limits<float>::infinity();need(!research::valid(bad,e),"infinite project work accepted");
    bad=deployed();bad.value.desired_workers=1;need(!research::valid(bad,e),"retired workers retained");
    tutorial::startingpoint::Plan old;
    need(!tutorial::startingpoint::plan(complete_project().value,deployed().value,old,e),"Startingpoint first-Release guard weakened");
    auto raw=research::encode(deployed());research::Row decoded;
    need(raw.size()==15&&!raw.contains("value")&&research::decode(raw,decoded,e)&&decoded==deployed(),"flat15 Research codec failed: "+e);
    raw["work"]=0;need(!research::decode(raw,decoded,e)&&decoded==research::Row{},"integer work accepted or partial decode leaked");
}
void codec(){
    std::string e;std::vector<uint8_t> bytes;Message decoded;
    for(const auto action:{Action::StartResearch,Action::SetResearchWorkers,Action::CommitResearch}){
        auto m=request(action);const auto schema=uint64_t(action)+2;
        need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"Research Request roundtrip: "+e);
        auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==schema,"Research schema/action binding differs");
        const auto reject=[&](nlohmann::json j){const auto data=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(data.data()),data.size(),decoded,e),"malformed Research packet accepted");};
        auto bad=raw;bad["schema"]=21u;reject(bad);bad=raw;bad["request"]["canonical_record_key"]="tech:friendslist";reject(bad);
        bad=raw;bad["request"]["action"]=19u;reject(bad);bad=raw;bad["request"]["price"]=10000;reject(bad);
        bad=raw;bad["request"]["desired_workers"]=51;reject(bad);
        m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;m.evidence=effect(action);
        need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"Research HostResult roundtrip: "+e);
        auto wrong=m;wrong.evidence.research_after->key="flightpaths";need(!encode(wrong,bytes,e),"foreign research key accepted");
        wrong=m;wrong.evidence.research_before.reset();need(!encode(wrong,bytes,e),"one-sided Research evidence accepted");
        wrong=m;wrong.evidence.startingpoint_before=wrong.evidence.startingpoint_after=tutorial::startingpoint::Row{};need(!encode(wrong,bytes,e),"Research carried Startingpoint evidence");
        wrong=m;wrong.evidence.after.line_income[19]++;need(!encode(wrong,bytes,e),"foreign ledger change accepted");
        wrong=m;wrong.request.action=Action::StartStartingpoint;wrong.request.canonical_record_key="tech:startingpoint";wrong.request.desired_workers=0;need(!encode(wrong,bytes,e),"Research evidence escaped action domain");
        if(action==Action::StartResearch){
            wrong=m;need(start::expected_payment(wrong.evidence.before,9999,wrong.evidence.after,e),e);need(!encode(wrong,bytes,e),"wrong Bridges price accepted despite valid ledger");
            wrong=m;need(start::expected_payment(wrong.evidence.before,0,wrong.evidence.after,e),e);need(!encode(wrong,bytes,e),"free Bridges purchase accepted");
            wrong=m;wrong.evidence.after.line_counts[3]--;need(!encode(wrong,bytes,e),"purchase omitted category3 accounting");
        }
        if(action==Action::SetResearchWorkers){wrong=m;wrong.evidence.research_after->value.bugs_total++;need(!encode(wrong,bytes,e),"workers intent changed bug totals");}
        if(action==Action::CommitResearch){wrong=m;wrong.evidence.research_after->value.commit_bugs->total=0;wrong.evidence.research_after->value.commit_bugs->known=0;need(!encode(wrong,bytes,e),"Commit hid actual bugs");}
    }
}
economy::State frame(){
    economy::State s;auto& f=s.day.frame;f.session=41;f.snapshot=snapshot;f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,88160,0,0,0};f.values.cash=88160;
    tutorial::State t;t.leveltwo_complete=true;t.research=complete_project();
    tutorial::startingpoint::Row old;old.purchased=old.deployed=true;old.inventory_count=1;old.release_feature_count=1;old.release_change_count=4;old.release_counter=9;t.startingpoint=old;
    t.staff_display=tutorial::staff_display::State{1,1,{{7,6,false}}};f.tutorial=t;s.day.history={{1,live::day::record(f.values)}};return s;
}
void frames(){
    auto state=frame();std::string e;std::vector<uint8_t> bytes;economy::State decoded;
    need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"complete Economy/Day/HostFrame Research roundtrip failed: "+e);
    auto invalid=state;invalid.day.frame.tutorial->startingpoint->release_counter++;need(!economy::encode(invalid,bytes,e),"shared Release counter mismatch accepted");
    invalid=state;invalid.day.frame.tutorial->startingpoint->release_change_count++;need(!economy::encode(invalid,bytes,e),"shared Release count mismatch accepted");
    invalid=state;auto& old=*invalid.day.frame.tutorial->startingpoint;old.project_present=true;old.deployed=false;old.inventory_count=old.release_feature_count=0;need(!economy::encode(invalid,bytes,e),"two simultaneous native current projects accepted");
    state.day.frame.tutorial->research=deployed();state.day.frame.tutorial->startingpoint->release_change_count=5;state.day.frame.tutorial->startingpoint->release_counter=10;state.day.frame.tutorial->staff_display.reset();
    need(economy::encode(state,bytes,e)&&economy::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==state,"complete Commit frame with real bug receipt exceeds depth or loses fields: "+e);
    auto raw=nlohmann::json::parse(bytes);raw["day"]["frame"]["tutorial"]["research"]["key"]="friendslist";const auto data=raw.dump();
    need(!economy::decode(reinterpret_cast<const uint8_t*>(data.data()),data.size(),decoded,e)&&decoded==economy::State{},"invalid nested key accepted or partial full frame leaked");
    auto legacy=*frame().day.frame.tutorial;legacy.research.reset();legacy.staff_display.reset();auto oldwire=tutorial::encode(legacy);tutorial::State restored;
    need(!oldwire.contains("research")&&tutorial::decode(oldwire,restored,e)&&restored==legacy,"old tutorial wire changed without Research");
}
struct LocalSession:Session {
    Context state;explicit LocalSession(uint64_t peer):state{5,peer,1,3,peer==1?Role::Host:Role::Client,snapshot}{}
    bool current(Context& out,std::string& e)override{out=state;e.clear();return true;}
};
struct LocalEngine:Engine {
    research::Row row;Ledger money=initial();Evidence receipt;int executions=0,adoptions=0;Action action;
    bool reject=false,partial=false,wait=false,corrupt_retirement=false;
    explicit LocalEngine(Action a):row(a==Action::StartResearch?baseline():complete_project()),action(a){}
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,4};return true;}
    bool current(const Binding&,const std::string&,std::string&)override{return true;}
    bool ledger(const Binding&,Ledger& out,std::string&)override{out=money;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string& e)override{e="unexpected Remove";return Outcome::RejectedNoEffect;}
    bool observe_remove(const Binding&,const Request&,Evidence&,std::string& e)override{e="unexpected Remove";return false;}
    Outcome execute_action(const Binding&,const Request& r,std::string& e)override{
        need(r.action==action,"wrong Host action");if(reject){e="original Research eligibility rejected";return Outcome::RejectedNoEffect;}
        ++executions;receipt=effect(action);receipt.retirement_confirmed=false;money=receipt.after;row=*receipt.research_after;return Outcome::Confirmed;
    }
    Outcome apply_action(const Binding&,const Request& r,const Evidence& host,std::string& e)override{
        need(r.action==action,"wrong Client action");if(!research::same_state(row,host.research_before)||money!=host.before){e="actual Client BEFORE row/ledger differs";return Outcome::RejectedNoEffect;}
        ++adoptions;money=host.after;const auto local_before=row;
        row=*host.research_after;row.value.release_counter=local_before.value.release_counter+uint32_t(action==Action::CommitResearch);
        receipt=host;receipt.research_before=local_before;receipt.research_after=row;receipt.retirement_confirmed=false;
        if(partial){e="original Client Research readback failed";return Outcome::Unknown;}return Outcome::Confirmed;
    }
    AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string& e)override{if(wait){e="Host BEFORE frame pending";return AuthorityReadiness::Wait;}return AuthorityReadiness::Ready;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}
    bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(corrupt_retirement)receipt.research_after->value.lines++;return true;}
};
struct Packet{uint64_t sender,target;std::vector<uint8_t> bytes;};
struct Wire:Transport {
    uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}
    bool claim(Handler,std::string&)override{return true;}
    bool send(uint64_t target,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,target,bytes});return true;}
};
bool submit(Controller& c,Action action,const std::string& key,uint64_t now,std::string& e){
    if(action==Action::StartResearch)return c.submit_start_research(id,key,now,e);
    if(action==Action::SetResearchWorkers)return c.submit_research_workers(id,key,3,now,e);
    return c.submit_commit_research(id,key,now,e);
}
void flow(Action action,int mode){
    LocalSession hs(1),cs(2);LocalEngine he(action),ce(action);std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);
    he.reject=mode==1;ce.partial=mode==2;ce.wait=mode==3;he.corrupt_retirement=mode==4;
    if(mode==6)ce.row.value.release_counter=560;
    Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    need(host.start(0,e)&&client.start(0,e),e);
    if(mode==5){need(!submit(client,action,"friendslist",0,e)&&packets.empty()&&!ce.adoptions&&!he.executions,"unproven key reached transport/effect");return;}
    need(submit(client,action,"bridges",0,e),e);need(!he.executions&&!ce.adoptions&&ce.money==initial(),"UI intent changed state before Host");
    const auto original_request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing action packet");auto p=packets.front();packets.pop_front();return(p.target==1?host:client).receive(p.sender,p.bytes.data(),p.bytes.size(),1,e);};
    const auto accepted=deliver();
    if(mode==4){need(host.status().locked&&host.status().outcome==Outcome::Unknown&&!host.status().confirmed,"changed retired Research receipt confirmed");return;}
    need(accepted,e);
    if(mode==1){need(deliver(),e);need(!he.executions&&!ce.adoptions&&client.status().outcome==Outcome::RejectedNoEffect&&client.status().cause.find("eligibility")!=std::string::npos,"eligibility rejection lost original cause");return;}
    need(he.executions==1&&!ce.adoptions,"Host failed once-only ordering");const auto result=packets.front();const auto applied=deliver();
    if(mode==2){need(!applied&&ce.adoptions==1&&client.status().locked,"partial adoption escaped Unknown");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"partial effect ACK became success");return;}
    need(applied,e);if(mode==3){need(!ce.adoptions&&packets.empty(),"missing BEFORE frame ACKed");ce.wait=false;need(client.tick(2,e),e);}
    need(ce.adoptions==1&&ce.money==he.money&&research::same_state(ce.row,he.row)&&!client.status().confirmed,"ACK preceded exact Host effects");
    need(deliver(),e);need(deliver(),e);need(host.status().confirmed==1&&client.status().confirmed==1,"required two-peer completion missing");
    need(host.receive(original_request.sender,original_request.bytes.data(),original_request.bytes.size(),3,e),e);
    need(client.receive(result.sender,result.bytes.data(),result.bytes.size(),3,e),e);
    need(he.executions==1&&ce.adoptions==1,"serialized duplicate replayed Research action");need(!submit(client,action,"bridges",4,e),"completed UUID reused");
}
}
int main(){try{plans();codec();frames();for(const auto action:{Action::StartResearch,Action::SetResearchWorkers,Action::CommitResearch})for(int mode=0;mode<7;++mode)flow(action,mode);std::cout<<"PASS "<<checks<<" Research assertions including21 serialized Controller scenarios\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
