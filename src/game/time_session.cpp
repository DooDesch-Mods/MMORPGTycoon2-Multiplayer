#include "time_session.h"
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>

namespace mt2mp::game::time_session {
namespace {
bool same(Clock a,Clock b){return a.selected==b.selected&&a.pause==b.pause;}
bool same(const Snapshot&a,const Snapshot&b){return a.present==b.present&&a.uncertain==b.uncertain&&a.revision==b.revision&&same(a.clock,b.clock);}
bool same(const Message&a,const Message&b){return a.kind==b.kind&&a.intent==b.intent&&a.result==b.result&&a.epoch==b.epoch&&a.peer==b.peer&&a.sequence==b.sequence&&a.observed_revision==b.observed_revision&&same(a.state,b.state)&&a.code==b.code;}
bool valid(Clock c){return std::isfinite(c.selected)&&std::isfinite(c.pause)&&c.selected>=0&&c.pause>=0&&std::isfinite(c.selected*c.pause)&&!std::signbit(c.selected)&&!std::signbit(c.pause);}
bool shape(const Message&m,std::string&e){
    auto fail=[&](const char*s){e=s;return false;};
    if(!m.epoch||m.peer<1||m.peer>4||uint8_t(m.kind)<1||uint8_t(m.kind)>4||uint8_t(m.intent)>4||uint8_t(m.result)>5||uint16_t(m.code)>uint16_t(Code::StaleState))return fail("invalid wire enum, epoch or peer");
    if(!valid(m.state.clock))return fail("nonfinite, negative or noncanonical clock pair");
    if(m.state.present&&!m.state.revision)return fail("present state requires revision");
    if(!m.state.present&&(m.state.uncertain||m.state.clock.selected!=0||m.state.clock.pause!=0))return fail("absent state contains clock values");
    switch(m.kind){
    case Kind::Request:
        if(!m.sequence||m.result!=Result::None||m.code!=Code::None||m.state.present||m.state.revision)return fail("invalid request fields");break;
    case Kind::State:
        if(m.peer!=1||m.sequence||m.observed_revision||m.intent!=Intent::Pause||m.result!=Result::None||m.code!=Code::None||!m.state.present)return fail("invalid state fields");break;
    case Kind::Receipt:
        if(!m.sequence||m.result==Result::None)return fail("invalid receipt sequence or result");
        if(m.result==Result::Confirmed||m.result==Result::NoChange){if(m.code!=Code::None||!m.state.present||m.state.uncertain)return fail("successful receipt needs certain clock");}
        else if(m.code==Code::None||((m.result==Result::Unknown||m.result==Result::Locked)&&m.state.present))return fail("failed receipt needs cause and unknown state absence");break;
    case Kind::PresentationAck:
        if(m.peer==1||m.sequence||m.observed_revision||m.intent!=Intent::Pause||!m.state.revision)return fail("invalid presentation acknowledgement fields");
        if(m.result==Result::Confirmed){if(m.code!=Code::None||!m.state.present)return fail("confirmed presentation needs state");}
        else if((m.result!=Result::Rejected&&m.result!=Result::Unknown&&m.result!=Result::Locked)||m.code==Code::None||m.state.present)return fail("failed presentation needs absent clock and cause");break;
    }
    e.clear();return true;
}
void put(std::array<uint8_t,wire_size>&b,size_t p,uint64_t v,size_t n){for(size_t i=0;i<n;++i)b[p+i]=uint8_t(v>>(i*8));}
uint64_t get(const uint8_t*b,size_t p,size_t n){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[p+i])<<(i*8);return v;}
uint32_t bits(float f){uint32_t u;std::memcpy(&u,&f,4);return u;}
float real(uint32_t u){float f;std::memcpy(&f,&u,4);return f;}
struct Busy {bool&value;explicit Busy(bool&v):value(v){value=true;}~Busy(){value=false;}};
Report okay(uint64_t seq=0,uint64_t rev=0,bool effect=false,Disposition d=Disposition::Accepted){return {d,Code::None,effect,seq,rev,{}};}
std::string cause_or(const std::string&cause,const char*fallback){return cause.empty()?fallback:cause;}
}
bool encode(const Message&m,std::array<uint8_t,wire_size>&b,std::string&e){
    if(!shape(m,e))return false;b.fill(0);b[0]='M';b[1]='2';b[2]='T';b[3]='M';b[4]=1;
    b[5]=uint8_t(m.kind);b[6]=uint8_t(m.intent);b[7]=uint8_t(m.result);put(b,8,m.epoch,8);b[16]=m.peer;b[17]=uint8_t(m.state.present)|(uint8_t(m.state.uncertain)<<1);
    put(b,24,m.sequence,8);put(b,32,m.observed_revision,8);put(b,40,m.state.revision,8);put(b,48,bits(m.state.clock.selected),4);put(b,52,bits(m.state.clock.pause),4);put(b,56,uint16_t(m.code),2);return true;
}
bool decode(const uint8_t*b,size_t n,Message&m,std::string&e){
    if(!b||n!=wire_size){e="time packet size must be 64";return false;}
    if(b[0]!='M'||b[1]!='2'||b[2]!='T'||b[3]!='M'||b[4]!=1||b[17]>3){e="time packet header invalid";return false;}
    for(size_t i=18;i<24;++i)if(b[i]){e="reserved byte nonzero";return false;}
    for(size_t i=58;i<64;++i)if(b[i]){e="reserved byte nonzero";return false;}
    Message v;v.kind=Kind(b[5]);v.intent=Intent(b[6]);v.result=Result(b[7]);v.epoch=get(b,8,8);v.peer=b[16];v.state.present=(b[17]&1)!=0;v.state.uncertain=(b[17]&2)!=0;
    v.sequence=get(b,24,8);v.observed_revision=get(b,32,8);v.state.revision=get(b,40,8);v.state.clock={real(uint32_t(get(b,48,4))),real(uint32_t(get(b,52,4)))};v.code=Code(get(b,56,2));
    if(!shape(v,e))return false;m=v;return true;
}
Report Controller::guard()const{
    if(!status_.started)return {Disposition::Rejected,Code::NotStarted,false,0,0,"controller not started"};
    if(busy_)return {Disposition::Rejected,Code::Reentrant,false,0,0,"controller callback reentry"};
    try{if(callbacks_.thread_id(callbacks_.user)!=config_.game_thread)return {Disposition::Rejected,Code::WrongThread,false,0,0,"controller requires bound game thread"};}
    catch(const std::exception&e){return {Disposition::Rejected,Code::CallbackException,false,0,0,e.what()};}
    catch(...){return {Disposition::Rejected,Code::CallbackException,false,0,0,"thread callback threw nonstandard exception"};}
    return okay();
}
Report Controller::error(Code c,const std::string&s,Disposition d){status_.last_code=c;status_.last_cause=s;++status_.failed;return {d,c,false,0,status_.observed.revision,s};}
bool Controller::room(size_t n)const{return n<=outbox_size-queued_;}
void Controller::emit(uint8_t t,const Message&m){outbox_[(head_+queued_)%outbox_size]={t,m};++queued_;}
Report Controller::start(const Config&c,const Callbacks&f){
    if(status_.started||busy_)return error(Code::Reentrant,"stop active controller before new epoch");
    if(!c.epoch||c.epoch==last_epoch_||!c.game_thread||c.local_peer<1||c.local_peer>4||(c.peer_mask&0xf0)||!(c.peer_mask&1)||!(c.peer_mask&(1u<<(c.local_peer-1)))||
       (c.role!=Role::Host&&c.role!=Role::Client)||(c.role==Role::Host)!=(c.local_peer==1)||!f.thread_id||
       (c.role==Role::Host&&(!f.read_host_clock||!f.set_host_clock))||(c.role==Role::Client&&!f.apply_client_presentation))return error(Code::Malformed,"invalid session configuration or reused epoch");
    try{if(f.thread_id(f.user)!=c.game_thread)return error(Code::WrongThread,"start requires bound game thread");}
    catch(const std::exception&e){return error(Code::CallbackException,e.what());}catch(...){return error(Code::CallbackException,"start thread callback threw nonstandard exception");}
    config_=c;callbacks_=f;status_={};status_.started=true;last_epoch_=c.epoch;local_sequence_=0;receipts_={};presentation_receipts_={};pending_={};client_receipts_={};presentation_history_={};host_history_={};head_=queued_=0;return okay();
}
Report Controller::stop(){auto g=guard();if(!g.okay())return g;status_.started=false;head_=queued_=0;callbacks_={};return okay();}
Report Controller::status(Status&s)const{auto g=guard();if(!g.okay())return g;s=status_;return okay();}
Report Controller::pop(Outbound&o,bool&available){available=false;auto g=guard();if(!g.okay())return g;if(queued_){o=outbox_[head_];head_=(head_+1)%outbox_size;--queued_;available=true;}return okay();}
bool Controller::read_clock(Clock&v,Code&code,std::string&why){
    try{if(!callbacks_.read_host_clock(callbacks_.user,config_.epoch,v,why)){code=Code::NativeRead;why=cause_or(why,"host clock read rejected");return false;}}
    catch(const std::exception&e){code=Code::CallbackException;why=e.what();return false;}catch(...){code=Code::CallbackException;why="host read callback threw nonstandard exception";return false;}
    if(!valid(v)){code=Code::NativeRead;why="host clock read returned invalid pair";return false;}return true;
}
bool Controller::publish(Clock v,Code&code,std::string&why){
    if(status_.observed.present&&same(status_.observed.clock,v)&&status_.observed.uncertain==status_.write_locked)return true;
    if(status_.observed.revision==UINT64_MAX){code=Code::RevisionExhausted;why="host revision exhausted";return false;}
    status_.observed={true,status_.write_locked,status_.observed.revision+1,v};host_history_[status_.observed.revision%history_size]=status_.observed;
    Message m;m.kind=Kind::State;m.epoch=config_.epoch;m.peer=1;m.state=status_.observed;emit(0,m);++status_.published;return true;
}
Report Controller::poll_host(){auto g=guard();if(!g.okay())return g;if(config_.role!=Role::Host)return error(Code::WrongRole,"only host polls native clock");if(!room(1))return error(Code::Backpressure,"host poll needs one outbox slot");Busy busy(busy_);Clock c;Code code=Code::None;std::string why;
    if(!read_clock(c,code,why)||!publish(c,code,why))return error(code,why);return okay(0,status_.observed.revision);
}
Report Controller::submit(Intent intent){
    auto g=guard();if(!g.okay())return g;if(uint8_t(intent)>4)return error(Code::Malformed,"invalid time intent");
    if(local_sequence_==UINT64_MAX)return error(Code::SequenceExpired,"local request sequence exhausted");
    if(config_.role==Role::Client&&(status_.write_locked||!status_.applied.present||status_.applied.revision!=status_.observed.revision))return error(status_.write_locked?Code::Locked:Code::NoState,"client needs current confirmed presentation",status_.write_locked?Disposition::Locked:Disposition::Rejected);
    Message m;m.epoch=config_.epoch;m.peer=config_.local_peer;m.sequence=local_sequence_+1;m.intent=intent;m.observed_revision=status_.observed.revision;
    Busy busy(busy_);
    if(config_.role==Role::Host){auto r=process_request(m);if(status_.processed_sequences[0]==m.sequence)local_sequence_=m.sequence;return r;}
    if(!room(1)||pending_[m.sequence%history_size].used)return error(Code::Backpressure,"client request queue or outstanding history full");
    pending_[m.sequence%history_size]={true,m,{},{}};local_sequence_=m.sequence;emit(1,m);return okay(m.sequence,status_.observed.revision);
}
Report Controller::receive(uint8_t sender,const Message&m){
    auto g=guard();if(!g.okay())return g;std::string why;if(!shape(m,why))return error(Code::Malformed,why);
    if(sender<1||sender>4||!(config_.peer_mask&(1u<<(sender-1))))return error(Code::UntrustedSender,"sender is not an authenticated session peer");
    if(m.epoch!=config_.epoch)return error(Code::WrongEpoch,"packet epoch differs from bound world");
    Busy busy(busy_);
    if(config_.role==Role::Host){
        if((m.kind!=Kind::Request&&m.kind!=Kind::PresentationAck)||sender!=m.peer)return error(Code::UntrustedSender,"host accepts requests and presentation acknowledgements from their named sender");
        if(m.kind==Kind::Request)return process_request(m);
        auto&cache=presentation_receipts_[sender-1][m.state.revision%history_size];
        if(cache.used&&cache.request.state.revision==m.state.revision){
            if(!same(cache.request,m))return error(Code::ConflictingDuplicate,"presentation acknowledgement changed");++status_.duplicates;
            return m.code==Code::None?okay(0,m.state.revision,false,Disposition::Duplicate):error(m.code,cache.cause,Disposition::Duplicate);
        }
        if(m.state.revision>status_.observed.revision)return error(Code::FutureObservation,"presentation acknowledgement refers to future revision");
        const auto&expected=host_history_[m.state.revision%history_size];if(expected.revision!=m.state.revision)return error(Code::SequenceExpired,"presentation acknowledgement revision evicted");
        if(m.result==Result::Confirmed&&!same(expected,m.state))return error(Code::ReadbackMismatch,"presentation acknowledgement does not match host state");
        cache={true,m,{},m.result==Result::Confirmed?"":"client presentation failed; remote original cause stays at client"};if(m.result!=Result::Confirmed)return error(m.code,cache.cause);
        auto&ack=status_.presentation_acks[sender-1];if(m.state.revision>ack)ack=m.state.revision;return okay(0,m.state.revision,true);
    }
    if(sender!=1||(m.kind!=Kind::State&&m.kind!=Kind::Receipt)||(m.kind==Kind::State?m.peer!=1:m.peer!=config_.local_peer))return error(Code::UntrustedSender,"client accepts host states and receipts addressed to itself");
    return process_client(m);
}
Report Controller::process_request(const Message&m){
    auto&high=status_.processed_sequences[m.peer-1];auto&cache=receipts_[m.peer-1][m.sequence%history_size];
    if(m.sequence<=high){
        if(!cache.used||cache.request.sequence!=m.sequence)return error(Code::SequenceExpired,"request receipt evicted; request cannot execute again");
        if(!same(cache.request,m))return error(Code::ConflictingDuplicate,"request id reused with different payload");
        if(!room(1))return error(Code::Backpressure,"duplicate receipt needs one outbox slot");emit(m.peer,cache.reply);++status_.duplicates;
        auto r=cache.reply.code==Code::None?okay(m.sequence,cache.reply.state.revision,false,Disposition::Duplicate):error(cache.reply.code,cache.cause,Disposition::Duplicate);r.sequence=m.sequence;return r;
    }
    if(m.sequence!=high+1)return error(Code::SequenceGap,"request sequence is not the next per-peer value");
    if(!room(3))return error(Code::Backpressure,"request needs three outbox slots before native execution");
    Message reply=m;reply.kind=Kind::Receipt;reply.result=Result::Rejected;
    auto finish=[&](Result result,Code code,const std::string&why,bool include_state){
        reply.result=result;reply.code=code;reply.state=include_state?status_.observed:Snapshot{};cache={true,m,reply,why};high=m.sequence;emit(m.peer,reply);
        auto r=code==Code::None?okay(m.sequence,reply.state.revision,result==Result::Confirmed):error(code,why,result==Result::Locked?Disposition::Locked:Disposition::Rejected);r.sequence=m.sequence;return r;
    };
    if(status_.write_locked)return finish(Result::Locked,Code::Locked,"host native effect unresolved; new epoch required",false);
    Clock current;Code code=Code::None;std::string why;
    if(!read_clock(current,code,why)||!publish(current,code,why))return finish(Result::Rejected,code,why,false);
    if(m.observed_revision>status_.observed.revision)return finish(Result::Rejected,Code::FutureObservation,"request observed revision is ahead of host",true);
    const float targets[]={0,1,3,9};const float target=m.intent==Intent::TogglePauseNormal?(current.selected*current.pause==0?1.0f:0.0f):targets[uint8_t(m.intent)];
    if(target==current.selected)return finish(Result::NoChange,Code::None,{},true);
    if(status_.observed.revision==UINT64_MAX)return finish(Result::Rejected,Code::RevisionExhausted,"host revision exhausted before write",true);
    ++status_.attempted;Effect effect=Effect::Unknown;
    try{effect=callbacks_.set_host_clock(callbacks_.user,config_.epoch,target,why);}
    catch(const std::exception&e){code=Code::CallbackException;why=e.what();}catch(...){code=Code::CallbackException;why="host write callback threw nonstandard exception";}
    if(effect==Effect::RejectedNoEffect)return finish(Result::Rejected,Code::NativeRejected,cause_or(why,"host native setter rejected without effect"),true);
    if(effect!=Effect::Confirmed){status_.write_locked=true;return finish(Result::Unknown,code==Code::None?Code::NativeUnknown:code,cause_or(why,"host native setter effect unknown"),false);}
    Clock actual;
    if(!read_clock(actual,code,why)){status_.write_locked=true;return finish(Result::Unknown,code,why,false);}
    if(actual.selected!=target){status_.write_locked=true;return finish(Result::Unknown,Code::ReadbackMismatch,"host setter readback differs from requested selected scale",false);}
    if(!publish(actual,code,why)){status_.write_locked=true;return finish(Result::Unknown,code,why,false);}
    ++status_.confirmed;return finish(Result::Confirmed,Code::None,{},true);
}
Report Controller::apply_state(const Snapshot&s){
    auto&cache=presentation_history_[s.revision%history_size];
    if(cache.used&&cache.request.state.revision==s.revision){
        if(!same(cache.request.state,s))return error(Code::ConflictingDuplicate,"host state revision changed payload");
        if(!room(1))return error(Code::Backpressure,"presentation acknowledgement replay needs one slot");emit(1,cache.reply);++status_.duplicates;
        return cache.reply.code==Code::None?okay(0,s.revision,false,Disposition::Duplicate):error(cache.reply.code,cache.cause,Disposition::Duplicate);
    }
    if(s.revision<=status_.observed.revision)return error(Code::StaleState,"state is older than retained client presentation",Disposition::Ignored);
    if(!room(1))return error(Code::Backpressure,"presentation needs one acknowledgement slot before native execution");
    status_.observed=s;Message source;source.kind=Kind::State;source.epoch=config_.epoch;source.peer=1;source.state=s;
    Message ack;ack.kind=Kind::PresentationAck;ack.epoch=config_.epoch;ack.peer=config_.local_peer;ack.state.revision=s.revision;
    Code code=Code::None;std::string why;Effect effect=Effect::Unknown;PresentationReadback actual;
    if(status_.write_locked){ack.result=Result::Locked;code=Code::Locked;why="client presentation effect unresolved; new epoch required";}
    else{
        ++status_.attempted;
        try{effect=callbacks_.apply_client_presentation(callbacks_.user,config_.epoch,s,actual,why);}
        catch(const std::exception&e){code=Code::CallbackException;why=e.what();}catch(...){code=Code::CallbackException;why="presentation callback threw nonstandard exception";}
        if(effect==Effect::Confirmed&&actual.items_confirmed&&actual.revision==s.revision&&same(actual.clock,s.clock)&&std::isfinite(actual.native_clock_scale)&&actual.native_clock_scale==0){status_.applied=s;ack.result=Result::Confirmed;ack.state=s;++status_.confirmed;}
        else if(effect==Effect::RejectedNoEffect){ack.result=Result::Rejected;code=Code::PresentationRejected;why=cause_or(why,"client presentation rejected without effect");}
        else{status_.write_locked=true;ack.result=Result::Unknown;if(code==Code::None)code=effect==Effect::Confirmed?Code::ReadbackMismatch:Code::PresentationUnknown;why=cause_or(why,"client presentation or native clock zero readback unconfirmed");}
    }
    ack.code=code;cache={true,source,ack,why};emit(1,ack);
    return code==Code::None?okay(0,s.revision,true):error(code,why,ack.result==Result::Locked?Disposition::Locked:Disposition::Rejected);
}
Report Controller::process_client(const Message&m){
    if(m.kind==Kind::State)return apply_state(m.state);
    auto&cached=client_receipts_[m.sequence%history_size];
    if(cached.used&&cached.reply.sequence==m.sequence){
        if(!same(cached.reply,m))return error(Code::ConflictingDuplicate,"host receipt changed payload");++status_.duplicates;
        if(m.code!=Code::None){auto r=error(m.code,"host time request failed; original callback cause stays at host",Disposition::Duplicate);r.sequence=m.sequence;return r;}
        auto r=m.state.present?apply_state(m.state):okay(m.sequence,m.state.revision,false,Disposition::Duplicate);r.sequence=m.sequence;return r;
    }
    auto&pending=pending_[m.sequence%history_size];
    if(!pending.used||pending.request.sequence!=m.sequence)return error(Code::UnexpectedReceipt,"receipt has no outstanding local request");
    if(pending.request.intent!=m.intent||pending.request.observed_revision!=m.observed_revision)return error(Code::ConflictingDuplicate,"receipt does not echo request intent and observation");
    if(m.state.present&&!room(1))return error(Code::Backpressure,"receipt presentation needs acknowledgement capacity");
    Report r=okay(m.sequence,m.state.revision);
    if(m.state.present)r=apply_state(m.state);
    cached={true,pending.request,m,{}};pending={};
    if(m.code!=Code::None)return error(m.code,"host time request failed; original callback cause stays at host");
    r.sequence=m.sequence;return r;
}
}
