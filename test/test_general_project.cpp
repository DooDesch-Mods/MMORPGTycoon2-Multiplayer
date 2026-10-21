#include "general_project_reader.h"
#include "general_project_codec.h"
#include "typed_action/action_session.h"
#include "live_host_state/state.h"
#include "bug_authority/random_result.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
namespace p=mt2mp::game::general_project;namespace a=mt2mp::action_session;
namespace {
int checks=0;void need(bool v,const std::string& e){++checks;if(!v)throw std::runtime_error(e);}
const char* id="20000000-0000-0000-0000-000000000002";const char* snapshot="10000000-0000-0000-0000-000000000001";
p::State initial(){p::State s;p::Catalogue row;row.source={0,10,"Tech",""};row.valid=row.visible=row.choose=row.afford=true;s.catalogue.push_back(row);s.techs.push_back({"Tech",{}});s.random={1,2,3,4};return s;}
std::string hash(const std::string& text){uint64_t n=1469598103934665603;for(auto c:text)n=(n^uint8_t(c))*1099511628211;std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<n;return out.str()+out.str()+out.str()+out.str();}
p::Proof proof(const p::State& s){p::Proof v;std::string e;need(p::proof(s.catalogue[0].source,s,std::string(64,'c'),hash,v,e),e);return v;}
void character_catalogue_effect(){
    auto state=initial();state.progress.current=state.catalogue[0].source;auto before=proof(state),after=before;std::string e;
    p::Intent intent{p::Command::Workers,state.catalogue[0].source,4};after.progress.workers=4;
    need(p::effect(intent,before,after,e),"complete same Character catalogue worker effect: "+e);
    after.character_catalogue_sha256=std::string(64,'d');need(!p::effect(intent,before,after,e),"worker effect hid Character membership change");
    auto wire=p::encode(before);need(p::codec::proof(wire).character_catalogue_sha256==before.character_catalogue_sha256,"complete Character digest absent from wire");
    wire.erase("character_catalogue_sha256");bool rejected=false;try{p::codec::proof(wire);}catch(const std::runtime_error&){rejected=true;}need(rejected,"missing Character digest decoded");
    p::Proof invalid;need(!p::proof(state.catalogue[0].source,state,"",hash,invalid,e),"uncaptured Character catalogue accepted");
    intent.command=p::Command::Commit;intent.workers=0;after=before;after.progress={};after.selected.owned=true;after.selected.tech_flags[1]=1;after.character_catalogue_sha256=std::string(64,'d');
    need(p::effect(intent,before,after,e),"original Commit Character catalogue birth rejected: "+e);
}
void policies(){std::string e;auto before=initial(),after=before;p::Intent i{p::Command::Start,before.catalogue[0].source,0};after.progress.current=i.source;after.catalogue[0].tech_flags[0]=1;after.techs[0].flags[0]=1;
    need(p::transition(i,before,after,e)&&p::effect(i,proof(before),proof(after),e),e);auto bad=after;bad.progress.lines=1;need(!p::transition(i,before,bad,e),"Start accepted non-reset original counter");
    before.progress.current=i.source;before.progress.lines=9;before.progress.work=std::bit_cast<uint32_t>(2.f);after=before;need(p::transition(i,before,after,e),"same source resume reset");
    i.command=p::Command::Workers;i.workers=4;after.progress.workers=4;need(p::transition(i,before,after,e),e);bad=after;++bad.free_points;need(!p::transition(i,before,bad,e),"worker setter hid payment effect");
    i.command=p::Command::Cancel;i.workers=0;after=before;after.progress={};need(p::transition(i,before,after,e)&&p::effect(i,proof(before),proof(after),e),e);bad=after;++bad.random[0];need(!p::transition(i,before,bad,e),"Cancel RNG drift admitted");
    i.command=p::Command::Commit;after.catalogue[0].owned=true;after.catalogue[0].tech_flags[1]=1;after.techs[0].flags[1]=1;need(p::effect(i,proof(before),proof(after),e),e);after.catalogue[0].tech_flags[1]=0;need(!p::effect(i,proof(before),proof(after),e),"missing deployment accepted");
    p::Progress progress;progress.workers=-1;need(!p::valid(progress),"orphan negative workers admitted");progress.current=i.source;need(p::valid(progress),"native zero-staff sentinel rejected");progress.workers=-2;need(!p::valid(progress),"invented negative workers accepted");
    i.command=p::Command::Start;auto wire=p::encode(i);need(p::codec::intent(wire)==i,e);wire["workers"]=uint64_t{1}<<40;bool rejected=false;try{p::codec::intent(wire);}catch(const std::runtime_error&){rejected=true;}need(rejected,"overflow workers decoded");
    wire=p::encode(proof(before));need(p::codec::proof(wire)==proof(before),e);wire["selected"]["afford"]=1;rejected=false;try{p::codec::proof(wire);}catch(const std::runtime_error&){rejected=true;}need(rejected,"integer bool decoded");
}
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;std::map<uintptr_t,std::string> strings;
    template<class T>void put(uintptr_t at,const T& v){const auto* p=reinterpret_cast<const uint8_t*>(&v);for(size_t i=0;i<sizeof v;++i)bytes[at+i]=p[i];}
    template<class T>bool get(uintptr_t at,size_t offset,T& v,std::string& e){auto* p=reinterpret_cast<uint8_t*>(&v);for(size_t i=0;i<sizeof v;++i){auto found=bytes.find(at+offset+i);if(found==bytes.end()){e="fixture exact address missing";return false;}p[i]=found->second;}return true;}
    void array(uintptr_t at,uintptr_t data,const std::vector<uintptr_t>& values){put(at,data);put(at+8,int32_t(values.size()));put(at+12,int32_t(values.size()));for(size_t i=0;i<values.size();++i)put(data+i*8,values[i]);}
};
void effects(){Memory m;constexpr uintptr_t image=0x100000,features=0x2000,values=0x3000,tech=0x4000,mod=0x5000,record=0x6000;auto s=initial();p::EffectOwners owners;std::string e;
    m.put(image+0x13df3c0,features);m.put(features,image+0x16ffdd0);m.put(features+0x27,std::array<uint8_t,10>{});m.put(image+0x13df300,values);m.put(values,image+0x1746a90);
    m.put(values+8,record);m.put(values+0x10,int32_t{1});m.put(values+0x14,int32_t{1});m.put(values+0x20,uintptr_t{0});m.put(values+0x28,int32_t{0});m.put(values+0x2c,int32_t{0});
    m.strings[record]="Value";m.put(record+0x20,uint32_t{3});m.put(record+0x24,uint32_t{6});m.put(record+0x28,image+0x1747a50);m.array(record+0x30,0x7000,{mod+0x44});m.array(tech+0x158,0x8000,{mod});
    m.put(mod,image+0x1700150);m.put(mod+0x40,int32_t{0});m.put(mod+0x44,std::array<uint32_t,2>{1,2});m.put(mod+0x4c,uint8_t{1});m.put(mod+0x4d,uint8_t{1});
    auto read=[&](uintptr_t at,size_t off,auto& v,std::string& x){return m.get(at,off,v,x);};auto text=[&](uintptr_t at,std::string& v,std::string& x){if(!m.strings.contains(at)){x="fixture string missing";return false;}v=m.strings.at(at);return true;};std::map<std::string,uintptr_t> keys{{"Tech",tech}};
    need(p::effects(read,text,image,keys,s,owners,e)&&p::valid(s),e);need(s.values[0][0].current==6&&s.techs[0].mods[0].active&&owners.modifiers==std::vector<uintptr_t>{mod},"actual effects not captured");
    const auto prior=proof(s);s.features[0]=1;need(proof(s).ownership_sha256!=prior.ownership_sha256,"Feature absent from ownership proof");s.features[0]=0;
    m.put(mod+0x4d,uint8_t{0});need(!p::effects(read,text,image,keys,s,owners,e)&&e.find("membership")!=std::string::npos,"inactive modifier hid active GameValue membership");m.put(mod+0x4d,uint8_t{1});m.put(mod+0x40,int32_t{2});need(!p::effects(read,text,image,keys,s,owners,e),"out-of-array GameValue target accepted");m.put(mod+0x40,int32_t{0});m.put(values,image+0x1746a98);need(!p::effects(read,text,image,keys,s,owners,e),"wrong actual GameValues class accepted");
}
void codec_frame(){std::string e;a::Message m,decoded;m.session=77;m.request.action=a::Action::GeneralProject;m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.peer=2;m.request.sequence=1;m.request.canonical_record_key="development:project";m.request.general_project={p::Command::Start,initial().catalogue[0].source,0};std::vector<uint8_t> bytes;
    need(a::encode(m,bytes,e)&&a::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);need(nlohmann::json::parse(bytes)["schema"]==35,"General project divergent schema");
    namespace live=mt2mp::game::live_host_state;live::Frame frame;frame.session=77;frame.epoch=3;frame.source_generation=9;frame.revision=1;frame.snapshot=snapshot;frame.for_action=id;frame.values.calendar={1,1,1,0,0,7};frame.management=mt2mp::game::management_simulation::LiveState{};
    auto state=initial();state.progress.current=state.catalogue[0].source;state.progress.workers=-1;frame.management->project=state.progress;a::Message result;result.kind=a::Kind::HostResult;result.outcome=a::Outcome::Confirmed;result.session=77;result.revision=1;result.request=m.request;result.evidence.before=live::ledger(frame.values);result.evidence.general_project_before=proof(state);a::Context client{77,2,1,3,a::Role::Client,snapshot};
    need(live::admit_before_result(1,client,3,9,nullptr,frame,result,0,0,e)==live::Admission::New,e);++frame.management->project.lines;need(live::admit_before_result(1,client,3,9,nullptr,frame,result,0,0,e)==live::Admission::Rejected,"pending ActionBefore hid wrong project progress");
}
void bugs(){namespace b=mt2mp::game::bug_authority;b::Lifetimes life;std::string e;int births=0;b::Local row;row.collection=20;row.bug=30;row.row.birth={b::BirthKind::Action,id,1,1};
    auto born=[&](std::string& x){++births;return life.born(row,x);};
    need(b::random_result(20,30,{},std::vector<uintptr_t>{30},life,born,e)&&births==1,e);
    need(b::random_result(20,30,std::vector<uintptr_t>{30},std::vector<uintptr_t>{30},life,born,e)&&births==1,"original MakeWorse duplicated birth");
    need(!b::random_result(21,30,std::vector<uintptr_t>{30},std::vector<uintptr_t>{30},life,born,e),"wrong current collection admitted");
    need(!b::random_result(20,30,std::vector<uintptr_t>{30},std::vector<uintptr_t>{30,30},life,born,e),"duplicate original pointer admitted");
    need(!b::random_result(20,0,std::vector<uintptr_t>{30},std::vector<uintptr_t>{},life,born,e),"null result membership changed");
    need(b::random_result(20,0,std::vector<uintptr_t>{30},std::vector<uintptr_t>{30},life,born,e),e);
    need(life.remove(30,e),e);need(!b::random_result(20,30,std::vector<uintptr_t>{30},std::vector<uintptr_t>{30},life,born,e),"retired pointer strengthened without new occurrence");
}
struct Session:a::Session {a::Context c;Session(uint64_t peer):c{77,peer,1,3,peer==1?a::Role::Host:a::Role::Client,snapshot}{}bool current(a::Context& out,std::string&)override{out=c;return true;}};
struct Engine:a::Engine {
    p::State state=initial();a::Ledger money;a::Evidence receipt;int executions=0;bool partial=false;
    Engine(){state.progress.current=state.catalogue[0].source;money.record_present=1;money.cash=money.record_cash=20000;}
    bool bind(const std::string&,a::Binding& out,std::string&)override{out={3,9};return true;}bool current(const a::Binding&,const std::string&,std::string&)override{return true;}bool ledger(const a::Binding&,a::Ledger& out,std::string&)override{out=money;return true;}
    a::Outcome execute_remove(const a::Binding&,const a::Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}a::Outcome apply_remove(const a::Binding&,const a::Request&,const a::Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}bool observe_remove(const a::Binding&,const a::Request&,a::Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    a::Outcome execute_action(const a::Binding&,const a::Request& r,std::string&)override{++executions;receipt={};receipt.before=receipt.after=money;receipt.general_project_before=proof(state);state.progress.workers=r.general_project.workers;receipt.general_project_after=proof(state);return a::Outcome::Confirmed;}
    a::Outcome apply_action(const a::Binding& b,const a::Request& r,const a::Evidence& authority,std::string& e)override{if(receipt.general_project_before)return a::Outcome::Unknown;auto out=execute_action(b,r,e);if(partial){e="fixture original interrupted";return a::Outcome::Unknown;}need(receipt.general_project_after==authority.general_project_after,"actual Client compact after differs");return out;}
    bool observe_action(const a::Binding&,const a::Request&,a::Evidence& out,std::string&)override{out=receipt;return true;}bool retire(const a::Binding&,const a::Request&,std::string&)override{receipt.retirement_confirmed=true;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};struct Wire:a::Transport {uint64_t peer;std::deque<Packet>& q;Wire(uint64_t p,std::deque<Packet>& packets):peer(p),q(packets){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{q.push_back({peer,to,bytes});return true;}};
void controller(bool partial){Session hs(1),cs(2);Engine he,ce;ce.partial=partial;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);a::Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;need(host.start(0,e)&&client.start(0,e),e);
    p::Intent i{p::Command::Workers,he.state.catalogue[0].source,3};need(client.submit_general_project(id,i,0,e),e);need(!he.executions&&!ce.executions,"Client source effect ran before Host ordering");const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing ordered packet");auto m=packets.front();packets.pop_front();return(m.to==1?host:client).receive(m.from,m.bytes.data(),m.bytes.size(),1,e);};need(deliver(),e);const auto result=packets.front();
    if(partial){need(!deliver()&&client.status().locked&&!client.status().confirmed,"Unknown native effect ACKed");need(deliver()&&host.status().locked&&!host.status().confirmed,"Host accepted Unknown Client");return;}
    need(deliver()&&deliver()&&deliver(),e);need(host.status().ack_mask==3&&host.status().confirmed==1&&client.status().confirmed==1&&he.state==ce.state,"complete original/ACK3 absent");
    need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,e)&&client.receive(result.from,result.bytes.data(),result.bytes.size(),2,e),e);need(he.executions==1&&ce.executions==1,"duplicate ordered delivery replayed original");need(!client.submit_general_project(id,i,3,e),"consumed original UUID replayed");
}
}
int main(){try{character_catalogue_effect();policies();effects();codec_frame();bugs();controller(false);controller(true);std::cout<<checks<<" General project actual state/codec/effect checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
