#include "bridge.h"
#include "apply_timing.h"
#include "economic_fence.h"
#include "network/node_native.h"
namespace mt2mp::game::live_host_state {
namespace {bool fail(std::string& e,const std::string& cause){e=cause;return false;}}
bool Bridge::route_receive(uint64_t sender,const uint8_t* bytes,size_t n,bool& handled,std::string& e){
    handled=false;
    if(!bytes||!n||n>max_frame_payload)return fail(e,"live Channel9 packet bound invalid");
    try{
        const auto j=nlohmann::json::parse(bytes,bytes+n,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
            if(depth>7)throw std::runtime_error("live Channel9 discriminator nesting exceeds7");
            return true;
        });
        if(j.is_object()&&j.contains("kind")&&j.at("kind").is_string()&&j.at("kind")=="host_state_subscriber_ack"){
            handled=true;
            if(suspended_){e="info: Subscriber ACK skipped for suspended snapshot";return true;}
            if(!economy_||context_.role!=action_session::Role::Host||sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&(1u<<(sender-1)))||
               j.size()!=8||j.at("schema")!=1||!j.at("session").is_number_unsigned()||!j.at("epoch").is_number_unsigned()||!j.at("source_generation").is_number_unsigned()||!j.at("frame_revision").is_number_unsigned()||
               j.at("session").get<uint64_t>()!=context_.session||j.at("epoch").get<uint64_t>()!=common_epoch_||j.at("snapshot_uuid")!=context_.snapshot_uuid||
               j.at("source_generation").get<uint64_t>()!=binding_.native_generation||!j.at("frame_revision").get<uint64_t>()||j.at("frame_revision").get<uint64_t>()>host_revision_)
                return fail(e,"Subscriber Frame ACK authenticated sender/session/epoch/revision differs");
            return economy_->acknowledge_subscribers(uint8_t(sender),uint8_t(context_.peer_mask&~(1u<<(context_.local_peer-1))),j.at("frame_revision").get<uint64_t>(),j.at("rows"),e);
        }
        // Action and state codecs version independently on the same channel.
        // Their kind identifies the owner; a schema number is not a message type.
        if(!j.is_object()||!j.contains("kind")||!j.at("kind").is_string()||
           j.at("kind")!=(economy_?"host_state_economy":(day_?"host_state_day":"host_state"))){e.clear();return true;}
        handled=true;
        return receive(sender,bytes,n,e);
    }catch(const std::exception& x){return fail(e,std::string("live Channel9 discriminator: ")+x.what());}
}
bool Bridge::capture(uint64_t action,Frame& out,std::string& e){out={};if(suspended_||locked_||context_.role!=action_session::Role::Host||host_revision_==UINT64_MAX)return fail(e,"live capture locked/wrong role/revision exhausted");Pin p;if(!port_.read(p,e))return false;if(p.context!=context_||p.binding!=binding_||p.common_epoch!=common_epoch_)return fail(e,"live capture actual Controller/world pin differs");uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);out.session=context_.session;out.snapshot=context_.snapshot_uuid;out.epoch=common_epoch_;out.source_generation=binding_.native_generation;out.revision=host_revision_+1;out.base_action_revision=action;out.time_revision=time;out.selected=p.selected;out.pause=p.pause;out.values=p.values;network::State nodes;if(!network::read_native(p,nodes,e))return false;out.network=std::move(nodes);if(!valid(out,e))return false;if(day_){day::Target target;if(!day_->capture(out,target,e))return false;captured_day_=std::move(target);if(economy_){day::economy::State state;if(!economy_->capture(*captured_day_,state,e))return false;captured_economy_=std::move(state);}}return true;}
bool Bridge::send(const Frame& frame,std::string& e){std::vector<uint8_t> bytes;if(economy_){if(!captured_economy_||captured_economy_->day.frame!=frame)return fail(e,"live exact captured Economy frame missing");if(!day::economy::encode(*captured_economy_,bytes,e))return false;}else if(day_){if(!captured_day_||captured_day_->frame!=frame)return fail(e,"live actual captured Day frame missing");if(!day::encode(*captured_day_,bytes,e))return false;}else if(!encode(frame,bytes,e))return false;try{for(uint64_t peer=1;peer<=4;++peer)if(peer!=context_.local_peer&&(context_.peer_mask&(1u<<(peer-1))))if(!transport_.send(peer,bytes,e)){locked_=true;return fail(e,"live Host frame send Unknown peer="+std::to_string(peer)+" original="+e+"; no retry");}host_revision_=frame.revision;last_=frame;if(day_){last_day_sequence_=captured_day_->history.back().sequence;last_day_=captured_day_;}if(economy_)last_economy_=captured_economy_;return true;}catch(const std::exception& x){locked_=true;return fail(e,std::string("live Host frame send Unknown original exception: ")+x.what());}catch(...){locked_=true;return fail(e,"live Host frame send Unknown original nonstandard exception");}}
bool Bridge::capture_before(const action_session::Request& r,const action_session::Ledger& before,uint64_t previous,std::string& e){Frame frame;if(r.snapshot_uuid!=context_.snapshot_uuid||r.host_revision!=previous||!capture(previous,frame,e))return fail(e,e.empty()?"live Host before request/snapshot/order differs":e);frame.for_action=r.action_uuid;if(!valid(frame,e))return false;if(ledger(frame.values)!=before)return fail(e,"live Host capture differs from actual action before ledger");host_before_=frame;if(day_){captured_day_->frame=frame;before_day_=captured_day_;if(economy_){captured_economy_->day.frame=frame;before_economy_=captured_economy_;}}e.clear();return true;}
bool Bridge::publish_before(const action_session::Message& result,std::string& e){if(!host_before_||result.kind!=action_session::Kind::HostResult||result.outcome!=action_session::Outcome::Confirmed||result.session!=context_.session||result.request.action_uuid!=host_before_->for_action||result.revision!=host_before_->base_action_revision+1||result.evidence.before!=ledger(host_before_->values))return fail(e,"live Host before publication differs from actual confirmed result");if(day_)captured_day_=before_day_;if(economy_)captured_economy_=before_economy_;if(!send(*host_before_,e)){locked_=true;return false;}host_before_.reset();before_day_.reset();before_economy_.reset();return true;}
bool Bridge::receive(uint64_t sender,const uint8_t* bytes,size_t n,std::string& e){if(locked_)return fail(e,"live Client frame locked after Unknown; no replay");if(suspended_){e="info: live frame waits during actual Snapshot load";return true;}Frame frame;std::optional<day::Target> target;std::optional<day::economy::State> economic;if(economy_){day::economy::State decoded;if(!day::economy::decode(bytes,n,decoded,e))return false;frame=decoded.day.frame;target=decoded.day;economic=std::move(decoded);}else if(day_){day::Target decoded;if(!day::decode(bytes,n,decoded,e))return false;frame=decoded.frame;target=std::move(decoded);}else if(!decode(bytes,n,frame,e))return false;uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);const auto gate=admit(sender,context_,common_epoch_,source_generation_?source_generation_:frame.source_generation,last_?&*last_:nullptr,frame,frame.base_action_revision,time,true,e);if(gate==Admission::Rejected)return false;if(gate==Admission::Duplicate||gate==Admission::Stale){if(gate==Admission::Duplicate&&day_&&(!last_day_||target!=last_day_))return fail(e,"live duplicate Frame conflicts with retained Day history");if(gate==Admission::Duplicate&&economy_&&economic!=last_economy_)return fail(e,"live duplicate Frame conflicts with retained Economy state");e.clear();return true;}if(queued_&&frame.revision<=queued_->revision){if(frame==*queued_&&(!day_||target==queued_day_)&&(!economy_||economic==queued_economy_)){e.clear();return true;}return fail(e,"live queued revision stale/conflicting; no overwrite");}if(queued_&&!queued_->for_action.empty())return fail(e,"live action before frame retained until matching HostResult; no overwrite");source_generation_=frame.source_generation;queued_=frame;queued_day_=std::move(target);queued_economy_=std::move(economic);e.clear();return true;}
bool Bridge::take(const Frame& frame,std::string& e){apply_timing::Scope measured_take(apply_timing::Part::Take);if(locked_)return fail(e,"live Client Apply locked after Unknown; no replay");Pin before;if(!port_.read(before,e))return false;if(before.context!=context_||before.binding!=binding_||before.common_epoch!=common_epoch_)return fail(e,"live Client actual pin differs from Controller");if(!frame.network||!network::preflight_native(port_,*frame.network,e))return fail(e,e.empty()?"live exact Node state/preflight absent":e);day::economy::Owners economy_before;if(economy_){if(!queued_economy_||queued_economy_->day.frame!=frame)return fail(e,"live exact queued Economy frame missing");bool prepared=false;{apply_timing::Scope measured(apply_timing::Part::BeforeDay);prepared=economy_->before_day(*queued_economy_,economy_before,e);}if(!prepared){if(economy_->locked()){locked_=true;receipt_.unknown=true;receipt_.cause=e;}return false;}}if(day_){if(!queued_day_||queued_day_->frame!=frame)return fail(e,"live exact queued Day frame missing");bool day_applied=false;{apply_timing::Scope measured(apply_timing::Part::DayTake);day_applied=day_->take(*queued_day_,receipt_,e);}if(!day_applied){if(day_->locked()||receipt_.unknown)locked_=true;return false;}if(!economic_fence().adopt_day_commit(e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause="live Day fence adoption Unknown original="+e;return false;}if(economy_){bool applied=false;{apply_timing::Scope measured(apply_timing::Part::AfterDay);applied=economy_->after_day(*queued_economy_,economy_before,e);}receipt_.attempted=receipt_.attempted||economy_->receipt().attempted;receipt_.attempted_stores+=economy_->receipt().attempted_stores;receipt_.completed_stores+=economy_->receipt().stores;if(!applied){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause="live actual Economy Apply Unknown original="+e;return false;}}}else receipt_=apply(port_,before,frame);if(receipt_.unknown)locked_=true;if(!receipt_.confirmed)return fail(e,receipt_.cause);network_receipt_=network::apply_native(port_,*frame.network);if(!network_activation_receipt_.record(frame,binding_,network_receipt_,e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;receipt_.cause=e;return false;}receipt_.attempted=receipt_.attempted||network_receipt_.attempted;if(!network_receipt_.confirmed){locked_=receipt_.attempted||network_receipt_.unknown;receipt_.unknown=locked_;receipt_.confirmed=false;receipt_.cause="live actual Node Apply failed original="+network_receipt_.cause;return fail(e,receipt_.cause);}if(economy_&&queued_economy_->additions.available){
    const auto rows=mt2mp::subscriber_lifecycle::additions::Cache::acknowledgement(queued_economy_->additions);
    if(!rows.empty()){
        const auto text=nlohmann::json{{"kind","host_state_subscriber_ack"},{"schema",1},{"session",context_.session},{"epoch",common_epoch_},{"snapshot_uuid",context_.snapshot_uuid},{"source_generation",frame.source_generation},{"frame_revision",frame.revision},{"rows",rows}}.dump();
        std::vector<uint8_t> bytes(text.begin(),text.end());
        if(bytes.size()>max_frame_payload||!transport_.send(context_.host_peer,bytes,e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;return fail(e,"Subscriber confirmed Frame ACK send failed original="+e+"; no native replay");}
    }
    economy_->subscriber_frame_committed(frame.revision);
}last_=frame;if(day_){last_day_sequence_=day_->sequence();last_day_=queued_day_;}if(economy_)last_economy_=queued_economy_;e.clear();return true;}
bool Bridge::tick(uint64_t action,bool pending,std::string& e,std::chrono::steady_clock::time_point now){
    if(suspended_){e="info: live state suspended for actual Snapshot load";return true;}
    if(locked_)return fail(e,"live frame locked after Unknown; no replay");
    if(context_.role==action_session::Role::Host){
        if(pending){e="info: Host frame waits for existing action completion";return true;}
        uint64_t time=0;
        if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);
        // Ordinary simulation captures are bounded; authority changes bypass the wait.
        // Action before-frames use capture_before/publish_before independently.
        if(last_&&last_capture_&&last_->base_action_revision==action&&last_->time_revision==time&&
           now-*last_capture_<std::chrono::milliseconds(100)){e.clear();return true;}
        Frame frame;if(!capture(action,frame,e))return false;
        last_capture_=now;
        if(last_&&(!day_||last_day_sequence_==day_->sequence())&&
           (!economy_||(captured_economy_&&last_economy_&&
               captured_economy_->subscribers==last_economy_->subscribers&&
               captured_economy_->loans==last_economy_->loans&&
               captured_economy_->offers==last_economy_->offers&&
               captured_economy_->npcs==last_economy_->npcs&&
               captured_economy_->additions==last_economy_->additions))&&
           last_->network==frame.network&&last_->values==frame.values&&last_->selected==frame.selected&&
           last_->pause==frame.pause&&last_->base_action_revision==action&&last_->time_revision==frame.time_revision){e.clear();return true;}
        return send(frame,e);
    }
    if(!queued_){e.clear();return true;}
    uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);
    const auto gate=admit(context_.host_peer,context_,common_epoch_,source_generation_,last_?&*last_:nullptr,*queued_,action,time,pending,e);
    if(gate==Admission::Wait)return true;
    if(gate==Admission::Rejected)return false;
    if(gate==Admission::New&&!take(*queued_,e))return false;
    queued_.reset();queued_day_.reset();queued_economy_.reset();e.clear();return true;
}
bool Bridge::before_result(uint64_t sender,const action_session::Message& result,uint64_t previous,std::string& e){if(locked_||!queued_)return fail(e,locked_?"live before-result locked; no replay":"live exact HostBeforeFrame missing before native replay");uint64_t time=0;if(!time_||!time_(time,e))return fail(e,e.empty()?"live actual Time authority provider missing":e);const auto gate=admit_before_result(sender,context_,common_epoch_,source_generation_,last_?&*last_:nullptr,*queued_,result,previous,time,e);if(gate!=Admission::New&&gate!=Admission::Duplicate)return fail(e,"live authority before-result refused: "+e);if(gate==Admission::New&&!take(*queued_,e))return false;queued_.reset();queued_day_.reset();queued_economy_.reset();e.clear();return true;}
action_session::AuthorityReadiness Bridge::before_ready(uint64_t sender,const action_session::Message& result,uint64_t previous,std::string& e){using Ready=action_session::AuthorityReadiness;if(locked_||!queued_){e=locked_?"live before-result locked":"live exact HostBeforeFrame missing before native replay";return Ready::Rejected;}uint64_t time=0;if(!time_||!time_(time,e)){if(e.empty())e="live actual Time authority provider missing";return Ready::Rejected;}const auto gate=admit_before_result(sender,context_,common_epoch_,source_generation_,last_?&*last_:nullptr,*queued_,result,previous,time,e);if(gate==Admission::Wait){e="info: live HostResult waits for actual Time revision measured="+std::to_string(time)+" required="+std::to_string(queued_->time_revision);return Ready::Wait;}if(gate!=Admission::New&&gate!=Admission::Duplicate)return Ready::Rejected;e.clear();return Ready::Ready;}
}

