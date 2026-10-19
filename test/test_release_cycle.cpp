#include "release_cycle_state.h"
#include "typed_action/action_session.h"
#include "live_host_state/state.h"
#include "mt2mp/core/hash.h"
#include <deque>
#include <iostream>
namespace c=mt2mp::game::release_cycle;namespace a=mt2mp::action_session;namespace live=mt2mp::game::live_host_state;namespace g=mt2mp::game::release_graph;
namespace {
unsigned checks=0;void need(bool yes,const std::string& why){++checks;if(!yes)throw std::runtime_error(why);}
const std::string snapshot="10000000-0000-0000-0000-000000000001",birth="20000000-0000-0000-0000-000000000002";
std::string digest(const std::string& value){return mt2mp::sha256(std::vector<unsigned char>(value.begin(),value.end()));}
g::Value graph(g::Kind kind){g::Value out;out.kind=kind;out.records.push_back({SIZE_MAX,0,{1,0,kind==g::Kind::Manager?"mmoReleaseManager":"vsObjectArray<mmoReleaseDemand>"},{}});return out;}
c::Publication initial(){c::Publication out;out.graph=graph(g::Kind::Manager);out.prepared=graph(g::Kind::PreparedDemands);out.random={1,2,3,4};return out;}
mt2mp::game::release_edit::State scalar(){mt2mp::game::release_edit::State out;out.version={1,2,3};out.planned={2,0,0};out.type=3;out.random={1,2,3,4};return out;}
c::Proof proof(const mt2mp::game::release_edit::State& s,const c::Publication& p,int32_t points){c::Proof out;std::string e;need(c::proof(s,p,points,digest,out,e),e);return out;}
void policy(){std::string e;auto s=scalar();auto value=initial();auto b=proof(s,value,5);
    need(c::codec::publication(c::encode(value))==value,"typed prepared/Manager publication roundtrip");
    need(c::publication_matches(b,value,digest,e),e);
    auto mismatched=value;mismatched.random[0]++;need(!c::publication_matches(b,mismatched,digest,e),"ordered Host RNG mismatch rejected before native effects");
    mismatched=value;mismatched.cycle=birth;mismatched.phase=c::Phase::Announcement;need(!c::publication_matches(b,mismatched,digest,e),"ordered Host cycle/phase mismatch rejected before native effects");
    mismatched=value;mismatched.graph.records[0].tokens.push_back({});need(!c::publication_matches(b,mismatched,digest,e),"ordered Host complete graph mismatch rejected before native effects");
    mismatched=value;mismatched.prepared.records[0].tokens.push_back({});need(!c::publication_matches(b,mismatched,digest,e),"ordered Host prepared value mismatch rejected before native effects");

    auto wrong=c::encode(value);wrong["random"][0]=true;bool rejected=false;try{c::codec::publication(wrong);}catch(const std::exception&){rejected=true;}need(rejected,"bool RNG rejected");
    c::Intent show{c::Command::Show,birth};auto after=b;after.cycle=birth;after.phase=c::Phase::Announcement;after.random[0]=7;after.predicate_random=after.random;
    need(c::effect(show,b,after,e),e);auto denied=b;denied.allowed=false;denied.random[0]=7;denied.predicate_random=denied.random;need(c::effect(show,b,denied,e),"denied original predicate may consume retained RNG");denied.random[1]++;need(!c::effect(show,b,denied,e),"unrecorded denied RNG rejected");
    c::Intent begin{c::Command::Begin,birth};auto playing=after;playing.phase=c::Phase::Playing;need(c::effect(begin,after,playing,e),e);
    auto altered=playing;altered.graph_sha256=digest("foreign authored state");need(!c::effect(begin,after,altered,e),"Begin may not change authored graph");
    c::Intent skip{c::Command::Skip,birth};auto waiting=playing;waiting.phase=c::Phase::AwaitPublish;need(c::effect(skip,playing,waiting,e),e);
    c::Intent finalize{c::Command::Finalize,birth};auto published=waiting;published.phase=c::Phase::Idle;published.cycle.clear();published.version=waiting.planned;published.history++;published.free_points+=4;
    need(c::effect(finalize,waiting,published,e),e);auto partial=published;partial.free_points--;need(!c::effect(finalize,waiting,partial,e),"major native statistics delta remains exact");partial=published;partial.history--;need(!c::effect(finalize,waiting,partial,e),"full original history append required");need(!c::before(finalize,playing,e),"publish before natural/Skip boundary rejected");
    c::Intent abort{c::Command::Abort,birth};auto idle=after;idle.phase=c::Phase::Idle;idle.cycle.clear();need(c::effect(abort,after,idle,e),e);abort.cycle=snapshot;need(!c::before(abort,after,e),"foreign active cycle rejected");
    live::Frame frame;frame.session=77;frame.epoch=3;frame.source_generation=9;frame.revision=1;frame.snapshot=snapshot;frame.values.calendar={1,1,1,0,0,7};frame.release=value;
    std::vector<uint8_t> bytes;live::Frame decoded;need(live::encode(frame,bytes,e)&&live::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==frame,e);need(nlohmann::json::parse(bytes)["schema"]==20,"Frame20 reserved schema");
    auto wire=nlohmann::json::parse(bytes);wire["release"]["prepared"]["kind"]=0;auto data=wire.dump();need(!live::decode(reinterpret_cast<const uint8_t*>(data.data()),data.size(),decoded,e),"wrong native prepared value kind rejected");
    frame.for_action=birth;a::Message result;result.kind=a::Kind::HostResult;result.outcome=a::Outcome::Confirmed;result.session=77;result.revision=1;result.request.action_uuid=birth;result.request.snapshot_uuid=snapshot;result.evidence.before=live::ledger(frame.values);result.evidence.release_cycle_before=b;
    a::Context client{77,2,1,3,a::Role::Client,snapshot};need(live::admit_before_result(1,client,3,9,nullptr,frame,result,0,0,e)==live::Admission::New,e);
    result.evidence.release_cycle_before->graph_sha256=digest("wrong");need(live::admit_before_result(1,client,3,9,nullptr,frame,result,0,0,e)==live::Admission::Rejected,"complete action-before graph hash required");
}
struct Session:a::Session {a::Context c;explicit Session(uint64_t peer):c{77,peer,1,3,peer==1?a::Role::Host:a::Role::Client,snapshot}{}bool current(a::Context& out,std::string&)override{out=c;return true;}};
struct Engine:a::Engine {
    c::Publication value=initial();mt2mp::game::release_edit::State state=scalar();int32_t points=5;unsigned executions=0;bool partial=false;a::Ledger money;a::Evidence retained;
    Engine(){money.record_present=1;money.cash=money.record_cash=20000;}
    bool bind(const std::string&,a::Binding& out,std::string&)override{out={3,9};return true;}bool current(const a::Binding&,const std::string&,std::string&)override{return true;}bool ledger(const a::Binding&,a::Ledger& out,std::string&)override{out=money;return true;}
    a::Outcome execute_remove(const a::Binding&,const a::Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}a::Outcome apply_remove(const a::Binding&,const a::Request&,const a::Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}bool observe_remove(const a::Binding&,const a::Request&,a::Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    a::Outcome execute_action(const a::Binding&,const a::Request& q,std::string& e)override{++executions;retained={};retained.before=money;retained.release_cycle_before=proof(state,value,points);const auto cmd=q.release_cycle.command;
        if(cmd==c::Command::Show){value.cycle=q.release_cycle.cycle;value.phase=c::Phase::Announcement;}
        else if(cmd==c::Command::Begin)value.phase=c::Phase::Playing;
        else if(cmd==c::Command::Skip)value.phase=c::Phase::AwaitPublish;
        else if(cmd==c::Command::Abort){value.phase=c::Phase::Idle;value.cycle.clear();}
        else {state.version=state.planned;state.history++;points+=4;value.phase=c::Phase::Idle;value.cycle.clear();}
        retained.after=money;retained.release_cycle_after=proof(state,value,points);retained.release_cycle_value=value;return a::Outcome::Confirmed;}
    a::Outcome apply_action(const a::Binding& b,const a::Request& q,const a::Evidence& host,std::string& e)override{need(*host.release_cycle_before==proof(state,value,points),"Client actual complete before differs");const auto result=execute_action(b,q,e);if(partial){e="fixture original interrupted";return a::Outcome::Unknown;}need(retained.release_cycle_value==host.release_cycle_value,"actual Host value missing from ordered original receipt");return result;}
    bool observe_action(const a::Binding&,const a::Request&,a::Evidence& out,std::string&)override{out=retained;return true;}bool retire(const a::Binding&,const a::Request&,std::string&)override{retained.retirement_confirmed=true;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};struct Wire:a::Transport{uint64_t peer;std::deque<Packet>& packets;Wire(uint64_t p,std::deque<Packet>& q):peer(p),packets(q){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& data,std::string&)override{packets.push_back({peer,to,data});return true;}};
void controller(bool partial){Session hs(1),cs(2);Engine he,ce;ce.partial=partial;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);a::Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;need(host.start(0,e)&&client.start(0,e),e);
    for(unsigned n=0;n<(partial?1u:4u);++n){auto id=birth;id.back()=char('2'+n);const auto cmd=n==0?c::Command::Show:n==1?c::Command::Begin:n==2?c::Command::Skip:c::Command::Finalize;c::Intent intent{cmd,birth};
        if(n==3)need(!client.submit_release_cycle(id,intent,n,e),"only Host may originate retained original finalize");
        need((n==3?host:client).submit_release_cycle(id,intent,n,e),e);if(!n)need(!ce.executions,"Client effect before Host order");
        Packet request;bool first=true;unsigned delivered=0;
        while(!packets.empty()){auto message=std::move(packets.front());packets.pop_front();if(first){request=message;first=false;}const bool ok=(message.to==1?host:client).receive(message.from,message.bytes.data(),message.bytes.size(),n+10,e);++delivered;if(!ok){need(partial&&client.status().locked,"unexpected actual Controller rejection "+e);}}
        if(partial){need(host.status().locked&&!host.status().confirmed&&ce.executions==1,"Unknown must lock and never confirm/replay");return;}
        need(host.status().confirmed==n+1&&client.status().confirmed==n+1&&host.status().ack_mask==3&&he.executions==n+1&&ce.executions==n+1&&he.value==ce.value,"complete original once/Host ACK3/value parity absent");
        if(n==0){need(host.receive(request.from,request.bytes.data(),request.bytes.size(),20,e),e);while(!packets.empty()){auto m=std::move(packets.front());packets.pop_front();need((m.to==1?host:client).receive(m.from,m.bytes.data(),m.bytes.size(),21,e),e);}need(he.executions==1&&ce.executions==1,"duplicate request replayed full original");}
    }
    need(he.points==9&&he.state.history==1&&he.state.version==std::array<int32_t,3>{2,0,0},"full original publish values absent");
}
}
int main(){try{policy();controller(false);controller(true);std::cout<<checks<<" Release cycle policy/Frame20/Controller assertions passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
