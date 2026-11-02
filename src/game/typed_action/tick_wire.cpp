#include "tick_wire.h"
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace mt2mp::action_session::tick_commands::wire {
namespace {
namespace ts=game::time_session;
constexpr size_t cause_limit=1024;
void require(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
bool uuid(const std::string& s){
    if(s.size()!=36)return false;
    for(size_t i=0;i<s.size();++i){
        if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    }return true;
}
uint64_t peer(const Input& in){return std::visit([](const auto& r){return r.peer;},in.command);}
bool member(uint8_t mask,uint64_t p){return p>=1&&p<=4&&(mask&(1u<<(p-1)))!=0;}
uint64_t session(const Packet& p){return std::visit([](const auto& v){return v.session;},p.value);}
const std::string& snapshot(const Packet& p){return std::visit([](const auto& v)->const std::string&{return v.snapshot_uuid;},p.value);}
struct Writer {
    std::vector<uint8_t> bytes;
    void raw(const uint8_t* p,size_t n){require(n<=max_transport_payload-bytes.size(),"Tick packet exceeds transport byte budget");bytes.insert(bytes.end(),p,p+n);}
    void number(uint64_t v,size_t n){for(size_t i=0;i<n;++i){const auto b=uint8_t(v>>(i*8));raw(&b,1);}}
    void text(const std::string& s){raw(reinterpret_cast<const uint8_t*>(s.data()),s.size());}
};
struct Reader {
    const uint8_t* data;size_t size,pos=0;
    const uint8_t* take(size_t n){require(n<=size-pos,"Truncated tick packet");const auto* p=data+pos;pos+=n;return p;}
    uint64_t number(size_t n){const auto* p=take(n);uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(p[i])<<(8*i);return v;}
    std::string text(size_t n){const auto* p=take(n);return std::string(reinterpret_cast<const char*>(p),n);}
};
void put_input(Writer& w,const Input& in,uint64_t owner,const std::string& world,uint8_t mask,uint8_t revision){
    require(in.session==owner&&in.snapshot_uuid==world&&member(mask,peer(in)),"Tick input session, snapshot or membership differs");
    std::vector<uint8_t> body;std::string cause;
    if(const auto* request=std::get_if<Request>(&in.command)){
        require(request->snapshot_uuid==world,"Gameplay request snapshot differs");
        auto message=std::make_unique<Message>();message->session=owner;message->request=*request;
        if(!action_session::encode(*message,body,cause))throw std::invalid_argument("Gameplay request codec: "+cause);
    }else if(std::holds_alternative<TimeRequest>(in.command)){
        const auto& time_request=std::get<TimeRequest>(in.command);
        ts::Message message;message.epoch=owner;message.peer=uint8_t(time_request.peer);message.sequence=time_request.sequence;
        message.observed_revision=time_request.observed_revision;message.intent=time_request.intent;
        std::array<uint8_t,ts::wire_size> bytes{};
        if(!ts::encode(message,bytes,cause))throw std::invalid_argument("Time request codec: "+cause);
        body.assign(bytes.begin(),bytes.end());
    }else{
        const auto& snapshot_request=std::get<SnapshotRequest>(in.command);
        require(snapshot_request.peer==1&&snapshot_request.sequence&&uuid(snapshot_request.nonce),"Snapshot request Host/sequence/nonce invalid");
        Writer save;save.number(snapshot_request.peer,8);save.number(snapshot_request.sequence,8);save.number(snapshot_request.observed_revision,8);save.text(snapshot_request.nonce);
        require(uint8_t(snapshot_request.purpose)<=1,"Snapshot purpose invalid");
        if(revision==3)save.number(uint8_t(snapshot_request.purpose),1);
        else require(snapshot_request.purpose==SnapshotPurpose::Ordinary,"Periodic snapshot requires tick wire version3");
        body=std::move(save.bytes);
    }
    w.number(in.command.index(),1);w.number(body.size(),4);w.raw(body.data(),body.size());
}
Input get_input(Reader& r,uint64_t owner,const std::string& world,uint8_t mask,uint8_t revision){
    const auto stream=r.number(1),length=r.number(4);
    require(stream<=2&&length>0&&length<=max_payload,"Tick command stream or length invalid");
    const auto* body=r.take(size_t(length));Input in;in.session=owner;in.snapshot_uuid=world;std::string cause;
    if(stream==0){
        auto message=std::make_unique<Message>();
        if(!action_session::decode(body,size_t(length),*message,cause))throw std::invalid_argument("Gameplay request codec: "+cause);
        require(message->kind==Kind::Request&&message->session==owner&&message->request.snapshot_uuid==world,"Embedded gameplay request envelope differs");
        in.command=std::move(message->request);
    }else if(stream==1){
        ts::Message message;
        if(!ts::decode(body,size_t(length),message,cause))throw std::invalid_argument("Time request codec: "+cause);
        require(message.kind==ts::Kind::Request&&message.epoch==owner,"Embedded time request envelope differs");
        in.command=TimeRequest{message.peer,message.sequence,message.observed_revision,message.intent};
    }else{
        require(length==(revision==3?61:60),"Snapshot request exact byte count differs");Reader save{body,size_t(length)};
        SnapshotRequest value{save.number(8),save.number(8),save.number(8),save.text(36)};
        if(revision==3){const auto purpose=save.number(1);require(purpose<=1,"Snapshot purpose invalid");value.purpose=SnapshotPurpose(purpose);}
        require(value.peer==1&&value.sequence&&uuid(value.nonce),"Snapshot request Host/sequence/nonce invalid");in.command=std::move(value);
    }
    require(member(mask,peer(in)),"Embedded request peer is outside membership");return in;
}
void receipt_shape(const Receipt& r){
    require(r.tick&&r.completed_commands<=Queue::batch_limit&&r.cause.size()<=cause_limit,"Tick receipt identity or bounds invalid");
    require(r.command_evidence.size()<=r.completed_commands,"Tick evidence count exceeds completed commands");
    uint64_t previous=0;
    for(const auto& value:r.command_evidence){
        require(value.order>previous&&value.order<=r.last_order&&evidence_digest(value.sha256),"Tick evidence order or SHA256 invalid");previous=value.order;
    }
    if(r.outcome==Outcome::Confirmed)require(r.tick_attempted&&r.tick_completed&&r.cause.empty(),"Confirmed tick receipt lacks complete execution");
    else if(r.outcome==Outcome::RejectedNoEffect)require(!r.completed_commands&&!r.tick_attempted&&!r.tick_completed&&!r.cause.empty(),"Rejected tick receipt contains effects or no cause");
    else require(r.outcome==Outcome::Unknown&&!r.tick_completed&&!r.cause.empty(),"Tick receipt must describe a completed attempt or retained failure");
}
bool identity(const Packet& p,const Context& c,std::string& cause){
    if(session(p)!=c.session||snapshot(p)!=c.snapshot_uuid||p.peer_mask!=c.peer_mask){cause="Tick wire session, snapshot or membership differs";return false;}return true;
}
}
bool encode(const Packet& p,std::vector<uint8_t>& out,std::string& cause){
    out.clear();
    try{
        const auto owner=session(p);const auto& world=snapshot(p);
        require(owner&&uuid(world)&&p.peer_mask<=15&&member(p.peer_mask,1),"Tick packet session, snapshot or membership invalid");
        const auto* proof=std::get_if<Receipt>(&p.value);
        const bool extended=proof&&!proof->command_evidence.empty();
        const auto periodic=[](const Input& input){const auto* s=std::get_if<SnapshotRequest>(&input.command);return s&&s->purpose==SnapshotPurpose::HostPeriodic;};
        bool version3=false;
        version3=std::holds_alternative<PeriodicRequest>(p.value);
        if(const auto* input=std::get_if<Input>(&p.value))version3=periodic(*input);
        if(const auto* batch=std::get_if<Batch>(&p.value))for(const auto& item:batch->commands)version3|=periodic(item.input);
        const uint8_t revision=extended?2:version3?3:1;
        Writer w;w.text("MT2K");w.number(revision,1);w.number(p.value.index()+1,1);w.number(p.peer_mask,1);w.number(0,1);w.number(owner,8);w.text(world);
        if(const auto* in=std::get_if<Input>(&p.value))put_input(w,*in,owner,world,p.peer_mask,revision);
        else if(const auto* batch=std::get_if<Batch>(&p.value)){
            require(batch->peer_mask==p.peer_mask&&batch->tick&&batch->commands.size()<=Queue::batch_limit,"Tick batch tick, membership or command count invalid");
            w.number(batch->tick,8);w.number(batch->previous_order,8);w.number(batch->commands.size(),2);auto order=batch->previous_order;
            for(const auto& item:batch->commands){require(order!=UINT64_MAX&&item.order==order+1,"Tick batch order is not contiguous");order=item.order;w.number(order,8);put_input(w,item.input,owner,world,p.peer_mask,revision);}
        }else if(std::holds_alternative<Receipt>(p.value)){
            const auto& receipt=std::get<Receipt>(p.value);receipt_shape(receipt);
            w.number(receipt.tick,8);w.number(receipt.last_order,8);w.number(receipt.completed_commands,2);w.number(uint8_t(receipt.outcome),1);
            w.number(uint8_t(receipt.tick_attempted)|(uint8_t(receipt.tick_completed)<<1),1);w.number(receipt.cause.size(),2);w.text(receipt.cause);
            if(extended){w.number(receipt.command_evidence.size(),1);for(const auto& value:receipt.command_evidence){w.number(value.order,8);w.text(value.sha256);}}
        }else if(const auto* periodic_request=std::get_if<PeriodicRequest>(&p.value)){
            require(periodic_request->peer!=1&&member(p.peer_mask,periodic_request->peer)&&periodic_request->tick&&periodic_request->invocation,"Periodic request identity invalid");
            w.number(periodic_request->peer,1);w.number(periodic_request->tick,8);w.number(periodic_request->invocation,8);
        }else{
            const auto& value=std::get<SnapshotControl>(p.value);
            require(value.tick&&value.order&&uuid(value.nonce)&&uint8_t(value.phase)>=1&&uint8_t(value.phase)<=3&&
                value.ack_mask&&!(value.ack_mask&~p.peer_mask)&&std::any_of(value.witness.begin(),value.witness.end(),[](uint8_t v){return v!=0;}),"Snapshot control identity/hash/ack invalid");
            w.number(value.tick,8);w.number(value.order,8);w.text(value.nonce);w.number(uint8_t(value.phase),1);w.number(value.ack_mask,1);w.raw(value.witness.data(),value.witness.size());
        }
        out=std::move(w.bytes);cause.clear();return true;
    }catch(const std::exception& e){cause=std::string("Tick wire encode: ")+e.what();return false;}
}
bool decode(const uint8_t* data,size_t size,Packet& out,std::string& cause){
    try{
        require(data&&size>=52&&size<=max_transport_payload,"Tick packet byte bound invalid");Reader r{data,size};
        require(r.text(4)=="MT2K","Tick packet magic invalid");const auto revision=r.number(1);
        require(revision==1||revision==2||revision==3,"Tick packet version invalid");
        const auto kind=r.number(1);Packet p;p.peer_mask=uint8_t(r.number(1));require(r.number(1)==0,"Tick packet reserved byte nonzero");
        require(revision!=2||kind==3,"Tick version2 requires canonical evidence receipt");
        require(revision!=3||kind==1||kind==2||kind==5,"Tick version3 requires an input, batch or periodic request");
        require(kind!=5||revision==3,"Periodic request requires tick version3");
        const auto owner=r.number(8);const auto world=r.text(36);
        require(kind>=1&&kind<=5&&owner&&uuid(world)&&p.peer_mask<=15&&member(p.peer_mask,1),"Tick packet kind, session, snapshot or membership invalid");
        if(kind==1)p.value=get_input(r,owner,world,p.peer_mask,uint8_t(revision));
        else if(kind==2){
            Batch batch;batch.session=owner;batch.snapshot_uuid=world;batch.peer_mask=p.peer_mask;batch.tick=r.number(8);batch.previous_order=r.number(8);const auto count=r.number(2);
            require(batch.tick&&count<=Queue::batch_limit,"Tick batch tick or count invalid");auto order=batch.previous_order;
            for(uint64_t i=0;i<count;++i){const auto next=r.number(8);require(order!=UINT64_MAX&&next==order+1,"Tick batch order is not contiguous");order=next;batch.commands.push_back({next,get_input(r,owner,world,p.peer_mask,uint8_t(revision))});}
            p.value=std::move(batch);
        }else if(kind==3){
            Receipt receipt;receipt.session=owner;receipt.snapshot_uuid=world;receipt.tick=r.number(8);receipt.last_order=r.number(8);receipt.completed_commands=size_t(r.number(2));receipt.outcome=Outcome(r.number(1));
            const auto flags=r.number(1),length=r.number(2);require(flags<=3&&length<=cause_limit,"Tick receipt flags or cause length invalid");receipt.tick_attempted=(flags&1)!=0;receipt.tick_completed=(flags&2)!=0;receipt.cause=r.text(size_t(length));
            if(revision==2){const auto count=r.number(1);require(count&&count<=Queue::batch_limit,"Tick evidence count outside1..64");
                for(size_t i=0;i<count;++i)receipt.command_evidence.push_back({r.number(8),r.text(64)});}
            receipt_shape(receipt);p.value=std::move(receipt);
        }else if(kind==4){
            SnapshotControl value;value.session=owner;value.snapshot_uuid=world;value.tick=r.number(8);value.order=r.number(8);value.nonce=r.text(36);
            value.phase=SnapshotPhase(r.number(1));value.ack_mask=uint8_t(r.number(1));const auto* hash=r.take(32);std::copy(hash,hash+32,value.witness.begin());
            require(value.tick&&value.order&&uuid(value.nonce)&&uint8_t(value.phase)>=1&&uint8_t(value.phase)<=3&&
                value.ack_mask&&!(value.ack_mask&~p.peer_mask)&&std::any_of(value.witness.begin(),value.witness.end(),[](uint8_t v){return v!=0;}),"Snapshot control identity/hash/ack invalid");p.value=std::move(value);
        }else{
            PeriodicRequest value;value.session=owner;value.snapshot_uuid=world;value.peer=r.number(1);value.tick=r.number(8);value.invocation=r.number(8);
            require(value.peer!=1&&member(p.peer_mask,value.peer)&&value.tick&&value.invocation,"Periodic request identity invalid");p.value=std::move(value);
        }
        require(r.pos==size,"Tick packet has trailing bytes");out=std::move(p);cause.clear();return true;
    }catch(const std::exception& e){cause=std::string("Tick wire decode: ")+e.what();return false;}
}
bool current(Session& session_reader,const Context& expected,std::string& cause){
    Context actual;
    if(!session_reader.current(actual,cause)){if(cause.empty())cause="Tick wire loaded session unavailable without a cause";return false;}
    if(actual!=expected){cause="Tick wire loaded session changed";return false;}return true;
}
Admission offer(Queue& queue,uint64_t sender,const Input& input,Assigned& assigned,std::string& cause){
    return queue.offer(sender,input,assigned,cause,[](const Batch& batch,std::string& error){std::vector<uint8_t> bytes;return encode(Packet{batch.peer_mask,batch},bytes,error);});
}
bool seal(Queue& queue,Batch& batch,std::vector<uint8_t>& bytes,std::string& cause){
    bytes.clear();std::vector<uint8_t> candidate;
    if(!queue.seal(batch,cause,[&](const Batch& pending,std::string& error){return encode(Packet{pending.peer_mask,pending},candidate,error);}))return false;
    bytes=std::move(candidate);return true;
}
Admission receive(Queue& queue,Session& session_reader,const Context& context,uint64_t sender,const uint8_t* data,size_t size,Assigned& assigned,std::string& cause){
    if(!current(session_reader,context,cause))return Admission::Rejected;
    if(sender==context.local_peer||!member(context.peer_mask,sender)){cause="Tick wire sender is not an authenticated remote member";return Admission::Rejected;}
    Packet p;if(!decode(data,size,p,cause)||!identity(p,context,cause))return Admission::Rejected;
    if(const auto* input=std::get_if<Input>(&p.value))return offer(queue,sender,*input,assigned,cause);
    if(const auto* batch=std::get_if<Batch>(&p.value))return queue.admit(sender,*batch,cause);
    if(const auto* receipt=std::get_if<Receipt>(&p.value))return queue.acknowledge(sender,*receipt,cause)?Admission::Accepted:Admission::Rejected;
    cause="Snapshot control requires its retained barrier receiver";return Admission::Rejected;
}
bool send(Transport& transport,Session& session_reader,const Context& context,uint64_t target,const std::vector<uint8_t>& bytes,std::string& cause){
    if(!current(session_reader,context,cause))return false;
    Packet p;if(!decode(bytes.data(),bytes.size(),p,cause)||!identity(p,context,cause))return false;
    if(const auto* input=std::get_if<Input>(&p.value)){
        if(context.role!=Role::Client||target!=context.host_peer||peer(*input)!=context.local_peer){cause="Tick request send requires local Client input to Host";return false;}
    }else if(std::holds_alternative<Batch>(p.value)){
        if(context.role!=Role::Host||target==context.local_peer||(target&&!member(context.peer_mask,target))){cause="Tick batch send requires Host to current remote members";return false;}
    }else if(const auto* periodic_request=std::get_if<PeriodicRequest>(&p.value)){
        if(context.role!=Role::Client||target!=context.host_peer||periodic_request->peer!=context.local_peer){cause="Periodic request send requires local Client to Host";return false;}
    }else if(const auto* control=std::get_if<SnapshotControl>(&p.value)){
        if(control->phase==SnapshotPhase::Release){
            if(context.role!=Role::Host||target==context.local_peer||(target&&!member(context.peer_mask,target))){cause="Snapshot Release requires Host to remote members";return false;}
        }else if(context.role!=Role::Client||target!=context.host_peer){cause="Snapshot acknowledgment requires Client to Host";return false;}
    }else if(context.role!=Role::Client||target!=context.host_peer){cause="Tick completion send requires Client to Host";return false;}
    if(!transport.send(target,bytes,cause)){if(cause.empty())cause="Tick transport enqueue failed without a cause";return false;}
    cause.clear();return true;
}
}
