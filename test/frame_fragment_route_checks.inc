#include "live_host_state/frame_apply_acks.h"
namespace channel9_fixture {
using namespace mt2mp::game::live_host_state;
namespace action_session=mt2mp::action_session;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Economy {
    unsigned calls=0;uint8_t peer=0,mask=0;uint64_t revision=0;Json rows;
    std::string failure;unsigned committed=0;void subscriber_frame_committed(uint64_t){++committed;}
    bool acknowledge_subscribers(uint8_t p,uint8_t m,uint64_t r,const Json& value,std::string& e){
        ++calls;peer=p;mask=m;revision=r;rows=value;e=failure;return failure.empty();
    }
};
struct Marketing {
    unsigned calls=0;uint64_t sender=0,revision=0;std::string failure;
    bool acknowledge(uint64_t peer,uint64_t frame,std::string& e){
        ++calls;sender=peer;revision=frame;e=failure;return failure.empty();
    }
};
struct Bridge {
    bool suspended_=false,locked_=false,day_=false;
    Context context_;action_session::Binding binding_;
    uint64_t common_epoch_=0,source_generation_=0,host_revision_=0;
    fragments::Assembly fragments_;FrameApplyAcks apply_acks_;Json received_frame_transport_;
    std::optional<economy::State> queued_economy_;ApplyReceipt receipt_;
    struct Transport {std::vector<std::vector<uint8_t>> packets;std::string failure;bool send(uint64_t,const std::vector<uint8_t>& bytes,std::string& e){if(!failure.empty()){e=failure;return false;}packets.push_back(bytes);return true;}} transport_;
    bool acknowledge_vips(uint64_t,uint64_t,std::string& e){e.clear();return true;}
    bool acknowledge_frame(const Frame&,std::string&);
    Economy* economy_=nullptr;Marketing* marketing_port_=nullptr;unsigned received=0;uint64_t received_sender=0;
    std::vector<uint8_t> received_bytes;std::string receive_failure;
    explicit Bridge(const Frame& f):context_(context(f)),binding_{1,f.source_generation},
        common_epoch_(f.epoch),source_generation_(f.source_generation),host_revision_(f.revision){}
    bool receive(uint64_t sender,const uint8_t* bytes,size_t n,std::string& e){
        ++received;received_sender=sender;received_bytes.assign(bytes,bytes+n);e=receive_failure;return e.empty();
    }
    bool route_receive(uint64_t,const uint8_t*,size_t,bool&,std::string&);
};
#include "frame_fragment_route.inc"
#include "frame_apply_ack_send.inc"

void checks(const Frame& f){
    using namespace action_session;
    std::string e;bool handled=true;Economy economy;
    mt2mp::subscriber_lifecycle::additions::Cache actual_cache;
    const auto before_cache=actual_cache.status();
    need(actual_cache.acknowledge(2,2,f.revision,Json::array(),e)&&actual_cache.status()==before_cache,"actual Subscriber cache accepts empty ACK without mutation");
    Bridge router(f);router.economy_=&economy;
    Message original;original.session=f.session;original.request.peer=2;original.request.sequence=1;
    original.request.action_uuid="22222222-2222-4222-8222-222222222222";
    original.request.snapshot_uuid=f.snapshot;
    original.request.canonical_record_key=std::string(64,'a')+":world:building:44";
    std::vector<uint8_t> request_bytes;
    for(const auto kind:{Kind::Request,Kind::HostResult,Kind::ApplyAck,Kind::Completion}){
        auto message=original;message.kind=kind;
        if(kind!=Kind::Request){message.revision=1;message.required_mask=3;message.outcome=Outcome::Confirmed;
            message.evidence.target_retired=message.evidence.undo_confirmed=message.evidence.selection_confirmed=message.evidence.retirement_confirmed=true;
        }
        if(kind==Kind::ApplyAck)message.ack_peer=2;
        std::vector<uint8_t> bytes;need(action_session::encode(message,bytes,e),"real action encode: "+e);
        const auto unchanged=bytes;const auto payload=Json::parse(bytes);
        need(payload.at("kind").is_number_unsigned(),"action fixture must exercise numeric wire kind");
        e="stale";handled=true;
        const bool routed=router.route_receive(kind==Kind::Request||kind==Kind::ApplyAck?2:1,bytes.data(),bytes.size(),handled,e);
        need(routed&&!handled&&e.empty(),"numeric action kind rejected before action decoder: "+e);
        Message decoded;need(action_session::decode(bytes.data(),bytes.size(),decoded,e)&&decoded==message&&bytes==unchanged,"numeric action delegation changed codec or bytes: "+e);
        need(router.received==0&&economy.calls==0&&!router.locked_&&!router.fragments_.pending(),"numeric action reached state receiver or changed routing state");
        if(kind==Kind::Request)request_bytes=bytes;
    }
    auto bad_action=Json::parse(request_bytes);bad_action["request"]["sequence"]=0;
    const auto bad_text=bad_action.dump();Message ignored;std::string decoder_cause;
    need(!action_session::decode(reinterpret_cast<const uint8_t*>(bad_text.data()),bad_text.size(),ignored,decoder_cause),"invalid action fixture accepted");
    need(router.route_receive(2,reinterpret_cast<const uint8_t*>(bad_text.data()),bad_text.size(),handled,e)&&!handled,"malformed numeric action stolen from decoder");
    need(!action_session::decode(reinterpret_cast<const uint8_t*>(bad_text.data()),bad_text.size(),ignored,e)&&e==decoder_cause,"action decoder's original error changed");
    for(const auto kind:{Kind::Request,Kind::HostResult,Kind::ApplyAck,Kind::Completion}){
        auto message=original;message.kind=kind;auto& r=message.request;r.action=Action::PlaceNPC;
        r.canonical_record_key="action:"+r.action_uuid;r.npc.region=67;r.npc.character_slot=0;r.npc.character_type=0;
        if(kind!=Kind::Request){
            message.revision=1;message.required_mask=3;message.outcome=Outcome::Confirmed;
            auto& proof=message.evidence;proof.undo_confirmed=proof.retirement_confirmed=true;
            proof.npc_before.roster_sha256=std::string(64,'a');proof.npc_after.roster_sha256=std::string(64,'b');
            proof.npc_before.quest_targets_sha256=proof.npc_after.quest_targets_sha256=std::string(64,'d');
            proof.npc_after.count=proof.npc_after.undo_count=1;auto& actor=proof.npc_after.created;
            actor.name="Quest NPC";actor.character_slot=actor.character_type=actor.level=0;
            actor.rotation=r.npc.rotation;actor.scale=r.npc.scale;actor.quest_count=1;
            mt2mp::npc_quest_records::Record quest;quest.sequence_id=1;quest.name="Quest";
            quest.destination_key=std::string(64,'a')+":world:building:1";
            quest.accepted.history={-1,0,INT32_MAX};quest.completed.history={3,2,1};actor.quests={quest};
        }
        if(kind==Kind::ApplyAck)message.ack_peer=2;
        std::vector<uint8_t> bytes;need(action_session::encode(message,bytes,e),"Quest NPC encode: "+e);
        Message decoded;const bool decoded_ok=action_session::decode(bytes.data(),bytes.size(),decoded,e);
        need(decoded_ok&&decoded==message,"Quest NPC decoder: "+e);
        int deepest=0;const auto packet=Json::parse(bytes,[&](int depth,Json::parse_event_t,Json&){deepest=std::max(deepest,depth);return true;});
        need(packet.at("schema")==19&&(kind==Kind::Request?deepest<=7:deepest==8),"Quest fixture schema/depth differs");
        std::cout<<"Quest schema19 kind="<<int(kind)<<" deepest="<<deepest<<" codec=PASS\n";
        const auto unchanged=bytes;const bool routed=router.route_receive(1,bytes.data(),bytes.size(),handled,e);
        need(routed&&!handled&&e.empty()&&bytes==unchanged,"valid Quest NPC action routing: "+e);
        if(kind==Kind::HostResult){
            auto excessive=packet;excessive["evidence"]["npc_after"]["created"]["quests"][0]["accepted"]["history"][0]=Json::array({1});
            const auto text=excessive.dump();
            need(!router.route_receive(1,reinterpret_cast<const uint8_t*>(text.data()),text.size(),handled,e)&&!handled&&
                e=="live Channel9 discriminator: live Channel9 discriminator nesting exceeds8","classifier maximum depth guard changed");
            need(!action_session::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e)&&
                e.find("action JSON nesting depth exceeds 8")!=std::string::npos,"action owner maximum depth guard changed");
        }
    }
    const auto route_json=[&](Bridge& target,uint64_t sender,const Json& j){const auto text=j.dump();return target.route_receive(sender,reinterpret_cast<const uint8_t*>(text.data()),text.size(),handled,e);};
    for(const auto* kind:{"host_state","host_state_day","host_state_economy"}){
        Bridge target(f);target.day_=std::string(kind)=="host_state_day";if(std::string(kind)=="host_state_economy")target.economy_=&economy;
        const Json packet={{"kind",kind}};need(route_json(target,1,packet)&&handled&&target.received==1&&target.received_sender==1&&Json::parse(target.received_bytes)==packet,"live state kind no longer routed");
        target.receive_failure="exact state receiver failure";need(!route_json(target,1,packet)&&handled&&e==target.receive_failure,"state receiver cause changed");
    }
    auto state=fixture();for(int32_t id=64;id<264;++id){auto row=state.subscribers[0];row.serialized_id=id;state.subscribers.push_back(row);}
    std::vector<uint8_t> bytes;fragments::Packets packets;
    need(economy::encode(state,bytes,e)&&fragments::encode(f,bytes,mt2mp::sha256,packets,e)&&packets.size()>1,"route fragment fixture: "+e);
    Bridge stream(f);stream.economy_=&economy;
    for(size_t i=0;i<packets.size();++i){const auto& packet=packets[i];need(stream.route_receive(1,packet.data(),packet.size(),handled,e)&&handled,"live fragment routing: "+e);need(stream.received==(i+1==packets.size()?1u:0u),"partial fragment reached state receiver");}
    need(stream.received_bytes==bytes&&stream.received_frame_transport_.at("admitted")==true,"complete route bytes/admission changed");
    Bridge malformed(f);auto invalid=Json::parse(packets[0]);invalid["sha256"]="wrong";
    fragments::Assembly direct;std::vector<uint8_t> complete;std::string direct_cause;
    need(!direct.take(1,context(f),f.epoch,f.source_generation,invalid,mt2mp::sha256,complete,direct_cause),"invalid fragment fixture accepted");
    need(!route_json(malformed,1,invalid)&&handled&&malformed.locked_&&e==direct_cause,"fragment failure cause changed");
    Bridge host(f);host.context_.role=Role::Host;host.context_.local_peer=1;host.economy_=&economy;
    host.apply_acks_.sent(f.revision,2,std::chrono::steady_clock::now());
    Json ack={{"kind","host_state_subscriber_ack"},{"schema",1},{"session",f.session},{"epoch",f.epoch},{"snapshot_uuid",f.snapshot},
        {"source_generation",f.source_generation},{"frame_revision",f.revision},{"rows",Json::array()}};
    need(route_json(host,2,ack)&&handled&&economy.calls==1&&economy.peer==2&&economy.mask==2&&economy.revision==f.revision,"Subscriber ACK no longer routed");
    need(host.apply_acks_.pending()==0&&host.apply_acks_.peers()[1].revision==f.revision,"validated actual ACK advances full-Apply progress");
    for(bool reject:{false,true}){
        Bridge target(f);Marketing marketing;target.context_.role=Role::Host;target.context_.local_peer=1;
        target.economy_=&economy;target.marketing_port_=&marketing;
        target.apply_acks_.sent(f.revision,2,std::chrono::steady_clock::now());
        if(reject)marketing.failure="original Marketing ACK failure";
        const bool accepted=route_json(target,2,ack);
        need(marketing.calls==1&&marketing.sender==2&&marketing.revision==f.revision,"Marketing ACK owner/revision was not forwarded once");
        need(accepted==!reject&&handled&&target.locked_==reject&&target.apply_acks_.pending()==(reject?2:0),"Marketing ACK failure confirmed full frame or failed to lock");
        if(reject)need(e==marketing.failure,"Marketing ACK original cause changed");
    }
    auto conflicting=ack;conflicting["rows"]=Json::array({{{"id",1},{"sha256",std::string(64,'a')}}});
    const auto prior_calls=economy.calls;
    need(!route_json(host,2,conflicting)&&e.find("duplicate applied Frame ACK")!=std::string::npos&&economy.calls==prior_calls,"conflicting duplicate rejected before Subscriber cache mutation");
    auto wrong_rows=ack;wrong_rows["rows"]=Json::object();e="stale cause";
    need(!route_json(host,2,wrong_rows)&&e=="live applied Frame ACK rows invalid"&&economy.calls==prior_calls,"malformed ACK rows retain their actual guard cause");
    Bridge empty(f);empty.context_.role=Role::Host;empty.context_.local_peer=1;empty.apply_acks_.sent(f.revision,2,std::chrono::steady_clock::now());
    need(!route_json(empty,2,ack)&&empty.apply_acks_.pending()==2,"bare Bridge retains original no-Economy ACK rejection");
    Bridge failed(f);failed.context_.role=Role::Host;failed.context_.local_peer=1;failed.economy_=&economy;failed.apply_acks_.sent(f.revision,2,std::chrono::steady_clock::now());
    economy.failure="unconfirmed Subscriber rows";need(!route_json(failed,2,ack)&&failed.apply_acks_.pending()==2,"failed Subscriber validation never confirms full frame");economy.failure.clear();
    for(bool available:{false,true}){
        Bridge client(f);Economy local;client.economy_=&local;client.queued_economy_=fixture();client.queued_economy_->additions={};client.queued_economy_->additions.available=available;
        need(client.acknowledge_frame(f,e)&&client.transport_.packets.size()==1,"actual successful Take ACK emitted with empty additions");
        const auto sent=Json::parse(client.transport_.packets[0]);need(sent.size()==8&&sent.at("rows")==Json::array()&&sent.at("frame_revision")==f.revision&&sent.at("source_generation")==f.source_generation,"same eight-field ACK contract retained");
        need(local.committed==(available?1u:0u),"Subscriber commit notification preserves availability contract");
    }
    Bridge bare(f);need(bare.acknowledge_frame(f,e)&&bare.transport_.packets.empty(),"bare Bridge does not emit Economy ACK");
    Bridge failed_send(f);failed_send.economy_=&economy;failed_send.transport_.failure="original ACK send failure";
    need(!failed_send.acknowledge_frame(f,e)&&failed_send.locked_&&failed_send.receipt_.unknown&&!failed_send.receipt_.confirmed&&e.find("original ACK send failure")!=std::string::npos,"ACK send failure after Apply is Unknown and cause retained");
    economy.failure="exact Subscriber ACK failure";need(!route_json(host,2,ack)&&handled&&e==economy.failure,"Subscriber ACK delegate cause changed");
    ack["session"]=f.session+1;need(!route_json(host,2,ack)&&handled&&e=="Subscriber Frame ACK authenticated sender/session/epoch/revision differs","malformed ACK cause changed");
    const std::string invalid_json="{";need(!host.route_receive(2,reinterpret_cast<const uint8_t*>(invalid_json.data()),invalid_json.size(),handled,e)&&!handled&&e.starts_with("live Channel9 discriminator: [json.exception.parse_error."),"malformed JSON cause changed");
    need(!host.route_receive(2,nullptr,1,handled,e)&&!handled&&e=="live Channel9 packet bound invalid","packet bound cause changed");
}
}
