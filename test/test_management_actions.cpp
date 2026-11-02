#include "typed_action/action_session.h"
#include "live_host_state/state.h"
#include "typed_action/tick_evidence.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::action_session;
namespace {
int assertions=0;void need(bool value,const std::string& e){++assertions;if(!value)throw std::runtime_error(e);}
const char* snapshot="10000000-0000-0000-0000-000000000001";
const char* token="20000000-0000-0000-0000-000000000002";
ManagementState initial(){ManagementState s;s.game_masters=2;s.release_funds=100;s.statistics_points=2;s.statistics.fill(4);s.purchased.fill(1);s.campaign_costs={500,1000,3000,500,-1,-1,-1,-1,-1};s.offers.push_back({{"loan","title","description"},{1000,1200,10},0,true});return s;}
Ledger money(){Ledger l;l.record_present=1;l.cash=l.record_cash=20000;return l;}
void policy(){
    auto b=initial(),a=b;ManagementIntent i;i.op=ManagementOp::HireGameMaster;++a.game_masters;need(management_transition(i,b,a),"GM hire");
    need(ordered_management(i),"GM hire missing ordered admission");i.op=ManagementOp::FireGameMaster;need(ordered_management(i),"GM fire missing ordered admission");i.op=ManagementOp::SpendStatistics;need(!ordered_management(i),"unrelated Management operation admitted");i.op=ManagementOp::HireGameMaster;
    ++a.statistics[0];need(!management_transition(i,b,a),"unrelated statistics drift accepted");
    i.op=ManagementOp::FireGameMaster;a=b;--a.game_masters;need(management_transition(i,b,a),"GM fire");
    i.op=ManagementOp::AllocateReleaseFunds;i.value=500;a=b;a.release_funds=500;need(management_transition(i,b,a),"release allocate");
    Ledger after;need(management_ledger(i,b,money(),after)&&after.cash==19600&&after.line_income[18]==-400&&after.line_counts[18]==1,"funds full Ledger");
    auto poor=money();poor.cash=399;need(!management_ledger(i,b,poor,after),"insufficient funds");
    auto expected=money();expected.cash-=400;expected.record_cash-=400;expected.record_expense+=400;expected.line_counts[18]++;expected.line_income[18]-=400;
    need(management_ledger(i,b,money(),after)&&after==expected,"increase preserves complete ledger");
    i.value=40;a=b;a.release_funds=40;expected=money();expected.cash+=60;expected.record_cash+=60;expected.record_income+=60;expected.line_counts[18]++;expected.line_income[18]+=60;
    need(management_transition(i,b,a)&&management_ledger(i,b,money(),after)&&after==expected,"refund preserves complete ledger");
    ++a.statistics[0];need(!management_transition(i,b,a),"funds changed unrelated management state");
    auto overflow=money();overflow.cash=INT32_MAX;need(!management_ledger(i,b,overflow,after),"refund overflow");
    i={};i.op=ManagementOp::SpendStatistics;i.statistics[0]=3;a=b;a.spent[0]=3;a.statistics[0]=7;a.statistics_points=0;a.purchased_points=1;a.purchased[0]=0;a.character_slots={4,4,4};
    need(management_transition(i,b,a),"statistics free then purchased accounting");++a.statistics_points;need(!management_transition(i,b,a),"wrong free statistics count");
    i={};i.op=ManagementOp::TakeLoan;i.key="loan";a=b;a.loans.push_back({b.offers[0].text,{1000,100,20,10}});a.offers.clear();need(management_transition(i,b,a),"take native loan membership");
    need(ordered_management(i)&&management_loan_ready(i,b),"available Loan not admitted");auto invalid_offer=b;invalid_offer.offers[0].available=false;need(!management_loan_ready(i,invalid_offer),"unavailable Loan admitted");invalid_offer=b;invalid_offer.offers.push_back(invalid_offer.offers[0]);need(!management_loan_ready(i,invalid_offer),"duplicate Loan offer admitted");
    need(management_ledger(i,b,money(),after)&&after.cash==21000&&after.line_income[17]==1000&&after.line_counts[17]==1,"take native category17");
    auto malformed=a;malformed.loans[0].amounts[2]=21;need(!management_transition(i,b,malformed),"invented loan interest");
    auto no_offer=b;no_offer.offers[0].available=false;need(!management_transition(i,no_offer,a),"unavailable loan");
    auto duplicate=b;duplicate.offers.push_back(duplicate.offers[0]);need(!management_transition(i,duplicate,a),"ambiguous offer key");
    i.op=ManagementOp::RepayLoan;b=a;a.loans.clear();need(ordered_management(i)&&management_loan_ready(i,b),"repay Loan not admitted");need(management_transition(i,b,a),"repay native erase");need(management_ledger(i,b,money(),after)&&after.cash==19000&&after.line_income[15]==-1000,"repay category15");
    need(!management_valid(ManagementIntent{ManagementOp(0)}),"unknown operation");
    i={};i.op=ManagementOp::StartCampaign;i.value=0;b=initial();b.campaign_costs[0]=100;a=b;
    a.marketing["campaigns"].push_back({{"customName",""},{"type",0},{"startedOn",1},{"duration",7},{"impressions",0},{"conversions",0},{"hype",0},{"active",1}});
    need(management_transition(i,b,a),"advertisement normal append");need(management_ledger(i,b,money(),after)&&after.cash==19300&&after.line_income[17]==-700,"advertisement native seven-day category17 payment");
    need(ordered_management(i)&&management_campaign_ready(i,b),"available Campaign not admitted");auto bad_day=money();bad_day.flat_day=1;need(ordered_campaign_effect(i,b,a,bad_day),"native Campaign current-day effect rejected");++bad_day.flat_day;need(!ordered_campaign_effect(i,b,a,bad_day),"wrong Campaign start day admitted");auto bad_row=a;bad_row.marketing["campaigns"][0]["impressions"]=1;bad_day.flat_day=1;need(!ordered_campaign_effect(i,b,bad_row,bad_day),"nonzero initial Campaign counters admitted");
    auto disabled=b;disabled.campaign_costs[0]=-1;auto disabled_after=a;disabled_after.campaign_costs[0]=-1;need(!management_transition(i,disabled,disabled_after)&&!management_ledger(i,disabled,money(),after),"disabled campaign admitted");disabled.campaign_costs[0]=0;disabled_after.campaign_costs[0]=0;need(!management_transition(i,disabled,disabled_after)&&!management_ledger(i,disabled,money(),after),"zero-cost campaign admitted");disabled.campaign_costs[0]=-2;need(!management_valid(disabled),"unsupported negative campaign cost admitted");
    a.marketing["positiveBuzz"]=1;need(!management_transition(i,b,a),"unrelated Marketing state changed");
    i={};i.op=ManagementOp::ToggleReleaseKind;b=initial();b.release_present=true;b.release_type=2;b.software_version={1,3,2};b.planned_version={1,4,0};a=b;a.release_type=3;a.planned_version={2,0,0};
    need(management_transition(i,b,a),"original minor to major version");a.planned_version[1]=1;need(!management_transition(i,b,a),"wrong planned major version");
    b=initial();b.services=ManagementServices{"action:building",{{1,"Inn",{0,10},true,true}},true,{{"Inn",{0,10}}}};
    i={};i.op=ManagementOp::SetServicePrice;i.key="action:building";i.service_ordinal=0;i.currency=1;i.value=20;a=b;a.services->rows[0].price={1,20};a.services->configs[0].price={1,20};
    need(management_transition(i,b,a),"service price and shared config");a.services->configs[0].price={0,10};need(!management_transition(i,b,a),"synced config effect omitted");
    i={};i.op=ManagementOp::SetServiceSync;i.key="action:building";i.service_ordinal=0;i.value=1;b.services->rows[0].synced=false;b.services->configs[0].price={1,50};a=b;a.services->rows[0].synced=true;a.services->rows[0].price={1,50};need(management_transition(i,b,a),"service enable sync adopts existing config");
    need(ordered_management(i)&&management_service_sync_ready(i,b),"service sync ordered admission failed");auto unavailable=b;unavailable.services->rows[0].allow_sync=false;need(!management_service_sync_ready(i,unavailable),"disabled sync admitted");unavailable=b;unavailable.services->rows[0].synced=true;need(!management_service_sync_ready(i,unavailable),"unchanged sync admitted");
    i.key="action:other";need(!management_transition(i,b,a),"foreign service building accepted");
    i={};i.op=ManagementOp::PurchaseStatistic;i.value=0;b=initial();b.purchased[0]=0;a=b;a.purchased[0]=1;
    need(management_transition(i,b,a),"native statistic purchase flag");need(management_ledger(i,b,money(),after)&&after.cash==12000&&after.line_income[5]==-8000,"native statistic purchase category5");
}
struct S:Session {Context c;S(uint64_t p):c{77,p,1,3,p==1?Role::Host:Role::Client,snapshot}{}bool current(Context& out,std::string&)override{out=c;return true;}};
struct E:Engine {
    ManagementState state=initial();Ledger cash=money();Evidence receipt;int executed=0,applied=0;bool partial=false,drift=false,retirement_drift=false,reject=false;
    bool bind(const std::string&,Binding& out,std::string&)override{out={3,9};return true;}bool current(const Binding&,const std::string&,std::string&)override{return true;}bool ledger(const Binding&,Ledger& out,std::string&)override{out=cash;return true;}
    Outcome execute_remove(const Binding&,const Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}Outcome apply_remove(const Binding&,const Request&,const Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}bool observe_remove(const Binding&,const Request&,Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    Outcome execute_action(const Binding&,const Request& q,std::string& e)override{need(q.action==Action::ManagementCommand,"wrong routed Host action");if(reject){e="actual Lounge cannot hire";return Outcome::RejectedNoEffect;}++executed;receipt={};receipt.before=cash;receipt.management_before=state;++state.game_masters;receipt.management_after=state;receipt.after=cash;return Outcome::Confirmed;}
    Outcome apply_action(const Binding&,const Request&,const Evidence& host,std::string& e)override{need(state==host.management_before&&cash==host.before,"Client before not authoritative");++applied;receipt=host;receipt.retirement_confirmed=false;++state.game_masters;if(drift){++state.release_funds;receipt.management_after=state;}if(partial){e="original interrupted";return Outcome::Unknown;}return Outcome::Confirmed;}
    bool observe_action(const Binding&,const Request&,Evidence& out,std::string&)override{out=receipt;return true;}bool retire(const Binding&,const Request&,std::string&)override{receipt.retirement_confirmed=true;if(retirement_drift)++receipt.management_after->release_funds;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};
struct W:Transport {uint64_t peer;std::deque<Packet>& packets;W(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{packets.push_back({peer,to,bytes});return true;}};
void flow(int mode){S hs(1),cs(2);E he,ce;std::deque<Packet> packets;W hw(1,packets),cw(2,packets);Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;
    ce.partial=mode==1;ce.drift=mode==2;he.retirement_drift=mode==3;he.reject=mode==4;need(host.start(0,e)&&client.start(0,e),e);ManagementIntent intent;
    need(client.submit_management(token,intent,0,e),e);need(!he.executed&&!ce.applied,"capture ran Client effect before ordering");auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing packet");auto p=packets.front();packets.pop_front();return(p.to==1?host:client).receive(p.from,p.bytes.data(),p.bytes.size(),1,e);};
    bool ok=deliver();if(mode==3){need(host.status().locked&&!host.status().confirmed&&host.status().cause.find("retirement")!=std::string::npos,"changed retirement admitted");return;}need(ok,e);
    if(mode==4){need(deliver(),e);need(!he.executed&&!ce.applied&&client.status().outcome==Outcome::RejectedNoEffect,"no-effect original cause lost");return;}
    auto result=packets.front();ok=deliver();if(mode==1||mode==2){need(!ok&&client.status().locked&&!client.status().confirmed,"partial/drift Client ACKed");need(deliver(),e);need(host.status().locked&&!host.status().confirmed,"failed ACK became success");return;}
    need(deliver()&&deliver(),e);need(host.status().ack_mask==3&&host.status().confirmed==1&&client.status().confirmed==1&&ce.state==he.state,"full ACK/state absent");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,e)&&client.receive(result.from,result.bytes.data(),result.bytes.size(),2,e),e);need(he.executed==1&&ce.applied==1,"duplicate replayed native original");need(!client.submit_management(token,intent,3,e),"consumed token resubmitted");
}
void codec(){Message m;m.session=77;m.request.action=Action::ManagementCommand;m.request.peer=2;m.request.sequence=1;m.request.action_uuid=token;m.request.snapshot_uuid=snapshot;m.request.canonical_record_key="management:business";std::vector<uint8_t> bytes;std::string e;Message round;
    need(encode(m,bytes,e)&&decode(bytes.data(),bytes.size(),round,e)&&round==m,e);auto j=nlohmann::json::parse(bytes);need(j.at("schema")==35,"wrong coherent schema");
    j["request"]["management"]["op"]=12;auto invalid=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(invalid.data()),invalid.size(),round,e),"unknown operation decoded");
    j=nlohmann::json::parse(bytes);j["request"]["management"]["value"]=1;invalid=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(invalid.data()),invalid.size(),round,e),"foreign argument admitted");
}
void ordered_price_evidence(){
    namespace tick=mt2mp::action_session::tick_commands;Request q;q.action=Action::ManagementCommand;q.peer=2;q.sequence=1;q.action_uuid=token;q.snapshot_uuid=snapshot;q.canonical_record_key="management:business";
    q.management.op=ManagementOp::SetServicePrice;q.management.key="action:building";q.management.service_ordinal=0;q.management.currency=0;q.management.value=11;
    Context context{77,1,1,3,Role::Host,snapshot};tick::Assigned assigned{1,{77,snapshot,q}};Evidence proof;proof.before=proof.after=money();proof.management_before=initial();
    proof.management_before->services=ManagementServices{"action:building",{{1,"Inn",{0,10},false,true}},false,{}};
    need(management_service_price_ready(q.management,*proof.management_before),"actual unsynced price owner rejected");auto no_manager=*proof.management_before;no_manager.services->rows[0].synced=true;need(!management_service_price_ready(q.management,no_manager),"synced price without original config manager admitted");
    proof.management_before->services->price=ManagementPriceWitness{std::string(64,'a'),{1,2,3,4}};proof.management_after=*proof.management_before;
    proof.management_after->services->rows[0].price={0,11};proof.management_after->services->price=ManagementPriceWitness{std::string(64,'b'),{1,2,3,5}};proof.retirement_confirmed=true;
    auto encode_proof=[&](const Evidence& value,std::string& digest,std::string& error){tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=Outcome::Confirmed;receipt.evidence=std::make_shared<Evidence>(value);return tick::canonical_evidence(context,assigned,receipt,digest,error);};
    std::string digest,changed,error;need(tick::requires_evidence(assigned.input)&&encode_proof(proof,digest,error),error);
    auto missing=proof;missing.management_after->services->price.reset();need(!encode_proof(missing,changed,error),"ordered ServicePrice accepted missing graph/RNG evidence");
    auto altered=proof;altered.management_after->services->price->random[3]++;need(encode_proof(altered,changed,error)&&changed!=digest,"ServicePrice RNG change omitted from canonical evidence");
    altered=proof;altered.management_after->services->price->random_extra++;need(encode_proof(altered,changed,error)&&changed!=digest,"ServicePrice RNG extra omitted from canonical evidence");
    altered=proof;altered.management_after->services->price->graph_sha256=std::string(64,'c');need(encode_proof(altered,changed,error)&&changed!=digest,"ServicePrice graph change omitted from canonical evidence");
    altered=proof;altered.management_before->services->price->graph_sha256=std::string(64,'G');need(!encode_proof(altered,changed,error),"invalid ServicePrice graph digest accepted");
    need(service_price_wire_fits(q,*proof.management_before,proof.before,{},error),error);
    auto bounded=*proof.management_before;bounded.services->config_present=true;std::vector<std::string> many;for(int i=0;i<300;++i)many.push_back(std::string(240,'x')+std::to_string(i));
    need(!service_price_wire_fits(q,bounded,proof.before,many,error),"ServicePrice oversized complete receipt admitted");
}
void ordered_evidence(ManagementOp op=ManagementOp::AllocateReleaseFunds){
    namespace tick=mt2mp::action_session::tick_commands;
    Request q;q.action=Action::ManagementCommand;q.peer=2;q.sequence=1;q.action_uuid=token;q.snapshot_uuid=snapshot;q.canonical_record_key="management:business";q.management.op=op;q.management.value=op==ManagementOp::AllocateReleaseFunds?500:0;if(op==ManagementOp::TakeLoan||op==ManagementOp::RepayLoan)q.management.key="loan";if(op==ManagementOp::SetServiceSync){q.management.key="action:building";q.management.service_ordinal=0;q.management.value=1;}
    Context context{77,1,1,3,Role::Host,snapshot};tick::Assigned assigned{1,{77,snapshot,q}};
    Evidence proof;proof.before=money();proof.management_before=initial();proof.management_before->campaign_costs={500,1000,3000,500,-1,-1,-1,-1,-1};if(op==ManagementOp::RepayLoan)proof.management_before->loans.push_back({{"loan","title","description"},{1000,100,20,10}});if(op==ManagementOp::SetServiceSync)proof.management_before->services=ManagementServices{"action:building",{{1,"Inn",{0,10},false,true}},true,{{"Inn",{0,20}}}};proof.management_after=*proof.management_before;if(op==ManagementOp::SetServiceSync){proof.management_after->services->rows[0].synced=true;proof.management_after->services->rows[0].price={0,20};}else if(op==ManagementOp::AllocateReleaseFunds)proof.management_after->release_funds=500;else if(op==ManagementOp::HireGameMaster||op==ManagementOp::FireGameMaster)proof.management_after->game_masters+=op==ManagementOp::HireGameMaster?1:-1;else if(op==ManagementOp::TakeLoan){proof.management_after->loans.push_back({proof.management_before->offers[0].text,{1000,100,20,10}});proof.management_after->offers.clear();}else if(op==ManagementOp::RepayLoan)proof.management_after->loans.clear();else proof.management_after->marketing["campaigns"].push_back({{"customName",""},{"type",0},{"startedOn",0},{"duration",7},{"impressions",0},{"conversions",0},{"hype",0},{"active",1}});need(ordered_management_ledger(q.management,*proof.management_before,proof.before,proof.after),"ordered evidence projection");proof.retirement_confirmed=true;
    auto digest=[&](const Evidence& value){tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=Outcome::Confirmed;receipt.evidence=std::make_shared<Evidence>(value);std::string result,e;need(tick::canonical_evidence(context,assigned,receipt,result,e),e);return result;};
    if(op==ManagementOp::TakeLoan||op==ManagementOp::RepayLoan){auto other=proof;if(op==ManagementOp::TakeLoan){other.management_before->offers[0].text[1]="different title";other.management_after->loans.back().text[1]="different title";}else other.management_before->loans[0].text[1]="different title";need(digest(other)!=digest(proof),"different Loan/Offer evidence digest accepted");}
    if(op==ManagementOp::HireGameMaster){auto uncharged=proof;uncharged.after=uncharged.before;tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=Outcome::Confirmed;receipt.evidence=std::make_shared<Evidence>(uncharged);std::string result,error;need(!tick::canonical_evidence(context,assigned,receipt,result,error),"uncharged ordered Hire evidence accepted");}
    const auto baseline=digest(proof);auto changed=proof;if(op==ManagementOp::AllocateReleaseFunds)changed.management_before->release_funds=101;else {++changed.management_before->game_masters;++changed.management_after->game_masters;}need(ordered_management_ledger(q.management,*changed.management_before,changed.before,changed.after),"changed Management projection");need(digest(changed)!=baseline,"different Management digest");
    auto check=[&](auto mutate){auto value=proof;mutate(value.before);if(op==ManagementOp::StartCampaign)value.management_after->marketing["campaigns"].back()["startedOn"]=value.before.flat_day;need(ordered_management_ledger(q.management,*value.management_before,value.before,value.after),"changed ledger projection");need(digest(value)!=baseline,"different ledger digest");};
    check([](auto& l){++l.cash;});check([](auto& l){++l.flat_day;});check([](auto& l){++l.record_day;});check([](auto& l){++l.record_cash;});check([](auto& l){++l.record_income;});check([](auto& l){++l.record_expense;});
    for(size_t n=0;n<20;++n){check([&](auto& l){++l.line_counts[n];});check([&](auto& l){++l.line_income[n];});}
    auto config=[&](uint64_t peer){tick::Config c;c.context={77,peer,1,3,peer==1?Role::Host:Role::Client,snapshot};return c;};auto validate=[](const tick::Input&,std::string&){return true;};tick::Queue host(config(1),validate),client(config(2),validate);tick::Assigned queued;tick::Batch batch;tick::Receipt h,c;std::string e;
    need(host.offer(2,assigned.input,queued,e)==tick::Admission::Accepted&&host.seal(batch,e)&&client.admit(1,batch,e)==tick::Admission::Accepted,e);
    auto current=[](std::string&){return true;};auto advance=[](uint64_t,std::string&){return Outcome::Confirmed;};auto consume=[&](tick::Queue& queue,tick::Receipt& out,const std::string& value){return queue.consume(1,current,[&](const auto& command,std::string& error){return queue.bind_evidence(command,value,error)?Outcome::Confirmed:Outcome::Unknown;},advance,out,e);};
    tick::Queue equal_host(config(1),validate),equal_client(config(2),validate);tick::Assigned equal_queued;tick::Batch equal_batch;tick::Receipt equal_h,equal_c;need(equal_host.offer(2,assigned.input,equal_queued,e)==tick::Admission::Accepted&&equal_host.seal(equal_batch,e)&&equal_client.admit(1,equal_batch,e)==tick::Admission::Accepted&&consume(equal_host,equal_h,baseline)&&consume(equal_client,equal_c,baseline)&&equal_host.acknowledge(2,equal_c,e),"matching Management evidence ACK failed: "+e);
    need(consume(host,h,baseline)&&consume(client,c,digest(changed)),e);need(!host.acknowledge(2,c,e)&&host.state().locked,"different management evidence ACKed");
}
void live_frame(){
    namespace m=mt2mp::game::management_simulation;namespace live=mt2mp::game::live_host_state;
    std::string e;auto a=m::project(initial()),b=a;b.game_masters=3;b.free_points=1;b.statistics[0]=5;b.spent[0]=1;b.purchased[0]=0;b.character_slots[1]=7;
    std::vector<m::Change> changes;need(m::plan(a,b,changes,e),e);need(changes==std::vector<m::Change>{{0,2,3},{2,2,1},{4,4,5},{14,0,1},{24,1,0},{42,0,7}},"Management scalar offsets/order differ");
    auto bad=b;bad.release_present=true;bad.release_type=2;need(!m::plan(a,bad,changes,e),"unbound new Release admitted");
    bad=b;bad.purchased[0]=2;need(!m::valid(bad,e),"purchase byte domain");
    auto pool=a;pool.gm_employment.resize(50);auto hired=pool;hired.lounge_employed=1;hired.gm_employment[0]={1,1};need(m::plan(pool,hired,changes,e)&&changes==std::vector<m::Change>{{44,0,1},{45,0,1},{46,0,1}},"native fixed-slot employment plan");
    hired.gm_employment[1][0]=1;need(!m::valid(hired,e),"noncanonical native employment slot");
    hired=pool;hired.gm_employment.clear();need(!m::plan(pool,hired,changes,e),"Lounge pool identity silently replaced");
    auto j=m::encode(b);m::LiveState result;need(m::decode(j,result,e)&&result==b,e);j["purchased"][0]=256;need(!m::decode(j,result,e),"truncated purchase byte accepted");
    j=m::encode(b);j["extra"]=1;need(!m::decode(j,result,e),"unknown live field");
    live::Frame frame;frame.session=77;frame.epoch=3;frame.source_generation=9;frame.revision=1;frame.snapshot=snapshot;frame.values.calendar={1,1,1,0,0,7};frame.management=a;
    std::vector<uint8_t> bytes;live::Frame decoded;need(live::encode(frame,bytes,e)&&live::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==frame,e);need(nlohmann::json::parse(bytes)["schema"]==19,"Management component not carried by complete Frame");
    auto malformed=nlohmann::json::parse(bytes);malformed["management"]=nullptr;auto text=malformed.dump();need(!live::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"armed Management component null");
    malformed=nlohmann::json::parse(bytes);malformed["schema"]=18;malformed.erase("management");text=malformed.dump();need(!live::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"legacy schema18 mandatory Monster component weakened");
    Context client{77,2,1,3,Role::Client,snapshot};Message host;host.kind=Kind::HostResult;host.outcome=Outcome::Confirmed;host.session=77;host.revision=1;host.request.snapshot_uuid=snapshot;host.request.action_uuid=token;host.evidence.before=live::ledger(frame.values);host.evidence.management_before=initial();frame.for_action=token;
    need(live::admit_before_result(1,client,3,9,nullptr,frame,host,0,0,e)==live::Admission::New,e);frame.management->free_points=0;
    need(live::admit_before_result(1,client,3,9,nullptr,frame,host,0,0,e)==live::Admission::Rejected,"pending action-before Management drift accepted");
    frame.management.reset();need(live::admit_before_result(1,client,3,9,nullptr,frame,host,0,0,e)==live::Admission::Rejected,"pending Management action lacked complete before component");
}
}
int main(){try{policy();codec();ordered_price_evidence();ordered_evidence();ordered_evidence(ManagementOp::HireGameMaster);ordered_evidence(ManagementOp::FireGameMaster);ordered_evidence(ManagementOp::TakeLoan);ordered_evidence(ManagementOp::RepayLoan);ordered_evidence(ManagementOp::StartCampaign);ordered_evidence(ManagementOp::SetServiceSync);live_frame();for(int n=0;n<5;++n)flow(n);std::cout<<assertions<<" management policy/codec/Controller/Frame assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