namespace mt2mp::game::live_host_state {
bool Bridge::suspend_snapshot(std::string& e){if(locked_||suspended_||host_before_||queued_||(day_&&!day_->suspend_snapshot(e)))return fail(e,e.empty()?"live snapshot suspend locked/pending/unbound":e);suspended_=true;network_activation_receipt_.clear();e.clear();return true;}
bool Bridge::rebind_snapshot(const Context& c,const Binding& b,uint64_t epoch,std::string& e){if(!suspended_||locked_||!epoch||c.session!=context_.session||c.role!=context_.role||c.local_peer!=context_.local_peer||c.host_peer!=context_.host_peer||c.peer_mask!=context_.peer_mask)return fail(e,"live snapshot rebind requires suspended same Controller");Pin actual;if(!port_.read(actual,e)||actual.context!=c||actual.binding!=b||actual.common_epoch!=epoch)return fail(e,e.empty()?"live snapshot fresh actual Pin differs":e);if(day_&&!day_->rebind_snapshot(c,b,e))return false;if(economy_&&!economy_->rebind_snapshot(e))return false;context_=c;binding_=b;common_epoch_=epoch;time_=[c](uint64_t& revision,std::string& cause){return read_time_authority(c,revision,cause);};last_.reset();queued_.reset();host_before_.reset();captured_day_.reset();queued_day_.reset();before_day_.reset();last_day_.reset();captured_economy_.reset();queued_economy_.reset();before_economy_.reset();last_economy_.reset();host_revision_=source_generation_=last_day_sequence_=0;last_capture_.reset();network_activation_receipt_.clear();suspended_=false;e.clear();return true;}
}

namespace mt2mp::game::live_host_state {
bool Bridge::after_action(const action_session::Message& m,std::string& e){if(!day_)return true;if(!day_->after_action(m,e)){locked_=true;return false;}return true;}
}
