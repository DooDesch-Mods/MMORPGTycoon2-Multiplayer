#include "release_edit_codec.h"
#include "release_edit_reader.h"
#include "release_ui_owner.h"
#include "modular_code.h"
#include "typed_action/action_session.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <deque>
namespace p=mt2mp::game::release_edit;namespace a=mt2mp::action_session;
namespace {
int checks=0;void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
const char* id="20000000-0000-0000-0000-000000000002";const char* snapshot="10000000-0000-0000-0000-000000000001";
std::string hash(const std::string& text){uint64_t n=1469598103934665603;for(auto c:text)n=(n^uint8_t(c))*1099511628211;std::ostringstream out;out<<std::hex<<std::setw(16)<<std::setfill('0')<<n;return out.str()+out.str()+out.str()+out.str();}
p::Defaults defaults{"Default announcement","Default changelog"};
p::State initial(){p::State s;s.version={1,2,3};s.planned={1,2,4};s.type=1;s.text={"Title","Generated",defaults.announcement,defaults.changelog};s.random={1,2,3,4};return s;}
p::Proof proof(const p::State& s,bool confirm=false){p::Proof out;std::string e;need(p::proof(s,defaults,confirm,hash,out,e),e);return out;}
p::State apply(const p::Intent& i,const p::State& b,bool& confirm){auto s=b;confirm=false;
    if(i.command==p::Command::SetType){need(p::expected_type(b,i.type,s.planned),"fixture overflow");s.type=i.type;}
    else if(i.command==p::Command::PlannerTitle)s.text[0]=i.text;
    else if(i.command==p::Command::Changelog){s.text[3]=i.text.empty()?defaults.changelog:i.text;if(!i.text.empty())s.custom=true;}
    else if(i.command==p::Command::Announcement)s.text[2]=i.text.empty()?defaults.announcement:i.text;
    else if(i.command==p::Command::UseCustom){confirm=!i.custom&&(b.text[2]!=defaults.announcement||b.text[3]!=defaults.changelog);s.custom=i.custom||confirm;}
    else{s.custom=false;s.text[2]=defaults.announcement;s.text[3]=defaults.changelog;}return s;
}
void state_and_codec(){std::string e;auto b=initial();
    for(auto command:{p::Command::SetType,p::Command::PlannerTitle,p::Command::Changelog,p::Command::Announcement,p::Command::UseCustom,p::Command::ResetCustom}){
        p::Intent i;i.command=command;if(command==p::Command::SetType)i.type=3;if(command==p::Command::PlannerTitle||command==p::Command::Changelog||command==p::Command::Announcement){i.text="Edited";i.text_sha256=hash(i.text);}
        bool confirm=false;const auto after=apply(i,b,confirm);need(p::transition(i,b,after,defaults,confirm,e),e);need(p::effect(i,proof(b),proof(after,confirm),e),e);need(p::codec::intent(p::encode(i))==i,"intent exact roundtrip");need(p::codec::proof(p::encode(proof(after,confirm)))==proof(after,confirm),"proof exact roundtrip");
        auto wrong=after;++wrong.allocated_funds;need(!p::transition(i,b,wrong,defaults,confirm,e),"unrelated funds changed");wrong=after;++wrong.random[0];need(!p::transition(i,b,wrong,defaults,confirm,e),"local RNG changed");auto bad=proof(after,confirm);bad.unrelated_sha256=hash("other");need(!p::effect(i,proof(b),bad,e),"compact unrelated state omitted");
    }
    p::Intent toggle;toggle.command=p::Command::UseCustom;b.text[3]="Custom";b.custom=true;bool confirm=false;const auto after=apply(toggle,b,confirm);need(confirm&&after==b,"unconfirmed toggle reset custom");need(p::transition(toggle,b,after,defaults,true,e),e);auto forced=after;forced.custom=false;forced.text[3]=defaults.changelog;need(!p::transition(toggle,b,forced,defaults,false,e),"toggle silently confirmed reset");need(!p::effect(toggle,proof(b),proof(forced),e),"compact toggle silently reset");
    p::Intent empty;empty.command=p::Command::Changelog;empty.text_sha256=hash("");b.custom=false;auto reset=apply(empty,b,confirm);need(!reset.custom&&reset.text[3]==defaults.changelog&&p::transition(empty,b,reset,defaults,false,e),"empty changelog did not preserve flag");empty.command=p::Command::Announcement;b.custom=true;reset=apply(empty,b,confirm);need(reset.custom&&p::transition(empty,b,reset,defaults,false,e),"empty announcement changed flag");
    auto wire=p::encode(empty);wire["custom"]=0;bool rejected=false;try{p::codec::intent(wire);}catch(const std::runtime_error&){rejected=true;}need(rejected,"integer boolean decoded");wire=p::encode(empty);wire["unused"]=1;rejected=false;try{p::codec::intent(wire);}catch(const std::runtime_error&){rejected=true;}need(rejected,"extra wire property decoded");
    p::Intent overflow;overflow.type=3;b=initial();b.version[0]=INT32_MAX;std::array<int32_t,3> next;need(!p::expected_type(b,3,next),"version overflow admitted");
    a::Message m,decoded;m.session=77;m.request.action=a::Action::ReleaseEdit;m.request.action_uuid=id;m.request.snapshot_uuid=snapshot;m.request.peer=2;m.request.sequence=1;m.request.canonical_record_key="release:editing";m.request.release_edit=empty;std::vector<uint8_t> bytes;
    need(a::encode(m,bytes,e)&&a::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,e);need(nlohmann::json::parse(bytes)["schema"]==35,"release divergent schema");m.request.general_project.command=mt2mp::game::general_project::Command::Cancel;need(!a::encode(m,bytes,e),"foreign intent leaked into Release action");
}
void physical_owner(){namespace o=mt2mp::game::release_ui_owner;std::string e;
    for(size_t stolen:{size_t(6),size_t(11),size_t(8)}){o::Owner v;v.entry=0x10000;v.trampoline=0x20000;v.detour=0x30000;v.generation=9;v.stolen=stolen;v.pristine.resize(128,0x90);for(size_t n=0;n<stolen;++n)v.pristine[n]=uint8_t(n+10);auto body=v.pristine;body[0]=0xe9;const int32_t delta=int32_t(v.trampoline+stolen+14-v.entry-5);std::memcpy(body.data()+1,&delta,4);std::vector<unsigned char> trampoline(stolen+28);std::copy_n(v.pristine.begin(),stolen,trampoline.begin());const unsigned char jump[]{0xff,0x25,0,0,0,0};std::memcpy(trampoline.data()+stolen,jump,6);const uintptr_t continuation=v.entry+stolen;std::memcpy(trampoline.data()+stolen+6,&continuation,8);std::memcpy(trampoline.data()+stolen+14,jump,6);std::memcpy(trampoline.data()+stolen+20,&v.detour,8);
        auto copy=[&](uintptr_t address,void* out,size_t count,std::string& error){const auto& source=address==v.entry?body:trampoline;if((address!=v.entry&&address!=v.trampoline)||count>source.size()){error="fixture address extent";return false;}std::memcpy(out,source.data(),count);return true;};auto digest=[](const auto& bytes){return hash(std::string(bytes.begin(),bytes.end()));};
        need(o::verify(v,copy,digest,digest(v.pristine),e),e);trampoline[stolen+6]^=1;need(!o::verify(v,copy,digest,digest(v.pristine),e),"wrong trampoline continuation admitted");trampoline[stolen+6]^=1;body[64]^=1;need(!o::verify(v,copy,digest,digest(v.pristine),e),"mutated full body admitted");body[64]^=1;v.generation=0;need(!o::verify(v,copy,digest,digest(v.pristine),e),"unowned route admitted");
    }
}
void modular_physical_owner(){namespace o=mt2mp::game::release_ui_owner;namespace code=mt2mp::game::modular_code;std::string e;
    need(code::route_stolen(0x5ba2c1)==0,"Unknown Modular entry inherited a stolen length");
    for(const auto& route:std::array<std::pair<uintptr_t,size_t>,3>{{{0x5bad60,6},{0x5bc0c0,6},{0x5ba2c0,7}}}){
        const auto guard=std::find_if(std::begin(code::guards),std::end(code::guards),[&](const auto& row){return row.rva==route.first;});
        need(guard!=std::end(code::guards),"Modular physical fixture lacks its complete pinned body");
        const auto stolen=code::route_stolen(route.first);need(stolen==route.second,"Modular installed stolen length differs from exact prologue");
        o::Owner v;v.entry=0x10000000+route.first;v.trampoline=0x20000000;v.detour=0x30000000;v.generation=9;v.stolen=stolen;v.pristine.assign(guard->original,guard->original+guard->bytes);
        auto body=v.pristine;body[0]=0xe9;const int32_t delta=int32_t(v.trampoline+stolen+14-v.entry-5);std::memcpy(body.data()+1,&delta,4);
        std::vector<unsigned char> trampoline(stolen+28);std::copy_n(v.pristine.begin(),stolen,trampoline.begin());constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};
        std::memcpy(trampoline.data()+stolen,jump,6);const uintptr_t continuation=v.entry+stolen;std::memcpy(trampoline.data()+stolen+6,&continuation,8);
        std::memcpy(trampoline.data()+stolen+14,jump,6);std::memcpy(trampoline.data()+stolen+20,&v.detour,8);
        const auto clean_body=body,clean_trampoline=trampoline;
        auto copy=[&](uintptr_t address,void* output,size_t count,std::string& error){const auto& bytes=address==v.entry?body:trampoline;if((address!=v.entry&&address!=v.trampoline)||count>bytes.size()){error="Modular fixture address extent";return false;}std::memcpy(output,bytes.data(),count);return true;};
        auto digest=[](const auto& bytes){return hash(std::string(bytes.begin(),bytes.end()));};const auto expected=digest(v.pristine);
        need(o::verify(v,copy,digest,expected,e),e);
        if(route.first==0x5ba2c0){v.stolen=6;need(!o::verify(v,copy,digest,expected,e)&&e=="Release UI trampoline/continuation/relay differs","Six-byte RightPress owner did not reproduce the native failure");v.stolen=stolen;}
        for(unsigned changed=0;changed<8;++changed){body=clean_body;trampoline=clean_trampoline;v.generation=9;
            if(changed==0)trampoline[0]^=1;
            if(changed==1)trampoline[stolen+6]^=1;
            if(changed==2)body[1]^=1;
            if(changed==3)trampoline[stolen+20]^=1;
            if(changed==4)body.back()^=1;
            if(changed==5)body[stolen-1]^=1;
            if(changed==6)v.generation=0;
            if(changed==7)trampoline[stolen+14]^=1;
            need(!o::verify(v,copy,digest,expected,e)&&!e.empty(),"Modular physical-owner tampering was accepted");
        }
        body=clean_body;trampoline=clean_trampoline;v.generation=9;need(o::verify(v,copy,digest,expected,e),e);
    }
}

struct Session:a::Session {a::Context c;Session(uint64_t peer):c{77,peer,1,3,peer==1?a::Role::Host:a::Role::Client,snapshot}{}bool current(a::Context& out,std::string&)override{out=c;return true;}};
struct Engine:a::Engine {p::State state=initial();a::Ledger money;a::Evidence receipt;bool partial=false;int executions=0;
    Engine(){money.record_present=1;money.cash=money.record_cash=20000;}
    bool bind(const std::string&,a::Binding& out,std::string&)override{out={3,9};return true;}bool current(const a::Binding&,const std::string&,std::string&)override{return true;}bool ledger(const a::Binding&,a::Ledger& out,std::string&)override{out=money;return true;}
    a::Outcome execute_remove(const a::Binding&,const a::Request&,std::string&)override{throw std::runtime_error("unexpected Remove");}a::Outcome apply_remove(const a::Binding&,const a::Request&,const a::Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}bool observe_remove(const a::Binding&,const a::Request&,a::Evidence&,std::string&)override{throw std::runtime_error("unexpected Remove");}
    a::Outcome execute_action(const a::Binding&,const a::Request& r,std::string&)override{++executions;receipt={};receipt.before=receipt.after=money;receipt.release_edit_before=proof(state);bool confirm=false;state=apply(r.release_edit,state,confirm);receipt.release_edit_after=proof(state,confirm);return a::Outcome::Confirmed;}
    a::Outcome apply_action(const a::Binding& b,const a::Request& r,const a::Evidence& authority,std::string& e)override{const auto out=execute_action(b,r,e);if(partial){e="fixture original interrupted";return a::Outcome::Unknown;}need(receipt.release_edit_after==authority.release_edit_after,"actual Client proof differs");return out;}
    bool observe_action(const a::Binding&,const a::Request&,a::Evidence& out,std::string&)override{out=receipt;return true;}bool retire(const a::Binding&,const a::Request&,std::string&)override{receipt.retirement_confirmed=true;return true;}
};
struct Packet{uint64_t from,to;std::vector<uint8_t> bytes;};struct Wire:a::Transport {uint64_t peer;std::deque<Packet>& q;Wire(uint64_t p,std::deque<Packet>& packets):peer(p),q(packets){}bool claim(Handler,std::string&)override{return true;}bool send(uint64_t to,const std::vector<uint8_t>& bytes,std::string&)override{q.push_back({peer,to,bytes});return true;}};
void controller(bool partial){Session hs(1),cs(2);Engine he,ce;ce.partial=partial;std::deque<Packet> packets;Wire hw(1,packets),cw(2,packets);a::Controller host(hs,he,hw,{}),client(cs,ce,cw,{});std::string e;need(host.start(0,e)&&client.start(0,e),e);p::Intent i;i.command=p::Command::Changelog;i.text="New changes";i.text_sha256=hash(i.text);need(client.submit_release_edit(id,i,0,e),e);need(!he.executions&&!ce.executions,"Client original ran before ordering");const auto request=packets.front();
    auto deliver=[&]{need(!packets.empty(),"missing ordered packet");auto m=packets.front();packets.pop_front();return(m.to==1?host:client).receive(m.from,m.bytes.data(),m.bytes.size(),1,e);};need(deliver(),e);const auto result=packets.front();
    if(partial){need(!deliver()&&client.status().locked&&!client.status().confirmed,"Unknown native effect ACKed");need(deliver()&&host.status().locked&&!host.status().confirmed,"Host accepted Unknown Client");return;}
    need(deliver()&&deliver()&&deliver(),e);need(host.status().ack_mask==3&&host.status().confirmed==1&&client.status().confirmed==1&&he.state==ce.state,"complete original/ACK3 absent");need(host.receive(request.from,request.bytes.data(),request.bytes.size(),2,e)&&client.receive(result.from,result.bytes.data(),result.bytes.size(),2,e),e);need(he.executions==1&&ce.executions==1,"duplicate delivery replayed native original");need(!client.submit_release_edit(id,i,3,e),"consumed UUID replayed");
}
}
int main(){try{state_and_codec();physical_owner();modular_physical_owner();controller(false);controller(true);std::cout<<checks<<" Release policy/codec/Controller/physical-owner checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
